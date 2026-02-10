@echo off
setlocal enabledelayedexpansion

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