param([switch]$RestartSteam)
$ErrorActionPreference='Stop'
$mgsActiveRuntime=(Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Khronos\OpenXR\1' -Name ActiveRuntime).ActiveRuntime
if(!$mgsActiveRuntime){throw 'No active headset OpenXR runtime is configured'}
# Start Steam with the user's real runtime and without the simulator's
# Operator layer. A running Steam client retains its launch environment.
& (Join-Path $PSScriptRoot 'launch-steam-simulator.ps1') -RuntimeManifest $mgsActiveRuntime -RestartSteam:$RestartSteam
