@echo off
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install_raw_trigger_driver.ps1"
if errorlevel 1 echo Try right-clicking this file and selecting Run as administrator.
pause
