param(
    [string]$GameDir,
    [string]$RuntimeManifest,
    [switch]$CheckOnly,
    [switch]$Probe,
    [switch]$RestartSteam
)
$ErrorActionPreference='Stop'
if (-not $GameDir -and -not $Probe) {
    $mgsSaved=Get-ItemProperty -LiteralPath 'HKCU:\Software\Nikami\MGS5VR\Launcher' -ErrorAction SilentlyContinue
    if ($mgsSaved.TppExe) { $GameDir=Split-Path -Parent $mgsSaved.TppExe }
}
if (-not $Probe) {
    if (-not $GameDir) { throw 'Select TPP in the launcher first, or pass -GameDir with the folder containing mgsvtpp.exe.' }
    $mgsTarget=(Resolve-Path -LiteralPath $GameDir).Path
    $mgsExe=Join-Path $mgsTarget 'mgsvtpp.exe'
    if (-not (Test-Path -LiteralPath $mgsExe -PathType Leaf)) { throw 'Select the folder containing mgsvtpp.exe.' }
    if (Get-Process -Name mgsvtpp -ErrorAction SilentlyContinue | Where-Object { -not $_.WaitForExit(0) }) {
        throw 'Close the current MGSV session before starting the headset session.'
    }
}
function Read-MgsRuntime([string]$mgsPath) {
    if (-not $mgsPath -or -not (Test-Path -LiteralPath $mgsPath -PathType Leaf)) { return $null }
    $mgsResolved=(Resolve-Path -LiteralPath $mgsPath).Path
    $mgsDefinition=Get-Content -Raw -LiteralPath $mgsResolved | ConvertFrom-Json
    if (-not $mgsDefinition.runtime.library_path) { throw 'Not an OpenXR runtime manifest.' }
    return [pscustomobject]@{
        Path=$mgsResolved
        Simulator=($mgsResolved -match 'MetaXRSimulator' -or
            $mgsDefinition.runtime.name -match 'Simulator' -or
            $mgsDefinition.runtime.library_path -match 'Simulator')
    }
}
if ($RuntimeManifest) {
    $mgsRuntime=Read-MgsRuntime $RuntimeManifest
} else {
    $mgsRuntimeSettings=Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Khronos\OpenXR\1' -ErrorAction SilentlyContinue
    $mgsRuntime=Read-MgsRuntime $mgsRuntimeSettings.ActiveRuntime
    if ($mgsRuntime -and $mgsRuntime.Simulator) {
        $mgsRuntime=Read-MgsRuntime $mgsRuntimeSettings.PreviousActiveRuntime
        if ($mgsRuntime -and -not $mgsRuntime.Simulator -and -not $Probe) {
            Write-Output 'Simulator is selected globally; using the recorded previous headset runtime for MGSV only.'
        }
    }
}
if (-not $mgsRuntime -or $mgsRuntime.Simulator) {
    throw 'Select your physical headset OpenXR runtime, or pass its JSON with -RuntimeManifest. A simulator is not a headset runtime.'
}
if (-not $Probe) { Write-Output ('Headset runtime: '+$mgsRuntime.Path) }
if ($CheckOnly) { Write-Output ('Ready to launch: '+$mgsExe); return }
if ($RestartSteam -and -not $Probe) {
    & (Join-Path $PSScriptRoot 'launch-steam-simulator.ps1') -RuntimeManifest $mgsRuntime.Path -RestartSteam
    return
}
if (-not $Probe -and -not (Get-Process -Name steam -ErrorAction SilentlyContinue)) {
    throw 'Open Steam and sign in before launching MGSV. Steam does not need to be restarted.'
}
# A running Steam client retains its old XR environment. Launch the owned game
# directly so only this child receives the requested runtime and clean controls.
$mgsNames=@('XR_RUNTIME_JSON','XR_API_LAYER_PATH','XR_ENABLE_API_LAYERS','OPENXR_SIMULATOR_HEADLESS','SteamAppId','SteamGameId')
$mgsPrevious=@{}
foreach ($mgsName in $mgsNames) { $mgsPrevious[$mgsName]=[Environment]::GetEnvironmentVariable($mgsName,'Process') }
try {
    $env:XR_RUNTIME_JSON=$mgsRuntime.Path
    $mgsLayers=@($mgsPrevious.XR_ENABLE_API_LAYERS -split ';' | Where-Object { $_ -and $_ -ne 'XR_APILAYER_METAX_operator' })
    $mgsLayerPaths=@($mgsPrevious.XR_API_LAYER_PATH -split ';' | Where-Object {
        $_ -and -not (Test-Path -LiteralPath (Join-Path $_ 'XrApiLayer_METAX_operator.json'))
    })
    [Environment]::SetEnvironmentVariable('XR_ENABLE_API_LAYERS',($mgsLayers -join ';'),'Process')
    [Environment]::SetEnvironmentVariable('XR_API_LAYER_PATH',($mgsLayerPaths -join ';'),'Process')
    [Environment]::SetEnvironmentVariable('OPENXR_SIMULATOR_HEADLESS',$null,'Process')
    if ($Probe) {
        $mgsPackage=Split-Path -Parent $PSScriptRoot
        $mgsProbe=Join-Path $mgsPackage 'mgs5vr_probe.exe'
        if (-not (Test-Path -LiteralPath $mgsProbe -PathType Leaf)) { $mgsProbe=Join-Path $mgsPackage 'build\Release\mgs5vr_probe.exe' }
        if (-not (Test-Path -LiteralPath $mgsProbe -PathType Leaf)) { throw 'Extract the complete package; mgs5vr_probe.exe is missing.' }
        $mgsProbeOutput=& $mgsProbe
        if ($LASTEXITCODE -ne 0) { throw ('Connect PC VR, then Detect again. '+($mgsProbeOutput -join ' ')) }
        $mgsProbeOutput
    } else {
        # Direct launch keeps XR settings local to MGSV, but Explorer does not
        # supply Steam's app identity. Set it for this child, including when a
        # different game's identity was inherited from the parent launcher.
        $env:SteamAppId='287700'
        $env:SteamGameId='287700'
        $mgsGame=Start-Process -FilePath $mgsExe -WorkingDirectory $mgsTarget -WindowStyle Normal -PassThru
        Write-Output ('Headset game launched, PID '+$mgsGame.Id+'. Steam and other applications were retained.')
    }
} finally {
    foreach ($mgsName in $mgsNames) { [Environment]::SetEnvironmentVariable($mgsName,$mgsPrevious[$mgsName],'Process') }
}
