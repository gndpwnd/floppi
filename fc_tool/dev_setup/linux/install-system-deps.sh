#!/usr/bin/env bash
# fc_tool system dependencies (requires sudo)
# Run this ONCE before setup-dev.sh on a fresh Ubuntu/Debian system.
#
# Usage:
#   chmod +x install-system-deps.sh
#   sudo ./install-system-deps.sh

set -euo pipefail

if [ "$EUID" -ne 0 ]; then
    echo "This script must be run with sudo:"
    echo "  sudo ./install-system-deps.sh"
    exit 1
fi

echo "=== Installing fc_tool system dependencies ==="

apt-get update
apt-get install -y \
    build-essential \
    pkg-config \
    libwebkit2gtk-4.1-dev \
    librsvg2-dev \
    libssl-dev \
    libgtk-3-dev \
    libayatana-appindicator3-dev \
    libudev-dev \
    xdg-utils \
    python3 \
    python3-venv \
    curl \
    wget \
    file

echo ""
echo "=== System dependencies installed ==="
echo "Now run ./setup-dev.sh (no sudo needed)"
