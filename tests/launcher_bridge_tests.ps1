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
    $settingRows=@($loaded.state.settings)
    Assert ($loaded.json -match '"settings"\s*:\s*\[' -and $loaded.state.settings -is [Array] -and $settingRows.Count -eq 54) 'serialized settings field is a flat array containing all 54 setting rows'
    $rowsValid=$true
    if ($loaded.state.settings -isnot [Array] -or $settingRows.Count -ne 54) { $rowsValid=$false }
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
    $externallyEdited=[IO.File]::ReadAllText($controls).Replace('idroid = tap(menu,550)','idroid = tap(menu,600)')
    [IO.File]::WriteAllText($controls,$externallyEdited,[Text.UTF8Encoding]::new($false))
    $beforeStale=File-Sha $controls
    $stale=Invoke-Bridge 'saveControls' $staleRevision @{ 'settings.handheld_menus'='0' }
    Assert (!$stale.ok -and $stale.error -match 'changed outside this launcher') 'stale revision is rejected with a reload message'
    Assert ((File-Sha $controls) -ceq $beforeStale) 'stale revision rejection preserves the external edit'

    $runtimeBefore=File-Sha $runtime
    $runtimeRejected=Invoke-Bridge 'saveRuntime' $loaded.state.runtimeRevision @{ 'theatre.width_cm'='99' }
    Assert (!$runtimeRejected.ok -and $runtimeRejected.error -match 'Invalid theatre.width_cm') 'out-of-range runtime setting is rejected'
    Assert ((File-Sha $runtime) -ceq $runtimeBefore) 'runtime validation failure leaves the fixture INI byte-identical'

    Write-Host 'All launcher settings bridge tests passed.'
} finally {
    # Guard recursive cleanup to this test's unique child below the QA root.
    $resolvedRoot=[IO.Path]::GetFullPath($qaRoot).TrimEnd([IO.Path]::DirectorySeparatorChar)+[IO.Path]::DirectorySeparatorChar
    $resolvedFixture=[IO.Path]::GetFullPath($fixture)
    if ($resolvedFixture.StartsWith($resolvedRoot,[StringComparison]::OrdinalIgnoreCase) -and (Test-Path -LiteralPath $resolvedFixture)) {
        Remove-Item -LiteralPath $resolvedFixture -Recurse -Force
    }
}
