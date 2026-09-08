@echo off
setlocal DisableDelayedExpansion
powershell.exe -NoLogo -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0tools\setup.ps1" -Mode Install
set "mgsSetupExit=%ERRORLEVEL%"
echo.
pause
exit /b %mgsSetupExit%
