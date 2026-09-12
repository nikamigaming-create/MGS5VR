param(
    [Parameter(Mandatory=$true)][string]$GameDir,
    [Parameter(Mandatory=$true)][string]$RuntimeManifest,
    [string]$OperatorDir,
    [string]$GraphicsConfig,
    [int]$RenderWidth=0,
    [int]$RenderHeight=0,
    [switch]$Headless,
    [switch]$RestartSteam
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
if (($RenderWidth -eq 0) -ne ($RenderHeight -eq 0)) { throw 'Specify both -RenderWidth and -RenderHeight.' }
if ($RenderWidth -and ($RenderWidth -lt 640 -or $RenderWidth -gt 4096 -or $RenderHeight -lt 360 -or $RenderHeight -gt 4096 -or $RenderWidth % 2 -or $RenderHeight % 2)) {
    throw 'Render dimensions must be even pixels, width 640..4096 and height 360..4096.'
}
# Keep the lightweight SIM default. A physical launch preserves the game's
# selected resolution unless an explicit render size is requested.
if (-not $RenderWidth -and $Headless) { $RenderWidth=1280; $RenderHeight=720 }
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
# These are native render dimensions: both eye images are drawn at this size.
# Upscaling the XR swapchain or the capture cannot recover lost HUD detail.
if ($mgsGraphics.graphics.videoout_setting.window_mode -ne 'Windowed' -or
    ($RenderWidth -and ($mgsGraphics.graphics.videoout_setting.width -ne $RenderWidth -or $mgsGraphics.graphics.videoout_setting.height -ne $RenderHeight))) {
    $mgsArtifacts = Join-Path $mgsRoot 'artifacts'
    New-Item -ItemType Directory -Path $mgsArtifacts -Force | Out-Null
    $mgsBackup = Join-Path $mgsArtifacts ('TPP_GRAPHICS_CONFIG.'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffffff')+'.backup')
    Copy-Item -LiteralPath $mgsGraphicsPath -Destination $mgsBackup -ErrorAction Stop
    $mgsGraphics.graphics.videoout_setting.window_mode = 'Windowed'
    if ($RenderWidth) {
        $mgsGraphics.graphics.videoout_setting.width = $RenderWidth
        $mgsGraphics.graphics.videoout_setting.height = $RenderHeight
    }
    [IO.File]::WriteAllText($mgsGraphicsPath,($mgsGraphics | ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
}
Write-Output ('Native render size: {0}x{1}' -f $mgsGraphics.graphics.videoout_setting.width,$mgsGraphics.graphics.videoout_setting.height)
& (Join-Path $PSScriptRoot 'launch-steam-simulator.ps1') -RuntimeManifest $mgsManifest `
    -OperatorDir $OperatorDir -Headless:$Headless -RestartSteam:$RestartSteam
