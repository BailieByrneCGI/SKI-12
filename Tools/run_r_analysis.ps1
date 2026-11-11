param(
    [string]$RscriptPath = '',
    [switch]$FitModel
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$cli = Join-Path $root 'build/rag-cli.exe'
if (-not (Test-Path -LiteralPath $cli)) { throw 'Build the C++ harness first: cmake --build build' }
$output = Join-Path $root 'Output'
$analytics = Join-Path $output 'Analytics'
& $cli export-analytics --output $output --analytics-dir $analytics
if ($LASTEXITCODE -ne 0) { throw 'Analytics export failed' }
if (-not $RscriptPath) {
    $found = Get-Command Rscript -ErrorAction SilentlyContinue
    if ($found) { $RscriptPath = $found.Source }
}
if (-not $RscriptPath) {
    $locations = @((Join-Path $env:LOCALAPPDATA 'Programs\R'), 'C:\Program Files\R')
    foreach ($location in $locations) {
        $installations = Get-ChildItem -Path $location -Filter 'R-*' -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending
        foreach ($installation in $installations) {
            foreach ($candidate in @('bin/Rscript.exe', 'bin/x64/Rscript.exe')) {
                $path = Join-Path $installation.FullName $candidate
                if (Test-Path -LiteralPath $path) { $RscriptPath = $path; break }
            }
            if ($RscriptPath) { break }
        }
        if ($RscriptPath) { break }
    }
}
if (-not $RscriptPath) {
    throw 'Rscript was not found. Install R from https://cran.r-project.org/bin/windows/base/ or pass -RscriptPath. The text-free findings.csv export is ready.'
}
$rArgs = @((Join-Path $PSScriptRoot 'R/analyse.R'), '--findings', (Join-Path $analytics 'findings.csv'),
           '--output', $analytics, '--decisions', (Join-Path $PSScriptRoot 'R/reviewer_decisions.csv'))
if ($FitModel) { $rArgs += '--fit-model' }
$previousLocale = @{}
foreach ($name in @('LC_ALL', 'LC_CTYPE', 'LANG')) {
    $previousLocale[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
    [Environment]::SetEnvironmentVariable($name, $null, 'Process')
}
try {
    & $RscriptPath @rArgs
    $rExitCode = $LASTEXITCODE
} finally {
    foreach ($name in $previousLocale.Keys) {
        [Environment]::SetEnvironmentVariable($name, $previousLocale[$name], 'Process')
    }
}
if ($rExitCode -ne 0) { throw "R analysis failed with exit code $rExitCode" }
