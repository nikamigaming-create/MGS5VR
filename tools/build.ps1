param([switch]$SkipBootstrap)
$ErrorActionPreference = 'Stop'
$mgsRoot = Split-Path -Parent $PSScriptRoot
if (-not $SkipBootstrap) { & (Join-Path $PSScriptRoot 'bootstrap.ps1') }
Push-Location $mgsRoot
try {
    & cmake --preset windows-x64
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
    & cmake --build --preset release --parallel
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    & ctest --preset release
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
} finally { Pop-Location }
