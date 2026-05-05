#!/bin/bash

###############################################################################
# Arduino Mega Bootloader Recovery Script
#
# Automatically diagnoses and recovers from bootloader upload failures on Linux.
# Tries multiple recovery strategies in sequence and reports which worked.
#
# Usage:
#   ./tools/recover_bootloader.sh
#   ./tools/recover_bootloader.sh /dev/ttyACM0     # Specify custom USB port
#   ./tools/recover_bootloader.sh --verbose        # Show detailed debug info
#
# This script handles:
# - DTR/RTS reset timing issues (most common on Linux)
# - USB power cycle recovery
# - Baud rate adjustment
# - Bootloader presence verification
#
###############################################################################

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}"
AUTO_ORIENTATION_DIR="${PROJECT_ROOT}/auto_orientation"
VERBOSE=false
CUSTOM_PORT=""
LOG_FILE="/tmp/bootloader_recovery_$(date +%s).log"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --verbose)
            VERBOSE=true
            shift
            ;;
        /dev/tty*)
            CUSTOM_PORT="$1"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--verbose] [/dev/ttyXXX]"
            exit 1
            ;;
    esac
done

###############################################################################
# Utility Functions
###############################################################################

log() {
    local level=$1
    shift
    local msg="$@"
    local timestamp=$(date '+%H:%M:%S')

    case $level in
        INFO)
            echo -e "${BLUE}[${timestamp}]${NC} ${msg}"
            ;;
        SUCCESS)
            echo -e "${GREEN}[${timestamp}] ✓${NC} ${msg}"
            ;;
        WARN)
            echo -e "${YELLOW}[${timestamp}] ⚠${NC} ${msg}"
            ;;
        ERROR)
            echo -e "${RED}[${timestamp}] ✗${NC} ${msg}"
            ;;
    esac

    echo "[${timestamp}] [${level}] ${msg}" >> "${LOG_FILE}"
}

verbose_log() {
    if [[ "${VERBOSE}" == true ]]; then
        log INFO "$@"
    fi
}

print_section() {
    echo ""
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}$@${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
}

###############################################################################
# Diagnostics
###############################################################################

check_environment() {
    print_section "Checking Environment"

    # Check if in correct directory
    if [[ ! -d "${AUTO_ORIENTATION_DIR}" ]]; then
        log ERROR "auto_orientation directory not found at ${AUTO_ORIENTATION_DIR}"
        log ERROR "Please run this script from the floppi root directory"
        exit 1
    fi
    verbose_log "Found auto_orientation project"

    # Check if platformio is installed
    if ! command -v pio &> /dev/null; then
        log ERROR "PlatformIO (pio) not found in PATH"
        log WARN "Install with: pip install platformio"
        exit 1
    fi
    verbose_log "PlatformIO found: $(pio --version)"

    # Check for avrdude
    if ! command -v avrdude &> /dev/null; then
        log WARN "avrdude not found in PATH (will try pio's bundled version)"
    else
        verbose_log "avrdude found: $(avrdude -v 2>&1 | head -1)"
    fi

    # Determine USB port
    if [[ -n "${CUSTOM_PORT}" ]]; then
        USB_PORT="${CUSTOM_PORT}"
        verbose_log "Using custom USB port: ${USB_PORT}"
    else
        # Try to find the port
        if [[ -e /dev/ttyACM0 ]]; then
            USB_PORT="/dev/ttyACM0"
        elif [[ -e /dev/ttyUSB0 ]]; then
            USB_PORT="/dev/ttyUSB0"
        else
            # List available ports
            log WARN "No standard USB port found. Checking available ports..."
            available=$(ls /dev/tty* 2>/dev/null | grep -E 'ttyACM|ttyUSB' || true)
            if [[ -z "${available}" ]]; then
                log ERROR "No USB serial ports found. Please:"
                log ERROR "1. Check USB cable is connected"
                log ERROR "2. Run: lsusb (look for Arduino Mega 2560)"
                log ERROR "3. Run: ls /dev/tty*"
                exit 1
            fi
            USB_PORT=$(echo "${available}" | head -1)
            log WARN "Found port: ${USB_PORT}"
        fi
    fi

    if [[ ! -e "${USB_PORT}" ]]; then
        log ERROR "USB port ${USB_PORT} does not exist"
        log ERROR "Available ports: $(ls /dev/tty* 2>/dev/null | grep -E 'ttyACM|ttyUSB' || echo 'none')"
        exit 1
    fi

    log SUCCESS "Environment check passed"
    log INFO "USB Port: ${USB_PORT}"
}

