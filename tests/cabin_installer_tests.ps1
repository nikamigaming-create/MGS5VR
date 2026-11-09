$ErrorActionPreference='Stop'
Import-Module Microsoft.PowerShell.Utility
Import-Module Microsoft.PowerShell.Management
$fixtureBuild=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\build'))
$fixtureRoot=Join-Path $fixtureBuild ('cabin-installer-tests-'+[Guid]::NewGuid().ToString('N'))
$fixtureInstaller=Join-Path $PSScriptRoot '..\tools\install-cabin-actors.ps1'
$fixtureFailures=0
function Expect([bool]$Value,[string]$Message){if(-not $Value){$script:fixtureFailures++;Write-Host "FAIL $Message"}}
# This suite operates only on synthetic paths; a live game elsewhere must not
# affect its transaction fixtures. It never sends input or terminates a process.
function Get-Process { param([string]$Name) @() }
function Invoke-Fixture([string]$Name,[string]$Mode,[bool]$MustFail){
    $case=Join-Path $fixtureRoot $Name
    $game=Join-Path $case 'game';$archive=Join-Path $game 'master\0\00.dat'
    [IO.Directory]::CreateDirectory((Split-Path -Parent $archive))|Out-Null
    [IO.File]::WriteAllText($archive,'original')
    $importer=Join-Path $case 'fake-importer.ps1'
    $fake=@'
$game=$args[1];$output=$args[2]
[IO.File]::WriteAllText($output,'augmented')
switch('__MODE__') {
 'failure' {$global:LASTEXITCODE=7;return}
 'concurrent' {[IO.File]::WriteAllText((Join-Path $game 'master\0\00.dat'),'concurrent-owner')}
 'stage_collision' {[IO.File]::WriteAllText((Join-Path $game 'master\0\00.dat.mgs5vr-cabin-stage'),'external-stage')}
 'receipt_stage_collision' {[IO.File]::WriteAllText((Join-Path $game 'mgs5vr-cabin-assets.json.mgs5vr-stage'),'external-receipt-stage')}
 'receipt_failure' {[IO.Directory]::CreateDirectory((Join-Path $game 'mgs5vr-cabin-assets.json'))|Out-Null}
}
$global:LASTEXITCODE=0
'@
    [IO.File]::WriteAllText($importer,$fake.Replace('__MODE__',$Mode))
    $failed=$false
    try {& $fixtureInstaller -GameDir $game -Importer $importer|Out-Null} catch {$failed=$true;Write-Host "$Name retained failure: $($_.Exception.Message)"}
    Expect ($failed -eq $MustFail) "$Name outcome"
    $expected=if($Mode -eq 'concurrent'){'concurrent-owner'}elseif($MustFail){'original'}else{'augmented'}
    Expect ([IO.File]::ReadAllText($archive) -eq $expected) "$Name archive preservation"
    if($Mode -eq 'stage_collision'){
        $stage=$archive+'.mgs5vr-cabin-stage'
        Expect ((Test-Path -LiteralPath $stage) -and [IO.File]::ReadAllText($stage) -eq 'external-stage') "$Name unrelated stage preserved"
    }else{Expect (-not (Test-Path -LiteralPath ($archive+'.mgs5vr-cabin-stage'))) "$Name stage cleanup"}
    Expect (-not (Test-Path -LiteralPath ($archive+'.mgs5vr-cabin-rollback'))) "$Name rollback cleanup"
    if($Mode -eq 'receipt_stage_collision'){
        $stage=Join-Path $game 'mgs5vr-cabin-assets.json.mgs5vr-stage'
        Expect ((Test-Path -LiteralPath $stage) -and [IO.File]::ReadAllText($stage) -eq 'external-receipt-stage') "$Name unrelated receipt stage preserved"
    }
    if(-not $MustFail){
        $receipt=Join-Path $game 'mgs5vr-cabin-assets.json';$before=[IO.File]::ReadAllText($receipt)
        & $fixtureInstaller -GameDir $game -Importer $importer -CheckOnly|Out-Null
        Expect ([IO.File]::ReadAllText($receipt) -eq $before) "$Name check-only preserves receipt"
        Expect ([IO.File]::ReadAllText($archive) -eq 'augmented') "$Name check-only preserves archive"
    }
}
try {
    [IO.Directory]::CreateDirectory($fixtureRoot)|Out-Null
    Invoke-Fixture 'install' 'success' $false
    Invoke-Fixture 'failed-preparation' 'failure' $true
    Invoke-Fixture 'concurrent-original-edit' 'concurrent' $true
    Invoke-Fixture 'concurrent-stage-owner' 'stage_collision' $true
    Invoke-Fixture 'concurrent-receipt-stage-owner' 'receipt_stage_collision' $true
    Invoke-Fixture 'receipt-publish-failure' 'receipt_failure' $true
} finally {
    # The unique root was created by this run. Resolve and verify containment
    # immediately before deleting its synthetic files; never use game paths.
    if(Test-Path -LiteralPath $fixtureRoot){
        $resolved=(Resolve-Path -LiteralPath $fixtureRoot).Path
        if(-not $resolved.StartsWith($fixtureBuild+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or
           (Split-Path -Leaf $resolved) -notmatch '^cabin-installer-tests-[0-9a-f]{32}$') {throw 'Refusing fixture cleanup outside build root'}
        Remove-Item -LiteralPath $resolved -Recurse
    }
}
Write-Host "Cabin installer transaction fixtures: $fixtureFailures failures. Synthetic files only."
if($fixtureFailures){exit 1}
