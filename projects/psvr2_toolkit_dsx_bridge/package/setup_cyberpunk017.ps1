#requires -Version 5.1
[CmdletBinding()]
param([string]$GameRoot, [switch]$CheckOnly, [switch]$Apply)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'Cyberpunk017.Common.ps1')

try {
    $root = Find-CyberpunkRoot -GameRoot $GameRoot
    $mods = Join-Path $root 'bin\x64\plugins\cyber_engine_tweaks\mods'
    $settings = Join-Path $mods 'DualSense Support\config\settings.json'
    $source = Join-Path $PSScriptRoot 'cet\PSVR2 Haptics\init.lua'
    $destination = Join-Path $mods 'PSVR2 Haptics\init.lua'
    $native = Join-Path $root 'red4ext\plugins\DualSenseSupport\DualSense Support.dll'
    foreach ($file in @($settings, $source, (Join-Path $mods 'nativeSettings\init.lua'))) {
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Required file missing: $file. Install Enhanced DualSense Support and Native Settings UI, then run the game once before setup." }
    }
    $json = Get-Content -LiteralPath $settings -Raw | ConvertFrom-Json
    if ($json.PSObject.Properties.Name -notcontains 'UDPautostart' -or $json.UDPautostart -isnot [bool]) {
        throw 'Unexpected EDS settings format; refusing to edit it. Run the game once with EDS installed.'
    }
    $disableUdp = $json.UDPautostart
    $disableNative = Test-Path -LiteralPath $native -PathType Leaf
    $installMenu = -not (Test-Path -LiteralPath $destination -PathType Leaf)
    if (-not $installMenu) {
        $installMenu = (Get-FileHash $source).Hash -ne (Get-FileHash $destination).Hash
    }
    Write-Host "Game: $root"
    Write-Host "Install/update PSVR2 audio controls inside the existing DualSense settings menu: $installMenu"
    Write-Host "Set EDS UDPautostart=false (preserving weapon and other settings): $disableUdp"
    Write-Host "Move unused EDS native launcher DLL to a recoverable backup: $disableNative"
    Write-Host 'No Toolkit driver installation. No change to weapon profiles or saved PSVR2 audio preferences.'
    if ($CheckOnly) { Write-Host 'Check complete; no files changed.'; exit 0 }
    if (-not ($installMenu -or $disableUdp -or $disableNative)) { Write-Host 'Setup already current; no changes needed.'; exit 0 }
    if (Get-Process Cyberpunk2077 -ErrorAction SilentlyContinue) { throw 'Close Cyberpunk before setup.' }
    if (-not $Apply -and (Read-Host 'Apply the changes listed above? Type YES to continue') -cne 'YES') {
        throw 'Cancelled; no files changed.'
    }
    $backup = Join-Path $root ('PSVR2Bridge-backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Path $backup | Out-Null
    Write-Host "Backup: $backup"
    # Capture every overwritten file first; never replace the companion settings.ini.
    if ($disableUdp) { Copy-Item -LiteralPath $settings -Destination (Join-Path $backup 'EDS-settings.json') }
    if ($installMenu -and (Test-Path -LiteralPath $destination)) {
        Copy-Item -LiteralPath $destination -Destination (Join-Path $backup 'PSVR2-Haptics-init.lua')
    }
    $nativeMoved = $false
    $menuWasPresent = Test-Path -LiteralPath $destination -PathType Leaf
    try {
        if ($disableNative) {
            Move-Item -LiteralPath $native -Destination (Join-Path $backup 'DualSense Support.dll')
            $nativeMoved = $true
        }
        if ($disableUdp) {
            $json.UDPautostart = $false
            [IO.File]::WriteAllText($settings, ($json | ConvertTo-Json -Depth 100), [Text.UTF8Encoding]::new($false))
        }
        if ($installMenu) {
            New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
            Copy-Item -LiteralPath $source -Destination $destination -Force
        }
    } catch {
        $reason = $_.Exception.Message
        if ($disableUdp) { Copy-Item -LiteralPath (Join-Path $backup 'EDS-settings.json') -Destination $settings -Force }
        if ($nativeMoved) { Copy-Item -LiteralPath (Join-Path $backup 'DualSense Support.dll') -Destination $native -Force }
        if ($installMenu -and $menuWasPresent) {
            Copy-Item -LiteralPath (Join-Path $backup 'PSVR2-Haptics-init.lua') -Destination $destination -Force
        } elseif ($installMenu -and (Test-Path -LiteralPath $destination -PathType Leaf)) {
            Remove-Item -LiteralPath $destination -Force
        }
        throw "Setup failed; reverted its changes. Backup: $backup. $reason"
    }
    Write-Host 'Setup complete. Use in-game Mods > DualSense > PSVR2 Haptics.' -ForegroundColor Green
    Write-Host "Backup: $backup"
    if ($disableNative) {
        Write-Host "Native launcher moved, not deleted. To undo with the game closed, copy '$backup\DualSense Support.dll' back to '$native'. It is not needed while using this bridge."
    }
    if ($disableUdp) { Write-Host "To restore the old EDS settings, copy '$backup\EDS-settings.json' back to '$settings'." }
    Write-Host 'To remove just the audio menu, remove the PSVR2 Haptics CET companion folder. Keep EDS.'
} catch { Write-Host "STOPPED: $($_.Exception.Message)" -ForegroundColor Red; exit 1 }
