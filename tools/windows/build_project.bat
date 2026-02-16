@echo off
setlocal enabledelayedexpansion

:: EasyKiConverter 构建脚本
:: 功能：Python 脚本的轻量级包装器

:: 检查帮助请求 (必须在最开始)
if "%~1"=="-h" goto help
if "%~1"=="--help" goto help
if "%~1"=="/h" goto help
if "%~1"=="help" goto help

:: 检查 Python
where python >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Python interpreter not found
    echo Please install Python 3.6+ and add to PATH
    pause
    exit /b 1
)

:: 检查 Python 版本 (需要 3.x)
for /f "tokens=2" %%i in ('python --version') do set py_ver=%%i
echo %py_ver% | findstr "^3\." >nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Python version too low (%py_ver%), need 3.6+
    pause
    exit /b 1
)

:: 获取项目根目录
set "PROJECT_ROOT=%~dp0..\"
cd /d "%PROJECT_ROOT%\"

:: 显示构建信息
echo ============================================================
echo EasyKiConverter Build
echo ============================================================
echo Project root: %PROJECT_ROOT%
echo Python version: %py_ver%
echo.

:: 调用 Python 构建管理器
python tools\python\build_project.py %*

:: 检查执行结果
if %ERRORLEVEL% neq 0 (
    echo.
    echo ============================================================
    echo [ERROR] Build failed, see build.log for details
    echo ============================================================
    pause
    exit /b 1
)

echo.
echo ============================================================
echo Build completed successfully!
echo ============================================================
pause
goto :eof

:help
echo EasyKiConverter 构建工具快捷入口
echo.
echo 用法:
echo   build_project.bat                 - 执行默认 Debug 构建
echo   build_project.bat -t Release      - 执行 Release 构建
echo   build_project.bat -c              - 清理构建目录 (会有确认提示)
echo   build_project.bat -c -y           - 强制清理并构建
echo   build_project.bat --check         - 仅执行环境依赖检查
echo   build_project.bat --config-only   - 仅执行 CMake 配置，不进行编译
echo   build_project.bat -t Release -i   - 构建 Release 并安装
echo   build_project.bat -v              - 详细日志模式
echo.
pause
goto :eof
