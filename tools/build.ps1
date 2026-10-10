param([switch]$SkipBootstrap,[switch]$NoDeploy,[switch]$Deploy)
$ErrorActionPreference = 'Stop'
if ($NoDeploy -and $Deploy) { throw 'Choose either -Deploy or -NoDeploy.' }
# Environment names are case-insensitive on Windows. Normalize PATH before
# Python launches MSBuild: inherited PATH plus a toolchain's Path otherwise
# produces two entries and MSB6001 before compilation starts.
$mgsBuildPath=$env:PATH
Remove-Item Env:PATH -ErrorAction SilentlyContinue
$env:Path=$mgsBuildPath
$mgsRoot = Split-Path -Parent $PSScriptRoot
if (-not $SkipBootstrap) { & (Join-Path $PSScriptRoot 'bootstrap.ps1') }
Push-Location $mgsRoot
try {
    $mgsBuildArgs = @((Join-Path $PSScriptRoot 'workspace.py'),'build')
    if ($NoDeploy) { $mgsBuildArgs += '--no-deploy' }
    if ($Deploy) { $mgsBuildArgs += '--deploy' }
    & python @mgsBuildArgs
    if ($LASTEXITCODE -ne 0) { throw 'Build, verification or local synchronization failed; inspect the reported step.' }
} finally { Pop-Location }
