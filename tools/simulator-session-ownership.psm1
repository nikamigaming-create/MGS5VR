Set-StrictMode -Version Latest

function Get-MgsValue($Object,[string]$Name) {
    if($Object -is [Collections.IDictionary]){
        foreach($key in $Object.Keys){if([string]::Equals([string]$key,$Name,[StringComparison]::OrdinalIgnoreCase)){return $Object[$key]}}
        return $null
    }
    $property=$Object.PSObject.Properties[$Name]
    if($property){return $property.Value}
    return $null
}

function ConvertTo-MgsUtcStartTime {
    param([Parameter(Mandatory=$true)]$Value)
    if($Value -is [DateTime]){return $Value.ToUniversalTime()}
    if($Value -is [DateTimeOffset]){return $Value.UtcDateTime}
    return [DateTime]::Parse([string]$Value,[Globalization.CultureInfo]::InvariantCulture,
        [Globalization.DateTimeStyles]::RoundtripKind).ToUniversalTime()
}

function ConvertTo-MgsProcessIdentity {
    param([Parameter(Mandatory=$true)]$Process)
    $path=[string](Get-MgsValue $Process 'ExecutablePath')
    if(!$path){$path=[string](Get-MgsValue $Process 'path')}
    $started=Get-MgsValue $Process 'CreationDate'
    if(!$started){$started=Get-MgsValue $Process 'startedUtc'}
    if(!$path -or !$started){throw 'A process identity requires an executable path and creation time.'}
    $utc=ConvertTo-MgsUtcStartTime $started
    [ordered]@{
        pid=[int]$(if($null -ne (Get-MgsValue $Process 'ProcessId')){Get-MgsValue $Process 'ProcessId'}else{Get-MgsValue $Process 'pid'})
        parentPid=[int]$(if($null -ne (Get-MgsValue $Process 'ParentProcessId')){Get-MgsValue $Process 'ParentProcessId'}else{Get-MgsValue $Process 'parentPid'})
        startedUtc=$utc.ToString('o')
        path=[IO.Path]::GetFullPath($path)
    }
}

function Test-MgsExactProcessIdentity {
    param([Parameter(Mandatory=$true)]$Expected,[Parameter(Mandatory=$true)]$Current)
    try { $expectedIdentity=ConvertTo-MgsProcessIdentity $Expected;$actual=ConvertTo-MgsProcessIdentity $Current } catch { return $false }
    if([int]$expectedIdentity.pid -ne [int]$actual.pid -or [int]$expectedIdentity.parentPid -ne [int]$actual.parentPid){return $false}
    if(![string]::Equals([string]$expectedIdentity.path,[string]$actual.path,[StringComparison]::OrdinalIgnoreCase)){return $false}
    try {
        $a=ConvertTo-MgsUtcStartTime $expectedIdentity.startedUtc
        $b=ConvertTo-MgsUtcStartTime $actual.startedUtc
        return $a.Ticks -eq $b.Ticks
    } catch { return $false }
}

function Test-MgsPathUnderRoot {
    param([string]$Path,[string]$Root)
    if(!$Path -or !$Root){return $false}
    $full=[IO.Path]::GetFullPath($Path).TrimEnd([IO.Path]::DirectorySeparatorChar,[IO.Path]::AltDirectorySeparatorChar)
    $base=[IO.Path]::GetFullPath($Root).TrimEnd([IO.Path]::DirectorySeparatorChar,[IO.Path]::AltDirectorySeparatorChar)
    return $full.StartsWith($base+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)
}

