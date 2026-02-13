#!/bin/bash
# Test calibration mode commands via fc_tool headless
#
# Prerequisites:
#   - Teensy 4.0 flashed with teensy40_calibration firmware
#   - ModemManager disabled: sudo systemctl stop ModemManager
#   - fc_tool built: npm run tauri build (from fc_tool/)
#
# Usage:
#   ./test_calibration.sh                    # Run all safe tests on /dev/ttyACM0
#   ./test_calibration.sh /dev/ttyACM0 help  # Run just the help test
#   ./test_calibration.sh /dev/ttyACM0 all   # Run all safe tests
#
# See docs/findings/teensy-serial-troubleshooting.md for debugging tips

set -euo pipefail

PORT="${1:-/dev/ttyACM0}"
TEST="${2:-all}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RESULTS_DIR="$SCRIPT_DIR/results"
FC_TOOL="/home/devel/floppi/fc_tool/src-tauri/target/release/fc_tool"
TEENSY_REBOOT="$HOME/.platformio/packages/tool-teensy/teensy_reboot"
BOOT_WAIT=6
CMD_WAIT=3

mkdir -p "$RESULTS_DIR"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

PASS_COUNT=0
FAIL_COUNT=0

pass() { echo -e "  ${GREEN}PASS${NC}: $1"; PASS_COUNT=$((PASS_COUNT + 1)); }
fail() { echo -e "  ${RED}FAIL${NC}: $1"; FAIL_COUNT=$((FAIL_COUNT + 1)); }
info() { echo -e "${YELLOW}INFO${NC}: $1"; }
section() { echo -e "\n${CYAN}=== $1 ===${NC}"; }

# Release any stale port holders
release_port() {
    local holders
    holders=$(fuser "$PORT" 2>/dev/null || true)
    if [ -n "$holders" ]; then
        info "Releasing port $PORT (held by PIDs: $holders)"
        fuser -k "$PORT" 2>/dev/null || true
        sleep 1
    fi
}

# Reboot Teensy and wait for re-enumeration
reboot_teensy() {
    release_port
    if [ -x "$TEENSY_REBOOT" ]; then
        info "Rebooting Teensy..."
        "$TEENSY_REBOOT" 2>/dev/null || true
        sleep 4
        # Verify port came back
        if [ ! -e "$PORT" ]; then
            info "Waiting for port re-enumeration..."
            sleep 3
        fi
        if [ ! -e "$PORT" ]; then
            fail "Port $PORT not found after reboot"
            return 1
        fi
    else
        info "teensy_reboot not found, skipping board reset"
    fi
}

