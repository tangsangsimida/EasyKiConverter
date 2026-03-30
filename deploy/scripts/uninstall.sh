#!/bin/bash
# 统一卸载脚本
# 支持 deb、flatpak、appimage 三种卸载方式
#
# 用法:
#   uninstall.sh --type deb          # Debian/Ubuntu 包卸载
#   uninstall.sh --type flatpak      # Flatpak 卸载
#   uninstall.sh --type appimage     # AppImage 卸载
#   uninstall.sh --type all         # 全部清理（包括用户数据）
#   uninstall.sh --help              # 显示帮助

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

usage() {
    cat << EOF
用法: $(basename "$0") [选项]

EasyKiConverter 统一卸载工具

选项:
    --type TYPE     指定卸载类型: deb, flatpak, appimage, all
    --remove-data   同时删除用户数据（~/.local/share/EasyKiConverter）
    --remove-config 同时删除用户配置（~/.config/EasyKiConverter）
    --help          显示此帮助信息

示例:
    $(basename "$0") --type deb
    $(basename "$0") --type flatpak --remove-data
    $(basename "$0") --type all

EOF
}

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

# 卸载 DEB 包
uninstall_deb() {
    log_info "开始卸载 DEB 包..."

    remove_app_files
    remove_system_desktop_file
    remove_system_icons
    remove_metainfo_file
    remove_autostart_files
    remove_user_scripts
    update_desktop_database
    update_icon_cache
    remove_user_desktop_files
    refresh_gnome_cache
    purge_dpkg_metadata "remove"
}

# 卸载 Flatpak
uninstall_flatpak() {
    log_info "开始卸载 Flatpak..."

    if command -v flatpak >/dev/null 2>&1; then
        if flatpak uninstall "$APP_ID" -y 2>/dev/null; then
            log_info "Flatpak 卸载成功"
        else
            log_warn "Flatpak 未安装或卸载失败"
        fi

        flatpak uninstall --unused -y 2>/dev/null || true
    else
        log_warn "flatpak 命令不可用"
    fi

    remove_flatpak_user_data
    remove_flatpak_repo_refs
    remove_user_desktop_files
    update_user_icon_cache
    refresh_gnome_cache
}

# 卸载 AppImage
uninstall_appimage() {
    log_info "开始卸载 AppImage..."

    remove_user_desktop_files
    remove_user_icons
    update_user_icon_cache
    refresh_gnome_cache
}

# 删除用户数据
clean_user_data() {
    log_info "删除用户数据..."
    remove_user_data
    remove_user_config
    remove_flatpak_user_data
    remove_flatpak_repo_refs
    log_info "用户数据清理完成"
}

# 主函数
main() {
    local uninstall_type=""
    local remove_data=false
    local remove_config=false

    while [ $# -gt 0 ]; do
        case "$1" in
            --type)
                uninstall_type="$2"
                shift 2
                ;;
            --remove-data)
                remove_data=true
                shift
                ;;
            --remove-config)
                remove_config=true
                shift
                ;;
            --help|-h)
                usage
                exit 0
                ;;
            *)
                log_error "未知选项: $1"
                usage
                exit 1
                ;;
        esac
    done

    if [ -z "$uninstall_type" ]; then
        log_error "请指定 --type"
        usage
        exit 1
    fi

    load_common

    echo ""
    echo "=========================================="
    echo "  EasyKiConverter 卸载工具"
    echo "=========================================="
    echo ""

    case "$uninstall_type" in
        deb)
            uninstall_deb
            ;;
        flatpak)
            uninstall_flatpak
            ;;
        appimage)
            uninstall_appimage
            ;;
        all)
            uninstall_deb
            uninstall_flatpak
            uninstall_appimage
            ;;
        *)
            log_error "未知卸载类型: $uninstall_type"
            usage
            exit 1
            ;;
    esac

    if [ "$remove_data" = true ]; then
        clean_user_data
    fi

    if [ "$remove_config" = true ]; then
        remove_user_config
    fi

    echo ""
    log_info "卸载完成！"
    echo ""
    if [ "$remove_data" = false ]; then
        log_warn "提示: 用户数据未删除。如需删除，请运行:"
        echo "    $(basename "$0") --type $uninstall_type --remove-data"
    fi
}

main "$@"
