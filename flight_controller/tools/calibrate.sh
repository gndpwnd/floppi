#!/bin/bash
# Interactive Calibration Wrapper
# Menu-driven interface around serial_monitor.py for flight controller calibration.
#
# Usage:
#   ./calibrate.sh                          # Auto-detect port, launch menu
#   ./calibrate.sh /dev/ttyACM0             # Specific port, launch menu
#   ./calibrate.sh /dev/ttyACM0 imu         # Run IMU calibration directly
#   ./calibrate.sh /dev/ttyACM0 dump        # Dump values directly
#   ./calibrate.sh help                     # Show usage

set -euo pipefail

# ======================================================================
# COLORS (matches build.sh palette)
# ======================================================================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m'
BOLD='\033[1m'

# ======================================================================
# PATHS
# ======================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FC_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SERIAL_MON="$SCRIPT_DIR/serial_monitor.py"

# ======================================================================
# HELPER FUNCTIONS
# ======================================================================

print_header() {
    clear
    echo -e "${BLUE}${BOLD}"
    echo "╔══════════════════════════════════════════════════════════╗"
    echo "║          dRehmFlight VTOL Flight Controller              ║"
    echo "║              Calibration & Tuning Tool                   ║"
    echo "╚══════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

print_error() { echo -e "${RED}${BOLD}[-] $1${NC}"; }
print_success() { echo -e "${GREEN}${BOLD}[+] $1${NC}"; }
print_info() { echo -e "${CYAN}[*] $1${NC}"; }
print_warning() { echo -e "${YELLOW}[!] $1${NC}"; }

# ======================================================================
# PREREQUISITES
# ======================================================================

check_prerequisites() {
    # Check serial_monitor.py exists
    if [ ! -f "$SERIAL_MON" ]; then
        print_error "serial_monitor.py not found at $SERIAL_MON"
        exit 1
    fi

    # Check Python 3
    if ! command -v python3 &>/dev/null; then
        print_error "Python 3 not found. Install it first."
        exit 1
    fi

    # Check ModemManager
    if systemctl is-active ModemManager &>/dev/null; then
        print_warning "ModemManager is active — it will interfere with Teensy serial!"
        echo -e "  Fix: ${BOLD}sudo systemctl stop ModemManager${NC}"
        echo ""
        read -p "  Stop ModemManager now? (y/n) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            sudo systemctl stop ModemManager
            print_success "ModemManager stopped"
            sleep 1
        else
            print_warning "Continuing with ModemManager active (may cause issues)"
        fi
    fi
}

# Auto-detect serial port
detect_port() {
    local ports=()

    # Check ttyACM* first (Teensy USB CDC)
    for p in /dev/ttyACM*; do
        [ -e "$p" ] && ports+=("$p")
    done

    # Then ttyUSB* (CP2102/CH340 for ESP32)
    for p in /dev/ttyUSB*; do
        [ -e "$p" ] && ports+=("$p")
    done

    if [ ${#ports[@]} -eq 0 ]; then
        print_error "No serial ports found (/dev/ttyACM* or /dev/ttyUSB*)"
        echo "  Check: Is the board connected? Is the USB cable data-capable?"
        exit 1
    fi

    if [ ${#ports[@]} -eq 1 ]; then
        echo "${ports[0]}"
        return
    fi

    # Multiple ports — let user pick
    echo -e "${CYAN}Multiple serial ports detected:${NC}" >&2
    for i in "${!ports[@]}"; do
        echo "  $i) ${ports[$i]}" >&2
    done
    read -p "Select port (0-$((${#ports[@]} - 1))): " choice
    if [[ "$choice" =~ ^[0-9]+$ ]] && [ "$choice" -lt ${#ports[@]} ]; then
        echo "${ports[$choice]}"
    else
        echo "${ports[0]}"
    fi
}

# Release stale port holders
release_port() {
    local holders
    holders=$(fuser "$PORT" 2>/dev/null || true)
    if [ -n "$holders" ]; then
        print_info "Releasing port $PORT (held by PIDs: $holders)"
        fuser -k "$PORT" 2>/dev/null || true
        sleep 1
    fi
}

# ======================================================================
# SERIAL OPERATION MODES
# ======================================================================

# Display: send command, show output, return to menu
run_display() {
    local cmd="$1"
    local label="$2"
    local wait="${3:-3}"

    print_info "Running: $label"
    echo -e "${MAGENTA}══════════════════════════════════════════════════════════${NC}"
    echo ""

    python3 "$SERIAL_MON" "$PORT" --no-dtr-reset --boot-wait 1 \
        --send "$cmd" --wait "$wait" 2>/dev/null || true

    echo ""
    echo -e "${MAGENTA}══════════════════════════════════════════════════════════${NC}"
    echo ""
    read -p "Press Enter to return to menu..."
}

# Interactive: send initial command, user interacts with firmware via y/n etc.
# Ctrl+C returns to menu.
run_interactive() {
    local cmd="$1"
    local label="$2"

    print_info "Running: $label"
    print_warning "Type y/n to respond to firmware prompts. Ctrl+C to exit."
    echo -e "${GREEN}══════════════════════════════════════════════════════════${NC}"
    echo ""

    python3 "$SERIAL_MON" "$PORT" --no-dtr-reset --boot-wait 1 \
        --send "$cmd" --wait 1 --interactive 2>/dev/null || true

    echo ""
    echo -e "${GREEN}══════════════════════════════════════════════════════════${NC}"
    echo ""
    read -p "Press Enter to return to menu..."
}

# Tuning: show current values, then enter interactive mode for live adjustment
run_tuning() {
    local cmd="$1"
    local label="$2"

    print_info "Running: $label"
    print_warning "Current values shown first. Type commands to adjust."
    print_warning "Example: $cmd kp_roll 0.25"
    print_warning "Ctrl+C to exit."
    echo -e "${YELLOW}══════════════════════════════════════════════════════════${NC}"
    echo ""

    python3 "$SERIAL_MON" "$PORT" --no-dtr-reset --boot-wait 1 \
        --send "$cmd" --wait 1 --interactive 2>/dev/null || true

    echo ""
    echo -e "${YELLOW}══════════════════════════════════════════════════════════${NC}"
    echo ""
    read -p "Press Enter to return to menu..."
}

# ======================================================================
# MAIN MENU
# ======================================================================

main_menu() {
    while true; do
        print_header
        echo -e "  Port: ${GREEN}${BOLD}$PORT${NC}"
        echo ""
        echo -e "${BOLD}--- Information ---${NC}"
        echo "   1) Help menu              2) Calibration status"
        echo "   3) Channel status          4) PID gains"
        echo "   5) Filter & limits         6) Dump all values"
        echo ""
        echo -e "${BOLD}--- Calibration ---${NC}"
        echo "   7) IMU single-position     8) IMU 6-position"
        echo "   9) IMU + orientation       10) Radio channel mapping"
        echo "  11) Failsafe detection      12) ESC endpoints"
        echo ""
        echo -e "${BOLD}--- Tuning ---${NC}"
        echo "  13) PID tuning (interactive)   14) Filter tuning (interactive)"
        echo "  15) Telemetry toggle           16) Network diagnostics"
        echo ""
        echo -e "${BOLD}--- Workflow ---${NC}"
        echo "  17) Sequential calibration (guided)"
        echo "   0) Exit"
        echo -e "${CYAN}══════════════════════════════════════════════════════════${NC}"

        read -p "Select option: " choice

        case "$choice" in
            # --- Information (display) ---
            1)  run_display "h" "Help Menu" 3 ;;
            2)  run_display "c" "Calibration Status" 3 ;;
            3)  run_display "s" "Channel Status" 3 ;;
            4)  run_display "g" "PID Gains" 3 ;;
            5)  run_display "p" "Filter & Limits" 3 ;;
            6)  run_display "d" "Dump All Calibration Values" 5 ;;

            # --- Calibration (interactive) ---
            7)  run_interactive "i" "IMU Calibration (single-position)" ;;
            8)  run_interactive "m" "IMU Calibration (6-position)" ;;
            9)  run_interactive "o" "IMU + Orientation Detection" ;;
            10) run_interactive "r" "Radio Channel Mapping" ;;
            11) run_interactive "f" "Failsafe Auto-Detection" ;;
            12) run_interactive "e" "ESC Endpoint Calibration" ;;

            # --- Tuning ---
            13) run_tuning "g" "PID Tuning" ;;
            14) run_tuning "p" "Filter & Limits Tuning" ;;
            15) run_display "t" "Telemetry Toggle" 2 ;;
            16) run_display "n" "Network Diagnostics" 5 ;;

            # --- Workflow ---
            17) run_interactive "a" "Sequential Calibration (guided)" ;;

            0)
                print_header
                echo -e "${GREEN}Calibration session ended.${NC}"
                echo ""
                exit 0
                ;;
            *)
                print_error "Invalid option"
                sleep 1
                ;;
        esac
    done
}

