#!/bin/bash
# Test fc_tool serial plotter with simulated data via virtual serial port.
#
# Creates a socat pty pair, runs the simulator on one end, and fc_tool
# (headless mode) on the other. Captures output for verification.
#
# Prerequisites:
#   - socat installed: sudo apt-get install socat
#   - fc_tool built: cd fc_tool && npx tauri build (or use dev binary)
#   - Python 3.x
#
# Usage:
#   ./test_plotter.sh                    # Run all plotter tests
#   ./test_plotter.sh imu                # Run just the IMU scenario
#   ./test_plotter.sh --fc-tool-bin PATH # Specify fc_tool binary location
#
# Virtual serial ports:
#   socat creates /tmp/vserial0 ↔ /tmp/vserial1
#   fc_tool reads from /tmp/vserial0, simulator writes to /tmp/vserial1

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FC_TOOL_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
RESULTS_DIR="$SCRIPT_DIR/results"
SIMULATOR="$SCRIPT_DIR/simulate_serial.py"
VSERIAL_A="/tmp/vserial0"
VSERIAL_B="/tmp/vserial1"

# Default: look for debug build, then release build
FC_TOOL_BIN="${FC_TOOL_BIN:-}"
if [ -z "$FC_TOOL_BIN" ]; then
    if [ -f "$FC_TOOL_DIR/src-tauri/target/debug/fc_tool" ]; then
        FC_TOOL_BIN="$FC_TOOL_DIR/src-tauri/target/debug/fc_tool"
    elif [ -f "$FC_TOOL_DIR/src-tauri/target/release/fc_tool" ]; then
        FC_TOOL_BIN="$FC_TOOL_DIR/src-tauri/target/release/fc_tool"
    fi
fi

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

PASS_COUNT=0
FAIL_COUNT=0
SOCAT_PID=""

test_pass() { echo -e "  ${GREEN}PASS${NC}: $1"; PASS_COUNT=$((PASS_COUNT + 1)); }
test_fail() { echo -e "  ${RED}FAIL${NC}: $1"; FAIL_COUNT=$((FAIL_COUNT + 1)); }
info() { echo -e "${YELLOW}INFO${NC}: $1"; }
section() { echo -e "\n${CYAN}=== $1 ===${NC}"; }

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

# ============================================================================
# Virtual serial port management
# ============================================================================

start_socat() {
    # Kill any lingering socat on these ptys
    stop_socat 2>/dev/null || true

    socat -d -d \
        pty,raw,echo=0,link="$VSERIAL_A" \
        pty,raw,echo=0,link="$VSERIAL_B" &
    SOCAT_PID=$!
    sleep 1

    if ! kill -0 "$SOCAT_PID" 2>/dev/null; then
        test_fail "socat failed to start"
        return 1
    fi

    if [ ! -e "$VSERIAL_A" ] || [ ! -e "$VSERIAL_B" ]; then
        test_fail "Virtual serial ports not created"
        stop_socat
        return 1
    fi

    info "Virtual serial pair: $VSERIAL_A ↔ $VSERIAL_B (socat PID $SOCAT_PID)"
}

stop_socat() {
    if [ -n "$SOCAT_PID" ] && kill -0 "$SOCAT_PID" 2>/dev/null; then
        kill "$SOCAT_PID" 2>/dev/null || true
        wait "$SOCAT_PID" 2>/dev/null || true
    fi
    SOCAT_PID=""
    rm -f "$VSERIAL_A" "$VSERIAL_B"
}

cleanup() {
    stop_socat
    # Kill any stray simulator processes
    pkill -f "simulate_serial.py.*vserial" 2>/dev/null || true
}

trap cleanup EXIT

# ============================================================================
# Test: simulator output format verification (no fc_tool needed)
# ============================================================================

