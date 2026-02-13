@echo off
setlocal enabledelayedexpansion

:: ==============================================================================
:: EasyKiConverter 一键构建脚本 (One-Click Build Script)
::
:: 功能说明:
::     1. 清理并重新配置 CMake 构建环境。
::     2. 使用指定的构建系统（默认 Ninja 或 MSVC）进行编译。
::     3. 自动检测环境中的必要工具。
::
:: 使用示例:
::     build_project.bat          - 默认 Debug 模式构建
::     build_project.bat Release  - Release 模式构建
::     build_project.bat Clean    - 仅清理构建目录
::
:: 环境要求:
::     - 已安装 CMake
::     - 已安装构建器 (Ninja, MSVC, 或 MinGW)
::     - 已安装 Qt6 并配置好环境变量
:: ==============================================================================

:: ==============================================================================
:: 工具链路径配置 (如果工具不在 PATH 中，请在此处指定路径)
:: ==============================================================================
set "QT_DIR="       :: 例如: C:\Qt\6.6.1\mingw_64
set "CMAKE_DIR="    :: 例如: C:\Program Files\CMake\bin
:: ==============================================================================

echo ============================================================
echo EasyKiConverter 项目一键构建
echo ============================================================

:: 1. 检查环境变量
where cmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    if "!CMAKE_DIR!"=="" (
        echo [错误] 未在 PATH 中找到 cmake.exe，且未在脚本中配置 CMAKE_DIR。
        echo 请安装 CMake 并将其添加到环境变量，或修改此脚本开头。
        pause
        exit /b 1
    )
    set "PATH=!CMAKE_DIR!;%PATH%"
)

:: 2. 处理参数
set "BUILD_TYPE=Debug"
if /i "%~1"=="Release" set "BUILD_TYPE=Release"
if /i "%~1"=="Clean" (
    echo 正在清理构建目录...
    if exist "..\..\build" rd /s /q "..\..\build"
    echo 清理完成。
    exit /b 0
)

:: 3. 确定构建目录 (假设脚本在 tools/windows)
set "PROJECT_ROOT=%~dp0..\.."
set "BUILD_DIR=%PROJECT_ROOT%\build"

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo [配置] 构建模式: %BUILD_TYPE%
echo [配置] 项目根目录: %PROJECT_ROOT%
echo [配置] 构建目录: %BUILD_DIR%

:: 4. 配置并构建
cd /d "%BUILD_DIR%"

echo.
echo >>> 正在配置 CMake...
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% "%PROJECT_ROOT%"
if %ERRORLEVEL% neq 0 (
    echo [警告] 默认使用 Ninja 失败，尝试系统默认生成器...
    cmake -DCMAKE_BUILD_TYPE=%BUILD_TYPE% "%PROJECT_ROOT%"
    if %ERRORLEVEL% neq 0 (
        echo [错误] CMake 配置失败！
        echo 提示: 请检查是否安装了编译器（如 MSVC 或 MinGW）以及 Qt 是否配置正确。
        pause
        exit /b 1
    )
)

echo.
echo >>> 正在开始编译 (并行)...
cmake --build . --parallel %NUMBER_OF_PROCESSORS%
if %ERRORLEVEL% neq 0 (
    echo [错误] 编译失败！
    pause
    exit /b 1
)

echo.
echo ============================================================
echo 构建成功完成！
echo 可执行文件位于: %BUILD_DIR%\bin\EasyKiConverter.exe
echo ============================================================
pause
exit /b 0
