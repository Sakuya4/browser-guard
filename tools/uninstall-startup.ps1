param(
    [string]$InstallDirectory = "$env:LOCALAPPDATA\browser_guard"
)

$ErrorActionPreference = "Stop"
$startupFolder = [Environment]::GetFolderPath("Startup")
$shortcutPath = Join-Path $startupFolder "browser_guard.lnk"
$desktopFolder = [Environment]::GetFolderPath("Desktop")
$desktopShortcutPath = Join-Path $desktopFolder "browser_guard Toggle.lnk"
$installedExePath = Join-Path $InstallDirectory "browser_guard.exe"
$installedControlExePath = Join-Path $InstallDirectory "browser_guard_control.exe"
$installedArgsPath = Join-Path $InstallDirectory "browser_guard.args.txt"
$disabledFlagPath = Join-Path $InstallDirectory "browser_guard.disabled"
$legacyPaths = @(
    (Join-Path $InstallDirectory "launch-browser-guard.bat"),
    (Join-Path $InstallDirectory "launch-browser-guard.ps1"),
    (Join-Path $InstallDirectory "toggle-browser-guard.bat"),
    (Join-Path $InstallDirectory "toggle-browser-guard.ps1")
)

if (Test-Path $shortcutPath) {
    Remove-Item -LiteralPath $shortcutPath -Force
}

if (Test-Path $desktopShortcutPath) {
    Remove-Item -LiteralPath $desktopShortcutPath -Force
}

if (Test-Path $installedExePath) {
    Remove-Item -LiteralPath $installedExePath -Force
}

if (Test-Path $installedControlExePath) {
    Remove-Item -LiteralPath $installedControlExePath -Force
}

if (Test-Path $installedArgsPath) {
    Remove-Item -LiteralPath $installedArgsPath -Force
}

if (Test-Path $disabledFlagPath) {
    Remove-Item -LiteralPath $disabledFlagPath -Force
}

foreach ($legacyPath in $legacyPaths) {
    if (Test-Path $legacyPath) {
        Remove-Item -LiteralPath $legacyPath -Force
    }
}

if (Test-Path $InstallDirectory) {
    $remaining = Get-ChildItem -LiteralPath $InstallDirectory -Force
    if ($remaining.Count -eq 0) {
        Remove-Item -LiteralPath $InstallDirectory -Force
    }
}

Write-Host "Removed startup shortcut and installed executable, if they existed."
