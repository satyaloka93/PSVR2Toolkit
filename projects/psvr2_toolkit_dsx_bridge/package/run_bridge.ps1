param(
    [string]$GameRoot,
    [int]$Port = 6969
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

$bridge = Join-Path $PSScriptRoot 'psvr2_toolkit_dsx_bridge.exe'
Write-Host "Cyberpunk config: $config"
Write-Host "DSX UDP port: $Port"
$log = Join-Path $PSScriptRoot 'bridge.log'
& $bridge --port $Port --cyberpunk-config $config 2>&1 | Tee-Object -FilePath $log
exit $LASTEXITCODE
