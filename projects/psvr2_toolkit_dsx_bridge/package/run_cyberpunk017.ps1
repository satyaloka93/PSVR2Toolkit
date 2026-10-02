#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$GameRoot,
    [string]$BridgeRoot = $PSScriptRoot,
    [ValidateRange(1, 65535)][int]$Port = 6969,
    [ValidateRange(0, 3)][double]$AudioHapticsGain = 1.35,
    [ValidateRange(0, 3)][double]$VRMotionGain = 1.0,
    [switch]$CheckOnly
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Find-CyberpunkRoot {
    if ($GameRoot) { return (Resolve-Path -LiteralPath $GameRoot).Path }
    $candidates = [System.Collections.Generic.List[string]]::new()
    foreach ($key in @(
        'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 1091500',
        'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 1091500'
    )) {
        $location = (Get-ItemProperty -Path $key -ErrorAction SilentlyContinue).InstallLocation
        if ($location) { $candidates.Add($location) }
    }
    $steam = (Get-ItemProperty -Path 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
    if ($steam) {
        $steam = $steam -replace '/', '\'
        $candidates.Add((Join-Path $steam 'steamapps\common\Cyberpunk 2077'))
        $libraries = Join-Path $steam 'steamapps\libraryfolders.vdf'
        if (Test-Path -LiteralPath $libraries) {
            foreach ($match in [regex]::Matches((Get-Content -LiteralPath $libraries -Raw), '"path"\s+"([^"]+)"')) {
                $library = $match.Groups[1].Value -replace '\\\\', '\'
                $candidates.Add((Join-Path $library 'steamapps\common\Cyberpunk 2077'))
            }
        }
    }
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath (Join-Path $candidate 'bin\x64\Cyberpunk2077.exe')) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw 'Cyberpunk 2077 not found; pass -GameRoot with its installation directory.'
}

try {
    $root = Find-CyberpunkRoot
    $exe = Join-Path $BridgeRoot 'psvr2_toolkit_dsx_bridge_017.exe'
    $mod = Join-Path $root 'bin\x64\plugins\cyber_engine_tweaks\mods\DualSense Support\config'
    $config = Join-Path $mod 'DualSenseXConfig.txt'
    $settings = Join-Path $mod 'settings.json'
    foreach ($file in @($exe, $config, $settings)) {
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Required file missing: $file" }
    }
    $json = Get-Content -LiteralPath $settings -Raw | ConvertFrom-Json
    if ($json.PSObject.Properties.Name -notcontains 'UDPautostart' -or $json.UDPautostart -ne $false) {
        throw 'Disable Enhanced DualSense Support UDP autostart first. This launcher does not rewrite mod settings.'
    }
    $native = Join-Path $root 'red4ext\plugins\DualSenseSupport\DualSense Support.dll'
    if (Test-Path -LiteralPath $native) {
        throw 'Disable the incompatible Enhanced DualSense Support native launcher DLL; retain its CET mod.'
    }
    if (Get-Process DSX, UDPClient, psvr2_toolkit_dsx_bridge, psvr2_toolkit_dsx_bridge_017 -ErrorAction SilentlyContinue) {
        throw 'Another DSX client or bridge is running. Only one Sense actuator owner is allowed.'
    }
    Write-Host "Bridge: $exe"
    Write-Host "Gameplay profiles: $config"
    Write-Host 'Motion mapping: Local\CyberpunkVR_PSVR2_Haptics_017_v1 (not legacy input slots).'
    if ($CheckOnly) { Write-Host 'Preflight passed; bridge not started.'; exit 0 }
    if (-not (Get-Process vrserver -ErrorAction SilentlyContinue)) { throw 'Start SteamVR first.' }
    if (@(Get-NetUDPEndpoint -LocalPort $Port -ErrorAction SilentlyContinue).Count) {
        throw "UDP port $Port is already in use."
    }
    $inv = [Globalization.CultureInfo]::InvariantCulture
    $gain = $AudioHapticsGain.ToString($inv)
    $motion = $VRMotionGain.ToString($inv)
    & $exe --port $Port --cyberpunk-config $config --audio-haptics-gain $gain --vr-motion-gain $motion
    exit $LASTEXITCODE
} catch { Write-Host "STOPPED: $($_.Exception.Message)" -ForegroundColor Red; exit 1 }
