#!/bin/sh
# 卸载后清理脚本
# 在软件包卸载后执行，清理桌面缓存和配置

# 应用配置变量
APP_NAME="easykiconverter"
APP_DESKTOP_NAME="com.tangsangsimida.easykiconverter"
APP_INSTALL_DIR="/opt/easykiconverter"
APP_BIN_LINK="/usr/bin/easykiconverter"
APP_DESKTOP_FILE="/usr/share/applications/${APP_DESKTOP_NAME}.desktop"
APP_DESKTOP_FILE_COMPAT="/usr/share/applications/com.tangsangsimida.EasyKiConverter.desktop"  # 兼容旧版本
APP_METAINFO_FILE="/usr/share/metainfo/${APP_DESKTOP_NAME}.metainfo.xml"
APP_METAINFO_FILE_COMPAT="/usr/share/metainfo/com.tangsangsimida.EasyKiConverter.metainfo.xml"  # 兼容旧版本
APP_BIN_LINK="/usr/bin/easykiconverter"
APP_DESKTOP_FILE="/usr/share/applications/${APP_DESKTOP_NAME}.desktop"
APP_METAINFO_FILE="/usr/share/metainfo/${APP_DESKTOP_NAME}.metainfo.xml"
APP_SCRIPT_DIR="/usr/share/easykiconverter"
AUTOSTART_FILE="/etc/xdg/autostart/easykiconverter-register.desktop"
AUTOSTART_FILE_SYSTEM="/usr/share/xdg/autostart/easykiconverter-register.desktop"

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
        return 0  # 文件不存在，视为成功
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
        return 0  # 目录不存在，视为成功
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
    # 删除旧版本的桌面文件（兼容性）
    safe_remove_file "$APP_DESKTOP_FILE_COMPAT"
}

# 删除系统级别的图标文件
remove_system_icons() {
    log_info "删除系统级别的图标文件..."
    for size in 16x16 32x32 48x48 64x64 128x128 256x256 512x512; do
        local icon_file="/usr/share/icons/hicolor/$size/apps/${APP_DESKTOP_NAME}.png"
        safe_remove_file "$icon_file"
    done
}

