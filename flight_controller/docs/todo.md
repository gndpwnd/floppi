# Flight Controller Firmware - Todo

> Last updated: 2026-02-13

## In Progress

_Focus: Get everything working on real hardware. Feature development is paused — ~90% of target features are implemented._

> **SERIAL POLICY**: Always use `tools/serial_monitor.py` (Python) or `pio device monitor` (PlatformIO) for serial communication. NEVER use raw bash commands (cat, stty, echo > /dev). Improve the Python scripts as needed. No fc_tool dependency.

### Current Hardware: Teensy 4.0 + MPU6050 + SSD1306 OLED + SBUS Receiver

- [ ] Run orientation detection (`o` command) — auto-detect MPU6050 mounting, generate axis transforms. AccZ=-0.08g indicates sideways mount.
- [ ] Complete IMU calibration (`i` command) with orientation correction — verify corrected AccZ ≈ 1.0g
- [ ] Verify radio receiver (`s` command) — confirm all 6 channels respond to transmitter input
- [ ] Radio calibration (`r` command) — full channel mapping with transmitter
- [ ] Verify OLED displays correct status during calibration (armed/idle/calibrating states)

## Up Next

_Priority queue for immediate work_

### Immediate: Validate All Calibration Commands

_Test every calibration command with the hardware we have. Use `serial_monitor.py` for serial._

- [x] `h` — help menu displays all commands
- [x] `c` — calibration status shows all stages
- [x] `i` → `y` → `y` — IMU calibration full flow (level warning, continue, calibrate, quality check, results)
- [x] `o` — orientation detection start + cancel verified (full test needs physical board manipulation)
- [x] `t` — telemetry output streams correctly (IMU ~50Hz, FULL ~20Hz)
- [x] `g` — PID gains display + set (kp_roll 0.2→0.25 round-trip verified)
- [x] `p` — filter/limits display
- [x] `d` — dump outputs all calibration values in config.h format
- [x] `a` — sequential workflow starts, shows stages, offers 6-pos/single-pos choice
- [x] `s` — channel status shows CH1-6 values
- [ ] `r` — radio calibration (BLOCKED: needs transmitter powered on)
- [ ] `f` — failsafe detection (BLOCKED: needs transmitter power-off cycle)
- [ ] `e` — ESC endpoint calibration (BLOCKED: needs ESCs/motors connected)

### Immediate: Calibration Wrapper Script (High Priority)

- [ ] Create `tools/calibrate.sh` — menu-driven wrapper for serial_monitor.py (see [plans/calibrate-sh-plan.md](plans/calibrate-sh-plan.md))
- [ ] Update `docs/2_calibration_guide.md` to reference calibrate.sh as primary method
- [ ] Windows `tools/calibrate.bat` — future, blocked by serial_monitor.py cross-platform

### Future: Test Infrastructure

- [x] Rewrite `test_calibration.sh` to use `serial_monitor.py` instead of fc_tool headless — 2026-02-13
- [ ] Modular test runner — harness + suites pattern (see roadmap)
- [ ] Auto-flash-and-test — `./test_runner.sh --board teensy40 --suite imu --flash`

### Next: Tuning & Flight (blocked by motors/ESCs)

- [ ] Complete motor/ESC bench test — verify PWM output, ESC calibration routine
- [ ] PID tuning on real hardware — use `g` command in calibration mode
- [ ] First hover test — tethered/constrained flight, validate stability

## Backlog

_Lower priority, do when time permits_

- [ ] Create example configurations for common VTOL types
- [ ] Implement full 9DOF Madgwick filter for MPU9250
- [ ] Low battery voltage monitoring (ADC)
- [ ] ESP32 test support in test harness
- [ ] Wiring validation on startup (detect if IMU/receiver/OLED are responding)

## Blocked

_Tasks waiting on something (include reason)_

- Motor/ESC testing — **no motors/ESCs connected**
- Failsafe calibration (`f`) — **needs transmitter power-off cycle**

## Recently Completed

_For context; clear periodically_

- [x] IMU calibration bug fixes from bench testing — 2026-02-12
  - Stability check rewritten: variance-based (was absolute bias, always failed on uncalibrated MPU6050)
  - Gyro quality thresholds relaxed: 2.0 → 15.0°/s (MPU6050 datasheet allows ±20°/s)
  - No-receiver calibration builds: `#error` skipped in CALIBRATION_MODE
  - Quality check now shows actual bias values in output
  - See [findings/calibration-test-results-2026-02-12.md](/docs/findings/calibration-test-results-2026-02-12.md)
- [x] Calibration test suite rewritten to use serial_monitor.py — 2026-02-13
  - 11 test functions (help, status, channels, pid, pid_set, params, dump, telemetry, imu, orientation, sequential)
  - No fc_tool dependency — uses serial_monitor.py backend
- [x] Serial monitor rewritten with raw termios — 2026-02-13
  - Dropped pyserial dependency. Uses raw POSIX termios matching Rust serialport-rs behavior.
  - Works with both Teensy USB CDC and ESP32 USB-UART.
  - 10/13 calibration commands verified (3 blocked by hardware)
- [x] calibrate.sh plan documented — 2026-02-13
  - Plan: [plans/calibrate-sh-plan.md](plans/calibrate-sh-plan.md)
  - Menu-driven wrapper, interactive pass-through, CLI mode
- [x] Calibration library modularization — 2026-02-10
- [x] RadioComm library modularization + command arbitration — 2026-02-11
- [x] All feature tiers implemented (base, optimization, racing) — 2026-02-09
- [x] All calibration routines implemented (IMU, radio, orientation, failsafe, ESC, mag, PID, filters) — 2026-02-09

## Notes

- **Serial tools**: `tools/serial_monitor.py` (primary), `pio device monitor` (fallback), `tools/calibrate.sh` (planned wrapper). No fc_tool dependency.
- **Teensy quirk**: Stop ModemManager first (`sudo systemctl stop ModemManager`), use `teensy_reboot` for board reset
- **MPU6050 mounting**: AccX≈1.02g, AccZ≈-0.08g → X-axis points down. Roll≈128° confirms non-standard mounting.
- **SBUS noise**: Floating serial pin produces random channel values when no receiver connected. Comment out USE_SBUS_RECEIVER for bench testing without receiver.

---

*Update every session: start by reading, end by updating.*
