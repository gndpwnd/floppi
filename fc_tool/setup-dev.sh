#!/usr/bin/env bash
# fc_tool development environment setup for Ubuntu/Debian
# Installs Rust, Node.js, and npm dependencies. No sudo required.
#
# For system packages (build-essential, libwebkit2gtk, etc.), run first:
#   sudo ./install-system-deps.sh
#
# Usage:
#   chmod +x setup-dev.sh
#   ./setup-dev.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

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

echo "[1/4] Checking system dependencies..."

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

echo "[2/4] Checking Rust toolchain..."

if [ -f "$HOME/.cargo/env" ]; then
    . "$HOME/.cargo/env"
fi

if command -v rustc &>/dev/null; then
    echo "  Rust already installed: $(rustc --version)"
else
    echo "  Installing Rust via rustup..."
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
    . "$HOME/.cargo/env"
fi

echo "  rustc: $(rustc --version)"
echo "  cargo: $(cargo --version)"

# ── 3. Node.js ──────────────────────────────────────────────────────

echo "[3/4] Checking Node.js..."

export NVM_DIR="${NVM_DIR:-$HOME/.nvm}"
if [ -s "$NVM_DIR/nvm.sh" ]; then
    . "$NVM_DIR/nvm.sh"
fi

if command -v node &>/dev/null; then
    echo "  Node.js already installed: $(node --version)"
else
    echo "  Installing Node.js 20 via nvm..."
    if [ ! -s "$NVM_DIR/nvm.sh" ]; then
        curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.1/install.sh | bash
        . "$NVM_DIR/nvm.sh"
    fi
    nvm install 20
fi

echo "  node: $(node --version)"
echo "  npm:  $(npm --version)"

# ── 4. npm dependencies ─────────────────────────────────────────────

echo "[4/4] Installing npm dependencies..."
cd "$SCRIPT_DIR"
npm install

# ── Done ─────────────────────────────────────────────────────────────

echo ""
echo "=== Setup complete ==="
echo ""
echo "To build and run fc_tool:"
echo "  cd $SCRIPT_DIR"
echo "  npm run tauri dev       # development mode"
echo "  npm run tauri build     # release binary"
echo ""
echo "The release binary will be in src-tauri/target/release/bundle/"
