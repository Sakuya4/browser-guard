param(
    [string]$InstallDirectory = "$env:LOCALAPPDATA\browser_guard"
)

$ErrorActionPreference = "Stop"
$startupFolder = [Environment]::GetFolderPath("Startup")
$shortcutPath = Join-Path $startupFolder "browser_guard.lnk"
$installedExePath = Join-Path $InstallDirectory "browser_guard.exe"

if (Test-Path $shortcutPath) {
    Remove-Item -LiteralPath $shortcutPath -Force
}

if (Test-Path $installedExePath) {
    Remove-Item -LiteralPath $installedExePath -Force
}

if (Test-Path $InstallDirectory) {
    $remaining = Get-ChildItem -LiteralPath $InstallDirectory -Force
    if ($remaining.Count -eq 0) {
        Remove-Item -LiteralPath $InstallDirectory -Force
    }
}

Write-Host "Removed startup shortcut and installed executable, if they existed."
