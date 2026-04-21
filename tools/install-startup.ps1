param(
    [string]$ExecutablePath = ".\build\Release\browser_guard.exe",
    [string]$InstallDirectory = "$env:LOCALAPPDATA\browser_guard",
    [string]$Arguments = "--aggressive-memory --trim-interval-ms 3000",
    [switch]$Overwrite
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$exeFullPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $ExecutablePath))

if (-not (Test-Path $exeFullPath)) {
    throw "Executable not found at $exeFullPath"
}

New-Item -ItemType Directory -Force -Path $InstallDirectory | Out-Null
$installedExePath = Join-Path $InstallDirectory "browser_guard.exe"

if ((Test-Path $installedExePath) -and -not $Overwrite) {
    throw "browser_guard.exe already exists at $installedExePath. Use -Overwrite to replace it."
}

Copy-Item -Force -Path $exeFullPath -Destination $installedExePath

$startupFolder = [Environment]::GetFolderPath("Startup")
$shortcutPath = Join-Path $startupFolder "browser_guard.lnk"
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $installedExePath
$shortcut.Arguments = $Arguments
$shortcut.WorkingDirectory = $InstallDirectory
$shortcut.Description = "Suspend background browsers and trim working sets"
$shortcut.Save()

Write-Host "Installed browser_guard to $installedExePath"
Write-Host "Startup shortcut created at $shortcutPath"
