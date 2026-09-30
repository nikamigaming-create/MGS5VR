param([switch]$Apply)
Import-Module Microsoft.PowerShell.Utility
Import-Module Microsoft.PowerShell.Management
$ErrorActionPreference='Stop'
$mgsRoot=[IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$mgsDist=Join-Path $mgsRoot 'dist'
$mgsPlay=Join-Path $mgsRoot 'play\BUILD.json'
if(!(Test-Path -LiteralPath $mgsPlay)){throw 'Create and verify play/ before retiring old local packages.'}
$mgsArchive=Join-Path $mgsRoot 'artifacts\dev\retired-builds'
$mgsRows=@()
$mgsCandidates=@(Get-ChildItem -LiteralPath $mgsDist | Where-Object {
    ($_.PSIsContainer -and $_.Name -like 'MGS5VR-*' -and
       (Test-Path -LiteralPath (Join-Path $_.FullName 'dinput8.dll')) -and
       (Test-Path -LiteralPath (Join-Path $_.FullName 'README.md'))) -or
    (!$_.PSIsContainer -and $_.Name -like 'MGS5VR-*.zip*')
})
foreach($mgsItem in $mgsCandidates){
    $mgsPath=[IO.Path]::GetFullPath($mgsItem.FullName)
    if(!$mgsPath.StartsWith($mgsDist+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Cleanup target escaped dist.'}
    if($mgsItem.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Cleanup does not follow links.'}
    $mgsFiles=if($mgsItem.PSIsContainer){@(Get-ChildItem -LiteralPath $mgsPath -Recurse -Force)}else{@($mgsItem)}
    if(@($mgsFiles | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count){throw 'Cleanup tree contains a link.'}
    $mgsBytes=($mgsFiles | Where-Object {!$_.PSIsContainer} | Measure-Object Length -Sum).Sum
    $mgsRow=[pscustomobject][ordered]@{path=$mgsPath;bytes=$mgsBytes;dll_sha256=$null}
    if($mgsItem.PSIsContainer){$mgsRow.dll_sha256=(Get-FileHash -LiteralPath (Join-Path $mgsPath 'dinput8.dll')).Hash.ToLowerInvariant()}
    $mgsRows+=,$mgsRow
}
$mgsTotal=($mgsRows | Measure-Object bytes -Sum).Sum
Write-Output ('Retire '+$mgsRows.Count+' generated package copies; '+[math]::Round($mgsTotal/1GB,3)+' GiB. Unique historical DLLs and package manifests are retained.')
if(!$Apply){$mgsRows | ConvertTo-Json -Depth 4;return}
New-Item -ItemType Directory -Path $mgsArchive -Force | Out-Null
foreach($mgsRow in $mgsRows){
    $mgsPath=$mgsRow.path
    if($mgsRow.dll_sha256){
        $mgsDll=Join-Path $mgsArchive ($mgsRow.dll_sha256+'.dll')
        if(!(Test-Path -LiteralPath $mgsDll)){Copy-Item -LiteralPath (Join-Path $mgsPath 'dinput8.dll') -Destination $mgsDll}
        foreach($mgsName in @('candidate.json','BUILD.json','manifest.json','release-manifest.json','SHA256SUMS.txt')){
            $mgsMetadata=Join-Path $mgsPath $mgsName
            if(Test-Path -LiteralPath $mgsMetadata){Copy-Item -LiteralPath $mgsMetadata -Destination (Join-Path $mgsArchive ((Split-Path -Leaf $mgsPath)+'-'+$mgsName))}
        }
    }
}
[IO.File]::WriteAllText((Join-Path $mgsArchive 'inventory.json'),($mgsRows|ConvertTo-Json -Depth 4),[Text.UTF8Encoding]::new($false))
# Every absolute target and its whole tree were checked above. Use native
# PowerShell deletion end-to-end, and never touch captures or retail assets.
foreach($mgsRow in $mgsRows){Remove-Item -LiteralPath $mgsRow.path -Recurse -Force}
Write-Output 'Old local package copies retired. Current play/ and acceptance evidence preserved.'