test_simulator_stdout() {
    local scenario="$1"
    local outfile="$RESULTS_DIR/sim_${scenario}.txt"

    section "Simulator: $scenario (stdout)"
    info "Running simulator for 2s..."

    python3 "$SIMULATOR" --stdout --scenario "$scenario" --rate 20 --duration 2 \
        > "$outfile" 2>/dev/null

    local lines
    lines=$(wc -l < "$outfile")
    if [ "$lines" -gt 0 ]; then
        test_pass "Produced $lines lines of output"
    else
        test_fail "No output produced"
        return 1
    fi
}

# ============================================================================
# Test: protocol format verification
# ============================================================================

test_protocol_formats() {
    local outfile="$RESULTS_DIR/sim_protocol.txt"

    section "Protocol Format Verification"
    info "Running protocol scenario for 3s..."

    python3 "$SIMULATOR" --stdout --scenario protocol --rate 20 --duration 3 \
        > "$outfile" 2>/dev/null

    check_output "$outfile" "Protocol formats" \
        "named_plot@0:" \
        "colon_fmt:" \
        "equals_fmt="

    # Plain number (just a float on its own line) — check separately (case-sensitive exact match)
    if grep -qE "^-?[0-9]+\.[0-9]+$" "$outfile"; then
        test_pass "Protocol formats — plain number format present"
    else
        test_fail "Protocol formats — plain number format missing"
    fi
}

# ============================================================================
# Test: ANSI format verification
# ============================================================================

test_ansi_output() {
    local outfile="$RESULTS_DIR/sim_ansi.txt"

    section "ANSI Escape Code Output"
    info "Running ANSI scenario for 2s..."

    python3 "$SIMULATOR" --stdout --scenario ansi --rate 10 --duration 2 \
        > "$outfile" 2>/dev/null

    # Check ANSI escape sequences present
    if grep -qP '\x1b\[' "$outfile"; then
        test_pass "ANSI escape sequences present"
    else
        test_fail "No ANSI escape sequences found"
    fi

    # Check interleaved plotter data
    if grep -qE "@0:" "$outfile"; then
        test_pass "Interleaved plotter data present"
    else
        test_fail "No interleaved plotter data"
    fi

    # Verify no corrupted escape sequences (incomplete or malformed)
    check_absent "$outfile" "ANSI output" "\\\\033"
}

# ============================================================================
# Test: stress scenario (tests max plots warning)
# ============================================================================

test_stress_format() {
    local outfile="$RESULTS_DIR/sim_stress.txt"

    section "Stress Scenario Format"
    info "Running stress scenario for 2s..."

    python3 "$SIMULATOR" --stdout --scenario stress --rate 20 --duration 2 \
        > "$outfile" 2>/dev/null

    # Verify multiple plot IDs present
    local plots
    plots=$(grep -oE "@[0-9]+:" "$outfile" | sort -u | wc -l)
    if [ "$plots" -ge 10 ]; then
        test_pass "Multiple plot IDs present ($plots unique IDs)"
    else
        test_fail "Expected 10+ plot IDs, found $plots"
    fi

    # Verify data lines have expected structure
    check_output "$outfile" "Stress data" "ch0@0:.*ch1@1:"
}

# ============================================================================
# Test: fc_tool headless mode with virtual serial
# ============================================================================

test_headless_with_socat() {
    local scenario="$1"
    local outfile="$RESULTS_DIR/headless_${scenario}.txt"

    section "Headless + socat: $scenario"

    if [ -z "$FC_TOOL_BIN" ] || [ ! -x "$FC_TOOL_BIN" ]; then
        info "SKIPPED — fc_tool binary not found (build with: npx tauri build)"
        return 0
    fi

    start_socat || return 1
    sleep 0.5

    # Start fc_tool in headless mode reading from vserial A
    "$FC_TOOL_BIN" --headless --port "$VSERIAL_A" --baud 115200 \
        > "$outfile" 2>/dev/null &
    local fc_pid=$!
    sleep 1

    # Start simulator writing to vserial B
    info "Simulator writing $scenario data for 3s..."
    python3 "$SIMULATOR" "$VSERIAL_B" --scenario "$scenario" --rate 30 --duration 3 \
        2>/dev/null

    # Wait for fc_tool to receive data
    sleep 1
    kill "$fc_pid" 2>/dev/null || true
    wait "$fc_pid" 2>/dev/null || true
    stop_socat

    local lines
    lines=$(wc -l < "$outfile" 2>/dev/null || echo 0)
    if [ "$lines" -gt 0 ]; then
        test_pass "fc_tool received $lines lines via virtual serial"
    else
        test_fail "fc_tool received no data"
    fi
}

