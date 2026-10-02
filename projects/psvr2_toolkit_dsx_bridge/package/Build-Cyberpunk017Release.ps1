#requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$BuildBin,
    [Parameter(Mandatory = $true)][string]$OutputZip
)
$ErrorActionPreference = 'Stop'
$sourceRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..\..')).Path
$packageRoot = (Resolve-Path -LiteralPath $PSScriptRoot).Path
$bin = (Resolve-Path -LiteralPath $BuildBin).Path
$zipPath = [IO.Path]::GetFullPath($OutputZip)
$outDir = Split-Path -Parent $zipPath
if (-not (Test-Path -LiteralPath $outDir -PathType Container)) { throw "Output directory missing: $outDir" }
if (Test-Path -LiteralPath $zipPath) { throw "Output ZIP already exists: $zipPath" }
$staging = Join-Path $outDir ('cyberpunk017-stage-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $staging | Out-Null
try {
    $files = @(
        @{ Source=(Join-Path $bin 'psvr2_toolkit_dsx_bridge_017.exe'); Name='psvr2_toolkit_dsx_bridge_017.exe' },
        @{ Source=(Join-Path $bin 'psvr2_toolkit_capi_loader.dll'); Name='psvr2_toolkit_capi_loader.dll' },
        @{ Source=(Join-Path $packageRoot 'run_cyberpunk017.cmd'); Name='run_cyberpunk017.cmd' },
        @{ Source=(Join-Path $packageRoot 'run_cyberpunk017.ps1'); Name='run_cyberpunk017.ps1' },
        @{ Source=(Join-Path $sourceRoot 'projects\psvr2_toolkit_dsx_bridge\README.md'); Name='README.md' },
        @{ Source=(Join-Path $sourceRoot 'LICENSE'); Name='LICENSE' }
    )
    foreach ($file in $files) {
        if (-not (Test-Path -LiteralPath $file.Source -PathType Leaf)) { throw "Release input missing: $($file.Source)" }
        Copy-Item -LiteralPath $file.Source -Destination (Join-Path $staging $file.Name)
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
