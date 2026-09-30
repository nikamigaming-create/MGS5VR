@echo off
if not exist "%~dp0play\BUILD.json" (
  echo Run tools\build.ps1 once to create the current test build.
  pause
  exit /b 1
)
if not exist "%~dp0artifacts\dev" mkdir "%~dp0artifacts\dev"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0play\tools\launch-headset.ps1" %* > "%~dp0artifacts\dev\headset-launch.log" 2>&1
set "MGS_HEADSET_EXIT=%ERRORLEVEL%"
type "%~dp0artifacts\dev\headset-launch.log"
if not "%MGS_HEADSET_EXIT%"=="0" (
  echo Headset launch failed. Details are saved in artifacts\dev\headset-launch.log.
  pause
)
exit /b %MGS_HEADSET_EXIT%
