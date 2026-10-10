#requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$BuildBin,
    [Parameter(Mandatory = $true)][string]$DriverDll,
    [Parameter(Mandatory = $true)][string]$OutputZip
)
$ErrorActionPreference = 'Stop'
$sourceRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..\..')).Path
$packageRoot = (Resolve-Path -LiteralPath $PSScriptRoot).Path
$bin = (Resolve-Path -LiteralPath $BuildBin).Path
$driver = (Resolve-Path -LiteralPath $DriverDll).Path
# The raw-trigger driver from the v0.2.0 full package is already headset-tested
# and unchanged by the 0.1.7 bridge update. Fail closed on another driver.
$expectedDriverSha256 = '977856e7c1d54a8aea0bf4ef19095ce901fb658d301ca4ba6bf8e077d83a2b43'
if ((Get-FileHash -LiteralPath $driver -Algorithm SHA256).Hash -ine $expectedDriverSha256) {
    throw 'Driver hash differs from the verified v0.2.0 raw-trigger driver; review before packaging.'
}
$zipPath = [IO.Path]::GetFullPath($OutputZip)
$outDir = Split-Path -Parent $zipPath
if (-not (Test-Path -LiteralPath $outDir -PathType Container)) { throw "Output directory missing: $outDir" }
if (Test-Path -LiteralPath $zipPath) { throw "Output ZIP already exists: $zipPath" }
$staging = Join-Path $outDir ('cyberpunk017-stage-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $staging | Out-Null
try {
    $files = @(
        @{ Source=(Join-Path $bin 'psvr2_toolkit_dsx_bridge_017.exe'); Name='bridge\psvr2_toolkit_dsx_bridge_017.exe' },
        @{ Source=(Join-Path $bin 'psvr2_toolkit_capi_loader.dll'); Name='bridge\psvr2_toolkit_capi_loader.dll' },
        @{ Source=$driver; Name='driver\driver_playstation_vr2.dll' },
        @{ Source=(Join-Path $packageRoot 'INSTALL_RAW_TRIGGER_DRIVER.cmd'); Name='driver\INSTALL_RAW_TRIGGER_DRIVER.cmd' },
        @{ Source=(Join-Path $packageRoot 'install_raw_trigger_driver.ps1'); Name='driver\install_raw_trigger_driver.ps1' },
        @{ Source=(Join-Path $packageRoot 'START_HERE_017.txt'); Name='START_HERE.txt' },
        @{ Source=(Join-Path $packageRoot 'run_cyberpunk017.cmd'); Name='bridge\run_cyberpunk017.cmd' },
        @{ Source=(Join-Path $packageRoot 'run_cyberpunk017.ps1'); Name='bridge\run_cyberpunk017.ps1' },
        @{ Source=(Join-Path $packageRoot 'Cyberpunk017.Common.ps1'); Name='bridge\Cyberpunk017.Common.ps1' },
        @{ Source=(Join-Path $packageRoot 'setup_cyberpunk017.cmd'); Name='bridge\setup_cyberpunk017.cmd' },
        @{ Source=(Join-Path $packageRoot 'setup_cyberpunk017.ps1'); Name='bridge\setup_cyberpunk017.ps1' },
        @{ Source=(Join-Path $packageRoot 'cet\PSVR2 Haptics\init.lua'); Name='bridge\cet\PSVR2 Haptics\init.lua' },
        @{ Source=(Join-Path $sourceRoot 'projects\psvr2_toolkit_dsx_bridge\README.md'); Name='README.md' },
        @{ Source=(Join-Path $sourceRoot 'LICENSE'); Name='LICENSE' }
    )
    foreach ($file in $files) {
        if (-not (Test-Path -LiteralPath $file.Source -PathType Leaf)) { throw "Release input missing: $($file.Source)" }
        $destination = Join-Path $staging $file.Name
        New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
        Copy-Item -LiteralPath $file.Source -Destination $destination
    }
    $hashes = foreach ($file in $files) {
        $hash = (Get-FileHash -LiteralPath (Join-Path $staging $file.Name) -Algorithm SHA256).Hash.ToLowerInvariant()
        "$hash  $($file.Name)"
    }
    [IO.File]::WriteAllLines((Join-Path $staging 'SHA256SUMS.txt'), $hashes, [Text.UTF8Encoding]::new($false))
    Compress-Archive -Path (Join-Path $staging '*') -DestinationPath $zipPath -CompressionLevel Optimal
    Write-Host "Package: $zipPath"
    Write-Host "SHA-256: $((Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant())"
} finally {
    if (Test-Path -LiteralPath $staging -PathType Container) { Remove-Item -LiteralPath $staging -Recurse -Force }
}
