param(
    [string]$OutputDir = "",
    [switch]$SkipJobKorea
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$ExeCandidates = @(
    (Join-Path $Root "build\Release\JobPostCounter.exe"),
    (Join-Path $Root "build\JobPostCounter.exe")
)

$Exe = $ExeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $Exe) {
    & (Join-Path $Root "build.ps1")
    $Exe = $ExeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Exe) {
    throw "JobPostCounter.exe was not found."
}

$Args = @()
if ($OutputDir) {
    $Args += @("--output-dir", $OutputDir)
}
if ($SkipJobKorea) {
    $Args += "--skip-jobkorea"
}

& $Exe @Args
exit $LASTEXITCODE
