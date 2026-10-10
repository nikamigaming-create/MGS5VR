@echo off
if not exist "%~dp0play\BUILD.json" (
  echo The pinned play build is missing. Build development, then explicitly pin-build after review.
  pause
  exit /b 1
)
if not exist "%~dp0artifacts\dev" mkdir "%~dp0artifacts\dev"
python "%~dp0tools\workspace.py" deploy --slot play > "%~dp0artifacts\dev\headset-launch.log" 2>&1
if errorlevel 1 goto failed
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\launch-headset.ps1" %* >> "%~dp0artifacts\dev\headset-launch.log" 2>&1
:failed
set "MGS_HEADSET_EXIT=%ERRORLEVEL%"
type "%~dp0artifacts\dev\headset-launch.log"
if not "%MGS_HEADSET_EXIT%"=="0" (
  echo Headset launch failed. Details are saved in artifacts\dev\headset-launch.log.
  pause
)
exit /b %MGS_HEADSET_EXIT%
