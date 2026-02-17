#!/bin/bash
# Test fc_tool serial monitor features with simulated data.
#
# Focuses on testing the terminal display: ANSI rendering, mixed data types,
# filter behavior (via manual verification), and headless mode I/O.
#
# Prerequisites:
#   - socat installed: sudo apt-get install socat
#   - fc_tool built (for headless tests, otherwise skipped)
#   - Python 3.x
#
# Usage:
#   ./test_monitor.sh                # Run all monitor tests
#   ./test_monitor.sh ansi           # Run just the ANSI test
#   ./test_monitor.sh headless_echo  # Test headless stdin→serial→stdout round-trip

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FC_TOOL_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
RESULTS_DIR="$SCRIPT_DIR/results"
SIMULATOR="$SCRIPT_DIR/simulate_serial.py"
VSERIAL_A="/tmp/vserial0"
VSERIAL_B="/tmp/vserial1"

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

pass() { echo -e "  ${GREEN}PASS${NC}: $1"; PASS_COUNT=$((PASS_COUNT + 1)); }
fail() { echo -e "  ${RED}FAIL${NC}: $1"; FAIL_COUNT=$((FAIL_COUNT + 1)); }
info() { echo -e "${YELLOW}INFO${NC}: $1"; }
section() { echo -e "\n${CYAN}=== $1 ===${NC}"; }

start_socat() {
    stop_socat 2>/dev/null || true
    socat -d -d \
        pty,raw,echo=0,link="$VSERIAL_A" \
        pty,raw,echo=0,link="$VSERIAL_B" &
    SOCAT_PID=$!
    sleep 1
    if ! kill -0 "$SOCAT_PID" 2>/dev/null; then
        fail "socat failed to start"
        return 1
    fi
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
    pkill -f "simulate_serial.py.*vserial" 2>/dev/null || true
}

trap cleanup EXIT

# ============================================================================
# Test: ANSI escape code generation
# ============================================================================

test_ansi_generation() {
    section "ANSI Escape Code Generation"
    local outfile="$RESULTS_DIR/monitor_ansi.txt"

    python3 "$SIMULATOR" --stdout --scenario ansi --rate 5 --duration 2 \
        > "$outfile" 2>/dev/null

    # Verify various ANSI SGR codes
    if grep -qP '\x1b\[1;32m' "$outfile"; then
        pass "Bold green (\\033[1;32m) present"
    else
        fail "Bold green not found"
    fi

    if grep -qP '\x1b\[1;31m' "$outfile"; then
        pass "Bold red (\\033[1;31m) present"
    else
        fail "Bold red not found"
    fi

    if grep -qP '\x1b\[0m' "$outfile"; then
        pass "Reset code (\\033[0m) present"
    else
        fail "Reset code not found"
    fi

    if grep -qP '\x1b\[2m' "$outfile"; then
        pass "Dim (\\033[2m) present"
    else
        fail "Dim not found"
    fi

    if grep -qP '\x1b\[4m' "$outfile"; then
        pass "Underline (\\033[4m) present"
    else
        fail "Underline not found"
    fi

    # Check that non-ANSI lines also exist
    if grep -q "Plain text line" "$outfile"; then
        pass "Plain text (no ANSI) lines present"
    else
        fail "No plain text lines found"
    fi
}

# ============================================================================
# Test: dashboard key=value format
# ============================================================================

test_dashboard_format() {
    section "Dashboard Key=Value Format"
    local outfile="$RESULTS_DIR/monitor_dashboard.txt"

    python3 "$SIMULATOR" --stdout --scenario dashboard --rate 10 --duration 2 \
        > "$outfile" 2>/dev/null

    if grep -qE "battery=[0-9]+\.[0-9]+" "$outfile"; then
        pass "battery=X.XX format present"
    else
        fail "battery key=value not found"
    fi

    if grep -qE "loop=[0-9]+" "$outfile"; then
        pass "loop=XXXus format present"
    else
        fail "loop key=value not found"
    fi

    if grep -qE "armed=(YES|NO)" "$outfile"; then
        pass "armed=YES/NO format present"
    else
        fail "armed key=value not found"
    fi
}

# ============================================================================
# Test: headless stdin→serial echo (round-trip through virtual serial)
# ============================================================================

