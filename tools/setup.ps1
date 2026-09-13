param(
    [ValidateSet('Install','Uninstall')][string]$Mode = 'Install',
    [string]$GameExe,
    [switch]$TheatrePreview
)
$ErrorActionPreference = 'Stop'

Write-Host "MGS5VR - $Mode"
Write-Host 'Close MGSV, then select mgsvtpp.exe or MgsGroundZeroes.exe in its game folder.'
Write-Host 'In Steam: right-click MGSV > Manage > Browse local files.'

if (-not $GameExe) {
    Add-Type -AssemblyName System.Windows.Forms
    $mgsDialog = New-Object System.Windows.Forms.OpenFileDialog
    try {
        $mgsDialog.Title = "MGS5VR $Mode - select your game executable"
        $mgsDialog.Filter = 'MGSV (TPP or Ground Zeroes)|mgsvtpp.exe;MgsGroundZeroes.exe'
        $mgsDialog.CheckFileExists = $true
        $mgsDialog.Multiselect = $false
        $mgsSteam = Get-ItemProperty -LiteralPath 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue
        if ($mgsSteam -and $mgsSteam.SteamPath) {
            $mgsCommon = Join-Path $mgsSteam.SteamPath 'steamapps\common'
            if (Test-Path -LiteralPath $mgsCommon -PathType Container) {
                $mgsDialog.InitialDirectory = $mgsCommon
            }
        }
        if ($mgsDialog.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) {
            Write-Host 'Cancelled. Nothing changed.'
            return
        }
        $GameExe = $mgsDialog.FileName
    } finally {
        $mgsDialog.Dispose()
    }
}

$mgsSelectedExe = (Resolve-Path -LiteralPath $GameExe).Path
if (-not (Test-Path -LiteralPath $mgsSelectedExe -PathType Leaf) -or
    [IO.Path]::GetFileName($mgsSelectedExe) -inotIn @('mgsvtpp.exe','MgsGroundZeroes.exe')) {
    throw 'Select mgsvtpp.exe or MgsGroundZeroes.exe in its game folder.'
}
$mgsSelectedDir = Split-Path -Parent $mgsSelectedExe
if ($Mode -eq 'Install') {
    if ($TheatrePreview) {
        & (Join-Path $PSScriptRoot 'install.ps1') -GameDir $mgsSelectedDir -EnableTheatrePreview
    } else {
        & (Join-Path $PSScriptRoot 'install.ps1') -GameDir $mgsSelectedDir -EnableVR
    }
} else {
    & (Join-Path $PSScriptRoot 'uninstall.ps1') -GameDir $mgsSelectedDir
}
