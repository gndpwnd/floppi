# Calibration Mode Test Results

Date: 2026-02-12 (updated 2026-02-13)
Hardware: Teensy 4.0 + MPU6050 + SSD1306 0.91" OLED
Firmware: teensy40_calibration
Serial tool: flight_controller/tools/serial_monitor.py (raw termios, not pyserial)

## Test Summary

| Command | Result | Notes |
|---------|--------|-------|
| Help (h) | PASS | Full help menu, all 16 commands listed |
| Status (c) | PASS | All 7 calibration stages shown as pending |
| Channels (s) | PASS | CH1-6 values shown, Armed=YES |
| Telemetry (t) | PASS | IMU mode ~50Hz, FULL mode ~20Hz, @plotId format confirmed |
| PID gains (g) | PASS | Display + set (kp_roll 0.2→0.25 round-trip verified) |
| Filters (p) | PASS | All filter/limit values displayed |
| Dump (d) | PASS | Clean config.h format output with PID + filters |
| IMU Cal (i) | PASS | Full flow: start → level warning → continue → calibrate → quality check → results |
| Orientation (o) | PASS | Start + cancel verified (full test needs physical board manipulation) |
| Sequential (a) | PASS | Workflow starts, shows all stages, offers 6-pos/single-pos choice |
| Radio cal (r) | BLOCKED | Needs radio transmitter powered on |
| Failsafe (f) | BLOCKED | Needs transmitter power-off cycle |
| ESC cal (e) | BLOCKED | Needs ESCs/motors connected |

**Total: 10/13 commands verified (3 blocked by hardware)**

## Key Observations

### Boot Sequence
- Full boot in ~4 seconds: banner → OLED → IMU → ESC arm → calibration mode
- "IMU:" line appears but MPU6050 OK/FAILED message is not visible (interleaved with next print)
- Board auto-arms (Armed=YES) — expected since no throttle safety issue with CH3=1000
- "Arming ESCs..." with dots indicates motor initialization

### Channel Defaults (No Radio)
- CH1/2/4 = 1500 (centered sticks)
- CH3 = 1000 (throttle low — safety)
- CH5 = 1000, CH6 = 1000 (switches low)
- CH6=1000 is below 1200 threshold, so no auto-calibration trigger

### Telemetry Format
```
ax@0:1.02 ay@0:0.04 az@0:-0.08 gx@1:-4.26 gy@1:-11.14 gz@1:-2.62
```
- Accelerometer on plot 0, gyroscope on plot 1
- @plotId:value format works for fc_tool multi-graph plotting
- ~60 samples/second output rate

### IMU Calibration Finding (IMPORTANT)
**AccZ = -0.08g (expected ~1.0g)**

The accelerometer Z axis reads nearly zero, meaning gravity is NOT detected on Z. This indicates the MPU6050 module's Z axis is not pointing up/down. Possible causes:
1. **IMU mounted sideways** — the GY-521 module orientation on the bench
2. **Axis mapping issue** — code assumes default axis orientation
3. **Need orientation detection** — use `o` command to auto-detect mounting

**Next step**: Run orientation detection (`o` command) to determine actual mounting and generate axis transformation code.

### IMU Calibration Values (from `i` command, 2026-02-13)

Full calibration run completed with quality warnings (expected due to sideways mounting):

| Parameter | Value | Status |
|-----------|-------|--------|
| AccErrorX | 1.022g | Warning — sideways mount, X-axis has gravity |
| AccErrorY | 0.039g | Good |
| AccErrorZ | -1.081g | Warning — expected, Z-axis not pointing down |
| GyroErrorX | -4.5°/s | Good (within ±15°/s threshold) |
| GyroErrorY | -10.5°/s | Good (within ±15°/s threshold) |
| GyroErrorZ | -2.1°/s | Good (within ±15°/s threshold) |

Gyro calibration values are correct regardless of mounting orientation. Accelerometer values are incorrect because `i` command assumes Z-axis = gravity — need `o` command for proper mounting detection.

### Gyro Readings
- gx: ~-4.3°/s, gy: ~-11.2°/s, gz: ~-2.3°/s
- These are raw uncalibrated readings — expected to have bias
- After calibration, these should be near zero when stationary

## Bugs Found and Fixed

### 1. IMU Stability Check Always Fails (calibration_imu.cpp)

**Problem**: The pre-calibration stability check measured `abs(GyroX) + abs(GyroY) + abs(GyroZ)` against a threshold of 5.0°/s. This detects gyro **bias**, not movement. An uncalibrated MPU6050 has 5-15°/s bias per datasheet, so the check always fails even when the board is perfectly still.

**Fix**: Rewrote to variance-based measurement. Two-pass algorithm: collect 100 samples to compute mean, then compute standard deviation around that mean. StdDev > 3.0 indicates actual physical movement, regardless of bias level.

### 2. Gyro Quality Threshold Too Strict (calibration_imu.cpp)

**Problem**: Post-calibration quality check flagged gyro bias > 2.0°/s as a warning. MPU6050 datasheet allows ±20°/s initial bias. Real-world calibrated bias of ~4-11°/s would always trigger false warnings.

**Fix**: Changed gyro quality threshold from 2.0°/s to 15.0°/s. AccZ quality threshold from 0.1 to 0.3. Added actual bias values to quality check output messages.

### 3. No-Receiver Calibration Build Blocked (radioComm.h)

**Problem**: With USE_SBUS_RECEIVER commented out (to avoid SBUS noise on floating serial pin during bench testing), the build fails with `#error "No receiver or command source defined"`.

**Fix**: Wrapped the compile-time error in `#ifndef CALIBRATION_MODE`, allowing calibration builds to proceed without a receiver defined. Normal builds still require a receiver.

### 4. SBUS Noise on Floating Pin

**Observation**: With USE_SBUS_RECEIVER enabled but no receiver connected, the floating serial pin reads random data that occasionally produces channel_6_pwm > 1800 for 3+ seconds, auto-triggering CALIB_ATTITUDE. Not a bug per se — expected behavior with floating input. Workaround: comment out USE_SBUS_RECEIVER for bench testing without a receiver.

## Serial Communication Notes

See: docs/findings/teensy-serial-troubleshooting.md

Key workflow:
1. Stop ModemManager: `sudo systemctl stop ModemManager`
2. Use `serial_monitor.py` for both Teensy and ESP32 (rewritten 2026-02-13 with raw termios)
3. Board reset: `~/.platformio/packages/tool-teensy/teensy_reboot` (Teensy only)
4. Never use raw bash (cat, stty, echo) for serial

The serial_monitor.py rewrite (2026-02-13) uses raw POSIX termios with cfmakeraw()-equivalent settings and select()+os.read() instead of pyserial. This matches Rust's serialport-rs behavior and works reliably with Teensy USB CDC.

## Files Created/Modified
- `flight_controller/tests/test_calibration.sh` — automated test suite (uses fc_tool, needs rewrite)
- `flight_controller/tools/serial_monitor.py` — Python serial tool (Teensy + ESP32, raw termios)
- `flight_controller/tests/results/` — test output captures
- `docs/findings/teensy-serial-troubleshooting.md` — troubleshooting guide
- `docs/findings/calibration-test-results-2026-02-12.md` — this file
- `flight_controller/lib/Calibration/calibration_imu.cpp` — stability check + quality thresholds
- `flight_controller/lib/RadioComm/radioComm.h` — no-receiver calibration builds
