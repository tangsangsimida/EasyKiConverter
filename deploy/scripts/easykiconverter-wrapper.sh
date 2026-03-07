#!/bin/bash
# EasyKiConverter 包装脚本
# 这个脚本调用 /opt/easykiconverter/AppRun 来启动应用程序

set -e

# 定义 AppDir 路径
APPDIR="/opt/easykiconverter"

# 检查 AppRun 是否存在且可执行
if [ ! -x "$APPDIR/AppRun" ]; then
    echo "错误: $APPDIR/AppRun 不存在或不可执行"
    echo "请确保 EasyKiConverter 已正确安装"
    exit 1
fi

# 触发 GNOME 应用列表刷新（如果可用）
if command -v gdbus >/dev/null 2>&1; then
    # 使用 gdbus 命令强制刷新 GNOME Shell 应用列表
    # 这是最有效的方法，会立即触发 GNOME Shell 重新加载应用列表
    gdbus call --session --dest org.gnome.Shell --object-path /org/gnome/Shell --method org.gnome.Shell.Eval "appSystem.reload()" 2>/dev/null || true
fi

# 备用方案：使用 gio 命令触发应用注册
if command -v gio >/dev/null 2>&1; then
    # 使用 gio 命令触发应用注册（不会实际启动应用）
    # 这会强制 GNOME Shell 重新加载应用列表
    gio launch com.tangsangsimida.EasyKiConverter --gapplication-service 2>/dev/null || true
fi

# 执行 AppRun，传递所有参数
exec "$APPDIR/AppRun" "$@"