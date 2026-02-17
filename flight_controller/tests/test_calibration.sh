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

test_pass() { echo -e "  ${GREEN}PASS${NC}: $1"; PASS_COUNT=$((PASS_COUNT + 1)); }
test_fail() { echo -e "  ${RED}FAIL${NC}: $1"; FAIL_COUNT=$((FAIL_COUNT + 1)); }
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
            test_fail "Port $PORT not found after reboot"
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

# Check output contains expected patterns (case-insensitive, regex alternation)
# Dumps captured output on failure for debugging.
# Args: file description expected_pattern1 expected_pattern2 ...
check_output() {
    local file="$1"
    local desc="$2"
    shift 2

    if [ ! -s "$file" ]; then
        test_fail "$desc — no output captured (file empty)"
        return 1
    fi

    local line_count
    line_count=$(wc -l < "$file")
    if [ "$line_count" -lt 3 ]; then
        info "$desc — WARNING: only $line_count lines captured (possible timeout)"
    fi

    local all_found=true
    for pattern in "$@"; do
        if grep -qiE "$pattern" "$file"; then
            test_pass "$desc — found '$pattern'"
        else
            test_fail "$desc — missing '$pattern'"
            all_found=false
        fi
    done

    if ! $all_found; then
        echo -e "  ${YELLOW}--- Captured output (first 15 lines) ---${NC}"
        head -15 "$file" | sed 's/^/    /'
        echo -e "  ${YELLOW}--- (end) ---${NC}"
    fi

    $all_found
}

# Check output does NOT contain a pattern (negative assertion)
# Args: file description pattern
check_absent() {
    local file="$1"
    local desc="$2"
    local pattern="$3"

    if grep -qiE "$pattern" "$file"; then
        test_fail "$desc — unexpected '$pattern' found"
        return 1
    else
        test_pass "$desc — correctly absent '$pattern'"
        return 0
    fi
}

# ============================================================
# Test functions
# ============================================================

test_help() {
    section "Help Menu (h)"
    info "Testing help menu command..."
    local outfile="$RESULTS_DIR/test_help.txt"
    run_serial "$outfile" 1 "h"
    check_output "$outfile" "Help menu" \
        "CALIBRATION COMMANDS" \
        "i - IMU calibration" \
        "t - Toggle telemetry" \
        "g - Show/set PID gains"
}

test_status() {
    section "Calibration Status (c)"
    info "Testing calibration status..."
    local outfile="$RESULTS_DIR/test_status.txt"
    run_serial "$outfile" 1 "c"
    check_output "$outfile" "Calibration status" \
        "CALIBRATION STATUS" \
        "\\[.\\].*IMU" \
        "Stage [0-9]|--- Stage"
}

test_channels() {
    section "Channel Status (s)"
    info "Testing channel/status display..."
    local outfile="$RESULTS_DIR/test_channels.txt"
    run_serial "$outfile" 1 "s"
    check_output "$outfile" "Channel status" \
        "CH1:.*CH2:" \
        "Armed:.*YES|Armed:.*NO"
}

test_pid() {
    section "PID Gains (g)"
    info "Testing PID gains display..."
    local outfile="$RESULTS_DIR/test_pid.txt"
    run_serial "$outfile" 1 "g"
    check_output "$outfile" "PID gains" \
        "kp_roll=.*[0-9]" \
        "ki_roll=.*[0-9]" \
        "kp_pitch=.*[0-9]" \
        "kp_yaw=.*[0-9]"
}

test_pid_set() {
    section "PID Set (g kp_roll 0.25)"
    info "Testing PID gain modification..."
    local outfile="$RESULTS_DIR/test_pid_set.txt"
    run_serial "$outfile" 1 "g kp_roll 0.25" "g"
    check_output "$outfile" "PID set" \
        "kp_roll.*=.*0\\.25"
    check_absent "$outfile" "PID set" "Unknown gain"
}

test_params() {
    section "Filter Parameters (p)"
    info "Testing filter parameters..."
    local outfile="$RESULTS_DIR/test_params.txt"
    run_serial "$outfile" 1 "p"
    check_output "$outfile" "Filter params" \
        "b_accel=.*[0-9]" \
        "b_gyro=.*[0-9]" \
        "max_roll=.*[0-9]"
}

test_param_set() {
    section "Param Set (p b_accel 0.12)"
    info "Testing filter parameter modification..."
    local outfile="$RESULTS_DIR/test_param_set.txt"
    run_serial "$outfile" 1 "p b_accel 0.12" "p"
    check_output "$outfile" "Param set" \
        "b_accel.*=.*0\\.12"
    check_absent "$outfile" "Param set" "Unknown param"
}

test_dump() {
    section "Calibration Dump (d)"
    info "Testing calibration value dump..."
    local outfile="$RESULTS_DIR/test_dump.txt"
    run_serial "$outfile" 2 "d"
    check_output "$outfile" "Calibration dump" \
        "#define KP_.*ROLL" \
        "#define B_ACCEL" \
        "CALIBRATION VALUES"
}

test_telemetry() {
    section "Telemetry (t)"
    info "Testing telemetry toggle (on for 3s, then off)..."
    local outfile="$RESULTS_DIR/test_telemetry.txt"
    run_serial "$outfile" 3 "t" "t"
    check_output "$outfile" "Telemetry" \
        "Telemetry:.*IMU|Telemetry.*50Hz"

    # Check for @plotId format
    if grep -qE "@[0-9]+:" "$outfile"; then
        test_pass "Telemetry — uses @plotId:value format"
    else
        info "Telemetry — @plotId format not detected (may use plain format)"
    fi
}

