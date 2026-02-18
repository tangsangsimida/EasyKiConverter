@echo off
setlocal enabledelayedexpansion

:: EasyKiConverter C++ 代码格式化工具
:: 使用 clang-format 格式化项目中的 C++ 源代码

:: 检查帮助请求
if "%~1"=="-h" goto help
if "%~1"=="--help" goto help
if "%~1"=="/h" goto help
if "%~1"=="help" goto help

echo ========================================
echo EasyKiConverter Code Formatter
echo ========================================
echo.

:: clang-format 可执行文件路径
set CLANG_FORMAT=clang-format

:: 检查 clang-format 是否可用
where %CLANG_FORMAT% >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] clang-format not found
    echo Please install clang-format and add to PATH
    pause
    exit /b 1
)

echo [INFO] Using clang-format: %CLANG_FORMAT%
echo.

:: 格式化代码
set TOTAL_FILES=0
set FORMATTED_FILES=0

echo Formatting src directory...
for /r "..\..\src" %%f in (*.h *.cpp) do (
    set /a TOTAL_FILES+=1
    echo Formatting: %%~nxf
    "%CLANG_FORMAT%" -i "%%f"
    if !errorlevel! equ 0 (
        set /a FORMATTED_FILES+=1
    )
)

echo.
echo ========================================
echo Formatting Complete
echo ========================================
echo Total files: %TOTAL_FILES%
echo Formatted successfully: %FORMATTED_FILES%
echo NOTE: This tool only handles C++ files
echo       Use format_qml.bat for QML files
echo.

pause
exit /b 0

:help
echo EasyKiConverter C++ 代码格式化工具
echo.
echo 用法: format_code.bat [选项]
echo.
echo 选项:
echo   -h, --help, /h, help    显示此帮助信息
echo.
echo 功能说明:
echo   使用 clang-format 格式化 C++ 源代码
echo 环境要求:
echo   - clang-format 已安装并添加到 PATH
echo 示例:
echo   format_code.bat
echo 注意:
echo   - 执行前请确保工作区已提交或备份
echo   - 本工具仅处理 C++ 文件
echo   - QML 文件请使用 format_qml.bat
echo.
pause
exit /b 0