#!/bin/sh
# 卸载后清理脚本
# 在软件包卸载后执行，清理桌面缓存

# 应用配置变量
APP_NAME="easykiconverter"
APP_ID="io.github.tangsangsimida.easykiconverter"
USER_DATA_DIR="${HOME}/.local/share/EasyKiConverter"
USER_CONFIG_DIR="${HOME}/.config/EasyKiConverter"
FLATPAK_USER_DATA_DIR="${HOME}/.var/app/${APP_ID}"

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

update_desktop_database() {
    log_info "更新桌面数据库..."
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database /usr/share/applications 2>/dev/null
    fi
}

update_icon_cache() {
    log_info "更新图标缓存..."
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        for theme in hicolor hicolor-dark Adwaita; do
            [ -d "/usr/share/icons/$theme" ] && gtk-update-icon-cache -q -t -f "/usr/share/icons/$theme" 2>/dev/null
        done
    fi
}

refresh_gnome_cache() {
    log_info "刷新 GNOME Shell 应用缓存..."
    if ! command -v loginctl >/dev/null 2>&1 || ! command -v sudo >/dev/null 2>&1; then
        log_warn "loginctl 或 sudo 命令不可用，跳过 GNOME 缓存刷新"
        return 1
    fi

    loginctl list-users 2>/dev/null | awk 'NR>1 {print $2}' | while read -r user; do
        [ -z "$user" ] && continue
        getent passwd "$user" >/dev/null 2>&1 || continue
        local user_id=$(getent passwd "$user" 2>/dev/null | cut -d: -f3)
        [ "$user_id" -lt 1000 ] && continue
        local sessions=$(loginctl list-sessions "$user" 2>/dev/null | awk '{print $1}' | grep -v "^$") || true
        for session in $sessions; do
            local runtime_dir=$(loginctl show-session "$session" -p XDG_RUNTIME_DIR --value 2>/dev/null)
            [ -z "$runtime_dir" ] && continue
            local dbus_bus="$runtime_dir/bus"
            [ ! -S "$dbus_bus" ] && continue
            sudo -u "$user" env XDG_RUNTIME_DIR="$runtime_dir" DBUS_SESSION_BUS_ADDRESS="unix:path=$dbus_bus" \
                gdbus call --session --dest org.gnome.Shell --object-path /org/gnome/Shell --method org.gnome.Shell.Eval "appSystem.reload()" 2>/dev/null && \
                log_info "GNOME Shell 刷新成功: $user" || log_warn "GNOME Shell 刷新失败: $user"
        done
    done
}

remove_user_data() {
    log_info "删除用户数据..."
    getent passwd | awk -F: '$3 >= 1000 {print $1, $6}' | while read -r user home; do
        [ -z "$user" ] || [ -z "$home" ] || [ ! -d "$home" ] && continue
        safe_remove_dir "$home/.local/share/EasyKiConverter"
    done
}

remove_user_config() {
    log_info "删除用户配置..."
    getent passwd | awk -F: '$3 >= 1000 {print $1, $6}' | while read -r user home; do
        [ -z "$user" ] || [ -z "$home" ] || [ ! -d "$home" ] && continue
        safe_remove_dir "$home/.config/EasyKiConverter"
        safe_remove_dir "$home/.config/easykiconverter-registered"
    done
}

remove_flatpak_user_data() {
    log_info "删除 Flatpak 用户数据..."
    getent passwd | awk -F: '$3 >= 1000 {print $1, $6}' | while read -r user home; do
        [ -z "$user" ] || [ -z "$home" ] || [ ! -d "$home" ] && continue
        safe_remove_dir "$home/.var/app/${APP_ID}"
        safe_remove_dir "$home/.local/share/flatpak/repo/refs/heads/deploy/app/${APP_ID}"
        safe_remove_dir "$home/.local/share/flatpak/repo/refs/heads/deploy/runtime/${APP_ID}.Debug"
        safe_remove_dir "$home/.local/share/flatpak/repo/refs/remotes/easykiconverter-origin"
        safe_remove_dir "$home/.local/share/flatpak/repo/refs/remotes/easykiconverter1-origin"
        safe_remove_dir "$home/.local/share/flatpak/repo/refs/remotes/debug-origin"
        safe_remove_dir "$home/.local/share/flatpak/repo/refs/remotes/local-repo"
        safe_remove_dir "$home/.local/share/flatpak/repo/refs/remotes/local-repo-local"
    done
}

purge_dpkg_metadata() {
    local purge_flag_file="/tmp/${APP_NAME}-purge-in-progress"
    if [ "$1" = "purge" ]; then
        safe_remove_file "$purge_flag_file"
        return 0
    elif [ "$1" = "remove" ]; then
        (
            sleep 2
            [ -f "$purge_flag_file" ] && exit 0
            if dpkg-query -W -f='${Status}' "$APP_NAME" 2>/dev/null | grep -q "config-files"; then
                dpkg --purge "$APP_NAME" >/dev/null 2>&1 || log_error "dpkg purge 失败"
            fi
            safe_remove_file "$purge_flag_file*"
        ) >/dev/null 2>&1 &
    fi
    return 0
}

log_info "执行 postrm 清理..."

update_desktop_database
update_icon_cache
refresh_gnome_cache

if [ "$1" = "purge" ]; then
    log_info "执行 purge，清理用户数据..."
    remove_user_data
    remove_user_config
    remove_flatpak_user_data
fi

purge_dpkg_metadata "$1"

exit 0
