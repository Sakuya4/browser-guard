param(
    [string]$ExecutablePath = "",
    [string]$InstallDirectory = "$env:LOCALAPPDATA\browser_guard",
    [string]$Arguments = "--aggressive-memory --trim-interval-ms 3000",
    [switch]$Overwrite
)

$ErrorActionPreference = "Stop"
function Resolve-BrowserGuardExecutable {
    param(
        [string]$ScriptRoot,
        [string]$ExecutablePath
    )

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($ExecutablePath)) {
        if ([System.IO.Path]::IsPathRooted($ExecutablePath)) {
            $candidates += $ExecutablePath
        } else {
            $candidates += (Join-Path $ScriptRoot $ExecutablePath)
            $candidates += (Join-Path (Split-Path -Parent $ScriptRoot) $ExecutablePath)
        }
    }

    $candidates += (Join-Path $ScriptRoot "browser_guard.exe")
    $candidates += (Join-Path (Split-Path -Parent $ScriptRoot) "build\Release\browser_guard.exe")

    foreach ($candidate in $candidates) {
        $fullPath = [System.IO.Path]::GetFullPath($candidate)
        if (Test-Path $fullPath) {
            return $fullPath
        }
    }

    return $null
}

$exeFullPath = Resolve-BrowserGuardExecutable -ScriptRoot $PSScriptRoot -ExecutablePath $ExecutablePath
if ($null -eq $exeFullPath) {
    throw "Executable not found. Put browser_guard.exe next to install-startup.ps1 or pass -ExecutablePath explicitly."
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
