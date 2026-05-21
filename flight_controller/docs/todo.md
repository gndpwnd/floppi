# Flight Controller Firmware - Todo

> Last updated: 2026-05-21 (end-of-session reconciliation)

## In Progress

_Focus: Get everything working on real hardware. Feature development is paused — ~90% of target features are implemented._

> **SERIAL POLICY**: Always use `tools/serial_monitor.py` (Python) or `pio device monitor` (PlatformIO) for serial communication. NEVER use raw bash commands (cat, stty, echo > /dev). Improve the Python scripts as needed. No fc_tool dependency.

### Current Hardware: Teensy 4.0 + MPU6050 + SSD1306 OLED + SBUS Receiver

_Receiver now connected. No ESCs/motors yet._

## Next Session: Hardware Validation

_The barometer/GPS/swarm-telemetry workstreams (W2/W4/W5/W6) all closed out 2026-05-21 — see "Completed 2026-05-21" below. There is **no queued static coding work**; everything remaining is hardware-gated._

### Hardware-gated next steps (need bench / ESCs / sensors)

- [ ] **Bench-validate the BMP388 + MS5611 barometer drivers** against real sensors — datasheet-reviewed but not hardware-tested. See [archive/session_records/2026-05-21_multi_agent_sensors_w6_native_tests.md](archive/session_records/2026-05-21_multi_agent_sensors_w6_native_tests.md).
- [ ] **Confirm `'b'` barometer sea-level calibration + swarm telemetry baro/gps blocks end-to-end** against the `swarm_api` server; confirm the `api_version` field once the server adopts it. See [findings/swarm_api_contract_2026-05-20.md](findings/swarm_api_contract_2026-05-20.md).
- [ ] **Motor / ESC test framework** — needs ESCs/motors/rig **and** an ESC-protocol decision. Spec only (unimplemented): `docs/plans/motor-test-framework-plan.md`.
- [ ] General on-hardware testing — calibration phases below, once ESCs/motors return.

### Known low-priority robustness item (not scheduled)

- [ ] `imu.cpp` `Madgwick6DOF()` returns NaN on a mathematically-exact zero-gradient accel input. Never occurs with real (noisy) sensor data, so not a flight risk. A future guard could clamp the gradient-normalisation denominator.

## Future Sessions Backlog

_Don't expand inline — see the planning docs._

- **Session 2 (low-risk research, no hardware)** and **Session 3 (bigger integration scaffolds)** items live in [findings/future_session_scaffolding_2026-05-20.md](findings/future_session_scaffolding_2026-05-20.md) §3 + §4. Highlights: motor/ESC test framework spec, swarm-API contract spec, WiFi failover trace, voltage-monitoring spec, barometer integration spec, GPS passthrough spec.
- **BNO055/BNO085 + calibration HAL port phases for FC v2** — see [/home/devel/floppi/docs/findings/bno_cross_project_2026-05-20.md](/home/devel/floppi/docs/findings/bno_cross_project_2026-05-20.md). Highest-ROI cheap win identified there: `calibration_storage` HAL port.
- **Modular test runner top-level dispatcher + remaining suite stubs** (`test_imu.sh`, `test_radio.sh`, `test_motors.sh`) — harness already exists; see [findings/test_infrastructure_v2_2026-05-20.md](findings/test_infrastructure_v2_2026-05-20.md).
- **ESP32 reset path in test harness** — stubbed today, needs hardware to validate. See §2 #10 in scaffolding doc.
- **3 ESP32 GPIO-conflict `[VERIFY]` flags** — raised by the 2026-05-20 wiring-guide audit; resolve against `pin_definitions_esp32.h` (and on hardware where needed). See [archive/session_records/2026-05-20_recon_builds_and_scaffolding.md](archive/session_records/2026-05-20_recon_builds_and_scaffolding.md).

## Awaiting Operator Input

_5 open questions from [findings/future_session_scaffolding_2026-05-20.md](findings/future_session_scaffolding_2026-05-20.md) §7._

- [ ] **Default IMU for FC v2** — Stay on MPU6050/9250, or transition to BNO055/BNO085 once bno-cross-project research lands? Affects magnetometer roadmap (§3.3) and IMU calibration story.
- [ ] **GPS scope clarification** — scope.md says GPS is flight-computer territory; auto_orientation has `GPS_QUICK_START.md`. Is the intent that FC is a passthrough for GPS data (Core 1 → API relay), or fully out? Affects §3.4.
- [ ] **Hardware availability through Q2 2026** — When will ESCs/motors be on the bench? Gates ESP32 smoke test, motor-test framework execution, and Phase 1-7 hardware calibration below.
- [ ] **swarm_api integration appetite** — Is `swarm_api` going to be actively used this quarter, or is FC's WiFi API mostly dormant? Affects priority of §3.5 contract spec.
- [ ] **ResearchHub readiness** — Is RAG pipeline ready to ingest the 14 findings docs? If not, what's the current blocker?

## When Hardware Returns: Calibration Phases (Teensy + SBUS)

_Goal: Complete all calibrations possible without ESCs. Get config.h fully populated with real hardware values. Verify live build runs stable (no motors, but PID + telemetry should be sane). Phase 0 is already done by 2026-05-20 work — SBUS re-enabled and all 10 envs compile per [findings/project_recon_2026-05-20.md](findings/project_recon_2026-05-20.md)._

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

### Completed 2026-05-21

