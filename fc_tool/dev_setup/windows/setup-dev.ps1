# TODO: UNTESTED — written on Linux, needs validation on a real Windows machine.
# fc_tool development environment setup for Windows
# Installs Rust, Node.js, PlatformIO, and npm dependencies. No admin required.
#
# Install locations (all default paths):
#   Rust:        %USERPROFILE%\.cargo\, %USERPROFILE%\.rustup\
#   Node.js:     System-wide via winget (default Program Files location)
#   PlatformIO:  %USERPROFILE%\.platformio\
#   npm:         fc_tool\node_modules\ (project-local)
#
# Prerequisites: Run install-system-deps.ps1 first (as Administrator).
#
# Usage:
#   .\setup-dev.ps1

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectDir = (Resolve-Path "$ScriptDir\..\..").Path

Write-Host "=== fc_tool dev environment setup (Windows) ===" -ForegroundColor Cyan
Write-Host ""

# ── 1. Verify system dependencies ───────────────────────────────

Write-Host "[1/5] Checking system dependencies..." -ForegroundColor Yellow

# Check for MSVC (cl.exe)
$clPath = Get-Command cl.exe -ErrorAction SilentlyContinue
if (-not $clPath) {
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vsWhere)) {
        Write-Host "  Visual Studio Build Tools not found." -ForegroundColor Red
        Write-Host "  Run install-system-deps.ps1 as Administrator first."
        exit 1
    }
    Write-Host "  MSVC Build Tools detected (cl.exe available in VS Developer shell)."
}
else {
    Write-Host "  MSVC: $($clPath.Source)"
}

# ── 2. Rust toolchain ───────────────────────────────────────────
# Installs to: %USERPROFILE%\.cargo\ and %USERPROFILE%\.rustup\

Write-Host "[2/5] Checking Rust toolchain..." -ForegroundColor Yellow

# Ensure cargo is on PATH
$cargoBin = "$env:USERPROFILE\.cargo\bin"
if (Test-Path $cargoBin) {
    $env:PATH = "$cargoBin;$env:PATH"
}

if (Get-Command rustc -ErrorAction SilentlyContinue) {
    Write-Host "  Rust already installed: $(rustc --version)"
}
else {
    Write-Host "  Installing Rust via rustup..."
    Write-Host "  Install location: %USERPROFILE%\.cargo\ and %USERPROFILE%\.rustup\"
    $rustupInit = "$env:TEMP\rustup-init.exe"
    Invoke-WebRequest -Uri "https://win.rustup.rs/x86_64" -OutFile $rustupInit
    & $rustupInit -y
    $env:PATH = "$cargoBin;$env:PATH"
}

Write-Host "  rustc: $(rustc --version)"
Write-Host "  cargo: $(cargo --version)"

# ── 3. Node.js ──────────────────────────────────────────────────
# Installs to: default winget location (Program Files)

Write-Host "[3/5] Checking Node.js..." -ForegroundColor Yellow

if (Get-Command node -ErrorAction SilentlyContinue) {
    Write-Host "  Node.js already installed: $(node --version)"
}
else {
    Write-Host "  Installing Node.js 20 via winget..."
    winget install OpenJS.NodeJS.LTS --accept-package-agreements --accept-source-agreements
    # Refresh PATH to pick up newly installed Node
    $env:PATH = [System.Environment]::GetEnvironmentVariable("PATH", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("PATH", "User")
}

Write-Host "  node: $(node --version)"
Write-Host "  npm:  $(npm --version)"

# ── 4. PlatformIO (optional, for firmware compile/flash) ────────
# Installs to: %USERPROFILE%\.platformio\
# PlatformIO auto-downloads toolchains on first `pio run`.

Write-Host "[4/5] Checking PlatformIO..." -ForegroundColor Yellow

$pioPenv = "$env:USERPROFILE\.platformio\penv\Scripts"
if (Test-Path $pioPenv) {
    $env:PATH = "$pioPenv;$env:PATH"
}

if (Get-Command pio -ErrorAction SilentlyContinue) {
    Write-Host "  PlatformIO already installed: $(pio --version)"
}
else {
    Write-Host "  Installing PlatformIO..."
    Write-Host "  Install location: %USERPROFILE%\.platformio\"
    # Check for Python 3
    $pythonCmd = $null
    if (Get-Command python -ErrorAction SilentlyContinue) {
        $pythonCmd = "python"
    }
    elseif (Get-Command python3 -ErrorAction SilentlyContinue) {
        $pythonCmd = "python3"
    }

    if ($pythonCmd) {
        $installerPath = "$env:TEMP\get-platformio.py"
        Invoke-WebRequest -Uri "https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py" -OutFile $installerPath
        & $pythonCmd $installerPath
        Remove-Item $installerPath -ErrorAction SilentlyContinue
        if (Test-Path $pioPenv) {
            $env:PATH = "$pioPenv;$env:PATH"
        }
    }
    else {
        Write-Host "  WARNING: Python not found. PlatformIO requires Python 3." -ForegroundColor Yellow
        Write-Host "  Install Python 3 from https://www.python.org/downloads/ or via: winget install Python.Python.3.12"
        Write-Host "  Then re-run this script."
        Write-Host "  Skipping PlatformIO. fc_tool will still work for serial monitoring."
    }
}

if (Get-Command pio -ErrorAction SilentlyContinue) {
    Write-Host "  pio: $(pio --version)"
    Write-Host "  PlatformIO will auto-download board toolchains on first firmware build."
}
else {
    Write-Host "  PlatformIO not installed. Compile/flash features will be unavailable."
    Write-Host "  Serial monitoring and IMU visualization will still work."
}

# ── 5. npm dependencies ─────────────────────────────────────────

Write-Host "[5/5] Installing npm dependencies..." -ForegroundColor Yellow
Push-Location $ProjectDir
npm install
Pop-Location

# ── Done ─────────────────────────────────────────────────────────

Write-Host ""
Write-Host "=== Setup complete ===" -ForegroundColor Green
Write-Host ""
Write-Host "Install locations:"
Write-Host "  Rust:        $env:USERPROFILE\.cargo\ and $env:USERPROFILE\.rustup\"
Write-Host "  Node.js:     (winget default)"
Write-Host "  PlatformIO:  $env:USERPROFILE\.platformio\"
Write-Host "  npm:         $ProjectDir\node_modules\"
Write-Host ""
Write-Host "To build and run fc_tool:"
Write-Host "  cd $ProjectDir"
Write-Host "  .\dev_setup\windows\build.ps1           # release binary"
Write-Host "  .\dev_setup\windows\build.ps1 dev       # development mode"
Write-Host ""
Write-Host "The release binary will be in src-tauri\target\release\bundle\"
