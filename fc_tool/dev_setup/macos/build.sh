#!/usr/bin/env bash
# TODO: UNTESTED — written on Linux, needs validation on a real macOS machine.
# fc_tool build script for macOS
#
# Prerequisites:
#   1. ./install-system-deps.sh   (Xcode CLT, once)
#   2. ./setup-dev.sh             (Rust, Node.js, npm deps)
#
# Usage:
#   ./build.sh          # build release binary
#   ./build.sh dev      # run in development mode (live reload)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$PROJECT_DIR"

# Source toolchains
if [ -f "$HOME/.cargo/env" ]; then
    . "$HOME/.cargo/env"
fi
export NVM_DIR="${NVM_DIR:-$HOME/.nvm}"
if [ -s "$NVM_DIR/nvm.sh" ]; then
    . "$NVM_DIR/nvm.sh"
fi
# PlatformIO (optional — for firmware compile/flash features)
PIO_PENV="$HOME/.platformio/penv/bin"
if [ -d "$PIO_PENV" ]; then
    export PATH="$PIO_PENV:$PATH"
fi

# Verify toolchains
for cmd in cargo node npm; do
    if ! command -v "$cmd" &>/dev/null; then
        echo "Error: $cmd not found. Run ./setup-dev.sh first."
        exit 1
    fi
done

MODE="${1:-build}"

case "$MODE" in
    dev)
        echo "=== Starting fc_tool in development mode ==="
        npm run tauri dev
        ;;
    build)
        echo "=== Building fc_tool release binary ==="
        npm run tauri build
        echo ""
        echo "=== Build artifacts ==="
        BUNDLE_DIR="$PROJECT_DIR/src-tauri/target/release/bundle"
        if [ -d "$BUNDLE_DIR" ]; then
            find "$BUNDLE_DIR" -maxdepth 2 -type f \( -name "*.dmg" -o -name "*.app" \) 2>/dev/null | while read -r f; do
                echo "  $(basename "$f")  ($(du -h "$f" | cut -f1))"
            done
        fi
        ;;
    *)
        echo "Usage: ./build.sh [dev|build]"
        echo "  dev    Run in development mode with live reload"
        echo "  build  Compile release binary (default)"
        exit 1
        ;;
esac
