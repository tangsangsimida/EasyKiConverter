#!/bin/bash
# Flatpak 卸载脚本
# 卸载 EasyKiConverter Flatpak 并清理相关文件

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
check_root() {
    if [ "$(id -u)" = "0" ]; then
        log_warn "检测到 root 身份运行，Flatpak 卸载可能需要用户确认"
    fi
}

# 检查 flatpak 是否可用
check_flatpak() {
    if ! command -v flatpak >/dev/null 2>&1; then
        log_error "flatpak 命令不可用，请安装 flatpak"
        exit 1
    fi
}

# 检查 desktop 文件
check_desktop_file() {
    local desktop_file="${HOME}/.local/share/applications/${APP_ID}.desktop"
    if [ -f "$desktop_file" ]; then
        log_info "找到用户 desktop 文件: $desktop_file"
        return 0
    fi
    return 1
}

# 卸载 Flatpak
uninstall_flatpak_app() {
    log_info "正在卸载 Flatpak 应用..."

    if flatpak uninstall "$APP_ID" -y 2>/dev/null; then
        log_info "Flatpak 应用卸载成功"
    else
        log_warn "Flatpak 应用未安装或卸载失败"
    fi

    log_info "清理未使用的 Flatpak 运行时..."
    flatpak uninstall --unused -y 2>/dev/null || true
}

# 删除用户数据
clean_user_data() {
    log_info "删除用户数据..."

    safe_remove_dir "${HOME}/.local/share/EasyKiConverter"
    safe_remove_dir "${HOME}/.var/app/${APP_ID}"
    safe_remove_dir "${HOME}/.local/share/flatpak/repo/refs/heads/deploy/app/${APP_ID}"
    safe_remove_dir "${HOME}/.local/share/flatpak/repo/refs/heads/deploy/runtime/${APP_ID}.Debug"
}

# 删除用户配置文件
clean_user_config() {
    log_info "删除用户配置..."
    safe_remove_dir "${HOME}/.config/EasyKiConverter"
}

# 删除桌面文件
remove_desktop_file() {
    local desktop_file="${HOME}/.local/share/applications/${APP_ID}.desktop"
    safe_remove_file "$desktop_file"
}

# 删除用户图标
remove_user_icons() {
    log_info "删除用户图标..."
    for size in 16 24 32 48 64 128 256 512; do
        safe_remove_file "${HOME}/.local/share/icons/hicolor/${size}x${size}/apps/${APP_ID}.png"
        safe_remove_file "${HOME}/.local/share/icons/hicolor-dark/${size}x${size}/apps/${APP_ID}.png"
    done
}

# 更新桌面数据库
update_desktop_database() {
    log_info "更新桌面数据库..."
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "${HOME}/.local/share/applications" 2>/dev/null || true
    fi
}

# 更新图标缓存
update_icon_cache() {
    log_info "更新图标缓存..."
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        for theme in hicolor hicolor-dark; do
            local icon_dir="${HOME}/.local/share/icons/$theme"
            if [ -d "$icon_dir" ]; then
                gtk-update-icon-cache -q -t -f "$icon_dir" 2>/dev/null || true
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
    echo "  EasyKiConverter Flatpak 卸载工具"
    echo "=========================================="
    echo ""

    load_common
    check_root
    check_flatpak

    local remove_data=false
    local remove_config=false

    while [ $# -gt 0 ]; do
        case "$1" in
            --remove-data)
                remove_data=true
                shift
                ;;
            --remove-config)
                remove_config=true
                shift
                ;;
            *)
                shift
                ;;
        esac
    done

    echo ""
    log_info "即将执行以下清理："
    echo "    - 卸载 Flatpak 应用"
    echo "    - 删除 desktop 文件"
    echo "    - 删除用户图标"
    echo ""

    if [ "$remove_data" = true ]; then
        echo "    - 删除用户数据 (~/.local/share/EasyKiConverter)"
        echo "    - 删除 Flatpak 用户数据 (~/.var/app/${APP_ID})"
    fi

    if [ "$remove_config" = true ]; then
        echo "    - 删除用户配置 (~/.config/EasyKiConverter)"
    fi

    echo ""
    read -p "确认卸载？(y/N): " confirm
    if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
        log_info "卸载已取消"
        exit 0
    fi

    echo ""
    log_info "开始卸载..."
    echo ""

    uninstall_flatpak_app
    remove_desktop_file
    remove_user_icons
    update_desktop_database
    update_icon_cache
    refresh_gnome

    if [ "$remove_data" = true ]; then
        clean_user_data
    fi

    if [ "$remove_config" = true ]; then
        clean_user_config
    fi

    echo ""
    log_info "卸载完成！"
    echo ""
    log_info "提示：如果任务栏图标仍然显示，请注销并重新登录或重启桌面环境"
    echo ""
}

main "$@"
