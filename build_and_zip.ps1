# ClipX Build and Package Script (PowerShell)
# This script builds the Release version and creates a portable ZIP package

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "ClipX Build and Package Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if build directory exists
if (-not (Test-Path "build")) {
    Write-Host "Creating build directory..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path "build" | Out-Null
}

# Configure and build
Push-Location build

Write-Host "Configuring CMake..." -ForegroundColor Yellow
cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    Pop-Location
    exit 1
}

Write-Host ""
Write-Host "Building Release version..." -ForegroundColor Yellow
cmake --build . --config Release
if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    Pop-Location
    exit 1
}

Pop-Location

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "Build completed successfully!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""

# Create package
$packageDir = "package"
$releaseDir = "build\bin\Release"

# Get version from git tag, fallback to "0.0.1" if no tag
try {
    $tagVersion = git describe --tags --abbrev=0 2>$null
    if ($tagVersion -match "^V?(\d+\.\d+\.\d+)") {
        $version = $matches[1]
    } else {
        $version = "0.0.1"
    }
} catch {
    $version = "0.0.1"
}

$zipName = "ClipX-v$version-Win64.zip"

Write-Host "Creating portable package..." -ForegroundColor Yellow
if (Test-Path $packageDir) {
    Remove-Item -Recurse -Force $packageDir
}
New-Item -ItemType Directory -Path $packageDir | Out-Null

# Copy files
Write-Host "Copying executables..."
Copy-Item "$releaseDir\ClipX.exe" -Destination $packageDir
Copy-Item "$releaseDir\Overlay.exe" -Destination $packageDir
Copy-Item "README_USER.md" -Destination "$packageDir\README.md"

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "Creating ZIP archive..." -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green

# Create ZIP
Compress-Archive -Path "$packageDir\*" -DestinationPath $zipName -Force

Write-Host ""
Write-Host "Package created: $zipName" -ForegroundColor Cyan
Write-Host "Contents:" -ForegroundColor Cyan
Get-ChildItem $packageDir | ForEach-Object { Write-Host "  - $($_.Name)" }
Write-Host ""

# Cleanup package directory
Remove-Item -Recurse -Force $packageDir

Write-Host "========================================" -ForegroundColor Green
Write-Host "Done! ZIP file: $zipName" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