# ============================================================================
# Parse args
# ============================================================================

TEST="${1:-all}"

# Handle --fc-tool-bin flag
for i in "$@"; do
    if [ "$i" = "--fc-tool-bin" ]; then
        shift
        FC_TOOL_BIN="$1"
        shift
        TEST="${1:-all}"
        break
    fi
done

mkdir -p "$RESULTS_DIR"

echo "========================================="
echo "  fc_tool Plotter Test Suite"
echo "  Test: $TEST"
echo "  Simulator: simulate_serial.py"
echo "  $(date)"
echo "========================================="

# Check prerequisites
if [ ! -f "$SIMULATOR" ]; then
    echo -e "${RED}ERROR${NC}: simulate_serial.py not found at $SIMULATOR"
    exit 1
fi

if ! command -v python3 &>/dev/null; then
    echo -e "${RED}ERROR${NC}: python3 not found"
    exit 1
fi

HAS_SOCAT=true
if ! command -v socat &>/dev/null; then
    echo -e "${YELLOW}INFO${NC}: socat not installed. Virtual serial tests will be skipped."
    echo "Install: sudo apt-get install socat"
    HAS_SOCAT=false
fi

if [ -n "$FC_TOOL_BIN" ] && [ -x "$FC_TOOL_BIN" ]; then
    info "fc_tool binary: $FC_TOOL_BIN"
else
    info "fc_tool binary not found — headless tests will be skipped"
fi

# ============================================================================
# Run tests
# ============================================================================

case "$TEST" in
    sim_imu)       test_simulator_stdout imu ;;
    sim_sine)      test_simulator_stdout sine ;;
    sim_mixed)     test_simulator_stdout mixed ;;
    sim_ansi)      test_ansi_output ;;
    sim_stress)    test_stress_format ;;
    sim_dashboard) test_simulator_stdout dashboard ;;
    sim_noise)     test_simulator_stdout noise ;;
    protocol)      test_protocol_formats ;;
    headless_imu)
        if $HAS_SOCAT; then test_headless_with_socat imu; else info "SKIPPED (no socat)"; fi
        ;;
    headless_sine)
        if $HAS_SOCAT; then test_headless_with_socat sine; else info "SKIPPED (no socat)"; fi
        ;;
    all)
        # Simulator output tests (no hardware, no fc_tool build needed)
        test_simulator_stdout imu
        test_simulator_stdout sine
        test_simulator_stdout mixed
        test_simulator_stdout dashboard
        test_simulator_stdout noise
        test_ansi_output
        test_stress_format
        test_protocol_formats

        # Headless mode tests (need socat + fc_tool build)
        if $HAS_SOCAT; then
            test_headless_with_socat imu
            test_headless_with_socat sine
            test_headless_with_socat mixed
        fi
        ;;
    *)
        echo "Unknown test: $TEST"
        echo "Available: sim_imu sim_sine sim_mixed sim_ansi sim_stress sim_dashboard sim_noise protocol headless_imu headless_sine all"
        exit 1
        ;;
esac

echo
echo "========================================="
echo -e "  Results: ${GREEN}$PASS_COUNT passed${NC}, ${RED}$FAIL_COUNT failed${NC}"
echo "  Output:  $RESULTS_DIR/"
echo "========================================="

[ "$FAIL_COUNT" -eq 0 ]
