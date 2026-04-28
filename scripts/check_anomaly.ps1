param(
    [string]$CsvPath = "data/job_post_counts.csv",
    [double]$MaxDailyChangeRatio = 0.20
)

$ErrorActionPreference = "Stop"

function Write-ActionOutput {
    param(
        [string]$Name,
        [string]$Value
    )

    if ($env:GITHUB_OUTPUT) {
        "$Name=$Value" | Out-File -FilePath $env:GITHUB_OUTPUT -Encoding utf8 -Append
    }
}

function Test-NumericText {
    param([string]$Value)
    return $Value -match '^[0-9]+$'
}

if (-not (Test-Path -LiteralPath $CsvPath)) {
    Write-ActionOutput -Name "is_anomaly" -Value "true"
    Write-ActionOutput -Name "message" -Value "CSV file is missing: $CsvPath"
    Write-Host "Anomaly: CSV file is missing: $CsvPath"
    exit 0
}

$rows = @(Import-Csv -LiteralPath $CsvPath)
if ($rows.Count -eq 0) {
    Write-ActionOutput -Name "is_anomaly" -Value "true"
    Write-ActionOutput -Name "message" -Value "CSV has no data rows."
    Write-Host "Anomaly: CSV has no data rows."
    exit 0
}

$latest = $rows[-1]
$reasons = New-Object System.Collections.Generic.List[string]

if ($latest.JobKoreaStatus -ne "ok") {
    $reasons.Add("measurement failed: status=$($latest.JobKoreaStatus), message=$($latest.JobKoreaMessage)")
}

if ([string]::IsNullOrWhiteSpace($latest.JobKoreaCount)) {
    $reasons.Add("today count is empty")
} elseif (-not (Test-NumericText -Value $latest.JobKoreaCount)) {
    $reasons.Add("today count is not numeric: $($latest.JobKoreaCount)")
}

$latestCount = 0L
$hasLatestCount = [long]::TryParse([string]$latest.JobKoreaCount, [ref]$latestCount)
$previous = $null

for ($i = $rows.Count - 2; $i -ge 0; --$i) {
    $candidate = $rows[$i]
    if ($candidate.DateKST -eq $latest.DateKST) {
        continue
    }
    if ($candidate.JobKoreaStatus -eq "ok" -and (Test-NumericText -Value $candidate.JobKoreaCount)) {
        $previous = $candidate
        break
    }
}

if ($hasLatestCount -and $null -ne $previous) {
    $previousCount = [long]$previous.JobKoreaCount
    if ($previousCount -gt 0) {
        $changeRatio = [math]::Abs($latestCount - $previousCount) / $previousCount
        if ($changeRatio -ge $MaxDailyChangeRatio) {
            $percent = [math]::Round($changeRatio * 100, 2)
            $signedDelta = $latestCount - $previousCount
            $reasons.Add("daily change is $percent% ($signedDelta), previous=$previousCount on $($previous.DateKST), latest=$latestCount on $($latest.DateKST)")
        }
    }
}

if ($reasons.Count -gt 0) {
    $message = $reasons -join "; "
    Write-ActionOutput -Name "is_anomaly" -Value "true"
    Write-ActionOutput -Name "message" -Value $message
    Write-Host "Anomaly: $message"
    exit 0
}

$okMessage = "OK: latest=$($latest.JobKoreaCount) on $($latest.DateKST)"
Write-ActionOutput -Name "is_anomaly" -Value "false"
Write-ActionOutput -Name "message" -Value $okMessage
Write-Host $okMessage
