param(
    [Parameter(Mandatory = $true)]
    [string]$TimestampKst,

    [string]$DateKst = "",

    [string]$JobKoreaCount = "",

    [ValidateSet("ok", "error")]
    [string]$JobKoreaStatus = "ok",

    [string]$JobKoreaMessage = "",

    [string]$JobKoreaSource = "https://www.jobkorea.co.kr/recruit/joblist?menucode=local&localorder=1",

    [string]$CsvPath = "data/job_post_counts.csv"
)

$ErrorActionPreference = "Stop"

function ConvertTo-CsvField {
    param([AllowNull()][string]$Value)

    if ($null -eq $Value) {
        $Value = ""
    }

    if ($Value -match '[,"\r\n]') {
        return '"' + ($Value -replace '"', '""') + '"'
    }
    return $Value
}

if ($TimestampKst -notmatch '^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$') {
    throw "TimestampKst must use 'yyyy-MM-dd HH:mm:ss': $TimestampKst"
}

if ([string]::IsNullOrWhiteSpace($DateKst)) {
    $DateKst = $TimestampKst.Substring(0, 10)
}

if ($DateKst -notmatch '^\d{4}-\d{2}-\d{2}$') {
    throw "DateKst must use 'yyyy-MM-dd': $DateKst"
}

if ($JobKoreaStatus -eq "ok") {
    if ($JobKoreaCount -notmatch '^[0-9]+$') {
        throw "JobKoreaCount must be numeric when JobKoreaStatus is ok: $JobKoreaCount"
    }
    if ([string]::IsNullOrWhiteSpace($JobKoreaMessage)) {
        $JobKoreaMessage = "parsed joblist hdnGICnt"
    }
} elseif ([string]::IsNullOrWhiteSpace($JobKoreaMessage)) {
    $JobKoreaMessage = "external measurement failed"
}

$headers = @(
    "TimestampKST",
    "DateKST",
    "JobKoreaCount",
    "JobKoreaStatus",
    "JobKoreaMessage",
    "JobKoreaSource"
)

$row = @(
    $TimestampKst,
    $DateKst,
    $JobKoreaCount,
    $JobKoreaStatus,
    $JobKoreaMessage,
    $JobKoreaSource
)

$csvDir = Split-Path -Parent $CsvPath
if ($csvDir -and -not (Test-Path -LiteralPath $csvDir)) {
    New-Item -ItemType Directory -Path $csvDir | Out-Null
}

$targetPath = $CsvPath
if (-not [System.IO.Path]::IsPathRooted($targetPath)) {
    $targetPath = Join-Path (Get-Location) $targetPath
}

$lines = New-Object System.Collections.Generic.List[string]
if (Test-Path -LiteralPath $targetPath) {
    $lines.AddRange([string[]](Get-Content -LiteralPath $targetPath))
} else {
    $lines.Add(($headers | ForEach-Object { ConvertTo-CsvField $_ }) -join ",")
}

$lines.Add(($row | ForEach-Object { ConvertTo-CsvField $_ }) -join ",")

$encoding = New-Object System.Text.UTF8Encoding($true)
[System.IO.File]::WriteAllText($targetPath, ($lines -join "`r`n") + "`r`n", $encoding)

Write-Host "Recorded external JobKorea count: status=$JobKoreaStatus count=$JobKoreaCount timestamp=$TimestampKst"
