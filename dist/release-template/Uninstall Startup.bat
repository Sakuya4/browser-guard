@echo off
setlocal
powershell -ExecutionPolicy Bypass -File "%~dp0uninstall-startup.ps1"
if errorlevel 1 (
  echo Uninstall failed.
  pause
  exit /b 1
)
echo Uninstall complete.
pause
