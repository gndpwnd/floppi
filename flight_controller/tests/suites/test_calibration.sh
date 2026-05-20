#!/bin/bash
# Calibration mode test suite (18 tests, 42 assertions).
#
# Migrated from tests/test_calibration.sh — the test functions and assertions
# are preserved verbatim. Shared infrastructure (port mgmt, serial wrapper,
# assertion helpers) lives in tests/lib/harness.sh.
#
# Prerequisites:
#   - Teensy 4.0 flashed with teensy40_calibration firmware
#     (or ESP32 with esp32_calibration — board reset path is a TODO)
#   - ModemManager disabled: sudo systemctl stop ModemManager
#   - Python 3.x installed (no extra packages needed)
#
# Usage:
#   ./tests/suites/test_calibration.sh                    # all tests on /dev/ttyACM0
#   ./tests/suites/test_calibration.sh /dev/ttyACM0 help  # single test
#   ./tests/suites/test_calibration.sh /dev/ttyACM0 all   # all tests (explicit)

set -euo pipefail

PORT="${1:-/dev/ttyACM0}"
TEST="${2:-all}"
SUITE_DIR="$(cd "$(dirname "$0")" && pwd)"
RESULTS_DIR="$SUITE_DIR/../results"

mkdir -p "$RESULTS_DIR"

# shellcheck source=../lib/harness.sh
source "$SUITE_DIR/../lib/harness.sh"

# ============================================================
# Test functions (preserved verbatim from test_calibration.sh)
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

    # Sequential workflow asks two questions (6-pos? then single-pos?), send 'n' to both
    run_serial "$outfile" 10 "a" "n" "n"
    check_output "$outfile" "Sequential start" \
        "SEQUENTIAL|sequential|workflow" \
        "Stage|stage"
}

test_unknown_command() {
    section "Unknown Command — firmware recovery"
    info "Verifying firmware handles unknown input gracefully..."
    local outfile="$RESULTS_DIR/test_unknown.txt"

    # Send garbage, then a known command to verify firmware is still alive
    run_serial "$outfile" 2 "z" "h"
    check_output "$outfile" "Unknown command recovery" \
        "CALIBRATION COMMANDS"
}

# ============================================================
# Main
# ============================================================

harness_print_header "Calibration Mode Test Suite" "$TEST"
harness_check_prereqs
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

harness_print_footer

# Exit with failure if any tests failed
[ "$FAIL_COUNT" -eq 0 ]
