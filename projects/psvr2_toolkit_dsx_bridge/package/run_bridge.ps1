param(
    [string]$GameRoot,
    [int]$Port = 6969,
    # Full-game audio-haptics gain, 0..3. The bridge's own default is 1.35 and is kept here so
    # normal runs are unchanged. Raise it to test whether quiet sources -- such as the VR melee
    # whoosh replayed on a physical katana slash -- survive the 28-320 Hz tactile band.
    [ValidateRange(0.0, 3.0)]
    [double]$AudioHapticsGain = 1.35,
    # Melee swing/impact pulses from the CyberpunkVR Port plugin, scaled independently of the
    # audio layer. 0 disables them via --no-vr-motion-haptics.
    [ValidateRange(0.0, 3.0)]
    [double]$VRMotionGain = 1.0
)

$ErrorActionPreference = 'Stop'
$modConfig = 'bin\x64\plugins\cyber_engine_tweaks\mods\DualSense Support\config\DualSenseXConfig.txt'

function Find-CyberpunkRoot {
    if ($GameRoot) {
        return (Resolve-Path -LiteralPath $GameRoot).Path
    }

    $candidates = [System.Collections.Generic.List[string]]::new()
    foreach ($key in @(
        'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 1091500',
        'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 1091500'
    )) {
        $location = (Get-ItemProperty -Path $key -ErrorAction SilentlyContinue).InstallLocation
        if ($location) { $candidates.Add($location) }
    }

    $steamPath = (Get-ItemProperty -Path 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
    if ($steamPath) {
        $steamPath = $steamPath -replace '/', '\'
        $candidates.Add((Join-Path $steamPath 'steamapps\common\Cyberpunk 2077'))
        $libraryFile = Join-Path $steamPath 'steamapps\libraryfolders.vdf'
        if (Test-Path -LiteralPath $libraryFile) {
            foreach ($match in [regex]::Matches((Get-Content -Raw -LiteralPath $libraryFile), '"path"\s+"([^"]+)"')) {
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

    throw 'Cyberpunk 2077 was not found. Run: .\run_bridge.ps1 -GameRoot "D:\path\to\Cyberpunk 2077"'
}

$root = Find-CyberpunkRoot
$config = Join-Path $root $modConfig
if (-not (Test-Path -LiteralPath $config)) {
    throw "Enhanced DualSense Support has not generated its config file: $config. Install/enable the mod and launch the game once."
}

# Enhanced DualSense Support's own UDP client must not run alongside this bridge: two clients
# on the controller-effect path give inconsistent trigger effects and missing haptics. The mod
# ships that option defaulted ON, so its "reset to defaults" silently re-enables the conflict --
# which is how a whole round of weapon-preset tuning once got measured under a conflict.
#
# Enforcing it here rather than editing the mod means the fix survives updating the mod from
# Nexus, and re-applies on every launch instead of needing to be remembered.
#
# The value is patched by string replacement rather than reserialising the JSON: this is a
# third-party data file and round-tripping it through ConvertTo-Json risks reformatting or
# dropping content. Parsing is used only to inspect.
function Sync-DualSenseSettings {
    param([string]$SettingsPath)

    $settingsFile = Join-Path (Split-Path -Parent $SettingsPath) 'settings.json'
    if (-not (Test-Path -LiteralPath $settingsFile)) { return }

    if (Get-Process Cyberpunk2077 -ErrorAction SilentlyContinue) {
        Write-Host '[!] Cyberpunk is running -- skipping mod settings check (the mod rewrites its config on exit).'
        return
    }

    $raw = Get-Content -LiteralPath $settingsFile -Raw
    $parsed = $null
    try { $parsed = $raw | ConvertFrom-Json } catch {
        Write-Host '[!] Could not parse the mod settings file -- leaving it alone.'
        return
    }

    if ($parsed.UDPautostart) {
        $patched = [regex]::Replace($raw, '("UDPautostart"\s*:\s*)true', '${1}false', 1)
        if ($patched -ne $raw) {
            # Not Set-Content -Encoding utf8: Windows PowerShell 5.1 writes a BOM, and the mod's
            # Lua JSON parser reads this file. Write UTF-8 without one.
            [System.IO.File]::WriteAllText($settingsFile, $patched,
                (New-Object System.Text.UTF8Encoding $false))
            Write-Host '[+] Disabled Enhanced DualSense Support UDP autostart (it conflicts with this bridge).'
        } else {
            Write-Host '[!] UDP autostart is ON but could not be patched automatically -- turn it off in Native Settings.'
        }
    }

    # Category overrides are the user's call, so warn rather than rewrite. Stock per-weapon
    # effects are correct on Sense; the overrides break haptics and make triggers inconsistent.
    if ($parsed.weaponsSettings) {
        $overridden = @($parsed.weaponsSettings.PSObject.Properties |
            Where-Object { $_.Value.value -and $_.Value.value -ne 1 })
        if ($overridden.Count -gt 0) {
            Write-Host "[!] $($overridden.Count) weapon categories are overridden (not Default)."
            Write-Host '    Stock effects are recommended on PS VR2 Sense; overrides give inconsistent triggers.'
            Write-Host '    Native Settings -> Enhanced DualSense Support -> weapon categories -> Default.'
        }
    }
}

Sync-DualSenseSettings -SettingsPath $config

$bridge = Join-Path $PSScriptRoot 'psvr2_toolkit_dsx_bridge.exe'
Write-Host "Cyberpunk config: $config"
Write-Host "DSX UDP port: $Port"
Write-Host "Audio haptics gain: $AudioHapticsGain"
Write-Host "VR motion gain: $VRMotionGain"
$inv = [System.Globalization.CultureInfo]::InvariantCulture
$gain = $AudioHapticsGain.ToString($inv)
$log = Join-Path $PSScriptRoot 'bridge.log'
$extra = @()
if ($VRMotionGain -le 0.0) {
    $extra += '--no-vr-motion-haptics'
} else {
    $extra += @('--vr-motion-gain', $VRMotionGain.ToString($inv))
}
& $bridge --port $Port --cyberpunk-config $config --audio-haptics-gain $gain @extra 2>&1 |
    Tee-Object -FilePath $log
exit $LASTEXITCODE
