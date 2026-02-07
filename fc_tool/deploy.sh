#!/bin/bash
# fc_tool deployment and development script
# Usage: ./deploy.sh [command]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Source environment
if [ -f "$HOME/.cargo/env" ]; then
    source "$HOME/.cargo/env"
fi

if [ -f "$HOME/.nvm/nvm.sh" ]; then
    source "$HOME/.nvm/nvm.sh"
fi

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_usage() {
    echo -e "${BLUE}fc_tool deployment script${NC}"
    echo ""
    echo "Usage: ./deploy.sh [command]"
    echo ""
    echo "Commands:"
    echo "  dev         Start development server (hot reload)"
    echo "  build       Build release binary"
    echo "  run         Run the release binary"
    echo "  test        Run tests"
    echo "  check       Run cargo check (fast compile check)"
    echo "  clean       Clean build artifacts"
    echo "  install     Install npm dependencies"
    echo "  serial      List available serial ports"
    echo "  help        Show this help message"
    echo ""
    echo "Examples:"
    echo "  ./deploy.sh dev       # Start dev server"
    echo "  ./deploy.sh build     # Build release"
    echo "  ./deploy.sh serial    # Check serial ports"
}

cmd_dev() {
    echo -e "${GREEN}Starting development server...${NC}"
    npm run tauri dev
}

cmd_build() {
    echo -e "${GREEN}Building release binary...${NC}"
    npm run tauri build
    echo -e "${GREEN}Build complete!${NC}"
    echo "Output: src-tauri/target/release/fc_tool"
    ls -lh src-tauri/target/release/fc_tool 2>/dev/null || true
}

cmd_run() {
    BINARY="src-tauri/target/release/fc_tool"
    if [ ! -f "$BINARY" ]; then
        echo -e "${YELLOW}Release binary not found. Building...${NC}"
        cmd_build
    fi
    echo -e "${GREEN}Running fc_tool...${NC}"
    "$BINARY"
}

cmd_test() {
    echo -e "${GREEN}Running tests...${NC}"
    cd src-tauri
    cargo test
    cd ..
    echo -e "${GREEN}Tests complete!${NC}"
}

cmd_check() {
    echo -e "${GREEN}Running cargo check...${NC}"
    cd src-tauri
    cargo check
    cd ..
    echo -e "${GREEN}Check complete!${NC}"
}

cmd_clean() {
    echo -e "${YELLOW}Cleaning build artifacts...${NC}"
    cd src-tauri
    cargo clean
    cd ..
    rm -rf node_modules/.cache 2>/dev/null || true
    echo -e "${GREEN}Clean complete!${NC}"
}

cmd_install() {
    echo -e "${GREEN}Installing dependencies...${NC}"
    npm install
    echo -e "${GREEN}Dependencies installed!${NC}"
}

cmd_serial() {
    echo -e "${GREEN}Available serial ports:${NC}"
    echo ""
    # List serial ports with details
    if command -v lsusb &> /dev/null; then
        echo -e "${BLUE}USB devices:${NC}"
        lsusb | grep -iE "serial|teensy|arduino|cp210|ch340|ftdi|16c0|2341|10c4|1a86" || echo "  No known serial devices found"
        echo ""
    fi

    echo -e "${BLUE}Serial ports:${NC}"
    ls -la /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || echo "  No serial ports found"
    echo ""

    if [ -d "/dev/serial/by-id" ]; then
        echo -e "${BLUE}By ID:${NC}"
        ls -la /dev/serial/by-id/ 2>/dev/null || echo "  No devices"
    fi
}

# Main command dispatch
case "${1:-help}" in
    dev)
        cmd_dev
        ;;
    build)
        cmd_build
        ;;
    run)
        cmd_run
        ;;
    test)
        cmd_test
        ;;
    check)
        cmd_check
        ;;
    clean)
        cmd_clean
        ;;
    install)
        cmd_install
        ;;
    serial)
        cmd_serial
        ;;
    help|--help|-h)
        print_usage
        ;;
    *)
        echo -e "${RED}Unknown command: $1${NC}"
        echo ""
        print_usage
        exit 1
        ;;
esac
