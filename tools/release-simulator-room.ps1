param(
    [Parameter(Mandatory=$true)][string]$Record,
    [Parameter(Mandatory=$true)][string]$RuntimeRoot,
    [Parameter(Mandatory=$true)][string]$Status
)
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'simulator-session-ownership.psm1') -Force
$started=[DateTime]::UtcNow
$result=[ordered]@{schema=1;state='pending';started_utc=$started.ToString('o');record=$Record}
function Write-MgsRoomStatus {
    [IO.File]::WriteAllText($Status,($result|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
}
try {
    $identity=Get-Content -LiteralPath $Record -Raw | ConvertFrom-Json
    $result.game=$identity.game
    Write-MgsRoomStatus
    # Every retry re-reads process identities and the entire peer graph.
    # The helper cannot stop another generation or a shared simulator.
    Stop-MgsOpaqueVrEnvironment -Record $identity -RuntimeRoot $RuntimeRoot -StartupPeerWaitSeconds 180
    $result.state='released'
} catch {
    $result.state='refused'
    $result.reason=$_.Exception.Message
} finally {
    $result.finished_utc=[DateTime]::UtcNow.ToString('o')
    Write-MgsRoomStatus
}
if($result.state -eq 'refused'){exit 1}
