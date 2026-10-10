param([switch]$Apply)
$ErrorActionPreference = 'Stop'
$mgsCleanupArgs = @((Join-Path $PSScriptRoot 'workspace.py'), 'clean-builds')
if ($Apply) { $mgsCleanupArgs += '--apply' }
& python @mgsCleanupArgs
if ($LASTEXITCODE -ne 0) { throw 'Build cleanup failed; inspect the receipt before retrying.' }
