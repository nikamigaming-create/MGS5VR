param(
    [Parameter(Mandatory=$true)][string]$GameDir,
    [Parameter(Mandatory=$true)][string]$RuntimeManifest,
    [Parameter(Mandatory=$true)][string]$OperatorDir
)
# Ask the existing Steam client to launch its owned game. A temporary per-game
# runtime lease avoids Steam's inherited environment. No Steam restart,
# desktop/focus/window manipulation or global XR changes.
$ErrorActionPreference='Stop'
Import-Module Microsoft.PowerShell.Utility
Import-Module Microsoft.PowerShell.Management
Import-Module CimCmdlets
$mgsRoot=Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $PSScriptRoot 'simulator-session-ownership.psm1') -Force
$mgsSteam=Get-Process steam -ErrorAction Stop
if(@($mgsSteam).Count -ne 1){throw 'One signed-in Steam client must already be running.'}
if(Get-Process mgsvtpp -ErrorAction SilentlyContinue | Where-Object { !$_.HasExited }){throw 'MGSV already runs; reuse its session.'}
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
function Test-MgsLiveProcess($Process) {
    # An observer can retain an exited process in CIM. Its generation is no
    # longer a running owner and must not block a fresh local test session.
    $mgsLive=Get-Process -Id $Process.ProcessId -ErrorAction SilentlyContinue
    return $mgsLive -and !$mgsLive.HasExited
}
$mgsExisting=@(Get-CimInstance Win32_Process | Where-Object { $_.ExecutablePath -and (Test-MgsPathUnderRoot $_.ExecutablePath $mgsRuntimeRoot) -and (Test-MgsLiveProcess $_) })
if($mgsExisting.Count){throw 'A simulator is already alive; inspect its ownership before launching another.'}
$mgsRecordPath=Join-Path $mgsRoot 'artifacts\simulator-steam-session.json'
[IO.Directory]::CreateDirectory((Split-Path -Parent $mgsRecordPath)) | Out-Null
# Recover an interrupted, already-dead lease before recording a new backup.
# The game and selected simulator have both been excluded above.
if(Test-Path -LiteralPath $mgsRecordPath){
    $mgsPrior=Get-Content -Raw -LiteralPath $mgsRecordPath | ConvertFrom-Json
    if($mgsPrior.PSObject.Properties['runtimeConfigLease']){
        Restore-MgsRuntimeConfigLease -Lease $mgsPrior.runtimeConfigLease -GameExe $mgsExe
    }
}
# Save the exact prior bytes, including an absent file, before dispatching.
# stop-simulator restores only this lease after the owned processes exit.
$mgsLease=New-MgsRuntimeConfigLease -GameExe $mgsExe -RuntimeManifest $mgsManifest -OperatorDir $mgsLayer
$mgsRecord=@{schema=3;launchMethod='steam_cli';pid=$mgsSteam.Id;started_utc=$mgsSteam.StartTime.ToUniversalTime().ToString('o');runtime=$mgsManifest;operator=$mgsLayer;gameExe=$mgsExe;runtimeConfigLease=$mgsLease;game=$null;ownedRuntimeProcesses=@()}
function Write-MgsTestRecord { [IO.File]::WriteAllText($mgsRecordPath,($mgsRecord|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false)) }
try { Write-MgsTestRecord }
catch {
    Restore-MgsRuntimeConfigLease -Lease $mgsLease -GameExe $mgsExe
    throw
}
$mgsLaunchUtc=[DateTime]::UtcNow
Start-Process -FilePath $mgsSteam.Path -ArgumentList '-applaunch','287700' -WindowStyle Hidden
$mgsDeadline=[DateTime]::UtcNow.AddSeconds(40)
do{
    if(!$mgsRecord.game){
        $mgsRows=@(Get-CimInstance Win32_Process -Filter "Name='mgsvtpp.exe'" | Where-Object {
            $_.ExecutablePath -eq $mgsExe -and $_.CreationDate -and $_.CreationDate.ToUniversalTime() -ge $mgsLaunchUtc -and (Test-MgsLiveProcess $_)
        })
        if($mgsRows.Count -gt 1){throw 'Multiple new MGSV owners appeared; refusing ambiguous session ownership.'}
        if($mgsRows.Count -eq 1){$mgsRecord.game=ConvertTo-MgsProcessIdentity $mgsRows[0];Write-MgsTestRecord}
    }
    if(!$mgsRecord.game){Start-Sleep -Milliseconds 250;continue}
    $mgsGame=Get-Process -Id $mgsRecord.game.pid -ErrorAction SilentlyContinue
    if(!$mgsGame -or $mgsGame.HasExited){throw 'MGSV exited before its simulator initialized; inspect mgs5vr.log.'}
    # A short-lived Steam helper may exit during enumeration and lose its
    # readable path. Only complete identities can be recorded as owned.
    $mgsSnapshot=@(Get-CimInstance Win32_Process | Where-Object { $_.ExecutablePath -and $_.CreationDate -and (Test-MgsLiveProcess $_) })
    $mgsRecord.ownedRuntimeProcesses=@(Get-MgsOwnedRuntimeProcesses -Processes $mgsSnapshot -Game $mgsRecord.game -RuntimeRoot $mgsRuntimeRoot)
    Write-MgsTestRecord
    if(@($mgsRecord.ownedRuntimeProcesses | Where-Object role -eq 'meta_gui').Count -and
       @($mgsRecord.ownedRuntimeProcesses | Where-Object role -eq 'runtime_child').Count){
        Write-Output ('MGSV test process '+$mgsGame.Id+' launched; Steam '+$mgsSteam.Id+' retained. Verify a fresh compositor capture.');return
    }
    Start-Sleep -Milliseconds 250
}while([DateTime]::UtcNow -lt $mgsDeadline)
throw 'Simulator initialization timed out; owned processes remain recorded for cleanup.'