function Get-MgsOwnedRuntimeProcesses {
    param([Parameter(Mandatory=$true)][object[]]$Processes,[Parameter(Mandatory=$true)]$Game,[Parameter(Mandatory=$true)][string]$RuntimeRoot)
    $byParent=@{}
    $byId=@{}
    foreach($p in $Processes){$pidValue=[int]$p.ProcessId;$byId[$pidValue]=$p;if(!$byParent.ContainsKey([int]$p.ParentProcessId)){$byParent[[int]$p.ParentProcessId]=[Collections.Generic.List[object]]::new()};$byParent[[int]$p.ParentProcessId].Add($p)}
    $result=[Collections.Generic.List[object]]::new()
    $frontier=@([pscustomobject]@{pid=[int]$Game.pid;depth=0})
    $seen=[Collections.Generic.HashSet[int]]::new()
    while($frontier.Count){
        $next=[Collections.Generic.List[object]]::new()
        foreach($parent in $frontier){
            if(!$byParent.ContainsKey([int]$parent.pid)){continue}
            foreach($child in $byParent[[int]$parent.pid]){
                $identity=ConvertTo-MgsProcessIdentity $child
                if(!$seen.Add([int]$identity.pid)){continue}
                $isRuntime=Test-MgsPathUnderRoot $identity.path $RuntimeRoot
                $isMetaGui=[IO.Path]::GetFileName($identity.path) -ieq 'MetaXRSimulator.exe'
                if($isRuntime){
                    $identity.role=if($parent.depth -eq 0 -and $isMetaGui){'meta_gui'}else{'runtime_child'}
                    $identity.depth=[int]$parent.depth+1
                    $parentIdentity=if([int]$parent.pid -eq [int]$Game.pid){$Game}else{ConvertTo-MgsProcessIdentity $byId[[int]$parent.pid]}
                    $identity.parentIdentity=[ordered]@{pid=[int]$parentIdentity.pid;parentPid=[int]$parentIdentity.parentPid;startedUtc=[string]$parentIdentity.startedUtc;path=[string]$parentIdentity.path}
                    $result.Add($identity)
                    $next.Add([pscustomobject]@{pid=$identity.pid;depth=$identity.depth})
                }
            }
        }
        $frontier=@($next)
    }
    return @($result | Sort-Object @{Expression={[int]$_.depth}},@{Expression={[int]$_.pid}})
}

function Test-MgsLoopbackAddress([string]$Address) {
    if(!$Address){return $false}
    if($Address -eq '::1'){return $true}
    $parsed=$null
    if(![Net.IPAddress]::TryParse($Address,[ref]$parsed)){return $false}
    if($parsed.AddressFamily -eq [Net.Sockets.AddressFamily]::InterNetworkV6){return $parsed.IsIPv6Loopback}
    return (($parsed.GetAddressBytes()[0] -eq 127))
}

function Assert-MgsSimulatorTcpIsolation {
    param(
        [Parameter(Mandatory=$true)][AllowNull()][AllowEmptyCollection()][object[]]$Connections,
        [Parameter(Mandatory=$true)][object[]]$Processes,
        [Parameter(Mandatory=$true)]$Record,
        [Parameter(Mandatory=$true)][int[]]$SimulatorPids,
        [Parameter(Mandatory=$true)][bool]$SnapshotComplete
    )
    if(!$SnapshotComplete){throw 'Loopback TCP peer inspection was incomplete; shared simulator ownership is unproven, refusing cleanup.'}
    if(!$Record -or !$Record.game){throw 'No exact session identities are available for TCP peer validation; shared simulator ownership is unproven, refusing cleanup.'}
    $known=[Collections.Generic.List[object]]::new()
    $known.Add($Record.game)
    foreach($identity in @($Record.ownedRuntimeProcesses)){$known.Add($identity)}
    foreach($connection in $Connections){
        $ownerValue=Get-MgsValue $connection 'OwningProcess'
        $state=[string](Get-MgsValue $connection 'State')
        if($null -eq $ownerValue -or !$state){throw 'A TCP connection row is incomplete; shared simulator ownership is unproven, refusing cleanup.'}
        $owner=[int]$ownerValue
        if($state -ine 'Established' -or $owner -notin $SimulatorPids){continue}
        $localAddress=[string](Get-MgsValue $connection 'LocalAddress')
        $remoteAddress=[string](Get-MgsValue $connection 'RemoteAddress')
        $localPortValue=Get-MgsValue $connection 'LocalPort'
        $remotePortValue=Get-MgsValue $connection 'RemotePort'
        if(!$localAddress -or !$remoteAddress -or $null -eq $localPortValue -or $null -eq $remotePortValue){
            throw "Established simulator TCP connection for PID $owner is incomplete; refusing cleanup."
        }
        if(!(Test-MgsLoopbackAddress $localAddress) -or !(Test-MgsLoopbackAddress $remoteAddress)){
            throw "Simulator PID $owner has an established non-loopback TCP peer; shared ownership cannot be excluded, refusing cleanup."
        }
        $peerRows=@($Connections | Where-Object {
            [string](Get-MgsValue $_ 'State') -ieq 'Established' -and
            [string](Get-MgsValue $_ 'LocalAddress') -eq $remoteAddress -and
            [int](Get-MgsValue $_ 'LocalPort') -eq [int]$remotePortValue -and
            [string](Get-MgsValue $_ 'RemoteAddress') -eq $localAddress -and
            [int](Get-MgsValue $_ 'RemotePort') -eq [int]$localPortValue
        })
        if($peerRows.Count -ne 1){
            throw "Simulator PID $owner has an established loopback connection without exactly one readable peer PID; refusing cleanup."
        }
        $peerPid=[int](Get-MgsValue $peerRows[0] 'OwningProcess')
        if($peerPid -eq $owner){continue}
        $expected=@($known | Where-Object { [int](Get-MgsValue $_ 'pid') -eq $peerPid })
        $current=@($Processes | Where-Object { [int](Get-MgsValue $_ 'ProcessId') -eq $peerPid })
        if($expected.Count -ne 1 -or $current.Count -ne 1 -or !(Test-MgsExactProcessIdentity $expected[0] $current[0])){
            throw "Simulator PID $owner has loopback client PID $peerPid outside exact live recorded identities; refusing cleanup."
        }
    }
    return $true
}