diagnose_bootloader() {
    print_section "Diagnosing Bootloader Status"

    log INFO "Attempting to read device signature (bootloader must respond)..."

    # Create temporary script to test connection
    local test_dir="/tmp/mega_bootloader_test"
    mkdir -p "${test_dir}"

    # Minimal test sketch
    cat > "${test_dir}/test.ino" << 'EOF'
void setup() {
  Serial.begin(115200);
  Serial.println("Arduino Mega Ready");
}

void loop() {
  delay(100);
}
EOF

    cd "${test_dir}" || exit 1

    # Try to read signature with avrdude through PlatformIO
    if timeout 5 pio run -p megaatmega2560 2>&1 | grep -q "Device signature"; then
        log SUCCESS "Bootloader is responding!"
        return 0
    else
        log WARN "Bootloader may not be responding (timeout or no serial connection)"
        return 1
    fi
}

###############################################################################
# Recovery Procedures
###############################################################################

recovery_attempt_count=0
successful_recovery=""

attempt_recovery() {
    local method=$1
    local description=$2

    recovery_attempt_count=$((recovery_attempt_count + 1))
    print_section "Recovery Attempt #${recovery_attempt_count}: ${description}"

    log INFO "Method: ${method}"
    log INFO "Description: ${description}"
}

record_success() {
    successful_recovery="$1"
    log SUCCESS "Recovery successful! ($(date '+%H:%M:%S'))"
}

###############################################################################
# Strategy 1: Reset Button Timing
###############################################################################

strategy_reset_button() {
    attempt_recovery "RESET_BUTTON" "Manual Reset Button Timing"

    log WARN "This strategy requires manual intervention."
    log INFO "Follow these steps:"
    echo ""
    echo "  1. Place your finger on the RESET button (red button on Mega)"
    echo "  2. When prompted, press and HOLD the reset button"
    echo "  3. Release when you see 'Found programmer' in output"
    echo "  4. If successful, upload completes automatically"
    echo ""

    read -p "Ready to try? (y/n) " -n 1 -r
    echo ""

    if [[ $REPLY =~ ^[Yy]$ ]]; then
        log INFO "Starting upload in 2 seconds..."
        sleep 2

        cd "${AUTO_ORIENTATION_DIR}" || exit 1

        # Run upload and capture output
        if pio run -e arduino_mega -t upload -v 2>&1 | tee -a "${LOG_FILE}"; then
            record_success "RESET_BUTTON"
            return 0
        else
            log WARN "Reset button timing may have been off"
            return 1
        fi
    else
        log WARN "Skipped reset button strategy"
        return 1
    fi
}

###############################################################################
# Strategy 2: USB Power Cycle
###############################################################################

