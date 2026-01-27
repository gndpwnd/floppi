#!/usr/bin/env bash
# fc_tool build script
# Compiles the Rust backend and frontend into a release binary.
#
# Prerequisites:
#   1. sudo ./install-system-deps.sh   (system libraries, once)
#   2. ./setup-dev.sh                  (Rust, Node.js, npm deps)
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

# Verify toolchains are available
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
        RELEASE_DIR="$PROJECT_DIR/src-tauri/target/release"
        if [ -f "$RELEASE_DIR/fc_tool" ]; then
            echo "  Binary:   $RELEASE_DIR/fc_tool"
            ls -lh "$RELEASE_DIR/fc_tool" | awk '{print "  Size:     "$5}'
        fi
        BUNDLE_DIR="$RELEASE_DIR/bundle"
        if [ -d "$BUNDLE_DIR" ]; then
            echo "  Bundles:"
            find "$BUNDLE_DIR" -maxdepth 2 -type f \( -name "*.deb" -o -name "*.rpm" -o -name "*.AppImage" \) 2>/dev/null | while read -r f; do
                echo "    $(basename "$f")  ($(du -h "$f" | cut -f1))"
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
