param([Parameter(Mandatory=$true)][string]$GameDir)
$ErrorActionPreference = 'Stop'
$mgsTarget = (Resolve-Path -LiteralPath $GameDir).Path
$mgsProcessName = if (Test-Path -LiteralPath (Join-Path $mgsTarget 'MgsGroundZeroes.exe')) { 'MgsGroundZeroes' } else { 'mgsvtpp' }
$mgsGameName = if ($mgsProcessName -eq 'MgsGroundZeroes') { 'Ground Zeroes' } else { 'The Phantom Pain' }
if (Get-Process -Name $mgsProcessName -ErrorAction SilentlyContinue | Where-Object { -not $_.WaitForExit(0) }) { throw "Close $mgsGameName before removing the mod." }
$mgsRecordPath = Join-Path $mgsTarget 'mgs5vr-install.json'
$mgsRecord = Get-Content -Raw -LiteralPath $mgsRecordPath | ConvertFrom-Json
if ($mgsRecord.schema -ne 1 -or $mgsRecord.product -ne 'MGS5VR theatre preview' -or $mgsRecord.game_dir -ne $mgsTarget) { throw 'Installation record does not match this directory.' }
$mgsOwnedNames = @('dinput8.dll','mgs5vr.ini','mgs5vr-controls.ini','mgs5vr_controls.exe',
    'retail-assets\Assets\tpp\item\tel\Scenes\tel0_main0_def.fmdl',
    'retail-assets\Assets\tpp\item\tel\Pictures\tel0_main0_def_c00_bsm.dds',
    'retail-assets\Assets\tpp\item\cct\Scenes\cct0_main1_def.fmdl',
    'retail-assets\Assets\tpp\item\cct\Pictures\cct0_main1_def_c00_bsm.dds',
    'retail-assets\Assets\tpp\item\rdi\Scenes\rdi0_main0_def.fmdl',
    'retail-assets\Assets\tpp\item\rdi\Pictures\rdi0_main0_def_c00_bsm.dds',
    'retail-assets\Assets\tpp\item\idr\Scenes\idr0_main0_def.fmdl',
    'retail-assets\Assets\tpp\item\idr\Pictures\idr0_main0_def_c00_bsm.dds') |
    Where-Object { $mgsRecord.files.PSObject.Properties.Name -contains $_ }
foreach ($mgsName in $mgsOwnedNames) {
    $mgsPath = Join-Path $mgsTarget $mgsName
    for ($mgsAncestor = $mgsPath; $mgsAncestor; $mgsAncestor = Split-Path -Parent $mgsAncestor) {
        if ((Test-Path -LiteralPath $mgsAncestor) -and ((Get-Item -LiteralPath $mgsAncestor).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Uninstall does not follow linked paths.' }
        if ($mgsAncestor -eq $mgsTarget) { break }
    }
    if (Test-Path -LiteralPath $mgsPath) {
        if ((Get-FileHash -LiteralPath $mgsPath -Algorithm SHA256).Hash -ne $mgsRecord.files.$mgsName) {
            throw "Modified $mgsName was preserved. Restore its recorded installed version or remove it manually after reviewing it."
        }
    }
}
foreach ($mgsName in $mgsOwnedNames) {
    $mgsPath = Join-Path $mgsTarget $mgsName
    if (Test-Path -LiteralPath $mgsPath) { Remove-Item -LiteralPath $mgsPath }
}
Remove-Item -LiteralPath $mgsRecordPath
Write-Output 'Removed the recorded MGS5VR files. Diagnostic log retained.'
