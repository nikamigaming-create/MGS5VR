param(
    [Parameter(Mandatory=$true)][string]$GameDir,
    [switch]$EnableVR,
    [switch]$EnableTheatrePreview,
    [switch]$EnableCameraObserver,
    [switch]$EnableHeadCameraExperiment,
    [switch]$EnableControllerRigExperiment,
    [switch]$EnableWristHudExperiment,
    [string]$CameraEvidenceDir
)
$ErrorActionPreference = 'Stop'
if ($EnableVR) {
    $EnableTheatrePreview = $true
    $EnableCameraObserver = $true
    $EnableHeadCameraExperiment = $true
    $EnableControllerRigExperiment = $true
    $EnableWristHudExperiment = $true
}
$mgsRoot = Split-Path -Parent $PSScriptRoot
$mgsTarget = (Resolve-Path -LiteralPath $GameDir).Path
$mgsExe = Join-Path $mgsTarget 'mgsvtpp.exe'
$mgsGameName = 'The Phantom Pain'
$mgsExpectedHash = '085c2f82d1c963c40b3d2d55786661dfee2b18cbbf388a710c00fa76c5e9bb45'
$mgsNativeAdapter = $true
if (-not (Test-Path -LiteralPath $mgsExe -PathType Leaf)) {
    $mgsExe = Join-Path $mgsTarget 'MgsGroundZeroes.exe'
    $mgsGameName = 'Ground Zeroes'
    $mgsExpectedHash = '7460d9dba9b6fe34893b5d330aca201983bb1734844f3688acffcc0ab22a1815'
    $mgsNativeAdapter = $false
}
if (-not (Test-Path -LiteralPath $mgsExe -PathType Leaf)) { throw 'Select the folder containing mgsvtpp.exe or MgsGroundZeroes.exe.' }
$mgsProcessName = [IO.Path]::GetFileNameWithoutExtension($mgsExe)
if (Get-Process -Name $mgsProcessName -ErrorAction SilentlyContinue | Where-Object { -not $_.WaitForExit(0) }) { throw "Close $mgsGameName before changing its mod files." }
$mgsHash = (Get-FileHash -LiteralPath $mgsExe -Algorithm SHA256).Hash.ToLowerInvariant()
if ($mgsHash -ne $mgsExpectedHash) {
    throw 'This development preview has only been baselined against executable version TPP 1.0.15.4 or GZ 1.0.0.5 with the exact supported SHA256.'
}
if (-not $mgsNativeAdapter -and ($EnableControllerRigExperiment -or $EnableWristHudExperiment)) {
    throw 'Ground Zeroes player/weapon/wrist adapters are not connected. Only explicit theatre or scene-camera experiments are available; no TPP hooks will be installed.'
}
$mgsDll = Join-Path $mgsRoot 'build\Release\dinput8.dll'
if (-not (Test-Path -LiteralPath $mgsDll -PathType Leaf)) { $mgsDll = Join-Path $mgsRoot 'dinput8.dll' }
if (-not (Test-Path -LiteralPath $mgsDll -PathType Leaf)) { throw 'Build the project with tools/build.ps1 first, or extract the complete development artifact.' }
$mgsChecker = Join-Path $mgsRoot 'build\Release\mgs5vr_controls.exe'
if (-not (Test-Path -LiteralPath $mgsChecker -PathType Leaf)) { $mgsChecker = Join-Path $mgsRoot 'mgs5vr_controls.exe' }
$mgsControls = Join-Path $mgsRoot 'config\mgs5vr-controls.ini'
if (-not (Test-Path -LiteralPath $mgsControls -PathType Leaf)) { $mgsControls = Join-Path $mgsRoot 'mgs5vr-controls.ini' }
foreach ($mgsRequired in @($mgsChecker,$mgsControls)) {
    if (-not (Test-Path -LiteralPath $mgsRequired -PathType Leaf)) { throw "Missing controls package file: $mgsRequired" }
}
$mgsNames = @('dinput8.dll','mgs5vr.ini','mgs5vr-controls.ini','mgs5vr_controls.exe','mgs5vr-install.json')
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
if ($EnableWristHudExperiment) {
    if (-not $EnableControllerRigExperiment) { throw 'The left-arm HUD requires -EnableControllerRigExperiment.' }
    $mgsConfig = $mgsConfig.Replace('wrist_hud_experiment=0','wrist_hud_experiment=1')
}
if ($CameraEvidenceDir) {
    if (-not $EnableCameraObserver) { throw '-CameraEvidenceDir requires -EnableCameraObserver.' }
    if (-not [IO.Path]::IsPathFullyQualified($CameraEvidenceDir) -or $CameraEvidenceDir -match '[\r\n]') { throw 'Camera evidence directory must be an absolute single-line path.' }
    $mgsConfig = $mgsConfig.Replace('camera_evidence_dir=',('camera_evidence_dir='+[IO.Path]::GetFullPath($CameraEvidenceDir)))
}
$mgsCreated = @()
$mgsAssetHashes = @{}
if ($mgsNativeAdapter -and $EnableControllerRigExperiment) {
    $mgsAssetHashes = @{
        'retail-assets\Assets\tpp\item\tel\Scenes\tel0_main0_def.fmdl' = '935739377E6E0B14EB7186E778E265E65C8E2F66D97B8909D74725800EB21011'
        'retail-assets\Assets\tpp\item\tel\Pictures\tel0_main0_def_c00_bsm.dds' = '7CB40D536F37FAA66D8153EF04AFE6A23331D5BCE45908DC7AA3F63558F566B3'
    }
    foreach ($mgsName in $mgsAssetHashes.Keys) {
        $mgsAsset = Join-Path $mgsTarget $mgsName
        if ((Test-Path -LiteralPath $mgsAsset) -and (Get-FileHash -LiteralPath $mgsAsset -Algorithm SHA256).Hash -ne $mgsAssetHashes[$mgsName]) {
            throw "Existing modified binocular asset was preserved: $mgsName"
        }
    }
}
try {
    $mgsMissingAssets = @($mgsAssetHashes.Keys | Where-Object { -not (Test-Path -LiteralPath (Join-Path $mgsTarget $_) -PathType Leaf) })
    if ($mgsMissingAssets.Count) {
        $mgsImporter = Join-Path $mgsRoot 'build\Release\mgs5vr_import.exe'
        if (-not (Test-Path -LiteralPath $mgsImporter -PathType Leaf)) { $mgsImporter = Join-Path $mgsRoot 'mgs5vr_import.exe' }
        if (-not (Test-Path -LiteralPath $mgsImporter -PathType Leaf)) { throw 'Missing owned-asset importer. Extract the complete MGS5VR package.' }
        & $mgsImporter $mgsTarget (Join-Path $mgsTarget 'retail-assets')
        if ($LASTEXITCODE -ne 0) { throw 'Binocular import failed; no VR DLL was installed. Verify the owned game files in Steam.' }
        $mgsCreated += $mgsMissingAssets
    }
    Copy-Item -LiteralPath $mgsDll -Destination (Join-Path $mgsTarget 'dinput8.dll')
    $mgsCreated += 'dinput8.dll'
    [IO.File]::WriteAllText((Join-Path $mgsTarget 'mgs5vr.ini'),$mgsConfig,[Text.UTF8Encoding]::new($false))
    $mgsCreated += 'mgs5vr.ini'
    Copy-Item -LiteralPath $mgsControls -Destination (Join-Path $mgsTarget 'mgs5vr-controls.ini')
    $mgsCreated += 'mgs5vr-controls.ini'
    Copy-Item -LiteralPath $mgsChecker -Destination (Join-Path $mgsTarget 'mgs5vr_controls.exe')
    $mgsCreated += 'mgs5vr_controls.exe'
    $mgsFiles = @{}
    foreach ($mgsName in $mgsCreated) { $mgsFiles[$mgsName] = (Get-FileHash -LiteralPath (Join-Path $mgsTarget $mgsName) -Algorithm SHA256).Hash }
    $mgsRecord = @{schema=1; product='MGS5VR theatre preview'; game_executable=[IO.Path]::GetFileName($mgsExe); game_sha256=$mgsHash; game_dir=$mgsTarget; installed_utc=[DateTime]::UtcNow.ToString('o'); files=$mgsFiles}
    [IO.File]::WriteAllText((Join-Path $mgsTarget 'mgs5vr-install.json'),($mgsRecord|ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
} catch {
    foreach ($mgsName in $mgsCreated) {
        $mgsPath = Join-Path $mgsTarget $mgsName
        if (Test-Path -LiteralPath $mgsPath) { Remove-Item -LiteralPath $mgsPath }
    }
    throw
}
$mgsMode = if ($EnableHeadCameraExperiment -and -not $mgsNativeAdapter) { 'GZ native scene-camera experiment' } elseif ($EnableHeadCameraExperiment) { 'tracked VR' } elseif ($EnableTheatrePreview) { 'large-screen preview' } else { 'disabled preview' }
Write-Output "Installed MGS5VR ($mgsMode) into $mgsTarget. Game executable, archives and saves were not modified."
if (-not $mgsNativeAdapter) {
    Write-Output 'Ground Zeroes: independent native scene stereo is experimental; tracked player arms, weapon aiming and wrist UI are not connected.'
    if ($EnableHeadCameraExperiment) { Write-Output 'Bind [system].toggle_vr in mgs5vr-controls.ini to enter the GZ scene-camera experiment. It is not automatic tracked first-person VR.' }
}
if ($EnableHeadCameraExperiment -and $mgsNativeAdapter) {
    Write-Output 'Launch the game and load Continue/Resume Game. Tracked VR enters automatically.'
}
Write-Output "Edit controls in $mgsTarget\mgs5vr-controls.ini; save and restart MGSV to apply."