# ======================================================================
# CLI MODE — direct command execution without menu
# ======================================================================

cli_dispatch() {
    local cmd="$1"
    case "$cmd" in
        help)         run_display "h" "Help Menu" 3 ;;
        status)       run_display "c" "Calibration Status" 3 ;;
        channels)     run_display "s" "Channel Status" 3 ;;
        pid)          run_display "g" "PID Gains" 3 ;;
        filters)      run_display "p" "Filter & Limits" 3 ;;
        dump)         run_display "d" "Dump All Calibration Values" 5 ;;
        imu)          run_interactive "i" "IMU Calibration (single-position)" ;;
        imu6)         run_interactive "m" "IMU Calibration (6-position)" ;;
        orientation)  run_interactive "o" "IMU + Orientation Detection" ;;
        radio)        run_interactive "r" "Radio Channel Mapping" ;;
        failsafe)     run_interactive "f" "Failsafe Auto-Detection" ;;
        esc)          run_interactive "e" "ESC Endpoint Calibration" ;;
        tuning-pid)   run_tuning "g" "PID Tuning" ;;
        tuning-filter) run_tuning "p" "Filter & Limits Tuning" ;;
        telemetry)    run_display "t" "Telemetry Toggle" 2 ;;
        network)      run_display "n" "Network Diagnostics" 5 ;;
        sequential)   run_interactive "a" "Sequential Calibration (guided)" ;;
        *)
            print_error "Unknown command: $cmd"
            echo ""
            echo "Available commands:"
            echo "  help status channels pid filters dump"
            echo "  imu imu6 orientation radio failsafe esc"
            echo "  tuning-pid tuning-filter telemetry network"
            echo "  sequential"
            exit 1
            ;;
    esac
}

