param(
    [Parameter(Mandatory=$true)][string]$Game,
    [string]$Output,
    [string]$GzsTool
)
$ErrorActionPreference = 'Stop'
if(-not $Output){$Output=Join-Path $PSScriptRoot '../private/item-catalog/scan.jsonl'}
if(-not $GzsTool){$GzsTool=Join-Path $PSScriptRoot '../.deps/gzstool'}
$mgsGameRoot = (Resolve-Path -LiteralPath $Game).Path
$mgsDependencyRoot = (Resolve-Path -LiteralPath $GzsTool).Path
foreach ($mgsDependency in 'CityHash.dll','Zlib.Portable.dll','GzsTool.Core.dll') {
    [Reflection.Assembly]::LoadFrom((Join-Path $mgsDependencyRoot $mgsDependency)) | Out-Null
}
[GzsTool.Core.Utility.Hashing]::ReadDictionary((Join-Path $mgsDependencyRoot 'qar_dictionary.txt'))
[GzsTool.Core.Utility.Hashing]::ReadMd5Dictionary((Join-Path $mgsDependencyRoot 'fpk_dictionary.txt'))
$mgsReferences = @((Join-Path $mgsDependencyRoot 'GzsTool.Core.dll'))
if (Test-Path -LiteralPath (Join-Path $PSHOME 'ref')) {
    $mgsReferences += @(Get-ChildItem -LiteralPath (Join-Path $PSHOME 'ref') -Filter '*.dll' -File | ForEach-Object { $_.FullName })
} else {
    $mgsReferences += @('System.dll','System.Core.dll')
}
Add-Type -Path (Join-Path $PSScriptRoot 'catalog-owned-assets.cs') -ReferencedAssemblies $mgsReferences
$mgsOutputPath = [IO.Path]::GetFullPath($Output)
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($mgsOutputPath)) | Out-Null
# Metadata stays private. This does not export game models, clips, scripts or textures.
[MgsOwnedCatalog]::Scan($mgsGameRoot, $mgsOutputPath)