# Send commands to fc_tool headless via FIFO, capture output
# The board auto-starts CALIB_ATTITUDE when no radio is connected (CH6 > 1800)
# We send 'n' first to cancel it, then the actual commands
# Args: output_file cmd1 cmd2 ...
run_fc_tool() {
    local outfile="$1"
    shift
    local commands=("$@")

    reboot_teensy

    local fifo
    fifo=$(mktemp -u /tmp/fc_fifo.XXXXXX)
    mkfifo "$fifo"

    # Calculate total timeout: boot + cancel auto-cal + (commands * cmd_wait) + buffer
    local total=$(( BOOT_WAIT + 3 + ${#commands[@]} * CMD_WAIT + 3 ))

    # Start fc_tool reading from FIFO, capture all output (stdout + stderr)
    timeout "$total" "$FC_TOOL" --headless --port "$PORT" --baud 115200 < "$fifo" > "$outfile" 2>&1 &
    local fc_pid=$!

    # Keep write end open for the duration
    exec 3>"$fifo"

    # Wait for board to boot and auto-calibration prompt to appear
    sleep "$BOOT_WAIT"

    # Cancel auto-calibration (board may prompt "Type 'y' to continue, 'n' to cancel")
    echo "n" >&3
    sleep 2

    # Send each command
    for cmd in "${commands[@]}"; do
        echo "$cmd" >&3
        sleep "$CMD_WAIT"
    done

    # Close write end
    exec 3>&-

    # Wait for fc_tool to finish (disable errexit for background cleanup)
    sleep 1
    set +e
    kill $fc_pid 2>/dev/null
    wait $fc_pid 2>/dev/null
    set -e
    rm -f "$fifo"
}

# Check output contains expected strings (case-insensitive, supports regex alternation)
# Args: file description expected_pattern1 expected_pattern2 ...
check_output() {
    local file="$1"
    local desc="$2"
    shift 2
    local all_found=true

    if [ ! -s "$file" ]; then
        fail "$desc — no output captured (file empty)"
        return 1
    fi

    for pattern in "$@"; do
        if grep -qiE "$pattern" "$file"; then
            pass "$desc — found '$pattern'"
        else
            fail "$desc — missing '$pattern'"
            all_found=false
        fi
    done

    $all_found
}

# ============================================================
# Test functions
# ============================================================

test_boot() {
    section "Boot Sequence"
    info "Testing board boot output..."
    local outfile="$RESULTS_DIR/test_boot.txt"

    reboot_teensy

    local fifo
    fifo=$(mktemp -u /tmp/fc_fifo.XXXXXX)
    mkfifo "$fifo"

    timeout 12 "$FC_TOOL" --headless --port "$PORT" --baud 115200 < "$fifo" > "$outfile" 2>&1 &
    local fc_pid=$!
    exec 3>"$fifo"
    sleep 10
    exec 3>&-
    sleep 1
    set +e
    kill $fc_pid 2>/dev/null
    wait $fc_pid 2>/dev/null
    set -e
    rm -f "$fifo"

    echo "--- Boot Output ---"
    cat "$outfile"
    echo -e "\n--- End ---"

    check_output "$outfile" "Boot" \
        "dRehmFlight|VTOL|Flight Controller" \
        "OLED.*initialized|OLED.*OK" \
        "CALIBRATION|calibrat"
}

test_help() {
    section "Help Menu (h)"
    info "Testing help menu command..."
    local outfile="$RESULTS_DIR/test_help.txt"
    run_fc_tool "$outfile" "h"
    echo "--- Output ---"
    cat "$outfile"
    echo -e "\n--- End ---"
    check_output "$outfile" "Help menu" \
        "CALIBRATION|calibrat" \
        "IMU|imu" \
        "telemetry|Telemetry" \
        "PID|pid|gain"
}

test_status() {
    section "Calibration Status (c)"
    info "Testing calibration status..."
    local outfile="$RESULTS_DIR/test_status.txt"
    run_fc_tool "$outfile" "c"
    echo "--- Output ---"
    cat "$outfile"
    echo -e "\n--- End ---"
    check_output "$outfile" "Calibration status" \
        "calibrat|status|CALIBRAT"
}

test_pid() {
    section "PID Gains (g)"
    info "Testing PID gains display..."
    local outfile="$RESULTS_DIR/test_pid.txt"
    run_fc_tool "$outfile" "g"
    echo "--- Output ---"
    cat "$outfile"
    echo -e "\n--- End ---"
    check_output "$outfile" "PID gains" \
        "kp_roll" \
        "ki_roll" \
        "kd_roll"
}

test_params() {
    section "Filter Parameters (p)"
    info "Testing filter parameters..."
    local outfile="$RESULTS_DIR/test_params.txt"
    run_fc_tool "$outfile" "p"
    echo "--- Output ---"
    cat "$outfile"
    echo -e "\n--- End ---"
    check_output "$outfile" "Filter params" \
        "filter|FILTER|param|PARAM|cutoff|Hz|hz|limit|LIMIT"
}

test_dump() {
    section "Calibration Dump (d)"
    info "Testing calibration value dump..."
    local outfile="$RESULTS_DIR/test_dump.txt"
    run_fc_tool "$outfile" "d"
    echo "--- Output ---"
    cat "$outfile"
    echo -e "\n--- End ---"
    check_output "$outfile" "Calibration dump" \
        "offset|bias|calibrat|gyro|accel|error|Error"
}

test_channel_status() {
    section "Channel Status (s)"
    info "Testing channel/status display..."
    local outfile="$RESULTS_DIR/test_channel_status.txt"
    run_fc_tool "$outfile" "s"
    echo "--- Output ---"
    cat "$outfile"
    echo -e "\n--- End ---"
    check_output "$outfile" "Channel status" \
        "CH1|ch1|channel" \
        "Armed|armed"
}

test_imu_cal() {
    section "IMU Quick Calibration (i)"
    info "Board must be FLAT and STILL!"
    local outfile="$RESULTS_DIR/test_imu_cal.txt"

    reboot_teensy

    local fifo
    fifo=$(mktemp -u /tmp/fc_fifo.XXXXXX)
    mkfifo "$fifo"

    # IMU cal: boot + cancel + send i + confirm y + stability check + countdown + calibrate
    timeout 35 "$FC_TOOL" --headless --port "$PORT" --baud 115200 < "$fifo" > "$outfile" 2>&1 &
    local fc_pid=$!
    exec 3>"$fifo"

    sleep "$BOOT_WAIT"
    echo "n" >&3       # Cancel auto-cal prompt (if any)
    sleep 2
    echo "i" >&3       # Start IMU calibration
    sleep 2
    echo "y" >&3       # Confirm calibration
    sleep 18           # Wait for stability check + countdown + calibration (~15s)
    exec 3>&-

    sleep 1
    set +e
    kill $fc_pid 2>/dev/null
    wait $fc_pid 2>/dev/null
    set -e
    rm -f "$fifo"

    echo "--- IMU Cal Output ---"
    cat "$outfile"
    echo -e "\n--- End ---"

    check_output "$outfile" "IMU calibration" \
        "IMU CALIBRATION|calibrat" \
        "Gyro|gyro|GyroError|offset" \
        "Acc|accel|AccError"
}

test_telemetry() {
    section "Telemetry (t)"
    info "Testing telemetry toggle (on then off)..."
    local outfile="$RESULTS_DIR/test_telemetry.txt"

    reboot_teensy

    local fifo
    fifo=$(mktemp -u /tmp/fc_fifo.XXXXXX)
    mkfifo "$fifo"

    # Longer timeout for telemetry: boot + cancel + enable + stream + disable
    timeout 20 "$FC_TOOL" --headless --port "$PORT" --baud 115200 < "$fifo" > "$outfile" 2>&1 &
    local fc_pid=$!
    exec 3>"$fifo"

    sleep "$BOOT_WAIT"
    echo "n" >&3       # Cancel auto-cal
    sleep 2
    echo "t" >&3       # Enable telemetry
    sleep 5            # Let it stream for 5 seconds
    echo "t" >&3       # Disable telemetry
    sleep 2
    exec 3>&-

    sleep 1
    set +e
    kill $fc_pid 2>/dev/null
    wait $fc_pid 2>/dev/null
    set -e
    rm -f "$fifo"

    local line_count
    line_count=$(wc -l < "$outfile")
    echo "--- Telemetry Output ($line_count lines, first 30) ---"
    head -30 "$outfile"
    echo -e "\n--- End ---"

    check_output "$outfile" "Telemetry" \
        "@|:|telemetry|Telemetry"

    # Check for plotId format if telemetry data found
    if grep -qE "@[0-9]+:" "$outfile"; then
        pass "Telemetry — uses @plotId format"
    else
        info "Telemetry — @plotId format not detected (may use plain format)"
    fi
}

# ============================================================
# Main
# ============================================================

echo "========================================="
echo "  Calibration Mode Test Suite"
echo "  Port: $PORT"
echo "  Test: $TEST"
echo "  $(date)"
echo "========================================="

# Check prerequisites
if [ ! -x "$FC_TOOL" ]; then
    echo -e "${RED}ERROR${NC}: fc_tool not found at $FC_TOOL"
    echo "Build it: cd fc_tool && npm run tauri build"
    exit 1
fi

if [ ! -e "$PORT" ]; then
    echo -e "${RED}ERROR${NC}: Serial port $PORT not found"
    echo "Check: ls /dev/ttyACM* /dev/ttyUSB*"
    exit 1
fi

# Check ModemManager
if systemctl is-active ModemManager >/dev/null 2>&1; then
    echo -e "${RED}WARNING${NC}: ModemManager is active! This will interfere with Teensy serial."
    echo "Fix: sudo systemctl stop ModemManager"
    echo "See: docs/findings/teensy-serial-troubleshooting.md"
    exit 1
fi

release_port

case "$TEST" in
    boot)       test_boot ;;
    help)       test_help ;;
    status)     test_status ;;
    pid)        test_pid ;;
    params)     test_params ;;
    dump)       test_dump ;;
    channels)   test_channel_status ;;
    imu)        test_imu_cal ;;
    telemetry)  test_telemetry ;;
    all)
        test_boot
        test_help
        test_status
        test_pid
        test_params
        test_dump
        test_channel_status
        test_telemetry
        ;;
    *)
        echo "Unknown test: $TEST"
        echo "Available: boot help status pid params dump channels imu telemetry all"
        exit 1
        ;;
esac

echo
echo "========================================="
echo "  Results: ${GREEN}$PASS_COUNT passed${NC}, ${RED}$FAIL_COUNT failed${NC}"
echo "  Output:  $RESULTS_DIR/"
echo "========================================="

# Exit with failure if any tests failed
[ "$FAIL_COUNT" -eq 0 ]
