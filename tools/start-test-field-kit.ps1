param(
    [string]$GameDir = 'D:\SteamLibrary\steamapps\common\MGS_TPP',
    [string]$Proxy = 'D:\code\gta-iv\out-openxr\external\meta-xr-operator-standalone-205.1\extracted\meta-xr-operator-standalone-public\windows\meta-xr-operator-mcp-proxy.exe',
    [int]$Port = 8771,
    [switch]$NoOpen
)
$ErrorActionPreference = 'Stop'
$fieldKitRoot = Split-Path -Parent $PSScriptRoot
$fieldKitPython = (Get-Command python -ErrorAction Stop).Source
$fieldKitOutput = Join-Path $fieldKitRoot 'artifacts\test-field-kit'
New-Item -ItemType Directory -Force -Path $fieldKitOutput | Out-Null
$fieldKitConnection = Join-Path $fieldKitOutput 'connection.json'
if (Test-Path -LiteralPath $fieldKitConnection) {
    $fieldKitExisting = Get-Content -LiteralPath $fieldKitConnection -Raw | ConvertFrom-Json
    try {
        $fieldKitState = Invoke-RestMethod -Uri ($fieldKitExisting.url + '/api/state') -Headers @{Authorization=('Bearer ' + $fieldKitExisting.tokens.human)} -TimeoutSec 2
        if ($fieldKitState.schema -eq 1) { if (-not $NoOpen) { Start-Process $fieldKitExisting.human_url }; return }
    } catch { }
}
# Launch only this local developer service. The game is started through its normal simulator launcher.
$fieldKitArguments = @(('"{0}"' -f (Join-Path $PSScriptRoot 'test-field-kit.py')), '--game-dir', ('"{0}"' -f $GameDir), '--proxy', ('"{0}"' -f $Proxy), '--port', $Port, '--output', ('"{0}"' -f $fieldKitOutput))
if (-not $NoOpen) { $fieldKitArguments += '--open' }
Start-Process -FilePath $fieldKitPython -ArgumentList $fieldKitArguments -WorkingDirectory $fieldKitRoot -WindowStyle Hidden -RedirectStandardOutput (Join-Path $fieldKitOutput 'server.log') -RedirectStandardError (Join-Path $fieldKitOutput 'server-error.log')
