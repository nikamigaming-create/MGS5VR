param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

$repo=Split-Path -Parent $PSScriptRoot
$bridge=Join-Path $repo 'tools/launcher-bridge.ps1'
$controlsSource=Join-Path $repo 'config/mgs5vr-controls.ini'
$runtimeSource=Join-Path $repo 'config/mgs5vr.ini'
$checker=Join-Path $repo 'build/Release/mgs5vr_controls.exe'
if (!(Test-Path -LiteralPath $checker -PathType Leaf)) { throw "Required validator missing: $checker" }
$qaRoot=Join-Path $repo 'artifacts/launcher-3d-qa'
[IO.Directory]::CreateDirectory($qaRoot) | Out-Null
$fixture=Join-Path $qaRoot ('settings-bridge-'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($fixture) | Out-Null
$game=Join-Path $fixture 'mgsvtpp.exe'
$controls=Join-Path $fixture 'mgs5vr-controls.ini'
$runtime=Join-Path $fixture 'mgs5vr.ini'
$requestPath=Join-Path $fixture 'request.json'

function Assert([bool]$Condition,[string]$Message) {
    if (!$Condition) { throw "FAIL: $Message" }
    Write-Host "PASS: $Message"
}
function File-Sha([string]$Path) {
    $stream=[IO.File]::OpenRead($Path)
    try { $sha=[Security.Cryptography.SHA256]::Create(); try { return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant() } finally { $sha.Dispose() } }
    finally { $stream.Dispose() }
}
function Invoke-Bridge([string]$Method,[string]$Revision,[hashtable]$Changes) {
    $request=[ordered]@{method=$Method;gameExe=$game}
    if ($Revision) { $request.revision=$Revision }
    if ($null -ne $Changes) { $request.changes=$Changes }
    [IO.File]::WriteAllText($requestPath,($request | ConvertTo-Json -Depth 8 -Compress),[Text.UTF8Encoding]::new($false))
    try {
        $raw=& $bridge -RequestPath $requestPath
        $json=([string[]]$raw) -join "`n"
        return [pscustomobject]@{ok=$true;json=$json;state=($json | ConvertFrom-Json);error=''}
    } catch {
        return [pscustomobject]@{ok=$false;json='';state=$null;error=$_.Exception.Message}
    }
}

try {
    # The bridge selects only by executable name/path; this empty file is a
    # disposable fixture and is never launched.
    [IO.File]::WriteAllBytes($game,[byte[]]@(0x4d,0x5a,0x00,0x00))
    Copy-Item -LiteralPath $controlsSource -Destination $controls
    Copy-Item -LiteralPath $runtimeSource -Destination $runtime

    $loaded=Invoke-Bridge 'load' '' $null
    Assert $loaded.ok 'load accepts a disposable MGSV path and returns a settings snapshot'
    Assert ($loaded.state.gameExe -eq $game -and $loaded.state.writable -eq $true) 'load binds the snapshot to the fixture and marks its copied controls writable'
    Assert (@($loaded.state.bindings.actions).Count -gt 0 -and @($loaded.state.runtime.PSObject.Properties).Count -gt 0) 'load returns parsed bindings and runtime settings'
    Assert ($loaded.state.runtime.'ui.map_control_prompts' -eq '1') 'reviewed Map control labels default on in the launcher runtime snapshot'
    Assert ($loaded.state.runtime.'optics.binocular_native_material' -eq '0') 'unproven binocular scene lighting defaults off in the launcher runtime snapshot'
    $settingRows=@($loaded.state.settings)
    Assert ($loaded.json -match '"settings"\s*:\s*\[' -and $loaded.state.settings -is [Array] -and $settingRows.Count -eq 55) 'serialized settings field is a flat array containing all 55 setting rows'
    $rowsValid=$true
    if ($loaded.state.settings -isnot [Array] -or $settingRows.Count -ne 55) { $rowsValid=$false }
    else {
        foreach ($row in $settingRows) {
            $numbers=@($row.value,$row.min,$row.max)
            if ($row.name -notmatch '^settings\.[a-z0-9_]+$' -or $numbers.Count -ne 3 -or
                @($numbers | Where-Object { $_ -isnot [ValueType] -or $_ -is [bool] }).Count -ne 0 -or
                [double]$row.min -gt [double]$row.max) { $rowsValid=$false; break }
        }
    }
    Assert $rowsValid 'each setting row has a named setting and numeric value/min/max bounds'
    Assert (@($settingRows | Where-Object { $_.name -eq 'settings.wrist_text_scale' -and $_.value -eq 1.5 }).Count -eq 1) 'arm text defaults to the requested larger size'
    [IO.File]::WriteAllText((Join-Path $fixture 'mgs5vr-display.ini'),"[display]`nenabled=1`nrender_width=2960`nrender_height=3220`n")
    [IO.File]::WriteAllText((Join-Path $fixture 'mgs5vr-render-size.txt'),"0 100 1920 1080 960 540 1`n")
    $measured=Invoke-Bridge 'load' '' $null
    Assert ($measured.ok -and $measured.state.display.requestedWidth -eq 2960 -and $measured.state.display.actualWidth -eq 1920 -and !$measured.state.display.live) 'requested high resolution cannot be reported as the measured game buffer or as a live session'

    $originalControls=[IO.File]::ReadAllText($controls)
    $saved=Invoke-Bridge 'saveControls' $loaded.state.controlsRevision @{ 'settings.handheld_menus'='1' }
    Assert $saved.ok 'valid control setting saves through the validator'
    Assert ([IO.File]::ReadAllText($controls) -match '(?m)^handheld_menus\s*=\s*1\s*$') 'valid setting is written to the fixture INI'
    Assert ($saved.state.backup -and (Test-Path -LiteralPath $saved.state.backup -PathType Leaf)) 'successful write returns a backup path that exists'
    Assert ([IO.File]::ReadAllText($saved.state.backup) -ceq $originalControls) 'backup preserves the exact original controls bytes/text'

    $fractional=Invoke-Bridge 'saveControls' $saved.state.controlsRevision @{ 'settings.right_hand_x_cm'='2.75' }
    Assert $fractional.ok 'fractional control setting saves through the validator'
    Assert ([IO.File]::ReadAllText($controls) -match '(?m)^right_hand_x_cm\s*=\s*2\.75\s*$') 'fractional setting is preserved exactly in the INI'
    $fractionalRow=@($fractional.state.settings | Where-Object name -eq 'settings.right_hand_x_cm')
    Assert ($fractionalRow.Count -eq 1 -and [double]$fractionalRow[0].value -eq 2.75) 'fractional value returns as a numeric setting row'

    $beforeConflict=File-Sha $controls
    $conflict=Invoke-Bridge 'saveControls' $fractional.state.controlsRevision @{
        'system.idroid'='press(a)'; 'system.pause'='press(a)'
    }
    Assert (!$conflict.ok) 'conflicting same-context bindings are rejected'
    Assert ((File-Sha $controls) -ceq $beforeConflict) 'rejected binding edit leaves the live fixture file byte-identical'

    # Simulate a different editor changing the file after the launcher load.
    $staleRevision=$fractional.state.controlsRevision
    $externallyEdited=[IO.File]::ReadAllText($controls).Replace('idroid = tap(menu,550)','idroid = tap(menu,600)').Replace('pause = hold(menu,550)','pause = hold(menu,600)')
    [IO.File]::WriteAllText($controls,$externallyEdited,[Text.UTF8Encoding]::new($false))
    $beforeStale=File-Sha $controls
    $stale=Invoke-Bridge 'saveControls' $staleRevision @{ 'settings.handheld_menus'='0' }
    Assert (!$stale.ok -and $stale.error -match 'changed outside this launcher') 'stale revision is rejected with a reload message'
    Assert ((File-Sha $controls) -ceq $beforeStale) 'stale revision rejection preserves the external edit'

    $runtimeBefore=File-Sha $runtime
    $runtimeRejected=Invoke-Bridge 'saveRuntime' $loaded.state.runtimeRevision @{ 'theatre.width_cm'='99' }
    Assert (!$runtimeRejected.ok -and $runtimeRejected.error -match 'Invalid theatre.width_cm') 'out-of-range runtime setting is rejected'
    Assert ((File-Sha $runtime) -ceq $runtimeBefore) 'runtime validation failure leaves the fixture INI byte-identical'

    $runtimeOriginal=[IO.File]::ReadAllText($runtime)
    $controlsBeforeMap=File-Sha $controls
    $mapOff=Invoke-Bridge 'saveRuntime' $loaded.state.runtimeRevision @{ 'ui.map_control_prompts'='0' }
    Assert $mapOff.ok ('Map-only opt-out saves through the launcher: '+$mapOff.error)
    Assert ($mapOff.state.runtime.'ui.map_control_prompts' -eq '0') 'Map-only opt-out returns the effective runtime value'
    Assert ([IO.File]::ReadAllText($mapOff.state.backup) -ceq $runtimeOriginal) 'Map opt-out backup retains the exact original runtime settings'
    $expectedRuntime=(($runtimeOriginal -split '\r?\n') -join "`r`n").Replace('map_control_prompts=1','map_control_prompts = 0')
    Assert ([IO.File]::ReadAllText($runtime) -ceq $expectedRuntime) 'Map opt-out preserves all other runtime values and comments under the existing CRLF writer'
    Assert ((File-Sha $controls) -ceq $controlsBeforeMap) 'Map opt-out preserves personal controls byte-for-byte'
    $beforeInvalidMap=File-Sha $runtime
    $invalidMap=Invoke-Bridge 'saveRuntime' $mapOff.state.runtimeRevision @{ 'ui.map_control_prompts'='2' }
    Assert (!$invalidMap.ok -and $invalidMap.error -match 'Invalid ui.map_control_prompts') 'Map labels reject values outside the on/off range'
    Assert ((File-Sha $runtime) -ceq $beforeInvalidMap) 'invalid Map label value cannot overwrite saved runtime settings'
    $mapOn=Invoke-Bridge 'saveRuntime' $mapOff.state.runtimeRevision @{ 'ui.map_control_prompts'='1' }
    Assert ($mapOn.ok -and $mapOn.state.runtime.'ui.map_control_prompts' -eq '1') 'Map control labels can be restored independently'

    # Older installations have no UI block; their first launcher load merges
    # the new default without writing or replacing the player's runtime file.
    $legacyRuntime=[regex]::Replace([IO.File]::ReadAllText($runtime),'(?ms)^\[ui\]\r?\n.*?(?=^\[diagnostics\])','')
    [IO.File]::WriteAllText($runtime,$legacyRuntime,[Text.UTF8Encoding]::new($false))
    $legacyHash=File-Sha $runtime
    $legacyLoaded=Invoke-Bridge 'load' '' $null
    Assert ($legacyLoaded.ok -and $legacyLoaded.state.runtime.'ui.map_control_prompts' -eq '1') 'an older INI without UI settings receives the default Map label value'
    Assert ((File-Sha $runtime) -ceq $legacyHash) 'merging the default on load does not rewrite the older runtime file'
    $legacyOff=Invoke-Bridge 'saveRuntime' $legacyLoaded.state.runtimeRevision @{ 'ui.map_control_prompts'='0' }
    Assert ($legacyOff.ok -and $legacyOff.state.runtime.'ui.map_control_prompts' -eq '0') 'the Map opt-out can be inserted into an older runtime INI'
    Assert ([IO.File]::ReadAllText($legacyOff.state.backup) -ceq $legacyRuntime -and (File-Sha $controls) -ceq $controlsBeforeMap) 'older-INI opt-out retains an exact backup and leaves personal bindings unchanged'

    $beforeMaterial=[IO.File]::ReadAllText($runtime)
    $materialOn=Invoke-Bridge 'saveRuntime' $legacyOff.state.runtimeRevision @{ 'optics.binocular_native_material'='1' }
    Assert ($materialOn.ok -and $materialOn.state.runtime.'optics.binocular_native_material' -eq '1') 'experimental binocular scene lighting opt-in saves through the launcher'
    Assert ([IO.File]::ReadAllText($materialOn.state.backup) -ceq $beforeMaterial -and (File-Sha $controls) -ceq $controlsBeforeMap) 'binocular opt-in retains an exact runtime backup and personal bindings'
    $originalValues=Get-MgsIni $beforeMaterial
    $materialValues=Get-MgsIni ([IO.File]::ReadAllText($runtime))
    $unchanged=$originalValues.Count -eq $materialValues.Count
    foreach ($key in $originalValues.Keys) {
        if ($key -ne 'optics.binocular_native_material' -and $originalValues[$key] -cne $materialValues[$key]) { $unchanged=$false }
    }
    Assert $unchanged 'binocular opt-in leaves every other runtime and asset value unchanged'
    $materialHash=File-Sha $runtime
    foreach ($invalidValue in @('2','-1','0.5','true')) {
        $invalidMaterial=Invoke-Bridge 'saveRuntime' $materialOn.state.runtimeRevision @{ 'optics.binocular_native_material'=$invalidValue }
        Assert (!$invalidMaterial.ok -and $invalidMaterial.error -match 'Invalid optics.binocular_native_material' -and (File-Sha $runtime) -ceq $materialHash) ('binocular scene lighting rejects '+$invalidValue+' without writing')
    }
    $materialOff=Invoke-Bridge 'saveRuntime' $materialOn.state.runtimeRevision @{ 'optics.binocular_native_material'='0' }
    Assert ($materialOff.ok -and $materialOff.state.runtime.'optics.binocular_native_material' -eq '0') 'experimental binocular scene lighting can be disabled independently'
    $olderMaterial=[regex]::Replace([IO.File]::ReadAllText($runtime),'(?m)^binocular_native_material[ \t]*=[^\r\n]*(?:\r?\n|$)','')
    [IO.File]::WriteAllText($runtime,$olderMaterial,[Text.UTF8Encoding]::new($false))
    $olderHash=File-Sha $runtime
    $olderLoaded=Invoke-Bridge 'load' '' $null
    Assert ($olderLoaded.ok -and $olderLoaded.state.runtime.'optics.binocular_native_material' -eq '0' -and (File-Sha $runtime) -ceq $olderHash) 'an older INI receives the safe binocular default off on read without being rewritten'
    $olderOn=Invoke-Bridge 'saveRuntime' $olderLoaded.state.runtimeRevision @{ 'optics.binocular_native_material'='1' }
    Assert ($olderOn.ok -and $olderOn.state.runtime.'optics.binocular_native_material' -eq '1' -and [IO.File]::ReadAllText($olderOn.state.backup) -ceq $olderMaterial) 'explicit binocular opt-in inserts its key into an older INI with an exact backup'

    Write-Host 'All launcher settings bridge tests passed.'
} finally {
    # Guard recursive cleanup to this test's unique child below the QA root.
    $resolvedRoot=[IO.Path]::GetFullPath($qaRoot).TrimEnd([IO.Path]::DirectorySeparatorChar)+[IO.Path]::DirectorySeparatorChar
    $resolvedFixture=[IO.Path]::GetFullPath($fixture)
    if ($resolvedFixture.StartsWith($resolvedRoot,[StringComparison]::OrdinalIgnoreCase) -and (Test-Path -LiteralPath $resolvedFixture)) {
        Remove-Item -LiteralPath $resolvedFixture -Recurse -Force
    }
}
