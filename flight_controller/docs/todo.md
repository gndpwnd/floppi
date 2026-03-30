# Flight Controller Firmware - Todo

> Last updated: 2026-03-30

## In Progress

_Focus: Get everything working on real hardware. Feature development is paused — ~90% of target features are implemented._

> **SERIAL POLICY**: Always use `tools/serial_monitor.py` (Python) or `pio device monitor` (PlatformIO) for serial communication. NEVER use raw bash commands (cat, stty, echo > /dev). Improve the Python scripts as needed. No fc_tool dependency.

### Current Hardware: Teensy 4.0 + MPU6050 + SSD1306 OLED + SBUS Receiver

_Receiver now connected. No ESCs/motors yet._

## Next Session: Hardware Calibration (Teensy + SBUS)

_Goal: Complete all calibrations possible without ESCs. Get config.h fully populated with real hardware values. Verify live build runs stable (no motors, but PID + telemetry should be sane)._

### Phase 0: Pre-flight Prep (before plugging in)

- [ ] Re-enable `USE_SBUS_RECEIVER` in config.h (currently commented out)
- [ ] Build `teensy40_calibration` — verify compiles with SBUS enabled
- [ ] Build `teensy40` (live) — should now pass (was failing without receiver)
- [ ] Verify `build-all` — expect 10/10 with SBUS re-enabled

### Phase 1: Connect & Smoke Test

- [ ] Plug in Teensy, stop ModemManager (`sudo systemctl stop ModemManager`)
- [ ] Flash `teensy40_calibration` — `dev.sh go teensy40_calibration`
- [ ] Verify boot: OLED shows status, serial shows "FLIGHT CONTROLLER READY"
- [ ] Verify OLED displays correct status (idle state) — visual check
- [ ] Run automated test suite — `dev.sh test` — confirm 42/42 still pass
- [ ] Verify SBUS receiver data (`s` command) — confirm CH1-6 respond to transmitter sticks

### Phase 2: Orientation Detection (`o` command)

- [ ] Run orientation detection — **requires physical board manipulation** (3 positions: level, nose-up, right-up)
- [ ] Use `calibrate.sh` interactively or `dev.sh calibrate`
- [ ] Copy generated axis transformation code to config.h / imu.cpp
- [ ] Rebuild and flash — verify corrected readings (AccZ should read ~1.0g when level)

### Phase 3: IMU Calibration (`i` command)

- [ ] Place board level on flat surface
- [ ] Run IMU calibration (`i` → `y` → `y`) — gyro bias + accel offsets
- [ ] Verify quality check passes (stability + level checks)
- [ ] Copy `#define` values to config.h (`IMU_ACC_ERROR_X/Y/Z`, `IMU_GYRO_ERROR_X/Y/Z`)
- [ ] Verify telemetry (`t`) shows: AccZ ≈ 1.0g, GyroX/Y/Z ≈ 0 deg/s, Roll/Pitch ≈ 0°

### Phase 4: Radio Calibration (`r` command)

- [ ] Power on transmitter
- [ ] Run radio calibration (`r`) — move sticks as guided (throttle, roll, pitch, yaw)
- [ ] Verify channel mapping detected correctly
- [ ] Copy `#define` values to config.h (`THROTTLE_CHANNEL`, `ROLL_CHANNEL`, etc.)

### Phase 5: Failsafe Detection (`f` command)

- [ ] Run failsafe detection (`f`) — TX on, read normal values
- [ ] Power off transmitter when prompted — read failsafe values
- [ ] Verify failsafe values are distinct from normal (receiver should output specific values on signal loss)
- [ ] Copy `#define` values to config.h (`FAILSAFE_THROTTLE/ROLL/PITCH/YAW/AUX1/AUX2`)

### Phase 6: Calibration Dump & Apply

- [ ] Run calibration dump (`d`) — get all calibrated values in one block
- [ ] Copy all values to config.h
- [ ] Rebuild `teensy40_calibration` with new values — flash and verify
- [ ] Run telemetry (`t`) — verify sane readings:
  - AccZ ≈ 1.0g, GyroX/Y/Z ≈ 0, Roll/Pitch ≈ 0° when level
  - Channel values respond correctly to transmitter input
  - Motor outputs should be sane-ish (hovering around 1000-1500 with no stick input, not pegged at extremes)

### Phase 7: Live Build Verification

- [ ] Flash `teensy40` (live build, no calibration overhead)
- [ ] Verify clean boot, OLED status, telemetry via serial
- [ ] Verify receiver channels work in live mode
- [ ] Confirm arming/disarming behavior (throttle low + CH5)
- [ ] Note: motor outputs won't drive anything (no ESCs) but values should be reasonable

### After Session: Update Config & Docs

