#!/bin/sh

# Pre-remove script
# 在卸载前清理桌面集成和应用程序文件

# 删除应用程序文件和可执行文件
# 这些文件应该在 dpkg 卸载时被自动删除，但为了确保清理完整，我们手动删除
if [ -d "/opt/easykiconverter" ]; then
    rm -rf /opt/easykiconverter || true
fi

if [ -f "/usr/bin/easykiconverter" ]; then
    rm -f /usr/bin/easykiconverter || true
fi

# 删除系统级别的桌面文件
if [ -f "/usr/share/applications/com.tangsangsimida.EasyKiConverter.desktop" ]; then
    rm -f /usr/share/applications/com.tangsangsimida.EasyKiConverter.desktop || true
fi

# 删除系统级别的图标文件
for size in 16x16 32x32 48x48 64x64 128x128 256x256 512x512; do
    ICON_FILE="/usr/share/icons/hicolor/$size/apps/com.tangsangsimida.EasyKiConverter.png"
    if [ -f "$ICON_FILE" ]; then
        rm -f "$ICON_FILE" || true
    fi
done || true

# 删除 AppStream 元数据文件
if [ -f "/usr/share/metainfo/com.tangsangsimida.EasyKiConverter.metainfo.xml" ]; then
    rm -f /usr/share/metainfo/com.tangsangsimida.EasyKiConverter.metainfo.xml || true
fi

# 删除自动启动文件的软链接
# 注意：软链接在 /etc/xdg/autostart/，实际文件在 /usr/share/xdg/autostart/
if [ -L "/etc/xdg/autostart/easykiconverter-register.desktop" ]; then
    rm -f /etc/xdg/autostart/easykiconverter-register.desktop || true
fi

# 删除用户级别的脚本文件
if [ -d "/usr/share/easykiconverter" ]; then
    rm -rf /usr/share/easykiconverter || true
fi

# 同步更新桌面数据库，确保缓存更新完成
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications 2>/dev/null || true
fi

# 同步更新图标缓存，确保缓存更新完成
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    for theme in hicolor Adwaita; do
        if [ -d "/usr/share/icons/$theme" ]; then
            gtk-update-icon-cache -q -t -f "/usr/share/icons/$theme" 2>/dev/null || true
        fi
    done
fi

# 为所有已登录用户触发 GNOME 应用列表刷新
if command -v loginctl >/dev/null 2>&1 && command -v sudo >/dev/null 2>&1; then
    # 获取所有已登录用户
    loginctl list-users 2>/dev/null | awk '{print $2}' | while read -r user; do
        if [ -n "$user" ] && [ "$user" != "USER" ]; then
            # 获取用户信息
            USER_INFO=$(getent passwd "$user" 2>/dev/null || echo "") || true
            if [ -n "$USER_INFO" ]; then
                USER_ID=$(echo "$USER_INFO" | cut -d: -f3)
                # 只为普通用户运行（UID >= 1000）
                if [ "$USER_ID" -ge 1000 ]; then
                    # 获取用户的活动会话
                    SESSIONS=$(loginctl list-sessions "$user" 2>/dev/null | awk '{print $1}' | grep -v "^$") || true
                    for SESSION in $SESSIONS; do
                        # 获取会话的 XDG_RUNTIME_DIR
                        RUNTIME_DIR=$(loginctl show-session "$SESSION" -p XDG_RUNTIME_DIR --value 2>/dev/null || echo "") || true
                        if [ -n "$RUNTIME_DIR" ]; then
                            # 检查 D-Bus 会话总线是否存在
                            DBUS_BUS="$RUNTIME_DIR/bus"
                            if [ -S "$DBUS_BUS" ]; then
                                # 使用 sudo -u 切换用户执行 gdbus 命令
                                # 方法1：调用 GNOME Shell 的 appSystem.reload() 方法
                                sudo -u "$user" \
                                    env XDG_RUNTIME_DIR="$RUNTIME_DIR" \
                                    DBUS_SESSION_BUS_ADDRESS="unix:path=$DBUS_BUS" \
                                    gdbus call --session --dest org.gnome.Shell --object-path /org/gnome/Shell --method org.gnome.Shell.Eval "appSystem.reload()" 2>/dev/null || true || true
                                
                                # 方法2：强制刷新应用数据库（使用 gio）
                                sudo -u "$user" \
                                    env XDG_RUNTIME_DIR="$RUNTIME_DIR" \
                                    DBUS_SESSION_BUS_ADDRESS="unix:path=$DBUS_BUS" \
                                    gio monitor --cancel "$HOME/.local/share/applications" 2>/dev/null || true || true
                                
                                # 方法3：清除 GNOME Shell 的应用图标缓存
                                CACHE_DIR="$RUNTIME_DIR"
                                if [ -d "$CACHE_DIR" ]; then
                                    # 删除应用图标缓存文件
                                    find "$CACHE_DIR" -name "*.desktop" -type f -delete 2>/dev/null || true
                                    find "$CACHE_DIR" -name "*easykiconverter*" -type f -delete 2>/dev/null || true
                                fi
                            fi
                        fi
                    done || true
                fi
            fi
        fi
    done || true
fi || true

# 清理用户级别的桌面文件
# 遍历所有普通用户，删除他们桌面文件副本
if command -v getent >/dev/null 2>&1; then
    getent passwd | awk -F: '$3 >= 1000 {print $1, $6}' | while read -r user home; do
        # 只为普通用户处理（UID >= 1000）
        if [ -n "$user" ] && [ -n "$home" ] && [ -d "$home" ]; then
            USER_DESKTOP="$home/.local/share/applications"
            if [ -d "$USER_DESKTOP" ]; then
                # 删除所有 EasyKiConverter 相关的桌面文件
                # 使用 find 命令查找所有匹配的文件，支持大小写不敏感的匹配
                find "$USER_DESKTOP" -maxdepth 1 -type f \( -iname "*EasyKi*.desktop" -o -iname "*easykiconverter*.desktop" \) 2>/dev/null | while read -r desktop_file; do
                    if [ -f "$desktop_file" ]; then
                        # 检查是否是当前应用创建的（检查关键字，不区分大小写）
                        if grep -qi "EasyKiConverter\|easykiconverter" "$desktop_file" 2>/dev/null || \
                           grep -qi "tangsangsimida" "$desktop_file" 2>/dev/null; then
                            rm -f "$desktop_file"
                            echo "已删除用户级别桌面文件: $desktop_file"
                        fi
                    fi
                done 2>/dev/null || true
                
                # 同步更新用户桌面数据库
                if command -v update-desktop-database >/dev/null 2>&1; then
                    update-desktop-database "$USER_DESKTOP" 2>/dev/null || true
                fi
            fi
        fi
    done || true
fi || true

exit 0