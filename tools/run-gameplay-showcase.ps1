param(
    [string]$GameDir,
    [string]$Proxy = 'D:\code\gta-iv\out-openxr\external\meta-xr-operator-standalone-205.1\extracted\meta-xr-operator-standalone-public\windows\meta-xr-operator-mcp-proxy.exe',
    [string]$RuntimeManifest = 'C:\Program Files\MetaXRSimulator\v207.0\meta_openxr_simulator.json',
    [string]$Suite,
    [string]$Output,
    [switch]$Launch,
    [switch]$Campaign
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $GameDir) {
    $launcherSettings = Get-ItemProperty -LiteralPath 'HKCU:\Software\Nikami\MGS5VR\Launcher' -ErrorAction SilentlyContinue
    if ($launcherSettings -and $launcherSettings.TppExe) {
        $GameDir = Split-Path -Parent $launcherSettings.TppExe
    }
}
if (-not $GameDir) { throw 'Pass -GameDir with the folder containing mgsvtpp.exe.' }
if (-not $Suite) { $Suite = Join-Path $repoRoot 'tools\gameplay_bot\suites\field-context-roundtrip.json' }
if (-not $Output) {
    $takeId = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
    $Output = Join-Path $repoRoot ('artifacts\showcase\' + $takeId)
}
$gamePath = (Resolve-Path -LiteralPath $GameDir).Path
foreach ($required in @((Join-Path $gamePath 'mgsvtpp.exe'), $Proxy, $Suite,
                         (Join-Path $repoRoot 'build\Release\mgs5vr_controls.exe'),
                         (Join-Path $repoRoot 'build\Release\mgs5vr_audio_capture.exe'))) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required file is missing: $required" }
}
$proxyPath = (Resolve-Path -LiteralPath $Proxy).Path
$suitePath = (Resolve-Path -LiteralPath $Suite).Path
$outputPath = [IO.Path]::GetFullPath($Output)
if (Test-Path -LiteralPath $outputPath) { throw 'Use a fresh output directory; a previous take is never overwritten.' }

# Launch is opt-in because normal game/simulator windows will appear. Input
# always stays inside the maintained OpenXR runner; no desktop/focus APIs.
if (-not (Get-Process -Name mgsvtpp -ErrorAction SilentlyContinue)) {
    if (-not $Launch) { throw 'MGSV is not running. Start it normally, or use -Launch when game windows may appear.' }
    if (-not (Test-Path -LiteralPath $RuntimeManifest -PathType Leaf)) { throw 'The simulator runtime manifest is missing.' }
    & (Join-Path $PSScriptRoot 'launch-steam-simulator.ps1') -RuntimeManifest $RuntimeManifest -OperatorDir (Split-Path -Parent $proxyPath)
    $gameDeadline = [DateTime]::UtcNow.AddSeconds(60)
    while (-not (Get-Process -Name mgsvtpp -ErrorAction SilentlyContinue)) {
        if ([DateTime]::UtcNow -ge $gameDeadline) { throw 'The Steam launch did not produce a game process within 60 seconds.' }
        Start-Sleep -Milliseconds 100
    }
}

# Startup, field arrival and every case share one operator connection/lease.
# Python verifies the exact process path/build and configured controls again.
if ($Campaign) {
    & python (Join-Path $PSScriptRoot 'gameplay-bot.py') campaign --game-dir $gamePath --proxy $proxyPath --output $outputPath --record
} else {
    & python (Join-Path $PSScriptRoot 'gameplay-bot.py') session --game-dir $gamePath --proxy $proxyPath --suite $suitePath --output $outputPath --record
}
if ($LASTEXITCODE -ne 0) { throw "The recorded session needs attention; results are retained at $outputPath" }
Write-Output "Recorded session: $outputPath"
