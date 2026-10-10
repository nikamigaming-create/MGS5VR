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
                    $parentIdentity=if([int]$parent.pid -eq [int]$Game.pid){ConvertTo-MgsProcessIdentity $Game}else{ConvertTo-MgsProcessIdentity $byId[[int]$parent.pid]}
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

function Expand-MgsLiveRuntimeRecord {
    param([Parameter(Mandatory=$true)]$Record,[Parameter(Mandatory=$true)][object[]]$Processes,
        [Parameter(Mandatory=$true)][string]$RuntimeRoot)
    $game=@($Processes | Where-Object { [int]$_.ProcessId -eq [int]$Record.game.pid })
    if($game.Count -ne 1 -or !(Test-MgsExactProcessIdentity $Record.game $game[0])){
        throw 'Exact recorded game owner changed; refusing late runtime registration.'
    }
    $known=@{}
    foreach($identity in @($Record.ownedRuntimeProcesses)){
        $current=@($Processes | Where-Object { [int]$_.ProcessId -eq [int]$identity.pid })
        if($current.Count -and ($current.Count -ne 1 -or !(Test-MgsExactProcessIdentity $identity $current[0]))){
            throw 'Recorded runtime generation changed; refusing late runtime registration.'
        }
        $known[[int]$identity.pid]=$identity
    }
    foreach($identity in @(Get-MgsOwnedRuntimeProcesses -Processes $Processes -Game $Record.game -RuntimeRoot $RuntimeRoot)){
        if((ConvertTo-MgsUtcStartTime $identity.startedUtc) -lt (ConvertTo-MgsUtcStartTime $identity.parentIdentity.startedUtc)){
            throw 'Runtime child predates its live parent; refusing late runtime registration.'
        }
        if($known.ContainsKey([int]$identity.pid)){
            if(!(Test-MgsExactProcessIdentity $known[[int]$identity.pid] $identity)){
                throw 'Discovered runtime generation changed; refusing late runtime registration.'
            }
        }else{$known[[int]$identity.pid]=$identity}
    }
    # Return a new snapshot; never replace the immutable helper's game owner.
    $expanded=$Record|ConvertTo-Json -Depth 8|ConvertFrom-Json
    $expanded.ownedRuntimeProcesses=@($known.Values | Sort-Object depth,pid)
    return $expanded
}

function Get-MgsOpaqueVrEnvironmentPlan {
    param(
        [Parameter(Mandatory=$true)]$Record,
        [Parameter(Mandatory=$true)][object[]]$Processes,
        [Parameter(Mandatory=$true)][string]$RuntimeRoot,
        [AllowEmptyCollection()][object[]]$TcpConnections=@(),
        [bool]$TcpSnapshotComplete=$false
    )
    # MGSV submits opaque native VR. The simulator's optional passthrough room
    # is unused, and its multi-GB allocation can evict the stereo swapchains.
    # Select only its exact recorded child; never select another game or GUI.
    $selected=[Collections.Generic.List[object]]::new()
    $expectedPath=Join-Path $RuntimeRoot 'synthetic_env_server\synth_env_server.exe'
    foreach($identity in @(Get-MgsValue $Record 'ownedRuntimeProcesses')){
        if(![string]::Equals([string]$identity.path,$expectedPath,[StringComparison]::OrdinalIgnoreCase)){continue}
        $current=@($Processes | Where-Object { [int]$_.ProcessId -eq [int]$identity.pid })
        if(!$current.Count){continue}
        if($current.Count -ne 1 -or !(Test-MgsExactProcessIdentity $identity $current[0])){throw 'Synthetic environment identity changed; refusing VR resource cleanup.'}
        $game=Get-MgsValue $Record 'game'
        if(!$game -or [IO.Path]::GetFileName([string]$game.path) -ine 'mgsvtpp.exe'){throw 'Exact MGSV owner is not live; refusing VR resource cleanup.'}
        $liveGame=@($Processes | Where-Object { [int]$_.ProcessId -eq [int]$game.pid })
        if($liveGame.Count -ne 1 -or !(Test-MgsExactProcessIdentity $game $liveGame[0])){throw 'Exact MGSV owner is not live; refusing VR resource cleanup.'}
        $parent=Get-MgsValue $identity 'parentIdentity'
        $gui=@($Processes | Where-Object { $parent -and [int]$_.ProcessId -eq [int]$parent.pid })
        if(!$parent -or $gui.Count -ne 1 -or !(Test-MgsExactProcessIdentity $parent $gui[0]) -or [int]$parent.parentPid -ne [int]$game.pid -or [int]$identity.parentPid -ne [int]$parent.pid -or ![string]::Equals([string]$parent.path,(Join-Path $RuntimeRoot 'MetaXRSimulator.exe'),[StringComparison]::OrdinalIgnoreCase)){
            throw 'Synthetic environment has no exact MGSV simulator parent; refusing VR resource cleanup.'
        }
        if([string](Get-MgsValue $current[0] 'CommandLine') -notmatch '(?i)(?:^|\s)-batchmode(?:\s|$)'){throw 'Synthetic environment is not the recorded batch room; refusing VR resource cleanup.'}
        Assert-MgsSimulatorTcpIsolation -Connections $TcpConnections -Processes $Processes -Record $Record -SimulatorPids @([int]$parent.pid,[int]$identity.pid) -SnapshotComplete $TcpSnapshotComplete | Out-Null
        $selected.Add($identity)
    }
    return @($selected)
}