strategy_usb_power_cycle() {
    attempt_recovery "USB_POWER_CYCLE" "USB Power Cycle Recovery"

    log WARN "This strategy requires carefully timed USB replugging."
    log INFO "Follow these steps:"
    echo ""
    echo "  1. Unplug USB cable from Arduino Mega"
    echo "  2. Wait 2 seconds"
    echo "  3. When prompted, start the upload"
    echo "  4. Watch for 'Connecting to programmer...' message"
    echo "  5. Immediately plug USB cable back in"
    echo "  6. (Timing: within 1-2 seconds of upload start)"
    echo ""

    read -p "Ready to try? (y/n) " -n 1 -r
    echo ""

    if [[ $REPLY =~ ^[Yy]$ ]]; then
        log WARN "Unplug the USB cable NOW and wait 2 seconds..."
        sleep 2

        log INFO "Starting upload in 3 seconds - be ready to plug in..."
        sleep 3

        cd "${AUTO_ORIENTATION_DIR}" || exit 1

        # Run upload with timeout to allow replugging
        if timeout 30 pio run -e arduino_mega -t upload -v 2>&1 | tee -a "${LOG_FILE}"; then
            record_success "USB_POWER_CYCLE"
            return 0
        else
            log WARN "USB power cycle timing may have been off"
            return 1
        fi
    else
        log WARN "Skipped USB power cycle strategy"
        return 1
    fi
}

###############################################################################
# Strategy 3: Different USB Port
###############################################################################

