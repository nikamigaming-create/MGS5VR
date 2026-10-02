# Close only the exact game/runtime identities launched by this checkout.
$ErrorActionPreference='Stop'
Import-Module Microsoft.PowerShell.Utility
Import-Module Microsoft.PowerShell.Management
Import-Module CimCmdlets
$mgsRoot=Split-Path -Parent $PSScriptRoot
$mgsRecordPath=Join-Path $mgsRoot 'artifacts\simulator-steam-session.json'
Import-Module (Join-Path $PSScriptRoot 'simulator-session-ownership.psm1') -Force
if(!(Test-Path -LiteralPath $mgsRecordPath)){throw 'No simulator session owned by this checkout.'}
$mgsRecord=Get-Content -Raw -LiteralPath $mgsRecordPath | ConvertFrom-Json
if(!$mgsRecord.game){
    if($mgsRecord.PSObject.Properties['runtimeConfigLease']){
        if(Get-Process mgsvtpp -ErrorAction SilentlyContinue | Where-Object { !$_.HasExited }){
            throw 'A game is running without an exact recorded owner; preserve its runtime lease for inspection.'
        }
        Restore-MgsRuntimeConfigLease -Lease $mgsRecord.runtimeConfigLease -GameExe $mgsRecord.gameExe
        Remove-Item -LiteralPath $mgsRecordPath
    }
    Write-Output 'No owned game session remains; any temporary runtime configuration was restored.';return
}
# Do not stop a process while an input/capture command still owns the game.
$mgsRunners=@(Get-CimInstance Win32_Process | Where-Object {
    $_.CommandLine -and $_.Name -match '^python' -and $_.CommandLine -match 'gameplay-bot\.py|gameplay-navigate\.py|gameplay-runner\.py'
})
if($mgsRunners.Count){throw 'Finish and release the active gameplay runner before closing its session.'}
function Find-MgsOwnedIdentity($Identity){
    $mgsRow=Get-CimInstance Win32_Process -Filter ("ProcessId="+[int]$Identity.pid)
    if(!$mgsRow){return $null}
    if(!(Test-MgsExactProcessIdentity -Expected $Identity -Current $mgsRow)){return $null}
    $mgsProcess=Get-Process -Id $Identity.pid -ErrorAction SilentlyContinue
    # A debugger/observer handle can retain an exited process in CIM. It is
    # already stopped; Kill then fails with Access Denied on the dead object.
    if($mgsProcess -and !$mgsProcess.HasExited){return $mgsProcess}
    return $null
}
# The simulator starts some children after the initial readiness snapshot.
# Add those descendants while the original game generation is still provable,
# before ending it. Never infer ownership from an executable name alone.
$mgsGameRow=Get-CimInstance Win32_Process -Filter ("ProcessId="+[int]$mgsRecord.game.pid)
if($mgsGameRow -and (Test-MgsExactProcessIdentity $mgsRecord.game $mgsGameRow)){
    $mgsRuntimeRoot=Split-Path -Parent $mgsRecord.runtime
    $mgsSnapshot=@(Get-CimInstance Win32_Process | Where-Object { $_.ExecutablePath -and $_.CreationDate })
    $mgsLateChildren=@(Get-MgsOwnedRuntimeProcesses -Processes $mgsSnapshot -Game $mgsRecord.game -RuntimeRoot $mgsRuntimeRoot)
    $mgsKnown=@{}
    foreach($mgsIdentity in @($mgsRecord.ownedRuntimeProcesses)){$mgsKnown[[int]$mgsIdentity.pid]=$mgsIdentity}
    foreach($mgsIdentity in $mgsLateChildren){
        if($mgsKnown.ContainsKey([int]$mgsIdentity.pid)){
            if(!(Test-MgsExactProcessIdentity $mgsKnown[[int]$mgsIdentity.pid] $mgsIdentity)){throw 'A recorded runtime PID changed generation; refusing cleanup.'}
        }else{$mgsKnown[[int]$mgsIdentity.pid]=$mgsIdentity}
    }
    $mgsRecord.ownedRuntimeProcesses=@($mgsKnown.Values)
    [IO.File]::WriteAllText($mgsRecordPath,($mgsRecord|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
}
$mgsGame=Find-MgsOwnedIdentity $mgsRecord.game
if($mgsGame){
    # Backend process cleanup only: no desktop/window/input automation.
    $mgsGame.Kill();[void]$mgsGame.WaitForExit(8000)
}
foreach($mgsIdentity in @($mgsRecord.ownedRuntimeProcesses | Sort-Object depth -Descending)){
    $mgsProcess=Find-MgsOwnedIdentity $mgsIdentity
    if($mgsProcess){$mgsProcess.Kill();[void]$mgsProcess.WaitForExit(8000)}
}
foreach($mgsIdentity in @($mgsRecord.game)+@($mgsRecord.ownedRuntimeProcesses)){
    if(Find-MgsOwnedIdentity $mgsIdentity){throw 'An owned session process is still alive; its record was preserved.'}
}
# Restore the game-local simulator configuration only after its owned session
# exits. Concurrent personal edits fail closed instead of being overwritten.
if($mgsRecord.PSObject.Properties['runtimeConfigLease']){
    Restore-MgsRuntimeConfigLease -Lease $mgsRecord.runtimeConfigLease -GameExe $mgsRecord.game.path
}
# Neither transport changes Steam's environment. Keep dead identities for
# diagnostics without advertising an inherited Steam runtime.
if($mgsRecord.launchMethod -in @('direct','steam_cli')){
    $mgsArchive=Join-Path $mgsRoot 'artifacts\dev\last-owned-session.json'
    [IO.Directory]::CreateDirectory((Split-Path -Parent $mgsArchive)) | Out-Null
    [IO.File]::WriteAllText($mgsArchive,($mgsRecord|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
    Remove-Item -LiteralPath $mgsRecordPath
}else{
    $mgsSteamRecord=@{schema=1;pid=$mgsRecord.pid;started_utc=$mgsRecord.started_utc;runtime=$mgsRecord.runtime;operator=$mgsRecord.operator}
    [IO.File]::WriteAllText($mgsRecordPath,($mgsSteamRecord|ConvertTo-Json),[Text.UTF8Encoding]::new($false))
}
Write-Output 'Owned MGSV/simulator session closed. Steam retained; no test menu is open.'
