@echo off
chcp 65001 >nul
fltmc >nul 2>&1
if %errorlevel% NEQ 0 (
  echo Requesting administrator rights... please click YES
  powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
  exit /b
)
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup.ps1"
echo.
echo ==== Done. Press any key to close ====
pause >nul
