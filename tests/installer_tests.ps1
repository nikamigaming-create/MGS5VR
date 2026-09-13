param([string]$PackageRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
Import-Module Microsoft.PowerShell.Utility
Import-Module Microsoft.PowerShell.Management
$mgsPackage = (Resolve-Path -LiteralPath $PackageRoot).Path
$mgsFixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ('mgs5vr-installer-test-' + [guid]::NewGuid().ToString('N'))
$mgsAllowedExes = @()
$mgsRunning = $false
$mgsFixtureAssetHashes = @{}

# Only authored, non-executable fixture files receive the supported hash.
# No game process is started or real game installation altered by this test.
function Get-FileHash {
    [CmdletBinding()]param([string]$LiteralPath, [string]$Algorithm)
    if ($mgsFixtureAssetHashes.ContainsKey($LiteralPath)) { return [pscustomobject]@{Hash=$mgsFixtureAssetHashes[$LiteralPath]} }
    if ($LiteralPath -in $mgsAllowedExes) {
        if ([IO.Path]::GetFileName($LiteralPath) -ieq 'MgsGroundZeroes.exe') {
            return [pscustomobject]@{Hash='7460d9dba9b6fe34893b5d330aca201983bb1734844f3688acffcc0ab22a1815'}
        }
        return [pscustomobject]@{Hash='085c2f82d1c963c40b3d2d55786661dfee2b18cbbf388a710c00fa76c5e9bb45'}
    }
    Microsoft.PowerShell.Utility\Get-FileHash -LiteralPath $LiteralPath -Algorithm $Algorithm
}
function Get-Process {
    [CmdletBinding()]param([string]$Name)
    if ($Name -notin @('mgsvtpp','MgsGroundZeroes')) { throw 'Unexpected process lookup.' }
    if ($mgsRunning) {
        $mgsProcess = New-Object PSObject
        $mgsProcess | Add-Member -MemberType ScriptMethod -Name WaitForExit -Value { param($Milliseconds) return $false }
        return $mgsProcess
    }
}
function Assert-Installer([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function Assert-Refused([scriptblock]$Action, [string]$Pattern) {
    $mgsRejected = $false
    try { & $Action } catch {
        if ($_.Exception.Message -notlike $Pattern) { throw }
        $mgsRejected = $true
    }
    Assert-Installer $mgsRejected "Expected refusal: $Pattern"
}
try {
    foreach ($mgsCase in @("Player's game [test]",'legacy','defaults','unsupported','running')) {
        $mgsCaseDir = Join-Path $mgsFixtureRoot $mgsCase
        New-Item -ItemType Directory -Path $mgsCaseDir | Out-Null
        $mgsCaseExe = Join-Path $mgsCaseDir 'mgsvtpp.exe'
        [IO.File]::WriteAllText($mgsCaseExe,'Installer fixture only; not executable.')
        if ($mgsCase -ne 'unsupported') { $mgsAllowedExes += $mgsCaseExe }
        if ($mgsCase -in @("Player's game [test]",'legacy')) {
            # Existing, unchanged local imports are reused, never claimed by
            # this installer. Only these authored fixtures get synthetic hashes.
            $mgsAssets = @{
                'retail-assets\Assets\tpp\item\tel\Scenes\tel0_main0_def.fmdl'='935739377E6E0B14EB7186E778E265E65C8E2F66D97B8909D74725800EB21011'
                'retail-assets\Assets\tpp\item\tel\Pictures\tel0_main0_def_c00_bsm.dds'='7CB40D536F37FAA66D8153EF04AFE6A23331D5BCE45908DC7AA3F63558F566B3'
            }
            foreach ($mgsAsset in $mgsAssets.Keys) {
                $mgsAssetPath=Join-Path $mgsCaseDir $mgsAsset
                [IO.Directory]::CreateDirectory((Split-Path -Parent $mgsAssetPath)) | Out-Null
                [IO.File]::WriteAllText($mgsAssetPath,'Authored parser-independent installer fixture; not game content.')
                $mgsFixtureAssetHashes[$mgsAssetPath]=$mgsAssets[$mgsAsset]
            }
        }
    }
    $mgsSetup = Join-Path $mgsPackage 'tools\setup.ps1'
    $mgsInstall = Join-Path $mgsPackage 'tools\install.ps1'
    $mgsUninstall = Join-Path $mgsPackage 'tools\uninstall.ps1'
    foreach ($mgsScript in @($mgsSetup,$mgsInstall,$mgsUninstall)) {
        $mgsParseErrors = $null
        [Management.Automation.Language.Parser]::ParseFile($mgsScript,[ref]$null,[ref]$mgsParseErrors) | Out-Null
        Assert-Installer ($mgsParseErrors.Count -eq 0) "Parse failed: $mgsScript"
    }
    $mgsPlayer = Join-Path $mgsFixtureRoot "Player's game [test]"
    $mgsPlayerExe = Join-Path $mgsPlayer 'mgsvtpp.exe'
    & $mgsSetup -Mode Install -GameExe $mgsPlayerExe
    $mgsConfigPath = Join-Path $mgsPlayer 'mgs5vr.ini'
    $mgsEnabled = [IO.File]::ReadAllText($mgsConfigPath)
    foreach ($mgsKey in @('enabled','camera_observer','head_camera_experiment','controller_rig_experiment','wrist_hud_experiment')) {
        Assert-Installer ($mgsEnabled -match "(?m)^$mgsKey=1\r?$") "Setup failed to enable $mgsKey"
    }
    Assert-Refused { & $mgsSetup -Mode Install -GameExe $mgsPlayerExe } 'Existing dinput8.dll was preserved.*'
    [IO.File]::WriteAllText($mgsConfigPath,$mgsEnabled + "`n; player changes")
    Assert-Refused { & $mgsSetup -Mode Uninstall -GameExe $mgsPlayerExe } 'Modified mgs5vr.ini was preserved.*'
    Assert-Installer (Test-Path -LiteralPath (Join-Path $mgsPlayer 'dinput8.dll')) 'Modified install was partially removed.'
    [IO.File]::WriteAllText($mgsConfigPath,$mgsEnabled,[Text.UTF8Encoding]::new($false))
    $mgsControlsPath = Join-Path $mgsPlayer 'mgs5vr-controls.ini'
    $mgsControlsOriginal = [IO.File]::ReadAllText($mgsControlsPath)
    Assert-Installer (Test-Path -LiteralPath (Join-Path $mgsPlayer 'mgs5vr_controls.exe')) 'Controls checker not installed.'
    [IO.File]::WriteAllText($mgsControlsPath,$mgsControlsOriginal + "`n; custom controller layout")
    Assert-Refused { & $mgsSetup -Mode Uninstall -GameExe $mgsPlayerExe } 'Modified mgs5vr-controls.ini was preserved.*'
    Assert-Installer (Test-Path -LiteralPath (Join-Path $mgsPlayer 'dinput8.dll')) 'Modified controls install was partially removed.'
    [IO.File]::WriteAllText($mgsControlsPath,$mgsControlsOriginal,[Text.UTF8Encoding]::new($false))

    $mgsLegacy = Join-Path $mgsFixtureRoot 'legacy'
    & $mgsInstall -GameDir $mgsLegacy -EnableTheatrePreview -EnableCameraObserver -EnableHeadCameraExperiment -EnableControllerRigExperiment -EnableWristHudExperiment
    Assert-Installer ($mgsEnabled -ceq [IO.File]::ReadAllText((Join-Path $mgsLegacy 'mgs5vr.ini'))) 'Setup differs from established VR configuration.'
    $mgsDefault = Join-Path $mgsFixtureRoot 'defaults'
    & $mgsInstall -GameDir $mgsDefault
    $mgsDisabled = [IO.File]::ReadAllText((Join-Path $mgsDefault 'mgs5vr.ini'))
    foreach ($mgsKey in @('enabled','camera_observer','head_camera_experiment','controller_rig_experiment','wrist_hud_experiment')) {
        Assert-Installer ($mgsDisabled -match "(?m)^$mgsKey=0\r?$") "Raw installer default changed for $mgsKey"
    }
    $mgsUnsupported = Join-Path $mgsFixtureRoot 'unsupported'
    $mgsGzDir = Join-Path $mgsFixtureRoot 'Ground Zeroes'
    New-Item -ItemType Directory -Path $mgsGzDir | Out-Null
    $mgsGzExe = Join-Path $mgsGzDir 'MgsGroundZeroes.exe'
    [IO.File]::WriteAllText($mgsGzExe,'Ground Zeroes fixture only; not executable.')
    $mgsAllowedExes += $mgsGzExe
    Assert-Refused { & $mgsSetup -Mode Install -GameExe $mgsGzExe } 'Ground Zeroes player/weapon/wrist adapters are not connected.*'
    Assert-Installer (-not (Test-Path -LiteralPath (Join-Path $mgsGzDir 'dinput8.dll'))) 'GZ native request silently installed theatre.'
    & $mgsSetup -Mode Install -GameExe $mgsGzExe -TheatrePreview
    $mgsGzConfig = [IO.File]::ReadAllText((Join-Path $mgsGzDir 'mgs5vr.ini'))
    Assert-Installer ($mgsGzConfig -match '(?m)^enabled=1\r?$') 'GZ explicit preview not enabled.'
    foreach ($mgsKey in @('camera_observer','head_camera_experiment','controller_rig_experiment','wrist_hud_experiment')) {
        Assert-Installer ($mgsGzConfig -match "(?m)^$mgsKey=0\r?$") "GZ must not enable TPP $mgsKey"
    }
    $mgsRunning = $true
    Assert-Refused { & $mgsSetup -Mode Uninstall -GameExe $mgsGzExe } 'Close Ground Zeroes*'
    $mgsRunning = $false
    & $mgsSetup -Mode Uninstall -GameExe $mgsGzExe
    Assert-Installer (Test-Path -LiteralPath $mgsGzExe) 'GZ executable was removed.'
    & $mgsInstall -GameDir $mgsGzDir -EnableTheatrePreview -EnableCameraObserver -EnableHeadCameraExperiment
    $mgsSceneConfig=[IO.File]::ReadAllText((Join-Path $mgsGzDir 'mgs5vr.ini'))
    Assert-Installer ($mgsSceneConfig -match '(?m)^head_camera_experiment=1\r?$') 'GZ explicit scene experiment not enabled.'
    Assert-Installer ($mgsSceneConfig -match '(?m)^controller_rig_experiment=0\r?$') 'GZ scene experiment enabled the TPP rig.'
    Assert-Installer ($mgsSceneConfig -match '(?m)^wrist_hud_experiment=0\r?$') 'GZ scene experiment enabled the TPP wrist.'
    & $mgsSetup -Mode Uninstall -GameExe $mgsGzExe
    Assert-Refused { & $mgsInstall -GameDir $mgsGzDir -EnableVR } 'Ground Zeroes player/weapon/wrist adapters are not connected.*'
    Assert-Refused { & $mgsSetup -Mode Install -GameExe (Join-Path $mgsUnsupported 'mgsvtpp.exe') } '*baselined against executable version*'
    Assert-Installer (-not (Test-Path -LiteralPath (Join-Path $mgsUnsupported 'dinput8.dll'))) 'Unsupported game was modified.'
    $mgsWrong = Join-Path $mgsUnsupported 'another.exe'
    [IO.File]::WriteAllText($mgsWrong,'Not a game.')
    Assert-Refused { & $mgsSetup -Mode Install -GameExe $mgsWrong } 'Select mgsvtpp.exe*'
    $mgsRunning = $true
    $mgsBusyDir = Join-Path $mgsFixtureRoot 'running'
    Assert-Refused { & $mgsSetup -Mode Install -GameExe (Join-Path $mgsBusyDir 'mgsvtpp.exe') } 'Close The Phantom Pain*'
    Assert-Refused { & $mgsSetup -Mode Uninstall -GameExe $mgsPlayerExe } 'Close The Phantom Pain*'
    Assert-Installer (-not (Test-Path -LiteralPath (Join-Path $mgsBusyDir 'dinput8.dll'))) 'Running game was modified.'
    $mgsRunning = $false

    foreach ($mgsInstalled in @($mgsPlayer,$mgsLegacy,$mgsDefault)) {
        [IO.File]::WriteAllText((Join-Path $mgsInstalled 'mgs5vr.log'),'Keep diagnostic log.')
        [IO.File]::WriteAllText((Join-Path $mgsInstalled 'player-data.txt'),'Keep unrelated data.')
        & $mgsSetup -Mode Uninstall -GameExe (Join-Path $mgsInstalled 'mgsvtpp.exe')
        foreach ($mgsRemoved in @('dinput8.dll','mgs5vr.ini','mgs5vr-controls.ini','mgs5vr_controls.exe','mgs5vr-install.json')) {
            Assert-Installer (-not (Test-Path -LiteralPath (Join-Path $mgsInstalled $mgsRemoved))) "Uninstall retained $mgsRemoved"
        }
        foreach ($mgsKept in @('mgsvtpp.exe','mgs5vr.log','player-data.txt')) {
            Assert-Installer (Test-Path -LiteralPath (Join-Path $mgsInstalled $mgsKept)) "Uninstall removed $mgsKept"
        }
    }
    Write-Output "Installer checks passed on PowerShell $($PSVersionTable.PSVersion): selected-file setup, VR preset, legacy/default compatibility, unusual paths, existing/modified files, unsupported/wrong executable, running-game refusal and removal. File-picker UI was not automated."
} finally {
    $mgsResolvedFixture = [IO.Path]::GetFullPath($mgsFixtureRoot)
    $mgsTempParent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')
    if ((Split-Path -Parent $mgsResolvedFixture) -ne $mgsTempParent -or
        (Split-Path -Leaf $mgsResolvedFixture) -notmatch '^mgs5vr-installer-test-[a-f0-9]{32}$') {
        throw 'Refusing cleanup outside the unique installer fixture directory.'
    }
    if (Test-Path -LiteralPath $mgsResolvedFixture) { Remove-Item -LiteralPath $mgsResolvedFixture -Recurse -Force }
}
