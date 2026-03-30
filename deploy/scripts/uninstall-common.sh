#!/bin/sh
# 通用卸载清理函数库
# 用于 prerm.sh、postrm.sh 和 uninstall-*.sh 的共享清理逻辑

# 应用配置变量
APP_NAME="easykiconverter"
APP_ID="io.github.tangsangsimida.easykiconverter"
APP_DESKTOP_NAME="${APP_ID}"
APP_INSTALL_DIR="/opt/easykiconverter"
APP_BIN_LINK="/usr/bin/easykiconverter"
APP_DESKTOP_FILE="/usr/share/applications/${APP_DESKTOP_NAME}.desktop"
APP_METAINFO_FILE="/usr/share/metainfo/${APP_DESKTOP_NAME}.metainfo.xml"
APP_SCRIPT_DIR="/usr/share/easykiconverter"
AUTOSTART_FILE="/etc/xdg/autostart/easykiconverter-register.desktop"
AUTOSTART_FILE_SYSTEM="/usr/share/xdg/autostart/easykiconverter-register.desktop"

# 用户数据目录
USER_DATA_DIR="${HOME}/.local/share/EasyKiConverter"
USER_CONFIG_DIR="${HOME}/.config/EasyKiConverter"
FLATPAK_USER_DATA_DIR="${HOME}/.var/app/${APP_ID}"
FLATPAK_REPO_DIR="${HOME}/.local/share/flatpak/repo/refs/heads/deploy/app/${APP_ID}"

# 日志函数
log_info() {
    echo "[INFO] $1" >&2
}

log_warn() {
    echo "[WARN] $1" >&2
}

log_error() {
    echo "[ERROR] $1" >&2
}

# 安全删除文件函数
safe_remove_file() {
    local file="$1"
    if [ -z "$file" ]; then
        log_error "safe_remove_file: 文件路径为空"
        return 1
    fi

    if [ -f "$file" ]; then
        if rm -f "$file" 2>/dev/null; then
            log_info "已删除文件: $file"
            return 0
        else
            log_warn "无法删除文件（权限不足或其他错误）: $file"
            return 1
        fi
    elif [ -L "$file" ]; then
        if rm -f "$file" 2>/dev/null; then
            log_info "已删除符号链接: $file"
            return 0
        else
            log_warn "无法删除符号链接（权限不足或其他错误）: $file"
            return 1
        fi
    else
        return 0
    fi
}

# 安全删除目录函数
safe_remove_dir() {
    local dir="$1"
    if [ -z "$dir" ]; then
        log_error "safe_remove_dir: 目录路径为空"
        return 1
    fi

    if [ -d "$dir" ]; then
        if rm -rf "$dir" 2>/dev/null; then
            log_info "已删除目录: $dir"
            return 0
        else
            log_warn "无法删除目录（权限不足或其他错误）: $dir"
            return 1
        fi
    else
        return 0
    fi
}

# 删除应用文件和可执行文件
remove_app_files() {
    log_info "删除应用文件和可执行文件..."
    safe_remove_dir "$APP_INSTALL_DIR"
    safe_remove_file "$APP_BIN_LINK"
}

# 删除系统级别的桌面文件
remove_system_desktop_file() {
    log_info "删除系统级别的桌面文件..."
    safe_remove_file "$APP_DESKTOP_FILE"
    safe_remove_file "/usr/share/applications/com.tangsangsimida.easykiconverter.desktop"
}

# 删除系统级别的图标文件
remove_system_icons() {
    log_info "删除系统级别的图标文件..."
    for size in 16x16 32x32 48x48 64x64 128x128 256x256 512x512; do
        safe_remove_file "/usr/share/icons/hicolor/$size/apps/${APP_DESKTOP_NAME}.png"
    done
    if [ -d "/usr/share/icons/hicolor-dark" ]; then
        for size in 16x16 32x32 48x48 64x64 128x128 256x256 512x512; do
            safe_remove_file "/usr/share/icons/hicolor-dark/$size/apps/${APP_DESKTOP_NAME}.png"
        done
    fi
}

# 删除 AppStream 元数据文件
remove_metainfo_file() {
    log_info "删除 AppStream 元数据文件..."
    safe_remove_file "$APP_METAINFO_FILE"
    safe_remove_file "/usr/share/metainfo/com.tangsangsimida.easykiconverter.metainfo.xml"
}

