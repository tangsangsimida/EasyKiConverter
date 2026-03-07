#!/bin/bash
# EasyKiConverter 注册脚本
# 此脚本在首次登录时运行，用于向 GNOME Shell 注册应用

# 检查是否有待处理的刷新请求
PENDING_REFRESH_FILE="$HOME/.config/easykiconverter/pending-refresh"
if [ -f "$PENDING_REFRESH_FILE" ]; then
    # 强制刷新 GNOME 应用列表
    if command -v gdbus >/dev/null 2>&1; then
        # 尝试使用 D-Bus 刷新 GNOME Shell
        gdbus call --session --dest org.gnome.Shell --object-path /org/gnome/Shell --method org.gnome.Shell.Eval "appSystem.reload()" 2>/dev/null || true
    fi

    # 尝试使用 gio 触发应用注册
    if command -v gio >/dev/null 2>&1; then
        gio launch com.tangsangsimida.EasyKiConverter --gapplication-service 2>/dev/null || true
    fi

    # 删除标记文件
    rm -f "$PENDING_REFRESH_FILE"
fi

# 检查是否已经注册过
MARKER_FILE="$HOME/.config/easykiconverter-registered"
if [ -f "$MARKER_FILE" ]; then
    # 已经注册过，删除自启动文件并退出
    AUTO_START_FILE="$HOME/.config/autostart/easykiconverter-register.desktop"
    if [ -f "$AUTO_START_FILE" ]; then
        rm -f "$AUTO_START_FILE"
    fi
    exit 0
fi

# 使用 gio 命令触发应用注册（不会实际启动应用）
if command -v gio >/dev/null 2>&1; then
    gio launch com.tangsangsimida.EasyKiConverter --gapplication-service 2>/dev/null || true
fi

# 创建标记文件
mkdir -p "$HOME/.config/easykiconverter"
touch "$MARKER_FILE"

# 删除自启动文件
AUTO_START_FILE="$HOME/.config/autostart/easykiconverter-register.desktop"
if [ -f "$AUTO_START_FILE" ]; then
    rm -f "$AUTO_START_FILE"
fi

exit 0