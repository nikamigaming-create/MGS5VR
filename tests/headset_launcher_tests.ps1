$ErrorActionPreference='Stop'
$mgsTool=Join-Path (Split-Path -Parent $PSScriptRoot) 'tools\launch-headset.ps1'
$mgsFixture=Join-Path ([IO.Path]::GetTempPath()) ('mgs5vr-headset-'+[guid]::NewGuid().ToString('N'))
$mgsNames=@('XR_RUNTIME_JSON','XR_API_LAYER_PATH','XR_ENABLE_API_LAYERS','OPENXR_SIMULATOR_HEADLESS','SteamAppId','SteamGameId')
$mgsOriginal=@{}
foreach ($mgsName in $mgsNames) { $mgsOriginal[$mgsName]=[Environment]::GetEnvironmentVariable($mgsName,'Process') }
$mgsLaunches=[Collections.Generic.List[object]]::new()
$mgsRunning=$false; $mgsFailLaunch=$false
function Assert([bool]$mgsOk,[string]$mgsWhy) { if (-not $mgsOk) { throw $mgsWhy } }
function Refuses([scriptblock]$mgsAction,[string]$mgsPattern) {
    try { & $mgsAction } catch { if ($_.Exception.Message -notlike $mgsPattern) { throw }; return }
    throw ('Expected rejection: '+$mgsPattern)
}
function Get-Process {
    [CmdletBinding()]param([string]$Name)
    if ($Name -eq 'steam') { return [pscustomobject]@{Id=123} }
    if ($mgsRunning) { $mgsProcess=New-Object PSObject; $mgsProcess | Add-Member ScriptMethod WaitForExit { param($ms) $false }; return $mgsProcess }
}
function Get-ItemProperty {
    [CmdletBinding()]param([string]$LiteralPath)
    return [pscustomobject]@{ActiveRuntime=$mgsSim; PreviousActiveRuntime=$mgsPhysical; TppExe=(Join-Path $mgsFixture 'mgsvtpp.exe')}
}
function Start-Process {
    param([string]$FilePath,[string]$WorkingDirectory,[string]$WindowStyle,[switch]$PassThru)
    Assert ($FilePath -eq (Join-Path $mgsFixture 'mgsvtpp.exe')) 'Must launch the selected game, never restart Steam.'
    Assert ($WorkingDirectory -eq $mgsFixture) 'Game working directory changed.'
    $mgsLaunches.Add(@{runtime=$env:XR_RUNTIME_JSON; layers=$env:XR_ENABLE_API_LAYERS; paths=$env:XR_API_LAYER_PATH; headless=$env:OPENXR_SIMULATOR_HEADLESS; appid=$env:SteamAppId; gameid=$env:SteamGameId})
    if ($mgsFailLaunch) { throw 'Authored launch failure' }
    return [pscustomobject]@{Id=456}
}
try {
    New-Item -ItemType Directory -Path $mgsFixture | Out-Null
    $mgsOperator=Join-Path $mgsFixture 'operator'; $mgsOther=Join-Path $mgsFixture 'other'
    New-Item -ItemType Directory -Path $mgsOperator,$mgsOther | Out-Null
    [IO.File]::WriteAllText((Join-Path $mgsFixture 'mgsvtpp.exe'),'Authored non-executable test fixture')
    [IO.File]::WriteAllText((Join-Path $mgsOperator 'XrApiLayer_METAX_operator.json'),'{}')
    $mgsSim=Join-Path $mgsFixture 'sim.json'; $mgsPhysical=Join-Path $mgsFixture 'physical.json'
    [IO.File]::WriteAllText($mgsSim,'{"runtime":{"name":"Meta XR Simulator","library_path":"sim.dll"}}')
    [IO.File]::WriteAllText($mgsPhysical,'{"runtime":{"name":"Fixture physical XR","library_path":"physical.dll"}}')
    $env:XR_RUNTIME_JSON=$mgsSim; $env:XR_ENABLE_API_LAYERS='XR_APILAYER_METAX_operator;XR_APILAYER_other'
    $env:XR_API_LAYER_PATH=$mgsOperator+';'+$mgsOther; $env:OPENXR_SIMULATOR_HEADLESS='1'
    [Environment]::SetEnvironmentVariable('SteamAppId',$null,'Process')
    [Environment]::SetEnvironmentVariable('SteamGameId',$null,'Process')
    & $mgsTool -CheckOnly
    Assert ($mgsLaunches.Count -eq 0) 'CheckOnly launched a process.'
    & $mgsTool
    Assert ($mgsLaunches.Count -eq 1) 'Expected exactly one game process.'
    $mgsLaunch=$mgsLaunches[0]
    Assert ($mgsLaunch.runtime -eq $mgsPhysical) 'Inherited simulator runtime reached the game.'
    Assert ($mgsLaunch.appid -eq '287700' -and $mgsLaunch.gameid -eq '287700') 'Direct launch omitted the TPP Steam identity.'
    Assert ($mgsLaunch.layers -eq 'XR_APILAYER_other' -and $mgsLaunch.paths -eq $mgsOther -and -not $mgsLaunch.headless) 'Operator inputs leaked or unrelated layers were removed.'
    Assert ($env:XR_RUNTIME_JSON -eq $mgsSim -and $env:OPENXR_SIMULATOR_HEADLESS -eq '1') 'Calling environment was changed.'
    Assert (-not $env:SteamAppId -and -not $env:SteamGameId) 'Game identity leaked into the caller.'
    Assert (-not (Test-Path -LiteralPath (Join-Path $mgsFixture 'steam_appid.txt'))) 'Launcher must not create or replace Steam identity files.'
    $env:SteamAppId='892970'; $env:SteamGameId='892970'
    & $mgsTool
    $mgsLaunch=$mgsLaunches[1]
    Assert ($mgsLaunch.appid -eq '287700' -and $mgsLaunch.gameid -eq '287700') 'Another game identity reached TPP.'
    Assert ($env:SteamAppId -eq '892970' -and $env:SteamGameId -eq '892970') 'Parent game identity was not restored.'
    Refuses { & $mgsTool -RuntimeManifest $mgsSim } '*simulator is not a headset runtime*'
    $mgsRunning=$true
    Refuses { & $mgsTool } '*Close the current MGSV session*'
    $mgsRunning=$false; $mgsFailLaunch=$true
    Refuses { & $mgsTool -RuntimeManifest $mgsPhysical } '*Authored launch failure*'
    Assert ($env:XR_RUNTIME_JSON -eq $mgsSim -and $env:XR_ENABLE_API_LAYERS -eq 'XR_APILAYER_METAX_operator;XR_APILAYER_other') 'Failure did not restore the calling environment.'
    Assert ($env:SteamAppId -eq '892970' -and $env:SteamGameId -eq '892970') 'Failed launch did not restore Steam identity.'
    Write-Output 'Headset launcher: simulator isolation, runtime selection, Steam identity isolation, unrelated layers, dry run, live-game refusal and failure cleanup passed.'
} finally {
    foreach ($mgsName in $mgsNames) { [Environment]::SetEnvironmentVariable($mgsName,$mgsOriginal[$mgsName],'Process') }
    $mgsResolved=[IO.Path]::GetFullPath($mgsFixture); $mgsTemp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($mgsResolved.StartsWith($mgsTemp,[StringComparison]::OrdinalIgnoreCase) -and [IO.Path]::GetFileName($mgsResolved).StartsWith('mgs5vr-headset-')) {
        if (Test-Path -LiteralPath $mgsResolved) { Remove-Item -LiteralPath $mgsResolved -Recurse -Force }
    }
}