strategy_different_port() {
    attempt_recovery "DIFFERENT_PORT" "Try Different USB Port"

    log INFO "Checking for alternative USB ports..."

    # Find all USB serial ports
    local ports=()
    for port in /dev/ttyACM* /dev/ttyUSB*; do
        if [[ -e "$port" ]] && [[ "$port" != "${USB_PORT}" ]]; then
            ports+=("$port")
        fi
    done

    if [[ ${#ports[@]} -eq 0 ]]; then
        log WARN "No alternative USB ports found"
        return 1
    fi

    local alt_port="${ports[0]}"
    log INFO "Trying alternative port: ${alt_port}"

    # Create temporary platformio.ini override
    local temp_ini=$(mktemp)
    cd "${AUTO_ORIENTATION_DIR}" || exit 1

    # Copy original and modify
    cp platformio.ini "${temp_ini}"
    sed -i "s|upload_port = .*|upload_port = ${alt_port}|" "${temp_ini}"
    sed -i "s|monitor_port = .*|monitor_port = ${alt_port}|" "${temp_ini}"

    if pio run -e arduino_mega -t upload -v 2>&1 | tee -a "${LOG_FILE}"; then
        record_success "DIFFERENT_PORT (${alt_port})"
        log INFO "Update platformio.ini to use: ${alt_port}"
        return 0
    else
        log WARN "Alternative port did not work"
        rm "${temp_ini}"
        return 1
    fi
}

###############################################################################
# Strategy 4: Slower Baud Rate
###############################################################################

strategy_slower_baud() {
    attempt_recovery "SLOWER_BAUD" "Reduce Upload Speed for Better Timing"

    local original_speed=115200
    local slower_speeds=(57600 38400 19200 9600)

    cd "${AUTO_ORIENTATION_DIR}" || exit 1

    for speed in "${slower_speeds[@]}"; do
        log INFO "Trying upload speed: ${speed} baud"

        # Create temporary platformio.ini
        local temp_ini=$(mktemp)
        cp platformio.ini "${temp_ini}"
        sed -i "s|upload_speed = .*|upload_speed = ${speed}|" "${temp_ini}"

        # Replace platformio.ini temporarily
        local backup="${AUTO_ORIENTATION_DIR}/platformio.ini.backup"
        cp platformio.ini "${backup}"
        cp "${temp_ini}" platformio.ini

        if timeout 120 pio run -e arduino_mega -t upload -v 2>&1 | tee -a "${LOG_FILE}"; then
            record_success "SLOWER_BAUD (${speed})"
            log SUCCESS "Successfully uploaded at ${speed} baud!"
            log INFO "Consider updating platformio.ini upload_speed to ${speed}"
            cp "${backup}" platformio.ini
            return 0
        else
            log WARN "Speed ${speed} did not work"
            cp "${backup}" platformio.ini
            rm "${temp_ini}"
        fi
    done

    return 1
}

###############################################################################
# Strategy 5: Check Serial Driver Configuration
###############################################################################

strategy_check_serial_driver() {
    attempt_recovery "SERIAL_DRIVER" "Verify Serial Driver Configuration"

    log INFO "Checking USB serial driver configuration..."

    # Determine which driver is in use
    local driver_info=$(lsusb -v 2>/dev/null | grep -A2 "Arduino" || echo "")

    if [[ -z "${driver_info}" ]]; then
        log WARN "Could not identify Arduino device via lsusb"
        log INFO "Try manual troubleshooting:"
        echo ""
        echo "  1. Verify USB cable is working (try different cable)"
        echo "  2. Check Arduino Mega is powered (LED should be lit)"
        echo "  3. Install latest USB drivers: sudo apt install arduino-core"
        echo "  4. Check permissions: sudo usermod -aG dialout \$USER"
        echo ""
        return 1
    fi

    verbose_log "Found Arduino device"

    # Check if we can read the device
    if timeout 2 stty -F "${USB_PORT}" 115200 2>&1 | head -1; then
        log SUCCESS "Serial port is accessible"
        return 0
    else
        log ERROR "Cannot access serial port ${USB_PORT}"
        log INFO "Try: sudo chmod 666 ${USB_PORT}"
        return 1
    fi
}

###############################################################################
# Main Recovery Flow
###############################################################################

main() {
    print_section "Arduino Mega Bootloader Recovery Tool"

    log INFO "Log file: ${LOG_FILE}"
    log INFO "Project root: ${PROJECT_ROOT}"

    # Check environment
    check_environment

    # Try to diagnose current status
    diagnose_bootloader || true

    echo ""
    echo "Recovery will attempt strategies in this order:"
    echo "  1. Manual reset button timing"
    echo "  2. USB power cycle"
    echo "  3. Different USB port"
    echo "  4. Slower baud rates"
    echo "  5. Serial driver check"
    echo ""

    read -p "Continue with recovery? (y/n) " -n 1 -r
    echo ""

    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log INFO "Recovery cancelled by user"
        exit 0
    fi

    # Try each strategy
    strategy_reset_button && show_results && exit 0
    strategy_usb_power_cycle && show_results && exit 0
    strategy_different_port && show_results && exit 0
    strategy_slower_baud && show_results && exit 0
    strategy_check_serial_driver && show_results && exit 0

    # If we got here, all strategies failed
    show_final_failure
}

show_results() {
    print_section "Recovery Complete!"

    log SUCCESS "Bootloader recovery successful!"
    echo ""
    echo "Details:"
    echo "  Successful method: ${successful_recovery}"
    echo "  Recovery attempts: ${recovery_attempt_count}"
    echo "  Full log: ${LOG_FILE}"
    echo ""

    # Verify upload
    log INFO "Verifying upload by reading device signature..."
    cd "${AUTO_ORIENTATION_DIR}" || exit 1

    if pio run -e arduino_mega -t upload -v 2>&1 | grep -q "Device signature"; then
        log SUCCESS "Device signature verified - bootloader is responsive!"
        return 0
    fi
}

show_final_failure() {
    print_section "Recovery Failed"

    log ERROR "All recovery strategies were unsuccessful."
    echo ""
    echo "This suggests one of the following:"
    echo "  1. Bootloader has been erased (need ICSP re-flash)"
    echo "  2. Hardware failure (USB port, serial circuit)"
    echo "  3. Incorrect USB port or driver issue"
    echo ""
    echo "Next steps:"
    echo "  1. Check the bootloader recovery guide:"
    echo "     ${PROJECT_ROOT}/docs/findings/bootloader_recovery_guide.md"
    echo ""
    echo "  2. If bootloader is erased, re-flash via ICSP:"
    echo "     - Get an ICSP programmer (Arduino as ISP)"
    echo "     - See guide for pinout and procedure"
    echo ""
    echo "  3. Debug information saved to: ${LOG_FILE}"
    echo "     Share this with Arduino support if needed"
    echo ""

    exit 1
}

# Run main
main
