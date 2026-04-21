@echo off
setlocal
powershell -ExecutionPolicy Bypass -File "%~dp0install-startup.ps1"
if errorlevel 1 (
  echo Installation failed.
  pause
  exit /b 1
)
echo Installation complete.
pause
