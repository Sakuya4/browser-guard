param(
    [Parameter(Mandatory = $true)][string]$OwnerPath,
    [Parameter(Mandatory = $true)][string]$BrokerPath,
    [Parameter(Mandatory = $true)][string]$FixturePath
)

$ErrorActionPreference = "Stop"
$testDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("browser-guard-recovery-" + [Guid]::NewGuid())
$journalPath = Join-Path $testDirectory "recovery.journal"
$markerPath = Join-Path $testDirectory "resumed.marker"
New-Item -ItemType Directory -Path $testDirectory | Out-Null

try {
    $arguments = @(
        ('"' + $BrokerPath + '"'),
        ('"' + $FixturePath + '"'),
        ('"' + $journalPath + '"'),
        ('"' + $markerPath + '"')
    )
    $owner = Start-Process -FilePath $OwnerPath -ArgumentList $arguments -PassThru -Wait -WindowStyle Hidden
    if ($owner.ExitCode -ne 0) {
        throw "Recovery owner fixture failed with exit code $($owner.ExitCode)."
    }

    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    while (-not (Test-Path -LiteralPath $markerPath) -and [DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 50
    }

    if (-not (Test-Path -LiteralPath $markerPath)) {
        throw "Recovery broker did not resume the suspended fixture after owner exit."
    }
    if (Test-Path -LiteralPath $journalPath) {
        throw "Recovery broker did not remove the resolved journal."
    }
} finally {
    Remove-Item -LiteralPath $testDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
