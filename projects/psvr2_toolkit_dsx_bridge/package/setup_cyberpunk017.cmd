@echo off
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup_cyberpunk017.ps1" %*
set "RC=%ERRORLEVEL%"
pause
exit /b %RC%
