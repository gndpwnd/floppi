#!/bin/bash
#=============================================================================
# Flash firmware and launch fc_tool
#=============================================================================
# Usage:
#   ./tools/flash_and_run.sh                  # flash teensy40_calibration + fc_tool
#   ./tools/flash_and_run.sh teensy40          # flash teensy40 (live) + fc_tool
#   ./tools/flash_and_run.sh esp32_calibration # flash esp32_calibration + fc_tool
#
# Workflow:
#   1. Builds and flashes the specified PlatformIO environment
#   2. Waits for the board to reconnect on serial
#   3. Launches fc_tool in dev mode
#   4. When fc_tool is closed (Ctrl+C or window close), script exits
#   5. You can then re-run to flash updated firmware
#
# Requires: pio, npm/npx (for fc_tool), system deps for Tauri
#=============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FC_DIR="$(dirname "$SCRIPT_DIR")"
FC_TOOL_DIR="$(dirname "$FC_DIR")/fc_tool"

ENV="${1:-teensy40_calibration}"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}=== Flash & Run ===${NC}"
echo -e "Environment: ${GREEN}${ENV}${NC}"
echo ""

#-----------------------------------------------------------------------------
# Step 1: Build and flash
#-----------------------------------------------------------------------------
echo -e "${YELLOW}[1/3] Building and flashing ${ENV}...${NC}"
cd "$FC_DIR"

if ! pio run -e "$ENV" -t upload; then
    echo -e "${RED}Flash failed!${NC}"
    echo "If the board didn't enter program mode, press the PROGRAM button and re-run."
    exit 1
fi

echo -e "${GREEN}Flash complete.${NC}"

#-----------------------------------------------------------------------------
# Step 2: Wait for serial port
#-----------------------------------------------------------------------------
echo -e "${YELLOW}[2/3] Waiting for board to reconnect...${NC}"

SERIAL_PORT=""
for i in $(seq 1 20); do
    SERIAL_PORT=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -1)
    if [ -n "$SERIAL_PORT" ]; then
        echo -e "${GREEN}Found: ${SERIAL_PORT}${NC}"
        break
    fi
    sleep 0.5
done

if [ -z "$SERIAL_PORT" ]; then
    echo -e "${RED}No serial port found after 10 seconds.${NC}"
    echo "Board may not have reconnected. Check USB connection."
    exit 1
fi

#-----------------------------------------------------------------------------
# Step 3: Launch fc_tool
#-----------------------------------------------------------------------------
echo -e "${YELLOW}[3/3] Launching fc_tool...${NC}"
echo -e "Close fc_tool (Ctrl+C or window close) to return here for re-flashing."
echo ""

cd "$FC_TOOL_DIR"
npx tauri dev 2>&1 || true

echo ""
echo -e "${CYAN}fc_tool closed. Re-run this script to flash and test again.${NC}"
