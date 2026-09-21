[CmdletBinding(PositionalBinding=$false)]
param(
    [Parameter(Mandatory=$true)][ValidateSet('Detect','Apply','Launch','Install','Update','Remove','Settings','Set','Validate')][string]$Action,
    [string]$GameExe,
    [ValidateSet('Current','Headset','Custom')][string]$Preset='Current',
    [ValidateRange(50,150)][int]$Scale=100,
    [int]$Width=0,[int]$Height=0,[string]$GraphicsConfig,
    [string[]]$Setting,[switch]$Runtime,[switch]$ConfirmRemove,
    [Parameter(ValueFromRemainingArguments=$true)][string[]]$AdditionalSettings
)
$ErrorActionPreference='Stop'
if ($AdditionalSettings) {
    if ($Action -ne 'Set') { throw 'Unexpected arguments; extra settings are only valid with -Action Set.' }
    # powershell.exe -File passes only the first native argument to an array
    # parameter. Collect the remaining section.key=value arguments explicitly.
    $Setting=@($Setting)+@($AdditionalSettings)
}
if ($Action -in @('Detect','Apply','Launch')) {
    & (Join-Path $PSScriptRoot 'launcher-display.ps1') -Mode $Action -GameExe $GameExe -Preset $Preset -Scale $Scale -Width $Width -Height $Height -GraphicsConfig $GraphicsConfig
} elseif ($Action -in @('Install','Update','Remove')) {
    if ($Action -eq 'Remove' -and !$ConfirmRemove) { throw 'Headless Remove requires -ConfirmRemove.' }
    & (Join-Path $PSScriptRoot 'launcher-maintenance.ps1') -Mode $Action -GameExe $GameExe
} else {
    if (!$GameExe -or !(Test-Path -LiteralPath $GameExe -PathType Leaf)) { throw 'Supply -GameExe for this game installation.' }
    $file=if ($Runtime) { 'mgs5vr.ini' } else { 'mgs5vr-controls.ini' }
    $path=Join-Path (Split-Path -Parent (Resolve-Path -LiteralPath $GameExe).Path) $file
    $mode=if ($Action -eq 'Settings') { 'List' } else { $Action }
    & (Join-Path $PSScriptRoot 'edit-settings.ps1') -Path $path -Mode $mode -Setting $Setting -Runtime:$Runtime
}
