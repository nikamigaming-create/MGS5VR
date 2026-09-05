param([Parameter(Mandatory=$true)][string]$GameDir)
$ErrorActionPreference = 'Stop'
$mgsTarget = (Resolve-Path -LiteralPath $GameDir).Path
if (Get-Process -Name mgsvtpp -ErrorAction SilentlyContinue | Where-Object { -not $_.WaitForExit(0) }) { throw 'Close The Phantom Pain before removing the mod.' }
$mgsRecordPath = Join-Path $mgsTarget 'mgs5vr-install.json'
$mgsRecord = Get-Content -Raw -LiteralPath $mgsRecordPath | ConvertFrom-Json
if ($mgsRecord.schema -ne 1 -or $mgsRecord.product -ne 'MGS5VR theatre preview' -or $mgsRecord.game_dir -ne $mgsTarget) { throw 'Installation record does not match this directory.' }
foreach ($mgsName in @('dinput8.dll','mgs5vr.ini')) {
    $mgsPath = Join-Path $mgsTarget $mgsName
    if (Test-Path -LiteralPath $mgsPath) {
        if ((Get-FileHash -LiteralPath $mgsPath -Algorithm SHA256).Hash -ne $mgsRecord.files.$mgsName) {
            throw "Modified $mgsName was preserved. Restore its recorded installed version or remove it manually after reviewing it."
        }
    }
}
foreach ($mgsName in @('dinput8.dll','mgs5vr.ini')) {
    $mgsPath = Join-Path $mgsTarget $mgsName
    if (Test-Path -LiteralPath $mgsPath) { Remove-Item -LiteralPath $mgsPath }
}
Remove-Item -LiteralPath $mgsRecordPath
Write-Output 'Removed the recorded MGS5VR files. Diagnostic log retained.'
