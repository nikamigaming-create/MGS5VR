$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repo=Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $repo 'tools/simulator-session-ownership.psm1') -Force
function Assert([bool]$condition,[string]$message){if(!$condition){throw "FAIL: $message"};Write-Host "PASS: $message"}
function Proc([int]$procId,[int]$parent,[string]$path,[string]$time,[string]$name){
    [pscustomobject]@{ProcessId=$procId;ParentProcessId=$parent;ExecutablePath=$path;CreationDate=[DateTime]::Parse($time).ToUniversalTime();Name=$name}
}
$fractionalStartText='2026-09-27T19:00:01.1234567Z'
$fractionalExpected=[DateTime]::Parse($fractionalStartText,[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind).ToUniversalTime()
$fractionalStartDate=[DateTime]::Parse($fractionalStartText,[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind)
$fractionalStartJson=('{"startedUtc":"'+$fractionalStartText+'"}' | ConvertFrom-Json).startedUtc
Assert ((ConvertTo-MgsUtcStartTime $fractionalStartText).Ticks -eq $fractionalExpected.Ticks) 'fractional process start survives serialized ISO string normalization'
Assert ((ConvertTo-MgsUtcStartTime $fractionalStartDate).Ticks -eq $fractionalExpected.Ticks) 'fractional process start survives DateTime normalization without string conversion'
Assert ((ConvertTo-MgsUtcStartTime $fractionalStartJson).Ticks -eq $fractionalExpected.Ticks) 'fractional process start survives ConvertFrom-Json DateTime input'
$fractionalIdentityText=[pscustomobject]@{pid=77;parentPid=7;startedUtc=$fractionalStartText;path='C:\Tools\runtime.exe'}
$fractionalIdentityDate=[pscustomobject]@{pid=77;parentPid=7;startedUtc=$fractionalStartDate;path='C:\Tools\runtime.exe'}
Assert (Test-MgsExactProcessIdentity $fractionalIdentityText $fractionalIdentityDate) 'exact identity comparison handles fractional string and DateTime timestamps'
$root='C:\Program Files\MetaXRSimulator\v207.0'
$gameProc=Proc 100 10 'D:\Games\MGSV\mgsvtpp.exe' '2026-09-27T19:00:00Z' 'mgsvtpp.exe'
$guiProc=Proc 200 100 "$root\MetaXRSimulator.exe" '2026-09-27T19:00:01Z' 'MetaXRSimulator.exe'
$syntheticProc=Proc 201 200 "$root\MetaXRBedroom.exe" '2026-09-27T19:00:02Z' 'MetaXRBedroom.exe'
$game=ConvertTo-MgsProcessIdentity $gameProc
$owned=Get-MgsOwnedRuntimeProcesses -Processes @($gameProc,$guiProc,$syntheticProc) -Game $game -RuntimeRoot $root
$record=[pscustomobject]@{schema=2;game=$game;ownedRuntimeProcesses=@($owned)}
$parsedGame=$game|ConvertTo-Json|ConvertFrom-Json
$parsedOwned=@(Get-MgsOwnedRuntimeProcesses -Processes @($gameProc,$guiProc,$syntheticProc) -Game $parsedGame -RuntimeRoot $root)
Assert (Test-MgsExactProcessIdentity (@($parsedOwned|Where-Object pid -eq 200)[0]).parentIdentity $game) 'JSON DateTime game owner keeps UTC and fractions in descendant parent identities'
$lateSharing=Proc 202 200 "$root\ls_server\local_sharing_server.exe" '2026-09-27T19:00:04Z' 'local_sharing_server.exe'
$expanded=Expand-MgsLiveRuntimeRecord -Record $record -Processes @($gameProc,$guiProc,$syntheticProc,$lateSharing) -RuntimeRoot $root
Assert ($expanded.ownedRuntimeProcesses.Count -eq 3 -and $record.ownedRuntimeProcesses.Count -eq 2) 'late exact runtime child extends a new snapshot without changing the recorded game generation'
Assert ((@($expanded.ownedRuntimeProcesses|Where-Object pid -eq 202)[0]).parentIdentity.pid -eq 200) 'late sharing server retains exact GUI ancestry'
$olderSharing=Proc 203 200 "$root\ls_server\local_sharing_server.exe" '2026-09-27T18:59:59Z' 'local_sharing_server.exe'
$olderRefused=$false
try{Expand-MgsLiveRuntimeRecord -Record $record -Processes @($gameProc,$guiProc,$syntheticProc,$olderSharing) -RuntimeRoot $root|Out-Null}catch{$olderRefused=$_.Exception.Message -match 'predates'}
Assert $olderRefused 'a stale child from a reused parent PID cannot become trusted'
$replacedGui=Proc 200 100 "$root\MetaXRSimulator.exe" '2026-09-27T20:00:01Z' 'MetaXRSimulator.exe'
$lateReuseRefused=$false
try{Expand-MgsLiveRuntimeRecord -Record $record -Processes @($gameProc,$replacedGui,$lateSharing) -RuntimeRoot $root|Out-Null}catch{$lateReuseRefused=$_.Exception.Message -match 'generation changed'}
Assert $lateReuseRefused 'late registration cannot adopt a changed recorded GUI generation'
$ownClientConnections=@(
    [pscustomobject]@{LocalAddress='127.0.0.1';LocalPort=5100;RemoteAddress='127.0.0.1';RemotePort=5101;State='Established';OwningProcess=200},
    [pscustomobject]@{LocalAddress='127.0.0.1';LocalPort=5101;RemoteAddress='127.0.0.1';RemotePort=5100;State='Established';OwningProcess=201}
)

Assert (@($owned).Count -eq 2 -and @($owned|Where-Object role -eq 'meta_gui').Count -eq 1 -and @($owned|Where-Object role -eq 'runtime_child').Count -eq 1) 'capture includes the directly spawned Meta GUI and its runtime child with ancestry identities'
$recordJson=$record|ConvertTo-Json -Depth 8
$persistedRecord=$recordJson|ConvertFrom-Json
$persistedPlan=@(Get-MgsOrphanCleanupPlan -Record $persistedRecord -Processes @($guiProc,$syntheticProc) -TcpConnections $ownClientConnections -TcpSnapshotComplete $true)
Assert ($persistedPlan.Count -eq 2 -and [int]$persistedPlan[0].pid -eq 201) 'serialized session record preserves exact ownership and child-first cleanup order'
$cleanupPlan=@(Get-MgsOrphanCleanupPlan -Record $record -Processes @($guiProc,$syntheticProc) -TcpConnections $ownClientConnections -TcpSnapshotComplete $true)
Assert ($cleanupPlan.Count -eq 2) 'only exact recorded runtime descendants are selected after the recorded game exits'
$selfConnections=@(
    [pscustomobject]@{LocalAddress='127.0.0.1';LocalPort=5200;RemoteAddress='127.0.0.1';RemotePort=5201;State='Established';OwningProcess=200},
    [pscustomobject]@{LocalAddress='127.0.0.1';LocalPort=5201;RemoteAddress='127.0.0.1';RemotePort=5200;State='Established';OwningProcess=200}
)
$selfPlan=@(Get-MgsOrphanCleanupPlan -Record $record -Processes @($guiProc,$syntheticProc) -TcpConnections @($ownClientConnections+$selfConnections) -TcpSnapshotComplete $true)
Assert ($selfPlan.Count -eq 2) 'the simulator internal loopback sockets do not masquerade as unknown clients'
$idleTcpPlan=@(Get-MgsOrphanCleanupPlan -Record $record -Processes @($guiProc,$syntheticProc) -TcpConnections @() -TcpSnapshotComplete $true)
Assert ($idleTcpPlan.Count -eq 2) 'a successful complete snapshot with no active TCP peers permits exact owned cleanup'
Assert ([int]$cleanupPlan[0].pid -eq 201 -and [int]$cleanupPlan[1].pid -eq 200) 'owned children are retired before their simulator parent'
Assert ((Test-MgsExactProcessIdentity ($owned|Where-Object pid -eq 200) $guiProc) -eq $true) 'exact PID, start, path, and parent identity matches'
Assert ((($owned|Where-Object pid -eq 200).parentIdentity.pid -eq $game.pid) -and (($owned|Where-Object pid -eq 201).parentIdentity.pid -eq 200)) 'recorded parent identities bind the GUI to the game and child to the GUI'

$tampered=($record|ConvertTo-Json -Depth 8|ConvertFrom-Json)
($tampered.ownedRuntimeProcesses|Where-Object pid -eq 201).parentIdentity.path='C:\Other\wrong-parent.exe'
$tamperRejected=$false
try { Get-MgsOrphanCleanupPlan -Record $tampered -Processes @($guiProc,$syntheticProc) | Out-Null } catch { $tamperRejected=$_.Exception.Message -match 'no exact recorded owner' }
Assert $tamperRejected 'a recorded parent identity mismatch is rejected'

$reused=Proc 200 100 "$root\MetaXRSimulator.exe" '2026-09-27T20:00:01Z' 'MetaXRSimulator.exe'
$reusedRejected=$false
try { Get-MgsOrphanCleanupPlan -Record $record -Processes @($reused,$syntheticProc) | Out-Null } catch { $reusedRejected=$_.Exception.Message -match 'refusing cleanup' }
Assert $reusedRejected 'reused PID with a different start time is refused'

$wrongParent=Proc 200 999 "$root\MetaXRSimulator.exe" '2026-09-27T19:00:01Z' 'MetaXRSimulator.exe'
$parentRejected=$false
try { Get-MgsOrphanCleanupPlan -Record $record -Processes @($wrongParent) | Out-Null } catch { $parentRejected=$_.Exception.Message -match 'refusing cleanup' }
Assert $parentRejected 'parent mismatch is refused even when PID, path, and start match'

$otherPath=Proc 200 100 'C:\Other\MetaXRSimulator.exe' '2026-09-27T19:00:01Z' 'MetaXRSimulator.exe'
$pathRejected=$false
try { Get-MgsOrphanCleanupPlan -Record $record -Processes @($otherPath) | Out-Null } catch { $pathRejected=$_.Exception.Message -match 'refusing cleanup' }
Assert $pathRejected 'path mismatch is refused'

$liveRejected=$false
try { Get-MgsOrphanCleanupPlan -Record $record -Processes @($gameProc,$guiProc,$syntheticProc) | Out-Null } catch { $liveRejected=$_.Exception.Message -match 'still running' }
Assert $liveRejected 'cleanup refuses while the exact recorded game is still alive'

$unknown=Proc 900 50 "$root\MetaXRSimulator.exe" '2026-09-27T19:01:00Z' 'MetaXRSimulator.exe'
$unknownRejected=$false
try { Get-MgsOrphanCleanupPlan -Record $record -Processes @($guiProc,$syntheticProc,$unknown) | Out-Null } catch { $unknownRejected=$_.Exception.Message -match 'not in the exited game' }
Assert $unknownRejected 'an unrecorded simulator instance blocks reuse'

$otherApp=Proc 900 50 'C:\Other\OpenNV.exe' '2026-09-27T19:01:00Z' 'OpenNV.exe'
$sharedConnections=@(
    [pscustomobject]@{LocalAddress='127.0.0.1';LocalPort=6100;RemoteAddress='127.0.0.1';RemotePort=6101;State='Established';OwningProcess=200},
    [pscustomobject]@{LocalAddress='127.0.0.1';LocalPort=6101;RemoteAddress='127.0.0.1';RemotePort=6100;State='Established';OwningProcess=900}
)
$sharedRejected=$false
try { Get-MgsOrphanCleanupPlan -Record $record -Processes @($guiProc,$syntheticProc,$otherApp) -TcpConnections @($sharedConnections+$selfConnections) -TcpSnapshotComplete $true | Out-Null } catch { $sharedRejected=$_.Exception.Message -match 'outside exact live recorded identities' }
Assert $sharedRejected 'an active loopback connection from another application blocks simulator cleanup'

$reusedClient=Proc 201 200 "$root\MetaXRBedroom.exe" '2026-09-27T20:01:00Z' 'MetaXRBedroom.exe'
$reusedClientRejected=$false
try { Get-MgsOrphanCleanupPlan -Record $record -Processes @($guiProc,$reusedClient) -TcpConnections $ownClientConnections -TcpSnapshotComplete $true | Out-Null } catch { $reusedClientRejected=$_.Exception.Message -match 'no longer has the recorded start/path/parent identity' }
Assert $reusedClientRejected 'a recorded client PID reused by a different process identity is refused'

$incompleteRejected=$false
try { Get-MgsOrphanCleanupPlan -Record $record -Processes @($guiProc,$syntheticProc) -TcpConnections $ownClientConnections -TcpSnapshotComplete $false | Out-Null } catch { $incompleteRejected=$_.Exception.Message -match 'ownership is unproven' }
Assert $incompleteRejected 'failed TCP inspection leaves shared simulator ownership unproven and blocks cleanup'

$missingPeerRejected=$false
try { Get-MgsOrphanCleanupPlan -Record $record -Processes @($guiProc,$syntheticProc) -TcpConnections @($ownClientConnections[0]) -TcpSnapshotComplete $true | Out-Null } catch { $missingPeerRejected=$_.Exception.Message -match 'exactly one readable peer PID' }
Assert $missingPeerRejected 'a loopback connection without a uniquely identified peer blocks cleanup'

$legacyRejected=$false
try { Get-MgsOrphanCleanupPlan -Record ([pscustomobject]@{schema=1}) -Processes @($unknown) | Out-Null } catch { $legacyRejected=$_.Exception.Message -match 'unrecorded MetaXRSimulator' }
Assert $legacyRejected 'legacy or missing ownership records never authorize stopping an existing simulator'

$nullRejected=$false
try { Get-MgsOrphanCleanupPlan -Record $null -Processes @($unknown) | Out-Null } catch { $nullRejected=$_.Exception.Message -match 'unrecorded MetaXRSimulator' }
Assert $nullRejected 'an explicitly null ownership record fails closed for an unknown simulator'

$mgsLeaseFixture=Join-Path ([IO.Path]::GetTempPath()) ('mgs5vr-runtime-lease-'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($mgsLeaseFixture) | Out-Null
$mgsLeaseExe=Join-Path $mgsLeaseFixture 'mgsvtpp.exe'
$mgsLeasePath=Join-Path $mgsLeaseFixture 'mgs5vr-runtime.ini'
try {
    $lease=New-MgsRuntimeConfigLease -GameExe $mgsLeaseExe -RuntimeManifest 'C:\Simulator\runtime.json' -OperatorDir 'C:\Operator'
    Assert ((Get-Content -Raw -LiteralPath $mgsLeasePath) -match 'headless=0') 'the test runtime lease keeps the real compositor enabled'
    Assert ((Get-Content -Raw -LiteralPath $mgsLeasePath) -match 'producer_pacing_test=baseline') 'default launches retain the existing producer margin'
    Assert ((Get-Content -Raw -LiteralPath $mgsLeasePath) -match 'idroid_ui_boundary_trace=0') 'UI boundary tracing is off for default launches'
    Assert (!(Test-Path (Join-Path $mgsLeaseFixture 'mgs5vr.ini')) -and !(Test-Path (Join-Path $mgsLeaseFixture 'mgs5vr-controls.ini'))) 'pacing configuration never creates or edits personal graphics or control INIs'
    $roundTrip=$lease|ConvertTo-Json|ConvertFrom-Json
    Restore-MgsRuntimeConfigLease -Lease $roundTrip -GameExe $mgsLeaseExe
    Restore-MgsRuntimeConfigLease -Lease $roundTrip -GameExe $mgsLeaseExe
    Assert (!(Test-Path -LiteralPath $mgsLeasePath)) 'an originally absent runtime file is removed after JSON-record recovery and repeated cleanup'

    $lease=New-MgsRuntimeConfigLease -GameExe $mgsLeaseExe -RuntimeManifest 'C:\Simulator\runtime.json' -OperatorDir 'C:\Operator' -ProducerPacingTest display -ProducerPacingTestSeconds 600 -IdroidUiBoundaryTrace -CameraEvidenceDir $mgsLeaseFixture
    $pacingText=Get-Content -Raw -LiteralPath $mgsLeasePath
    Assert ($pacingText -match 'producer_pacing_test=display' -and $pacingText -match 'producer_pacing_test_seconds=600') 'display-period pacing is explicit in the isolated runtime lease'
    Assert ($pacingText -match 'idroid_ui_boundary_trace=1') 'UI boundary tracing requires explicit ownership of the runtime lease'
    Assert ($pacingText.Contains('camera_evidence_dir='+$mgsLeaseFixture)) 'owned evidence output is selected without editing personal INIs'
    Restore-MgsRuntimeConfigLease -Lease ($lease|ConvertTo-Json|ConvertFrom-Json) -GameExe $mgsLeaseExe
    Assert (!(Test-Path -LiteralPath $mgsLeasePath)) 'pacing diagnostic cleanup removes its entire temporary runtime lease'
    foreach($bad in @(29,1201)){
        $badPacingRefused=$false
        try { New-MgsRuntimeConfigLease -GameExe $mgsLeaseExe -RuntimeManifest 'C:\Simulator\runtime.json' -OperatorDir 'C:\Operator' -ProducerPacingTest display -ProducerPacingTestSeconds $bad | Out-Null } catch { $badPacingRefused=$true }
        Assert ($badPacingRefused -and !(Test-Path -LiteralPath $mgsLeasePath)) 'unbounded pacing requests are refused before runtime files are written'
    }

    [byte[]]$personalBytes=@(239,187,191,91,114,117,110,116,105,109,101,93,13,10,101,110,97,98,108,101,100,61,48,13,10)
    [IO.File]::WriteAllBytes($mgsLeasePath,$personalBytes)
    $lease=New-MgsRuntimeConfigLease -GameExe $mgsLeaseExe -RuntimeManifest 'C:\Simulator\runtime.json' -OperatorDir 'C:\Operator'
    $roundTrip=$lease|ConvertTo-Json|ConvertFrom-Json
    Restore-MgsRuntimeConfigLease -Lease $roundTrip -GameExe $mgsLeaseExe
    Restore-MgsRuntimeConfigLease -Lease $roundTrip -GameExe $mgsLeaseExe
    Assert ([Convert]::ToBase64String([IO.File]::ReadAllBytes($mgsLeasePath)) -eq [Convert]::ToBase64String($personalBytes)) 'personal runtime bytes and BOM survive a simulator lease and repeated cleanup exactly'

    $lease=New-MgsRuntimeConfigLease -GameExe $mgsLeaseExe -RuntimeManifest 'C:\Simulator\runtime.json' -OperatorDir 'C:\Operator'
    [IO.File]::WriteAllText($mgsLeasePath,'personal concurrent edit')
    $editRefused=$false
    try { Restore-MgsRuntimeConfigLease -Lease $lease -GameExe $mgsLeaseExe } catch { $editRefused=$_.Exception.Message -match 'changed during the test' }
    Assert ($editRefused -and [IO.File]::ReadAllText($mgsLeasePath) -eq 'personal concurrent edit') 'cleanup refuses to overwrite runtime edits made after launch'

    $wrongLease=$lease|ConvertTo-Json|ConvertFrom-Json
    $wrongLease.path=Join-Path ([IO.Path]::GetTempPath()) 'unrelated-runtime.ini'
    $wrongPathRefused=$false
    try { Restore-MgsRuntimeConfigLease -Lease $wrongLease -GameExe $mgsLeaseExe } catch { $wrongPathRefused=$_.Exception.Message -match 'does not belong beside' }
    Assert $wrongPathRefused 'a runtime lease cannot restore or remove a file outside the recorded game folder'

    $wrongLease=$lease|ConvertTo-Json|ConvertFrom-Json
    $wrongLease.beforeBase64=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes('changed backup'))
    $backupRefused=$false
    try { Restore-MgsRuntimeConfigLease -Lease $wrongLease -GameExe $mgsLeaseExe } catch { $backupRefused=$_.Exception.Message -match 'backup changed' }
    Assert $backupRefused 'a modified runtime backup cannot replace personal settings'
} finally {
    $mgsResolved=[IO.Path]::GetFullPath($mgsLeaseFixture)
    if(!(Test-MgsPathUnderRoot $mgsResolved ([IO.Path]::GetTempPath()))){throw 'Runtime lease fixture escaped its temporary test folder.'}
    Remove-Item -LiteralPath $mgsResolved -Recurse -Force
}

$roomProc=Proc 202 200 "$root\synthetic_env_server\synth_env_server.exe" '2026-09-27T19:00:03Z' 'synth_env_server.exe'
$roomProc | Add-Member -NotePropertyName CommandLine -NotePropertyValue ('"'+$roomProc.ExecutablePath+'" Bedroom -batchmode')
$roomOwned=Get-MgsOwnedRuntimeProcesses -Processes @($gameProc,$guiProc,$roomProc) -Game $game -RuntimeRoot $root
$roomRecord=@{game=$game;ownedRuntimeProcesses=@($roomOwned)} | ConvertTo-Json -Depth 8 | ConvertFrom-Json
$otherGameProc=Proc 999 1 'D:\Other\other-game.exe' '2026-09-27T19:00:00Z' 'other-game.exe'
$roomPlan=@(Get-MgsOpaqueVrEnvironmentPlan -Record $roomRecord -Processes @($gameProc,$guiProc,$roomProc,$otherGameProc) -RuntimeRoot $root -TcpConnections @() -TcpSnapshotComplete $true)
Assert ($roomPlan.Count -eq 1 -and [int]$roomPlan[0].pid -eq 202) 'opaque VR resource cleanup selects only its recorded batch room, retaining the game, GUI and unrelated game'
$goneRoom=@(Get-MgsOpaqueVrEnvironmentPlan -Record $roomRecord -Processes @($gameProc,$guiProc) -RuntimeRoot $root)
Assert ($goneRoom.Count -eq 0) 'an already released room is a no-op without requiring network inspection'
$unrecordedRoom=@(Get-MgsOpaqueVrEnvironmentPlan -Record $record -Processes @($gameProc,$guiProc,$roomProc) -RuntimeRoot $root)
Assert ($unrecordedRoom.Count -eq 0) 'an unrecorded room is never selected even under the same simulator'
$missingGameRefused=$false
try{Get-MgsOpaqueVrEnvironmentPlan -Record $roomRecord -Processes @($guiProc,$roomProc) -RuntimeRoot $root -TcpConnections @() -TcpSnapshotComplete $true | Out-Null}catch{$missingGameRefused=$_.Exception.Message -match 'MGSV owner is not live'}
Assert $missingGameRefused 'resource cleanup cannot stop a room after its recorded game owner exits'
$reusedRoom=Proc 202 200 $roomProc.ExecutablePath '2026-09-27T19:00:04Z' 'synth_env_server.exe'
$reusedRoom | Add-Member -NotePropertyName CommandLine -NotePropertyValue $roomProc.CommandLine
$reuseRoomRefused=$false
try{Get-MgsOpaqueVrEnvironmentPlan -Record $roomRecord -Processes @($gameProc,$guiProc,$reusedRoom) -RuntimeRoot $root -TcpConnections @() -TcpSnapshotComplete $true | Out-Null}catch{$reuseRoomRefused=$_.Exception.Message -match 'identity changed'}
Assert $reuseRoomRefused 'resource cleanup rejects a reused room PID'
$networkUnknownRefused=$false
try{Get-MgsOpaqueVrEnvironmentPlan -Record $roomRecord -Processes @($gameProc,$guiProc,$roomProc) -RuntimeRoot $root | Out-Null}catch{$networkUnknownRefused=$_.Exception.Message -match 'inspection was incomplete'}
Assert $networkUnknownRefused 'resource cleanup requires complete shared-client inspection'
$outsideClient=Proc 999 1 'D:\Other\other-game.exe' '2026-09-27T19:00:00Z' 'other-game.exe'
$outsideConnections=@(
    [pscustomobject]@{LocalAddress='127.0.0.1';LocalPort=5100;RemoteAddress='127.0.0.1';RemotePort=5101;State='Established';OwningProcess=200},
    [pscustomobject]@{LocalAddress='127.0.0.1';LocalPort=5101;RemoteAddress='127.0.0.1';RemotePort=5100;State='Established';OwningProcess=999}
)
$sharedRoomRefused=$false
try{Get-MgsOpaqueVrEnvironmentPlan -Record $roomRecord -Processes @($gameProc,$guiProc,$roomProc,$outsideClient) -RuntimeRoot $root -TcpConnections $outsideConnections -TcpSnapshotComplete $true | Out-Null}catch{$sharedRoomRefused=$_.Exception.Message -match 'outside exact live recorded identities'}
Assert $sharedRoomRefused 'resource cleanup cannot interrupt a simulator shared with another live application'
$roomProc.CommandLine='"'+$roomProc.ExecutablePath+'" Bedroom'
$interactiveRoomRefused=$false
try{Get-MgsOpaqueVrEnvironmentPlan -Record $roomRecord -Processes @($gameProc,$guiProc,$roomProc) -RuntimeRoot $root -TcpConnections @() -TcpSnapshotComplete $true | Out-Null}catch{$interactiveRoomRefused=$_.Exception.Message -match 'not the recorded batch room'}
Assert $interactiveRoomRefused 'resource cleanup never selects an interactive synthetic environment editor'

# Exercise the bounded startup retry without touching a process. A network
# refusal must acquire a fresh plan; all other ownership refusals stay final.
$ownershipModule=Get-Module simulator-session-ownership
& $ownershipModule {
    $script:stopAttempts=0
    function script:Stop-MgsOpaqueVrEnvironmentOnce {
        param($Record,$RuntimeRoot)
        $script:stopAttempts++
        if($script:stopAttempts -eq 1){throw 'Simulator PID 200 has an established non-loopback TCP peer; shared ownership cannot be excluded, refusing cleanup.'}
        'fresh isolated plan'
    }
}
$retryResult=Stop-MgsOpaqueVrEnvironment -Record $roomRecord -RuntimeRoot $root
$retryAttempts=& $ownershipModule { $script:stopAttempts }
Assert ($retryResult -eq 'fresh isolated plan' -and $retryAttempts -eq 2) 'a transient outbound startup peer gets a fresh ownership plan before cleanup'
& $ownershipModule {
    $script:stopAttempts=0
    function script:Stop-MgsOpaqueVrEnvironmentOnce {
        param($Record,$RuntimeRoot)
        $script:stopAttempts++
        if($script:stopAttempts -eq 1){throw 'Simulator PID 200 has an established non-loopback TCP peer; refusing cleanup.'}
        throw 'Synthetic environment identity changed; refusing VR resource cleanup.'
    }
}
$changedDuringRetry=$false
try{Stop-MgsOpaqueVrEnvironment -Record $roomRecord -RuntimeRoot $root | Out-Null}catch{$changedDuringRetry=$_.Exception.Message -match 'identity changed'}
$retryAttempts=& $ownershipModule { $script:stopAttempts }
Assert ($changedDuringRetry -and $retryAttempts -eq 2) 'a changed identity after a transient network refusal stops retry immediately'
& $ownershipModule {
    $script:stopAttempts=0
    function script:Stop-MgsOpaqueVrEnvironmentOnce {
        param($Record,$RuntimeRoot)
        $script:stopAttempts++
        throw 'Simulator PID 200 has loopback client PID 999 outside exact live recorded identities; refusing cleanup.'
    }
}
$sharedRetryRefused=$false
try{Stop-MgsOpaqueVrEnvironment -Record $roomRecord -RuntimeRoot $root | Out-Null}catch{$sharedRetryRefused=$_.Exception.Message -match 'outside exact live recorded identities'}
$retryAttempts=& $ownershipModule { $script:stopAttempts }
Assert ($sharedRetryRefused -and $retryAttempts -eq 1) 'a shared local client refuses startup cleanup without retry'

& $ownershipModule {
    $script:stopAttempts=0
    function script:Stop-MgsOpaqueVrEnvironmentOnce {
        param($Record,$RuntimeRoot)
        $script:stopAttempts++
        throw 'Simulator PID 200 has an established non-loopback TCP peer; shared ownership cannot be excluded, refusing cleanup.'
    }
}
$expiredPeerRefused=$false
try{Stop-MgsOpaqueVrEnvironment -Record $roomRecord -RuntimeRoot $root -StartupPeerWaitSeconds 0 | Out-Null}catch{$expiredPeerRefused=$_.Exception.Message -match 'established non-loopback TCP peer'}
$retryAttempts=& $ownershipModule { $script:stopAttempts }
Assert ($expiredPeerRefused -and $retryAttempts -eq 1) 'an expired startup deadline retains the live peer and refuses room cleanup'

$mgsReleaseFixture=Join-Path ([IO.Path]::GetTempPath()) ('mgs5vr-room-helper-'+[Guid]::NewGuid().ToString('N'))
try {
    & $ownershipModule {
        $script:helperLaunches=@()
        function script:Start-Process {
            param($FilePath,$ArgumentList,$WindowStyle,$RedirectStandardOutput,$RedirectStandardError)
            $script:helperLaunches+=@{file=$FilePath;args=$ArgumentList;style=$WindowStyle;output=$RedirectStandardOutput;error=$RedirectStandardError}
        }
    }
    $deferred=Start-MgsOpaqueVrEnvironmentRelease -Record $roomRecord -RuntimeRoot $root -EvidenceDirectory $mgsReleaseFixture
    $launches=@(& $ownershipModule { $script:helperLaunches })
    $snapshots=@(Get-ChildItem -LiteralPath $mgsReleaseFixture -Filter '*.json')
    Assert ($launches.Count -eq 1 -and $launches[0].style -eq 'Hidden' -and $snapshots.Count -eq 1) 'a long startup peer defers to exactly one hidden bounded helper without blocking launch'
    $immutable=Get-Content -LiteralPath $snapshots[0].FullName -Raw | ConvertFrom-Json
    Assert (Test-MgsExactProcessIdentity $immutable.game $roomRecord.game) 'the deferred helper receives this exact game generation in an independent snapshot'
    $roomRecord.game.pid=777
    $unchanged=Get-Content -LiteralPath $snapshots[0].FullName -Raw | ConvertFrom-Json
    Assert ([int]$unchanged.game.pid -ne 777) 'a later launch record cannot replace the helper owner'
    Assert ($deferred -match 'release is pending') 'deferred launch reports pending cleanup without claiming room release'
    & $ownershipModule {
        function script:Stop-MgsOpaqueVrEnvironmentOnce {
            param($Record,$RuntimeRoot)
            throw 'Simulator PID 200 has loopback client PID 999 outside exact live recorded identities; refusing cleanup.'
        }
    }
    $sharedDeferredRefused=$false
    try{Start-MgsOpaqueVrEnvironmentRelease -Record $roomRecord -RuntimeRoot $root -EvidenceDirectory $mgsReleaseFixture | Out-Null}catch{$sharedDeferredRefused=$_.Exception.Message -match 'outside exact live recorded identities'}
    $launches=@(& $ownershipModule { $script:helperLaunches })
    Assert ($sharedDeferredRefused -and $launches.Count -eq 1) 'a shared client never creates a deferred cleanup helper'
} finally {
    $mgsResolved=[IO.Path]::GetFullPath($mgsReleaseFixture)
    if(!(Test-MgsPathUnderRoot $mgsResolved ([IO.Path]::GetTempPath()))){throw 'Room helper fixture escaped its temporary test folder.'}
    if(Test-Path -LiteralPath $mgsResolved){Remove-Item -LiteralPath $mgsResolved -Recurse -Force}
}

Write-Host 'All simulator session ownership and runtime lease tests passed.'