- [ ] Commit config.h with real calibration values
- [ ] Update this todo with results and next steps
- [ ] Update notes section with any new findings

## Up Next (After Calibration Session)

_After all calibrations done, with real config.h values:_

### ESC & Motor Testing (needs ESCs connected)

- [ ] Connect ESCs to motor output pins (no props!)
- [ ] ESC endpoint calibration (`e` command) — full flow
- [ ] Verify motor spin-up responds to throttle
- [ ] Verify motor mixing — tilt board, observe differential motor response

### PID Tuning (needs motors + props)

- [ ] PID tuning on real hardware — use `g` command in calibration mode
- [ ] Start with conservative defaults, iterate
- [ ] First hover test — tethered/constrained flight

## Backlog

_Lower priority, do when time permits_

- [ ] Create example configurations for common VTOL types
- [ ] Implement full 9DOF Madgwick filter for MPU9250
- [ ] Low battery voltage monitoring (ADC)
- [ ] ESP32 test support in test harness
- [ ] Wiring validation on startup (detect if IMU/receiver/OLED are responding)
- [ ] Windows `tools/calibrate.bat` — blocked by serial_monitor.py cross-platform
- [ ] Modular test runner — harness + suites pattern (see roadmap)
- [ ] Auto-flash-and-test — `./test_runner.sh --board teensy40 --suite imu --flash`

## Blocked

_Tasks waiting on something (include reason)_

- ESC endpoint calibration (`e` full flow) — **no ESCs/motors connected**
- Motor/ESC bench test — **no ESCs/motors connected**
- PID tuning — **needs motors + props on a tethered drone**

## Recently Completed

_For context; clear periodically_

- [x] Acro support firmware improvements — 2026-02-20
  - **BUG FIX**: MPU6050 gyro/accel range init — `initialize()` hardcodes 250 DPS/2G, now explicitly set from config.h
  - MAX_RATE defaults increased: roll/pitch 200→500, yaw 160→400 deg/s
  - Quaternion telemetry mode 3: q0-q3 + gyro rates (gimbal-lock-free)
  - Gyro saturation warning + acro quick setup guide in config.h
- [x] Unified dev workflow script (`tools/dev.sh`) — 2026-02-20
- [x] Acrobatics command architecture research — 2026-02-20
- [x] Build verification (all environments) — 7/10 pass, 3 expected fail (SBUS commented out) — 2026-02-20
- [x] Bench test session — 42/42 pass — 2026-02-17
- [x] calibrate.sh, serial_monitor.py improvements, test harness fixes — 2026-02-17

## Notes

- **ResearchHub integration** (2026-03-30): ResearchHub set up for auto-research on flight dynamics topics (quaternions, PID/LQR/MPC, IMU fusion, acrobatics trajectory planning, rotational dynamics, coordinate transforms, safety constraints). Existing 14 findings documents in `docs/findings/` will be ingested into ResearchHub RAG knowledge base. PDF storage at `docs/findings/sources/pdfs/`.
- **Dev workflow**: `tools/dev.sh` is the primary entry point — `dev.sh go` (build+flash+monitor), `dev.sh build`, `dev.sh flash`, `dev.sh monitor`, `dev.sh test`, `dev.sh calibrate`, `dev.sh diagnose`. Dynamically parses platformio.ini.
- **Serial tools**: `tools/calibrate.sh` (menu-driven calibration), `tools/serial_monitor.py` (backend/scripting), `pio device monitor` (fallback). No fc_tool dependency.
- **Teensy quirks**: Stop ModemManager (`sudo systemctl stop ModemManager`). Use `teensy_reboot` for board reset (DTR toggle doesn't reboot Teensy 4.0). USB CDC degrades after ~15 rapid open/close cycles — only `teensy_reboot` or physical unplug recovers.
- **MPU6050 mounting**: AccX≈1.02g, AccY≈0.05g, AccZ≈-0.10g → X-axis points down. Roll≈130° (drifting due to uncalibrated gyro). Needs orientation detection (`o` command) to fix.
- **SBUS receiver**: Now connected. Was commented out in config.h for bench testing without receiver. Re-enable before next session.
- **Gyro bias**: Uncalibrated biases: GX≈-4°/s, GY≈-11°/s, GZ≈-2°/s. Causes attitude drift. Will be zeroed by IMU calibration (`i` command).
- **Motor outputs**: PID outputs pegged at extremes (1000/2000) due to perceived 130° roll. Will normalize after orientation + IMU calibration.
- **Gyro range fix**: MPU6050 init bug fixed 2026-02-20. Gyro and accel now set to config.h values (GYRO_1000DPS, ACCEL_8G) after library init.

---

*Update every session: start by reading, end by updating.*
