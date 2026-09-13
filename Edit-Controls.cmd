@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -STA -File "%~dp0tools\edit-controls.ps1" %*
