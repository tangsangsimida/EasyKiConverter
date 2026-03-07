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
DESKTOP_FILE="/usr/share/applications/com.tangsangsimida.EasyKiConverter.desktop"
if [ -f "$DESKTOP_FILE" ]; then
    chmod 644 "$DESKTOP_FILE"
else
    echo "警告: 桌面文件不存在: $DESKTOP_FILE"
fi

# 在后台异步更新桌面数据库，避免阻塞安装过程
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications >/dev/null 2>&1 &
fi

# 在后台异步更新图标缓存，避免阻塞安装过程
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    for theme in hicolor Adwaita; do
        if [ -d "/usr/share/icons/$theme" ]; then
            gtk-update-icon-cache -q -t -f "/usr/share/icons/$theme" >/dev/null 2>&1 &
        fi
    done
fi

# 对于 GNOME 桌面环境，尝试重新加载应用列表
if [ -n "$XDG_CURRENT_DESKTOP" ] && echo "$XDG_CURRENT_DESKTOP" | grep -qi "gnome"; then
    # 尝试通知 GNOME Shell 重新加载应用
    if command -v gdbus >/dev/null 2>&1; then
        gdbus call --session --dest org.gnome.Shell --object-path /org/gnome/Shell --method org.gnome.Shell.Eval "appSystem.reload()" >/dev/null 2>&1 &
    fi
fi

# 不等待后台进程完成，立即退出
exit 0