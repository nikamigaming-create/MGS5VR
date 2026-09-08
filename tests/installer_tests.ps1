param([string]$PackageRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
Import-Module Microsoft.PowerShell.Utility
Import-Module Microsoft.PowerShell.Management
$mgsPackage = (Resolve-Path -LiteralPath $PackageRoot).Path
$mgsFixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ('mgs5vr-installer-test-' + [guid]::NewGuid().ToString('N'))
$mgsAllowedExes = @()
$mgsRunning = $false

# Only authored, non-executable fixture files receive the supported hash.
# No game process is started or real game installation altered by this test.
function Get-FileHash {
    [CmdletBinding()]param([string]$LiteralPath, [string]$Algorithm)
    if ($LiteralPath -in $mgsAllowedExes) {
        return [pscustomobject]@{Hash='085c2f82d1c963c40b3d2d55786661dfee2b18cbbf388a710c00fa76c5e9bb45'}
    }
    Microsoft.PowerShell.Utility\Get-FileHash -LiteralPath $LiteralPath -Algorithm $Algorithm
}
function Get-Process {
    [CmdletBinding()]param([string]$Name)
    if ($Name -ne 'mgsvtpp') { throw 'Unexpected process lookup.' }
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
        foreach ($mgsRemoved in @('dinput8.dll','mgs5vr.ini','mgs5vr-install.json')) {
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
