#Requires -Version 5.1
<#
Publishes WinKit (plus its WinKit.Updater helper) as a self-contained win-x64
Release build and zips it into a portable, no-install distribution, alongside
a .sha256 checksum the auto-updater verifies before applying an update.
Version defaults to the value in WinKit.UI.csproj so the archive name stays
in sync with the app.
#>
param(
    [string]$Version
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$uiProject = Join-Path $repoRoot "src\WinKit.UI\WinKit.UI.csproj"
$updaterProject = Join-Path $repoRoot "src\WinKit.Updater\WinKit.Updater.csproj"
$publishDir = Join-Path $repoRoot "publish\win-x64"
$outputDir = Join-Path $PSScriptRoot "output"

if (-not $Version) {
    $csprojContent = Get-Content $uiProject -Raw
    if ($csprojContent -match '<Version>([^<]+)</Version>') {
        $Version = $Matches[1]
    } else {
        throw "Could not determine version from $uiProject"
    }
}

Write-Host "Publishing WinKit $Version (self-contained win-x64)..."
if (Test-Path $publishDir) {
    Remove-Item $publishDir -Recurse -Force
}

dotnet publish $uiProject -c Release -r win-x64 --self-contained true -o $publishDir
if ($LASTEXITCODE -ne 0) {
    throw "dotnet publish failed"
}

Write-Host "Publishing WinKit.Updater helper..."
dotnet publish $updaterProject -c Release -r win-x64 --self-contained true -o $publishDir
if ($LASTEXITCODE -ne 0) {
    throw "dotnet publish (WinKit.Updater) failed"
}

if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

$zipPath = Join-Path $outputDir "WinKit-$Version-win-x64-portable.zip"
if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}

Write-Host "Creating portable archive..."
Compress-Archive -Path (Join-Path $publishDir "*") -DestinationPath $zipPath -CompressionLevel Optimal

# GitHub computes and serves a SHA256 digest for every uploaded release asset, so a
# separate .sha256 sidecar file isn't needed — the auto-updater reads that digest
# directly from the Releases API. Printed here only as a local sanity check.
$hash = (Get-FileHash -Path $zipPath -Algorithm SHA256).Hash

Write-Host "Portable build complete: portable\output\WinKit-$Version-win-x64-portable.zip"
Write-Host "Checksum: $hash"
