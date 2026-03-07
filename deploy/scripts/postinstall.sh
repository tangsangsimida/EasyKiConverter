#!/bin/sh
set -e

# 确保 AppRun 有执行权限
if [ -f "/opt/easykiconverter/AppRun" ]; then
    chmod +x /opt/easykiconverter/AppRun
fi

# 确保包装脚本有执行权限
if [ -f "/usr/bin/easykiconverter" ]; then
    chmod +x /usr/bin/easykiconverter
fi

# 验证桌面文件存在且可读
DESKTOP_FILE="/usr/share/applications/com.tangsangsimida.easykiconverter.desktop"
DESKTOP_FILE_OLD="/usr/share/applications/com.tangsangsimida.EasyKiConverter.desktop"
if [ -f "$DESKTOP_FILE" ]; then
    chmod 644 "$DESKTOP_FILE"
else
    echo "警告: 桌面文件不存在: $DESKTOP_FILE"
fi

# 删除旧版本的桌面文件（兼容性）
if [ -f "$DESKTOP_FILE_OLD" ]; then
    rm -f "$DESKTOP_FILE_OLD"
fi

# 同步更新桌面数据库（前台执行，确保完成）
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications 2>/dev/null || true
fi

# 同步更新图标缓存（前台执行，确保完成）
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    for theme in hicolor Adwaita; do
        if [ -d "/usr/share/icons/$theme" ]; then
            gtk-update-icon-cache -q -t -f "/usr/share/icons/$theme" 2>/dev/null || true
        fi
    done
fi

# 刷新 AppStream 缓存（前台执行，确保完成）
if command -v appstreamcli >/dev/null 2>&1; then
    appstreamcli refresh-cache --force 2>/dev/null || true
fi

# 强制更新 systemd 的用户会话（如果存在）
if command -v systemctl >/dev/null 2>&1; then
    # 尝试触发桌面环境的重新加载（不需要用户登录）
    # 使用 systemd-run 在系统级别运行，不依赖用户会话
    systemctl reload dbus 2>/dev/null || true
fi

# 为所有已登录用户触发 GNOME 应用列表刷新
# 使用 trigger-gnome-refresh.sh 脚本来触发刷新
if [ -x "/usr/share/easykiconverter/trigger-gnome-refresh.sh" ]; then
    # 获取所有已登录用户
    loginctl list-users 2>/dev/null | awk '{print $2}' | while read -r user; do
        if [ -n "$user" ] && [ "$user" != "USER" ]; then
            # 为每个用户触发 GNOME 刷新
            /usr/share/easykiconverter/trigger-gnome-refresh.sh "$user" 2>/dev/null || true
        fi
    done
fi

exit 0