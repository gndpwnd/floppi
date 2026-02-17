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

start_socat() {
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

    # Verify various ANSI SGR codes using check_output
    # (grep -P for ANSI isn't compatible with check_output's -iE, so manual checks)
    if grep -qP '\x1b\[1;32m' "$outfile"; then
        test_pass "ANSI — bold green (\\033[1;32m) present"
    else
        test_fail "ANSI — bold green not found"
    fi

    if grep -qP '\x1b\[1;31m' "$outfile"; then
        test_pass "ANSI — bold red (\\033[1;31m) present"
    else
        test_fail "ANSI — bold red not found"
    fi

    if grep -qP '\x1b\[0m' "$outfile"; then
        test_pass "ANSI — reset code (\\033[0m) present"
    else
        test_fail "ANSI — reset code not found"
    fi

    if grep -qP '\x1b\[2m' "$outfile"; then
        test_pass "ANSI — dim (\\033[2m) present"
    else
        test_fail "ANSI — dim not found"
    fi

    if grep -qP '\x1b\[4m' "$outfile"; then
        test_pass "ANSI — underline (\\033[4m) present"
    else
        test_fail "ANSI — underline not found"
    fi

    # Check that non-ANSI lines also exist
    check_output "$outfile" "ANSI mixed" "Plain text line"
}

# ============================================================================
# Test: dashboard key=value format
# ============================================================================

test_dashboard_format() {
    section "Dashboard Key=Value Format"
    local outfile="$RESULTS_DIR/monitor_dashboard.txt"

    python3 "$SIMULATOR" --stdout --scenario dashboard --rate 10 --duration 2 \
        > "$outfile" 2>/dev/null

    check_output "$outfile" "Dashboard" \
        "battery=[0-9]+\.[0-9]+" \
        "loop=[0-9]+" \
        "armed=(YES|NO)"
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
        test_pass "Echo round-trip: sent and received '$test_msg'"
    else
        test_fail "Echo round-trip failed — message not received"
        if [ -s "$outfile" ]; then
            echo -e "  ${YELLOW}--- Captured output ---${NC}"
            head -5 "$outfile" | sed 's/^/    /'
            echo -e "  ${YELLOW}--- (end) ---${NC}"
        fi
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
        test_pass "Received $lines lines of ANSI data through headless mode"
    else
        test_fail "No ANSI data received"
    fi

    # Verify ANSI codes pass through (headless is raw)
    if grep -qP '\x1b\[' "$outfile"; then
        test_pass "ANSI codes preserved in headless output"
    else
        test_fail "ANSI codes stripped (should be raw passthrough)"
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
