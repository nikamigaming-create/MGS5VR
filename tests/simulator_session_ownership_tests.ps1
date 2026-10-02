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
    $roundTrip=$lease|ConvertTo-Json|ConvertFrom-Json
    Restore-MgsRuntimeConfigLease -Lease $roundTrip -GameExe $mgsLeaseExe
    Restore-MgsRuntimeConfigLease -Lease $roundTrip -GameExe $mgsLeaseExe
    Assert (!(Test-Path -LiteralPath $mgsLeasePath)) 'an originally absent runtime file is removed after JSON-record recovery and repeated cleanup'

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

Write-Host 'All simulator session ownership and runtime lease tests passed.'
