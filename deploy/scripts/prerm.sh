#!/bin/sh
# Pre-remove script
# 在卸载前清理桌面集成和应用程序文件

# 应用配置变量
APP_NAME="easykiconverter"
APP_DESKTOP_NAME="io.github.tangsangsimida.easykiconverter"
APP_INSTALL_DIR="/opt/easykiconverter"
APP_BIN_LINK="/usr/bin/easykiconverter"
APP_DESKTOP_FILE="/usr/share/applications/${APP_DESKTOP_NAME}.desktop"
APP_METAINFO_FILE="/usr/share/metainfo/${APP_DESKTOP_NAME}.metainfo.xml"
APP_SCRIPT_DIR="/usr/share/easykiconverter"
AUTOSTART_FILE="/etc/xdg/autostart/easykiconverter-register.desktop"
AUTOSTART_FILE_SYSTEM="/usr/share/xdg/autostart/easykiconverter-register.desktop"

log_info() {
    echo "[INFO] $1" >&2
}

log_warn() {
    echo "[WARN] $1" >&2
}

log_error() {
    echo "[ERROR] $1" >&2
}

safe_remove_file() {
    local file="$1"
    [ -z "$file" ] && return 1
    if [ -f "$file" ] || [ -L "$file" ]; then
        rm -f "$file" 2>/dev/null && log_info "已删除: $file"
    fi
    return 0
}

safe_remove_dir() {
    local dir="$1"
    [ -z "$dir" ] && return 1
    if [ -d "$dir" ]; then
        rm -rf "$dir" 2>/dev/null && log_info "已删除目录: $dir"
    fi
    return 0
}

remove_app_files() {
    log_info "删除应用文件和可执行文件..."
    safe_remove_dir "$APP_INSTALL_DIR"
    safe_remove_file "$APP_BIN_LINK"
}

remove_system_desktop_file() {
    log_info "删除系统级别的桌面文件..."
    safe_remove_file "$APP_DESKTOP_FILE"
    safe_remove_file "/usr/share/applications/com.tangsangsimida.easykiconverter.desktop"
}

remove_system_icons() {
    log_info "删除系统级别的图标文件..."
    for size in 16x16 32x32 48x48 64x64 128x128 256x256 512x512; do
        safe_remove_file "/usr/share/icons/hicolor/$size/apps/${APP_DESKTOP_NAME}.png"
        safe_remove_file "/usr/share/icons/hicolor-dark/$size/apps/${APP_DESKTOP_NAME}.png"
    done
}

remove_metainfo_file() {
    log_info "删除 AppStream 元数据文件..."
    safe_remove_file "$APP_METAINFO_FILE"
    safe_remove_file "/usr/share/metainfo/com.tangsangsimida.easykiconverter.metainfo.xml"
}

remove_autostart_files() {
    log_info "删除自动启动文件..."
    safe_remove_file "$AUTOSTART_FILE"
    safe_remove_file "$AUTOSTART_FILE_SYSTEM"
}

remove_user_scripts() {
    log_info "删除用户级别的脚本文件..."
    safe_remove_dir "$APP_SCRIPT_DIR"
}

remove_user_desktop_files() {
    log_info "删除用户级别的桌面文件..."
    getent passwd | awk -F: '$3 >= 1000 {print $1, $6}' | while read -r user home; do
        [ -z "$user" ] || [ -z "$home" ] || [ ! -d "$home" ] && continue
        local user_desktop="$home/.local/share/applications"
        [ -d "$user_desktop" ] || continue
        find "$user_desktop" -maxdepth 1 -type f \( -iname "*EasyKi*.desktop" -o -iname "*easykiconverter*.desktop" \) 2>/dev/null | while read -r df; do
            if [ -f "$df" ] && grep -qi "EasyKiConverter\|easykiconverter\|tangsangsimida" "$df" 2>/dev/null; then
                safe_remove_file "$df"
            fi
        done
        safe_remove_file "$home/.config/easykiconverter-registered"
        safe_remove_file "$home/.config/easykiconverter/pending-refresh"
    done
}

log_info "执行 pre-remove 清理..."

remove_app_files
remove_system_desktop_file
remove_system_icons
remove_metainfo_file
remove_autostart_files
remove_user_scripts
remove_user_desktop_files

exit 0
