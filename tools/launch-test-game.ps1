param(
    [Parameter(Mandatory=$true)][string]$GameDir,
    [Parameter(Mandatory=$true)][string]$RuntimeManifest,
    [Parameter(Mandatory=$true)][string]$OperatorDir
)
# Direct child environment, as in the established headset launcher. Steam
# stays running. No desktop/focus/window manipulation or global XR changes.
$ErrorActionPreference='Stop'
Import-Module Microsoft.PowerShell.Utility
Import-Module Microsoft.PowerShell.Management
Import-Module CimCmdlets
$mgsRoot=Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $PSScriptRoot 'simulator-session-ownership.psm1') -Force
$mgsSteam=Get-Process steam -ErrorAction Stop
if(@($mgsSteam).Count -ne 1){throw 'One signed-in Steam client must already be running.'}
if(Get-Process mgsvtpp -ErrorAction SilentlyContinue){throw 'MGSV already runs; reuse its session.'}
$mgsTarget=(Resolve-Path -LiteralPath $GameDir).Path
$mgsExe=Join-Path $mgsTarget 'mgsvtpp.exe'
$mgsSha=[Security.Cryptography.SHA256]::Create()
$mgsStream=[IO.File]::OpenRead($mgsExe)
try{$mgsExeHash=[BitConverter]::ToString($mgsSha.ComputeHash($mgsStream)).Replace('-','').ToLowerInvariant()}
finally{$mgsStream.Dispose();$mgsSha.Dispose()}
if($mgsExeHash -ne '085c2f82d1c963c40b3d2d55786661dfee2b18cbbf388a710c00fa76c5e9bb45'){throw 'Unsupported game executable.'}
$mgsManifest=(Resolve-Path -LiteralPath $RuntimeManifest).Path
$mgsLayer=(Resolve-Path -LiteralPath $OperatorDir).Path
if(!(Test-Path -LiteralPath (Join-Path $mgsLayer 'XrApiLayer_METAX_operator.json'))){throw 'Operator manifest missing.'}
$mgsRuntimeRoot=Split-Path -Parent $mgsManifest
$mgsExisting=@(Get-CimInstance Win32_Process | Where-Object { $_.ExecutablePath -and (Test-MgsPathUnderRoot $_.ExecutablePath $mgsRuntimeRoot) })
if($mgsExisting.Count){throw 'A simulator is already alive; inspect its ownership before launching another.'}
$mgsNames=@('XR_RUNTIME_JSON','XR_API_LAYER_PATH','XR_ENABLE_API_LAYERS','OPENXR_SIMULATOR_HEADLESS','SteamAppId','SteamGameId')
$mgsPrevious=@{}
foreach($mgsName in $mgsNames){$mgsPrevious[$mgsName]=[Environment]::GetEnvironmentVariable($mgsName,'Process')}
try{
    $env:XR_RUNTIME_JSON=$mgsManifest
    $env:XR_API_LAYER_PATH=$mgsLayer
    $env:XR_ENABLE_API_LAYERS='XR_APILAYER_METAX_operator'
    # Use the normal compositor for final-eye validation and the simulator view.
    $env:OPENXR_SIMULATOR_HEADLESS=$null
    $env:SteamAppId='287700';$env:SteamGameId='287700'
    # MGSV is the interactive game, not a background helper. Starting it hidden
    # prevents its first D3D image and therefore OpenXR initialization. Let the
    # game create its normal render window; never focus or manipulate it.
    $mgsGame=Start-Process -FilePath $mgsExe -WorkingDirectory $mgsTarget -PassThru
}finally{
    foreach($mgsName in $mgsNames){[Environment]::SetEnvironmentVariable($mgsName,$mgsPrevious[$mgsName],'Process')}
}
$mgsRow=Get-CimInstance Win32_Process -Filter ("ProcessId="+$mgsGame.Id)
$mgsGameIdentity=ConvertTo-MgsProcessIdentity $mgsRow
$mgsRecord=@{schema=2;launchMethod='direct';pid=$mgsSteam.Id;started_utc=$mgsSteam.StartTime.ToUniversalTime().ToString('o');runtime=$mgsManifest;operator=$mgsLayer;game=$mgsGameIdentity;ownedRuntimeProcesses=@()}
$mgsRecordPath=Join-Path $mgsRoot 'artifacts\simulator-steam-session.json'
[IO.Directory]::CreateDirectory((Split-Path -Parent $mgsRecordPath)) | Out-Null
function Write-MgsTestRecord { [IO.File]::WriteAllText($mgsRecordPath,($mgsRecord|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false)) }
Write-MgsTestRecord
$mgsDeadline=[DateTime]::UtcNow.AddSeconds(40)
do{
    if($mgsGame.HasExited){throw 'MGSV exited before its simulator initialized; inspect mgs5vr.log.'}
    # A short-lived Steam helper may exit during enumeration and lose its
    # readable path. Only complete identities can be recorded as owned.
    $mgsSnapshot=@(Get-CimInstance Win32_Process | Where-Object { $_.ExecutablePath -and $_.CreationDate })
    $mgsRecord.ownedRuntimeProcesses=@(Get-MgsOwnedRuntimeProcesses -Processes $mgsSnapshot -Game $mgsGameIdentity -RuntimeRoot $mgsRuntimeRoot)
    Write-MgsTestRecord
    if(@($mgsRecord.ownedRuntimeProcesses | Where-Object role -eq 'meta_gui').Count -and
       @($mgsRecord.ownedRuntimeProcesses | Where-Object role -eq 'runtime_child').Count){
        Write-Output ('MGSV test process '+$mgsGame.Id+' launched; Steam '+$mgsSteam.Id+' retained. Verify a fresh compositor capture.');return
    }
    Start-Sleep -Milliseconds 250
}while([DateTime]::UtcNow -lt $mgsDeadline)
throw 'Simulator initialization timed out; owned processes remain recorded for cleanup.'