# 删除自动启动文件
remove_autostart_files() {
    log_info "删除自动启动文件..."
    safe_remove_file "$AUTOSTART_FILE"
    safe_remove_file "$AUTOSTART_FILE_SYSTEM"
}

# 删除用户级别的脚本文件
remove_user_scripts() {
    log_info "删除用户级别的脚本文件..."
    safe_remove_dir "$APP_SCRIPT_DIR"
}

# 删除用户数据（用户配置、日志、缓存等）
remove_user_data() {
    log_info "删除用户数据..."
    safe_remove_dir "$USER_DATA_DIR"
}

# 删除用户配置
remove_user_config() {
    log_info "删除用户配置..."
    safe_remove_dir "$USER_CONFIG_DIR"
}

# 删除 Flatpak 用户数据
remove_flatpak_user_data() {
    log_info "删除 Flatpak 用户数据..."
    safe_remove_dir "$FLATPAK_USER_DATA_DIR"
}

# 删除 Flatpak 仓库引用
remove_flatpak_repo_refs() {
    log_info "删除 Flatpak 仓库引用..."
    safe_remove_dir "${HOME}/.local/share/flatpak/repo/refs/heads/deploy/app/${APP_ID}"
    safe_remove_dir "${HOME}/.local/share/flatpak/repo/refs/heads/deploy/runtime/${APP_ID}.Debug"
    safe_remove_dir "${HOME}/.local/share/flatpak/repo/refs/remotes/easykiconverter-origin"
    safe_remove_dir "${HOME}/.local/share/flatpak/repo/refs/remotes/easykiconverter1-origin"
    safe_remove_dir "${HOME}/.local/share/flatpak/repo/refs/remotes/debug-origin"
    safe_remove_dir "${HOME}/.local/share/flatpak/repo/refs/remotes/local-repo"
    safe_remove_dir "${HOME}/.local/share/flatpak/repo/refs/remotes/local-repo-local"
}

# 更新桌面数据库
update_desktop_database() {
    log_info "更新桌面数据库..."
    if command -v update-desktop-database >/dev/null 2>&1; then
        if update-desktop-database /usr/share/applications 2>/dev/null; then
            log_info "桌面数据库更新成功"
        else
            log_warn "桌面数据库更新失败"
        fi
    fi
}

# 更新图标缓存
update_icon_cache() {
    log_info "更新图标缓存..."
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        for theme in hicolor hicolor-dark Adwaita; do
            if [ -d "/usr/share/icons/$theme" ]; then
                if gtk-update-icon-cache -q -t -f "/usr/share/icons/$theme" 2>/dev/null; then
                    log_info "图标缓存更新成功: $theme"
                else
                    log_warn "图标缓存更新失败: $theme"
                fi
            fi
        done
    fi
}

# 更新用户图标缓存
update_user_icon_cache() {
    log_info "更新用户图标缓存..."
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        for theme in hicolor hicolor-dark; do
            local user_icon_dir="${HOME}/.local/share/icons/$theme"
            if [ -d "$user_icon_dir" ]; then
                if gtk-update-icon-cache -q -t -f "$user_icon_dir" 2>/dev/null; then
                    log_info "用户图标缓存更新成功: $theme"
                else
                    log_warn "用户图标缓存更新失败: $theme"
                fi
            fi
        done
    fi
}

# 验证用户参数安全性
validate_user() {
    local user="$1"
    if [ -z "$user" ]; then
        log_error "validate_user: 用户名为空"
        return 1
    fi

    if ! echo "$user" | grep -qE '^[a-zA-Z0-9_-]+$'; then
        log_error "validate_user: 用户名包含不安全字符: $user"
        return 1
    fi

    if ! getent passwd "$user" >/dev/null 2>&1; then
        log_error "validate_user: 用户不存在: $user"
        return 1
    fi

    return 0
}

# 安全执行用户命令
safe_run_as_user() {
    local user="$1"
    shift

    if ! validate_user "$user"; then
        return 1
    fi

    if ! sudo -u "$user" "$@" 2>/dev/null; then
        log_warn "以用户 $user 执行命令失败"
        return 1
    fi

    return 0
}