function Get-MgsOrphanCleanupPlan {
    param([Parameter(Mandatory=$true)][AllowNull()]$Record,[Parameter(Mandatory=$true)][object[]]$Processes,
          [object[]]$TcpConnections=@(),[bool]$TcpSnapshotComplete=$false)
    $simulators=@($Processes | Where-Object { ([string]$_.Name -ieq 'MetaXRSimulator.exe') -or [IO.Path]::GetFileName([string]$_.ExecutablePath) -ieq 'MetaXRSimulator.exe' })
    if(!$Record -or [int]$Record.schema -lt 2 -or !$Record.game){
        if($simulators.Count){throw 'An unrecorded MetaXRSimulator process is running; refusing to reuse or stop it.'}
        return @()
    }
    $game=@($Processes | Where-Object { [int]$_.ProcessId -eq [int]$Record.game.pid })
    if($game.Count -and (Test-MgsExactProcessIdentity $Record.game $game[0])){throw 'The recorded MGSV session is still running; refusing orphan cleanup.'}
    $owned=@($Record.ownedRuntimeProcesses)
    foreach($expected in $owned){
        $parentExpected=if([int]$expected.parentPid -eq [int]$Record.game.pid){$Record.game}else{@($owned | Where-Object { [int]$_.pid -eq [int]$expected.parentPid } | Select-Object -First 1)}
        if(!$parentExpected -or !(Test-MgsExactProcessIdentity $expected.parentIdentity $parentExpected)){
            throw "Recorded runtime PID $($expected.pid) has no exact recorded owner; refusing cleanup."
        }
        $liveParent=@($Processes | Where-Object { [int]$_.ProcessId -eq [int]$expected.parentPid })
        if($liveParent.Count -and !(Test-MgsExactProcessIdentity $expected.parentIdentity $liveParent[0])){
            throw "Recorded parent PID $($expected.parentPid) was reused; refusing cleanup."
        }
    }
    $liveOwned=[Collections.Generic.List[object]]::new()
    foreach($expected in $owned){
        $samePid=@($Processes | Where-Object { [int]$_.ProcessId -eq [int]$expected.pid })
        if(!$samePid.Count){continue}
        if(!(Test-MgsExactProcessIdentity $expected $samePid[0])){throw "Recorded runtime PID $($expected.pid) no longer has the recorded start/path/parent identity; refusing cleanup."}
        $liveOwned.Add($expected)
    }
    foreach($sim in $simulators){
        $identity=ConvertTo-MgsProcessIdentity $sim
        $match=@($owned | Where-Object { Test-MgsExactProcessIdentity $_ $identity })
        if(!$match.Count){throw "MetaXRSimulator PID $($identity.pid) is not in the exited game's exact ownership record; refusing reuse."}
    }
    if($simulators.Count){
        [void](Assert-MgsSimulatorTcpIsolation -Connections $TcpConnections -Processes $Processes -Record $Record `
            -SimulatorPids @($simulators | ForEach-Object { [int]$_.ProcessId }) -SnapshotComplete $TcpSnapshotComplete)
    }
    return @($liveOwned | Sort-Object @{Expression={[int]$_.depth};Descending=$true},@{Expression={[int]$_.pid};Descending=$true})
}

function Get-MgsBytesHash([byte[]]$Bytes) {
    $hash=[Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($hash.ComputeHash($Bytes)).Replace('-','').ToLowerInvariant() }
    finally { $hash.Dispose() }
}

function New-MgsRuntimeConfigLease {
    param([Parameter(Mandatory=$true)][string]$GameExe,
          [Parameter(Mandatory=$true)][string]$RuntimeManifest,
          [Parameter(Mandatory=$true)][string]$OperatorDir)
    $exe=[IO.Path]::GetFullPath($GameExe)
    $path=Join-Path (Split-Path -Parent $exe) 'mgs5vr-runtime.ini'
    $present=Test-Path -LiteralPath $path -PathType Leaf
    [byte[]]$before=@()
    if($present){$before=[IO.File]::ReadAllBytes($path)}
    $text="[runtime]`r`nenabled=1`r`nmanifest=$RuntimeManifest`r`napi_layer_path=$OperatorDir`r`napi_layers=XR_APILAYER_METAX_operator`r`nheadless=0`r`n"
    $bytes=[Text.UTF8Encoding]::new($false).GetBytes($text)
    $lease=[ordered]@{path=$path;gameExe=$exe;beforePresent=[bool]$present;
        beforeBase64=[Convert]::ToBase64String($before);beforeSha256=(Get-MgsBytesHash $before);
        writtenSha256=(Get-MgsBytesHash $bytes)}
    try { [IO.File]::WriteAllBytes($path,$bytes) }
    catch {
        if($present){[IO.File]::WriteAllBytes($path,$before)}
        elseif(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path}
        throw
    }
    return $lease
}

function Restore-MgsRuntimeConfigLease {
    param([Parameter(Mandatory=$true)]$Lease,[Parameter(Mandatory=$true)][string]$GameExe)
    $exe=[IO.Path]::GetFullPath($GameExe)
    $path=Join-Path (Split-Path -Parent $exe) 'mgs5vr-runtime.ini'
    if(![string]::Equals($exe,[string](Get-MgsValue $Lease 'gameExe'),[StringComparison]::OrdinalIgnoreCase) -or
       ![string]::Equals($path,[string](Get-MgsValue $Lease 'path'),[StringComparison]::OrdinalIgnoreCase)){
        throw 'Runtime configuration lease does not belong beside the recorded game; refusing restoration.'
    }
    $present=[bool](Get-MgsValue $Lease 'beforePresent')
    [byte[]]$before=[Convert]::FromBase64String([string](Get-MgsValue $Lease 'beforeBase64'))
    $beforeHash=Get-MgsBytesHash $before
    if($beforeHash -ne [string](Get-MgsValue $Lease 'beforeSha256')){throw 'Runtime configuration backup changed; refusing restoration.'}
    if(!(Test-Path -LiteralPath $path -PathType Leaf)){
        if(!$present){return}
        throw 'Runtime configuration disappeared during the test; refusing to replace an unrelated change.'
    }
    $currentHash=Get-MgsBytesHash ([IO.File]::ReadAllBytes($path))
    if($present -and $currentHash -eq $beforeHash){return}
    if($currentHash -ne [string](Get-MgsValue $Lease 'writtenSha256')){
        throw 'Runtime configuration changed during the test; its personal changes and session record were preserved.'
    }
    if($present){[IO.File]::WriteAllBytes($path,$before)}else{Remove-Item -LiteralPath $path}
}

Export-ModuleMember -Function ConvertTo-MgsUtcStartTime,ConvertTo-MgsProcessIdentity,Test-MgsExactProcessIdentity,Test-MgsPathUnderRoot,Get-MgsOwnedRuntimeProcesses,Assert-MgsSimulatorTcpIsolation,Get-MgsOrphanCleanupPlan,New-MgsRuntimeConfigLease,Restore-MgsRuntimeConfigLease
