param(
    [Parameter(Mandatory=$true)][ValidateSet('Install','Update','Remove')][string]$Mode,
    [Parameter(Mandatory=$true)][string]$GameExe
)
$ErrorActionPreference='Stop'
$mgsTargetExe=(Resolve-Path -LiteralPath $GameExe).Path
$mgsTarget=Split-Path -Parent $mgsTargetExe
$mgsTpp=[IO.Path]::GetFileName($mgsTargetExe) -ieq 'mgsvtpp.exe'
if (-not $mgsTpp -and [IO.Path]::GetFileName($mgsTargetExe) -ine 'MgsGroundZeroes.exe') { throw 'Choose mgsvtpp.exe or MgsGroundZeroes.exe.' }
$mgsExpected=if ($mgsTpp) {'085c2f82d1c963c40b3d2d55786661dfee2b18cbbf388a710c00fa76c5e9bb45'} else {'7460d9dba9b6fe34893b5d330aca201983bb1734844f3688acffcc0ab22a1815'}
if ((Get-FileHash -LiteralPath $mgsTargetExe -Algorithm SHA256).Hash -ine $mgsExpected) {
    throw 'Unsupported game executable. This package requires TPP 1.0.15.4 or GZ 1.0.0.5.'
}
$mgsProcessName=if ($mgsTpp) {'mgsvtpp'} else {'MgsGroundZeroes'}
$mgsGameName=if ($mgsTpp) {'The Phantom Pain'} else {'Ground Zeroes'}
if (Get-Process -Name $mgsProcessName -ErrorAction SilentlyContinue | Where-Object { -not $_.WaitForExit(0) }) {
    throw "Close $mgsGameName before changing its mod files."
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
$mgsNames+=@('assets\controls\left.mesh','assets\controls\right.mesh','assets\controls\left.png','assets\controls\right.png',
    'assets\controls\LICENSE.txt','assets\controls\provenance.json') | Where-Object { $mgsRecord.files.PSObject.Properties.Name -contains $_ }
foreach ($mgsName in $mgsNames) {
    $mgsPath=Join-Path $mgsTarget $mgsName
    if (-not [IO.Path]::GetFullPath($mgsPath).StartsWith($mgsTarget+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe mod file path.' }
    for ($mgsAncestor=$mgsPath; $mgsAncestor; $mgsAncestor=Split-Path -Parent $mgsAncestor) {
        # Resolve once, with the race between Test-Path and Get-Item treated as
        # a missing ancestor. This also works under CTest's mocked PowerShell
        # commands and keeps the linked-path guard fail-closed.
        $mgsAncestorItem=Get-Item -LiteralPath $mgsAncestor -ErrorAction SilentlyContinue
        if ($mgsAncestorItem -and ($mgsAncestorItem.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'Maintenance does not follow linked paths.'
        }
    }
    if (-not (Test-Path -LiteralPath $mgsPath -PathType Leaf)) { throw "Incomplete installation: $mgsName is missing. No files changed." }
    if ($mgsName -ne 'mgs5vr-install.json' -and $mgsRecord.files.PSObject.Properties.Name -notcontains $mgsName) {
        throw "Unrecorded $mgsName was preserved. No files changed."
    }
    if (($mgsName -in @('dinput8.dll','mgs5vr_controls.exe') -or $mgsName.StartsWith('assets\controls\')) -and
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
$mgsAdded=@()
try {
    foreach ($mgsName in $mgsNames) {
        [IO.Directory]::CreateDirectory((Split-Path -Parent (Join-Path $mgsBackup $mgsName))) | Out-Null
        Move-Item -LiteralPath (Join-Path $mgsTarget $mgsName) -Destination (Join-Path $mgsBackup $mgsName)
        $mgsMoved+=$mgsName
    }
    if ($Mode -eq 'Update') {
        & (Join-Path $PSScriptRoot 'install.ps1') -GameDir $mgsTarget -EnableVR
        foreach ($mgsName in $mgsNames) {
            $mgsFreshHashes[$mgsName]=(Get-FileHash -LiteralPath (Join-Path $mgsTarget $mgsName) -Algorithm SHA256).Hash
        }
        $mgsNewRecord=Get-Content -Raw -LiteralPath $mgsRecordPath | ConvertFrom-Json
        $mgsAdded=@('assets\controls\left.mesh','assets\controls\right.mesh','assets\controls\left.png','assets\controls\right.png',
            'assets\controls\LICENSE.txt','assets\controls\provenance.json') | Where-Object {
                $mgsNames -notcontains $_ -and $mgsNewRecord.files.PSObject.Properties.Name -contains $_ }
        foreach ($mgsName in $mgsAdded) { $mgsFreshHashes[$mgsName]=(Get-FileHash -LiteralPath (Join-Path $mgsTarget $mgsName) -Algorithm SHA256).Hash }
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
    foreach ($mgsName in $mgsAdded) {
        $mgsPath=Join-Path $mgsTarget $mgsName
        if (Test-Path -LiteralPath $mgsPath) {
            if ((Get-FileHash -LiteralPath $mgsPath -Algorithm SHA256).Hash -ine $mgsFreshHashes[$mgsName]) { throw "A new controller asset changed during recovery; preserved: $mgsPath. Original error: $mgsFailure" }
            Remove-Item -LiteralPath $mgsPath
        }
    }
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
