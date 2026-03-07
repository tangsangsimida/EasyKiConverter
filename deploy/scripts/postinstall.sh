#!/bin/sh
set -e

# 确保符号链接和目标文件有执行权限
if [ -L "/usr/bin/easykiconverter" ]; then
    # 设置符号链接本身为可执行（虽然符号链接权限通常被忽略）
    chmod +x /usr/bin/easykiconverter 2>/dev/null || true
    # 确保目标文件 AppRun 有执行权限
    chmod +x /opt/easykiconverter/AppRun 2>/dev/null || true
elif [ -f "/usr/bin/easykiconverter" ]; then
    # 如果不是符号链接，直接设置执行权限
    chmod +x /usr/bin/easykiconverter 2>/dev/null || true
fi

# 更新桌面数据库
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications || true
fi

# 更新图标缓存
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    for theme in hicolor Adwaita; do
        if [ -d "/usr/share/icons/$theme" ]; then
            gtk-update-icon-cache -q -t -f "/usr/share/icons/$theme" || true
        fi
    done
fi

exit 0