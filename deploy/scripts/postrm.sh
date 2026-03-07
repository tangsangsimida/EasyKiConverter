#!/bin/sh
set -e

# 卸载后清理脚本
# 在软件包卸载后执行，清理桌面缓存和配置

# 更新桌面数据库（移除已卸载的桌面文件）
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications 2>/dev/null || true
fi

# 更新图标缓存（移除已卸载的图标）
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    for theme in hicolor Adwaita; do
        if [ -d "/usr/share/icons/$theme" ]; then
            gtk-update-icon-cache -q -t -f "/usr/share/icons/$theme" 2>/dev/null || true
        fi
    done
fi

# 删除自动启动文件（如果存在）
AUTOSTART_FILE="/etc/xdg/autostart/easykiconverter-register.desktop"
if [ -f "$AUTOSTART_FILE" ]; then
    rm -f "$AUTOSTART_FILE"
fi

# 注意：不删除用户级别的配置文件
# 用户的配置文件位于 ~/.config/EasyKiConverter/
# 如果需要删除，可以取消下面的注释（但通常不建议自动删除）
# if [ -d "$HOME/.config/EasyKiConverter" ]; then
#     rm -rf "$HOME/.config/EasyKiConverter"
# fi

exit 0