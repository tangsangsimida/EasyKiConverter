#!/bin/bash
# GNOME 刷新触发脚本
# 此脚本由 postinstall.sh 调用，用于触发 GNOME 应用列表刷新

set -e

# 检查参数
if [ -z "$1" ]; then
    echo "用法: $0 <username>"
    exit 1
fi

USERNAME="$1"

# 获取用户信息
USER_INFO=$(getent passwd "$USERNAME")
if [ -z "$USER_INFO" ]; then
    echo "错误: 用户 $USERNAME 不存在"
    exit 1
fi

# 解析用户信息
USER_HOME=$(echo "$USER_INFO" | cut -d: -f6)
USER_ID=$(echo "$USER_INFO" | cut -d: -f3)

# 只为普通用户运行（UID >= 1000）
if [ "$USER_ID" -lt 1000 ]; then
    exit 0
fi

# 检查用户是否已登录
if ! loginctl list-users 2>/dev/null | grep -q "$USERNAME"; then
    exit 0
fi

# 获取用户的活动会话
SESSIONS=$(loginctl list-sessions "$USERNAME" 2>/dev/null | awk '{print $1}' | grep -v "^$")

if [ -z "$SESSIONS" ]; then
    exit 0
fi

# 遍历所有活动会话
for SESSION in $SESSIONS; do
    # 获取会话的 XDG_RUNTIME_DIR
    RUNTIME_DIR=$(loginctl show-session "$SESSION" -p XDG_RUNTIME_DIR --value 2>/dev/null || echo "")
    
    if [ -z "$RUNTIME_DIR" ]; then
        continue
    fi
    
    # 检查 D-Bus 会话总线是否存在
    DBUS_BUS="$RUNTIME_DIR/bus"
    if [ ! -S "$DBUS_BUS" ]; then
        continue
    fi
    
    # 设置环境变量
    export XDG_RUNTIME_DIR="$RUNTIME_DIR"
    export DBUS_SESSION_BUS_ADDRESS="unix:path=$DBUS_BUS"
    
    # 使用 sudo -u 切换用户执行 gdbus 命令
    # 直接调用 GNOME Shell 的 appSystem.reload() 方法
    sudo -u "$USERNAME" \
        env XDG_RUNTIME_DIR="$RUNTIME_DIR" \
        DBUS_SESSION_BUS_ADDRESS="unix:path=$DBUS_BUS" \
        gdbus call --session --dest org.gnome.Shell --object-path /org/gnome/Shell --method org.gnome.Shell.Eval "appSystem.reload()" 2>/dev/null || true
done

exit 0