function Stop-MgsOpaqueVrEnvironmentOnce {
    param([Parameter(Mandatory=$true)]$Record,[Parameter(Mandatory=$true)][string]$RuntimeRoot)
    $snapshot=@(Get-CimInstance Win32_Process | Where-Object { $_.ExecutablePath -and $_.CreationDate })
    $roomPath=Join-Path $RuntimeRoot 'synthetic_env_server\synth_env_server.exe'
    $recordedRoom=@($Record.ownedRuntimeProcesses | Where-Object { [string]::Equals([string]$_.path,$roomPath,[StringComparison]::OrdinalIgnoreCase) })
    if(!@($snapshot | Where-Object { [int]$_.ProcessId -in @($recordedRoom | ForEach-Object { [int]$_.pid }) }).Count){return}
    $Record=Expand-MgsLiveRuntimeRecord -Record $Record -Processes $snapshot -RuntimeRoot $RuntimeRoot
    $connections=@(Get-NetTCPConnection -ErrorAction Stop)
    $plan=@(Get-MgsOpaqueVrEnvironmentPlan -Record $Record -Processes $snapshot -RuntimeRoot $RuntimeRoot -TcpConnections $connections -TcpSnapshotComplete $true)
    foreach($identity in $plan){
        $process=$null
        try{
            $process=[Diagnostics.Process]::GetProcessById([int]$identity.pid)
            if($process.HasExited){continue}
            # Recheck all owners and connections immediately before stopping
            # this process handle. PID reuse or a new shared client blocks it.
            $latest=@(Get-CimInstance Win32_Process | Where-Object { $_.ExecutablePath -and $_.CreationDate })
            $Record=Expand-MgsLiveRuntimeRecord -Record $Record -Processes $latest -RuntimeRoot $RuntimeRoot
            $latestConnections=@(Get-NetTCPConnection -ErrorAction Stop)
            $verified=@(Get-MgsOpaqueVrEnvironmentPlan -Record $Record -Processes $latest -RuntimeRoot $RuntimeRoot -TcpConnections $latestConnections -TcpSnapshotComplete $true)
            if(!@($verified | Where-Object { [int]$_.pid -eq [int]$identity.pid }).Count){continue}
            $started=ConvertTo-MgsUtcStartTime $identity.startedUtc
            if(![string]::Equals($process.MainModule.FileName,[string]$identity.path,[StringComparison]::OrdinalIgnoreCase) -or [Math]::Abs(($process.StartTime.ToUniversalTime()-$started).TotalMilliseconds) -gt 10){throw 'Synthetic environment process handle changed; refusing resource cleanup.'}
            $process.Kill()
            if(!$process.WaitForExit(2000)){
                $process.Refresh()
                if(!$process.HasExited){throw 'Unused synthetic environment did not exit within two seconds.'}
            }
            Write-Output ('Released unused simulator passthrough room '+$identity.pid+'; MGSV and stereo compositor retained.')
        }finally{if($process){$process.Dispose()}}
    }
}

function Stop-MgsOpaqueVrEnvironment {
    param([Parameter(Mandatory=$true)]$Record,[Parameter(Mandatory=$true)][string]$RuntimeRoot,
        [ValidateRange(0,180)][int]$StartupPeerWaitSeconds=6)
    # The freshly started GUI briefly opens an outbound HTTPS connection.
    # A refused snapshot is not permission to stop its room. Retry only that
    # transient network condition, taking all identities and peers afresh;
    # shared local clients and changed owners still fail immediately.
    # Launchers defer a long-lived startup peer to an exact-owner helper.
    # Keep ownership checks intact and report progress during any bounded wait.
    $deadline=[DateTime]::UtcNow.AddSeconds($StartupPeerWaitSeconds)
    $nextProgress=[DateTime]::UtcNow
    do{
        try{
            Stop-MgsOpaqueVrEnvironmentOnce -Record $Record -RuntimeRoot $RuntimeRoot
            return
        }catch{
            if($_.Exception.Message -notmatch 'established non-loopback TCP peer' -or [DateTime]::UtcNow -ge $deadline){throw}
            if([DateTime]::UtcNow -ge $nextProgress){
                Write-Host 'Waiting for the simulator startup peer to close; checking fresh ownership before releasing its unused room.'
                $nextProgress=[DateTime]::UtcNow.AddSeconds(5)
            }
            Start-Sleep -Milliseconds 200
        }
    }while($true)
}

