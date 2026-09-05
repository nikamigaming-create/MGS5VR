$ErrorActionPreference = 'Stop'
$mgsRoot = Split-Path -Parent $PSScriptRoot
$mgsDeps = Join-Path $mgsRoot '.deps'
New-Item -ItemType Directory -Force -Path $mgsDeps | Out-Null
$mgsSources = @(
    @{Name='OpenXR-SDK'; Url='https://github.com/KhronosGroup/OpenXR-SDK.git'; Tag='release-1.1.49'; Commit='977f6675bc0057d5a54ed290cb5c71c699b1c0ab'},
    @{Name='minhook'; Url='https://github.com/TsudaKageyu/minhook.git'; Tag='v1.3.4'; Commit='c3fcafdc10146beb5919319d0683e44e3c30d537'}
)
foreach ($mgsSource in $mgsSources) {
    $mgsDestination = Join-Path $mgsDeps $mgsSource.Name
    if (-not (Test-Path -LiteralPath $mgsDestination)) {
        & git clone --quiet --depth 1 --branch $mgsSource.Tag $mgsSource.Url $mgsDestination
        if ($LASTEXITCODE -ne 0) { throw "Dependency download failed: $($mgsSource.Name)" }
    }
    $mgsRevision = & git -C $mgsDestination rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $mgsRevision -ne $mgsSource.Commit) {
        throw "Unexpected dependency revision in $mgsDestination. Existing files were preserved."
    }
}
Write-Output 'Pinned build dependencies are ready.'
