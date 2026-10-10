param(
    [Parameter(Mandatory=$true)][string]$GameDir,
    [ValidateSet('afgh','mafr')][string]$Location = 'afgh',
    [string[]]$Blocks = @(),
    [switch]$IncludeEmplacementLocators,
    [string]$Output = '',
    [string]$Reader = ''
)
$ErrorActionPreference = 'Stop'
$mgsRoot = Split-Path $PSScriptRoot -Parent
$mgsPrivate = [IO.Path]::GetFullPath((Join-Path $mgsRoot 'private'))
if (!$Output) { $Output = Join-Path $mgsPrivate "native-navigation/$Location" }
$Output = [IO.Path]::GetFullPath($Output)
if (!$Output.StartsWith($mgsPrivate + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Owned navigation output must remain inside this checkout/private/.'
}
if (!$Reader) { $Reader = Join-Path $mgsRoot '.deps/gzstool' }
$Reader = [IO.Path]::GetFullPath($Reader)
foreach ($mgsDependency in @('GzsTool.Core.dll','CityHash.dll','qar_dictionary.txt','fpk_dictionary.txt')) {
    if (!(Test-Path -LiteralPath (Join-Path $Reader $mgsDependency) -PathType Leaf)) {
        throw "Missing archive reader dependency: $mgsDependency"
    }
}
foreach ($mgsBlock in $Blocks) {
    if ($mgsBlock -notmatch '^\d{3}_\d{3}$') { throw 'Blocks must use the native NNN_NNN identity.' }
}
[Reflection.Assembly]::LoadFrom((Join-Path $Reader 'CityHash.dll')) | Out-Null
[Reflection.Assembly]::LoadFrom((Join-Path $Reader 'GzsTool.Core.dll')) | Out-Null
[GzsTool.Core.Utility.Hashing]::ReadDictionary((Join-Path $Reader 'qar_dictionary.txt'))
[GzsTool.Core.Utility.Hashing]::ReadMd5Dictionary((Join-Path $Reader 'fpk_dictionary.txt'))
New-Item -ItemType Directory -Path $Output -Force | Out-Null
$mgsManifest = [Collections.Generic.List[object]]::new()
$mgsPacks = [Collections.Generic.List[object]]::new()
$mgsSeen = @{}
$mgsNativePaths = @{}
$mgsNavigationCount = 0
$mgsLocatorCount = 0
$mgsMd5 = [Security.Cryptography.MD5]::Create()
$mgsPackPattern = '^/Assets/tpp/pack/location/' + $Location + '/pack_small/\d{3}/' + $Location + '_(\d{3}_\d{3})\.fpk$'
try {
    # Decode only indexed map packs. Texture archives and unrelated assets are
    # never unpacked. Stream/decode buffers expire after each selected pack.
    foreach ($mgsArchive in (Get-ChildItem -LiteralPath (Join-Path $GameDir 'master') -Filter '*.dat' -File | Sort-Object Name)) {
        if ($mgsArchive.Name -like 'texture*') { continue }
        $mgsInput = [IO.File]::Open($mgsArchive.FullName,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
        try {
            $mgsMagic = [byte[]]::new(4)
            if ($mgsInput.Read($mgsMagic,0,4) -ne 4 -or [Text.Encoding]::ASCII.GetString($mgsMagic) -ne 'SQAR') { continue }
            $mgsInput.Position = 0
            $mgsQar = [GzsTool.Core.Qar.QarFile]::new()
            $mgsQar.Read($mgsInput)
            foreach ($mgsEntry in $mgsQar.Entries) {
                if ($mgsEntry.FilePath -notmatch $mgsPackPattern) { continue }
                $mgsBlock = $Matches[1]
                if ($Blocks.Count -and $mgsBlock -notin $Blocks) { continue }
                $mgsHash = $mgsEntry.Hash.ToString('x16')
                if ($mgsSeen.ContainsKey($mgsHash)) { throw "Duplicate native package revision: $mgsHash" }
                $mgsSeen[$mgsHash] = $true
                # GzsTool 0.5.3 names the two TPP v1 size properties backwards.
                # Verify the actual owned header instead of trusting labels.
                # This is the same v1 layout validated by import-owned-assets.cs.
                if ($mgsEntry.DataOffset -lt 32 -or $mgsEntry.DataOffset -gt $mgsInput.Length) { throw 'Native package offset is invalid.' }
                $mgsHeader = [byte[]]::new(32)
                $mgsInput.Position = $mgsEntry.DataOffset - 32
                if ($mgsInput.Read($mgsHeader,0,32) -ne 32) { throw 'Short native package header.' }
                $mgsLow = [uint64]([BitConverter]::ToUInt32($mgsHeader,0) -bxor [uint32]0x41441043)
                $mgsHigh = [uint64]([BitConverter]::ToUInt32($mgsHeader,4) -bxor [uint32]0x41441043)
                if (($mgsLow -bor ($mgsHigh -shl 32)) -ne $mgsEntry.Hash) { throw 'Native package header hash differs from its index.' }
                $mgsStored = [BitConverter]::ToUInt32($mgsHeader,8) -bxor [uint32]0x11c22050
                $mgsExpected = [BitConverter]::ToUInt32($mgsHeader,12) -bxor [uint32]3495299267 # 0xd05608c3
                if ($mgsExpected -le 0 -or $mgsExpected -gt 134217728 -or
                    $mgsStored -lt 4 -or $mgsStored -gt 134217728 -or $mgsStored -gt $mgsInput.Length - $mgsEntry.DataOffset) {
                    throw "Native package exceeds the bounded read limit: $($mgsEntry.FilePath)"
                }
                $mgsPayload = $mgsEntry.Export($mgsInput).DataStream.Invoke()
                try {
                    if ($mgsPayload.Length -ne $mgsExpected) { throw 'Decoded native package size mismatch.' }
                    $mgsFpk = [GzsTool.Core.Fpk.FpkFile]::new()
                    $mgsFpk.Read($mgsPayload)
                    $mgsFound = 0
                    foreach ($mgsFile in $mgsFpk.Entries) {
                        $mgsNavigation = $mgsFile.FilePath -match ('^/Assets/tpp/level/location/' + $Location + '/.*\.nav2$')
                        $mgsEmplacement = $IncludeEmplacementLocators -and $mgsFile.FilePath -match ('^/Assets/tpp/level/location/' + $Location + '/.*/lba/hw(?:00|03)[^/]*\.lba$')
                        if (!$mgsNavigation -and !$mgsEmplacement) { continue }
                        if ($mgsFile.DataSize -le 0 -or $mgsFile.DataSize -gt 16777216 -or
                            $mgsFile.DataOffset -gt $mgsPayload.Length -or
                            $mgsFile.DataSize -gt $mgsPayload.Length - $mgsFile.DataOffset) { throw 'Native navigation extent is invalid.' }
                        $mgsPathHash = $mgsMd5.ComputeHash([Text.Encoding]::GetEncoding(1252).GetBytes($mgsFile.FilePath))
                        if ([Convert]::ToBase64String($mgsPathHash) -cne [Convert]::ToBase64String($mgsFile.Md5Hash)) { throw 'Native navigation path MD5 mismatch.' }
                        $mgsTargetDirectory = $Output
                        if ($mgsEmplacement) {
                            $mgsTargetDirectory = Join-Path $Output $mgsBlock
                            New-Item -ItemType Directory -Path $mgsTargetDirectory -Force | Out-Null
                        }
                        $mgsTarget = Join-Path $mgsTargetDirectory ([IO.Path]::GetFileName($mgsFile.FilePath))
                        $mgsData = $mgsFile.Export($mgsPayload).DataStream.Invoke()
                        try {
                            $mgsBytes = [byte[]]::new($mgsFile.DataSize)
                            $mgsDone = 0
                            while ($mgsDone -lt $mgsBytes.Length) {
                                $mgsRead = $mgsData.Read($mgsBytes,$mgsDone,$mgsBytes.Length-$mgsDone)
                                if ($mgsRead -le 0) { throw 'Short native navigation read.' }
                                $mgsDone += $mgsRead
                            }
                        } finally { $mgsData.Dispose() }
                        $mgsSha = [Security.Cryptography.SHA256]::Create()
                        try { $mgsDigest = ([BitConverter]::ToString($mgsSha.ComputeHash($mgsBytes))).Replace('-','').ToLowerInvariant() }
                        finally { $mgsSha.Dispose() }
                        if ($mgsNativePaths.ContainsKey($mgsFile.FilePath)) { throw 'Duplicate native navigation path.' }
                        $mgsNativePaths[$mgsFile.FilePath] = $mgsDigest
                        if (Test-Path -LiteralPath $mgsTarget) {
                            if ((Get-FileHash -LiteralPath $mgsTarget -Algorithm SHA256).Hash.ToLowerInvariant() -cne $mgsDigest) { throw 'Existing native navigation revision differs.' }
                        } else { [IO.File]::WriteAllBytes($mgsTarget,$mgsBytes) }
                        $mgsManifest.Add([pscustomobject]@{archive=$mgsArchive.FullName;pack=$mgsEntry.FilePath;pack_hash=$mgsHash;path=$mgsFile.FilePath;file=$mgsTarget;sha256=$mgsDigest;bytes=$mgsBytes.Length;path_md5_verified=$true})
                        if ($mgsNavigation) { $mgsNavigationCount++ }
                        if ($mgsEmplacement) { $mgsLocatorCount++ }
                        $mgsFound++
                    }
                    $mgsPacks.Add([pscustomobject]@{archive=$mgsArchive.FullName;path=$mgsEntry.FilePath;hash=$mgsHash;block=$mgsBlock;navigation_files=$mgsFound})
                } finally { $mgsPayload.Dispose() }
                if ($mgsPacks.Count % 25 -eq 0) { Write-Output "Indexed native packs read: $($mgsPacks.Count); NAV2 files: $mgsNavigationCount" }
            }
        } finally { $mgsInput.Dispose() }
    }
    if (!$mgsManifest.Count) { throw 'No indexed native navigation files found.' }
    if ($Blocks.Count) {
        foreach ($mgsBlock in $Blocks) { if ($mgsBlock -notin @($mgsPacks | ForEach-Object block)) { throw "Requested native block missing: $mgsBlock" } }
    }
    $mgsUtf8 = [Text.UTF8Encoding]::new($false)
    [IO.File]::WriteAllText((Join-Path $Output 'manifest.json'),(ConvertTo-Json -InputObject @($mgsManifest.ToArray()) -Depth 4),$mgsUtf8)
    [IO.File]::WriteAllText((Join-Path $Output 'import.json'),(ConvertTo-Json -Depth 5 -InputObject @{
        schema=1;location=$Location;indexed_small_packs=$mgsPacks.Count;navigation_files=$mgsNavigationCount;emplacement_locator_files=$mgsLocatorCount;selected_blocks=$Blocks;
        packs=@($mgsPacks.ToArray());reader_sha256=(Get-FileHash -LiteralPath (Join-Path $Reader 'GzsTool.Core.dll') -Algorithm SHA256).Hash.ToLowerInvariant();
        scope='Owned indexed small-block NAV2 import; player clearance, vehicle roads, other package classes and whole-game acceptance remain unproven.'
    }),$mgsUtf8)
    Write-Output "Imported $mgsNavigationCount native $Location NAV2 files and $mgsLocatorCount emplacement locator files from $($mgsPacks.Count) indexed small-block packs."
} finally { $mgsMd5.Dispose() }