test_headless_echo() {
    section "Headless Echo (stdin → serial → stdout)"

    if [ -z "$FC_TOOL_BIN" ] || [ ! -x "$FC_TOOL_BIN" ]; then
        info "SKIPPED — fc_tool binary not found"
        return 0
    fi

    if ! command -v socat &>/dev/null; then
        info "SKIPPED — socat not installed"
        return 0
    fi

    local outfile="$RESULTS_DIR/monitor_echo.txt"
    start_socat || return 1
    sleep 0.5

    # Start fc_tool headless on vserial A
    "$FC_TOOL_BIN" --headless --port "$VSERIAL_A" --baud 115200 \
        > "$outfile" 2>/dev/null &
    local fc_pid=$!
    sleep 1

    # Write known data to vserial B
    local test_msg="ECHO_TEST_$(date +%s)"
    echo "$test_msg" > "$VSERIAL_B"
    sleep 1

    kill "$fc_pid" 2>/dev/null || true
    wait "$fc_pid" 2>/dev/null || true
    stop_socat

    if grep -q "$test_msg" "$outfile"; then
        pass "Echo round-trip: sent and received '$test_msg'"
    else
        fail "Echo round-trip failed — message not received"
    fi
}

# ============================================================================
# Test: headless with ANSI data
# ============================================================================

test_headless_ansi() {
    section "Headless + ANSI Data"

    if [ -z "$FC_TOOL_BIN" ] || [ ! -x "$FC_TOOL_BIN" ]; then
        info "SKIPPED — fc_tool binary not found"
        return 0
    fi

    if ! command -v socat &>/dev/null; then
        info "SKIPPED — socat not installed"
        return 0
    fi

    local outfile="$RESULTS_DIR/monitor_headless_ansi.txt"
    start_socat || return 1
    sleep 0.5

    "$FC_TOOL_BIN" --headless --port "$VSERIAL_A" --baud 115200 \
        > "$outfile" 2>/dev/null &
    local fc_pid=$!
    sleep 1

    python3 "$SIMULATOR" "$VSERIAL_B" --scenario ansi --rate 10 --duration 3 \
        2>/dev/null

    sleep 1
    kill "$fc_pid" 2>/dev/null || true
    wait "$fc_pid" 2>/dev/null || true
    stop_socat

    local lines
    lines=$(wc -l < "$outfile" 2>/dev/null || echo 0)
    if [ "$lines" -gt 0 ]; then
        pass "Received $lines lines of ANSI data through headless mode"
    else
        fail "No ANSI data received"
    fi

    # Verify ANSI codes pass through (headless is raw)
    if grep -qP '\x1b\[' "$outfile"; then
        pass "ANSI codes preserved in headless output"
    else
        fail "ANSI codes stripped (should be raw passthrough)"
    fi
}

# ============================================================================
# Parse args and run
# ============================================================================

TEST="${1:-all}"
mkdir -p "$RESULTS_DIR"

echo "========================================="
echo "  fc_tool Monitor Test Suite"
echo "  Test: $TEST"
echo "  $(date)"
echo "========================================="

if [ ! -f "$SIMULATOR" ]; then
    echo -e "${RED}ERROR${NC}: simulate_serial.py not found at $SIMULATOR"
    exit 1
fi

if ! command -v python3 &>/dev/null; then
    echo -e "${RED}ERROR${NC}: python3 not found"
    exit 1
fi

case "$TEST" in
    ansi)           test_ansi_generation ;;
    dashboard)      test_dashboard_format ;;
    headless_echo)  test_headless_echo ;;
    headless_ansi)  test_headless_ansi ;;
    all)
        # Simulator-only tests (no build needed)
        test_ansi_generation
        test_dashboard_format

        # Headless mode tests (need socat + build)
        test_headless_echo
        test_headless_ansi
        ;;
    *)
        echo "Unknown test: $TEST"
        echo "Available: ansi dashboard headless_echo headless_ansi all"
        exit 1
        ;;
esac

echo
echo "========================================="
echo -e "  Results: ${GREEN}$PASS_COUNT passed${NC}, ${RED}$FAIL_COUNT failed${NC}"
echo "  Output:  $RESULTS_DIR/"
echo "========================================="

[ "$FAIL_COUNT" -eq 0 ]
