@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM EasyKiConverter 代码格式化工具 (C++)
REM ============================================================================
REM 说明:
REM   本工具通过集成 clang-format 自动格式化项目中的 C++ 源代码，确保代码风格统一。
REM
REM 主要功能:
REM   1. 递归扫描 src 目录下的所有 .cpp 和 .h 文件。
REM   2. 使用项目根目录中的 .clang-format 配置文件进行就地修改。
REM   3. 统计并输出处理的文件总数和成功格式化的数量。
REM
REM 环境要求:
REM   - 操作系统: Windows
REM   - 依赖: 必须安装 clang-format 并将其路径添加到 PATH 环境变量中。
REM   - 配置文件: 项目根目录必须存在 .clang-format 文件。
REM
REM 使用方法:
REM   1. 打开终端 (CMD 或 PowerShell)。
REM   2. 进入项目 tools/windows 目录。
REM   3. 执行 format_code.bat。
REM
REM 注意事项:
REM   - 本脚本目前仅处理 C++ 文件，不处理 QML 文件。
REM   - 执行前请确保当前工作区已提交或备份，以防非预期修改。
REM ============================================================================

echo ========================================
echo EasyKiConverter Code Formatter
echo ========================================
echo.

REM ========================================
REM Configuration - Please modify paths below for your environment
REM ========================================

REM clang-format executable path
REM If clang-format is in system PATH, use "clang-format"
REM Otherwise, specify the full path
set CLANG_FORMAT=clang-format

REM ========================================
REM Check if clang-format is available
REM ========================================

where %CLANG_FORMAT% >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: clang-format not found in PATH: %CLANG_FORMAT%
    echo.
    echo Please ensure clang-format is installed and in your PATH,
    echo or set the full path in the configuration section above.
    echo.
    echo Example: set CLANG_FORMAT=C:\path\to\clang-format.exe
    echo.
    pause
    exit /b 1
)

echo INFO: Using clang-format: %CLANG_FORMAT%
echo.

REM ========================================
REM Format code
REM ========================================

set TOTAL_FILES=0
set FORMATTED_FILES=0

echo Starting formatting src directory (C++ files only)...
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
echo.
echo NOTE: QML files are not formatted by this tool.
echo       Use the dedicated QML formatter tool instead.
echo.

pause
