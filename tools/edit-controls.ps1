param([string]$Path, [switch]$ValidateOnly, [string]$PreviewPath)
$ErrorActionPreference='Stop'
$mgsRoot=Split-Path -Parent $PSScriptRoot
$mgsChecker=Join-Path $mgsRoot 'mgs5vr_controls.exe'
if (!(Test-Path -LiteralPath $mgsChecker)) { $mgsChecker=Join-Path $mgsRoot 'build\Release\mgs5vr_controls.exe' }
if (!(Test-Path -LiteralPath $mgsChecker)) { throw 'Extract the complete release package, including mgs5vr_controls.exe.' }
if ($ValidateOnly) {
    if (!$Path) { throw '-ValidateOnly requires -Path.' }
    & $mgsChecker --check $Path
    exit $LASTEXITCODE
}
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()
$mgsWindow=[Windows.Forms.Form]::new()
$mgsWindow.Text='MGS5VR - Controls editor (no game launch needed)'
$mgsWindow.Size=[Drawing.Size]::new(1100,850)
$mgsWindow.MinimumSize=[Drawing.Size]::new(800,650)
$mgsWindow.StartPosition='CenterScreen'
$mgsLayout=[Windows.Forms.TableLayoutPanel]::new()
$mgsLayout.Dock='Fill'; $mgsLayout.ColumnCount=1; $mgsLayout.RowCount=4
foreach ($mgsHeight in @(105,0,145,42)) {
    $mgsStyle=[Windows.Forms.RowStyle]::new()
    if ($mgsHeight) { $mgsStyle.SizeType='Absolute'; $mgsStyle.Height=$mgsHeight }
    else { $mgsStyle.SizeType='Percent'; $mgsStyle.Height=100 }
    [void]$mgsLayout.RowStyles.Add($mgsStyle)
}
$mgsHelp=[Windows.Forms.Label]::new()
$mgsHelp.Dock='Fill'; $mgsHelp.Padding=[Windows.Forms.Padding]::new(12)
$mgsHelp.Text="Choose the controls file beside the GAME's dinput8.dll. Errors appear here while you edit; invalid changes cannot be saved.`r`nPlain button = native hold. press(a) = one press. tap(b,300) / hold(b,300) share the SAME threshold.`r`nLonger chords consume simpler actions in the same mode. Missing keys keep defaults: explicitly disable actions you replace.`r`nThis is an external editor, not an in-game menu. Restart the game only after saving a valid layout."
$mgsText=[Windows.Forms.TextBox]::new()
$mgsText.Multiline=$true; $mgsText.AcceptsTab=$true; $mgsText.AcceptsReturn=$true
$mgsText.WordWrap=$false; $mgsText.ScrollBars='Both'; $mgsText.Dock='Fill'
$mgsText.Font=[Drawing.Font]::new('Consolas',11)
$mgsErrors=[Windows.Forms.TextBox]::new()
$mgsErrors.Multiline=$true; $mgsErrors.ReadOnly=$true; $mgsErrors.ScrollBars='Vertical'; $mgsErrors.Dock='Fill'
$mgsErrors.Font=[Drawing.Font]::new('Consolas',10)
$mgsButtons=[Windows.Forms.FlowLayoutPanel]::new(); $mgsButtons.Dock='Fill'
$mgsOpen=[Windows.Forms.Button]::new(); $mgsOpen.Text='Open game config'; $mgsOpen.Width=145
$mgsCheck=[Windows.Forms.Button]::new(); $mgsCheck.Text='Check now'; $mgsCheck.Width=100
$mgsSave=[Windows.Forms.Button]::new(); $mgsSave.Text='Save valid changes'; $mgsSave.Width=160; $mgsSave.Enabled=$false
foreach ($mgsButton in @($mgsOpen,$mgsCheck,$mgsSave)) { [void]$mgsButtons.Controls.Add($mgsButton) }
foreach ($mgsControl in @($mgsHelp,$mgsText,$mgsErrors,$mgsButtons)) { [void]$mgsLayout.Controls.Add($mgsControl) }
[void]$mgsWindow.Controls.Add($mgsLayout)
$script:mgsLoadedPath=$null; $script:mgsLoadedText=''; $script:mgsDirty=$false
$mgsTimer=[Windows.Forms.Timer]::new(); $mgsTimer.Interval=600
function Test-MgsDraft {
    $mgsTimer.Stop(); $mgsSave.Enabled=$false
    $mgsTemporary=[IO.Path]::GetTempFileName()
    try {
        [IO.File]::WriteAllText($mgsTemporary,$mgsText.Text,[Text.UTF8Encoding]::new($false))
        $mgsStart=[Diagnostics.ProcessStartInfo]::new()
        $mgsStart.FileName=$mgsChecker; $mgsStart.Arguments='--check "'+$mgsTemporary+'"'
        $mgsStart.UseShellExecute=$false; $mgsStart.CreateNoWindow=$true
        $mgsStart.RedirectStandardOutput=$true; $mgsStart.RedirectStandardError=$true
        $mgsProcess=[Diagnostics.Process]::Start($mgsStart)
        try {
            $mgsOutputTask=$mgsProcess.StandardOutput.ReadToEndAsync()
            $mgsErrorTask=$mgsProcess.StandardError.ReadToEndAsync()
            if (!$mgsProcess.WaitForExit(5000)) {
                $mgsProcess.Kill() # Only this editor-owned validation helper, never MGSV.
                throw 'Validation exceeded five seconds. No game or config was changed.'
            }
            $mgsOut=$mgsOutputTask.Result; $mgsError=$mgsErrorTask.Result
            $mgsValid=$mgsProcess.ExitCode -eq 0
        } finally { $mgsProcess.Dispose() }
        $mgsErrors.Text=if ($mgsValid) { 'VALID - no game restart needed to check. Save to apply on your next launch.' } else { $mgsError+$mgsOut }
        $mgsErrors.ForeColor=if ($mgsValid) { [Drawing.Color]::DarkGreen } else { [Drawing.Color]::DarkRed }
        $mgsSave.Enabled=$mgsValid -and [bool]$script:mgsLoadedPath
        return $mgsValid
    } catch { $mgsErrors.Text=$_.Exception.Message; return $false }
    finally { if (Test-Path -LiteralPath $mgsTemporary) { Remove-Item -LiteralPath $mgsTemporary } }
}
function Open-MgsConfig([string]$MgsSelectedPath) {
    $script:mgsLoadedPath=(Resolve-Path -LiteralPath $MgsSelectedPath).Path
    $script:mgsLoadedText=[IO.File]::ReadAllText($script:mgsLoadedPath)
    $mgsText.Text=$script:mgsLoadedText -replace "\r?\n","`r`n"
    $mgsText.Select(0,0); $script:mgsDirty=$false
    $mgsWindow.Text='MGS5VR Controls - '+$script:mgsLoadedPath
    [void](Test-MgsDraft)
}
$mgsText.Add_TextChanged({ $script:mgsDirty=$true; $mgsSave.Enabled=$false; $mgsTimer.Stop(); $mgsTimer.Start() })
$mgsTimer.Add_Tick({ [void](Test-MgsDraft) })
$mgsCheck.Add_Click({ [void](Test-MgsDraft) })
$mgsOpen.Add_Click({
    if ($script:mgsDirty -and [Windows.Forms.MessageBox]::Show('Discard unsaved edits?','MGS5VR','YesNo') -ne 'Yes') { return }
    $mgsDialog=[Windows.Forms.OpenFileDialog]::new()
    $mgsDialog.Title='Choose mgs5vr-controls.ini beside the game executable'
    $mgsDialog.Filter='MGS5VR controls|mgs5vr-controls.ini|INI files|*.ini'
    try { if ($mgsDialog.ShowDialog() -eq 'OK') { Open-MgsConfig $mgsDialog.FileName } }
    catch { $mgsErrors.Text=$_.Exception.Message }
    finally { $mgsDialog.Dispose() }
})
$mgsSave.Add_Click({
    if (!(Test-MgsDraft)) { return }
    try {
        if ([IO.File]::ReadAllText($script:mgsLoadedPath) -cne $script:mgsLoadedText) { throw 'The file changed outside this editor. Reopen it before saving; your external changes were preserved.' }
        $mgsSuffix=[Guid]::NewGuid().ToString('N')
        $mgsPending=$script:mgsLoadedPath+'.'+$mgsSuffix+'.tmp'
        $mgsBackup=$script:mgsLoadedPath+'.'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+$mgsSuffix+'.bak'
        try {
            [IO.File]::WriteAllText($mgsPending,$mgsText.Text,[Text.UTF8Encoding]::new($false))
            [IO.File]::Replace($mgsPending,$script:mgsLoadedPath,$mgsBackup)
        } finally { if (Test-Path -LiteralPath $mgsPending) { Remove-Item -LiteralPath $mgsPending } }
        $script:mgsLoadedText=$mgsText.Text; $script:mgsDirty=$false
        $mgsErrors.Text="SAVED - restart MGSV to apply. Previous layout preserved:`r`n$mgsBackup"
    } catch { $mgsErrors.Text=$_.Exception.Message }
})
$mgsWindow.Add_FormClosing({ param($mgsSender,$mgsEvent)
    if ($script:mgsDirty -and [Windows.Forms.MessageBox]::Show('Discard unsaved edits?','MGS5VR','YesNo') -ne 'Yes') { $mgsEvent.Cancel=$true }
})
try {
    if ($Path) { Open-MgsConfig $Path } else { $mgsErrors.Text='Open your game controls file to begin. Nothing is changed until you save.' }
    if ($PreviewPath) {
        # Render this editor's own controls, never a desktop screenshot. Keep
        # the native surface invisible while WinForms initializes child handles.
        $mgsWindow.Opacity=0; $mgsWindow.ShowInTaskbar=$false; $mgsWindow.Show()
        $mgsLayout.PerformLayout(); $mgsText.Select(0,0)
        $mgsBitmap=[Drawing.Bitmap]::new($mgsWindow.Width,$mgsWindow.Height)
        try { $mgsWindow.DrawToBitmap($mgsBitmap,[Drawing.Rectangle]::new(0,0,$mgsWindow.Width,$mgsWindow.Height)); $mgsBitmap.Save($PreviewPath) }
        finally { $mgsBitmap.Dispose(); $mgsWindow.Hide() }
    } else { [void]$mgsWindow.ShowDialog() }
} finally { $mgsTimer.Dispose(); $mgsWindow.Dispose() }
