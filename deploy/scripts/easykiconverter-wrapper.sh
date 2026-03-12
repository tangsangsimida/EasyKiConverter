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

# 执行 AppRun，传递所有参数
exec "$APPDIR/AppRun" "$@"