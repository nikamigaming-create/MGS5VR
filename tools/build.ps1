param([switch]$SkipBootstrap,[switch]$NoDeploy)
$ErrorActionPreference = 'Stop'
$mgsRoot = Split-Path -Parent $PSScriptRoot
if (-not $SkipBootstrap) { & (Join-Path $PSScriptRoot 'bootstrap.ps1') }
Push-Location $mgsRoot
try {
    $mgsBuildArgs = @((Join-Path $PSScriptRoot 'workspace.py'),'build')
    if ($NoDeploy) { $mgsBuildArgs += '--no-deploy' }
    & python @mgsBuildArgs
    if ($LASTEXITCODE -ne 0) { throw 'Build, verification or local synchronization failed; inspect the reported step.' }
} finally { Pop-Location }
