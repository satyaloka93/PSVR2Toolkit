@echo off
cd /d "%~dp0"
echo Start SteamVR and connect both PSVR2 Sense controllers before continuing.
echo Do not run DSX or Enhanced DualSense Support's bundled UDPClient.exe.
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_bridge.ps1"
echo.
echo Bridge exited. Review any error above.
pause
