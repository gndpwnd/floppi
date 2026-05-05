#!/bin/bash
#=============================================================================
# Floppi Project — System Permissions Setup (idempotent)
#=============================================================================
# Sets up udev rules and permissions for Teensy/ESP32 development.
# Run with: sudo bash setup_permissions.sh
#
# Safe to run multiple times — checks before modifying.
#=============================================================================

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log()  { echo -e "${GREEN}[OK]${NC} $1"; }
skip() { echo -e "${YELLOW}[SKIP]${NC} $1 (already done)"; }
fail() { echo -e "${RED}[FAIL]${NC} $1"; }

if [ "$(id -u)" -ne 0 ]; then
    echo "This script must be run as root (sudo)."
    exit 1
fi

REAL_USER="${SUDO_USER:-$(logname 2>/dev/null || echo $USER)}"
echo "Setting up permissions for user: $REAL_USER"
echo ""

#-----------------------------------------------------------------------------
# 1. Teensy udev rules
#-----------------------------------------------------------------------------
TEENSY_RULES="/etc/udev/rules.d/00-teensy.rules"
PIO_TEENSY_RULES="$(find /home/$REAL_USER/.platformio -name '00-teensy.rules' 2>/dev/null | head -1)"

if [ -f "$TEENSY_RULES" ]; then
    skip "Teensy udev rules ($TEENSY_RULES)"
else
    if [ -n "$PIO_TEENSY_RULES" ]; then
        cp "$PIO_TEENSY_RULES" "$TEENSY_RULES"
        log "Installed Teensy udev rules from PlatformIO"
    else
        cat > "$TEENSY_RULES" << 'RULES'
# UDEV Rules for Teensy boards (www.pjrc.com/teensy)
ATTRS{idVendor}=="16c0", ATTRS{idProduct}=="04*", ENV{ID_MM_DEVICE_IGNORE}="1", ENV{ID_MM_PORT_IGNORE}="1"
ATTRS{idVendor}=="16c0", ATTRS{idProduct}=="04[789a]*", ENV{MTP_NO_PROBE}="1"
KERNEL=="ttyACM*", ATTRS{idVendor}=="16c0", ATTRS{idProduct}=="0483", MODE:="0666", RUN:="/bin/stty -F /dev/%k raw -echo"
KERNEL=="hidraw*", ATTRS{idVendor}=="16c0", ATTRS{idProduct}=="0[45]*", MODE:="0666"
SUBSYSTEMS=="usb", ATTRS{idVendor}=="16c0", ATTRS{idProduct}=="04[789B]?", MODE:="0666"
SUBSYSTEMS=="usb", ATTRS{idVendor}=="16c0", ATTRS{idProduct}=="04[789A]?", MODE:="0666"
RULES
        log "Installed Teensy udev rules (inline fallback)"
    fi
fi

#-----------------------------------------------------------------------------
# 2. ESP32 udev rules (CP2102/CH340 USB-serial adapters)
#-----------------------------------------------------------------------------
ESP32_RULES="/etc/udev/rules.d/99-esp32.rules"

if [ -f "$ESP32_RULES" ]; then
    skip "ESP32 udev rules ($ESP32_RULES)"
else
    cat > "$ESP32_RULES" << 'RULES'
# UDEV Rules for ESP32 USB-serial adapters (CP2102, CH340, FTDI)
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", MODE="0666"
SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", MODE="0666"
SUBSYSTEM=="tty", ATTRS{idVendor}=="0403", MODE="0666"
RULES
    log "Installed ESP32 udev rules"
fi

#-----------------------------------------------------------------------------
# 3. Add user to dialout group (serial port access)
#-----------------------------------------------------------------------------
if id -nG "$REAL_USER" | grep -qw dialout; then
    skip "User '$REAL_USER' in dialout group"
else
    usermod -aG dialout "$REAL_USER"
    log "Added '$REAL_USER' to dialout group (re-login required)"
fi

#-----------------------------------------------------------------------------
# 4. Add user to plugdev group (USB device access)
#-----------------------------------------------------------------------------
if id -nG "$REAL_USER" | grep -qw plugdev; then
    skip "User '$REAL_USER' in plugdev group"
else
    if getent group plugdev > /dev/null 2>&1; then
        usermod -aG plugdev "$REAL_USER"
        log "Added '$REAL_USER' to plugdev group"
    else
        skip "plugdev group does not exist on this system"
    fi
fi

#-----------------------------------------------------------------------------
# 5. Reload udev rules
#-----------------------------------------------------------------------------
udevadm control --reload-rules
udevadm trigger
log "Reloaded udev rules"

#-----------------------------------------------------------------------------
# Done
#-----------------------------------------------------------------------------
echo ""
echo "Setup complete. Summary:"
echo "  - Teensy udev rules: $TEENSY_RULES"
echo "  - ESP32 udev rules:  $ESP32_RULES"
echo "  - User groups: $(id -nG $REAL_USER)"
echo ""
echo "If group membership changed, log out and back in for it to take effect."
echo "Unplug and replug any connected boards to apply the new udev rules."
