# Bench Test Session — 2026-02-17

## Hardware

- Teensy 4.0 + MPU6050 (GY-521) + DSD Tech 0.91" OLED (SSD1306 128x32)
- No RC receiver connected
- USB serial at /dev/ttyACM0
- Firmware: `teensy40_calibration`

## Test Summary

| Check | Result | Notes |
|-------|--------|-------|
| OLED I2C detection | PASS | Found at 0x3C, initialized OK |
| OLED display type | PASS | SSD1306 128x32 matches config.h |
| IMU initialization | PASS | MPU6050 detected, warm-up takes ~5-10s |
| ESC arming | PASS | "ESCs armed" on boot |
| Calibration mode | PASS | Full menu, all 15 commands listed |
| Serial commands | PASS | All 18 test functions pass (42/42 checks) |
| Telemetry streaming | PASS | IMU 50Hz, FULL 20Hz, `@plotId:value` format |

## IMU Readings (stationary on desk)

| Axis | Accel (g) | Gyro (deg/s) | Expected (level) |
|------|-----------|--------------|-------------------|
| X | **1.02** | -4.0 | 0.00 |
| Y | 0.05 | -11.1 | 0.00 |
| Z | -0.10 | -2.1 | **1.00** |

**Analysis**: Gravity vector is on X-axis (1.02g) instead of Z-axis (-0.10g). The MPU6050 is mounted ~90 degrees from standard orientation (X-axis pointing down). Madgwick filter computes roll ~130 degrees, gyro drift causes continuous attitude drift.

**Fix**: Run orientation detection (`o` command) to auto-detect the mounting and generate axis transformations.

## Issues Found

### 1. SBUS Noise on Floating Pin

**Problem**: With no SBUS receiver connected, the SBUS serial pin floats and produces random channel values. When CH6 randomly hits HIGH, it auto-triggers the orientation detection calibration.

**Fix**: Comment out `USE_SBUS_RECEIVER` in config.h when bench testing without a receiver. This is already documented in todo.md.

**Config change**: `#define USE_SBUS_RECEIVER` → `//#define USE_SBUS_RECEIVER` for this session.

### 2. USB CDC State Degradation

**Problem**: After ~15-17 rapid serial open/close cycles (from the test suite), the Teensy USB CDC stops responding. New connections get zero bytes. DTR toggle does NOT fix this — only `teensy_reboot` or physical unplug restores the CDC.

**Root cause**: Teensy 4.0 USB CDC doesn't implement DTR-based reset. The kernel's CDC state accumulates stale data across rapid open/close cycles until it becomes non-functional.

**Fix**: Added CDC recovery to `run_serial()` in test_calibration.sh: if zero bytes captured, trigger `teensy_reboot`, wait for boot, and retry. All 42 tests now pass.

### 3. Test Suite Boot Timing

**Problem**: The `all` test run calls `reboot_teensy()` then immediately starts the first test. But IMU warm-up takes 5-10 seconds, so the first test captures boot messages instead of command responses.

**Fix**: Added boot drain step after `reboot_teensy()` that waits for "CALIBRATION MODE" or "FLIGHT CONTROLLER READY" before starting tests.

### 4. Sequential Test Cancel Incomplete

**Problem**: The sequential workflow (`a`) asks two questions: "6-position?" then "single-position?". Test only sent one `n`, leaving the firmware waiting for input.

**Fix**: Changed `run_serial "$outfile" 10 "a" "n"` to `run_serial "$outfile" 10 "a" "n" "n"`.

## Serial Monitor Improvements

Updated `tools/serial_monitor.py` with:
- **Kernel buffer flush on connect**: `tcflush(TCIOFLUSH)` clears stale data
- **Silence-based drain**: After last command, reads until quiet (0.5s silence) instead of fixed 0.5s timer
- **Exit code 2** for empty output (helps test scripts detect failures)
- **`--wait-for` flag**: Wait until output matches a regex pattern (useful for boot detection)
- **`--quiet` flag**: Suppress stdout while still saving to `--output` file

## Motor Outputs

With uncalibrated IMU (perceived 130 degree roll), PID outputs are pegged:
- m1=1000, m2=2000, m3=2000, m4=1000

This is correct PID behavior — it's trying to correct a massive perceived tilt. Will normalize after orientation detection + IMU calibration.

## Next Steps

1. **Orientation detection (`o`)** — requires physically repositioning the board in 3 orientations. Must be done interactively via `calibrate.sh`.
2. **IMU calibration (`i`)** — after orientation is set, calibrate gyro biases and accel offsets.
3. **Re-enable SBUS** — uncomment `USE_SBUS_RECEIVER` when receiver is available.
4. **OLED verification** — user should visually confirm OLED shows correct states during calibration.

## Files Changed

- `tools/serial_monitor.py` — output capture fixes, new flags
- `tests/test_calibration.sh` — boot drain, CDC recovery, sequential cancel fix
- `include/config.h` — SBUS commented out for bench testing
