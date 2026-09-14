<#
.SYNOPSIS
    Publishes the self-contained build WinKit.iss packages, then compiles the
    installer with Inno Setup if ISCC.exe is available.

.DESCRIPTION
    Publishes WinKit.UI as a self-contained, win-x64, ReadyToRun build (no
    .NET runtime install required). WinKit.Updater.exe is NOT included here -
    the installed (Program Files) copy self-updates by downloading and running
    a new WinKitSetup-<version>.exe (see UpdateService.ApplyViaInstallerAsync),
    not the portable updater helper.

    If Inno Setup's compiler (ISCC.exe) is found on PATH or in one of its
    common install locations, this also compiles WinKit.iss into
    installer\output\WinKitSetup-<version>.exe. Otherwise it just publishes
    the files the .iss script expects and tells you how to compile it yourself.

.PARAMETER Version
    Version to stamp into the assemblies and pass to the installer.
    Defaults to the version already set in WinKit.UI.csproj.

.EXAMPLE
    .\installer\Publish-Installer.ps1
    .\installer\Publish-Installer.ps1 -Version 1.2.0
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

Write-Host "Publishing WinKit for the installer, v$Version ($rid, self-contained)..." -ForegroundColor Cyan

$publishDir = Join-Path $repoRoot "src\WinKit.UI\bin\Release\net9.0-windows\$rid\publish"

& dotnet publish (Join-Path $repoRoot "src\WinKit.UI\WinKit.UI.csproj") `
    -c Release -r $rid --self-contained true `
    -p:Version=$Version -p:AssemblyVersion="$Version.0" -p:FileVersion="$Version.0" `
    -p:PublishReadyToRun=true -p:PublishSingleFile=false
if ($LASTEXITCODE -ne 0) { throw "dotnet publish failed for WinKit.UI" }

Get-ChildItem $publishDir -Filter "*.pdb" -Recurse | Remove-Item -Force

New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

$isccPath = (Get-Command "ISCC.exe" -ErrorAction SilentlyContinue).Source
if (-not $isccPath) {
    $candidates = @(
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
    )
    $isccPath = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

if (-not $isccPath) {
    Write-Warning "Inno Setup (ISCC.exe) was not found. Published files are ready at:`n  $publishDir`nInstall Inno Setup (https://jrsoftware.org/isinfo.php) and run:`n  ISCC.exe /DMyAppVersion=$Version installer\WinKit.iss"
    return
}

$issPath = Join-Path $PSScriptRoot "WinKit.iss"
Write-Host "Compiling installer with $isccPath..." -ForegroundColor DarkCyan
& $isccPath "/DMyAppVersion=$Version" $issPath
if ($LASTEXITCODE -ne 0) { throw "ISCC.exe failed" }

$exePath = Join-Path $outputDirectory "WinKitSetup-$Version.exe"
if (Test-Path $exePath) {
    $sha256 = (Get-FileHash -Path $exePath -Algorithm SHA256).Hash
    $sizeMb = [math]::Round((Get-Item $exePath).Length / 1MB, 1)
    Write-Host ""
    Write-Host "Done: $exePath ($sizeMb MB)" -ForegroundColor Green
    Write-Host "SHA256: $sha256"
}
