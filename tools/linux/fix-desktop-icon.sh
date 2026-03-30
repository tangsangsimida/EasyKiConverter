#!/bin/bash
#
# 修复用户级别桌面文件覆盖系统配置的问题
#
# 此脚本用于清理用户级别可能错误的 .desktop 文件，确保使用系统级别的正确配置。
# 开发过程中经常会遇到这种情况，特别是测试构建版本时。
#
# 使用方法：
#   ./tools/linux/fix-desktop-icon.sh
#

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 应用标识符
APP_ID="io.github.tangsangsimida.easykiconverter"
OLD_APP_ID="com.tangsangsimida.easykiconverter"

# 辅助函数
print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

print_info() {
    echo -e "${GREEN}✓${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

# 检查是否为 root 用户
check_root() {
    if [ "$EUID" -ne 0 ]; then
        print_warning "此脚本需要 root 权限来刷新系统级缓存"
        echo "请使用 sudo 运行：sudo $0"
        echo ""
        echo "注意：清理用户级别文件不需要 root 权限，但刷新系统缓存需要。"
        echo "如果您只想清理用户级别文件，可以取消注释下面的检查。"
        # exit 1
    fi
}

# 清理用户级别的桌面文件
cleanup_user_desktop_files() {
    print_header "步骤 1: 清理用户级别的桌面文件"

    local user_app_dir="$HOME/.local/share/applications"
    local found_files=0

    if [ -d "$user_app_dir" ]; then
        for file in "$user_app_dir"/${APP_ID}.desktop "$user_app_dir"/${OLD_APP_ID}.desktop; do
            if [ -f "$file" ]; then
                found_files=$((found_files + 1))
                print_warning "发现用户级别桌面文件: $file"
                echo "  文件内容（前 5 行）:"
                head -5 "$file" | sed 's/^/    /'
                echo ""
                read -p "是否删除此文件? (y/N): " -n 1 -r
                echo
                if [[ $REPLY =~ ^[Yy]$ ]]; then
                    rm -f "$file"
                    print_info "已删除: $file"
                else
                    print_warning "跳过删除: $file"
                fi
            fi
        done

        if [ $found_files -eq 0 ]; then
            print_info "未发现用户级别的桌面文件"
        fi
    else
        print_info "用户应用目录不存在: $user_app_dir"
    fi

    echo ""
}

# 清理用户级别的图标缓存
cleanup_user_icon_cache() {
    print_header "步骤 2: 清理用户级别的图标缓存"

    local user_cache_dir="$HOME/.cache/icons"
    local found_cache=0

    if [ -d "$user_cache_dir" ]; then
        for cache_dir in "$user_cache_dir"/*; do
            if [ -d "$cache_dir" ]; then
                local icon_cache="$cache_dir/${APP_ID}.png"
                if [ -f "$icon_cache" ]; then
                    found_cache=$((found_cache + 1))
                    print_info "清理图标缓存: $icon_cache"
                    rm -f "$icon_cache"
                fi
            fi
        done

        if [ $found_cache -eq 0 ]; then
            print_info "未发现用户级别的图标缓存"
        fi
    fi

    echo ""
}

# 刷新用户级别的桌面数据库
refresh_user_desktop_db() {
    print_header "步骤 3: 刷新用户级别的桌面数据库"

    if command -v update-desktop-database >/dev/null 2>&1; then
        if [ -d "$HOME/.local/share/applications" ]; then
            update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
            print_info "已刷新用户级别桌面数据库"
        else
            print_info "用户应用目录不存在，跳过刷新"
        fi
    else
        print_warning "update-desktop-database 命令不可用"
    fi

    echo ""
}

# 刷新系统级别的桌面数据库
refresh_system_desktop_db() {
    print_header "步骤 4: 刷新系统级别的桌面数据库"

    if [ "$EUID" -eq 0 ]; then
        if command -v update-desktop-database >/dev/null 2>&1; then
            update-desktop-database /usr/share/applications 2>/dev/null || true
            print_info "已刷新系统级别桌面数据库"
        else
            print_warning "update-desktop-database 命令不可用"
        fi
    else
        print_warning "需要 root 权限，跳过系统级刷新"
        echo "  使用 sudo 运行此脚本以刷新系统级缓存"
    fi

    echo ""
}

# 刷新图标缓存
refresh_icon_cache() {
    print_header "步骤 5: 刷新图标缓存"

    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        for theme in hicolor Adwaita; do
            if [ -d "/usr/share/icons/$theme" ]; then
                if [ "$EUID" -eq 0 ]; then
                    gtk-update-icon-cache -q -t -f "/usr/share/icons/$theme" 2>/dev/null || true
                    print_info "已刷新图标缓存: $theme"
                else
                    print_warning "需要 root 权限，跳过 $theme"
                fi
            fi
        done
    else
        print_warning "gtk-update-icon-cache 命令不可用"
    fi

    echo ""
}

# 刷新 AppStream 缓存
refresh_appstream_cache() {
    print_header "步骤 6: 刷新 AppStream 缓存"

    if command -v appstreamcli >/dev/null 2>&1; then
        if [ "$EUID" -eq 0 ]; then
            appstreamcli refresh-cache --force 2>/dev/null || true
            print_info "已刷新 AppStream 缓存"
        else
            print_warning "需要 root 权限，跳过 AppStream 刷新"
        fi
    else
        print_info "appstreamcli 命令不可用，跳过"
    fi

    echo ""
}

# 验证系统级配置
verify_system_config() {
    print_header "步骤 7: 验证系统级配置"

    local desktop_file="/usr/share/applications/${APP_ID}.desktop"
    local icon_file="/usr/share/icons/hicolor/48x48/apps/${APP_ID}.png"
    local exec_file="/usr/bin/easykiconverter"

    if [ -f "$desktop_file" ]; then
        print_info "系统桌面文件存在: $desktop_file"
    else
        print_error "系统桌面文件不存在: $desktop_file"
    fi

    if [ -f "$icon_file" ]; then
        print_info "系统图标文件存在: $icon_file"
    else
        print_error "系统图标文件不存在: $icon_file"
    fi

    if [ -f "$exec_file" ]; then
        print_info "可执行文件存在: $exec_file"
    else
        print_warning "可执行文件不存在: $exec_file"
    fi

    echo ""
}

# 显示最终建议
show_final_advice() {
    print_header "完成"

    echo "如果图标仍未显示，请尝试以下操作："
    echo ""
    echo "1. 注销并重新登录"
    echo "2. 或重启系统: sudo reboot"
    echo "3. 在应用菜单中搜索 'EasyKiConverter'"
    echo ""
    echo "对于 GNOME 用户，可以尝试："
    echo "  按 Super 键（Windows 键）打开应用菜单"
    echo "  搜索 'EasyKiConverter'"
    echo "  如果找到应用，右键点击并选择 '添加到收藏夹'"
    echo ""
}

# 主函数
main() {
    print_header "EasyKiConverter 桌面图标修复工具"

    echo "此脚本将："
    echo "  1. 清理用户级别的桌面文件（避免覆盖系统配置）"
    echo "  2. 清理用户级别的图标缓存"
    echo "  3. 刷新用户级别的桌面数据库"
    echo "  4. 刷新系统级别的桌面数据库（需要 root）"
    echo "  5. 刷新图标缓存（需要 root）"
    echo "  6. 刷新 AppStream 缓存（需要 root）"
    echo "  7. 验证系统级配置"
    echo ""

    read -p "是否继续? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "操作已取消"
        exit 0
    fi

    echo ""

    # 执行清理和刷新步骤
    cleanup_user_desktop_files
    cleanup_user_icon_cache
    refresh_user_desktop_db
    refresh_system_desktop_db
    refresh_icon_cache
    refresh_appstream_cache
    verify_system_config
    show_final_advice

    print_header "修复完成"
}

# 运行主函数
main "$@"