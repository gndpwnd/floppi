#!/bin/bash
# Test calibration mode commands via serial_monitor.py
#
# Prerequisites:
#   - Teensy 4.0 flashed with teensy40_calibration firmware
#   - ModemManager disabled: sudo systemctl stop ModemManager
#   - Python 3.x installed (no extra packages needed)
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
FC_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
RESULTS_DIR="$SCRIPT_DIR/results"
SERIAL_MON="$FC_DIR/tools/serial_monitor.py"
TEENSY_REBOOT="$HOME/.platformio/packages/tool-teensy/teensy_reboot"

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

# Send commands via serial_monitor.py and capture output
# Args: output_file wait_secs cmd1 cmd2 ...
run_serial() {
    local outfile="$1"
    local wait_secs="$2"
    shift 2
    local send_args=()
    for cmd in "$@"; do
        send_args+=(--send "$cmd")
    done

    python3 "$SERIAL_MON" "$PORT" --no-dtr-reset --boot-wait 1 \
        "${send_args[@]}" --wait "$wait_secs" --output "$outfile" 2>/dev/null
}

# Check output contains expected strings (case-insensitive, supports regex alternation)
# Args: file description expected_pattern1 expected_pattern2 ...
check_output() {
    local file="$1"
    local desc="$2"
    shift 2

    if [ ! -s "$file" ]; then
        fail "$desc — no output captured (file empty)"
        return 1
    fi

    local all_found=true
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

test_help() {
    section "Help Menu (h)"
    info "Testing help menu command..."
    local outfile="$RESULTS_DIR/test_help.txt"
    run_serial "$outfile" 3 "h"
    check_output "$outfile" "Help menu" \
        "CALIBRATION COMMANDS|calibrat" \
        "IMU|imu" \
        "telemetry|Telemetry" \
        "PID|pid|gain"
}

test_status() {
    section "Calibration Status (c)"
    info "Testing calibration status..."
    local outfile="$RESULTS_DIR/test_status.txt"
    run_serial "$outfile" 3 "c"
    check_output "$outfile" "Calibration status" \
        "CALIBRATION STATUS|calibrat" \
        "IMU|imu" \
        "Stage|stage"
}

test_channels() {
    section "Channel Status (s)"
    info "Testing channel/status display..."
    local outfile="$RESULTS_DIR/test_channels.txt"
    run_serial "$outfile" 3 "s"
    check_output "$outfile" "Channel status" \
        "CH1|ch1" \
        "Armed|armed"
}

test_pid() {
    section "PID Gains (g)"
    info "Testing PID gains display..."
    local outfile="$RESULTS_DIR/test_pid.txt"
    run_serial "$outfile" 3 "g"
    check_output "$outfile" "PID gains" \
        "kp_roll" \
        "ki_roll" \
        "kd_roll"
}

test_pid_set() {
    section "PID Set (g kp_roll 0.25)"
    info "Testing PID gain modification..."
    local outfile="$RESULTS_DIR/test_pid_set.txt"
    run_serial "$outfile" 3 "g kp_roll 0.25" "g"
    check_output "$outfile" "PID set" \
        "kp_roll=0.25"
}

test_params() {
    section "Filter Parameters (p)"
    info "Testing filter parameters..."
    local outfile="$RESULTS_DIR/test_params.txt"
    run_serial "$outfile" 3 "p"
    check_output "$outfile" "Filter params" \
        "b_accel|B_ACCEL" \
        "b_gyro|B_GYRO" \
        "max_roll|MAX_ROLL"
}

test_dump() {
    section "Calibration Dump (d)"
    info "Testing calibration value dump..."
    local outfile="$RESULTS_DIR/test_dump.txt"
    run_serial "$outfile" 3 "d"
    check_output "$outfile" "Calibration dump" \
        "#define.*KP_ROLL|KP_ROLL_ANGLE" \
        "#define.*B_ACCEL|B_ACCEL" \
        "CALIBRATION VALUES|config.h"
}

test_telemetry() {
    section "Telemetry (t)"
    info "Testing telemetry toggle (on for 3s, then off)..."
    local outfile="$RESULTS_DIR/test_telemetry.txt"
    run_serial "$outfile" 3 "t" "t"
    check_output "$outfile" "Telemetry" \
        "Telemetry|telemetry"

    # Check for @plotId format
    if grep -qE "@[0-9]+:" "$outfile"; then
        pass "Telemetry — uses @plotId:value format"
    else
        info "Telemetry — @plotId format not detected (may use plain format)"
    fi
}

test_imu_cal() {
    section "IMU Calibration (i)"
    info "Board must be FLAT and STILL!"
    info "This takes ~20s (stability check + countdown + calibration)"
    local outfile="$RESULTS_DIR/test_imu_cal.txt"

    # Flow: i → y (start) → y (continue despite level warning) → n (don't retry)
    # Timing: 15s between sends allows for stability check, calibration, quality check
    python3 "$SERIAL_MON" "$PORT" --no-dtr-reset --boot-wait 1 \
        --send i --send y --send y --send n --wait 15 \
        --output "$outfile" 2>/dev/null

    check_output "$outfile" "IMU calibration" \
        "IMU CALIBRATION|calibrat" \
        "Gyro|gyro|GyroError" \
        "Acc|accel|AccError"
}

test_orientation_start() {
    section "Orientation Detection (o) — start/cancel"
    info "Verifying orientation command starts correctly..."
    local outfile="$RESULTS_DIR/test_orientation_start.txt"

    # Send o, then cancel with n (full test needs physical board manipulation)
    run_serial "$outfile" 8 "o" "n"
    check_output "$outfile" "Orientation start" \
        "ORIENTATION|orientation" \
        "auto-detect|axis|mount"
}

test_sequential_start() {
    section "Sequential Calibration (a) — start/cancel"
    info "Verifying sequential workflow starts correctly..."
    local outfile="$RESULTS_DIR/test_sequential_start.txt"

    run_serial "$outfile" 8 "a" "n"
    check_output "$outfile" "Sequential start" \
        "SEQUENTIAL|sequential|workflow" \
        "Stage|stage"
}

# ============================================================
# Main
# ============================================================

echo "========================================="
echo "  Calibration Mode Test Suite"
echo "  Port: $PORT"
echo "  Test: $TEST"
echo "  Tool: serial_monitor.py (raw termios)"
echo "  $(date)"
echo "========================================="

# Check prerequisites
if [ ! -f "$SERIAL_MON" ]; then
    echo -e "${RED}ERROR${NC}: serial_monitor.py not found at $SERIAL_MON"
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
    exit 1
fi

release_port

case "$TEST" in
    help)        test_help ;;
    status)      test_status ;;
    channels)    test_channels ;;
    pid)         test_pid ;;
    pid_set)     test_pid_set ;;
    params)      test_params ;;
    dump)        test_dump ;;
    telemetry)   test_telemetry ;;
    imu)         test_imu_cal ;;
    orientation) test_orientation_start ;;
    sequential)  test_sequential_start ;;
    all)
        test_help
        test_status
        test_channels
        test_pid
        test_pid_set
        test_params
        test_dump
        test_telemetry
        test_imu_cal
        test_orientation_start
        test_sequential_start
        ;;
    *)
        echo "Unknown test: $TEST"
        echo "Available: help status channels pid pid_set params dump telemetry imu orientation sequential all"
        exit 1
        ;;
esac

echo
echo "========================================="
echo -e "  Results: ${GREEN}$PASS_COUNT passed${NC}, ${RED}$FAIL_COUNT failed${NC}"
echo "  Output:  $RESULTS_DIR/"
echo "========================================="

# Exit with failure if any tests failed
[ "$FAIL_COUNT" -eq 0 ]
