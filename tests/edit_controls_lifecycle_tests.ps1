param([switch]$EditorDriver)
$ErrorActionPreference='Stop'
$mgsTestEditor=Join-Path (Split-Path -Parent $PSScriptRoot) 'tools\edit-controls.ps1'
if (-not $EditorDriver) {
    & $mgsTestEditor -LifecycleTest
    return
}

# Dot-sourced by edit-controls.ps1 after its real handlers are registered.
# The editor deliberately keeps its Form hidden; this drives only its controls.
$mgsTestTempBase=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar,[IO.Path]::AltDirectorySeparatorChar)
$mgsTestLeaf='mgs5vr-controls-lifecycle-'+[Guid]::NewGuid().ToString('N')
$mgsTestRoot=[IO.Path]::GetFullPath((Join-Path $mgsTestTempBase $mgsTestLeaf))
$mgsTestParent=[IO.Path]::GetDirectoryName($mgsTestRoot).TrimEnd([IO.Path]::DirectorySeparatorChar,[IO.Path]::AltDirectorySeparatorChar)
if ($mgsTestParent -ne $mgsTestTempBase -or $mgsTestLeaf -notmatch '^mgs5vr-controls-lifecycle-[0-9a-f]{32}$') {
    throw 'Controls lifecycle test directory failed its temp-path containment check.'
}
$mgsTestPath=Join-Path $mgsTestRoot 'mgs5vr-controls.ini'
$mgsFixture=Join-Path $mgsRoot 'config\mgs5vr-controls.ini'
New-Item -ItemType Directory -Path $mgsTestRoot | Out-Null

