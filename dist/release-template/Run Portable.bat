@echo off
setlocal
start "" "%~dp0browser_guard.exe" --aggressive-memory --trim-interval-ms 3000