# 删除 AppStream 元数据文件
remove_metainfo_file() {
    log_info "删除 AppStream 元数据文件..."
    safe_remove_file "$APP_METAINFO_FILE"
    # 删除旧版本的元数据文件（兼容性）
    safe_remove_file "$APP_METAINFO_FILE_COMPAT"
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
        for theme in hicolor Adwaita; do
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

# 验证用户参数安全性
validate_user() {
    local user="$1"
    if [ -z "$user" ]; then
        log_error "validate_user: 用户名为空"
        return 1
    fi

    # 检查用户名是否只包含安全字符（字母、数字、下划线、连字符）
    if ! echo "$user" | grep -qE '^[a-zA-Z0-9_-]+$'; then
        log_error "validate_user: 用户名包含不安全字符: $user"
        return 1
    fi

    # 检查用户是否存在
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
    local cmd="$@"

    if ! validate_user "$user"; then
        return 1
    fi

    if ! sudo -u "$user" $cmd 2>/dev/null; then
        log_warn "以用户 $user 执行命令失败: $cmd"
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

    # 遍历所有普通用户（UID >= 1000）
    getent passwd | awk -F: '$3 >= 1000 {print $1, $6}' | while read -r user home; do
        if [ -z "$user" ] || [ -z "$home" ] || [ ! -d "$home" ]; then
            continue
        fi

        if ! validate_user "$user"; then
            continue
        fi

        local user_desktop="$home/.local/share/applications"
        if [ ! -d "$user_desktop" ]; then
            continue
        fi

        # 使用 find 命令查找所有匹配的文件
        find "$user_desktop" -maxdepth 1 -type f \( -iname "*EasyKi*.desktop" -o -iname "*easykiconverter*.desktop" \) 2>/dev/null | while read -r desktop_file; do
            if [ ! -f "$desktop_file" ]; then
                continue
            fi

            # 检查文件内容，确保是当前应用的桌面文件
            if grep -qi "EasyKiConverter\|easykiconverter\|tangsangsimida" "$desktop_file" 2>/dev/null; then
                if safe_remove_file "$desktop_file"; then
                    log_info "已删除用户级别桌面文件: $desktop_file"
                fi
            fi
        done

        # 更新用户桌面数据库
        if command -v update-desktop-database >/dev/null 2>&1; then
            if update-desktop-database "$user_desktop" 2>/dev/null; then
                log_info "用户桌面数据库更新成功: $user"
            else
                log_warn "用户桌面数据库更新失败: $user"
            fi
        fi

        # 删除用户级别的应用注册标记文件
        local marker_file="$home/.config/easykiconverter-registered"
        safe_remove_file "$marker_file"

        # 删除待处理刷新标记文件
        local pending_file="$home/.config/easykiconverter/pending-refresh"
        safe_remove_file "$pending_file"
    done

    return 0
}

# 刷新 GNOME Shell 应用缓存
refresh_gnome_cache() {
    log_info "刷新 GNOME Shell 应用缓存..."

    if ! command -v loginctl >/dev/null 2>&1 || ! command -v sudo >/dev/null 2>&1; then
        log_warn "loginctl 或 sudo 命令不可用，跳过 GNOME 缓存刷新"
        return 1
    fi

    # 获取所有已登录用户
    loginctl list-users 2>/dev/null | awk 'NR>1 {print $2}' | while read -r user; do
        if [ -z "$user" ]; then
            continue
        fi

        if ! validate_user "$user"; then
            continue
        fi

        # 获取用户信息
        local user_info=$(getent passwd "$user" 2>/dev/null)
        if [ -z "$user_info" ]; then
            continue
        fi

        local user_id=$(echo "$user_info" | cut -d: -f3)

        # 只为普通用户运行（UID >= 1000）
        if [ "$user_id" -lt 1000 ]; then
            continue
        fi

        # 获取用户的活动会话
        local sessions=$(loginctl list-sessions "$user" 2>/dev/null | awk '{print $1}' | grep -v "^$") || true
        for session in $sessions; do
            # 获取会话的 XDG_RUNTIME_DIR
            local runtime_dir=$(loginctl show-session "$session" -p XDG_RUNTIME_DIR --value 2>/dev/null)
            if [ -z "$runtime_dir" ]; then
                continue
            fi

            # 检查 D-Bus 会话总线是否存在
            local dbus_bus="$runtime_dir/bus"
            if [ ! -S "$dbus_bus" ]; then
                continue
            fi

            # 方法1：调用 GNOME Shell 的 appSystem.reload() 方法
            if safe_run_as_user "$user" env XDG_RUNTIME_DIR="$runtime_dir" DBUS_SESSION_BUS_ADDRESS="unix:path=$dbus_bus" gdbus call --session --dest org.gnome.Shell --object-path /org/gnome/Shell --method org.gnome.Shell.Eval "appSystem.reload()"; then
                log_info "GNOME Shell 应用列表刷新成功: $user"
            else
                log_warn "GNOME Shell 应用列表刷新失败: $user"
            fi

            # 方法2：强制刷新应用数据库
            if safe_run_as_user "$user" env XDG_RUNTIME_DIR="$runtime_dir" DBUS_SESSION_BUS_ADDRESS="unix:path=$dbus_bus" gio monitor --cancel "$home/.local/share/applications"; then
                log_info "应用数据库刷新成功: $user"
            else
                log_warn "应用数据库刷新失败: $user"
            fi

            # 方法3：清除 GNOME Shell 的应用图标缓存
            if [ -d "$runtime_dir" ]; then
                # 删除应用图标缓存文件
                find "$runtime_dir" -name "*.desktop" -type f -delete 2>/dev/null
                find "$runtime_dir" -name "*easykiconverter*" -type f -delete 2>/dev/null
                log_info "GNOME Shell 应用图标缓存清除成功: $user"
            fi
        done
    done

    return 0
}

# 自动清除 dpkg 元数据
purge_dpkg_metadata() {
    log_info "清除 dpkg 元数据..."

    local purge_flag_file="/tmp/${APP_NAME}-purge-in-progress"

    if [ "$1" = "purge" ]; then
        # 已在 purge 模式，清理标志文件
        safe_remove_file "$purge_flag_file"
        return 0
    elif [ "$1" = "remove" ]; then
        # 在 remove 模式下，等待短暂时间后自动执行 purge
        (
            sleep 2

            # 检查标志文件，防止并发执行
            if [ -f "$purge_flag_file" ]; then
                exit 0
            fi

            # 创建标志文件（使用更安全的临时文件创建方法）
            if ! mktemp "$purge_flag_file.XXXXXX" >/dev/null 2>&1; then
                log_error "无法创建标志文件: $purge_flag_file"
                exit 1
            fi

            # 检查包状态
            if dpkg-query -W -f='${Status}' "$APP_NAME" 2>/dev/null | grep -q "config-files"; then
                log_info "包处于 config-files 状态，执行 purge"
                if dpkg --purge "$APP_NAME" >/dev/null 2>&1; then
                    log_info "dpkg purge 成功"
                else
                    log_error "dpkg purge 失败"
                fi
            fi

            # 清理标志文件
            safe_remove_file "$purge_flag_file*"
        ) >/dev/null 2>&1 &

        return 0
    fi

    return 1
}

# === 主清理逻辑 ===

# 确保删除应用程序文件和可执行文件（双重保险）
remove_app_files

# 删除系统级别的桌面文件
remove_system_desktop_file

# 删除系统级别的图标文件
remove_system_icons

# 删除 AppStream 元数据文件
remove_metainfo_file

# 删除自动启动文件
remove_autostart_files

# 删除用户级别的脚本文件
remove_user_scripts

# 强制更新桌面数据库（移除已卸载的桌面文件）
update_desktop_database

# 强制更新图标缓存（移除已卸载的图标）
update_icon_cache

# 为所有已登录用户触发 GNOME 应用列表刷新
refresh_gnome_cache

# 删除用户级别的桌面文件副本
remove_user_desktop_files

# 注意：不删除用户级别的配置文件
# 用户的配置文件位于 ~/.config/EasyKiConverter/
# 如果需要删除，可以取消下面的注释（但通常不建议自动删除）
# if [ -d "$HOME/.config/EasyKiConverter" ]; then
#     safe_remove_dir "$HOME/.config/EasyKiConverter"
# fi

# 自动清除 dpkg 元数据，确保状态从 'rc' 变为完全清除
purge_dpkg_metadata "$1"

exit 0