param(
    [int]$SampleSeconds = 12,
    [int]$IntervalMilliseconds = 1000,
    [string]$ExecutablePath = ".\build\Release\browser_guard.exe",
    [string]$CsvPath = ".\docs\memory-benchmark.csv",
    [string]$MarkdownPath = ".\docs\memory-benchmark.md"
)

$ErrorActionPreference = "Stop"
$supported = @("chrome", "msedge", "firefox", "brave", "opera", "vivaldi")

function Get-BrowserMemorySnapshot {
    $processes = Get-Process -ErrorAction SilentlyContinue | Where-Object { $supported -contains $_.ProcessName }
    [pscustomobject]@{
        Timestamp = Get-Date
        ProcessCount = ($processes | Measure-Object).Count
        WorkingSetMB = [math]::Round((($processes | Measure-Object -Property WorkingSet64 -Sum).Sum / 1MB), 2)
        PrivateMB = [math]::Round((($processes | Measure-Object -Property PrivateMemorySize64 -Sum).Sum / 1MB), 2)
    }
}

function Get-AverageSnapshot([object[]]$samples) {
    [pscustomobject]@{
        SampleCount = $samples.Count
        AvgProcessCount = [math]::Round((($samples | Measure-Object -Property ProcessCount -Average).Average), 2)
        AvgWorkingSetMB = [math]::Round((($samples | Measure-Object -Property WorkingSetMB -Average).Average), 2)
        AvgPrivateMB = [math]::Round((($samples | Measure-Object -Property PrivateMB -Average).Average), 2)
    }
}

function Collect-Samples([string]$phase, [int]$seconds, [int]$intervalMs) {
    $samples = @()
    $iterations = [Math]::Max([Math]::Floor(($seconds * 1000) / $intervalMs), 1)
    for ($i = 0; $i -lt $iterations; $i++) {
        $snapshot = Get-BrowserMemorySnapshot
        $snapshot | Add-Member -NotePropertyName Phase -NotePropertyValue $phase
        $samples += $snapshot
        Start-Sleep -Milliseconds $intervalMs
    }
    return ,$samples
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$exeFullPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $ExecutablePath))
$csvFullPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $CsvPath))
$mdFullPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $MarkdownPath))

if (-not (Test-Path $exeFullPath)) {
    throw "Executable not found at $exeFullPath"
}

New-Item -ItemType Directory -Force -Path ([System.IO.Path]::GetDirectoryName($csvFullPath)) | Out-Null
New-Item -ItemType Directory -Force -Path ([System.IO.Path]::GetDirectoryName($mdFullPath)) | Out-Null

Write-Host "Collecting baseline samples..."
$baselineSamples = Collect-Samples -phase "baseline" -seconds $SampleSeconds -intervalMs $IntervalMilliseconds

Write-Host "Starting browser_guard with aggressive memory settings..."
$guardProcess = Start-Process -FilePath $exeFullPath -ArgumentList @("--aggressive-memory", "--trim-interval-ms", "3000") -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 3

try {
    Write-Host "Collecting guarded samples..."
    $guardedSamples = Collect-Samples -phase "guarded" -seconds $SampleSeconds -intervalMs $IntervalMilliseconds
}
finally {
    if ($null -ne $guardProcess -and -not $guardProcess.HasExited) {
        Stop-Process -Id $guardProcess.Id -Force
    }
}

$allSamples = @($baselineSamples + $guardedSamples)
$allSamples | Export-Csv -Path $csvFullPath -NoTypeInformation -Encoding UTF8

$baseline = Get-AverageSnapshot $baselineSamples
$guarded = Get-AverageSnapshot $guardedSamples
$workingSetDelta = [math]::Round(($guarded.AvgWorkingSetMB - $baseline.AvgWorkingSetMB), 2)
$privateDelta = [math]::Round(($guarded.AvgPrivateMB - $baseline.AvgPrivateMB), 2)
$workingSetPercent = if ($baseline.AvgWorkingSetMB -gt 0) { [math]::Round((($workingSetDelta / $baseline.AvgWorkingSetMB) * 100), 2) } else { 0 }
$privatePercent = if ($baseline.AvgPrivateMB -gt 0) { [math]::Round((($privateDelta / $baseline.AvgPrivateMB) * 100), 2) } else { 0 }

$report = @"
# Memory Benchmark

Generated: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")

This report compares supported browser memory usage before and after browser_guard was running with aggressive memory mode enabled.

## Method

- Sample window per phase: $SampleSeconds seconds
- Sample interval: $IntervalMilliseconds ms
- Guard command: browser_guard.exe --aggressive-memory --trim-interval-ms 3000
- Browsers included: chrome, msedge, firefox, brave, opera, vivaldi
- Recommended test condition: keep the browser open, then focus another app during the guarded phase

## Summary

| Phase | Avg process count | Avg working set (MB) | Avg private bytes (MB) |
| --- | ---: | ---: | ---: |
| Baseline | $($baseline.AvgProcessCount) | $($baseline.AvgWorkingSetMB) | $($baseline.AvgPrivateMB) |
| Guarded | $($guarded.AvgProcessCount) | $($guarded.AvgWorkingSetMB) | $($guarded.AvgPrivateMB) |

## Delta

| Metric | Delta (MB) | Percent |
| --- | ---: | ---: |
| Working set | $workingSetDelta | $workingSetPercent% |
| Private bytes | $privateDelta | $privatePercent% |

## Interpretation

- Negative delta means memory usage dropped while browser_guard was running.
- Working set usually reacts faster than private bytes because trimming removes resident pages first.
- If the browser stayed focused or kept active audio output, the reduction can be small by design.
"@

Set-Content -Path $mdFullPath -Value $report -Encoding UTF8

Write-Host "Saved CSV to $csvFullPath"
Write-Host "Saved Markdown report to $mdFullPath"
