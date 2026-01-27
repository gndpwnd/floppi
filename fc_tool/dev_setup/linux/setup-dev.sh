#!/usr/bin/env bash
# fc_tool development environment setup for Ubuntu/Debian
# Installs Rust, Node.js, PlatformIO, and npm dependencies. No sudo required.
#
# Install locations (all default paths):
#   Rust:        ~/.cargo/, ~/.rustup/
#   Node.js:     ~/.nvm/
#   PlatformIO:  ~/.platformio/
#   npm:         fc_tool/node_modules/ (project-local)
#
# For system packages (build-essential, libwebkit2gtk, etc.), run first:
#   sudo ./install-system-deps.sh
#
# Usage:
#   chmod +x setup-dev.sh
#   ./setup-dev.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo "=== fc_tool dev environment setup (Ubuntu/Debian) ==="
echo ""

# ── 1. Verify system packages ───────────────────────────────────────

REQUIRED_PKGS=(
    build-essential
    pkg-config
    libwebkit2gtk-4.1-dev
    librsvg2-dev
    libssl-dev
    libgtk-3-dev
    libayatana-appindicator3-dev
    libudev-dev
)

echo "[1/5] Checking system dependencies..."

MISSING_PKGS=()
for pkg in "${REQUIRED_PKGS[@]}"; do
    if ! dpkg -s "$pkg" &>/dev/null; then
        MISSING_PKGS+=("$pkg")
    fi
done

if [ ${#MISSING_PKGS[@]} -eq 0 ]; then
    echo "  All system packages installed."
else
    echo "  Missing: ${MISSING_PKGS[*]}"
    echo ""
    echo "  Run first:  sudo ./install-system-deps.sh"
    echo ""
    exit 1
fi

# ── 2. Rust toolchain ───────────────────────────────────────────────
# Installs to: ~/.cargo/ and ~/.rustup/

echo "[2/5] Checking Rust toolchain..."

if [ -f "$HOME/.cargo/env" ]; then
    . "$HOME/.cargo/env"
fi

if command -v rustc &>/dev/null; then
    echo "  Rust already installed: $(rustc --version)"
else
    echo "  Installing Rust via rustup..."
    echo "  Install location: ~/.cargo/ and ~/.rustup/"
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
    . "$HOME/.cargo/env"
fi

echo "  rustc: $(rustc --version)"
echo "  cargo: $(cargo --version)"

# ── 3. Node.js ──────────────────────────────────────────────────────
# Installs to: ~/.nvm/

echo "[3/5] Checking Node.js..."

export NVM_DIR="${NVM_DIR:-$HOME/.nvm}"
if [ -s "$NVM_DIR/nvm.sh" ]; then
    . "$NVM_DIR/nvm.sh"
fi

if command -v node &>/dev/null; then
    echo "  Node.js already installed: $(node --version)"
else
    echo "  Installing Node.js 20 via nvm..."
    echo "  Install location: ~/.nvm/"
    if [ ! -s "$NVM_DIR/nvm.sh" ]; then
        curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.1/install.sh | bash
        . "$NVM_DIR/nvm.sh"
    fi
    nvm install 20
fi

echo "  node: $(node --version)"
echo "  npm:  $(npm --version)"

# ── 4. PlatformIO (optional, for firmware compile/flash) ────────────
# Installs to: ~/.platformio/
# PlatformIO auto-downloads toolchains on first `pio run`.

echo "[4/5] Checking PlatformIO..."

PIO_PENV="$HOME/.platformio/penv/bin"
if [ -d "$PIO_PENV" ]; then
    export PATH="$PIO_PENV:$PATH"
fi

if command -v pio &>/dev/null; then
    echo "  PlatformIO already installed: $(pio --version)"
else
    echo "  Installing PlatformIO..."
    echo "  Install location: ~/.platformio/"
    # Use the official installer script which handles Python venv setup
    if command -v python3 &>/dev/null; then
        curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py -o /tmp/get-platformio.py
        python3 /tmp/get-platformio.py
        rm -f /tmp/get-platformio.py
        export PATH="$PIO_PENV:$PATH"
    else
        echo "  WARNING: python3 not found. PlatformIO requires Python 3."
        echo "  Install Python 3 (sudo apt-get install python3) and re-run this script."
        echo "  Skipping PlatformIO installation. fc_tool will still work for serial monitoring."
    fi
fi

if command -v pio &>/dev/null; then
    echo "  pio: $(pio --version)"
    echo "  PlatformIO will auto-download board toolchains on first firmware build."
else
    echo "  PlatformIO not installed. Compile/flash features will be unavailable."
    echo "  Serial monitoring and IMU visualization will still work."
fi

# ── 5. npm dependencies ─────────────────────────────────────────────

echo "[5/5] Installing npm dependencies..."
cd "$PROJECT_DIR"
npm install

# ── Done ─────────────────────────────────────────────────────────────

echo ""
echo "=== Setup complete ==="
echo ""
echo "Install locations:"
echo "  Rust:        ~/.cargo/ and ~/.rustup/"
echo "  Node.js:     ~/.nvm/"
echo "  PlatformIO:  ~/.platformio/"
echo "  npm:         $PROJECT_DIR/node_modules/"
echo ""
echo "To build and run fc_tool:"
echo "  cd $PROJECT_DIR"
echo "  ./dev_setup/linux/build.sh           # release binary"
echo "  ./dev_setup/linux/build.sh dev       # development mode"
echo ""
echo "The release binary will be in src-tauri/target/release/bundle/"
