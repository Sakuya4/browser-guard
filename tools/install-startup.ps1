param(
    [string]$ExecutablePath = "",
    [string]$ControlExecutablePath = "",
    [string]$InstallDirectory = "$env:LOCALAPPDATA\browser_guard",
    [string]$Arguments = "--aggressive-memory --aggressive-suspend --trim-interval-ms 3000 --background-grace-ms 2500 --manual-resume-grace-ms 8000 --heartbeat-interval-ms 5000 --window-probe-timeout-ms 750",
    [switch]$Overwrite
)

$ErrorActionPreference = "Stop"

function Stop-InstalledProcessIfRunning {
    param(
        [string]$ProcessName,
        [string]$ExpectedPath
    )

    $normalizedExpectedPath = [System.IO.Path]::GetFullPath($ExpectedPath)
    $matchingProcesses = Get-CimInstance Win32_Process -Filter "Name = '$ProcessName'" | Where-Object {
        $_.ExecutablePath -and ([System.IO.Path]::GetFullPath($_.ExecutablePath) -ieq $normalizedExpectedPath)
    }

    foreach ($process in $matchingProcesses) {
        Stop-Process -Id $process.ProcessId -Force -ErrorAction Stop
        Wait-Process -Id $process.ProcessId -Timeout 5 -ErrorAction SilentlyContinue
    }
}

function Resolve-ExecutableCandidate {
    param(
        [string]$ScriptRoot,
        [string]$ExecutablePath,
        [string[]]$DefaultRelativePaths = @()
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

    foreach ($relativePath in $DefaultRelativePaths) {
        $candidates += (Join-Path $ScriptRoot $relativePath)
        $candidates += (Join-Path (Split-Path -Parent $ScriptRoot) $relativePath)
    }

    foreach ($candidate in $candidates) {
        $fullPath = [System.IO.Path]::GetFullPath($candidate)
        if (Test-Path $fullPath) {
            return $fullPath
        }
    }

    return $null
}

$exeFullPath = Resolve-ExecutableCandidate -ScriptRoot $PSScriptRoot -ExecutablePath $ExecutablePath -DefaultRelativePaths @(
    "browser_guard.exe",
    "build\Release\browser_guard.exe"
)
if ($null -eq $exeFullPath) {
    throw "Executable not found. Put browser_guard.exe next to install-startup.ps1 or pass -ExecutablePath explicitly."
}

$controlExeFullPath = Resolve-ExecutableCandidate -ScriptRoot $PSScriptRoot -ExecutablePath $ControlExecutablePath -DefaultRelativePaths @(
    "browser_guard_control.exe",
    "build\Release\browser_guard_control.exe"
)
if ($null -eq $controlExeFullPath) {
    throw "Control executable not found. Put browser_guard_control.exe next to install-startup.ps1 or pass -ControlExecutablePath explicitly."
}

New-Item -ItemType Directory -Force -Path $InstallDirectory | Out-Null
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

if ((Test-Path $installedExePath) -and -not $Overwrite) {
    throw "browser_guard.exe already exists at $installedExePath. Use -Overwrite to replace it."
}

if ($Overwrite) {
    Stop-InstalledProcessIfRunning -ProcessName "browser_guard.exe" -ExpectedPath $installedExePath
    Stop-InstalledProcessIfRunning -ProcessName "browser_guard_control.exe" -ExpectedPath $installedControlExePath
}

Copy-Item -Force -Path $exeFullPath -Destination $installedExePath
Copy-Item -Force -Path $controlExeFullPath -Destination $installedControlExePath
Set-Content -Path $installedArgsPath -Value $Arguments -Encoding Ascii
Remove-Item -LiteralPath $disabledFlagPath -Force -ErrorAction SilentlyContinue
foreach ($legacyPath in $legacyPaths) {
    Remove-Item -LiteralPath $legacyPath -Force -ErrorAction SilentlyContinue
}

$startupFolder = [Environment]::GetFolderPath("Startup")
$shortcutPath = Join-Path $startupFolder "browser_guard.lnk"
$desktopFolder = [Environment]::GetFolderPath("Desktop")
$desktopShortcutPath = Join-Path $desktopFolder "browser_guard Toggle.lnk"
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $installedControlExePath
$shortcut.Arguments = "--launch"
$shortcut.WorkingDirectory = $InstallDirectory
$shortcut.Description = "Suspend background browsers and trim working sets"
$shortcut.IconLocation = "$installedControlExePath,0"
$shortcut.Save()

$desktopShortcut = $shell.CreateShortcut($desktopShortcutPath)
$desktopShortcut.TargetPath = $installedControlExePath
$desktopShortcut.Arguments = ""
$desktopShortcut.WorkingDirectory = $InstallDirectory
$desktopShortcut.IconLocation = "$installedControlExePath,0"
$desktopShortcut.Description = "Toggle browser_guard on or off"
$desktopShortcut.Save()

Write-Host "Installed browser_guard to $installedExePath"
Write-Host "Installed browser_guard_control to $installedControlExePath"
Write-Host "Startup shortcut created at $shortcutPath"
Write-Host "Desktop toggle shortcut created at $desktopShortcutPath"
