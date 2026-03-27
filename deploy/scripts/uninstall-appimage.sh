#!/bin/bash
# AppImage 卸载脚本
# 删除 EasyKiConverter AppImage 创建的用户级文件

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_ID="io.github.tangsangsimida.easykiconverter"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 加载公共函数库
load_common() {
    if [ -f "${SCRIPT_DIR}/uninstall-common.sh" ]; then
        . "${SCRIPT_DIR}/uninstall-common.sh"
    else
        log_error "找不到 uninstall-common.sh"
        exit 1
    fi
}

# 检查是否以 root 身份运行
check_user() {
    if [ "$(id -u)" = "0" ]; then
        log_error "不要以 root 身份运行此脚本"
        log_info "此脚本应该在普通用户模式下运行"
        exit 1
    fi
}

# 删除桌面文件
remove_desktop_file() {
    local desktop_file="${HOME}/.local/share/applications/${APP_ID}.desktop"

    if [ -f "$desktop_file" ]; then
        if rm -f "$desktop_file"; then
            log_info "已删除 desktop 文件: $desktop_file"
        else
            log_error "无法删除 desktop 文件: $desktop_file"
            return 1
        fi
    else
        log_warn "未找到 desktop 文件: $desktop_file"
    fi

    return 0
}

# 删除所有尺寸的图标（浅色和暗色主题）
remove_icons() {
    local removed_count=0

    for size in 16 24 32 48 64 128 256 512; do
        local icon_file="${HOME}/.local/share/icons/hicolor/${size}x${size}/apps/${APP_ID}.png"
        if [ -f "$icon_file" ]; then
            if rm -f "$icon_file"; then
                log_info "已删除图标 ($size x $size): $icon_file"
                ((removed_count++))
            fi
        fi
    done

    if [ -d "${HOME}/.local/share/icons/hicolor-dark" ]; then
        for size in 16 24 32 48 64 128 256 512; do
            local icon_file="${HOME}/.local/share/icons/hicolor-dark/${size}x${size}/apps/${APP_ID}.png"
            if [ -f "$icon_file" ]; then
                if rm -f "$icon_file"; then
                    log_info "已删除暗色图标 ($size x $size): $icon_file"
                    ((removed_count++))
                fi
            fi
        done
    fi

    if [ $removed_count -eq 0 ]; then
        log_warn "未找到任何图标文件"
    fi
}

# 更新桌面数据库
update_desktop_database() {
    if command -v update-desktop-database >/dev/null 2>&1; then
        if update-desktop-database "${HOME}/.local/share/applications" 2>/dev/null; then
            log_info "桌面数据库更新成功"
        else
            log_warn "桌面数据库更新失败"
        fi
    fi
}

# 更新图标缓存
update_icon_cache() {
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        for theme in hicolor hicolor-dark; do
            local icon_dir="${HOME}/.local/share/icons/$theme"
            if [ -d "$icon_dir" ]; then
                if gtk-update-icon-cache -q "$icon_dir" 2>/dev/null; then
                    log_info "图标缓存更新成功: $theme"
                else
                    log_warn "图标缓存更新失败: $theme"
                fi
            fi
        done
    fi
}

# 刷新 GNOME Shell
refresh_gnome() {
    log_info "刷新 GNOME Shell..."

    if ! command -v loginctl >/dev/null 2>&1; then
        log_warn "loginctl 不可用，跳过 GNOME 刷新"
        return
    fi

    local user=$(whoami)
    local session=$(loginctl list-sessions "$user" 2>/dev/null | awk 'NR>1 {print $1}' | head -1)

    if [ -n "$session" ]; then
        local runtime_dir=$(loginctl show-session "$session" -p XDG_RUNTIME_DIR --value 2>/dev/null)
        if [ -n "$runtime_dir" ]; then
            export XDG_RUNTIME_DIR="$runtime_dir"
            export DBUS_SESSION_BUS_ADDRESS="unix:path=$runtime_dir/bus"
            gdbus call --session --dest org.gnome.Shell --object-path /org/gnome/Shell \
                --method org.gnome.Shell.Eval "appSystem.reload()" 2>/dev/null && \
                log_info "GNOME Shell 刷新成功" || log_warn "GNOME Shell 刷新失败"
        fi
    fi
}

# 主函数
main() {
    echo "=========================================="
    echo "  EasyKiConverter AppImage 卸载工具"
    echo "=========================================="
    echo ""

    load_common
    check_user

    echo ""
    log_info "即将删除以下文件："
    echo "  - Desktop 文件: ${HOME}/.local/share/applications/${APP_ID}.desktop"
    echo "  - 图标文件（所有尺寸）"
    echo ""

    read -p "确认卸载？(y/N): " confirm
    if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
        log_info "卸载已取消"
        exit 0
    fi

    echo ""
    log_info "开始卸载..."
    echo ""

    remove_desktop_file
    remove_icons
    update_desktop_database
    update_icon_cache
    refresh_gnome

    echo ""
    log_info "卸载完成！"
    echo ""
    log_info "提示："
    echo "  1. 您可以手动删除 AppImage 文件"
    echo "  2. 如果任务栏图标仍然显示，请注销并重新登录"
    echo "  3. 或者重启桌面环境以刷新图标缓存"
    echo ""
}

main "$@"
