# TODO: UNTESTED — written on Linux, needs validation on a real Windows machine.
# fc_tool build script for Windows
#
# Prerequisites:
#   1. Run install-system-deps.ps1 as Administrator (once)
#   2. Run setup-dev.ps1 (once)
#
# Usage:
#   .\build.ps1          # build release binary
#   .\build.ps1 dev      # run in development mode (live reload)

param(
    [Parameter(Position = 0)]
    [ValidateSet("build", "dev")]
    [string]$Mode = "build"
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectDir = (Resolve-Path "$ScriptDir\..\..").Path

# Source Rust if needed
if (-not (Get-Command cargo -ErrorAction SilentlyContinue)) {
    $cargoEnv = "$env:USERPROFILE\.cargo\bin"
    if (Test-Path $cargoEnv) {
        $env:PATH = "$cargoEnv;$env:PATH"
    }
    else {
        Write-Host "Error: cargo not found. Run setup-dev.ps1 first." -ForegroundColor Red
        exit 1
    }
}

# Source PlatformIO if available (optional — for firmware compile/flash features)
$pioPenv = "$env:USERPROFILE\.platformio\penv\Scripts"
if (Test-Path $pioPenv) {
    $env:PATH = "$pioPenv;$env:PATH"
}

# Verify toolchains
foreach ($cmd in @("cargo", "node", "npm")) {
    if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) {
        Write-Host "Error: $cmd not found. Run setup-dev.ps1 first." -ForegroundColor Red
        exit 1
    }
}

Push-Location $ProjectDir

switch ($Mode) {
    "dev" {
        Write-Host "=== Starting fc_tool in development mode ===" -ForegroundColor Cyan
        npm run tauri dev
    }
    "build" {
        Write-Host "=== Building fc_tool release binary ===" -ForegroundColor Cyan
        npm run tauri build

        Write-Host ""
        Write-Host "=== Build artifacts ===" -ForegroundColor Green
        $bundleDir = "$ProjectDir\src-tauri\target\release\bundle"
        if (Test-Path $bundleDir) {
            Get-ChildItem -Path $bundleDir -Recurse -Include "*.msi", "*.exe" | ForEach-Object {
                $size = [math]::Round($_.Length / 1MB, 1)
                Write-Host "  $($_.Name)  (${size} MB)"
            }
        }
    }
}

Pop-Location
