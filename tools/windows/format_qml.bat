@echo off
setlocal enabledelayedexpansion

:: EasyKiConverter QML 代码格式化工具
:: 使用 Python 工具调用 qmlformat 格式化项目中的 QML 源代码

:: 检查帮助请求
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

:: 获取项目根目录
set "PROJECT_ROOT=%~dp0..\.."
cd /d "%PROJECT_ROOT%\"

echo ========================================
echo EasyKiConverter QML Formatter
echo ========================================
echo.

:: 调用 Python 格式化工具
python tools\python\format_code.py --qml %*

:: 检查执行结果
if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Formatting failed
    pause
    exit /b 1
)

echo.
echo ========================================
echo Formatting Complete
echo ========================================
echo NOTE: This tool only handles QML files
echo       Use format_code.bat for C++ files
echo.

pause
exit /b 0

:help
echo EasyKiConverter QML 代码格式化工具
echo.
echo 用法: format_qml.bat [选项]
echo.
echo 选项:
echo   -h, --help, /h, help    显示此帮助信息
echo   -s, --fix-empty-lines  修复 QML 文件中的多余空行
echo   -v, --verbose          详细输出模式
echo   --check                仅检查不修改
echo   --dirs DIR [DIR]       指定格式化目录
echo.
echo 功能说明:
echo   使用 qmlformat 格式化 QML 源代码
echo   通过 Python 工具调用，支持缓存加速
echo.
echo 示例:
echo   format_qml.bat                 # 格式化 QML 文件
echo   format_qml.bat -s              # 修复多余空行
echo   format_qml.bat --check         # 检查格式
echo   format_qml.bat -v              # 详细输出
echo.
echo 注意:
echo   - 执行前请确保工作区已提交或备份
echo   - 本工具仅处理 QML 文件
echo   - C++ 文件请使用 format_code.bat
echo.
pause
exit /b 0
