# Teensy Serial Communication Troubleshooting Guide

Date: 2026-02-12
Hardware: Teensy 4.0, MPU6050, SSD1306 0.91" OLED

## Problem Summary

After initial successful serial connection, the Teensy becomes completely unresponsive to serial reads from any tool (pyserial, fc_tool, minicom, raw stty+cat). No data received, no commands processed.

## Root Causes Identified

### 1. ModemManager Interference (CRITICAL)

**Symptom**: USB serial device connects but no data flows in either direction.

**Cause**: Ubuntu's ModemManager service probes newly-connected USB serial devices with AT modem commands, corrupting the Teensy's USB CDC state.

**Fix**:
```bash
# Stop temporarily
sudo systemctl stop ModemManager

# Disable permanently (recommended for development)
sudo systemctl disable ModemManager

# Or add udev rule to ignore Teensy devices:
# /etc/udev/rules.d/99-teensy.rules
# ATTRS{idVendor}=="16c0", ATTRS{idProduct}=="0483", ENV{ID_MM_DEVICE_IGNORE}="1"
```

**Verification**: `systemctl is-active ModemManager` — should be "inactive"

### 2. Teensy USB CDC State Goes Stale

**Symptom**: Board was previously responsive but now no serial data in either direction. Port exists, USB properly enumerated, but reads return zero bytes.

**Cause**: When a host-side serial connection is killed uncleanly (e.g., `timeout` sends SIGTERM to fc_tool), the Teensy's USB CDC endpoint can enter a stale state. The board is still running but USB serial is non-functional.

**What DOESN'T fix it**:
- Unplugging/replugging USB (power cycles board but Teensy 4.0 doesn't always cleanly re-enumerate)
- USB device reset via sysfs (`unbind`/`bind`)
- Opening the port with different DTR/RTS settings
- Different serial tools (pyserial, minicom, raw cat)

**What DOES fix it**:
```bash
# Use PlatformIO's teensy_reboot tool
~/.platformio/packages/tool-teensy/teensy_reboot

# Then wait 3-5 seconds for re-enumeration
sleep 4
ls /dev/ttyACM*
```

### 3. PySerial Cannot Read Teensy USB CDC

**Symptom**: fc_tool (Rust serialport crate) reads data successfully, but pyserial gets zero bytes on the same port.

**Cause**: Teensy 4.0's USB CDC implementation has specific requirements for DTR/RTS signaling that pyserial doesn't handle correctly on Linux. The Rust `serialport` crate handles this differently at the termios level.

**Workaround**: Use fc_tool headless mode for serial communication. The Python serial_monitor.py in `tools/` has a `--release` flag for port cleanup but should NOT be relied on for Teensy data reads. Use it only for ESP32 or standard Arduino boards.

### 4. Stale Port Holders

**Symptom**: "Device or resource busy" when opening serial port, or silently fails to read.

**Cause**: Previous serial tools didn't close the port cleanly (timeout, crash, etc.).

**Fix**:
```bash
# Check who holds the port
fuser /dev/ttyACM0

# Kill holder
fuser -k /dev/ttyACM0
sleep 1

# Verify port is free
fuser /dev/ttyACM0  # should show nothing
```

### 5. CH6 Auto-Triggers Calibration (No Radio Connected)

**Symptom**: Board boots and immediately starts "IMU Calibration + Orientation Detection" without user input.

**Cause**: With no radio receiver connected, `channel_6_pwm` defaults to 0 or an uninitialized value that may exceed 1800, triggering `CALIB_ATTITUDE` mode via the CH6 hold detection in `checkCalibrationSwitch()`.

**Note**: This is expected behavior — the board is designed to respond to CH6 switch positions. When testing without a radio, be aware that calibration may auto-start.

## Diagnostic Checklist (Run These In Order)

When serial stops working, run these checks before trying to reboot:

```bash
# 1. Is the port present?
ls /dev/ttyACM* /dev/ttyUSB*

# 2. Is the USB device properly enumerated?
lsusb | grep "16c0:0483"
# Expected: "Van Ooijen Technische Informatica Teensyduino Serial"

# 3. Is ModemManager running?
systemctl is-active ModemManager
# If "active" → stop it: sudo systemctl stop ModemManager

# 4. Is something holding the port?
fuser /dev/ttyACM0
# If PIDs shown → kill: fuser -k /dev/ttyACM0; sleep 1

# 5. Try fc_tool (most reliable for Teensy)
timeout 10 fc_tool --headless --port /dev/ttyACM0 --baud 115200

# 6. If still no data → teensy_reboot
~/.platformio/packages/tool-teensy/teensy_reboot
sleep 4
timeout 10 fc_tool --headless --port /dev/ttyACM0 --baud 115200
```

## Tools Reference

| Tool | Teensy Support | ESP32 Support | Notes |
|------|---------------|---------------|-------|
| fc_tool headless | YES | YES | Most reliable for Teensy |
| serial_monitor.py | NO (read fails) | YES | Use for ESP32 only |
| minicom | NO (same issue) | YES | Terminal emulator |
| teensy_reboot | YES | N/A | PlatformIO tool, resets board |
| fuser -k | Both | Both | Release stale port holders |

## Key Paths

- fc_tool binary: `fc_tool/src-tauri/target/release/fc_tool`
- teensy_reboot: `~/.platformio/packages/tool-teensy/teensy_reboot`
- serial_monitor.py: `flight_controller/tools/serial_monitor.py`
- Test script: `flight_controller/tests/test_calibration.sh`
