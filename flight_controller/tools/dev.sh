#!/bin/bash
# Unified development workflow for flight controller firmware.
# Dynamically parses platformio.ini — no hardcoded environment names.
#
# Usage:
#   ./dev.sh build [ENV]              Build firmware (default: teensy40)
#   ./dev.sh flash [ENV]              Build + flash firmware
#   ./dev.sh monitor [PORT]           Open serial monitor
#   ./dev.sh go [ENV]                 Build + flash + monitor (full workflow)
#   ./dev.sh test [PORT] [SUITE]      Run calibration test suite
#   ./dev.sh calibrate [PORT] [CMD]   Open calibration tool
#   ./dev.sh envs                     List all build environments
#   ./dev.sh build-all                Build every environment (CI check)
#   ./dev.sh diagnose [PORT]          Check system readiness
#   ./dev.sh help                     Show this help
#
# Environment resolution:
#   ENV is any [env:NAME] from platformio.ini.
#   If omitted, uses platformio.ini default_envs.
#
# Port resolution:
#   PORT auto-detected from /dev/ttyACM* or /dev/ttyUSB*.
#   Override with explicit path: ./dev.sh monitor /dev/ttyUSB0

set -euo pipefail

# ======================================================================
# PATHS
# ======================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FC_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PLATFORMIO_INI="$FC_DIR/platformio.ini"
SERIAL_MON="$SCRIPT_DIR/serial_monitor.py"
CALIBRATE_SH="$SCRIPT_DIR/calibrate.sh"
TEST_SH="$FC_DIR/tests/test_calibration.sh"
TEENSY_REBOOT="$HOME/.platformio/packages/tool-teensy/teensy_reboot"

# ======================================================================
# COLORS
# ======================================================================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m'
BOLD='\033[1m'

