@echo off
if not exist "%~dp0artifacts\dev" mkdir "%~dp0artifacts\dev"
python "%~dp0tools\workspace.py" deploy --slot dev > "%~dp0artifacts\dev\dev-headset-launch.log" 2>&1
if errorlevel 1 goto failed
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\launch-headset.ps1" %* >> "%~dp0artifacts\dev\dev-headset-launch.log" 2>&1
:failed
set "MGS_DEV_EXIT=%ERRORLEVEL%"
type "%~dp0artifacts\dev\dev-headset-launch.log"
if not "%MGS_DEV_EXIT%"=="0" (
  echo Development launch failed. The pinned play build is unchanged.
  pause
)
exit /b %MGS_DEV_EXIT%
