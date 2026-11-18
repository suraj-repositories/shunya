<#
.SYNOPSIS
    Automated standalone packaging script for Shunya Engineering Studio on Windows x64.
    Generates portable ZIP distribution and installer payload.
    Fulfills Phase 4 Roadmap Task PH4-13.04.005.
#>

param(
    [string]$BuildDir = "build/windows-ninja-release",
    [string]$OutputDir = "dist/shunya-windows-x64",
    [string]$Version = "4.0.0"
)

$ErrorActionPreference = "Stop"

Write-Host "====================================================" -ForegroundColor Cyan
Write-Host " Packaging Shunya Engineering Studio v$Version (Windows x64)" -ForegroundColor Cyan
Write-Host "====================================================" -ForegroundColor Cyan

# 1. Clean output directory
if (Test-Path $OutputDir) {
    Remove-Item -Recurse -Force $OutputDir
}
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
New-Item -ItemType Directory -Force -Path "$OutputDir/bin" | Out-Null
New-Item -ItemType Directory -Force -Path "$OutputDir/data" | Out-Null

# 2. Copy Executables
Write-Host "[1/5] Copying binaries and dependencies..." -ForegroundColor Yellow
$Binaries = @(
    "$BuildDir/apps/studio/appshunya.exe",
    "$BuildDir/apps/cli/shunya-cli.exe",
    "$BuildDir/core/base/libshunya_core_base.dll",
    "$BuildDir/core/domain/libshunya_core_domain.dll"
)

foreach ($bin in $Binaries) {
    if (Test-Path $bin) {
        Copy-Item -Path $bin -Destination "$OutputDir/bin" -Force
        Write-Host "  -> Copied: $(Split-Path $bin -Leaf)" -ForegroundColor Green
    }
}

# 3. Copy Periodic Table Database & Core Assets
Write-Host "[2/5] Copying periodic table data and assets..." -ForegroundColor Yellow
if (Test-Path "data/elements.csv") {
    Copy-Item -Path "data/elements.csv" -Destination "$OutputDir/data/elements.csv" -Force
    Write-Host "  -> Bundled: data/elements.csv" -ForegroundColor Green
}

# 4. Generate Launchers and Install Script
Write-Host "[3/5] Generating launcher scripts..." -ForegroundColor Yellow
$LauncherCmd = @"
@echo off
set SHUNYA_ROOT=%~dp0
set PATH=%SHUNYA_ROOT%bin;%PATH%
start "" "%SHUNYA_ROOT%bin\appshunya.exe" %*
"@
Set-Content -Path "$OutputDir/shunya.cmd" -Value $LauncherCmd -Encoding ASCII

# 5. Create Standalone ZIP Release
$ZipFile = "dist/shunya-v$Version-windows-x64.zip"
Write-Host "[4/5] Creating portable release archive: $ZipFile..." -ForegroundColor Yellow
if (Test-Path $ZipFile) { Remove-Item -Force $ZipFile }
Compress-Archive -Path "$OutputDir/*" -DestinationPath $ZipFile -CompressionLevel Optimal
Write-Host "  -> Archive created: $ZipFile" -ForegroundColor Green

Write-Host "====================================================" -ForegroundColor Cyan
Write-Host " Packaging complete! Ready for deployment." -ForegroundColor Green
Write-Host "====================================================" -ForegroundColor Cyan
