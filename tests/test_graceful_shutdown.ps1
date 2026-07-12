param(
    [Parameter(Mandatory = $true)]
    [string]$GuardPath,

    [Parameter(Mandatory = $true)]
    [string]$ControllerPath
)

$ErrorActionPreference = "Stop"
$guardDirectory = Split-Path -Parent $GuardPath
$disabledPath = Join-Path $guardDirectory "browser_guard.disabled"
$guardProcess = $null

Remove-Item -LiteralPath $disabledPath -Force -ErrorAction SilentlyContinue

try {
    $guardProcess = Start-Process -FilePath $GuardPath -ArgumentList @(
        "--minimized-only",
        "--background-grace-ms",
        "60000"
    ) -PassThru -WindowStyle Hidden

    Start-Sleep -Milliseconds 500
    if ($guardProcess.HasExited) {
        throw "Guard exited before the shutdown request."
    }

    $controller = Start-Process -FilePath $ControllerPath -ArgumentList "--shutdown" -PassThru -Wait -WindowStyle Hidden
    if ($controller.ExitCode -ne 0) {
        throw "Controller returned exit code $($controller.ExitCode)."
    }

    $guardProcess.WaitForExit(5000) | Out-Null
    if (-not $guardProcess.HasExited) {
        throw "Guard did not exit after the graceful shutdown request."
    }
    if (Test-Path -LiteralPath $disabledPath) {
        throw "Dedicated shutdown mode must not create the toggle disabled flag."
    }
} finally {
    if ($null -ne $guardProcess -and -not $guardProcess.HasExited) {
        Stop-Process -Id $guardProcess.Id -Force -ErrorAction SilentlyContinue
    }
    Remove-Item -LiteralPath $disabledPath -Force -ErrorAction SilentlyContinue
}
