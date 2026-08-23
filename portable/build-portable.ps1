#Requires -Version 5.1
<#
Publishes WinKit as a self-contained win-x64 Release build and zips it
into a portable, no-install distribution. Version defaults to the value
in WinKit.UI.csproj so the archive name stays in sync with the app.
#>
param(
    [string]$Version
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$uiProject = Join-Path $repoRoot "src\WinKit.UI\WinKit.UI.csproj"
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

if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

$zipPath = Join-Path $outputDir "WinKit-$Version-win-x64-portable.zip"
if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}

Write-Host "Creating portable archive..."
Compress-Archive -Path (Join-Path $publishDir "*") -DestinationPath $zipPath -CompressionLevel Optimal

Write-Host "Portable build complete: portable\output\WinKit-$Version-win-x64-portable.zip"
