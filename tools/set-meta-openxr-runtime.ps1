param([string]$ResultPath)
$ErrorActionPreference='Stop'
$mgsResult=[ordered]@{success=$false;runtime=$null;previous=$null;error=$null}
try {
    $mgsProgramFiles=$env:ProgramW6432
    if (-not $mgsProgramFiles) { $mgsProgramFiles=[Environment]::GetFolderPath('ProgramFiles') }
    $mgsManifest=(Resolve-Path -LiteralPath (Join-Path $mgsProgramFiles 'Oculus\Support\oculus-runtime\oculus_openxr_64.json')).Path
    $mgsDefinition=Get-Content -LiteralPath $mgsManifest -Raw | ConvertFrom-Json
    if ($mgsDefinition.runtime.name -ne 'Oculus OpenXR') { throw 'The selected manifest is not Meta Quest Link OpenXR.' }
    $mgsLibrary=Join-Path (Split-Path -Parent $mgsManifest) $mgsDefinition.runtime.library_path
    if (-not (Test-Path -LiteralPath $mgsLibrary -PathType Leaf)) { throw 'Meta OpenXR runtime library is missing.' }
    $mgsBase=[Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine,[Microsoft.Win32.RegistryView]::Registry64)
    try {
        $mgsKey=$mgsBase.OpenSubKey('SOFTWARE\Khronos\OpenXR\1',$true)
        if (-not $mgsKey) { throw 'Windows OpenXR registration is missing.' }
        try {
            $mgsResult.previous=$mgsKey.GetValue('ActiveRuntime')
            $mgsKey.SetValue('ActiveRuntime',$mgsManifest,[Microsoft.Win32.RegistryValueKind]::String)
            if ($mgsKey.GetValue('ActiveRuntime') -ne $mgsManifest) { throw 'Windows did not retain the Meta OpenXR selection.' }
        } finally { $mgsKey.Dispose() }
    } finally { $mgsBase.Dispose() }
    $mgsResult.runtime=$mgsManifest
    $mgsResult.success=$true
} catch { $mgsResult.error=$_.Exception.Message }
$mgsJson=$mgsResult | ConvertTo-Json
if ($ResultPath) { [IO.File]::WriteAllText($ResultPath,$mgsJson,[Text.UTF8Encoding]::new($false)) }
Write-Output $mgsJson
if (-not $mgsResult.success) { exit 1 }