_Full session record: [archive/session_records/2026-05-21_multi_agent_sensors_w6_native_tests.md](archive/session_records/2026-05-21_multi_agent_sensors_w6_native_tests.md). Wire-level detail: [findings/session_synthesis_2026-05-21.md](findings/session_synthesis_2026-05-21.md). All changes UNCOMMITTED._

- [x] **W6 — swarm telemetry baro/GPS blocks** — outbound `/api/telemetry` POST now carries barometer + GPS data; JSON buffer 384→512 B (`src/api_client.cpp`). Wire contract reconciled in [findings/swarm_api_contract_2026-05-20.md](findings/swarm_api_contract_2026-05-20.md).
- [x] **W4 — barometer field calibration** — `'b'` serial command + `CALIBRATED_BAROMETER` marker + new `lib/Calibration/calibration_baro.{h,cpp}`.
- [x] **BMP388 + MS5611 barometer drivers** — implemented alongside BMP280; selector build-flag-overridable (`-DBAROMETER_BMP388` / `-DBAROMETER_MS5611`). All three datasheet-reviewed.
- [x] **Native host-side test harness** — `tools/build_tests.sh` (glob-discovery) + `tests/native/` (filters/barometer/mixer/attitude), 5/5 green (~110 checks). Pure-math only by operator direction.
- [x] **Build coverage matrix** — `build.sh`/`build.bat` gained a `USE_BAROMETER`+`USE_GPS` × esp32/esp32s3 matrix runner.
- [x] **RC-channel clamp** — `[1000,2000]µs` clamp in `src/web_server.cpp` (P3, defense-in-depth).
- [x] **Teensy parity recon** — ESP32/Teensy split confirmed correct by design; 0 parity work. See [findings/teensy_parity_assessment_2026-05-21.md](findings/teensy_parity_assessment_2026-05-21.md).
- [x] **Doc-drift fixes** — findings INDEX W2/W5 mislabel, `session3_readiness` superseded banner, `0_quickstart` + `README` baro/GPS notes, `pin_definitions_esp32.h` C-1 TODO resolved.

### Completed 2026-05-20

_Full session record: [archive/session_records/2026-05-20_recon_builds_and_scaffolding.md](archive/session_records/2026-05-20_recon_builds_and_scaffolding.md)._

- [x] **PID tuning guide** — `docs/pid-tuning-guide.md` created. Documents the calibration-mode `g` workflow, conservative starting values per VTOL type, the "oscillate → reduce 20%" loop, and re-tune triggers.
- [x] **Wiring-guide fidelity audit (Teensy + ESP32)** — complete; 2 fixes applied + 3 ESP32 GPIO-conflict `[VERIFY]` flags raised against `pin_definitions*.h`. See session record.
- [x] **WiFi onboarding + diagnose decision tree** — `docs/esp32_wifi_onboarding.md` (first-time ESP32 WiFi setup) and `docs/diagnose_decision_tree.md` (symptom → `dev.sh diagnose` → fix) created.
- [x] **BNO055/BNO085 Phase A scaffolding** — `USE_BNO055` / `USE_BNO085` flags + I2C-detect stubs landed (flags OFF by default). See session record.
- [x] **SBUS re-enabled** in `include/config.h:93` (was commented out for bench testing). Restores 10/10 env compile. See [findings/project_recon_2026-05-20.md](findings/project_recon_2026-05-20.md).
- [x] **Test harness modularization** — `tests/test_calibration.sh` (480 lines) split into `tests/lib/harness.sh` (246 lines, shared functions) + `tests/suites/test_calibration.sh` (304 lines, all 18 tests / 42 assertions preserved verbatim). Original entry point preserved as a 21-line `exec` wrapper. ESP32 reset path stubbed in harness (documented, not yet functional). See [findings/test_infrastructure_v2_2026-05-20.md](findings/test_infrastructure_v2_2026-05-20.md).
- [x] **Project recon delivered** — 779-line comprehensive recon at [findings/project_recon_2026-05-20.md](findings/project_recon_2026-05-20.md) — used to plan today's work.
- [x] **Cross-project IMU research** — phased plan for porting auto_orientation's BNO055/BNO085 drivers and `calibration_storage` HAL into flight_controller. Identifies the calibration HAL port as highest-ROI cheap win. See [/home/devel/floppi/docs/findings/bno_cross_project_2026-05-20.md](/home/devel/floppi/docs/findings/bno_cross_project_2026-05-20.md).
- [x] **Future-session scaffolding plan** — 3-session agenda + 5 operator open questions at [findings/future_session_scaffolding_2026-05-20.md](findings/future_session_scaffolding_2026-05-20.md).

### Pre-2026-05-20

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
- **SBUS receiver**: Now connected. Was commented out in config.h for bench testing without receiver; **re-enabled 2026-05-20** in `include/config.h:93`. All 10 build envs now compile.
- **Gyro bias**: Uncalibrated biases: GX≈-4°/s, GY≈-11°/s, GZ≈-2°/s. Causes attitude drift. Will be zeroed by IMU calibration (`i` command).
- **Motor outputs**: PID outputs pegged at extremes (1000/2000) due to perceived 130° roll. Will normalize after orientation + IMU calibration.
- **Gyro range fix**: MPU6050 init bug fixed 2026-02-20. Gyro and accel now set to config.h values (GYRO_1000DPS, ACCEL_8G) after library init.

---

*Update every session: start by reading, end by updating.*