function Start-MgsOpaqueVrEnvironmentRelease {
    param([Parameter(Mandatory=$true)]$Record,[Parameter(Mandatory=$true)][string]$RuntimeRoot,
        [Parameter(Mandatory=$true)][string]$EvidenceDirectory)
    try {
        Stop-MgsOpaqueVrEnvironment -Record $Record -RuntimeRoot $RuntimeRoot -StartupPeerWaitSeconds 0
        return
    } catch {
        # Only the observed startup HTTPS peer may defer cleanup. A shared
        # local client, changed identity or incomplete inventory stays fatal.
        if($_.Exception.Message -notmatch 'established non-loopback TCP peer'){throw}
    }
    $directory=[IO.Path]::GetFullPath($EvidenceDirectory)
    [IO.Directory]::CreateDirectory($directory) | Out-Null
    $prefix=Join-Path $directory ('simulator-room-release-'+[Guid]::NewGuid().ToString('N'))
    $snapshot=$prefix+'.json'
    # This immutable snapshot belongs to this exact launch. A subsequent
    # session can never replace the identities used by the helper.
    $bytes=[Text.UTF8Encoding]::new($false).GetBytes(($Record|ConvertTo-Json -Depth 8))
    $stream=[IO.File]::Open($snapshot,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
    try{$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
    $helper=Join-Path $PSScriptRoot 'release-simulator-room.ps1'
    $arguments=@('-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',('"'+$helper+'"'),
        '-Record',('"'+$snapshot+'"'),'-RuntimeRoot',('"'+$RuntimeRoot+'"'),'-Status',('"'+$prefix+'.status.json"'))
    $powershell=Join-Path ([Environment]::GetFolderPath('System')) 'WindowsPowerShell\v1.0\powershell.exe'
    Start-Process -FilePath $powershell -ArgumentList $arguments -WindowStyle Hidden `
        -RedirectStandardOutput ($prefix+'.log') -RedirectStandardError ($prefix+'.error.log') | Out-Null
    Write-Output ('Unused simulator room release is pending; exact-owner helper evidence: '+$prefix+'.status.json')
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
          [Parameter(Mandatory=$true)][string]$OperatorDir,
          [ValidateRange(1024,65535)][int]$OperatorPort=8720,
          [switch]$TrackingFaultTest,
          [switch]$IdroidUiBoundaryTrace,
          [string]$CameraEvidenceDir='',
          [ValidateSet('baseline','display')][string]$ProducerPacingTest='baseline',
          [ValidateRange(30,1200)][int]$ProducerPacingTestSeconds=900)
    $exe=[IO.Path]::GetFullPath($GameExe)
    if($CameraEvidenceDir){
        if(![IO.Path]::IsPathRooted($CameraEvidenceDir) -or ![IO.Directory]::Exists($CameraEvidenceDir)){
            throw 'Camera evidence needs an existing absolute directory before the runtime lease is written.'
        }
        $CameraEvidenceDir=[IO.Path]::GetFullPath($CameraEvidenceDir)
    }
    $path=Join-Path (Split-Path -Parent $exe) 'mgs5vr-runtime.ini'
    $present=Test-Path -LiteralPath $path -PathType Leaf
    [byte[]]$before=@()
    if($present){$before=[IO.File]::ReadAllBytes($path)}
    $text="[runtime]`r`nenabled=1`r`nmanifest=$RuntimeManifest`r`napi_layer_path=$OperatorDir`r`napi_layers=XR_APILAYER_METAX_operator`r`noperator_port=$OperatorPort`r`ntracking_fault_test=$([int][bool]$TrackingFaultTest)`r`nidroid_ui_boundary_trace=$([int][bool]$IdroidUiBoundaryTrace)`r`ncamera_evidence_dir=$CameraEvidenceDir`r`nproducer_pacing_test=$ProducerPacingTest`r`nproducer_pacing_test_seconds=$ProducerPacingTestSeconds`r`nheadless=0`r`n"
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

Export-ModuleMember -Function ConvertTo-MgsUtcStartTime,ConvertTo-MgsProcessIdentity,Test-MgsExactProcessIdentity,Test-MgsPathUnderRoot,Get-MgsOwnedRuntimeProcesses,Expand-MgsLiveRuntimeRecord,Get-MgsOpaqueVrEnvironmentPlan,Stop-MgsOpaqueVrEnvironment,Start-MgsOpaqueVrEnvironmentRelease,Assert-MgsSimulatorTcpIsolation,Get-MgsOrphanCleanupPlan,New-MgsRuntimeConfigLease,Restore-MgsRuntimeConfigLease
