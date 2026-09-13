param(
    [Parameter(Mandatory=$true)][ValidateSet('Install','Update','Remove')][string]$Mode,
    [Parameter(Mandatory=$true)][string]$GameExe
)
$ErrorActionPreference='Stop'
$mgsTargetExe=(Resolve-Path -LiteralPath $GameExe).Path
$mgsTarget=Split-Path -Parent $mgsTargetExe
if ([IO.Path]::GetFileName($mgsTargetExe) -ine 'mgsvtpp.exe') {
    throw 'The launcher installs tracked VR for The Phantom Pain only. Ground Zeroes first-person adapters are still in development.'
}
if ((Get-FileHash -LiteralPath $mgsTargetExe -Algorithm SHA256).Hash -ine '085c2f82d1c963c40b3d2d55786661dfee2b18cbbf388a710c00fa76c5e9bb45') {
    throw 'Unsupported game executable. This package requires TPP 1.0.15.4.'
}
if (Get-Process -Name mgsvtpp -ErrorAction SilentlyContinue | Where-Object { -not $_.WaitForExit(0) }) {
    throw 'Close The Phantom Pain before changing its mod files.'
}
$mgsRecordPath=Join-Path $mgsTarget 'mgs5vr-install.json'
if ($Mode -eq 'Install') {
    & (Join-Path $PSScriptRoot 'install.ps1') -GameDir $mgsTarget -EnableVR
    return
}
$mgsRecord=Get-Content -Raw -LiteralPath $mgsRecordPath | ConvertFrom-Json
if ($mgsRecord.schema -ne 1 -or $mgsRecord.product -ne 'MGS5VR theatre preview' -or $mgsRecord.game_dir -ine $mgsTarget) {
    throw 'The installation record does not belong to this folder. No files changed.'
}
$mgsNames=@('dinput8.dll','mgs5vr.ini','mgs5vr-controls.ini','mgs5vr_controls.exe','mgs5vr-install.json')
foreach ($mgsName in $mgsNames) {
    $mgsPath=Join-Path $mgsTarget $mgsName
    if ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($mgsPath)) -ine $mgsTarget) { throw 'Unsafe mod file path.' }
    for ($mgsAncestor=$mgsPath; $mgsAncestor; $mgsAncestor=Split-Path -Parent $mgsAncestor) {
        if ((Test-Path -LiteralPath $mgsAncestor) -and ((Get-Item -LiteralPath $mgsAncestor).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'Maintenance does not follow linked paths.'
        }
    }
    if (-not (Test-Path -LiteralPath $mgsPath -PathType Leaf)) { throw "Incomplete installation: $mgsName is missing. No files changed." }
    if ($mgsName -ne 'mgs5vr-install.json' -and $mgsRecord.files.PSObject.Properties.Name -notcontains $mgsName) {
        throw "Unrecorded $mgsName was preserved. No files changed."
    }
    if ($mgsName -in @('dinput8.dll','mgs5vr_controls.exe') -and
        (Get-FileHash -LiteralPath $mgsPath -Algorithm SHA256).Hash -ine $mgsRecord.files.$mgsName) {
        throw "Modified $mgsName was preserved. No files changed."
    }
}
$mgsPackage=Split-Path -Parent $PSScriptRoot
if ($Mode -eq 'Update') {
    $mgsChecker=Join-Path $mgsPackage 'mgs5vr_controls.exe'
    if (-not (Test-Path -LiteralPath $mgsChecker)) { $mgsChecker=Join-Path $mgsPackage 'build\Release\mgs5vr_controls.exe' }
    if (-not (Test-Path -LiteralPath $mgsChecker)) { throw 'The package is incomplete: controls checker missing.' }
    & $mgsChecker --check (Join-Path $mgsTarget 'mgs5vr-controls.ini')
    if ($LASTEXITCODE -ne 0) { throw 'The existing controls need correction before updating. No files changed.' }
}
$mgsBackupRoot=Join-Path $mgsTarget 'mgs5vr-launcher-backups'
if ((Test-Path -LiteralPath $mgsBackupRoot) -and ((Get-Item -LiteralPath $mgsBackupRoot).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
    throw 'The backup folder must not be a link.'
}
$mgsBackup=Join-Path $mgsBackupRoot ((Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $mgsBackup | Out-Null
$mgsMoved=@()
$mgsFreshHashes=@{}
try {
    foreach ($mgsName in $mgsNames) {
        Move-Item -LiteralPath (Join-Path $mgsTarget $mgsName) -Destination (Join-Path $mgsBackup $mgsName)
        $mgsMoved+=$mgsName
    }
    if ($Mode -eq 'Update') {
        & (Join-Path $PSScriptRoot 'install.ps1') -GameDir $mgsTarget -EnableVR
        foreach ($mgsName in $mgsNames) {
            $mgsFreshHashes[$mgsName]=(Get-FileHash -LiteralPath (Join-Path $mgsTarget $mgsName) -Algorithm SHA256).Hash
        }
        $mgsNewRecord=Get-Content -Raw -LiteralPath $mgsRecordPath | ConvertFrom-Json
        foreach ($mgsName in @('mgs5vr.ini','mgs5vr-controls.ini')) {
            Copy-Item -LiteralPath (Join-Path $mgsBackup $mgsName) -Destination (Join-Path $mgsTarget $mgsName)
            $mgsFreshHashes[$mgsName]=(Get-FileHash -LiteralPath (Join-Path $mgsTarget $mgsName) -Algorithm SHA256).Hash
            $mgsNewRecord.files.$mgsName=$mgsFreshHashes[$mgsName]
        }
        [IO.File]::WriteAllText($mgsRecordPath,($mgsNewRecord|ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
        $mgsFreshHashes['mgs5vr-install.json']=(Get-FileHash -LiteralPath $mgsRecordPath -Algorithm SHA256).Hash
        Write-Output 'Update complete. Your controls and VR settings were preserved.'
    } else {
        Write-Output 'MGS5VR removed from startup. Imported binocular cache and diagnostic logs retained.'
    }
    Write-Output "Previous mod files and settings are recoverable in: $mgsBackup"
} catch {
    $mgsFailure=$_
    foreach ($mgsName in $mgsMoved) {
        $mgsPath=Join-Path $mgsTarget $mgsName
        if (Test-Path -LiteralPath $mgsPath) {
            if (-not $mgsFreshHashes.ContainsKey($mgsName) -or
                (Get-FileHash -LiteralPath $mgsPath -Algorithm SHA256).Hash -ine $mgsFreshHashes[$mgsName]) {
                throw "Update stopped; a file changed during recovery. Previous files remain at $mgsBackup. Original error: $mgsFailure"
            }
            # Only an exact file this transaction just installed is removed.
            Remove-Item -LiteralPath $mgsPath
        }
        Move-Item -LiteralPath (Join-Path $mgsBackup $mgsName) -Destination $mgsPath
    }
    throw "Previous installation restored. $mgsFailure"
}
