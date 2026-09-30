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
if(!$mgsRecord.game){Write-Output 'No owned game session remains.';return}
# Do not stop a process while an input/capture command still owns the game.
$mgsRunners=@(Get-CimInstance Win32_Process | Where-Object {
    $_.CommandLine -and $_.Name -match '^python' -and $_.CommandLine -match 'gameplay-bot\.py|gameplay-navigate\.py|gameplay-runner\.py'
})
if($mgsRunners.Count){throw 'Finish and release the active gameplay runner before closing its session.'}
function Find-MgsOwnedIdentity($Identity){
    $mgsRow=Get-CimInstance Win32_Process -Filter ("ProcessId="+[int]$Identity.pid)
    if(!$mgsRow){return $null}
    if(!(Test-MgsExactProcessIdentity -Expected $Identity -Current $mgsRow)){return $null}
    return Get-Process -Id $Identity.pid -ErrorAction SilentlyContinue
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
# A direct launch's XR environment belongs to the game, never Steam. Keep its
# dead identities for diagnostics without advertising an inherited Steam env.
if($mgsRecord.launchMethod -eq 'direct'){
    $mgsArchive=Join-Path $mgsRoot 'artifacts\dev\last-owned-session.json'
    [IO.Directory]::CreateDirectory((Split-Path -Parent $mgsArchive)) | Out-Null
    [IO.File]::WriteAllText($mgsArchive,($mgsRecord|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
    Remove-Item -LiteralPath $mgsRecordPath
}else{
    $mgsSteamRecord=@{schema=1;pid=$mgsRecord.pid;started_utc=$mgsRecord.started_utc;runtime=$mgsRecord.runtime;operator=$mgsRecord.operator}
    [IO.File]::WriteAllText($mgsRecordPath,($mgsSteamRecord|ConvertTo-Json),[Text.UTF8Encoding]::new($false))
}
Write-Output 'Owned MGSV/simulator session closed. Steam retained; no test menu is open.'
