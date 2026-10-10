@echo off
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install_raw_trigger_driver.ps1" %*
set "RC=%ERRORLEVEL%"
pause
exit /b %RC%
