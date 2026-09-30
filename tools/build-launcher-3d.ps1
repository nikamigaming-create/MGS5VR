param([Parameter(Mandatory=$true)][string]$OutputDir)
$ErrorActionPreference='Stop'
Import-Module Microsoft.PowerShell.Utility
Import-Module Microsoft.PowerShell.Management
Import-Module Microsoft.PowerShell.Archive
$root=Split-Path -Parent $PSScriptRoot
$cache=Join-Path $root 'build/_deps/webview2'
$archive=Join-Path $cache 'sdk.zip'
$sdk=Join-Path $cache 'sdk'
$expected='5EA526BBD728ADDA0DA4D31219267E96460494A427E4894C4E09D9F320F4B9AA'
New-Item -ItemType Directory -Force -Path $cache,$OutputDir | Out-Null
if (!(Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest -Uri 'https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/1.0.3537.50/microsoft.web.webview2.1.0.3537.50.nupkg' -OutFile $archive
}
$mgsSdkStream = [IO.File]::OpenRead($archive)
$mgsSdkHash = [Security.Cryptography.SHA256]::Create()
try {
    $mgsSdkDigest = [BitConverter]::ToString($mgsSdkHash.ComputeHash($mgsSdkStream)).Replace('-','')
} finally { $mgsSdkHash.Dispose(); $mgsSdkStream.Dispose() }
if ($mgsSdkDigest -ne $expected) { throw 'WebView2 SDK hash does not match the pinned release.' }
# Re-extract from the verified archive, including headers and the license.
Expand-Archive -LiteralPath $archive -DestinationPath $sdk -Force
$core=Join-Path $sdk 'lib/net462/Microsoft.Web.WebView2.Core.dll'
$forms=Join-Path $sdk 'lib/net462/Microsoft.Web.WebView2.WinForms.dll'
$exe=Join-Path $OutputDir 'MGS5VR-FieldKit.exe'
$csc=Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
$savedLib=$env:LIB
try {
    Remove-Item Env:LIB -ErrorAction SilentlyContinue
    & $csc /nologo /target:winexe /platform:x64 /optimize+ /warnaserror+ /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.Web.Extensions.dll "/r:$core" "/r:$forms" "/out:$exe" (Join-Path $PSScriptRoot 'launcher-3d.cs')
    if($LASTEXITCODE -ne 0){throw 'Embedded launcher compilation failed.'}
} finally { $env:LIB=$savedLib }
Copy-Item -LiteralPath $core,$forms -Destination $OutputDir -Force
Copy-Item -LiteralPath (Join-Path $sdk 'runtimes/win-x64/native/WebView2Loader.dll') -Destination $OutputDir -Force
Copy-Item -LiteralPath (Join-Path $sdk 'LICENSE.txt') -Destination (Join-Path $OutputDir 'WebView2-LICENSE.txt') -Force
[IO.File]::WriteAllText(($exe+'.config'),'<?xml version="1.0"?><configuration><startup><supportedRuntime version="v4.0" sku=".NETFramework,Version=v4.6.2"/></startup></configuration>')
Write-Output "Built $exe"
