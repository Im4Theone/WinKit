#Requires -Version 5.1
<#
Publishes WinKit as a self-contained win-x64 Release build and compiles
the Inno Setup installer from it. Version defaults to the value in
WinKit.UI.csproj so the installer stays in sync with the app.
#>
param(
    [string]$Version
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$uiProject = Join-Path $repoRoot "src\WinKit.UI\WinKit.UI.csproj"
$publishDir = Join-Path $repoRoot "publish\win-x64"
$issScript = Join-Path $PSScriptRoot "WinKit.iss"

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

$isccCommand = Get-Command ISCC.exe -ErrorAction SilentlyContinue
if ($isccCommand) {
    $isccPath = $isccCommand.Source
} else {
    $candidates = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles}\Inno Setup 6\ISCC.exe",
        "${env:LOCALAPPDATA}\Programs\Inno Setup 6\ISCC.exe"
    )
    $isccPath = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $isccPath) {
        throw "ISCC.exe (Inno Setup compiler) not found. Install Inno Setup 6 (https://jrsoftware.org/isinfo.php)."
    }
}

Write-Host "Compiling installer with Inno Setup..."
& $isccPath "/DMyAppVersion=$Version" $issScript
if ($LASTEXITCODE -ne 0) {
    throw "ISCC compilation failed"
}

$setupPath = Join-Path $PSScriptRoot "output\WinKitSetup-$Version.exe"

# GitHub computes and serves a SHA256 digest for every uploaded release asset, so a
# separate .sha256 sidecar file isn't needed — the auto-updater reads that digest
# directly from the Releases API. Printed here only as a local sanity check.
$hash = (Get-FileHash -Path $setupPath -Algorithm SHA256).Hash

Write-Host "Installer build complete: installer\output\WinKitSetup-$Version.exe"
Write-Host "Checksum: $hash"
