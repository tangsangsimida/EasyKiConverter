@echo off
setlocal enabledelayedexpansion

:: EasyKiConverter QML 代码格式化工具
:: 使用 qmlformat 格式化项目中的 QML 源代码

:: 检查帮助请求
if "%~1"=="-h" goto help
if "%~1"=="--help" goto help
if "%~1"=="/h" goto help
if "%~1"=="help" goto help

echo ========================================
echo EasyKiConverter QML Formatter
echo ========================================
echo.

:: qmlformat 可执行文件路径
set QMLFORMAT=qmlformat

:: 配置文件路径
set CONFIG_FILE=..\..\.qmlformat.json

:: 检查 qmlformat 是否可用
where %QMLFORMAT% >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] qmlformat not found
    echo Please install qmlformat and add to PATH
    echo qmlformat is typically in Qt bin directory:
    echo   C:\Qt\6.x.x\mingw_64\bin\qmlformat.exe
    pause
    exit /b 1
)

echo [INFO] Using qmlformat: %QMLFORMAT%
echo [INFO] Configuration file: %CONFIG_FILE%
echo.

:: 格式化 QML 文件
set TOTAL_FILES=0
set FORMATTED_FILES=0

echo Formatting QML directory...
echo Using configuration: .qmlformat.json
echo.

for /r "..\..\src\ui\qml" %%f in (*.qml) do (
    set /a TOTAL_FILES+=1
    echo Formatting: %%~nxf
    "%QMLFORMAT%" -i -s "%CONFIG_FILE%" "%%f"
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
echo Configuration file: .qmlformat.json
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
echo.
echo 功能说明:
echo   使用 qmlformat 格式化 QML 源代码
echo 环境要求:
echo   - qmlformat 已安装并添加到 PATH (Qt 6 组件)
echo 示例:
echo   format_qml.bat
echo 注意:
echo   - 执行前请确保工作区已提交或备份
echo   - 本工具仅处理 QML 文件
echo   - C++ 文件请使用 format_code.bat
echo.
pause
exit /b 0