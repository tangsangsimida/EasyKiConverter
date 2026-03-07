#!/bin/sh
set -e

# Pre-remove script
# 在卸载前清理桌面集成

# 在后台异步更新桌面数据库，避免阻塞卸载过程
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications >/dev/null 2>&1 &
fi

# 在后台异步更新图标缓存，避免阻塞卸载过程
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    for theme in hicolor Adwaita; do
        if [ -d "/usr/share/icons/$theme" ]; then
            gtk-update-icon-cache -q -t -f "/usr/share/icons/$theme" >/dev/null 2>&1 &
        fi
    done
fi

# 清理用户级别的桌面文件
# 检查是否有用户的桌面目录
if [ -n "$SUDO_USER" ]; then
    USER_HOME=$(eval echo ~$SUDO_USER)
    USER_DESKTOP="$USER_HOME/.local/share/applications"
    
    # 删除用户级别的 EasyKiConverter 桌面文件
    if [ -d "$USER_DESKTOP" ]; then
        for desktop_file in "$USER_DESKTOP"/*EasyKi*.desktop "$USER_DESKTOP"/*easykiconverter*.desktop; do
            if [ -f "$desktop_file" ]; then
                # 检查是否是当前应用创建的（通过检查 Exec 路径）
                if grep -q "/opt/easykiconverter" "$desktop_file" 2>/dev/null; then
                    rm -f "$desktop_file"
                    echo "已删除用户级别桌面文件: $desktop_file"
                fi
            fi
        done 2>/dev/null || true
        
        # 在后台异步更新用户桌面数据库
        if command -v update-desktop-database >/dev/null 2>&1; then
            update-desktop-database "$USER_DESKTOP" >/dev/null 2>&1 &
        fi
    fi
fi

# 不等待后台进程完成，立即退出
exit 0