function Assert-MgsLifecycle([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }
function Wait-MgsDebounce {
    $deadline=[DateTime]::UtcNow.AddSeconds(5)
    while ($mgsTimer.Enabled -and [DateTime]::UtcNow -lt $deadline) {
        [Windows.Forms.Application]::DoEvents()
        [Threading.Thread]::Sleep(20)
    }
    [Windows.Forms.Application]::DoEvents()
    Assert-MgsLifecycle (-not $mgsTimer.Enabled) 'The 600 ms debounce did not finish within five seconds.'
}
function Invoke-MgsSaveClick {
    Assert-MgsLifecycle ($mgsSave.Enabled) 'Save was not enabled for a valid, dirty draft.'
    $flags=[Reflection.BindingFlags]::Instance -bor [Reflection.BindingFlags]::NonPublic
    $onClick=$mgsSave.GetType().GetMethod('OnClick',$flags)
    Assert-MgsLifecycle ($null -ne $onClick) 'Could not invoke the hidden Save button click handler.'
    [void]$onClick.Invoke($mgsSave,@([EventArgs]::Empty))
}
try {
    Copy-Item -LiteralPath $mgsFixture -Destination $mgsTestPath
    Open-MgsConfig $mgsTestPath
    Assert-MgsLifecycle (-not $mgsSave.Enabled -and $mgsErrors.Text.StartsWith('VALID')) 'An unchanged valid file should not offer Save.'

    $mgsText.Text += "`r`n; lifecycle valid edit`r`n"
    Assert-MgsLifecycle (-not $mgsSave.Enabled -and $mgsErrors.Text.StartsWith('CHECKING')) 'A valid edit should immediately disable Save and replace stale VALID feedback with CHECKING.'
    Wait-MgsDebounce
    Assert-MgsLifecycle ($mgsSave.Enabled -and $mgsErrors.Text.StartsWith('VALID')) 'The valid edit did not re-enable Save after debounced validation.'
    Invoke-MgsSaveClick
    Assert-MgsLifecycle (-not $script:mgsDirty -and -not $mgsSave.Enabled -and $mgsErrors.Text.StartsWith('SAVED')) 'Saving a valid edit should clear dirty state and disable Save.'
    $mgsSavedText=[IO.File]::ReadAllText($mgsTestPath)
    Assert-MgsLifecycle ($mgsSavedText.Contains('lifecycle valid edit')) 'The valid test edit was not written to its temporary file.'

    $mgsText.Text += "`r`n[gameplay]`r`ninvalid_lifecycle_action=press(a)`r`n"
    Assert-MgsLifecycle (-not $mgsSave.Enabled -and $mgsErrors.Text.StartsWith('CHECKING')) 'An invalid edit should enter the pending state immediately.'
    Wait-MgsDebounce
    Assert-MgsLifecycle (-not $mgsSave.Enabled -and $mgsErrors.ForeColor.ToArgb() -eq [Drawing.Color]::DarkRed.ToArgb()) 'An invalid draft must keep Save disabled and show red feedback.'
    Assert-MgsLifecycle ($mgsErrors.Text.Contains('unknown action: gameplay.invalid_lifecycle_action') -and $mgsErrors.Text.Contains('Correct the reported binding')) 'Invalid feedback must include the checker diagnostic and a recovery instruction.'
    Assert-MgsLifecycle ([IO.File]::ReadAllText($mgsTestPath) -ceq $mgsSavedText) 'Invalid text overwrote the saved temporary file.'

    $mgsText.Text=$mgsSavedText
    Assert-MgsLifecycle (-not $mgsSave.Enabled -and $mgsErrors.Text.StartsWith('CHECKING')) 'Correcting an invalid edit should immediately show pending validation.'
    Wait-MgsDebounce
    Assert-MgsLifecycle ($mgsSave.Enabled -and $mgsErrors.Text.StartsWith('VALID')) 'Corrected valid text did not re-enable Save.'
    Invoke-MgsSaveClick
    Assert-MgsLifecycle (-not $mgsSave.Enabled -and $mgsErrors.Text.StartsWith('SAVED')) 'The corrected draft did not complete the save lifecycle.'
    Assert-MgsLifecycle (@(Get-ChildItem -LiteralPath $mgsTestRoot -Filter '*.bak' -File).Count -eq 2) 'Each successful save should keep an atomic backup in the temporary test directory.'

    $mgsLatestSaved=[IO.File]::ReadAllText($mgsTestPath)
    $mgsText.Text += "`r`n; lifecycle conflict draft`r`n"
    Wait-MgsDebounce
    Assert-MgsLifecycle ($mgsSave.Enabled) 'The valid conflict-test draft should be saveable before an external edit.'
    $mgsExternalText=$mgsLatestSaved+"`r`n; external edit preserved`r`n"
    [IO.File]::WriteAllText($mgsTestPath,$mgsExternalText,[Text.UTF8Encoding]::new($false))
    Invoke-MgsSaveClick
    Assert-MgsLifecycle (-not $mgsSave.Enabled -and $mgsErrors.Text.Contains('changed outside this editor')) 'External edits should block Save with a recovery message.'
    Assert-MgsLifecycle ([IO.File]::ReadAllText($mgsTestPath) -ceq $mgsExternalText) 'The editor overwrote an external edit.'
    Assert-MgsLifecycle (@(Get-ChildItem -LiteralPath $mgsTestRoot -Filter '*.bak' -File).Count -eq 2) 'A conflicted save should not create a backup or replace the external file.'
    Write-Output 'PASS: hidden controls editor valid edit/save, invalid diagnostic, correction/save, debounce states, backups, and external-edit conflict.'
} finally {
    $mgsTimer.Stop()
    $mgsResolvedTemp=[IO.Path]::GetFullPath($mgsTestRoot)
    $mgsResolvedParent=[IO.Path]::GetDirectoryName($mgsResolvedTemp).TrimEnd([IO.Path]::DirectorySeparatorChar,[IO.Path]::AltDirectorySeparatorChar)
    $mgsResolvedLeaf=[IO.Path]::GetFileName($mgsResolvedTemp)
    if ($mgsResolvedParent -eq $mgsTestTempBase -and $mgsResolvedLeaf -match '^mgs5vr-controls-lifecycle-[0-9a-f]{32}$' -and (Test-Path -LiteralPath $mgsResolvedTemp)) {
        Remove-Item -LiteralPath $mgsResolvedTemp -Recurse -Force
    } else {
        throw 'Refusing to recursively remove a controls lifecycle test path outside the expected temp directory.'
    }
}
