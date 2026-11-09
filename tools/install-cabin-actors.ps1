param(
    [Parameter(Mandatory=$true)][string]$GameDir,
    [Parameter(Mandatory=$true)][string]$Importer,
    [switch]$CheckOnly,
    [switch]$Remove
)
$ErrorActionPreference='Stop'
Import-Module Microsoft.PowerShell.Utility -Function ConvertTo-Json,ConvertFrom-Json
Import-Module Microsoft.PowerShell.Management
function Get-CabinHash {
    param([string]$LiteralPath,[string]$Algorithm)
    $cabinHashInput=[IO.File]::OpenRead($LiteralPath)
    $cabinHashAlgorithm=[Security.Cryptography.SHA256]::Create()
    try{return [pscustomobject]@{Hash=[BitConverter]::ToString($cabinHashAlgorithm.ComputeHash($cabinHashInput)).Replace('-','')}}
    finally{$cabinHashAlgorithm.Dispose();$cabinHashInput.Dispose()}
}
$cabinGame=(Resolve-Path -LiteralPath $GameDir).Path
$cabinImporter=(Resolve-Path -LiteralPath $Importer).Path
$cabinArchive=Join-Path $cabinGame 'master\0\00.dat'
$cabinReceipt=Join-Path $cabinGame 'mgs5vr-cabin-assets.json'
$cabinTemporary=$cabinArchive+'.mgs5vr-cabin-stage'
$cabinRollback=$cabinArchive+'.mgs5vr-cabin-rollback'
$cabinReceiptStage=$cabinReceipt+'.mgs5vr-stage'
foreach($cabinPath in @($cabinArchive,$cabinReceipt,$cabinTemporary,$cabinRollback,$cabinReceiptStage)) {
    for($cabinPart=$cabinPath;$cabinPart;$cabinPart=Split-Path -Parent $cabinPart) {
        if((Test-Path -LiteralPath $cabinPart) -and ((Get-Item -LiteralPath $cabinPart).Attributes -band [IO.FileAttributes]::ReparsePoint)) {throw 'Cabin installer does not follow linked paths.'}
        if($cabinPart -eq $cabinGame){break}
    }
}
if((Test-Path -LiteralPath $cabinTemporary) -or (Test-Path -LiteralPath $cabinRollback) -or (Test-Path -LiteralPath $cabinReceiptStage)) {throw 'An unfinished native cabin archive transaction is preserved; recover it before updating.'}
$cabinOldReceipt=if(Test-Path -LiteralPath $cabinReceipt){[IO.File]::ReadAllBytes($cabinReceipt)}else{$null}
$cabinBefore=(Get-CabinHash -LiteralPath $cabinArchive -Algorithm SHA256).Hash
$cabinImporterHash=(Get-CabinHash -LiteralPath $cabinImporter -Algorithm SHA256).Hash
if(-not $Remove -and (Test-Path -LiteralPath $cabinReceipt)) {
    $cabinExisting=Get-Content -Raw -LiteralPath $cabinReceipt|ConvertFrom-Json
    if($cabinExisting.schema -eq 1 -and $cabinExisting.module -eq 'native-cabin-actors-v1' -and $cabinExisting.archive_sha256 -eq $cabinBefore -and $cabinExisting.importer_sha256 -eq $cabinImporterHash) {
        Write-Output 'Native cabin actor datasets already match the checked importer.'
        return
    }
}
if($CheckOnly){throw 'Native cabin actor assets need an update; close MGSV before applying the checked candidate.'}
if(Get-Process -Name mgsvtpp -ErrorAction SilentlyContinue|Where-Object {-not $_.WaitForExit(0)}){throw 'Close MGSV before updating native cabin actor assets.'}
$cabinStage=Join-Path ([IO.Path]::GetTempPath()) ('mgs5vr-cabin-'+[Guid]::NewGuid().ToString('N'))
$cabinPrepared=Join-Path $cabinStage '00.dat'
$cabinReplaced=$false
$cabinRecovered=$false
$cabinOwnTemporary=$false
$cabinOwnReceiptStage=$false
$cabinReceiptChanged=$false
try {
    [IO.Directory]::CreateDirectory($cabinStage)|Out-Null
    $cabinOperation=if($Remove){'--remove-cabin'}else{'--prepare-cabin'}
    & $cabinImporter $cabinOperation $cabinGame $cabinPrepared
    if($LASTEXITCODE -ne 0){throw 'Native cabin package preparation failed; installed archive preserved.'}
    if((Get-CabinHash -LiteralPath $cabinArchive -Algorithm SHA256).Hash -ne $cabinBefore){throw 'Installed archive changed during preparation; preserved.'}
    $cabinAfter=(Get-CabinHash -LiteralPath $cabinPrepared -Algorithm SHA256).Hash
    [IO.File]::Copy($cabinPrepared,$cabinTemporary,$false)
    $cabinOwnTemporary=$true
    if((Get-CabinHash -LiteralPath $cabinTemporary -Algorithm SHA256).Hash -ne $cabinAfter){throw 'Native cabin stage copy differs.'}
    if((Get-CabinHash -LiteralPath $cabinArchive -Algorithm SHA256).Hash -ne $cabinBefore){throw 'Installed archive changed during stage copy; preserved.'}
    [IO.File]::Replace($cabinTemporary,$cabinArchive,$cabinRollback)
    $cabinOwnTemporary=$false
    $cabinReplaced=$true
    if((Get-CabinHash -LiteralPath $cabinRollback -Algorithm SHA256).Hash -ne $cabinBefore){throw 'Installed archive changed at atomic replacement; restoring the concurrent original.'}
    if((Get-CabinHash -LiteralPath $cabinArchive -Algorithm SHA256).Hash -ne $cabinAfter){throw 'Installed native cabin archive verification failed.'}
    if($Remove){if(Test-Path -LiteralPath $cabinReceipt){Remove-Item -LiteralPath $cabinReceipt;$cabinReceiptChanged=$true}}
    else {
        $cabinRecord=@{schema=1;module='native-cabin-actors-v1';archive_sha256=$cabinAfter;importer_sha256=$cabinImporterHash;scenes=@(40010,40020,40050);updated_utc=[DateTime]::UtcNow.ToString('o')}
        $cabinRecordBytes=[Text.UTF8Encoding]::new($false).GetBytes(($cabinRecord|ConvertTo-Json))
        $cabinRecordFile=[IO.File]::Open($cabinReceiptStage,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
        $cabinOwnReceiptStage=$true
        try{$cabinRecordFile.Write($cabinRecordBytes,0,$cabinRecordBytes.Length);$cabinRecordFile.Flush($true)}finally{$cabinRecordFile.Dispose()}
        if(Test-Path -LiteralPath $cabinReceipt){[IO.File]::Replace($cabinReceiptStage,$cabinReceipt,[NullString]::Value)}
        else{[IO.File]::Move($cabinReceiptStage,$cabinReceipt)}
        $cabinOwnReceiptStage=$false
        $cabinReceiptChanged=$true
    }
    $cabinRecovered=$true
    if($Remove){Write-Output 'Removed native cabin actor additions; unrelated native assets and saves preserved.'}
    else {Write-Output 'Installed native cabin actors for Afghanistan, Africa and Mother Base; saves and other archive entries preserved.'}
} catch {
    if($cabinReplaced) {
        $cabinRestoreHash=(Get-CabinHash -LiteralPath $cabinRollback -Algorithm SHA256).Hash
        [IO.File]::Replace($cabinRollback,$cabinArchive,[NullString]::Value)
        if((Get-CabinHash -LiteralPath $cabinArchive -Algorithm SHA256).Hash -ne $cabinRestoreHash){throw 'Native archive recovery verification failed; preserve transaction files.'}
    }
    if($cabinReceiptChanged){if($null -ne $cabinOldReceipt){[IO.File]::WriteAllBytes($cabinReceipt,$cabinOldReceipt)}elseif(Test-Path -LiteralPath $cabinReceipt){Remove-Item -LiteralPath $cabinReceipt}}
    $cabinRecovered=$true
    throw
} finally {
    if($cabinRecovered -or -not $cabinReplaced) {
        $cabinCleanup=@($cabinPrepared)
        if($cabinOwnTemporary){$cabinCleanup+=$cabinTemporary}
        if($cabinOwnReceiptStage){$cabinCleanup+=$cabinReceiptStage}
        if($cabinReplaced){$cabinCleanup+=$cabinRollback}
        foreach($cabinPath in $cabinCleanup) {if(Test-Path -LiteralPath $cabinPath){Remove-Item -LiteralPath $cabinPath}}
        if(Test-Path -LiteralPath $cabinStage){Remove-Item -LiteralPath $cabinStage}
    }
}
