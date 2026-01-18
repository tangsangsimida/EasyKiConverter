@echo off
REM EasyKiConverter 测试运行脚本
REM 用于运行单元测试并生成覆盖率报告

echo ========================================
echo EasyKiConverter 测试运行脚本
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

echo [1/5] 清理旧的构建文件...
if exist build\tests rmdir /s /q build\tests
if exist build\coverage rmdir /s /q build\coverage

echo [2/5] 配置测试项目（启用覆盖率）...
cd tests
cmake -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="%QT_PATH%" -DENABLE_COVERAGE=ON
if %errorlevel% neq 0 (
    echo 配置失败！
    pause
    exit /b 1
)

echo [3/5] 编译测试项目...
cmake --build build --config Debug
if %errorlevel% neq 0 (
    echo 编译失败！
    pause
    exit /b 1
)

echo [4/5] 运行测试...
cd build
ctest --output-on-failure
if %errorlevel% neq 0 (
    echo 测试失败！
    pause
    exit /b 1
)

echo [5/5] 生成覆盖率报告...
cmake --build . --target coverage
if %errorlevel% neq 0 (
    echo 覆盖率报告生成失败！
    echo 可能是因为未安装 gcovr
    echo 请运行: pip install gcovr
    pause
    exit /b 0
)

echo.
echo ========================================
echo 测试完成！
echo ========================================
echo.
echo 覆盖率报告位置:
echo - HTML: build\coverage\index.html
echo - XML:  build\coverage.xml
echo.
echo 正在打开 HTML 报告...
start build\coverage\index.html

pause