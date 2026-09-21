$ErrorActionPreference='Stop'
$mgsRoot=Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $mgsRoot 'tools\settings-store.psm1') -Force
$mgsChecker=Join-Path $mgsRoot 'build\Release\mgs5vr_controls.exe'
$mgsLauncher=Join-Path $mgsRoot 'build\Release\MGS5VR-Launcher.exe'
$mgsFixture=Join-Path ([IO.Path]::GetTempPath()) ('mgs5vr-settings-'+[guid]::NewGuid().ToString('N'))
function Assert([bool]$value,[string]$reason) { if (!$value) { throw $reason } }
function Refuses([scriptblock]$action) { $bad=$false;try { & $action } catch { $bad=$true };Assert $bad 'Invalid settings were accepted.' }
try {
    [void](New-Item -ItemType Directory -Path $mgsFixture)
    $mgsPath=Join-Path $mgsFixture 'mgs5vr-controls.ini'
    $mgsExe=Join-Path $mgsFixture 'mgsvtpp.exe'
    [IO.File]::WriteAllText($mgsExe,'Not executable: settings fixture.')
    $original="; preserve user bindings`r`n[system]`r`ntoggle_vr=disabled`r`n[settings]`r`nturn_mode=off`r`nsupport_grip_radius_cm=10`r`n; keep my comment`r`n"
    [IO.File]::WriteAllText($mgsPath,$original)
    $updated=Set-MgsIniValue $original 'settings.support_detach_radius_cm' '42'
    $updated=Set-MgsIniValue $updated 'settings.hand_rest_curl_percent' '10'
    $backup=Save-MgsSettings $mgsPath $original $updated $mgsChecker
    Assert ([IO.File]::ReadAllText($backup) -ceq $original) 'Backup is not exact.'
    $values=Get-MgsIni ([IO.File]::ReadAllText($mgsPath))
    Assert ($values['system.toggle_vr'] -eq 'disabled' -and $values['settings.turn_mode'] -eq 'off' -and $values['settings.support_detach_radius_cm'] -eq '42') 'Saving settings changed unrelated bindings.'
    Assert ($updated.Contains('; keep my comment')) 'Saving removed user comments.'
    $invalid=Set-MgsIniValue $updated 'settings.support_detach_radius_cm' '5'
    Refuses { Save-MgsSettings $mgsPath $updated $invalid $mgsChecker }
    Assert ([IO.File]::ReadAllText($mgsPath) -ceq $updated) 'Invalid save overwrote a working file.'
    Refuses { Save-MgsSettings $mgsPath $original $updated $mgsChecker }
    $all=& $mgsChecker --settings-json $mgsPath | ConvertFrom-Json
    Assert ($LASTEXITCODE -eq 0 -and $all.Count -ge 35) 'Missing editable settings.'
    Assert (($all | Where-Object name -eq 'settings.support_detach_radius_cm').value -eq 42) 'Settings listing ignores saved values.'
    $headless=& $mgsLauncher --headless -Action Settings -GameExe $mgsExe | Out-String
    Assert ($LASTEXITCODE -eq 0) 'Headless launcher failed.'
    $rows=$headless | ConvertFrom-Json
    Assert (($rows | Where-Object name -eq 'settings.turn_mode').value -eq 'off') 'Headless launcher settings output incorrect.'
    $setOutput=& $mgsLauncher --headless -Action Set -GameExe $mgsExe -Setting 'settings.idroid_screen_width_cm=40' | Out-String
    Assert ($LASTEXITCODE -eq 0) 'Headless launcher set failed.'
    Assert ((Get-MgsIni ([IO.File]::ReadAllText($mgsPath)))['settings.idroid_screen_width_cm'] -eq '40') 'Headless Set did not persist.'
    $setOutput=& $mgsLauncher --headless -Action Set -GameExe $mgsExe -Setting 'settings.turn_mode=snap' 'settings.snap_turn_degrees=45' | Out-String
    Assert ($LASTEXITCODE -eq 0) "Headless snap turn configuration failed: $setOutput"
    $turnSettings=Get-MgsIni ([IO.File]::ReadAllText($mgsPath))
    Assert ($turnSettings['settings.turn_mode'] -eq 'snap' -and $turnSettings['settings.snap_turn_degrees'] -eq '45') 'Snap mode or angle did not persist.'
    $savedTurn=[IO.File]::ReadAllText($mgsPath)
    Refuses { Save-MgsSettings $mgsPath $savedTurn (Set-MgsIniValue $savedTurn 'settings.snap_turn_degrees' '91') $mgsChecker }
    Assert ([IO.File]::ReadAllText($mgsPath) -ceq $savedTurn) 'Invalid snap angle replaced the working setting.'
    Refuses { Set-MgsIniValue $updated 'settings.foo' "bad`n[settings]" }
    Refuses { Get-MgsIni "[settings]`na=1`na=2" }
    $runtime=Join-Path $mgsFixture 'mgs5vr.ini';$runtimeText="[theatre]`nenabled=1`n[opening]`ninteractive_cabin=0`ndog_fmdl=C:\Owned game\dog.fmdl`n"
    [IO.File]::WriteAllText($runtime,$runtimeText)
    $runtimeUpdated=Set-MgsIniValue $runtimeText 'opening.interactive_cabin' '1'
    [void](Save-MgsSettings $runtime $runtimeText $runtimeUpdated $mgsChecker -Runtime)
    Refuses { Save-MgsSettings $runtime $runtimeUpdated (Set-MgsIniValue $runtimeUpdated 'theatre.enabled' '2') $mgsChecker -Runtime }
    Assert ((Get-MgsIni ([IO.File]::ReadAllText($runtime)))['opening.dog_fmdl'] -eq 'C:\Owned game\dog.fmdl') 'Runtime save changed an asset path.'
    Write-Output 'Settings editor/headless launcher: schema, persistence, backup, invalid/stale edits and native configuration passed.'
} finally {
    $resolved=[IO.Path]::GetFullPath($mgsFixture);$temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolved.StartsWith($temp,[StringComparison]::OrdinalIgnoreCase) -and [IO.Path]::GetFileName($resolved).StartsWith('mgs5vr-settings-')) {
        if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
    }
}
