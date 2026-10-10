# Shared discovery, safe with Windows PowerShell 5.1 StrictMode and absent keys.
function Get-OptionalRegistryString {
    param([string]$Path, [string]$Name)
    $entry = Get-ItemProperty -LiteralPath $Path -ErrorAction SilentlyContinue
    if ($null -eq $entry) { return $null }
    $property = $entry.PSObject.Properties[$Name]
    if ($null -eq $property -or $property.Value -isnot [string]) { return $null }
    return $property.Value
}

function Find-CyberpunkRoot {
    param([string]$GameRoot)
    if ($GameRoot) {
        if (-not (Test-Path -LiteralPath (Join-Path $GameRoot 'bin\x64\Cyberpunk2077.exe') -PathType Leaf)) {
            throw "Not a Cyberpunk game folder: $GameRoot (expected bin\x64\Cyberpunk2077.exe)."
        }
        return (Resolve-Path -LiteralPath $GameRoot).Path
    }
    $candidates = [System.Collections.Generic.List[string]]::new()
    foreach ($key in @(
        'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 1091500',
        'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 1091500',
        'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\GOGPACK 1423049311_is1',
        'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\GOGPACK 1423049311_is1'
    )) {
        $location = Get-OptionalRegistryString $key 'InstallLocation'
        if ($location) { $candidates.Add($location) }
    }
    $steam = Get-OptionalRegistryString 'HKCU:\Software\Valve\Steam' 'SteamPath'
    if ($steam) {
        $candidates.Add((Join-Path $steam 'steamapps\common\Cyberpunk 2077'))
        $libraries = Join-Path $steam 'steamapps\libraryfolders.vdf'
        if (Test-Path -LiteralPath $libraries -PathType Leaf) {
            foreach ($match in [regex]::Matches((Get-Content -LiteralPath $libraries -Raw), '"path"\s+"([^"]+)"')) {
                $library = $match.Groups[1].Value -replace '\\\\', '\'
                $candidates.Add((Join-Path $library 'steamapps\common\Cyberpunk 2077'))
            }
        }
    }
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath (Join-Path $candidate 'bin\x64\Cyberpunk2077.exe') -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw 'Cyberpunk 2077 not found. Run again with -GameRoot "D:\path\to\Cyberpunk 2077".'
}
