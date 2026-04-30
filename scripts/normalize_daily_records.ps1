param(
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

if (-not (Test-Path -LiteralPath $CsvPath)) {
    Write-Host "No CSV file to normalize: $CsvPath"
    exit 0
}

$rows = @(Import-Csv -LiteralPath $CsvPath)
if ($rows.Count -eq 0) {
    Write-Host "No CSV rows to normalize."
    exit 0
}

$headers = @(
    "TimestampKST",
    "DateKST",
    "JobKoreaCount",
    "JobKoreaStatus",
    "JobKoreaMessage",
    "JobKoreaSource"
)

$byDate = [ordered]@{}
$removed = 0

foreach ($row in $rows) {
    $date = [string]$row.DateKST
    if ([string]::IsNullOrWhiteSpace($date)) {
        $date = "__row_$($byDate.Count)_$removed"
    }

    if (-not $byDate.Contains($date)) {
        $byDate[$date] = $row
        continue
    }

    $existing = $byDate[$date]
    if ($existing.JobKoreaStatus -ne "ok" -and $row.JobKoreaStatus -eq "ok") {
        $byDate[$date] = $row
    }
    ++$removed
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add(($headers | ForEach-Object { ConvertTo-CsvField $_ }) -join ",")

foreach ($row in $byDate.Values) {
    $fields = foreach ($header in $headers) {
        ConvertTo-CsvField ([string]$row.$header)
    }
    $lines.Add($fields -join ",")
}

$encoding = New-Object System.Text.UTF8Encoding($true)
[System.IO.File]::WriteAllText((Resolve-Path -LiteralPath $CsvPath), ($lines -join "`r`n") + "`r`n", $encoding)

Write-Host "Normalized daily records. Removed duplicate rows: $removed"