# 删除用户级别的桌面文件
remove_user_desktop_files() {
    log_info "删除用户级别的桌面文件..."

    if ! command -v getent >/dev/null 2>&1; then
        log_warn "getent 命令不可用，跳过用户级别清理"
        return 1
    fi

    getent passwd | awk -F: '$3 >= 1000 {print $1, $6}' | while read -r user home; do
        if [ -z "$user" ] || [ -z "$home" ] || [ ! -d "$home" ]; then
            continue
        fi

        if ! validate_user "$user"; then
            continue
        fi

        local user_desktop="$home/.local/share/applications"
        if [ -d "$user_desktop" ]; then
            find "$user_desktop" -maxdepth 1 -type f \( -iname "*EasyKi*.desktop" -o -iname "*easykiconverter*.desktop" \) 2>/dev/null | while read -r desktop_file; do
                if [ -f "$desktop_file" ] && grep -qi "EasyKiConverter\|easykiconverter\|tangsangsimida" "$desktop_file" 2>/dev/null; then
                    safe_remove_file "$desktop_file"
                    log_info "已删除用户级别桌面文件: $desktop_file"
                fi
            done

            if command -v update-desktop-database >/dev/null 2>&1; then
                update-desktop-database "$user_desktop" 2>/dev/null
            fi
        fi

        safe_remove_file "$home/.config/easykiconverter-registered"
        safe_remove_file "$home/.config/easykiconverter/pending-refresh"
    done

    return 0
}

# 删除用户图标
remove_user_icons() {
    log_info "删除用户图标..."
    for size in 16 24 32 48 64 128 256 512; do
        safe_remove_file "${HOME}/.local/share/icons/hicolor/${size}x${size}/apps/${APP_DESKTOP_NAME}.png"
        safe_remove_file "${HOME}/.local/share/icons/hicolor-dark/${size}x${size}/apps/${APP_DESKTOP_NAME}.png"
    done
}

# 刷新 GNOME Shell 应用缓存
refresh_gnome_cache() {
    log_info "刷新 GNOME Shell 应用缓存..."

    if ! command -v loginctl >/dev/null 2>&1 || ! command -v sudo >/dev/null 2>&1; then
        log_warn "loginctl 或 sudo 命令不可用，跳过 GNOME 缓存刷新"
        return 1
    fi

    loginctl list-users 2>/dev/null | awk 'NR>1 {print $2}' | while read -r user; do
        if [ -z "$user" ] || ! validate_user "$user"; then
            continue
        fi

        local user_info=$(getent passwd "$user" 2>/dev/null)
        [ -z "$user_info" ] && continue

        local user_id=$(echo "$user_info" | cut -d: -f3)
        [ "$user_id" -lt 1000 ] && continue

        local sessions=$(loginctl list-sessions "$user" 2>/dev/null | awk '{print $1}' | grep -v "^$") || true
        for session in $sessions; do
            local runtime_dir=$(loginctl show-session "$session" -p XDG_RUNTIME_DIR --value 2>/dev/null)
            [ -z "$runtime_dir" ] && continue

            local dbus_bus="$runtime_dir/bus"
            [ ! -S "$dbus_bus" ] && continue

            safe_run_as_user "$user" env XDG_RUNTIME_DIR="$runtime_dir" DBUS_SESSION_BUS_ADDRESS="unix:path=$dbus_bus" \
                gdbus call --session --dest org.gnome.Shell --object-path /org/gnome/Shell --method org.gnome.Shell.Eval "appSystem.reload()" 2>/dev/null && \
                log_info "GNOME Shell 应用列表刷新成功: $user" || log_warn "GNOME Shell 应用列表刷新失败: $user"
        done
    done

    return 0
}

# 自动清除 dpkg 元数据
purge_dpkg_metadata() {
    log_info "清除 dpkg 元数据..."

    local purge_flag_file="/tmp/${APP_NAME}-purge-in-progress"

    if [ "$1" = "purge" ]; then
        safe_remove_file "$purge_flag_file"
        return 0
    elif [ "$1" = "remove" ]; then
        (
            sleep 2

            if [ -f "$purge_flag_file" ]; then
                exit 0
            fi

            if ! mktemp "$purge_flag_file.XXXXXX" >/dev/null 2>&1; then
                log_error "无法创建标志文件: $purge_flag_file"
                exit 1
            fi

            if dpkg-query -W -f='${Status}' "$APP_NAME" 2>/dev/null | grep -q "config-files"; then
                log_info "包处于 config-files 状态，执行 purge"
                dpkg --purge "$APP_NAME" >/dev/null 2>&1 || log_error "dpkg purge 失败"
            fi

            safe_remove_file "$purge_flag_file*"
        ) >/dev/null 2>&1 &

        return 0
    fi

    return 1
}
