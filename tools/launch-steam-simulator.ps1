param(
    [Parameter(Mandatory=$true)][string]$RuntimeManifest,
    [string]$OperatorDir,
    [switch]$RestartSteam,
    [switch]$Headless
)
$ErrorActionPreference='Stop'
$mgsRuntimePath=(Resolve-Path -LiteralPath $RuntimeManifest).Path
$mgsRuntime=Get-Content -Raw -LiteralPath $mgsRuntimePath | ConvertFrom-Json
if(!$mgsRuntime.runtime.library_path){throw 'Not an OpenXR runtime manifest'}
$mgsOperatorPath=$null
if($OperatorDir){
    $mgsOperatorPath=(Resolve-Path -LiteralPath $OperatorDir).Path
    if(!(Test-Path -LiteralPath (Join-Path $mgsOperatorPath 'XrApiLayer_METAX_operator.json'))){throw 'Meta XR Operator manifest not found'}
}
if(Get-Process mgsvtpp -ErrorAction SilentlyContinue){throw 'MGSV is already running'}
$mgsSteamPath=(Get-ItemProperty -LiteralPath 'HKCU:\Software\Valve\Steam').SteamPath
$mgsSteamExe=Join-Path $mgsSteamPath 'steam.exe'
if(Get-Process steam -ErrorAction SilentlyContinue){
    if($RestartSteam){
        Start-Process -FilePath $mgsSteamExe -ArgumentList '-shutdown' -WindowStyle Hidden
        $mgsDeadline=[DateTime]::UtcNow.AddSeconds(25)
        while(Get-Process steam -ErrorAction SilentlyContinue){
            if([DateTime]::UtcNow -gt $mgsDeadline){throw 'Steam has not exited; no game launch was attempted'}
            Start-Sleep -Milliseconds 250
        }
    }else{
        # Reuse the client that already launched this SIM session. A new
        # caller's XR environment does not replace the running Steam client's.
        Start-Process -FilePath $mgsSteamExe -ArgumentList '-applaunch','287700' -WindowStyle Hidden
        Write-Output 'Launching MGSV through the running Steam client; its existing XR environment is retained.'
        return
    }
}
# Steam forwards launches to an existing client; that client does not inherit
# the new caller's environment. Set these before starting a fresh Steam client.
# No OpenXR registry setting is changed.
$mgsNames=@('XR_RUNTIME_JSON','XR_API_LAYER_PATH','XR_ENABLE_API_LAYERS','OPENXR_SIMULATOR_HEADLESS')
$mgsPrevious=@{}
foreach($mgsName in $mgsNames){$mgsPrevious[$mgsName]=[Environment]::GetEnvironmentVariable($mgsName,'Process')}
try{
    $env:XR_RUNTIME_JSON=$mgsRuntimePath
    [Environment]::SetEnvironmentVariable('XR_API_LAYER_PATH',$mgsOperatorPath,'Process')
    [Environment]::SetEnvironmentVariable('XR_ENABLE_API_LAYERS',$(if($mgsOperatorPath){'XR_APILAYER_METAX_operator'}else{$null}),'Process')
    [Environment]::SetEnvironmentVariable('OPENXR_SIMULATOR_HEADLESS',$(if($Headless){'1'}else{$null}),'Process')
    Start-Process -FilePath $mgsSteamExe -ArgumentList '-applaunch','287700' -WindowStyle Hidden
    Write-Output "Steam launched MGSV with OpenXR runtime: $mgsRuntimePath"
    if($mgsOperatorPath){Write-Output 'Meta XR Operator layer enabled for this launch.'}
}finally{
    foreach($mgsName in $mgsNames){[Environment]::SetEnvironmentVariable($mgsName,$mgsPrevious[$mgsName],'Process')}
}
