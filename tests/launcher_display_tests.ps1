$ErrorActionPreference='Stop'
Import-Module Microsoft.PowerShell.Utility
Import-Module Microsoft.PowerShell.Management
$mgsTool=Join-Path (Split-Path -Parent $PSScriptRoot) 'tools\launcher-display.ps1'
$mgsFixture=Join-Path ([IO.Path]::GetTempPath()) ('mgs5vr-display-'+[guid]::NewGuid().ToString('N'))
$mgsRunning=$false;$mgsLaunches=[pscustomobject]@{Count=0}
function Get-Process {
    [CmdletBinding()]param([string]$Name)
    if ($Name -eq 'steam') { return [pscustomobject]@{Id=123} }
    if ($mgsRunning) { $p=New-Object PSObject; $p | Add-Member ScriptMethod WaitForExit {param($ms) $false}; return $p }
}
function Get-ItemProperty {
    [CmdletBinding()]param([string]$LiteralPath)
    return [pscustomobject]@{ActiveRuntime=(Join-Path $mgsFixture 'headset.json')}
}
function Start-Process {
    param([string]$FilePath,[string]$WorkingDirectory,[string]$WindowStyle,[switch]$PassThru)
    if ($FilePath -notin @((Join-Path $mgsFixture 'mgsvtpp.exe'),'steam://rungameid/311340')) { throw 'Unexpected launch' }
    $mgsLaunches.Count++
    return [pscustomobject]@{Id=456}
}
function Assert([bool]$ok,[string]$why) { if (-not $ok) { throw $why } }
function Refuses([scriptblock]$action,[string]$pattern) {
    $rejected=$false;try { & $action } catch { if ($_.Exception.Message -notlike $pattern) { throw };$rejected=$true }
    Assert $rejected ('Expected refusal: '+$pattern)
}
try {
    New-Item -ItemType Directory -Path $mgsFixture | Out-Null
    $mgsExe=Join-Path $mgsFixture 'mgsvtpp.exe'
    $mgsGz=Join-Path $mgsFixture 'MgsGroundZeroes.exe'
    $mgsConfig=Join-Path $mgsFixture 'TPP_GRAPHICS_CONFIG'
    [IO.File]::WriteAllText($mgsExe,'Not executable: authored test fixture.')
    [IO.File]::WriteAllText($mgsGz,'Not executable: authored test fixture.')
    [IO.File]::WriteAllText((Join-Path $mgsFixture 'headset.json'),'{"runtime":{"name":"Fixture headset","library_path":"fixture.dll"}}')
    $mgsBuiltDll=Join-Path (Split-Path -Parent $PSScriptRoot) 'build\Release\dinput8.dll'
    $mgsFixtureDll=Join-Path $mgsFixture 'dinput8.dll'
    Copy-Item -LiteralPath $mgsBuiltDll -Destination $mgsFixtureDll
    $mgsOriginal='{"as":"keep-me","project":"tpp","graphics":{"quality_setting":{"texture":"ExtraHigh","motion_blur_amount":"Large","depth_of_field":"Enable","ssao":"Off"},"videoout_setting":{"width":1920,"height":1080,"window_mode":"FullScreen","vsync":"Disable","display_index":2}}}'
    [IO.File]::WriteAllText($mgsConfig,$mgsOriginal)
    & $mgsTool -Mode Apply -GameExe $mgsExe -Preset Custom -Width 2560 -Height 2560 -GraphicsConfig $mgsConfig
    $mgsChanged=[IO.File]::ReadAllText($mgsConfig) | ConvertFrom-Json
    Assert ($mgsChanged.graphics.videoout_setting.width -eq 2560 -and $mgsChanged.graphics.videoout_setting.height -eq 2560) 'Custom dimensions were not applied.'
    Assert ($mgsChanged.graphics.videoout_setting.window_mode -eq 'FlexibleWindowed') 'Native arbitrary-size windowed mode must be used.'
    Assert ($mgsChanged.graphics.quality_setting.depth_of_field -eq 'Disable' -and $mgsChanged.graphics.quality_setting.motion_blur_amount -eq 'Off' -and $mgsChanged.graphics.quality_setting.ssao -eq 'Off') 'Native graphics fallback / blur regression.'
    Assert ($mgsChanged.as -eq 'keep-me' -and $mgsChanged.graphics.quality_setting.texture -eq 'ExtraHigh' -and $mgsChanged.graphics.videoout_setting.display_index -eq 2) 'Unrelated graphics settings changed.'
    $mgsBackups=@(Get-ChildItem -LiteralPath $mgsFixture -Filter '*.backup')
    Assert ($mgsBackups.Count -eq 1 -and [IO.File]::ReadAllText($mgsBackups[0].FullName) -ceq $mgsOriginal) 'Exact recovery copy missing.'
    & $mgsTool -Mode Apply -GameExe $mgsExe -Preset Custom -Width 5120 -Height 5120 -GraphicsConfig $mgsConfig
    $mgsChanged=[IO.File]::ReadAllText($mgsConfig) | ConvertFrom-Json
    Assert ($mgsChanged.graphics.videoout_setting.width -eq 5120 -and $mgsChanged.graphics.videoout_setting.height -eq 5120) 'Above-4K dimensions were rejected or clamped.'
    $mgsSaved=[IO.File]::ReadAllText($mgsConfig)
    foreach ($mgsSize in @(@(2559,2560),@(8194,2560),@(0,2560),@(2560,1))) {
        Refuses { & $mgsTool -Mode Apply -GameExe $mgsExe -Preset Custom -Width $mgsSize[0] -Height $mgsSize[1] -GraphicsConfig $mgsConfig } '*Use even native dimensions*'
    }
    $mgsRunning=$true
    Refuses { & $mgsTool -Mode Apply -GameExe $mgsExe -Preset Custom -Width 1280 -Height 720 -GraphicsConfig $mgsConfig } '*Close the game normally*'
    $mgsRunning=$false
    Refuses { & $mgsTool -Mode Apply -GameExe $mgsGz -Preset Custom -Width 1280 -Height 720 -GraphicsConfig $mgsConfig } '*currently supports TPP*'
    Assert ([IO.File]::ReadAllText($mgsConfig) -ceq $mgsSaved) 'Rejected requests changed graphics.'
    Assert ($mgsLaunches.Count -eq 0) 'Apply launched a game.'
    & $mgsTool -Mode Apply -GameExe $mgsExe -Preset Custom -Width 5120 -Height 4096 -GraphicsConfig $mgsConfig
    $mgsHigh=[IO.File]::ReadAllText($mgsConfig) | ConvertFrom-Json
    Assert ($mgsHigh.graphics.videoout_setting.width -eq 5120 -and $mgsHigh.graphics.videoout_setting.height -eq 4096) 'High-resolution Apply did not persist both dimensions.'
    Assert ($mgsHigh.graphics.videoout_setting.window_mode -eq 'FlexibleWindowed') 'High resolution lost the native arbitrary-size mode.'
    $mgsSaved=[IO.File]::ReadAllText($mgsConfig)
    & $mgsTool -Mode Launch -GameExe $mgsExe -Preset Current
    Assert ($mgsLaunches.Count -eq 1) 'Launch did not use the physical headset launcher.'
    Assert ([IO.File]::ReadAllText($mgsConfig) -ceq $mgsSaved) 'Current preset edited graphics.'
    [IO.File]::WriteAllText($mgsFixtureDll,'Authored mismatched DLL fixture; not executable.')
    Refuses { & $mgsTool -Mode Apply -GameExe $mgsExe -Preset Custom -Width 1280 -Height 720 -GraphicsConfig $mgsConfig } '*before applying a resolution*'
    Assert ([IO.File]::ReadAllText($mgsConfig) -ceq $mgsSaved) 'Mismatched DLL must not change graphics settings.'
    Copy-Item -LiteralPath $mgsBuiltDll -Destination $mgsFixtureDll -Force
    [IO.File]::WriteAllText($mgsConfig,'{"project":"wrong","graphics":{}}')
    Refuses { & $mgsTool -Mode Apply -GameExe $mgsExe -Preset Custom -Width 1280 -Height 720 -GraphicsConfig $mgsConfig } '*Not a supported TPP graphics configuration*'
    Write-Output 'Launcher display settings: backups, preservation, validation, running-game refusal and headset launch passed.'
} finally {
    $mgsResolved=[IO.Path]::GetFullPath($mgsFixture)
    $mgsTemp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($mgsResolved.StartsWith($mgsTemp,[StringComparison]::OrdinalIgnoreCase) -and [IO.Path]::GetFileName($mgsResolved).StartsWith('mgs5vr-display-')) {
        if (Test-Path -LiteralPath $mgsResolved) { Remove-Item -LiteralPath $mgsResolved -Recurse -Force }
    }
}
