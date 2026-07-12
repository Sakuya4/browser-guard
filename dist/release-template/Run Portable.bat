@echo off
setlocal
start "" "%~dp0browser_guard.exe" --aggressive-memory --minimized-only --trim-interval-ms 3000
