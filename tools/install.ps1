param(
    [Parameter(Mandatory=$true)][string]$GameDir,
    [switch]$EnableTheatrePreview,
    [switch]$EnableCameraObserver,
    [switch]$EnableHeadCameraExperiment,
    [switch]$EnableControllerRigExperiment,
    [string]$CameraEvidenceDir
)
$ErrorActionPreference = 'Stop'
$mgsRoot = Split-Path -Parent $PSScriptRoot
$mgsTarget = (Resolve-Path -LiteralPath $GameDir).Path
$mgsExe = Join-Path $mgsTarget 'mgsvtpp.exe'
if (-not (Test-Path -LiteralPath $mgsExe -PathType Leaf)) { throw 'Select the folder containing mgsvtpp.exe.' }
if (Get-Process -Name mgsvtpp -ErrorAction SilentlyContinue | Where-Object { -not $_.WaitForExit(0) }) { throw 'Close The Phantom Pain before changing its mod files.' }
$mgsHash = (Get-FileHash -LiteralPath $mgsExe -Algorithm SHA256).Hash.ToLowerInvariant()
if ($mgsHash -ne '085c2f82d1c963c40b3d2d55786661dfee2b18cbbf388a710c00fa76c5e9bb45') {
    throw 'This development preview has only been baselined against executable version 1.0.15.4 / SHA256 085c2f82...e9bb45.'
}
$mgsDll = Join-Path $mgsRoot 'build\Release\dinput8.dll'
if (-not (Test-Path -LiteralPath $mgsDll -PathType Leaf)) { $mgsDll = Join-Path $mgsRoot 'dinput8.dll' }
if (-not (Test-Path -LiteralPath $mgsDll -PathType Leaf)) { throw 'Build the project with tools/build.ps1 first, or extract the complete development artifact.' }
$mgsNames = @('dinput8.dll','mgs5vr.ini','mgs5vr-install.json')
foreach ($mgsName in $mgsNames) {
    if (Test-Path -LiteralPath (Join-Path $mgsTarget $mgsName)) {
        throw "Existing $mgsName was preserved. Use the recorded uninstall script first; do not overwrite another mod."
    }
}
$mgsConfigPath = Join-Path $mgsRoot 'config\mgs5vr.ini'
if (-not (Test-Path -LiteralPath $mgsConfigPath -PathType Leaf)) { $mgsConfigPath = Join-Path $mgsRoot 'mgs5vr.ini' }
$mgsConfig = Get-Content -Raw -LiteralPath $mgsConfigPath
if ($EnableTheatrePreview) { $mgsConfig = $mgsConfig.Replace('enabled=0','enabled=1') }
if ($EnableCameraObserver) { $mgsConfig = $mgsConfig.Replace('camera_observer=0','camera_observer=1') }
if ($EnableHeadCameraExperiment) {
    if (-not $EnableTheatrePreview -or -not $EnableCameraObserver) { throw 'The head-camera experiment requires the OpenXR preview and camera observer.' }
    $mgsConfig = $mgsConfig.Replace('head_camera_experiment=0','head_camera_experiment=1')
}
if ($EnableControllerRigExperiment) {
    if (-not $EnableHeadCameraExperiment) { throw 'The controller rig experiment requires -EnableHeadCameraExperiment.' }
    $mgsConfig = $mgsConfig.Replace('controller_rig_experiment=0','controller_rig_experiment=1')
}
if ($CameraEvidenceDir) {
    if (-not $EnableCameraObserver) { throw '-CameraEvidenceDir requires -EnableCameraObserver.' }
    if (-not [IO.Path]::IsPathFullyQualified($CameraEvidenceDir) -or $CameraEvidenceDir -match '[\r\n]') { throw 'Camera evidence directory must be an absolute single-line path.' }
    $mgsConfig = $mgsConfig.Replace('camera_evidence_dir=',('camera_evidence_dir='+[IO.Path]::GetFullPath($CameraEvidenceDir)))
}
$mgsCreated = @()
try {
    Copy-Item -LiteralPath $mgsDll -Destination (Join-Path $mgsTarget 'dinput8.dll')
    $mgsCreated += 'dinput8.dll'
    [IO.File]::WriteAllText((Join-Path $mgsTarget 'mgs5vr.ini'),$mgsConfig,[Text.UTF8Encoding]::new($false))
    $mgsCreated += 'mgs5vr.ini'
    $mgsFiles = @{}
    foreach ($mgsName in $mgsCreated) { $mgsFiles[$mgsName] = (Get-FileHash -LiteralPath (Join-Path $mgsTarget $mgsName) -Algorithm SHA256).Hash }
    $mgsRecord = @{schema=1; product='MGS5VR theatre preview'; game_sha256=$mgsHash; game_dir=$mgsTarget; installed_utc=[DateTime]::UtcNow.ToString('o'); files=$mgsFiles}
    [IO.File]::WriteAllText((Join-Path $mgsTarget 'mgs5vr-install.json'),($mgsRecord|ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
} catch {
    foreach ($mgsName in $mgsCreated) { Remove-Item -LiteralPath (Join-Path $mgsTarget $mgsName) }
    throw
}
Write-Output "Installed theatre preview into $mgsTarget (enabled=$($EnableTheatrePreview.IsPresent)). Game executable and archives were not modified."
