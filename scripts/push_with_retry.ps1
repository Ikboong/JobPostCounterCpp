param(
    [ValidateRange(1, 10)]
    [int]$MaxAttempts = 3
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

for ($attempt = 1; $attempt -le $MaxAttempts; $attempt++) {
    git push origin HEAD:main
    $pushExitCode = $LASTEXITCODE

    if ($pushExitCode -eq 0) {
        exit 0
    }

    if ($attempt -eq $MaxAttempts) {
        throw "git push failed after $MaxAttempts attempts (exit code $pushExitCode)."
    }

    Write-Warning "git push attempt $attempt failed. Rebasing onto the latest origin/main before retrying."
    git pull --rebase origin main
    $pullExitCode = $LASTEXITCODE

    if ($pullExitCode -ne 0) {
        throw "git pull --rebase failed with exit code $pullExitCode. Resolve the conflict instead of overwriting origin/main."
    }
}