ok()   { echo -e "${GREEN}${BOLD}[OK]${NC} $1"; }
err()  { echo -e "${RED}${BOLD}[ERROR]${NC} $1" >&2; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
info() { echo -e "${CYAN}[INFO]${NC} $1"; }
step() { echo -e "${BLUE}${BOLD}[$1]${NC} $2"; }

# ======================================================================
# PLATFORMIO.INI PARSING (dynamic — no hardcoded envs)
# ======================================================================

# Extract all [env:NAME] entries from platformio.ini
get_envs() {
    if [ ! -f "$PLATFORMIO_INI" ]; then
        err "platformio.ini not found at $PLATFORMIO_INI"
        exit 1
    fi
    grep -oP '^\[env:(\K[a-zA-Z0-9_-]+)(?=\])' "$PLATFORMIO_INI"
}

# Get default_envs from [platformio] section
get_default_env() {
    if [ -f "$PLATFORMIO_INI" ]; then
        grep -oP '^default_envs\s*=\s*\K[a-zA-Z0-9_-]+' "$PLATFORMIO_INI" 2>/dev/null || true
    fi
}

# Get board name for a given environment
get_board() {
    local env="$1"
    local in_section=false
    while IFS= read -r line; do
        if [[ "$line" =~ ^\[env:${env}\]$ ]]; then
            in_section=true
            continue
        fi
        if $in_section && [[ "$line" =~ ^\[ ]]; then
            break
        fi
        if $in_section && [[ "$line" =~ ^board[[:space:]]*=[[:space:]]*(.+) ]]; then
            echo "${BASH_REMATCH[1]}"
            return
        fi
    done < "$PLATFORMIO_INI"

    # Check if it extends another env
    local extends
    extends=$(get_extends "$env")
    if [ -n "$extends" ]; then
        get_board "$extends"
    fi
}

# Get extends= value for an environment
get_extends() {
    local env="$1"
    local in_section=false
    while IFS= read -r line; do
        if [[ "$line" =~ ^\[env:${env}\]$ ]]; then
            in_section=true
            continue
        fi
        if $in_section && [[ "$line" =~ ^\[ ]]; then
            break
        fi
        if $in_section && [[ "$line" =~ ^extends[[:space:]]*=[[:space:]]*env:(.+) ]]; then
            echo "${BASH_REMATCH[1]}"
            return
        fi
    done < "$PLATFORMIO_INI"
}

# Resolve ENV arg: use provided, or fall back to default_envs
resolve_env() {
    local env="${1:-}"
    if [ -z "$env" ]; then
        env=$(get_default_env)
    fi
    if [ -z "$env" ]; then
        err "No environment specified and no default_envs in platformio.ini"
        echo "  Available: $(get_envs | tr '\n' ' ')" >&2
        exit 1
    fi
    # Validate env exists
    if ! get_envs | grep -qx "$env"; then
        err "Unknown environment: $env"
        echo "  Available: $(get_envs | tr '\n' ' ')" >&2
        exit 1
    fi
    echo "$env"
}

# ======================================================================
# PORT DETECTION & SYSTEM CHECKS
# ======================================================================

# Auto-detect serial port
detect_port() {
    local override="${1:-}"
    if [ -n "$override" ] && [[ "$override" == /dev/* ]]; then
        if [ -e "$override" ]; then
            echo "$override"
            return
        else
            err "Specified port $override not found"
            exit 1
        fi
    fi

    # Check ttyACM* first (Teensy USB CDC), then ttyUSB* (CP2102/CH340 for ESP32)
    local port
    port=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -1 || true)
    if [ -n "$port" ]; then
        echo "$port"
        return
    fi

    err "No serial ports found (/dev/ttyACM* or /dev/ttyUSB*)"
    echo "  Check: Is the board plugged in? Is the USB cable data-capable?" >&2
    exit 1
}

# Wait for serial port to appear (after flash)
wait_for_port() {
    local max_wait="${1:-15}"
    info "Waiting for serial port (up to ${max_wait}s)..."
    for i in $(seq 1 "$max_wait"); do
        local port
        port=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -1 || true)
        if [ -n "$port" ]; then
            ok "Port found: $port"
            echo "$port"
            return
        fi
        sleep 1
    done
    err "No serial port found after ${max_wait}s"
    exit 1
}

# Release stale port holders
release_port() {
    local port="$1"
    if command -v fuser &>/dev/null; then
        local holders
        holders=$(fuser "$port" 2>/dev/null || true)
        if [ -n "$holders" ]; then
            info "Releasing port $port (held by PIDs: $holders)"
            fuser -k "$port" 2>/dev/null || true
            sleep 1
        fi
    fi
}

# Check and optionally stop ModemManager
check_modemmanager() {
    if ! command -v systemctl &>/dev/null; then
        return
    fi
    if systemctl is-active ModemManager &>/dev/null; then
        warn "ModemManager is active — it will corrupt Teensy USB CDC serial!"
        info "Stopping ModemManager..."
        if sudo systemctl stop ModemManager 2>/dev/null; then
            ok "ModemManager stopped"
        else
            err "Failed to stop ModemManager. Run: sudo systemctl stop ModemManager"
            exit 1
        fi
    fi
}

# Check all prerequisites for serial operations
check_serial_prereqs() {
    check_modemmanager
    if [ ! -f "$SERIAL_MON" ]; then
        err "serial_monitor.py not found at $SERIAL_MON"
        exit 1
    fi
    if ! command -v python3 &>/dev/null; then
        err "Python 3 not found"
        exit 1
    fi
}

# Reboot Teensy (only way to reliably reset Teensy 4.0)
reboot_board() {
    local port="$1"
    if [[ "$port" == *ttyACM* ]] && [ -x "$TEENSY_REBOOT" ]; then
        info "Rebooting Teensy..."
        "$TEENSY_REBOOT" 2>/dev/null || true
        sleep 3
    fi
}

# ======================================================================
# COMMANDS
# ======================================================================

cmd_build() {
    local env
    env=$(resolve_env "${1:-}")
    local board
    board=$(get_board "$env")

    step "BUILD" "$env (board: ${board:-unknown})"
    cd "$FC_DIR"
    if pio run -e "$env"; then
        ok "Build succeeded: $env"
    else
        err "Build failed: $env"
        exit 1
    fi
}

cmd_flash() {
    local env
    env=$(resolve_env "${1:-}")
    local board
    board=$(get_board "$env")

    step "FLASH" "$env (board: ${board:-unknown})"
    cd "$FC_DIR"
    if pio run -e "$env" -t upload; then
        ok "Flash succeeded: $env"
    else
        err "Flash failed: $env"
        echo "  Teensy: press the PROGRAM button and retry" >&2
        exit 1
    fi
}

cmd_monitor() {
    local port
    port=$(detect_port "${1:-}")
    check_serial_prereqs
    release_port "$port"

    step "MONITOR" "$port @ 115200 baud"
    info "Interactive serial monitor. Ctrl+C to exit."
    python3 "$SERIAL_MON" "$port" --no-dtr-reset --boot-wait 0 --interactive
}

cmd_go() {
    local env
    env=$(resolve_env "${1:-}")

    # Build + flash
    cmd_flash "$env"

    # Wait for port
    local port
    port=$(wait_for_port 15)

    # Wait for firmware boot (IMU warmup takes 5-10s)
    check_serial_prereqs
    release_port "$port"

    step "BOOT" "Waiting for firmware boot..."
    python3 "$SERIAL_MON" "$port" --no-dtr-reset --boot-wait 0 \
        --wait-for "READY\|CALIBRATION MODE" --timeout 20 --quiet 2>/dev/null || true

    # Open interactive monitor
    step "MONITOR" "$port @ 115200 baud"
    info "Interactive serial monitor. Ctrl+C to exit."
    sleep 0.3
    python3 "$SERIAL_MON" "$port" --no-dtr-reset --boot-wait 0 --interactive
}

cmd_test() {
    local port
    port=$(detect_port "${1:-}")
    local suite="${2:-all}"
    check_serial_prereqs

    if [ ! -f "$TEST_SH" ]; then
        err "test_calibration.sh not found at $TEST_SH"
        exit 1
    fi

    step "TEST" "suite=$suite port=$port"
    bash "$TEST_SH" "$port" "$suite"
}

cmd_calibrate() {
    local port
    port=$(detect_port "${1:-}")
    local subcmd="${2:-}"
    check_serial_prereqs

    if [ ! -f "$CALIBRATE_SH" ]; then
        err "calibrate.sh not found at $CALIBRATE_SH"
        exit 1
    fi

    step "CALIBRATE" "port=$port ${subcmd:+cmd=$subcmd}"
    if [ -n "$subcmd" ]; then
        bash "$CALIBRATE_SH" "$port" "$subcmd"
    else
        bash "$CALIBRATE_SH" "$port"
    fi
}

cmd_envs() {
    local default_env
    default_env=$(get_default_env)
    local envs
    envs=$(get_envs)

    echo -e "${CYAN}${BOLD}Build Environments${NC} (from platformio.ini):"
    echo ""
    while IFS= read -r env; do
        local board
        board=$(get_board "$env")
        local marker=""
        if [ "$env" = "$default_env" ]; then
            marker=" ${GREEN}(default)${NC}"
        fi
        echo -e "  ${BOLD}$env${NC}${marker}"
        echo -e "    board: ${MAGENTA}${board:-unknown}${NC}"
    done <<< "$envs"
    echo ""
    echo -e "  Total: $(echo "$envs" | wc -l) environments"
}

cmd_build_all() {
    local envs
    envs=$(get_envs)
    local pass=0
    local fail=0
    local skip=0
    local failed_envs=()

    # Filter out abstract base envs (no board defined)
    local buildable_envs=()
    while IFS= read -r env; do
        local board
        board=$(get_board "$env")
        if [ -n "$board" ]; then
            buildable_envs+=("$env")
        else
            skip=$((skip + 1))
        fi
    done <<< "$envs"

    local total=${#buildable_envs[@]}
    step "BUILD-ALL" "$total environments (${skip} abstract bases skipped)"
    echo ""

    cd "$FC_DIR"
    for env in "${buildable_envs[@]}"; do
        echo -ne "  Building ${BOLD}$env${NC}... "
        if pio run -e "$env" >/dev/null 2>&1; then
            echo -e "${GREEN}OK${NC}"
            pass=$((pass + 1))
        else
            echo -e "${RED}FAIL${NC}"
            fail=$((fail + 1))
            failed_envs+=("$env")
        fi
    done

    echo ""
    echo -e "  Results: ${GREEN}$pass passed${NC}, ${RED}$fail failed${NC} / $total total"
    if [ ${#failed_envs[@]} -gt 0 ]; then
        echo -e "  ${RED}Failed:${NC} ${failed_envs[*]}"
        exit 1
    fi
    ok "All $total environments build successfully"
}

cmd_diagnose() {
    local port_arg="${1:-}"

    step "DIAGNOSE" "System readiness check"
    echo ""

    # PlatformIO
    if command -v pio &>/dev/null; then
        ok "PlatformIO: $(pio --version 2>/dev/null | head -1)"
    else
        err "PlatformIO: not found"
    fi

    # Python
    if command -v python3 &>/dev/null; then
        ok "Python 3: $(python3 --version 2>&1)"
    else
        err "Python 3: not found"
    fi

    # serial_monitor.py
    if [ -f "$SERIAL_MON" ]; then
        ok "serial_monitor.py: found"
    else
        err "serial_monitor.py: not found at $SERIAL_MON"
    fi

    # teensy_reboot
    if [ -x "$TEENSY_REBOOT" ]; then
        ok "teensy_reboot: found"
    else
        warn "teensy_reboot: not found (needed for Teensy board reset)"
    fi

    # ModemManager
    if command -v systemctl &>/dev/null; then
        if systemctl is-active ModemManager &>/dev/null; then
            err "ModemManager: ACTIVE (will corrupt serial — run: sudo systemctl stop ModemManager)"
        else
            ok "ModemManager: inactive"
        fi
    else
        info "ModemManager: systemctl not available (non-systemd system)"
    fi

    # Serial ports
    local ports
    ports=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || true)
    if [ -n "$ports" ]; then
        ok "Serial ports:"
        echo "$ports" | while read -r p; do
            local holders
            holders=$(fuser "$p" 2>/dev/null || true)
            if [ -n "$holders" ]; then
                echo -e "    $p ${YELLOW}(held by PIDs: $holders)${NC}"
            else
                echo -e "    $p ${GREEN}(free)${NC}"
            fi
        done
    else
        warn "Serial ports: none found"
    fi

    # platformio.ini
    if [ -f "$PLATFORMIO_INI" ]; then
        local env_count
        env_count=$(get_envs | wc -l)
        ok "platformio.ini: $env_count environments"
    else
        err "platformio.ini: not found"
    fi

    echo ""
}

cmd_help() {
    echo -e "${BLUE}${BOLD}Flight Controller Dev Tool${NC}"
    echo ""
    echo "Usage: ./dev.sh <command> [args...]"
    echo ""
    echo -e "${BOLD}Commands:${NC}"
    echo "  build [ENV]              Build firmware for environment"
    echo "  flash [ENV]              Build + flash firmware"
    echo "  monitor [PORT]           Open serial monitor (interactive)"
    echo "  go [ENV]                 Build + flash + wait for boot + monitor"
    echo "  test [PORT] [SUITE]      Run calibration test suite"
    echo "  calibrate [PORT] [CMD]   Open calibration tool (menu or direct)"
    echo "  envs                     List all build environments"
    echo "  build-all                Build every environment (CI check)"
    echo "  diagnose [PORT]          Check system readiness"
    echo "  help                     Show this help"
    echo ""
    echo -e "${BOLD}Environment:${NC}"
    echo "  ENV is any [env:NAME] from platformio.ini."
    echo "  If omitted, uses default_envs (currently: $(get_default_env))."
    echo "  Available: $(get_envs | tr '\n' ' ')"
    echo ""
    echo -e "${BOLD}Port:${NC}"
    echo "  Auto-detected from /dev/ttyACM* or /dev/ttyUSB*."
    echo "  Override: ./dev.sh monitor /dev/ttyUSB0"
    echo ""
    echo -e "${BOLD}Examples:${NC}"
    echo "  ./dev.sh go teensy40_calibration    # Build, flash, monitor"
    echo "  ./dev.sh build esp32                # Build ESP32 firmware"
    echo "  ./dev.sh flash teensy40             # Build + flash live firmware"
    echo "  ./dev.sh monitor                    # Auto-detect port, open monitor"
    echo "  ./dev.sh test /dev/ttyACM0 all      # Run all calibration tests"
    echo "  ./dev.sh calibrate                  # Auto-detect port, open menu"
    echo "  ./dev.sh build-all                  # Verify all envs compile"
    echo "  ./dev.sh diagnose                   # System readiness check"
}

# ======================================================================
# ENTRY POINT
# ======================================================================

COMMAND="${1:-help}"
shift 2>/dev/null || true

case "$COMMAND" in
    build)       cmd_build "$@" ;;
    flash)       cmd_flash "$@" ;;
    monitor|mon) cmd_monitor "$@" ;;
    go)          cmd_go "$@" ;;
    test)        cmd_test "$@" ;;
    calibrate|cal) cmd_calibrate "$@" ;;
    envs|list)   cmd_envs ;;
    build-all)   cmd_build_all ;;
    diagnose|diag) cmd_diagnose "$@" ;;
    help|--help|-h) cmd_help ;;
    *)
        err "Unknown command: $COMMAND"
        echo "  Run: ./dev.sh help" >&2
        exit 1
        ;;
esac
