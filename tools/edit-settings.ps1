param(
    [Parameter(Mandatory=$true)][string]$Path,
    [ValidateSet('Editor','List','Set','Validate')][string]$Mode='Editor',
    [string[]]$Setting,
    [switch]$Runtime
)
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'settings-store.psm1') -Force
$mgsRoot=Split-Path -Parent $PSScriptRoot
$mgsChecker=Join-Path $mgsRoot 'mgs5vr_controls.exe'
if (!(Test-Path -LiteralPath $mgsChecker)) { $mgsChecker=Join-Path $mgsRoot 'build\Release\mgs5vr_controls.exe' }
if (!(Test-Path -LiteralPath $mgsChecker)) { throw 'Extract the complete launcher package.' }
$mgsPath=(Resolve-Path -LiteralPath $Path).Path
$mgsOriginal=[IO.File]::ReadAllText($mgsPath)

function Get-MgsRows {
    if ($Runtime) {
        $defaultsPath=Join-Path $mgsRoot 'config\mgs5vr.ini'
        $values=Get-MgsIni ([IO.File]::ReadAllText($defaultsPath))
        $actual=Get-MgsIni $mgsOriginal
        foreach ($key in $actual.Keys) { $values[$key]=$actual[$key] }
        foreach ($key in $values.Keys) { [pscustomobject]@{name=$key;value=$values[$key];range='Restart game after saving'} }
    } else {
        $schema=& $mgsChecker --settings-json $mgsPath
        if ($LASTEXITCODE -ne 0) { throw 'Controls file is invalid; open Edit controls to correct it.' }
        foreach ($item in ($schema | ConvertFrom-Json)) {
            $value=[string]$item.value;$range="$($item.min)..$($item.max)"
            if ($item.name -eq 'settings.turn_mode') { $value=@('snap','native_smooth','off')[[int]$item.value];$range='snap / native_smooth / off' }
            if ($item.name -eq 'settings.hud_mode') { $value=@('full','binoculars_only','off')[[int]$item.value];$range='full / binoculars_only / off' }
            [pscustomobject]@{name=$item.name;value=$value;range=$range}
        }
    }
}
if ($Mode -eq 'Validate') {
    if ($Runtime) { Test-MgsRuntime $mgsOriginal } else { & $mgsChecker --check $mgsPath; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE } }
    exit 0
}
if ($Mode -eq 'List') { @(Get-MgsRows) | ConvertTo-Json -Depth 4; exit 0 }
if ($Mode -eq 'Set') {
    if (!$Setting) { throw 'Set needs -Setting section.key=value (one or more values).' }
    $mgsUpdated=$mgsOriginal
    foreach ($entry in $Setting) {
        $split=$entry.IndexOf('=');if ($split -lt 1) { throw 'Use section.key=value.' }
        $mgsUpdated=Set-MgsIniValue $mgsUpdated $entry.Substring(0,$split) $entry.Substring($split+1)
    }
    $mgsBackup=Save-MgsSettings $mgsPath $mgsOriginal $mgsUpdated $mgsChecker -Runtime:$Runtime
    Write-Output "Saved $mgsPath. Backup: $mgsBackup"
    exit 0
}

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()
$mgsWindow=[Windows.Forms.Form]::new();$mgsWindow.Text='MGS5VR - VR settings';$mgsWindow.Size=[Drawing.Size]::new(920,760)
$mgsWindow.MinimumSize=[Drawing.Size]::new(720,540);$mgsWindow.StartPosition='CenterScreen'
$mgsLayout=[Windows.Forms.TableLayoutPanel]::new();$mgsLayout.Dock='Fill';$mgsLayout.RowCount=3;$mgsLayout.ColumnCount=1
[void]$mgsLayout.RowStyles.Add([Windows.Forms.RowStyle]::new('Absolute',100))
[void]$mgsLayout.RowStyles.Add([Windows.Forms.RowStyle]::new('Percent',100))
[void]$mgsLayout.RowStyles.Add([Windows.Forms.RowStyle]::new('Absolute',90))
$mgsHelp=[Windows.Forms.Label]::new();$mgsHelp.Dock='Fill';$mgsHelp.Padding=[Windows.Forms.Padding]::new(12)
$mgsHelp.Text="Edit a value, then Save. Units are in each setting's name. Existing bindings are preserved.`r`nRelease controls for 2 seconds to apply. Runtime settings need a restart. handheld_menus=0: paused panel in 3D; 1: handheld opt-in.`r`niDroid: tap LEFT Menu; grips change tabs; sticks navigate/pan; A selects; B goes back. Hold B for 0.75 s to close.`r`n3D / screen: hold LEFT Menu + right B for 0.55 s. Release the chord before switching again."
$mgsGrid=[Windows.Forms.DataGridView]::new();$mgsGrid.Dock='Fill';$mgsGrid.AllowUserToAddRows=$false;$mgsGrid.AllowUserToDeleteRows=$false
$mgsGrid.RowHeadersVisible=$false;$mgsGrid.AutoSizeColumnsMode='Fill'
foreach ($column in @('Setting','Value','Allowed')) { [void]$mgsGrid.Columns.Add($column,$column) }
$mgsGrid.Columns[0].ReadOnly=$true;$mgsGrid.Columns[2].ReadOnly=$true
$mgsGrid.Columns[0].FillWeight=48;$mgsGrid.Columns[1].FillWeight=25;$mgsGrid.Columns[2].FillWeight=27
foreach ($row in (Get-MgsRows)) { [void]$mgsGrid.Rows.Add($row.name,$row.value,$row.range) }
$mgsFooter=[Windows.Forms.FlowLayoutPanel]::new();$mgsFooter.Dock='Fill'
$mgsSave=[Windows.Forms.Button]::new();$mgsSave.Text='Save settings';$mgsSave.Width=140
$mgsBindings=[Windows.Forms.Button]::new();$mgsBindings.Text='Button mappings';$mgsBindings.Width=140;$mgsBindings.Enabled=!$Runtime
$mgsRuntime=[Windows.Forms.Button]::new();$mgsRuntime.Text='Runtime / assets';$mgsRuntime.Width=140;$mgsRuntime.Enabled=!$Runtime
$mgsStatus=[Windows.Forms.Label]::new();$mgsStatus.Width=850;$mgsStatus.Height=45
foreach ($control in @($mgsSave,$mgsBindings,$mgsRuntime,$mgsStatus)) { [void]$mgsFooter.Controls.Add($control) }
foreach ($control in @($mgsHelp,$mgsGrid,$mgsFooter)) { [void]$mgsLayout.Controls.Add($control) }
[void]$mgsWindow.Controls.Add($mgsLayout)
$mgsSave.Add_Click({
    try {
        [void]$mgsGrid.EndEdit();$updated=$mgsOriginal
        foreach ($row in $mgsGrid.Rows) { $updated=Set-MgsIniValue $updated ([string]$row.Cells[0].Value) ([string]$row.Cells[1].Value) }
        $backup=Save-MgsSettings $mgsPath $mgsOriginal $updated $mgsChecker -Runtime:$Runtime
        $script:mgsOriginal=$updated;$mgsStatus.Text="Saved. Previous settings: $backup"
    } catch { $mgsStatus.Text=$_.Exception.Message }
})
$mgsBindings.Add_Click({ & (Join-Path $PSScriptRoot 'edit-controls.ps1') -Path $mgsPath })
$mgsRuntime.Add_Click({ & (Join-Path $PSScriptRoot 'edit-settings.ps1') -Path (Join-Path (Split-Path -Parent $mgsPath) 'mgs5vr.ini') -Runtime })
try { [void]$mgsWindow.ShowDialog() } finally { $mgsWindow.Dispose() }
