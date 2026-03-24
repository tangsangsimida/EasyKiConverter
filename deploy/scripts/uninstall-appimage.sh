#!/bin/bash
# AppImage 卸载脚本
# 删除 EasyKiConverter AppImage 创建的用户级 desktop 文件和图标

set -e

# 应用配置
APP_DESKTOP_NAME="io.github.tangsangsimida.easykiconverter"
USER_DESKTOP_DIR="$HOME/.local/share/applications"
USER_ICON_DIR="$HOME/.local/share/icons"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查是否以正确的用户身份运行
check_user() {
    if [ "$(id -u)" = "0" ]; then
        log_error "不要以 root 身份运行此脚本"
        log_info "此脚本应该在普通用户模式下运行"
        exit 1
    fi
}

# 检查 desktop 文件是否存在
check_desktop_file() {
    local desktop_file="$USER_DESKTOP_DIR/$APP_DESKTOP_NAME.desktop"

    if [ ! -f "$desktop_file" ]; then
        log_warn "未找到 desktop 文件: $desktop_file"
        log_warn "AppImage 可能未被正确安装"
        return 1
    fi

    return 0
}

# 删除 desktop 文件
remove_desktop_file() {
    local desktop_file="$USER_DESKTOP_DIR/$APP_DESKTOP_NAME.desktop"

    if [ -f "$desktop_file" ]; then
        if rm -f "$desktop_file"; then
            log_info "已删除 desktop 文件: $desktop_file"
        else
            log_error "无法删除 desktop 文件: $desktop_file"
            return 1
        fi
    fi

    return 0
}

# 删除所有尺寸的图标（浅色和暗色主题）
remove_icons() {
    local removed_count=0

    # 删除浅色主题图标
    for size in 16 24 32 48 64 128 256 512; do
        local icon_file="$USER_ICON_DIR/hicolor/${size}x${size}/apps/$APP_DESKTOP_NAME.png"

        if [ -f "$icon_file" ]; then
            if rm -f "$icon_file"; then
                log_info "已删除图标 ($size x $size): $icon_file"
                ((removed_count++))
            else
                log_error "无法删除图标: $icon_file"
            fi
        fi
    done

    # 删除暗色主题图标
    if [ -d "$USER_ICON_DIR/hicolor-dark" ]; then
        for size in 16 24 32 48 64 128 256 512; do
            local icon_file="$USER_ICON_DIR/hicolor-dark/${size}x${size}/apps/$APP_DESKTOP_NAME.png"

            if [ -f "$icon_file" ]; then
                if rm -f "$icon_file"; then
                    log_info "已删除暗色图标 ($size x $size): $icon_file"
                    ((removed_count++))
                else
                    log_error "无法删除图标: $icon_file"
                fi
            fi
        done
    fi

    return 0
}

# 更新桌面数据库
update_desktop_database() {
    if command -v update-desktop-database >/dev/null 2>&1; then
        if update-desktop-database "$USER_DESKTOP_DIR" 2>/dev/null; then
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
            if [ -d "$USER_ICON_DIR/$theme" ]; then
                if gtk-update-icon-cache -q "$USER_ICON_DIR/$theme" 2>/dev/null; then
                    log_info "图标缓存更新成功: $theme"
                else
                    log_warn "图标缓存更新失败: $theme"
                fi
            fi
        done
    fi
}

# 主函数
main() {
    echo "=========================================="
    echo "  EasyKiConverter AppImage 卸载工具"
    echo "=========================================="
    echo ""

    # 检查用户身份
    check_user

    # 检查 desktop 文件是否存在
    if ! check_desktop_file; then
        echo ""
        log_warn "没有找到需要卸载的文件"
        exit 0
    fi

    echo ""
    log_info "即将删除以下文件："
    echo "  - Desktop 文件: $USER_DESKTOP_DIR/$APP_DESKTOP_NAME.desktop"
    echo "  - 图标文件（所有尺寸）"
    echo ""

    # 询问用户确认
    read -p "确认卸载？(y/N): " confirm
    if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
        log_info "卸载已取消"
        exit 0
    fi

    echo ""
    log_info "开始卸载..."
    echo ""

    # 删除 desktop 文件
    remove_desktop_file

    # 删除图标
    remove_icons

    # 更新桌面数据库
    update_desktop_database

    # 更新图标缓存
    update_icon_cache

    echo ""
    log_info "卸载完成！"
    echo ""
    log_info "提示："
    echo "  1. 您可以手动删除 AppImage 文件"
    echo "  2. 如果任务栏图标仍然显示，请注销并重新登录"
    echo "  3. 或者重启桌面环境以刷新图标缓存"
    echo ""
}

# 运行主函数
main "$@"