param(
    [Parameter(Mandatory=$true)][string]$RuntimeManifest,
    [string]$OperatorDir,
    [switch]$RestartSteam,
    [switch]$Headless
)
$ErrorActionPreference='Stop'
$mgsOwnershipModule=Join-Path $PSScriptRoot 'simulator-session-ownership.psm1'
Import-Module -Name $mgsOwnershipModule -Force

function Get-MgsProcessSnapshot {
    @(Get-CimInstance Win32_Process | Where-Object { $_.ProcessId -and $_.CreationDate })
}
function Get-MgsTcpSnapshot {
    try { return @(Get-NetTCPConnection -ErrorAction Stop) }
    catch { throw 'Unable to inspect current TCP peers; shared simulator ownership is unproven, refusing cleanup.' }
}
function Stop-MgsRecordedRuntimeProcess($Identity,$SessionRecord) {
    # Hold an OS process handle after rechecking all recorded identity fields;
    # Kill() then targets that process object, not a later PID reuse.
    $proc=$null
    try {
        $proc=[Diagnostics.Process]::GetProcessById([int]$Identity.pid)
        if($proc.HasExited){return}
        $current=Get-CimInstance Win32_Process -Filter "ProcessId=$([int]$Identity.pid)"
        if(!$current -or !(Test-MgsExactProcessIdentity $Identity $current)){
            throw "Recorded runtime PID $($Identity.pid) changed identity before cleanup; refusing to stop it."
        }
        $path=$proc.MainModule.FileName
        $recordedStart=ConvertTo-MgsUtcStartTime $Identity.startedUtc
        $startDeltaMs=[Math]::Abs(($proc.StartTime.ToUniversalTime().Ticks-$recordedStart.Ticks)/[double]([TimeSpan]::TicksPerMillisecond))
        $pathMatches=[string]::Equals([IO.Path]::GetFullPath($path),[string]$Identity.path,[StringComparison]::OrdinalIgnoreCase)
        if(!$pathMatches -or $startDeltaMs -gt 10){
            throw "Recorded runtime PID $($Identity.pid) changed start/path identity before cleanup; refusing to stop it."
        }
        # Close the audit-to-stop race as far as user-mode observation allows:
        # check loopback peers again immediately before terminating this exact
        # process handle. Any newly connected unrecorded client blocks cleanup.
        $latest=Get-MgsProcessSnapshot
        $latestSimulators=@($latest | Where-Object { ([string]$_.Name -ieq 'MetaXRSimulator.exe') -or [IO.Path]::GetFileName([string]$_.ExecutablePath) -ieq 'MetaXRSimulator.exe' })
        if($latestSimulators.Count){
            $latestTcp=Get-MgsTcpSnapshot
            [void](Get-MgsOrphanCleanupPlan -Record $SessionRecord -Processes $latest -TcpConnections $latestTcp -TcpSnapshotComplete $true)
        }
        $proc.Kill()
    } finally { if($proc){$proc.Dispose()} }
}
function Retire-MgsRecordedOrphanRuntime([string]$SessionRecord) {
    $snapshot=Get-MgsProcessSnapshot
    $record=$null
    if(Test-Path -LiteralPath $SessionRecord){try{$record=Get-Content -Raw -LiteralPath $SessionRecord|ConvertFrom-Json}catch{}}
    $simulators=@($snapshot | Where-Object { ([string]$_.Name -ieq 'MetaXRSimulator.exe') -or [IO.Path]::GetFileName([string]$_.ExecutablePath) -ieq 'MetaXRSimulator.exe' })
    $recordedLive=$false
    $recordedRuntime=@()
    if($record -and $record.PSObject.Properties['ownedRuntimeProcesses']){$recordedRuntime=@($record.ownedRuntimeProcesses)}
    foreach($expected in $recordedRuntime){
        if(@($snapshot | Where-Object { [int]$_.ProcessId -eq [int]$expected.pid }).Count){$recordedLive=$true;break}
    }
    if(!$simulators.Count -and !$recordedLive){return}
    $tcpSnapshot=@()
    $tcpComplete=$true
    if($simulators.Count){$tcpSnapshot=Get-MgsTcpSnapshot}
    $plan=@(Get-MgsOrphanCleanupPlan -Record $record -Processes $snapshot -TcpConnections $tcpSnapshot -TcpSnapshotComplete $tcpComplete)
    if(!$plan.Count){throw 'A simulator process is present but no exact exited-session cleanup target was found; refusing launch.'}
    foreach($owned in $plan){
        # A process may start connecting after the initial plan. Revalidate
        # current loopback peers immediately before each exact-PID stop.
        $beforeStop=Get-MgsProcessSnapshot
        $simulatorsBefore=@($beforeStop | Where-Object { ([string]$_.Name -ieq 'MetaXRSimulator.exe') -or [IO.Path]::GetFileName([string]$_.ExecutablePath) -ieq 'MetaXRSimulator.exe' })
        if($simulatorsBefore.Count){
            $tcpBefore=Get-MgsTcpSnapshot
            [void](Get-MgsOrphanCleanupPlan -Record $record -Processes $beforeStop -TcpConnections $tcpBefore -TcpSnapshotComplete $true)
        }
        Stop-MgsRecordedRuntimeProcess $owned $record
    }
    $deadline=[DateTime]::UtcNow.AddSeconds(5)
    do {
        $current=Get-MgsProcessSnapshot
        $left=@($current | Where-Object { ([string]$_.Name -ieq 'MetaXRSimulator.exe') -or [IO.Path]::GetFileName([string]$_.ExecutablePath) -ieq 'MetaXRSimulator.exe' })
        $ownedLeft=$false
        foreach($expected in $plan){
            $samePid=@($current | Where-Object { [int]$_.ProcessId -eq [int]$expected.pid })
            if($samePid.Count){
                if(!(Test-MgsExactProcessIdentity $expected $samePid[0])){throw "Runtime PID $($expected.pid) was reused during cleanup; refusing to continue launch."}
                $ownedLeft=$true
            }
        }
        if(!$left.Count -and !$ownedLeft){return}
        Start-Sleep -Milliseconds 100
    } while([DateTime]::UtcNow -lt $deadline)
    throw 'Recorded simulator cleanup did not finish; no game launch was attempted.'
}
function Wait-MgsGameProcess([string]$SessionRecord,[string]$RuntimeRoot,[DateTime]$LaunchUtc) {
    $mgsLaunchDeadline=[DateTime]::UtcNow.AddSeconds(30)
    $mgsGame=$null
    $mgsOwned=@()
    do {
        if(!$mgsGame){
            $mgsLaunched=@(Get-CimInstance Win32_Process -Filter "Name='mgsvtpp.exe'")
            if($mgsLaunched.Count -gt 1){throw 'More than one MGSV process appeared during launch; refusing to select an ambiguous session.'}
            if($mgsLaunched.Count -eq 1){
                $mgsGame=ConvertTo-MgsProcessIdentity $mgsLaunched[0]
                $mgsCreated=[DateTime]::Parse($mgsGame.startedUtc,[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind).ToUniversalTime()
                if($mgsCreated -lt $LaunchUtc.AddSeconds(-2)){throw 'Observed MGSV predates this launch request; refusing to record an unrelated game session.'}
            }
        }
        if($mgsGame){
            # Wait within the original 30-second launch deadline for both the
            # game's directly spawned Meta GUI and its runtime child. Never
            # mark an incomplete ownership tree as a successful launch record.
            $snapshot=Get-MgsProcessSnapshot
            $mgsOwned=@(Get-MgsOwnedRuntimeProcesses -Processes $snapshot -Game $mgsGame -RuntimeRoot $RuntimeRoot)
            $hasGui=@($mgsOwned|Where-Object role -eq 'meta_gui').Count -gt 0
            $hasChild=@($mgsOwned|Where-Object role -eq 'runtime_child').Count -gt 0
            if($hasGui -and $hasChild){
                $mgsRecord=$null
                if(Test-Path -LiteralPath $SessionRecord){try{$mgsRecord=Get-Content -Raw -LiteralPath $SessionRecord|ConvertFrom-Json}catch{}}
                if(!$mgsRecord){throw 'Steam launch record disappeared before game ownership could be recorded.'}
                $mgsRecordMap=[ordered]@{}
                foreach($property in $mgsRecord.PSObject.Properties){$mgsRecordMap[$property.Name]=$property.Value}
                $mgsRecordMap.schema=2;$mgsRecordMap.game=$mgsGame;$mgsRecordMap.ownedRuntimeProcesses=$mgsOwned
                [IO.File]::WriteAllText($SessionRecord,($mgsRecordMap|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
                $mgsOwned | ConvertTo-Json -Compress -Depth 5 | Write-Output
                Write-Output 'MGSV and its exact Meta simulator process tree are recorded. A fresh compositor capture is still required to verify the VR session.'
                return
            }
        }
        Start-Sleep -Milliseconds 250
    } while([DateTime]::UtcNow -lt $mgsLaunchDeadline)
    if($mgsGame){throw 'MGSV started, but its directly spawned Meta GUI and runtime child were not both identified within 30 seconds; ownership was not recorded, and no further launch should be attempted until the simulator state is inspected.'}
    $mgsLeftover=Get-CimInstance Win32_Process -Filter "Name='MetaXRSimulator.exe'"
    if($mgsLeftover){
        $mgsLeftover | Select-Object ProcessId,ParentProcessId,ExecutablePath,CreationDate | ConvertTo-Json -Compress | Write-Output
    }
    throw 'Steam accepted the request but MGSV did not start within 30 seconds. Inspect the reported simulator ownership before recovering an orphan. No input runner should start.'
}
$mgsRuntimePath=(Resolve-Path -LiteralPath $RuntimeManifest).Path
$mgsRuntime=Get-Content -Raw -LiteralPath $mgsRuntimePath | ConvertFrom-Json
if(!$mgsRuntime.runtime.library_path){throw 'Not an OpenXR runtime manifest'}
$mgsOperatorPath=$null
if($OperatorDir){
    $mgsOperatorPath=(Resolve-Path -LiteralPath $OperatorDir).Path
    if(!(Test-Path -LiteralPath (Join-Path $mgsOperatorPath 'XrApiLayer_METAX_operator.json'))){throw 'Meta XR Operator manifest not found'}
}
if(Get-Process mgsvtpp -ErrorAction SilentlyContinue){throw 'MGSV is already running'}
$mgsSteamPath=(Get-ItemProperty -LiteralPath 'HKCU:\Software\Valve\Steam').SteamPath
$mgsSteamExe=Join-Path $mgsSteamPath 'steam.exe'
$mgsSessionRecord=Join-Path (Split-Path -Parent $PSScriptRoot) 'artifacts\simulator-steam-session.json'
Retire-MgsRecordedOrphanRuntime $mgsSessionRecord
$mgsRuntimeRoot=Split-Path -Parent $mgsRuntimePath
if(Get-Process steam -ErrorAction SilentlyContinue){
    if($RestartSteam){
        Start-Process -FilePath $mgsSteamExe -ArgumentList '-shutdown' -WindowStyle Hidden
        $mgsDeadline=[DateTime]::UtcNow.AddSeconds(25)
        while(Get-Process steam -ErrorAction SilentlyContinue){
            if([DateTime]::UtcNow -gt $mgsDeadline){throw 'Steam has not exited; no game launch was attempted'}
            Start-Sleep -Milliseconds 250
        }
    }else{
        # A fresh caller's environment never replaces the running client's.
        # Require evidence that this exact Steam process inherited our runtime
        # and operator settings, rather than launching a flat game and timing out.
        $mgsSteamProcess=Get-Process steam -ErrorAction Stop
        $mgsRecorded=$null
        if(Test-Path -LiteralPath $mgsSessionRecord){
            try{$mgsRecorded=Get-Content -Raw -LiteralPath $mgsSessionRecord | ConvertFrom-Json}catch{}
        }
        $mgsRecordedStart=if($mgsRecorded){(ConvertTo-MgsUtcStartTime $mgsRecorded.started_utc).ToString('o')}else{$null}
        if(!$mgsRecorded -or @($mgsSteamProcess).Count -ne 1 -or
           $mgsRecorded.pid -ne $mgsSteamProcess.Id -or
           $mgsRecordedStart -ne $mgsSteamProcess.StartTime.ToUniversalTime().ToString('o') -or
           $mgsRecorded.runtime -ine $mgsRuntimePath -or $mgsRecorded.operator -ine $mgsOperatorPath){
            throw 'The running Steam process has no matching simulator launch record. Close the game, then use -RestartSteam to inherit this runtime/operator environment. No game was launched.'
        }
        $mgsLaunchUtc=[DateTime]::UtcNow
        Start-Process -FilePath $mgsSteamExe -ArgumentList '-applaunch','287700' -WindowStyle Hidden
        Write-Output 'Launching MGSV through the running Steam client; its existing XR environment is retained.'
        Wait-MgsGameProcess $mgsSessionRecord $mgsRuntimeRoot $mgsLaunchUtc
        return
    }
}
# Steam forwards launches to an existing client; that client does not inherit
# the new caller's environment. Set these before starting a fresh Steam client.
# No OpenXR registry setting is changed.
$mgsNames=@('XR_RUNTIME_JSON','XR_API_LAYER_PATH','XR_ENABLE_API_LAYERS','OPENXR_SIMULATOR_HEADLESS')
$mgsPrevious=@{}
foreach($mgsName in $mgsNames){$mgsPrevious[$mgsName]=[Environment]::GetEnvironmentVariable($mgsName,'Process')}
try{
    $env:XR_RUNTIME_JSON=$mgsRuntimePath
    [Environment]::SetEnvironmentVariable('XR_API_LAYER_PATH',$mgsOperatorPath,'Process')
    [Environment]::SetEnvironmentVariable('XR_ENABLE_API_LAYERS',$(if($mgsOperatorPath){'XR_APILAYER_METAX_operator'}else{$null}),'Process')
    [Environment]::SetEnvironmentVariable('OPENXR_SIMULATOR_HEADLESS',$(if($Headless){'1'}else{$null}),'Process')
    $mgsLaunchUtc=[DateTime]::UtcNow
    Start-Process -FilePath $mgsSteamExe -ArgumentList '-applaunch','287700' -WindowStyle Hidden
    $mgsClientDeadline=[DateTime]::UtcNow.AddSeconds(10)
    do{
        $mgsClient=Get-Process steam -ErrorAction SilentlyContinue
        if($mgsClient){break}
        Start-Sleep -Milliseconds 100
    }while([DateTime]::UtcNow -lt $mgsClientDeadline)
    if(@($mgsClient).Count -eq 1){
        $mgsClientRecord=@{schema=1;pid=$mgsClient.Id;started_utc=$mgsClient.StartTime.ToUniversalTime().ToString('o');runtime=$mgsRuntimePath;operator=$mgsOperatorPath}
        [IO.Directory]::CreateDirectory((Split-Path -Parent $mgsSessionRecord)) | Out-Null
        [IO.File]::WriteAllText($mgsSessionRecord,($mgsClientRecord|ConvertTo-Json),[Text.UTF8Encoding]::new($false))
    }
    Write-Output "Requested MGSV with OpenXR runtime: $mgsRuntimePath"
    if($mgsOperatorPath){Write-Output 'Meta XR Operator layer enabled for this launch.'}
    Wait-MgsGameProcess $mgsSessionRecord $mgsRuntimeRoot $mgsLaunchUtc
}finally{
    foreach($mgsName in $mgsNames){[Environment]::SetEnvironmentVariable($mgsName,$mgsPrevious[$mgsName],'Process')}
}
