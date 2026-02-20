# Flight Controller Firmware - Todo

> Last updated: 2026-02-20

## In Progress

_Focus: Get everything working on real hardware. Feature development is paused — ~90% of target features are implemented._

> **SERIAL POLICY**: Always use `tools/serial_monitor.py` (Python) or `pio device monitor` (PlatformIO) for serial communication. NEVER use raw bash commands (cat, stty, echo > /dev). Improve the Python scripts as needed. No fc_tool dependency.

### Current Hardware: Teensy 4.0 + MPU6050 + SSD1306 OLED (no receiver)

- [ ] Run orientation detection (`o` command) — auto-detect MPU6050 mounting, generate axis transforms. AccX≈1.02g confirms X-axis pointing down. **Requires physical board manipulation.** Use `calibrate.sh` interactively.
- [ ] Complete IMU calibration (`i` command) with orientation correction — verify corrected AccZ ≈ 1.0g
- [ ] Verify OLED displays correct status during calibration (armed/idle/calibrating states) — user visual check

### Blocked Until Receiver Available

- [ ] Verify radio receiver (`s` command) — confirm all 6 channels respond to transmitter input
- [ ] Radio calibration (`r` command) — full channel mapping with transmitter
- [ ] Re-enable `USE_SBUS_RECEIVER` in config.h (currently commented out for bench testing)

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
- [x] `p` — filter/limits display + set (b_accel 0.14→0.12 round-trip verified)
- [x] `d` — dump outputs all calibration values in config.h format
- [x] `a` — sequential workflow starts, shows stages, offers 6-pos/single-pos choice
- [x] `s` — channel status shows CH1-6 values
- [x] `n` — network diagnostics (correctly reports ESP32-only on Teensy)
- [x] `m` — 6-position IMU calibration start/cancel verified
- [x] `e` — ESC calibration start/cancel verified
- [x] `f` — failsafe detection start/cancel verified
- [x] `r` — radio calibration start/cancel verified
- [ ] `r` — radio calibration full flow (BLOCKED: needs transmitter powered on)
- [ ] `f` — failsafe detection full flow (BLOCKED: needs transmitter power-off cycle)
- [ ] `e` — ESC endpoint calibration full flow (BLOCKED: needs ESCs/motors connected)

### Test Infrastructure — Completed This Session

- [x] Full test suite: 18 tests, **42/42 checks pass** — 2026-02-17
- [x] Boot drain fix — `reboot_teensy()` now waits for "FLIGHT CONTROLLER READY" before starting tests
- [x] CDC recovery — `run_serial()` auto-recovers from Teensy USB CDC degradation via `teensy_reboot`
- [x] Sequential cancel fix — sends two `n` to fully exit the multi-question workflow
- [x] serial_monitor.py improvements — kernel buffer flush, silence-based drain, `--wait-for`, `--quiet`, exit code 2

### Calibration Wrapper Script

- [x] Create `tools/calibrate.sh` — menu-driven wrapper for serial_monitor.py — 2026-02-17
- [x] Update `docs/2_calibration_guide.md` to reference calibrate.sh as primary method — 2026-02-17
- [ ] Windows `tools/calibrate.bat` — future, blocked by serial_monitor.py cross-platform

### Future: Test Infrastructure

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
- Failsafe calibration (`f` full flow) — **needs transmitter power-off cycle**
- Radio calibration (`r` full flow) — **needs transmitter powered on**
- SBUS re-enable — **commented out in config.h for bench testing without receiver**

## Recently Completed

_For context; clear periodically_

- [x] Unified dev workflow script (`tools/dev.sh`) — 2026-02-20
  - Single CLI: build, flash, monitor, go, test, calibrate, envs, build-all, diagnose
  - Dynamic platformio.ini parsing — no hardcoded environment names
  - Auto port detection, ModemManager check, Teensy CDC recovery
- [x] Acrobatics command architecture research — 2026-02-20
  - See [findings/acrobatics-command-architecture.md](findings/acrobatics-command-architecture.md)
  - Confirmed: rate mode + air mode + serial/I2C commands already supports all acrobatic maneuvers
  - Flight computer sends rate setpoints, FC tracks them
