Set-StrictMode -Version Latest

function Get-MgsIni([string]$Text) {
    $result=[ordered]@{}; $section=''
    foreach ($line in ($Text -split '\r?\n')) {
        if ($line -match '^\s*\[([^]]+)\]\s*$') { $section=$Matches[1].ToLowerInvariant() }
        elseif ($section -and $line -match '^\s*([^;#=]+?)\s*=\s*(.*?)\s*$') {
            $key=$section+'.'+$Matches[1].Trim().ToLowerInvariant()
            if ($result.Contains($key)) { throw "Duplicate setting: $key" }
            $result[$key]=$Matches[2]
        }
    }
    return $result
}
function Set-MgsIniValue([string]$Text,[string]$Name,[string]$Value) {
    if ($Name -notmatch '^([a-z_][a-z0-9_]*)\.([a-z_][a-z0-9_]*)$' -or $Value -match '[\r\n]') { throw 'Use section.key and a single-line value.' }
    $section=$Matches[1];$key=$Matches[2]
    $lines=[Collections.Generic.List[string]]::new()
    $lines.AddRange([string[]]($Text -split '\r?\n'))
    $inside=$false;$found=$false;$insert=-1
    for ($i=0;$i -lt $lines.Count;$i++) {
        if ($lines[$i] -match '^\s*\[([^]]+)\]\s*$') {
            if ($inside) { $insert=$i; break }
            $inside=$Matches[1] -ieq $section
            if ($inside) { $found=$true; $insert=$i+1 }
        } elseif ($inside) {
            if ($lines[$i] -match ('^\s*'+[regex]::Escape($key)+'\s*=')) {
                $lines[$i]=$key+' = '+$Value;return ($lines -join "`r`n")
            }
            $insert=$i+1
        }
    }
    if (!$found) { $lines.Add('');$lines.Add('['+$section+']');$insert=$lines.Count }
    $lines.Insert($insert,$key+' = '+$Value)
    return ($lines -join "`r`n")
}
function Test-MgsRuntime([string]$Text) {
    $values=Get-MgsIni $Text
    $ranges=@{
        'theatre.enabled'=@(0,1);'theatre.width_cm'=@(100,3000);'theatre.distance_cm'=@(100,3000)
        'diagnostics.native_actions'=@(0,1);'diagnostics.camera_observer'=@(0,1)
        'diagnostics.head_camera_experiment'=@(0,1);'diagnostics.controller_rig_experiment'=@(0,1)
        'diagnostics.wrist_hud_experiment'=@(0,1);'opening.interactive_cabin'=@(0,1)
    }
    foreach ($name in $ranges.Keys) {
        if ($values.Contains($name)) {
            $number=0
            if (![int]::TryParse($values[$name],[ref]$number) -or $number -lt $ranges[$name][0] -or $number -gt $ranges[$name][1]) {
                throw "Invalid $name; expected integer $($ranges[$name][0])..$($ranges[$name][1])."
            }
        }
    }
}
function Save-MgsSettings([string]$Path,[string]$Original,[string]$Updated,[string]$Checker,[switch]$Runtime) {
    $resolved=(Resolve-Path -LiteralPath $Path -ErrorAction Stop).Path
    if ([IO.File]::ReadAllText($resolved) -cne $Original) { throw 'The file changed outside this editor. Reload before saving.' }
    $pending=$resolved+'.'+[guid]::NewGuid().ToString('N')+'.tmp'
    try {
        [IO.File]::WriteAllText($pending,$Updated,[Text.UTF8Encoding]::new($false))
        if ($Runtime) { Test-MgsRuntime $Updated }
        else {
            $checkOutput=& $Checker --check $pending 2>&1
            if ($LASTEXITCODE -ne 0) { throw ($checkOutput -join "`n") }
        }
        if ([IO.File]::ReadAllText($resolved) -cne $Original) { throw 'The file changed during validation. Reload before saving.' }
        if ($Updated -ceq $Original) { return $null }
        $backup=$resolved+'.'+[guid]::NewGuid().ToString('N')+'.backup'
        [IO.File]::Replace($pending,$resolved,$backup)
        return $backup
    } finally { if (Test-Path -LiteralPath $pending) { Remove-Item -LiteralPath $pending } }
}
Export-ModuleMember -Function Get-MgsIni,Set-MgsIniValue,Test-MgsRuntime,Save-MgsSettings
