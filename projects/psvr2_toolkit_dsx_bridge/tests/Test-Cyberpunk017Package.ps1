#requires -Version 5.1
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$OutputRoot,
      [Parameter(Mandatory=$true)][string]$DriverDll)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$package = (Resolve-Path (Join-Path $PSScriptRoot '..\package')).Path
$scratch = Join-Path $OutputRoot ('haptics-fixtures-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $scratch | Out-Null
$passed = 0
function Assert-That([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "FAIL: $Message" }
    $script:passed++
    Write-Host ('PASS: ' + ($Message -split "`r?`n")[0])
}
function Write-Fixture([string]$Path, [string]$Text) {
    New-Item -ItemType Directory -Path (Split-Path -Parent $Path) -Force | Out-Null
    [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}
function Invoke-FixtureScript([string]$Script, [string]$Arguments, [switch]$Running) {
    # Only fixture directories are passed. Mock processes to test both guards
    # deterministically, never start the actual bridge/game or touch real registry.
    $processBody = 'return'
    if ($Running) { $processBody = 'return [pscustomobject]@{Id=123}' }
    $command = "function Get-Process { $processBody }; & '$($Script.Replace("'", "''"))' $Arguments 6>&1"
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
    $output = & (Join-Path $PSHOME 'powershell.exe') -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand $encoded 2>&1
    return [pscustomobject]@{Code=$LASTEXITCODE; Output=($output | Out-String)}
}

# StrictMode discovery, including optional registry properties and Steam fallback.
. (Join-Path $package 'Cyberpunk017.Common.ps1')
$script:registryMock = @{}
function Get-ItemProperty { param($LiteralPath, $ErrorAction); return $script:registryMock[$LiteralPath] }
Assert-That ($null -eq (Get-OptionalRegistryString 'absent' 'InstallLocation')) 'missing registry key is harmless'
$script:registryMock['absent'] = [pscustomobject]@{DisplayName='No install property'}
Assert-That ($null -eq (Get-OptionalRegistryString 'absent' 'InstallLocation')) 'missing registry property is harmless'
$steam = Join-Path $scratch 'Steam'
$library = Join-Path $scratch 'Other Library'
$game = Join-Path $library 'steamapps\common\Cyberpunk 2077'
Write-Fixture (Join-Path $game 'bin\x64\Cyberpunk2077.exe') 'test fixture, not executable'
Write-Fixture (Join-Path $steam 'steamapps\libraryfolders.vdf') ('"libraryfolders" { "1" { "path" "' + $library.Replace('\','\\') + '" } }')
$script:registryMock['HKCU:\Software\Valve\Steam'] = [pscustomobject]@{SteamPath=$steam}
Assert-That ((Find-CyberpunkRoot) -eq $game) 'Steam-library fallback after missing uninstall keys'
Assert-That ((Find-CyberpunkRoot -GameRoot $game) -eq $game) 'explicit game root'
$invalidRootRejected = $false
try { Find-CyberpunkRoot -GameRoot $scratch | Out-Null } catch { $invalidRootRejected = $true }
Assert-That $invalidRootRejected 'invalid explicit root rejected'

$bridge = Join-Path $scratch 'bridge'
Copy-Item -LiteralPath $package -Destination $bridge -Recurse
$mods = Join-Path $game 'bin\x64\plugins\cyber_engine_tweaks\mods'
$settings = Join-Path $mods 'DualSense Support\config\settings.json'
$native = Join-Path $game 'red4ext\plugins\DualSenseSupport\DualSense Support.dll'
$menu = Join-Path $mods 'PSVR2 Haptics\init.lua'
$audio = Join-Path $mods 'PSVR2 Haptics\settings.ini'
$original = '{"UDPautostart":true,"weaponsSettings":{"SMG":{"value":1}},"custom":[1,2,3]}'
Write-Fixture $settings $original
Write-Fixture $native 'original native launcher'
Write-Fixture (Join-Path $mods 'nativeSettings\init.lua') '-- fixture'
Write-Fixture (Join-Path $mods 'DualSense Support\config\DualSenseXConfig.txt') 'fixture'
Write-Fixture (Join-Path $bridge 'psvr2_toolkit_dsx_bridge_017.exe') 'not executed by check-only'
Write-Fixture (Join-Path $bridge 'psvr2_toolkit_capi_loader.dll') 'fixture'
$setup = Join-Path $bridge 'setup_cyberpunk017.ps1'
$run = Join-Path $bridge 'run_cyberpunk017.ps1'
$argsRoot = "-GameRoot '$game'"
$result = Invoke-FixtureScript $setup "$argsRoot -CheckOnly"
Assert-That ($result.Code -eq 0) "setup check succeeds: $($result.Output)"
Assert-That ((Get-Content $settings -Raw) -eq $original -and (Test-Path $native) -and -not (Test-Path $menu)) 'check-only does not modify fixture'
$result = Invoke-FixtureScript $setup "$argsRoot -Apply" -Running
Assert-That ($result.Code -ne 0 -and -not (Test-Path $menu)) 'setup refuses running game'
$result = Invoke-FixtureScript $setup "$argsRoot -Apply"
Assert-That ($result.Code -eq 0) "setup succeeds: $($result.Output)"
$newSettings = Get-Content $settings -Raw | ConvertFrom-Json
Assert-That ($newSettings.UDPautostart -eq $false -and $newSettings.weaponsSettings.SMG.value -eq 1 -and $newSettings.custom.Count -eq 3) 'only UDP ownership setting changes'
Assert-That (-not (Test-Path $native) -and (Test-Path $menu)) 'native DLL moved and companion installed'
$backups = @(Get-ChildItem (Join-Path $game 'PSVR2Bridge-backups') -Directory)
Assert-That ($backups.Count -eq 1 -and (Get-Content (Join-Path $backups[0].FullName 'EDS-settings.json') -Raw) -eq $original) 'EDS backup is byte-preserved'
Assert-That ((Get-Content (Join-Path $backups[0].FullName 'DualSense Support.dll') -Raw) -eq 'original native launcher') 'native DLL recoverable'
Write-Fixture $audio "Version=1`nAudioEnabled=1`nAudioStrength=80`n"
$savedAudio = Get-Content $audio -Raw
$result = Invoke-FixtureScript $setup "$argsRoot -Apply"
Assert-That ($result.Code -eq 0 -and @(Get-ChildItem (Join-Path $game 'PSVR2Bridge-backups') -Directory).Count -eq 1) 'repeat setup is no-op'
Assert-That ((Get-Content $audio -Raw) -eq $savedAudio) 'repeat setup preserves audio preferences'
Write-Fixture $menu '-- previous companion version'
$result = Invoke-FixtureScript $setup "$argsRoot -Apply"
Assert-That ($result.Code -eq 0 -and (Get-FileHash $menu).Hash -eq (Get-FileHash (Join-Path $package 'cet\PSVR2 Haptics\init.lua')).Hash) 'companion upgrade replaces only its code'
Assert-That ((Get-Content $audio -Raw) -eq $savedAudio) 'companion upgrade preserves preferences'
$result = Invoke-FixtureScript $run "$argsRoot -CheckOnly"
Assert-That ($result.Code -eq 0) "launcher preflight succeeds: $($result.Output)"
$result = Invoke-FixtureScript $run "$argsRoot -CheckOnly" -Running
Assert-That ($result.Code -ne 0 -and $result.Output -match 'Only one Sense actuator owner') 'launcher rejects duplicate client'
Write-Fixture $native 'EDS reinstall fixture'
$result = Invoke-FixtureScript $run "$argsRoot -CheckOnly"
Assert-That ($result.Code -ne 0 -and $result.Output -match 'setup_cyberpunk017.cmd') 'native-DLL error supplies actionable setup command'
Write-Fixture $settings '{"UDPautostart":"false"}'
$result = Invoke-FixtureScript $setup "$argsRoot -Apply"
Assert-That ($result.Code -ne 0 -and (Test-Path $native)) 'unknown EDS settings format rejected before mutations'

# Driver checks/replacement/restoration are limited to a disposable fake driver directory.
$driverPackage = Join-Path $scratch 'driver'
New-Item -ItemType Directory -Path $driverPackage | Out-Null
Copy-Item $DriverDll (Join-Path $driverPackage 'driver_playstation_vr2.dll')
Copy-Item (Join-Path $package 'install_raw_trigger_driver.ps1') $driverPackage
$driverTarget = Join-Path $scratch 'Fake PSVR2 App\bin\win64'
$installed = Join-Path $driverTarget 'driver_playstation_vr2.dll'
Write-Fixture $installed 'old driver fixture'
$installer = Join-Path $driverPackage 'install_raw_trigger_driver.ps1'
$argsDriver = "-DriverDirectory '$driverTarget'"
$result = Invoke-FixtureScript $installer "$argsDriver -Install"
Assert-That ($result.Code -ne 0 -and $result.Output -match 'Base PSVR2Toolkit installation is incomplete' -and (Get-Content $installed -Raw) -eq 'old driver fixture') 'stock driver not replaced without base Toolkit files'
foreach ($dependency in @('driver_playstation_vr2_orig.dll', 'libcrossipc.dll', 'libusb-1.0.dll')) {
    Write-Fixture (Join-Path $driverTarget $dependency) 'base Toolkit fixture'
}
$result = Invoke-FixtureScript $installer "$argsDriver -CheckOnly"
Assert-That ($result.Code -ne 0 -and $result.Output -match 'missing psvr2_toolkit_capi.dll' -and (Get-Content $installed -Raw) -eq 'old driver fixture') 'incomplete Toolkit API diagnosed without mutation'
Write-Fixture (Join-Path $driverTarget 'psvr2_toolkit_capi.dll') 'base Toolkit fixture'
$result = Invoke-FixtureScript $installer "$argsDriver -CheckOnly"
Assert-That ($result.Code -eq 0 -and (Get-Content $installed -Raw) -eq 'old driver fixture') 'driver check-only preserves different driver'
$result = Invoke-FixtureScript $installer "$argsDriver -Install" -Running
Assert-That ($result.Code -ne 0 -and (Get-Content $installed -Raw) -eq 'old driver fixture') 'driver replacement blocked while SteamVR runs'
$result = Invoke-FixtureScript $installer "$argsDriver -Install"
Assert-That ($result.Code -eq 0 -and (Get-FileHash $installed).Hash -eq (Get-FileHash $DriverDll).Hash) 'explicit driver install and hash verification'
$driverBackups = @(Get-ChildItem $driverTarget -Filter '*.bak')
Assert-That ($driverBackups.Count -eq 1 -and (Get-Content $driverBackups[0].FullName -Raw) -eq 'old driver fixture') 'driver backup preserved'
$result = Invoke-FixtureScript $installer "$argsDriver -Install" -Running
Assert-That ($result.Code -eq 0 -and @(Get-ChildItem $driverTarget -Filter '*.bak').Count -eq 1) 'identical driver skipped even if SteamVR runs'
$result = Invoke-FixtureScript $installer "$argsDriver -RestoreBackup '$($driverBackups[0].FullName)' -Install"
Assert-That ($result.Code -eq 0 -and (Get-Content $installed -Raw) -eq 'old driver fixture') 'rollback restores exact original driver'
$result = Invoke-FixtureScript $installer "$argsDriver -RestoreBackup '$installed' -Install"
Assert-That ($result.Code -ne 0) 'rollback refuses a non-backup target'
Write-Host "$passed package assertions passed. Disposable evidence retained at $scratch"
