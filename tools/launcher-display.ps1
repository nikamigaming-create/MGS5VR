param(
    [ValidateSet('Detect','Apply','Launch')][string]$Mode='Detect',
    [string]$GameExe,
    [ValidateSet('Current','Headset','Custom')][string]$Preset='Current',
    [ValidateRange(50,150)][int]$Scale=100,
    [int]$Width=0,[int]$Height=0,
    [string]$GraphicsConfig
)
$ErrorActionPreference='Stop'
Import-Module Microsoft.PowerShell.Utility
Import-Module Microsoft.PowerShell.Management
$mgsPackage=Split-Path -Parent $PSScriptRoot
function Get-BinaryHash([string]$mgsPath) {
    $mgsSha=[Security.Cryptography.SHA256]::Create()
    $mgsStream=[IO.File]::OpenRead($mgsPath)
    try { return [BitConverter]::ToString($mgsSha.ComputeHash($mgsStream)) }
    finally { $mgsStream.Dispose(); $mgsSha.Dispose() }
}
function Read-HeadsetSize {
    $mgsProbe=Join-Path $mgsPackage 'mgs5vr_probe.exe'
    if (-not (Test-Path -LiteralPath $mgsProbe -PathType Leaf)) { $mgsProbe=Join-Path $mgsPackage 'build\Release\mgs5vr_probe.exe' }
    if (-not (Test-Path -LiteralPath $mgsProbe -PathType Leaf)) { throw 'Extract the complete launcher package; mgs5vr_probe.exe is missing.' }
    # No XR session, game launch, runtime switch, Steam restart or desktop change.
    $mgsProbeOutput=& (Join-Path $PSScriptRoot 'launch-headset.ps1') -Probe
    $mgsResult=($mgsProbeOutput -join '') | ConvertFrom-Json
    if (-not $mgsResult.headset_available -or $mgsResult.recommended_width -lt 1 -or $mgsResult.recommended_height -lt 1) {
        throw ('OpenXR did not provide a stereo resolution: '+$mgsResult.error)
    }
    return $mgsResult
}
if ($Mode -eq 'Detect' -or $Preset -eq 'Headset') {
    $mgsHeadset=Read-HeadsetSize
    Write-Output ('OpenXR: {0} / {1}' -f $mgsHeadset.runtime,$mgsHeadset.system)
    Write-Output ('Headset recommendation: {0} x {1} pixels PER EYE.' -f $mgsHeadset.recommended_width,$mgsHeadset.recommended_height)
    Write-Output 'For Virtual Desktop, select your quality in VD first and use its active OpenXR runtime. Ultra/Godlike are not desktop modes.'
    if ($Mode -eq 'Detect') { return }
    $Width=[int]([Math]::Ceiling([double]$mgsHeadset.recommended_width*$Scale/200)*2)
    $Height=[int]([Math]::Ceiling([double]$mgsHeadset.recommended_height*$Scale/200)*2)
    if ($Width -gt $mgsHeadset.maximum_width -or $Height -gt $mgsHeadset.maximum_height) { throw 'Selected scale exceeds the OpenXR texture limit. Reduce the scale.' }
}
if (-not $GameExe -or -not (Test-Path -LiteralPath $GameExe -PathType Leaf)) { throw 'Select your game executable first.' }
$mgsExe=(Resolve-Path -LiteralPath $GameExe).Path
$mgsName=[IO.Path]::GetFileName($mgsExe)
if ($mgsName -notin @('mgsvtpp.exe','MgsGroundZeroes.exe')) { throw 'Choose mgsvtpp.exe or MgsGroundZeroes.exe.' }
$mgsTpp=$mgsName -ieq 'mgsvtpp.exe'
$mgsApp=if ($mgsTpp) {'287700'} else {'311340'}
if (Get-Process -Name ([IO.Path]::GetFileNameWithoutExtension($mgsExe)) -ErrorAction SilentlyContinue | Where-Object { -not $_.WaitForExit(0) }) {
    throw 'Close the game normally before applying resolution. The launcher will not terminate it.'
}
if ($Preset -ne 'Current') {
    if ($Width -lt 640 -or $Width -gt 4096 -or $Height -lt 360 -or $Height -gt 4096 -or $Width%2 -or $Height%2) {
        throw 'Use even native dimensions: width 640..4096, height 360..4096. Reduce headset scale if necessary.'
    }
    # GZ's graphics format must be established independently; never edit it as TPP.
    if (-not $mgsTpp) { throw 'Resolution editing currently supports TPP. Ground Zeroes keeps its existing native graphics settings.' }
    $mgsPackageDll=Join-Path $mgsPackage 'dinput8.dll'
    if (-not (Test-Path -LiteralPath $mgsPackageDll -PathType Leaf)) { $mgsPackageDll=Join-Path $mgsPackage 'build\Release\dinput8.dll' }
    $mgsInstalledDll=Join-Path ([IO.Path]::GetDirectoryName($mgsExe)) 'dinput8.dll'
    if (-not (Test-Path -LiteralPath $mgsPackageDll -PathType Leaf) -or
        -not (Test-Path -LiteralPath $mgsInstalledDll -PathType Leaf) -or
        (Get-BinaryHash $mgsPackageDll) -ne (Get-BinaryHash $mgsInstalledDll)) {
        throw 'Install or Update / Keep My Settings from this complete launcher package before applying a resolution. No graphics settings changed.'
    }
    if (-not $GraphicsConfig) {
        $mgsSteam=(Get-ItemProperty -LiteralPath 'HKCU:\Software\Valve\Steam').SteamPath
        $mgsCandidates=@(Get-ChildItem -LiteralPath (Join-Path $mgsSteam 'userdata') -Directory | ForEach-Object {
            $mgsCandidate=Join-Path $_.FullName '287700\local\TPP_GRAPHICS_CONFIG'
            if (Test-Path -LiteralPath $mgsCandidate -PathType Leaf) { $mgsCandidate }
        })
        if ($mgsCandidates.Count -ne 1) { throw 'Use ACCOUNT CONFIG in the launcher to select this Steam account''s TPP_GRAPHICS_CONFIG. Run TPP once if it does not exist.' }
        $GraphicsConfig=$mgsCandidates[0]
    }
    $mgsGraphicsPath=(Resolve-Path -LiteralPath $GraphicsConfig).Path
    if ([IO.Path]::GetFileName($mgsGraphicsPath) -ne 'TPP_GRAPHICS_CONFIG') { throw 'Select the TPP_GRAPHICS_CONFIG file, not a campaign save.' }
    $mgsOriginal=[IO.File]::ReadAllText($mgsGraphicsPath)
    $mgsGraphics=$mgsOriginal | ConvertFrom-Json
    $mgsVideo=$mgsGraphics.graphics.videoout_setting
    if ($mgsGraphics.project -ne 'tpp' -or -not $mgsVideo -or -not $mgsVideo.PSObject.Properties['width'] -or -not $mgsVideo.PSObject.Properties['height'] -or -not $mgsVideo.PSObject.Properties['window_mode']) { throw 'Not a supported TPP graphics configuration.' }
    # Native Windowed validates against the monitor's mode list. FlexibleWindowed
    # is FOX's own arbitrary-size windowed path; all render targets use this size.
    $mgsVideo.width=$Width; $mgsVideo.height=$Height; $mgsVideo.window_mode='FlexibleWindowed'
    $mgsQuality=$mgsGraphics.graphics.quality_setting
    if (-not $mgsQuality) { throw 'Missing TPP quality settings; run the game once to create its graphics configuration.' }
    $mgsQuality | Add-Member -NotePropertyName depth_of_field -NotePropertyValue 'Disable' -Force
    $mgsQuality | Add-Member -NotePropertyName motion_blur_amount -NotePropertyValue 'Off' -Force
    $mgsNew=$mgsGraphics | ConvertTo-Json -Depth 32
    # Atomic replacement, exact recovery copy beside the source, and stale-read check.
    if ($mgsOriginal -ne [IO.File]::ReadAllText($mgsGraphicsPath)) { throw 'Graphics settings changed while reading. Try again with the game closed.' }
    if ($mgsNew -ne $mgsOriginal) {
        $mgsSuffix=[guid]::NewGuid().ToString('N')
        $mgsTemporary=$mgsGraphicsPath+'.mgs5vr-'+$mgsSuffix+'.tmp'
        $mgsBackup=$mgsGraphicsPath+'.mgs5vr-'+$mgsSuffix+'.backup'
        [IO.File]::WriteAllText($mgsTemporary,$mgsNew,[Text.UTF8Encoding]::new($false))
        try { [IO.File]::Replace($mgsTemporary,$mgsGraphicsPath,$mgsBackup) }
        catch { if (Test-Path -LiteralPath $mgsTemporary) { Remove-Item -LiteralPath $mgsTemporary }; throw }
        Write-Output ('Previous graphics settings: '+$mgsBackup)
    }
    $mgsDisplayPath=Join-Path ([IO.Path]::GetDirectoryName($mgsExe)) 'mgs5vr-display.ini'
    $mgsDisplay="; MGS5VR process-local VR rendering / small desktop preview.`r`n; No Windows resolution, DSR, runtime or Steam settings are changed.`r`n[display]`r`nenabled=1`r`nrender_width=$Width`r`nrender_height=$Height`r`nmirror_width=960`r`nmirror_height=540`r`n"
    if (Test-Path -LiteralPath $mgsDisplayPath) {
        $mgsDisplayBackup=$mgsDisplayPath+'.'+[guid]::NewGuid().ToString('N')+'.backup'
        Copy-Item -LiteralPath $mgsDisplayPath -Destination $mgsDisplayBackup -ErrorAction Stop
        Write-Output ('Previous display settings: '+$mgsDisplayBackup)
    }
    [IO.File]::WriteAllText($mgsDisplayPath,$mgsDisplay,[Text.UTF8Encoding]::new($false))
    Write-Output ('Requested native render size: {0} x {1} PER EYE (before the headset FOV crop). Windowed; Windows resolution unchanged.' -f $Width,$Height)
    Write-Output 'Depth of field OFF. Motion blur OFF. PC preview: 960 x 540 with the matching DLL. Check ACTUAL after launch.'
} else { Write-Output 'Keeping existing game graphics settings.' }
if ($Mode -eq 'Launch') {
    if ($mgsTpp) {
        & (Join-Path $PSScriptRoot 'launch-headset.ps1') -GameDir ([IO.Path]::GetDirectoryName($mgsExe))
    } else {
        Start-Process ('steam://rungameid/'+$mgsApp)
        Write-Output 'Ground Zeroes launch requested through Steam.'
    }
}
