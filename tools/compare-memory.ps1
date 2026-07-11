param(
    [ValidateRange(2, 20)][int]$TrialCount = 5,
    [ValidateRange(5, 300)][int]$PhaseSeconds = 15,
    [ValidateRange(250, 5000)][int]$IntervalMilliseconds = 1000,
    [ValidateRange(0, 30)][int]$WarmupSeconds = 3,
    [string]$ExecutablePath = ".\build\Release\browser_guard.exe",
    [string]$ControllerPath = ".\build\Release\browser_guard_control.exe",
    [string]$CsvPath = ".\docs\memory-benchmark.csv",
    [string]$MarkdownPath = ".\docs\memory-benchmark.md"
)

$ErrorActionPreference = "Stop"
$supported = @("chrome.exe", "msedge.exe", "firefox.exe", "brave.exe", "opera.exe", "vivaldi.exe")
$guardArguments = @(
    "--aggressive-memory",
    "--minimized-only",
    "--background-grace-ms", "1000",
    "--heartbeat-interval-ms", "60000",
    "--trim-interval-ms", "3000"
)

function Resolve-RepositoryPath([string]$Path, [string]$Root) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $Root $Path))
}

function Get-Sum([object[]]$Items, [string]$Property) {
    $sum = ($Items | Measure-Object -Property $Property -Sum).Sum
    if ($null -eq $sum) { return 0 }
    return [double]$sum
}

function Get-BrowserSnapshot([int]$Trial, [string]$Order, [string]$Phase) {
    $processes = @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue | Where-Object {
        $supported -contains $_.Name
    })
    $cpu100ns = (Get-Sum $processes "KernelModeTime") + (Get-Sum $processes "UserModeTime")

    [pscustomobject]@{
        Trial = $Trial
        Order = $Order
        Phase = $Phase
        TimestampUtc = [DateTime]::UtcNow.ToString("o")
        ProcessCount = $processes.Count
        WorkingSetMB = [math]::Round((Get-Sum $processes "WorkingSetSize") / 1MB, 3)
        PrivateMB = [math]::Round((Get-Sum $processes "PrivatePageCount") / 1MB, 3)
        CpuSeconds = [math]::Round($cpu100ns / 10000000, 4)
        PageFaults = [math]::Round((Get-Sum $processes "PageFaults"), 0)
    }
}

function Collect-Samples([int]$Trial, [string]$Order, [string]$Phase) {
    $samples = @()
    $iterations = [Math]::Max([Math]::Floor(($PhaseSeconds * 1000) / $IntervalMilliseconds), 1)
    for ($index = 0; $index -lt $iterations; $index++) {
        $samples += Get-BrowserSnapshot -Trial $Trial -Order $Order -Phase $Phase
        if ($index + 1 -lt $iterations) {
            Start-Sleep -Milliseconds $IntervalMilliseconds
        }
    }
    return ,$samples
}

function Stop-GuardSafely([string]$ResolvedControllerPath) {
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    $controller = Start-Process -FilePath $ResolvedControllerPath -ArgumentList "--shutdown" -PassThru -Wait -WindowStyle Hidden
    $timer.Stop()
    if ($controller.ExitCode -ne 0) {
        throw "Controller failed safe shutdown with exit code $($controller.ExitCode)."
    }
    return [math]::Round($timer.Elapsed.TotalMilliseconds, 2)
}

function Start-Guard([string]$ResolvedExecutablePath) {
    $process = Start-Process -FilePath $ResolvedExecutablePath -ArgumentList $guardArguments -PassThru -WindowStyle Hidden
    Start-Sleep -Seconds $WarmupSeconds
    if ($process.HasExited) {
        throw "browser_guard exited before the guarded phase began."
    }
    return $process
}

function Get-Mean([double[]]$Values) {
    if ($Values.Count -eq 0) { return 0 }
    return [math]::Round((($Values | Measure-Object -Average).Average), 3)
}

function Get-StandardDeviation([double[]]$Values) {
    if ($Values.Count -lt 2) { return 0 }
    $mean = ($Values | Measure-Object -Average).Average
    $sumSquares = ($Values | ForEach-Object { [math]::Pow($_ - $mean, 2) } | Measure-Object -Sum).Sum
    return [math]::Round([math]::Sqrt($sumSquares / ($Values.Count - 1)), 3)
}

function Get-PhaseTrialSummary([object[]]$Samples, [int]$Trial, [string]$Phase) {
    $phaseSamples = @($Samples | Where-Object { $_.Trial -eq $Trial -and $_.Phase -eq $Phase })
    $first = $phaseSamples[0]
    $last = $phaseSamples[-1]

    [pscustomobject]@{
        Trial = $Trial
        Phase = $Phase
        AvgWorkingSetMB = Get-Mean ([double[]]$phaseSamples.WorkingSetMB)
        AvgPrivateMB = Get-Mean ([double[]]$phaseSamples.PrivateMB)
        CpuSecondsDelta = [math]::Round($last.CpuSeconds - $first.CpuSeconds, 4)
        PageFaultDelta = [math]::Round($last.PageFaults - $first.PageFaults, 0)
        AvgProcessCount = Get-Mean ([double[]]$phaseSamples.ProcessCount)
    }
}

