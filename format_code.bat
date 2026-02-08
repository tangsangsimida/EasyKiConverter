@echo off
chcp 65001 > nul
setlocal enabledelayedexpansion

echo ========================================
echo EasyKiConverter Code Formatter
echo ========================================
echo.

set CLANG_FORMAT=C:\software\QT\Tools\QtCreator\bin\clang\bin\clang-format.exe

if not exist "%CLANG_FORMAT%" (
    echo ERROR: clang-format not found: %CLANG_FORMAT%
    echo Please check the path
    pause
    exit /b 1
)

echo INFO: Using clang-format: %CLANG_FORMAT%
echo INFO: Using config: .clang-format
echo.

set TOTAL_FILES=0
set FORMATTED_FILES=0

echo Starting formatting src directory...
for /r "src" %%f in (*.h *.cpp) do (
    set /a TOTAL_FILES+=1
    echo Formatting: %%f
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

pause