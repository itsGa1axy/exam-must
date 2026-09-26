@echo off
setlocal

rem Run the host programming script and keep the window open.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0flash.ps1" %*
set "exit_code=%ERRORLEVEL%"

if not "%exit_code%"=="0" (
    echo.
    echo Host programming failed. Exit code: %exit_code%
) else (
    echo.
    echo Host programming workflow finished.
)
pause
exit /b %exit_code%
