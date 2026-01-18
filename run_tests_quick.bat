@echo off
REM EasyKiConverter 快速测试运行脚本
REM 用于开发过程中快速运行测试（不生成覆盖率报告）

echo ========================================
echo EasyKiConverter 快速测试
echo ========================================
echo.

REM 设置 Qt 路径（根据你的实际安装路径修改）
set QT_PATH=C:\software\QT\6.10.1\mingw_64

REM 检查 Qt 路径是否存在
if not exist "%QT_PATH%" (
    echo 错误: Qt 路径不存在: %QT_PATH%
    echo 请修改脚本中的 QT_PATH 变量
    pause
    exit /b 1
)

echo [1/3] 配置测试项目...
cd tests
cmake -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="%QT_PATH%"
if %errorlevel% neq 0 (
    echo 配置失败！
    pause
    exit /b 1
)

echo [2/3] 编译测试项目...
cmake --build build --config Debug
if %errorlevel% neq 0 (
    echo 编译失败！
    pause
    exit /b 1
)

echo [3/3] 运行测试...
cd build
ctest --output-on-failure
if %errorlevel% neq 0 (
    echo 测试失败！
    pause
    exit /b 1
)

echo.
echo ========================================
echo 所有测试通过！
echo ========================================
echo.

pause