- [x] Acro support firmware improvements — 2026-02-20
  - **BUG FIX**: MPU6050 gyro/accel range init — `initialize()` hardcodes 250 DPS/2G, now explicitly set from config.h
  - MAX_RATE defaults increased: roll/pitch 200→500, yaw 160→400 deg/s
  - Quaternion telemetry mode 3: q0-q3 + gyro rates (gimbal-lock-free)
  - Gyro saturation warning: one-shot diagnostic, warns if >90% range
  - Acro quick setup guide added to config.h (6-step comment block)
- [x] Build verification (all environments) — 2026-02-20
  - 7/10 pass: all calibration envs + all ESP32 envs
  - 3 expected failures: teensy40, teensy41, teensy36 (SBUS commented out, live builds require a command source)
  - Will be 10/10 when receiver is re-enabled
- [x] Bench test session — 2026-02-17
  - See [archive/bench-test-2026-02-17.md](archive/bench-test-2026-02-17.md)
  - OLED, IMU, telemetry, all serial commands verified
  - 42/42 automated test checks pass
  - IMU mounting confirmed (X-axis down, needs orientation detection)
  - SBUS noise issue documented (comment out when no receiver)
  - USB CDC degradation issue found and fixed (auto-recovery in test harness)
- [x] serial_monitor.py improvements — 2026-02-17
  - Kernel buffer flush on connect (`tcflush`)
  - Silence-based drain (replaces fixed 0.5s timer)
  - `--wait-for` pattern matching, `--quiet` flag, exit code 2
- [x] test_calibration.sh fixes — 2026-02-17
  - Boot drain after `reboot_teensy()` (waits for firmware READY)
  - CDC recovery (auto-recovery via `teensy_reboot` on empty output)
  - Sequential cancel sends two `n` for multi-question workflow
  - `|| true` on serial_monitor.py calls (tolerate exit code 2 under `set -euo pipefail`)
- [x] calibrate.sh implemented — 2026-02-17
- [x] waitForConfirmation() dead parameter removed — 2026-02-17
- [x] IMU calibration bug fixes from bench testing — 2026-02-12
- [x] Calibration test suite rewritten to use serial_monitor.py — 2026-02-13
- [x] Serial monitor rewritten with raw termios — 2026-02-13

## Notes

- **Dev workflow**: `tools/dev.sh` is the primary entry point — `dev.sh go` (build+flash+monitor), `dev.sh build`, `dev.sh flash`, `dev.sh monitor`, `dev.sh test`, `dev.sh calibrate`, `dev.sh diagnose`. Dynamically parses platformio.ini.
- **Serial tools**: `tools/calibrate.sh` (menu-driven calibration), `tools/serial_monitor.py` (backend/scripting), `pio device monitor` (fallback). No fc_tool dependency.
- **Teensy quirks**: Stop ModemManager (`sudo systemctl stop ModemManager`). Use `teensy_reboot` for board reset (DTR toggle doesn't reboot Teensy 4.0). USB CDC degrades after ~15 rapid open/close cycles — only `teensy_reboot` or physical unplug recovers.
- **MPU6050 mounting**: AccX≈1.02g, AccY≈0.05g, AccZ≈-0.10g → X-axis points down. Roll≈130° (drifting due to uncalibrated gyro). Needs orientation detection (`o` command) to fix.
- **SBUS noise**: Floating serial pin produces random channel values when no receiver connected. Comment out `USE_SBUS_RECEIVER` in config.h for bench testing without receiver. Currently commented out.
- **Gyro bias**: Uncalibrated biases: GX≈-4°/s, GY≈-11°/s, GZ≈-2°/s. Causes attitude drift. Will be zeroed by IMU calibration (`i` command).
- **Motor outputs**: PID outputs pegged at extremes (1000/2000) due to perceived 130° roll. Will normalize after orientation + IMU calibration.

---

*Update every session: start by reading, end by updating.*
