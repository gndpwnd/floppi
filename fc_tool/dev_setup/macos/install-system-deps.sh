#!/usr/bin/env bash
# TODO: UNTESTED — written on Linux, needs validation on a real macOS machine.
# fc_tool system dependencies for macOS
# Run this ONCE on a fresh macOS system.
#
# Usage:
#   chmod +x install-system-deps.sh
#   ./install-system-deps.sh
#
# Note: No sudo required. Homebrew handles privileges itself.
# Xcode Command Line Tools will prompt for installation if not present.

set -euo pipefail

echo "=== Installing fc_tool system dependencies (macOS) ==="
echo ""

# Xcode Command Line Tools (provides clang, make, etc.)
echo "[1/2] Checking Xcode Command Line Tools..."
if xcode-select -p &>/dev/null; then
    echo "  Xcode CLT already installed: $(xcode-select -p)"
else
    echo "  Installing Xcode Command Line Tools..."
    echo "  A dialog will appear — click 'Install' and wait for it to finish."
    xcode-select --install
    echo ""
    echo "  After installation completes, re-run this script."
    exit 0
fi

# Tauri on macOS uses WKWebView (built into macOS) — no webkit package needed.
# No extra system libraries required beyond Xcode CLT.

echo "[2/2] Checking Homebrew (optional, for convenience)..."
if command -v brew &>/dev/null; then
    echo "  Homebrew already installed: $(brew --version | head -1)"
else
    echo "  Homebrew not found. It's optional but recommended."
    echo "  Install: /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
fi

echo ""
echo "=== System dependencies installed ==="
echo "Now run ./setup-dev.sh (no admin required)"