test_imu_cal() {
    section "IMU Calibration (i)"
    info "Board must be FLAT and STILL!"
    info "This takes ~60s (stability check + countdown + calibration)"
    local outfile="$RESULTS_DIR/test_imu_cal.txt"

    # Flow: i → y (start) → y (continue despite level warning) → n (don't retry)
    # Timing: 15s between sends allows for stability check, calibration, quality check
    python3 "$SERIAL_MON" "$PORT" --no-dtr-reset --boot-wait 1 \
        --send i --send y --send y --send n --wait 15 \
        --output "$outfile" 2>/dev/null

    check_output "$outfile" "IMU calibration" \
        "IMU CALIBRATION" \
        "Gyro.*Error|GyroError|GYRO_ERROR" \
        "Acc.*Error|AccError|ACC_ERROR"
}

test_6pos_imu_start() {
    section "6-Position IMU Calibration (m) — start/cancel"
    info "Verifying 6-position IMU command starts correctly..."
    local outfile="$RESULTS_DIR/test_6pos_imu.txt"

    run_serial "$outfile" 10 "m" "n"
    check_output "$outfile" "6-position IMU start" \
        "6.Position|6-position|CALIB" \
        "IMU|imu|calibrat"
}

test_orientation_start() {
    section "Orientation Detection (o) — start/cancel"
    info "Verifying orientation command starts correctly..."
    local outfile="$RESULTS_DIR/test_orientation_start.txt"

    run_serial "$outfile" 10 "o" "n"
    check_output "$outfile" "Orientation start" \
        "ORIENTATION|orientation" \
        "auto-detect|axis|mount"
}

test_radio_start() {
    section "Radio Calibration (r) — start/cancel"
    info "Verifying radio calibration command starts correctly..."
    local outfile="$RESULTS_DIR/test_radio.txt"

    run_serial "$outfile" 10 "r" "n"
    check_output "$outfile" "Radio start" \
        "Radio|radio|RADIO" \
        "channel|Channel|CHANNEL"
}

test_failsafe_start() {
    section "Failsafe Detection (f) — start/cancel"
    info "Verifying failsafe detection command starts correctly..."
    local outfile="$RESULTS_DIR/test_failsafe.txt"

    run_serial "$outfile" 10 "f" "n"
    check_output "$outfile" "Failsafe start" \
        "Failsafe|failsafe|FAILSAFE" \
        "transmitter|receiver|signal"
}

test_esc_start() {
    section "ESC Calibration (e) — start/cancel"
    info "Verifying ESC calibration command starts correctly..."
    local outfile="$RESULTS_DIR/test_esc.txt"

    run_serial "$outfile" 10 "e" "n"
    check_output "$outfile" "ESC start" \
        "ESC|esc" \
        "motor|Motor|PWM|endpoint|propeller|PROPELLER"
}

test_network_diag() {
    section "Network Diagnostics (n)"
    info "Testing network diagnostics command..."
    local outfile="$RESULTS_DIR/test_network.txt"

    run_serial "$outfile" 3 "n"
    # Teensy prints "only available on ESP32"; ESP32 prints diagnostics
    check_output "$outfile" "Network diagnostics" \
        "Network|network|ESP32|WiFi|diagnostics"
}

test_sequential_start() {
    section "Sequential Calibration (a) — start/cancel"
    info "Verifying sequential workflow starts correctly..."
    local outfile="$RESULTS_DIR/test_sequential_start.txt"

    run_serial "$outfile" 10 "a" "n"
    check_output "$outfile" "Sequential start" \
        "SEQUENTIAL|sequential|workflow" \
        "Stage|stage"
}

test_unknown_command() {
    section "Unknown Command — firmware recovery"
    info "Verifying firmware handles unknown input gracefully..."
    local outfile="$RESULTS_DIR/test_unknown.txt"

    # Send garbage, then a known command to verify firmware is still alive
    run_serial "$outfile" 1 "z" "h"
    check_output "$outfile" "Unknown command recovery" \
        "CALIBRATION COMMANDS"
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
    param_set)   test_param_set ;;
    params)      test_params ;;
    dump)        test_dump ;;
    telemetry)   test_telemetry ;;
    imu)         test_imu_cal ;;
    imu6)        test_6pos_imu_start ;;
    orientation) test_orientation_start ;;
    radio)       test_radio_start ;;
    failsafe)    test_failsafe_start ;;
    esc)         test_esc_start ;;
    network)     test_network_diag ;;
    sequential)  test_sequential_start ;;
    unknown)     test_unknown_command ;;
    all)
        reboot_teensy
        test_help
        test_status
        test_channels
        test_pid
        test_pid_set
        test_param_set
        test_params
        test_dump
        test_telemetry
        test_imu_cal
        test_6pos_imu_start
        test_orientation_start
        test_radio_start
        test_failsafe_start
        test_esc_start
        test_network_diag
        test_sequential_start
        test_unknown_command
        ;;
    *)
        echo "Unknown test: $TEST"
        echo "Available: help status channels pid pid_set param_set params dump telemetry"
        echo "           imu imu6 orientation radio failsafe esc network sequential unknown all"
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
