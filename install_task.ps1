param(
    [string]$TaskName = "JobPostCounterDaily",
    [string]$Time = "09:00",
    [string]$OutputDir = "",
    [switch]$SkipJobKorea
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$RunScript = Join-Path $Root "run.ps1"

if (-not (Test-Path $RunScript)) {
    throw "run.ps1 was not found."
}

$ArgList = @(
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", "`"$RunScript`""
)
if ($OutputDir) {
    $ArgList += @("-OutputDir", "`"$OutputDir`"")
}
if ($SkipJobKorea) {
    $ArgList += "-SkipJobKorea"
}

$Action = New-ScheduledTaskAction -Execute "powershell.exe" -Argument ($ArgList -join " ") -WorkingDirectory $Root
$Trigger = New-ScheduledTaskTrigger -Daily -At $Time
$Settings = New-ScheduledTaskSettingsSet -StartWhenAvailable -MultipleInstances IgnoreNew -ExecutionTimeLimit (New-TimeSpan -Minutes 10)

Register-ScheduledTask -TaskName $TaskName -Action $Action -Trigger $Trigger -Settings $Settings -Description "Measure JobKorea job-posting counts and update Excel files." -Force | Out-Null

Write-Host "Registered task '$TaskName' at $Time"
