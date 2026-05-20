#!/bin/bash
# Shared test harness for flight_controller test suites.
#
# Source this file from a suite script:
#   SUITE_DIR="$(cd "$(dirname "$0")" && pwd)"
#   source "$SUITE_DIR/../lib/harness.sh"
#
# Required environment (set by the suite before sourcing OR after, with defaults):
#   PORT          — serial device path (default /dev/ttyACM0)
#   RESULTS_DIR   — directory to capture per-test output
#
# Provides:
#   Globals:   FC_DIR, SERIAL_MON, TEENSY_REBOOT, PASS_COUNT, FAIL_COUNT, colors
#   Logging:   test_pass, test_fail, info, section
#   Port mgmt: release_port, reboot_teensy, detect_board_type
#   Serial:    run_serial
#   Asserts:   check_output (assert_pattern_in_output), check_absent
#   Setup:     harness_check_prereqs, harness_print_header, harness_print_footer

# Guard against multiple sourcing
if [ -n "${_FC_HARNESS_SOURCED:-}" ]; then
    return 0
fi
_FC_HARNESS_SOURCED=1

# Resolve flight_controller root from this file's location: tests/lib/harness.sh
_HARNESS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FC_DIR="$(cd "$_HARNESS_DIR/../.." && pwd)"
SERIAL_MON="$FC_DIR/tools/serial_monitor.py"
TEENSY_REBOOT="${TEENSY_REBOOT:-$HOME/.platformio/packages/tool-teensy/teensy_reboot}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# Counters (suite is expected to read these at the end)
PASS_COUNT=0
FAIL_COUNT=0

test_pass() { echo -e "  ${GREEN}PASS${NC}: $1"; PASS_COUNT=$((PASS_COUNT + 1)); }
test_fail() { echo -e "  ${RED}FAIL${NC}: $1"; FAIL_COUNT=$((FAIL_COUNT + 1)); }
info()      { echo -e "${YELLOW}INFO${NC}: $1"; }
section()   { echo -e "\n${CYAN}=== $1 ===${NC}"; }

# Auto-detect board type for reset behavior.
# TODO: detect via lsusb / device path / config flag
#   Teensy:  use teensy_reboot (16C0:0483 etc.)
#   ESP32:   toggle RTS/DTR (CP210x / CH340 / native USB)
detect_board_type() {
    # TODO: implement real detection. For now, default to teensy to preserve
    # current behaviour of the existing test_calibration.sh harness.
    echo "teensy"
}

# Release any stale port holders.
release_port() {
    local holders
    holders=$(fuser "$PORT" 2>/dev/null || true)
    if [ -n "$holders" ]; then
        info "Releasing port $PORT (held by PIDs: $holders)"
        fuser -k "$PORT" 2>/dev/null || true
        sleep 1
    fi
}

# Reboot the board and wait for re-enumeration.
# Currently implements teensy_reboot path; ESP32 RTS/DTR toggle is a TODO.
reboot_teensy() {
    release_port
    local board
    board="$(detect_board_type)"
    case "$board" in
        teensy)
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
                # Drain boot output so first test gets clean responses.
                info "Waiting for firmware boot (IMU warmup)..."
                python3 "$SERIAL_MON" "$PORT" --no-dtr-reset --boot-wait 0 \
                    --send "" --wait 12 --output "$RESULTS_DIR/boot_drain.txt" 2>/dev/null
                if grep -q "CALIBRATION MODE\|FLIGHT CONTROLLER READY" "$RESULTS_DIR/boot_drain.txt" 2>/dev/null; then
                    info "Firmware booted OK"
                else
                    info "WARNING: boot drain did not see READY marker (may still work)"
                fi
            else
                info "teensy_reboot not found, skipping board reset"
            fi
            ;;
        esp32)
            # TODO: implement RTS/DTR toggle reset for ESP32.
            info "ESP32 reset not yet implemented — skipping board reset"
            ;;
        *)
            info "Unknown board type '$board' — skipping board reset"
            ;;
    esac
}

# Send commands via serial_monitor.py and capture output.
# Args: output_file wait_secs cmd1 [cmd2 ...]
run_serial() {
    local outfile="$1"
    local wait_secs="$2"
    shift 2
    local send_args=()
    local cmd
    for cmd in "$@"; do
        send_args+=(--send "$cmd")
    done

    # Brief pause to let the kernel release the port from any previous connection.
    sleep 0.3

    # serial_monitor.py may return non-zero (exit 2 = no output). Assertions
    # check output files separately, so tolerate non-zero exits.
    python3 "$SERIAL_MON" "$PORT" --no-dtr-reset --boot-wait 1 \
        "${send_args[@]}" --wait "$wait_secs" --output "$outfile" 2>/dev/null || true

    # If output file is empty, USB CDC may be in a bad state (Teensy quirk after
    # many rapid open/close cycles). Recovery: teensy_reboot, then retry once.
    if [ ! -s "$outfile" ]; then
        info "Empty output — attempting CDC recovery (teensy_reboot)..."
        if [ -x "$TEENSY_REBOOT" ]; then
            "$TEENSY_REBOOT" 2>/dev/null || true
            sleep 5
            python3 "$SERIAL_MON" "$PORT" --no-dtr-reset --boot-wait 0 \
                --send "" --wait 12 --output "$RESULTS_DIR/_cdc_recovery.txt" 2>/dev/null || true
        else
            sleep 2
        fi
        python3 "$SERIAL_MON" "$PORT" --no-dtr-reset --boot-wait 1 \
            "${send_args[@]}" --wait "$wait_secs" --output "$outfile" 2>/dev/null || true
    fi
}

# Assert that output file contains all expected patterns (case-insensitive ERE).
# Dumps captured output on failure for debugging.
# Args: file description expected_pattern1 [expected_pattern2 ...]
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
    local pattern
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

# Alias matching the recon report's naming convention.
assert_pattern_in_output() { check_output "$@"; }

# Assert that output does NOT contain a pattern.
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

# Verify serial_monitor.py + port + ModemManager prerequisites. Exits on failure.
harness_check_prereqs() {
    if [ ! -f "$SERIAL_MON" ]; then
        echo -e "${RED}ERROR${NC}: serial_monitor.py not found at $SERIAL_MON"
        exit 1
    fi
    if [ ! -e "$PORT" ]; then
        echo -e "${RED}ERROR${NC}: Serial port $PORT not found"
        echo "Check: ls /dev/ttyACM* /dev/ttyUSB*"
        exit 1
    fi
    if systemctl is-active ModemManager >/dev/null 2>&1; then
        echo -e "${RED}WARNING${NC}: ModemManager is active! This will interfere with Teensy serial."
        echo "Fix: sudo systemctl stop ModemManager"
        exit 1
    fi
}

# Print a standard suite header.
# Args: suite_name test_selector
harness_print_header() {
    local suite="$1"
    local test_sel="$2"
    echo "========================================="
    echo "  $suite"
    echo "  Port: $PORT"
    echo "  Test: $test_sel"
    echo "  Tool: serial_monitor.py (raw termios)"
    echo "  $(date)"
    echo "========================================="
}

# Print a standard suite footer.
harness_print_footer() {
    echo
    echo "========================================="
    echo -e "  Results: ${GREEN}$PASS_COUNT passed${NC}, ${RED}$FAIL_COUNT failed${NC}"
    echo "  Output:  $RESULTS_DIR/"
    echo "========================================="
}
