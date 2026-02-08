@echo off
setlocal enabledelayedexpansion

echo ========================================
echo EasyKiConverter QML Formatter
echo ========================================
echo.

REM ========================================
REM Configuration - Please modify paths below for your environment
REM ========================================

REM qmlformat executable path
REM If qmlformat is in system PATH, use "qmlformat"
REM Otherwise, specify the full path
set QMLFORMAT=qmlformat

REM ========================================
REM Check if qmlformat is available
REM ========================================

where %QMLFORMAT% >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: qmlformat not found in PATH: %QMLFORMAT%
    echo.
    echo Please ensure qmlformat is installed and in your PATH,
    echo or set the full path in the configuration section above.
    echo.
    echo Example: set QMLFORMAT=C:\path\to\qmlformat.exe
    echo.
    echo qmlformat is typically located in your Qt bin directory:
    echo   C:\Qt\6.x.x\mingw_64\bin\qmlformat.exe
    echo.
    pause
    exit /b 1
)

echo INFO: Using qmlformat: %QMLFORMAT%
echo.

REM ========================================
REM Format QML files
REM ========================================

set TOTAL_FILES=0
set FORMATTED_FILES=0

echo Starting formatting QML directory...
for /r "..\..\src\ui\qml" %%f in (*.qml) do (
    set /a TOTAL_FILES+=1
    echo Formatting: %%~nxf
    "%QMLFORMAT%" -i "..\..\.qmlformat.ini" "%%f"
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
echo Configuration file: .qmlformat.ini
echo NOTE: This tool only formats QML files.
echo       Use format_code.bat for C++ files.
echo.

pause
