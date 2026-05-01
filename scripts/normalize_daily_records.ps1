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

function Get-RecordSortKey {
    param($Record)

    $timestamp = [string]$Record.Row.TimestampKST
    if ($timestamp -match '^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$') {
        return $timestamp
    }

    return "9999-12-31 23:59:59"
}

function Select-DailyRecord {
    param([object[]]$Records)

    $okRecords = @($Records | Where-Object {
        $_.Row.JobKoreaStatus -eq "ok" -and
        -not [string]::IsNullOrWhiteSpace([string]$_.Row.JobKoreaCount)
    })

    $candidates = $Records
    if ($okRecords.Count -gt 0) {
        $candidates = $okRecords
    }

    return @($candidates | Sort-Object `
        @{ Expression = { Get-RecordSortKey $_ } }, `
        @{ Expression = { $_.Index } })[0].Row
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
$rowIndex = 0

foreach ($row in $rows) {
    $date = [string]$row.DateKST
    if ([string]::IsNullOrWhiteSpace($date)) {
        $date = "__row_$rowIndex"
    }

    if (-not $byDate.Contains($date)) {
        $byDate[$date] = @()
    }

    $byDate[$date] += [pscustomobject]@{
        Row = $row
        Index = $rowIndex
    }
    ++$rowIndex
}

$removed = $rows.Count - $byDate.Count

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add(($headers | ForEach-Object { ConvertTo-CsvField $_ }) -join ",")

foreach ($records in $byDate.Values) {
    $row = Select-DailyRecord -Records @($records)
    $fields = foreach ($header in $headers) {
        ConvertTo-CsvField ([string]$row.$header)
    }
    $lines.Add($fields -join ",")
}

$encoding = New-Object System.Text.UTF8Encoding($true)
[System.IO.File]::WriteAllText((Resolve-Path -LiteralPath $CsvPath), ($lines -join "`r`n") + "`r`n", $encoding)

Write-Host "Normalized daily records. Removed duplicate rows: $removed"
