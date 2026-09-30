param([Parameter(Mandatory=$true)][string]$RequestPath)
$ErrorActionPreference='Stop'
Import-Module Microsoft.PowerShell.Utility
Import-Module Microsoft.PowerShell.Management
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'settings-store.psm1') -Force
$root=Split-Path -Parent $PSScriptRoot
$checker=Join-Path $root 'mgs5vr_controls.exe'
if (!(Test-Path -LiteralPath $checker)) { $checker=Join-Path $root 'build/Release/mgs5vr_controls.exe' }
$request=Get-Content -LiteralPath $RequestPath -Raw | ConvertFrom-Json
$game=[IO.Path]::GetFullPath([string]$request.gameExe)
if ([IO.Path]::GetFileName($game) -ine 'mgsvtpp.exe' -and [IO.Path]::GetFileName($game) -ine 'MgsGroundZeroes.exe') { throw 'Choose the game executable.' }
if (!(Test-Path -LiteralPath $game -PathType Leaf)) { throw 'The selected game executable is missing.' }
$dir=Split-Path -Parent $game
$controlsPath=Join-Path $dir 'mgs5vr-controls.ini'
$runtimePath=Join-Path $dir 'mgs5vr.ini'
function Revision([string]$value) {
    $hash=[Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($hash.ComputeHash([Text.Encoding]::UTF8.GetBytes($value)))).Replace('-','').ToLowerInvariant() } finally { $hash.Dispose() }
}
function Read-Checker([string]$mode,[string]$path) {
    $result=if ($path) { & $checker $mode $path 2>&1 } else { & $checker $mode 2>&1 }
    if ($LASTEXITCODE -ne 0) { throw ($result -join "`n") }
    return ($result -join "`n") | ConvertFrom-Json
}
function Snapshot {
    $hasControls=Test-Path -LiteralPath $controlsPath
    $controls=if ($hasControls) { [IO.File]::ReadAllText($controlsPath) } else { [IO.File]::ReadAllText((Join-Path $root 'config/mgs5vr-controls.ini')) }
    $values=Get-MgsIni ([IO.File]::ReadAllText((Join-Path $root 'config/mgs5vr-controls.ini')))
    foreach ($entry in (Get-MgsIni $controls).GetEnumerator()) { $values[$entry.Key]=$entry.Value }
    $path=if($hasControls){$controlsPath}else{''}
    $runtime=[IO.File]::ReadAllText((Join-Path $root 'config/mgs5vr.ini'))
    $runtimeValues=Get-MgsIni $runtime
    if (Test-Path -LiteralPath $runtimePath) {
        $runtime=[IO.File]::ReadAllText($runtimePath)
        foreach ($entry in (Get-MgsIni $runtime).GetEnumerator()) { $runtimeValues[$entry.Key]=$entry.Value }
    }
    $schema=Read-Checker '--settings-json' $path
    $settings=@(foreach ($row in $schema) { [ordered]@{name=[string]$row.name;value=[double]$row.value;default=[double]$row.default;min=[double]$row.min;max=[double]$row.max} })
    $display=[ordered]@{requestedWidth=0;requestedHeight=0;actualWidth=0;actualHeight=0;live=$false}
    $displayPath=Join-Path $dir 'mgs5vr-display.ini'
    if (Test-Path -LiteralPath $displayPath) {
        $requested=Get-MgsIni ([IO.File]::ReadAllText($displayPath))
        if ($requested['display.enabled'] -eq '1') {
            $display.requestedWidth=[int]$requested['display.render_width']
            $display.requestedHeight=[int]$requested['display.render_height']
        }
    }
    $actualPath=Join-Path $dir 'mgs5vr-render-size.txt'
    if (Test-Path -LiteralPath $actualPath) {
        $actual=[IO.File]::ReadAllText($actualPath).Trim() -split '\s+'
        if ($actual.Count -eq 7 -and ($actual -join ' ') -match '^\d+( \d+){6}$') {
            $display.actualWidth=[int]$actual[2];$display.actualHeight=[int]$actual[3]
            $sourceProcess=Get-Process -Id ([int]$actual[0]) -ErrorAction SilentlyContinue
            $display.live=!!($sourceProcess -and $sourceProcess.Path -ieq $game -and ((Get-Date)-(Get-Item -LiteralPath $actualPath).LastWriteTime).TotalSeconds -lt 15)
        }
    }
    return [ordered]@{gameExe=$game;installed=(Test-Path -LiteralPath (Join-Path $dir 'mgs5vr-install.json'));writable=$hasControls;display=$display;
        controlsRevision=(Revision $controls);runtimeRevision=(Revision $runtime);values=$values;runtime=$runtimeValues;
        bindings=(Read-Checker '--bindings-json' $path);settings=$settings}
}
$backup=$null
if ($request.method -eq 'saveControls' -or $request.method -eq 'saveRuntime') {
    $isRuntime=$request.method -eq 'saveRuntime'
    $path=if($isRuntime){$runtimePath}else{$controlsPath}
    $original=[IO.File]::ReadAllText($path)
    if ((Revision $original) -cne [string]$request.revision) { throw 'Settings changed outside this launcher. Reload before saving; your changes have not been written.' }
    $allowed=Get-MgsIni ([IO.File]::ReadAllText((Join-Path $root $(if($isRuntime){'config/mgs5vr.ini'}else{'config/mgs5vr-controls.ini'}))))
    $updated=$original
    foreach ($change in $request.changes.PSObject.Properties) {
        if (!$allowed.Contains($change.Name)) { throw "Unknown setting: $($change.Name)" }
        $updated=Set-MgsIniValue $updated $change.Name ([string]$change.Value)
    }
    $backup=Save-MgsSettings $path $original $updated $checker -Runtime:$isRuntime
} elseif ($request.method -ne 'load') { throw 'Unsupported settings request.' }
$state=Snapshot
$state['backup']=$backup
$state | ConvertTo-Json -Depth 12 -Compress
