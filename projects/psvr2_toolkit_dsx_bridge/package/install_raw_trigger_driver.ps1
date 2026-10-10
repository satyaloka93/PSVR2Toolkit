#requires -Version 5.1
[CmdletBinding()]
param([string]$DriverDirectory, [switch]$CheckOnly, [switch]$Install,
      [string]$RestoreBackup)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

try {
    $expected = '977856e7c1d54a8aea0bf4ef19095ce901fb658d301ca4ba6bf8e077d83a2b43'
    $source = Join-Path $PSScriptRoot 'driver_playstation_vr2.dll'
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Package is missing $source" }
    if ((Get-FileHash -LiteralPath $source).Hash -ine $expected) { throw 'Bundled driver does not match the documented tested version; refusing installation.' }
    $target = $DriverDirectory
    if (-not $target) {
        $openVrPaths = Join-Path $env:LOCALAPPDATA 'openvr\openvrpaths.vrpath'
        if (Test-Path -LiteralPath $openVrPaths -PathType Leaf) {
            $paths = Get-Content -Raw -LiteralPath $openVrPaths | ConvertFrom-Json
            if ($paths.PSObject.Properties.Name -contains 'external_drivers') {
                foreach ($driverRoot in $paths.external_drivers) {
                    if ($driverRoot -like '*PlayStation VR2 App*') {
                        $candidate = Join-Path $driverRoot 'bin\win64'
                        if (Test-Path -LiteralPath (Join-Path $candidate 'driver_playstation_vr2.dll') -PathType Leaf) {
                            $target = $candidate
                            break
                        }
                    }
                }
            }
        }
    }
    if (-not $target) { throw 'PlayStation VR2 driver not found. Run the PlayStation VR2 App once, or pass -DriverDirectory with its bin\win64 directory.' }
    $target = (Resolve-Path -LiteralPath $target).Path
    $installed = Join-Path $target 'driver_playstation_vr2.dll'
    if (-not (Test-Path -LiteralPath $installed -PathType Leaf)) { throw "Existing driver missing: $installed" }
    $installedHash = (Get-FileHash -LiteralPath $installed).Hash.ToLowerInvariant()
    Write-Host "Installed: $installed"
    Write-Host "Installed SHA-256: $installedHash"
    Write-Host "Bundled SHA-256:   $expected"
    Write-Host 'Bundled driver: unchanged v0.2.0 tested baseline (raw-trigger transport and grip PCM attenuation fix).'
    Write-Host 'This bridge update does not introduce a new driver revision. A different hash does not by itself mean incompatible.'
    # This is a driver update, not the complete base Toolkit installer. Never
    # replace a stock Sony DLL without the real-driver backup and CAPI/runtime.
    # Allow rollback even if a dependency has subsequently disappeared.
    if (-not $RestoreBackup) {
        foreach ($dependency in @('driver_playstation_vr2_orig.dll', 'libcrossipc.dll', 'libusb-1.0.dll', 'psvr2_toolkit_capi.dll')) {
            if (-not (Test-Path -LiteralPath (Join-Path $target $dependency) -PathType Leaf)) {
                throw "Base PSVR2Toolkit installation is incomplete: missing $dependency. No driver changed. Follow https://github.com/BnuuySolutions/PSVR2Toolkit/wiki/Installation first, then rerun this update."
            }
        }
    }
    if ($RestoreBackup) {
        $restore = (Resolve-Path -LiteralPath $RestoreBackup).Path
        if ((Split-Path -Parent $restore) -ine $target -or
            (Split-Path -Leaf $restore) -notlike 'driver_playstation_vr2.pre-raw-trigger-*.bak') {
            throw 'RestoreBackup must be an installer-created backup alongside the installed driver.'
        }
        $source = $restore
        Write-Host "Requested restore: $restore"
    } elseif ($installedHash -eq $expected) {
        Write-Host 'Already identical. Driver left untouched; no reinstall or SteamVR restart needed.' -ForegroundColor Green
        exit 0
    }
    if ($CheckOnly) { Write-Host 'Check complete; no files changed. Keep your existing driver if it works, or explicitly install the tested baseline.'; exit 0 }
    if (Get-Process vrserver, vrmonitor, vrcompositor -ErrorAction SilentlyContinue) {
        throw 'Close SteamVR completely before replacing or restoring the Toolkit driver.'
    }
    if (-not $Install -and (Read-Host 'Replace the displayed driver, saving a backup? Type INSTALL to confirm') -cne 'INSTALL') {
        throw 'Cancelled; installed driver unchanged.'
    }
    $backup = Join-Path $target ('driver_playstation_vr2.pre-raw-trigger-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.bak')
    Copy-Item -LiteralPath $installed -Destination $backup
    Write-Host "Backup: $backup"
    try {
        Copy-Item -LiteralPath $source -Destination $installed -Force
        if ((Get-FileHash -LiteralPath $installed).Hash -ne (Get-FileHash -LiteralPath $source).Hash) {
            throw 'Copy verification failed.'
        }
    } catch {
        $reason = $_.Exception.Message
        Copy-Item -LiteralPath $backup -Destination $installed -Force
        throw "Driver replacement failed; restored the previous driver. $reason"
    }
    Write-Host 'Driver operation complete.' -ForegroundColor Green
    Write-Host "Rollback: .\install_raw_trigger_driver.ps1 -DriverDirectory `"$target`" -RestoreBackup `"$backup`""
    Write-Host 'Next: use the separate bridge folder. Driver setup is not a per-game launch step.'
} catch { Write-Host "STOPPED: $($_.Exception.Message)" -ForegroundColor Red; exit 1 }
