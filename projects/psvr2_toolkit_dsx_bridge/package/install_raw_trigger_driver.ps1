$ErrorActionPreference = 'Stop'

$running = Get-Process vrserver, vrmonitor, vrcompositor -ErrorAction SilentlyContinue
if ($running) {
    throw 'SteamVR is still running. Close it completely before installing the Toolkit driver.'
}

$target = $null
$openVrPaths = Join-Path $env:LOCALAPPDATA 'openvr\openvrpaths.vrpath'
if (Test-Path -LiteralPath $openVrPaths) {
    $paths = Get-Content -Raw -LiteralPath $openVrPaths | ConvertFrom-Json
    foreach ($driverRoot in $paths.external_drivers) {
        if ($driverRoot -like '*PlayStation VR2 App*') {
            $candidate = Join-Path $driverRoot 'bin\win64'
            if (Test-Path -LiteralPath (Join-Path $candidate 'driver_playstation_vr2.dll')) {
                $target = $candidate
                break
            }
        }
    }
}

if (-not $target) {
    throw 'The PlayStation VR2 SteamVR plug-in was not found. Install and run the PlayStation VR2 App once, then retry.'
}

$source = Join-Path $PSScriptRoot 'driver_playstation_vr2.dll'
if (-not (Test-Path -LiteralPath $source)) {
    throw "Package is missing $source"
}

$installed = Join-Path $target 'driver_playstation_vr2.dll'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$backup = Join-Path $target "driver_playstation_vr2.pre-raw-trigger-$stamp.bak"
Copy-Item -LiteralPath $installed -Destination $backup
Copy-Item -LiteralPath $source -Destination $installed -Force

Write-Host 'Raw-trigger Toolkit driver installed successfully.' -ForegroundColor Green
Write-Host "Backup: $backup"
Write-Host 'Start SteamVR, connect both Sense controllers, then run run_bridge.cmd.'
