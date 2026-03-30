# REVOLTEACS + REVOLTELAUNCHER Build Script
# Builds Release|Win32 and copies outputs to .\Release\

param(
    [string]$Configuration = "Release",
    [string]$Platform = "x86"
)

$ErrorActionPreference = "Stop"
$SolutionDir = $PSScriptRoot
$OutputDir = Join-Path $SolutionDir "Release"

# Find MSBuild via vswhere
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere not found. Is Visual Studio 2022 installed?"
    exit 1
}

$vsPath = & $vswhere -latest -requires Microsoft.Component.MSBuild -property installationPath
if (-not $vsPath) {
    Write-Error "Visual Studio with MSBuild not found."
    exit 1
}

$msbuild = Join-Path $vsPath "MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path $msbuild)) {
    Write-Error "MSBuild.exe not found at: $msbuild"
    exit 1
}

Write-Host "Using MSBuild: $msbuild" -ForegroundColor Cyan

# Ensure output directory exists
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
}

# --- Build REVOLTEACS solution (DLL + OffsetScanner) ---
$revoltSln = Join-Path $SolutionDir "REVOLTEACS.sln"
Write-Host "`nBuilding REVOLTEACS ($Configuration|$Platform) ..." -ForegroundColor Cyan
& $msbuild $revoltSln /p:Configuration=$Configuration /p:Platform=$Platform /m /verbosity:minimal
if ($LASTEXITCODE -ne 0) {
    Write-Error "REVOLTEACS build failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

# Copy REVOLTEACS.dll
$revoltDll = Join-Path $SolutionDir "REVOLTEACS\$Configuration\REVOLTEACS.dll"
if (Test-Path $revoltDll) {
    Copy-Item $revoltDll -Destination $OutputDir -Force
    Write-Host "Copied REVOLTEACS.dll -> Release\" -ForegroundColor Green
} else {
    Write-Warning "REVOLTEACS.dll not found at: $revoltDll"
}

# --- Build REVOLTELAUNCHER ---
$launcherSln = Join-Path $SolutionDir "REVOLTELAUNCHER\Launcher.sln"
if (Test-Path $launcherSln) {
    Write-Host "`nBuilding REVOLTELAUNCHER ($Configuration|$Platform) ..." -ForegroundColor Cyan
    & $msbuild $launcherSln /p:Configuration=$Configuration /p:Platform=$Platform /m /verbosity:minimal
    if ($LASTEXITCODE -ne 0) {
        Write-Error "REVOLTELAUNCHER build failed with exit code $LASTEXITCODE"
        exit $LASTEXITCODE
    }

    # Copy Launcher.exe — check both possible output paths
    $launcherExe = Join-Path $SolutionDir "REVOLTELAUNCHER\$Configuration\Launcher.exe"
    if (-not (Test-Path $launcherExe)) {
        $launcherExe = Join-Path $SolutionDir "REVOLTELAUNCHER\Win32\$Configuration\Launcher.exe"
    }
    if (-not (Test-Path $launcherExe)) {
        $launcherExe = Join-Path $SolutionDir "REVOLTELAUNCHER\Debug\Launcher.exe"
    }
    if (Test-Path $launcherExe) {
        Copy-Item $launcherExe -Destination $OutputDir -Force
        Write-Host "Copied Launcher.exe -> Release\" -ForegroundColor Green
    } else {
        Write-Warning "Launcher.exe not found after build"
    }
} else {
    Write-Warning "REVOLTELAUNCHER\Launcher.sln not found, skipping launcher build"
}

Write-Host "`nBuild complete! Output in: $OutputDir" -ForegroundColor Green