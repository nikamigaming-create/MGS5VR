param(
    [Parameter(Mandatory=$true)][string]$GameDir,
    [Parameter(Mandatory=$true)][string]$RuntimeManifest,
    [string]$OperatorDir,
    [string]$GraphicsConfig,
    [switch]$Headless
)
$ErrorActionPreference = 'Stop'
$mgsRoot = Split-Path -Parent $PSScriptRoot
$mgsTarget = (Resolve-Path -LiteralPath $GameDir).Path
$mgsExe = Join-Path $mgsTarget 'mgsvtpp.exe'
if (-not (Test-Path -LiteralPath $mgsExe -PathType Leaf)) { throw 'Select the folder containing mgsvtpp.exe.' }
if (Get-Process -Name mgsvtpp -ErrorAction SilentlyContinue | Where-Object { -not $_.WaitForExit(0) }) { throw 'The Phantom Pain is already running.' }
$mgsManifest = (Resolve-Path -LiteralPath $RuntimeManifest).Path
$mgsRuntime = Get-Content -Raw -LiteralPath $mgsManifest | ConvertFrom-Json
if (-not $mgsRuntime.runtime.library_path) { throw 'Not an OpenXR runtime manifest.' }
if ($OperatorDir -and -not (Test-Path -LiteralPath (Join-Path $OperatorDir 'XrApiLayer_METAX_operator.json'))) { throw 'Meta XR Operator layer manifest not found.' }
if (-not $GraphicsConfig) {
    $mgsSteam = (Get-ItemProperty -LiteralPath 'HKCU:\Software\Valve\Steam').SteamPath
    $mgsFound = @(Get-ChildItem -LiteralPath (Join-Path $mgsSteam 'userdata') -Directory | ForEach-Object {
        $mgsCandidate = Join-Path $_.FullName '287700\local\TPP_GRAPHICS_CONFIG'
        if (Test-Path -LiteralPath $mgsCandidate -PathType Leaf) { $mgsCandidate }
    })
    if ($mgsFound.Count -ne 1) { throw 'Specify -GraphicsConfig for the Steam account used by this test.' }
    $GraphicsConfig = $mgsFound[0]
}
$mgsGraphicsPath = (Resolve-Path -LiteralPath $GraphicsConfig).Path
$mgsGraphicsText = [IO.File]::ReadAllText($mgsGraphicsPath)
$mgsGraphics = $mgsGraphicsText | ConvertFrom-Json
if ($mgsGraphics.project -ne 'tpp' -or -not $mgsGraphics.graphics.videoout_setting) { throw 'Not a TPP graphics configuration.' }
# This launcher always uses a normal 1280x720 window, per the requested test setup.
if ($mgsGraphics.graphics.videoout_setting.window_mode -ne 'Windowed' -or
    $mgsGraphics.graphics.videoout_setting.width -ne 1280 -or $mgsGraphics.graphics.videoout_setting.height -ne 720) {
    $mgsArtifacts = Join-Path $mgsRoot 'artifacts'
    New-Item -ItemType Directory -Path $mgsArtifacts -Force | Out-Null
    $mgsBackup = Join-Path $mgsArtifacts ('TPP_GRAPHICS_CONFIG.'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffffff')+'.backup')
    Copy-Item -LiteralPath $mgsGraphicsPath -Destination $mgsBackup -ErrorAction Stop
    $mgsGraphics.graphics.videoout_setting.window_mode = 'Windowed'
    $mgsGraphics.graphics.videoout_setting.width = 1280
    $mgsGraphics.graphics.videoout_setting.height = 720
    [IO.File]::WriteAllText($mgsGraphicsPath,($mgsGraphics | ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
}
$mgsVariables = @('XR_RUNTIME_JSON','XR_API_LAYER_PATH','XR_ENABLE_API_LAYERS','OPENXR_SIMULATOR_HEADLESS')
$mgsPrevious = @{}
foreach ($mgsVariable in $mgsVariables) { $mgsPrevious[$mgsVariable] = [Environment]::GetEnvironmentVariable($mgsVariable,'Process') }
Push-Location $mgsTarget
try {
    $env:XR_RUNTIME_JSON = $mgsManifest
    if ($OperatorDir) {
        $env:XR_API_LAYER_PATH = (Resolve-Path -LiteralPath $OperatorDir).Path
        $env:XR_ENABLE_API_LAYERS = 'XR_APILAYER_METAX_operator'
    } else {
        [Environment]::SetEnvironmentVariable('XR_API_LAYER_PATH',$null,'Process')
        [Environment]::SetEnvironmentVariable('XR_ENABLE_API_LAYERS',$null,'Process')
    }
    [Environment]::SetEnvironmentVariable('OPENXR_SIMULATOR_HEADLESS',$(if ($Headless) {'1'} else {$null}),'Process')
    & $mgsExe
} finally {
    Pop-Location
    foreach ($mgsVariable in $mgsVariables) { [Environment]::SetEnvironmentVariable($mgsVariable,$mgsPrevious[$mgsVariable],'Process') }
}