# ======================================================================
# ENTRY POINT
# ======================================================================

# Handle --help / help as first argument
if [[ "${1:-}" == "--help" || "${1:-}" == "-h" || "${1:-}" == "help" ]]; then
    echo -e "${BLUE}${BOLD}Calibration & Tuning Tool${NC}"
    echo ""
    echo "Usage:"
    echo "  ./calibrate.sh                          # Auto-detect port, launch menu"
    echo "  ./calibrate.sh /dev/ttyACM0             # Specific port, launch menu"
    echo "  ./calibrate.sh /dev/ttyACM0 imu         # Run command directly"
    echo ""
    echo "Commands:"
    echo "  help status channels pid filters dump"
    echo "  imu imu6 orientation radio failsafe esc"
    echo "  tuning-pid tuning-filter telemetry network"
    echo "  sequential"
    echo ""
    echo "Firmware serial commands (for reference):"
    echo "  h c s g p d t r i m o f e n a"
    exit 0
fi

check_prerequisites

# Parse arguments: [port] [command]
if [[ $# -ge 1 && "$1" == /dev/* ]]; then
    PORT="$1"
    shift
elif [[ $# -ge 1 ]]; then
    # First arg is not a port — auto-detect
    PORT=$(detect_port)
else
    PORT=$(detect_port)
fi

# Verify port exists
if [ ! -e "$PORT" ]; then
    print_error "Serial port $PORT not found"
    echo "  Check: ls /dev/ttyACM* /dev/ttyUSB*"
    exit 1
fi

release_port

if [[ $# -ge 1 ]]; then
    # CLI mode — run command directly
    cli_dispatch "$1"
else
    # Menu mode
    main_menu
fi
