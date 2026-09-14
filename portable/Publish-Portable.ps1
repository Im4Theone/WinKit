<#
.SYNOPSIS
    Builds the self-contained portable distribution of WinKit and zips it as
    WinKit-<version>-portable.zip, matching the asset name WinKit.Infrastructure's
    UpdateService looks for on GitHub Releases.

.DESCRIPTION
    Publishes WinKit.UI and WinKit.Updater as self-contained, win-x64,
    ReadyToRun executables (no .NET runtime install required on the target
    machine - this is what makes it "portable": unzip and run). Both are
    published into the same folder so WinKit.Updater.exe sits next to
    WinKit.exe, since the portable self-update path (see UpdateService.
    ApplyViaPortableUpdaterAsync) launches it from the install directory.

.PARAMETER Version
    Version to stamp into the assemblies and the output zip name.
    Defaults to the version already set in WinKit.UI.csproj.

.EXAMPLE
    .\portable\Publish-Portable.ps1
    .\portable\Publish-Portable.ps1 -Version 1.2.0
#>
[CmdletBinding()]
param(
    [string]$Version
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$rid = "win-x64"
$outputDirectory = Join-Path $PSScriptRoot "output"

if (-not $Version) {
    $csprojPath = Join-Path $repoRoot "src\WinKit.UI\WinKit.UI.csproj"
    $csproj = [xml](Get-Content $csprojPath)
    $Version = $csproj.Project.PropertyGroup.Version | Where-Object { $_ } | Select-Object -First 1
    if (-not $Version) {
        throw "Could not determine version from $csprojPath; pass -Version explicitly."
    }
}

Write-Host "Publishing WinKit portable v$Version ($rid, self-contained)..." -ForegroundColor Cyan

$stagingDir = Join-Path $outputDirectory "staging"
if (Test-Path $stagingDir) {
    Remove-Item $stagingDir -Recurse -Force
}
New-Item -ItemType Directory -Path $stagingDir -Force | Out-Null

$publishArgs = @(
    "-c", "Release"
    "-r", $rid
    "--self-contained", "true"
    "-p:Version=$Version"
    "-p:AssemblyVersion=$Version.0"
    "-p:FileVersion=$Version.0"
    "-p:PublishReadyToRun=true"
    "-p:PublishSingleFile=false"
    "-o", $stagingDir
)

Write-Host "Publishing WinKit.UI..." -ForegroundColor DarkCyan
& dotnet publish (Join-Path $repoRoot "src\WinKit.UI\WinKit.UI.csproj") @publishArgs
if ($LASTEXITCODE -ne 0) { throw "dotnet publish failed for WinKit.UI" }

Write-Host "Publishing WinKit.Updater..." -ForegroundColor DarkCyan
& dotnet publish (Join-Path $repoRoot "src\WinKit.Updater\WinKit.Updater.csproj") @publishArgs
if ($LASTEXITCODE -ne 0) { throw "dotnet publish failed for WinKit.Updater" }

# The .pdb files aren't useful to end users and just add dead weight to the download.
Get-ChildItem $stagingDir -Filter "*.pdb" -Recurse | Remove-Item -Force

New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$zipPath = Join-Path $outputDirectory "WinKit-$Version-portable.zip"
if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}

Write-Host "Zipping to $zipPath..." -ForegroundColor DarkCyan
Compress-Archive -Path (Join-Path $stagingDir "*") -DestinationPath $zipPath -CompressionLevel Optimal

$sha256 = (Get-FileHash -Path $zipPath -Algorithm SHA256).Hash
$sizeMb = [math]::Round((Get-Item $zipPath).Length / 1MB, 1)

Write-Host ""
Write-Host "Done: $zipPath ($sizeMb MB)" -ForegroundColor Green
Write-Host "SHA256: $sha256"
