param(
    [ValidateRange(3,15)][int]$DurationSeconds = 5,
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$cmake = Get-Command cmake -ErrorAction Stop
$source = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'background-desktop-probe.cpp')).Path
$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $OutputPath) {
    $OutputPath = Join-Path $repoRoot ('artifacts\background-probes\background-desktop-probe.'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')+'.json')
}
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
if (Test-Path -LiteralPath $OutputPath) { throw "Evidence output already exists; choose a fresh path: $OutputPath" }
New-Item -ItemType Directory -Path (Split-Path -Parent $OutputPath) -Force | Out-Null
$tempBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar,[IO.Path]::AltDirectorySeparatorChar)
$tempRoot = Join-Path $tempBase ('mgs5vr-background-probe-' + [Guid]::NewGuid().ToString('N'))
$tempRootFull = [IO.Path]::GetFullPath($tempRoot)
if ([IO.Path]::GetDirectoryName($tempRootFull).TrimEnd([IO.Path]::DirectorySeparatorChar,[IO.Path]::AltDirectorySeparatorChar) -ne $tempBase -or
    [IO.Path]::GetFileName($tempRootFull) -notmatch '^mgs5vr-background-probe-[0-9a-f]{32}$') {
    throw 'Temporary probe path failed its safety check.'
}

New-Item -ItemType Directory -Path $tempRootFull | Out-Null
$probeExitCode = $null
$failure = $null
$probeReport = $null
try {
    $buildRoot = Join-Path $tempRootFull 'build'
    $sourceCmake = $source.Replace('\','/')
    $cmakeText = @"
cmake_minimum_required(VERSION 3.24)
project(mgs5vr_background_probe LANGUAGES CXX)
add_executable(mgs5vr_background_probe [==[$sourceCmake]==])
target_compile_features(mgs5vr_background_probe PRIVATE cxx_std_17)
target_compile_definitions(mgs5vr_background_probe PRIVATE UNICODE _UNICODE)
target_link_libraries(mgs5vr_background_probe PRIVATE d3d11 dxgi user32)
"@
    [IO.File]::WriteAllText((Join-Path $tempRootFull 'CMakeLists.txt'),$cmakeText,[Text.UTF8Encoding]::new($false))

    & $cmake.Source -S $tempRootFull -B $buildRoot -G 'Visual Studio 17 2022' -A x64
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed with exit code $LASTEXITCODE." }
    & $cmake.Source --build $buildRoot --config Release --target mgs5vr_background_probe --parallel 2
    if ($LASTEXITCODE -ne 0) { throw "The D3D probe build failed with exit code $LASTEXITCODE." }

    $probeExe = Join-Path $buildRoot 'Release\mgs5vr_background_probe.exe'
    if (-not (Test-Path -LiteralPath $probeExe -PathType Leaf)) { throw 'The built D3D probe executable is missing.' }
    $reportPath = Join-Path $tempRootFull 'result.json'
    & $probeExe --run $DurationSeconds $reportPath
    $probeExitCode = $LASTEXITCODE
    if (Test-Path -LiteralPath $reportPath -PathType Leaf) { $probeReport = Get-Content -Raw -LiteralPath $reportPath | ConvertFrom-Json }
    if ($probeExitCode -ne 0) { throw "The isolated D3D probe did not pass; exit code $probeExitCode." }
} catch {
    $failure = $_
}
finally {
    $evidence = [ordered]@{
        wrapper='tools/background-desktop-probe.ps1'
        completedUtc=[DateTime]::UtcNow.ToString('o')
        passed=($null -eq $failure)
        probeExitCode=$probeExitCode
        error=$(if ($failure) { $failure.Exception.Message } else { $null })
        probe=$probeReport
    }
    [IO.File]::WriteAllText($OutputPath,($evidence | ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
    Write-Output "Evidence: $OutputPath"
    $resolvedTemp = [IO.Path]::GetFullPath($tempRootFull)
    $parent = [IO.Path]::GetDirectoryName($resolvedTemp).TrimEnd([IO.Path]::DirectorySeparatorChar,[IO.Path]::AltDirectorySeparatorChar)
    $leaf = [IO.Path]::GetFileName($resolvedTemp)
    if ($parent -eq $tempBase -and $leaf -match '^mgs5vr-background-probe-[0-9a-f]{32}$' -and (Test-Path -LiteralPath $resolvedTemp)) {
        Remove-Item -LiteralPath $resolvedTemp -Recurse -Force
    }
}
if ($failure) { throw $failure }