function Format-MeanSd([double[]]$Values) {
    return "$(Get-Mean $Values) ± $(Get-StandardDeviation $Values)"
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$exeFullPath = Resolve-RepositoryPath $ExecutablePath $repoRoot
$controllerFullPath = Resolve-RepositoryPath $ControllerPath $repoRoot
$csvFullPath = Resolve-RepositoryPath $CsvPath $repoRoot
$mdFullPath = Resolve-RepositoryPath $MarkdownPath $repoRoot

foreach ($requiredPath in @($exeFullPath, $controllerFullPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required executable not found at $requiredPath"
    }
}

New-Item -ItemType Directory -Force -Path ([System.IO.Path]::GetDirectoryName($csvFullPath)) | Out-Null
New-Item -ItemType Directory -Force -Path ([System.IO.Path]::GetDirectoryName($mdFullPath)) | Out-Null

Write-Warning "Benchmark only minimized, idle browser windows. Save important browser work before continuing."
$allSamples = @()
$resumeLatencies = @()
$guardProcess = $null

try {
    Stop-GuardSafely $controllerFullPath | Out-Null

    for ($trial = 1; $trial -le $TrialCount; $trial++) {
        $order = if (($trial % 2) -eq 1) { "baseline-guarded" } else { "guarded-baseline" }
        $phases = if (($trial % 2) -eq 1) { @("baseline", "guarded") } else { @("guarded", "baseline") }

        foreach ($phase in $phases) {
            if ($phase -eq "guarded") {
                $guardProcess = Start-Guard $exeFullPath
            } else {
                Stop-GuardSafely $controllerFullPath | Out-Null
                Start-Sleep -Seconds $WarmupSeconds
            }

            Write-Host "Trial $trial/$TrialCount ($order): collecting $phase samples..."
            $allSamples += Collect-Samples -Trial $trial -Order $order -Phase $phase

            if ($phase -eq "guarded") {
                $resumeLatencies += Stop-GuardSafely $controllerFullPath
                $guardProcess = $null
            }
        }
    }
} finally {
    Stop-GuardSafely $controllerFullPath | Out-Null
}

$allSamples | Export-Csv -Path $csvFullPath -NoTypeInformation -Encoding UTF8
$trialSummaries = @()
for ($trial = 1; $trial -le $TrialCount; $trial++) {
    $trialSummaries += Get-PhaseTrialSummary $allSamples $trial "baseline"
    $trialSummaries += Get-PhaseTrialSummary $allSamples $trial "guarded"
}

$baseline = @($trialSummaries | Where-Object Phase -eq "baseline")
$guarded = @($trialSummaries | Where-Object Phase -eq "guarded")
$sortedLatency = @($resumeLatencies | Sort-Object)
$p95Index = [Math]::Max([Math]::Ceiling(0.95 * $sortedLatency.Count) - 1, 0)

$report = @"
# Memory Benchmark

Generated: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss zzz")

This report uses alternating AB/BA trials. Each trial is the statistical unit; raw samples are saved in `memory-benchmark.csv`.

## Method

- Trials: $TrialCount
- Phase duration: $PhaseSeconds seconds
- Sample interval: $IntervalMilliseconds ms
- Order: odd trials baseline→guarded; even trials guarded→baseline
- Guard mode: `browser_guard.exe $($guardArguments -join ' ')`
- Safety: minimized-only suspension and graceful controller shutdown
- Browsers: Chrome, Edge, Firefox, Brave, Opera, and Vivaldi

## Results across trials (mean ± sample SD)

| Metric | Baseline | Guarded |
| --- | ---: | ---: |
| Working set (MB) | $(Format-MeanSd ([double[]]$baseline.AvgWorkingSetMB)) | $(Format-MeanSd ([double[]]$guarded.AvgWorkingSetMB)) |
| Private bytes (MB) | $(Format-MeanSd ([double[]]$baseline.AvgPrivateMB)) | $(Format-MeanSd ([double[]]$guarded.AvgPrivateMB)) |
| CPU time per phase (s) | $(Format-MeanSd ([double[]]$baseline.CpuSecondsDelta)) | $(Format-MeanSd ([double[]]$guarded.CpuSecondsDelta)) |
| Page faults per phase | $(Format-MeanSd ([double[]]$baseline.PageFaultDelta)) | $(Format-MeanSd ([double[]]$guarded.PageFaultDelta)) |
| Process count | $(Format-MeanSd ([double[]]$baseline.AvgProcessCount)) | $(Format-MeanSd ([double[]]$guarded.AvgProcessCount)) |

## Recovery cost

- Mean graceful shutdown/resume time: $(Get-Mean ([double[]]$resumeLatencies)) ms
- P95 graceful shutdown/resume time: $($sortedLatency[$p95Index]) ms

## Interpretation limits

- Working-set reduction means fewer resident pages, not equivalent private-memory deallocation.
- Page faults and resume time expose part of the cost paid for reclaiming resident memory.
- Results apply only to this machine, browser workload, power state, and trial procedure.
- This script does not prove protection for downloads, meetings, muted video, unsaved forms, or other browser-internal work.
"@

Set-Content -Path $mdFullPath -Value $report -Encoding UTF8
Write-Host "Saved raw samples to $csvFullPath"
Write-Host "Saved report to $mdFullPath"
