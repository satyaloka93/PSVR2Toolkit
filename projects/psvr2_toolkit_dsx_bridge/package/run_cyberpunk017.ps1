#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameRoot,
    [string]$BridgeRoot = $PSScriptRoot,
    [ValidateRange(1, 65535)][int]$Port = 6969,
    [ValidateRange(0, 3)][double]$AudioHapticsGain = 0,
    [ValidateRange(0, 3)][double]$VRMotionGain = 1.0,
    [switch]$CheckOnly,
    [switch]$NoGameAudioHaptics
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

. (Join-Path $PSScriptRoot 'Cyberpunk017.Common.ps1')

try {
    $root = Find-CyberpunkRoot -GameRoot $GameRoot
    $exe = Join-Path $BridgeRoot 'psvr2_toolkit_dsx_bridge_017.exe'
    $mod = Join-Path $root 'bin\x64\plugins\cyber_engine_tweaks\mods\DualSense Support\config'
    $config = Join-Path $mod 'DualSenseXConfig.txt'
    $settings = Join-Path $mod 'settings.json'
    foreach ($file in @($exe, (Join-Path $BridgeRoot 'psvr2_toolkit_capi_loader.dll'), $config, $settings)) {
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Required file missing: $file" }
    }
    $json = Get-Content -LiteralPath $settings -Raw | ConvertFrom-Json
    if ($json.PSObject.Properties.Name -notcontains 'UDPautostart' -or $json.UDPautostart -ne $false) {
        throw 'Disable Enhanced DualSense Support UDP autostart first. This launcher does not rewrite mod settings.'
    }
    $native = Join-Path $root 'red4ext\plugins\DualSenseSupport\DualSense Support.dll'
    if (Test-Path -LiteralPath $native) {
        throw "The separate EDS native launcher is not used by this bridge. Close Cyberpunk and run .\setup_cyberpunk017.cmd -GameRoot `"$root`" to reversibly disable `"$native`" and install the audio menu. Keep the DualSense Support CET mod. UDPautostart=false alone does not stop RED4ext loading this DLL."
    }
    if (Get-Process DSX, UDPClient, psvr2_toolkit_dsx_bridge, psvr2_toolkit_dsx_bridge_017 -ErrorAction SilentlyContinue) {
        throw 'Another DSX client or bridge is running. Only one Sense actuator owner is allowed.'
    }
    Write-Host "Bridge: $exe"
    Write-Host "Gameplay profiles: $config"
    Write-Host 'Motion mapping: Local\CyberpunkVR_PSVR2_Haptics_017_v1 (not legacy input slots).'
    $menu = Join-Path $root 'bin\x64\plugins\cyber_engine_tweaks\mods\PSVR2 Haptics\init.lua'
    if (-not (Test-Path -LiteralPath $menu -PathType Leaf)) {
        throw "Audio menu is not installed. Close the game and run .\setup_cyberpunk017.cmd -GameRoot `"$root`" once."
    }
    Write-Host 'Audio: controlled live by in-game Mods > DualSense > PSVR2 Haptics (OFF on first install).'
    if ($NoGameAudioHaptics) { Write-Host 'Audio is forcibly disabled by -NoGameAudioHaptics for this run.' }
    if ($CheckOnly) { Write-Host 'Preflight passed; bridge not started.'; exit 0 }
    if (-not (Get-Process vrserver -ErrorAction SilentlyContinue)) { throw 'Start SteamVR first.' }
    if (@(Get-NetUDPEndpoint -LocalPort $Port -ErrorAction SilentlyContinue).Count) {
        throw "UDP port $Port is already in use."
    }
    $inv = [Globalization.CultureInfo]::InvariantCulture
    $gain = $AudioHapticsGain.ToString($inv)
    $motion = $VRMotionGain.ToString($inv)
    $extra = @()
    if ($NoGameAudioHaptics) { $extra += '--no-game-audio-haptics' }
    & $exe --port $Port --cyberpunk-config $config --audio-haptics-gain $gain --vr-motion-gain $motion @extra
    exit $LASTEXITCODE
} catch { Write-Host "STOPPED: $($_.Exception.Message)" -ForegroundColor Red; exit 1 }
