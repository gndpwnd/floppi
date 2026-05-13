# Todo: Auto Orientation Framework

**Current phase**: Phase 4 — Auto-orientation + auto-PID + balancing-robot reference
**Last updated**: 2026-05-12

For phase-level context see [roadmap.md](roadmap.md). For framework bounds see [scope.md](scope.md).

---

## Live planning (this session, 2026-05-12 late evening)

Phase 4 implementation + universal auto-tune research and coding session. See [docs/PHASE_4_STRUCTURAL_FIXES.md](PHASE_4_STRUCTURAL_FIXES.md) for the coordinating doc, [docs/archive/session_records/2026-05-12_evening_phase4_landing.md](archive/session_records/2026-05-12_evening_phase4_landing.md) for the full session record.

- [x] Phase A (structural fixes): raw-gyro D-term, real-signal estimator, MsTimer2 ISR, soft-cutoff (items 1-4)
- [x] Phase B (universal auto-tune): scalar RLS plant identifier + closed-form PD-from-K_motor + rate-limited ramp (item 5)
- [x] 5 research agents: inverted-pendulum methods, OSS bot survey, universal zero-knowledge tuning, bootstrap protocol, Osoyoo reference review (all in `findings/`)
- [x] Multi-orientation balance vision + feasibility research (Phase 4.11 designed, not coded)
- [x] All vision docs: UNIVERSAL_BALANCE_BOT_VISION, MINIMIZE_ACCELERATIONS_PHILOSOPHY, MULTI_ORIENTATION_BALANCE_VISION, AUTO_TUNING_REALITY_CHECK
- [x] Build verified: `pio run -e arduino_uno_balancing` at 99.9% flash / 78.4% RAM. 7 PlantIdentifier tests pass.
- [ ] Hardware validation when bot is plugged back in (no bench access this session)
- [ ] Phase 4.10c — full 5-stage bootstrap state machine (designed, deferred until hw drives need)
- [ ] Phase 4.11 — Level 2 multi-orientation (firmware-only arbitrary mounting orientation, ~1 week)

### Prior session — 2026-05-12 framework planning (research only, no code)

### Active research agents (background)

| Agent | Topic | Status |
|-------|-------|--------|
| Online adaptive balance tracking | How to detect & adapt to drift in mounting offset over time (tether, battery, payload) | Running |
| Disturbance compensation | Cable drag, push detection, cascade control, feedforward strategies | Running |
| Tetherless operation strategy | Workflow without USB tether for each MCU class (BLE pendant, on-bot button, WiFi) | Running |
| Browser dashboard architecture | Three.js + LittleFS + WebSocket UI design for ESP32 | Running |

### Completed research agents (findings in `docs/findings/`)

- [x] Self-balancing dynamics + balance-point capture (one-shot)
- [x] Auto-PID tuning algorithm comparison
- [x] BNO055 driver + multi-IMU strategy
- [x] Multi-MCU port strategy (Nano / Mega / Teensy 4.x / ESP32 / ESP32-S3)
- [x] WiFi/telemetry integration design
- [x] MPU6050 + external magnetometer pipeline
- [x] Application catalog (9 applications profiled)
- [x] Test infrastructure expansion (HIL deferred; build-matrix + scenario test prioritized)

---

## Phase 4 — Auto-orientation framework + balancing-robot reference

### 4.1 — Persistent storage HAL (foundational)

- [ ] Create `src/storage/` directory
- [ ] Write `persistent_storage.h` — single API for read/write/commit/clear/capacity
- [ ] Write `persistent_storage_avr.cpp` — wraps `<EEPROM.h>` (existing semantics)
- [ ] Write `persistent_storage_teensy.cpp` — Teensy emulated EEPROM (defer if no hardware yet)
- [ ] Write `persistent_storage_esp32.cpp` — Preferences/NVS (begin + commit)
- [ ] Refactor `src/config/calibration_storage.cpp` to call the HAL (no direct `<EEPROM.h>`)
- [ ] Native unit test: round-trip on AVR backend
- [ ] Document in `docs/implementation/persistent_storage.md`
- [ ] **Fixes Known Issue KI-1**

### 4.2 — Calibration blob sensor tagging

- [ ] Add `CAL_EEPROM_SENSOR_OFFSET` byte to header in `calibration_storage.h`
- [ ] Define sensor IDs: `CAL_SENSOR_BNO085 = 0x85`, `CAL_SENSOR_BNO055 = 0x55`, `CAL_SENSOR_MPU6050_HMC = 0x60`, `CAL_SENSOR_MPU9250 = 0x95`
- [ ] Bump `CAL_FORMAT_VERSION` to `0x02`
- [ ] On `restoreFromEEPROM`, refuse mismatched sensor ID
- [ ] Migration logic: old `0x01` blobs treated as `CAL_SENSOR_BNO085` for backward compatibility
- [ ] Update tests
- [ ] **Fixes Known Issue KI-3**

### 4.3 — Automatic mounting-angle capture (one-shot)

- [ ] Create `src/navigation/mounting_calibration.{h,cpp}`
- [ ] Implement `MountingCalibration` class with `start_capture()`, `is_stable()`, `capture()`, `get_offset_quaternion()`
- [ ] Gyro-stillness detection: 3-sample window, threshold `< 0.5 °/s` on each axis
- [ ] Gravity vector capture: accel low-pass over 200 ms when stable
- [ ] Shortest-arc quaternion computation from observed gravity to `[0, 0, -1]`
- [ ] 24-byte `AutoOrientRecord` serialization (magic + version + q_mount[4] + QC + CRC8)
- [ ] EEPROM persistence via HAL (uses 4.1 + 4.2)
- [ ] Unit tests with synthetic gravity inputs
- [ ] Document in `docs/implementation/mounting_calibration.md`

### 4.4 — Online adaptive mounting-offset tracking (new — from 2026-05-12 user insight)

- [ ] Read `findings/online_adaptive_balance_tracking.md` (forthcoming)
- [ ] Implement chosen algorithm (likely sliding-window mean of pitch-when-stable for Mega; 3-state Kalman extension for Teensy/ESP32)
- [ ] Drift confidence field in `OrientationData` or new `MountingCalibrationStatus` struct
- [ ] Safety: lock adaptation during tip-over / windup; refuse beyond ±5° from one-shot reference
- [ ] EEPROM auto-save policy (every N minutes of stable runtime)
- [ ] Scenario tests for: cable-drag injection, step disturbance, simulated battery discharge curve
- [ ] Document in `docs/implementation/online_balance_adaptation.md`

### 4.5 — Generic auto-PID-tuner

- [ ] Create `src/control/` directory
- [ ] Write `pid_controller.{h,cpp}` — generic single-axis PID (port from `PID_v1` API surface, but our own implementation)
- [ ] Write `auto_pid_tuner.h` — `AutoPIDTuner` class + `ITuningStrategy` virtual base + `TuningResult` POD + `SafetyLimits` struct
- [ ] Write `tuners/relay_feedback.cpp` (`USE_TUNER_RELAY`) — Åström-Hägglund 1984 with amplitude limit
- [ ] Write `tuners/twiddle.cpp` (`USE_TUNER_TWIDDLE`) — coordinate descent, simpler / safer fallback
- [ ] Write `tuners/rls_systemid.cpp` (`USE_TUNER_RLS`) — for known-model plants like drones (Phase 7)
- [ ] Unit tests: simulated plant, verify each strategy converges
- [ ] Document in `docs/implementation/auto_pid_tuner.md`

### 4.6 — BNO055 driver

- [ ] Create `src/sensors/bno055.{h,cpp}` implementing `OrientationSensor`
- [ ] Read quaternion via `getQuat()`; derive Euler through existing `quaternion_conversions.h`
- [ ] Map BNO055's separate `getCalibration(&sys, &accel, &gyro, &mag)` to all four `OrientationData` cal fields (fixes KI-2 on the BNO085 side too)
- [ ] Implement `getCalibrationProfile` / `setCalibrationProfile` for the 22-byte BNO055 blob
- [ ] Add `Adafruit BNO055` to lib_deps in relevant build envs
- [ ] Unit tests with a mocked Adafruit_BNO055
- [ ] Document in `docs/implementation/bno055_driver.md`
- [ ] Hardware test: swap BNO085 → BNO055 on Mega, verify orientation streams correctly

### 4.7 — Self-balancing robot reference application — LANDED 2026-05-12

- [x] Create `src/applications/balancing_robot/` directory
- [x] Add `USE_BALANCING_ROBOT` flag to `src/config/mode.h`
- [x] Write `balance_app.{h,cpp}` — state machine: `IDLE → CAPTURE → TUNE → RUN → (HELD / FALLEN soft-cutoff)`
- [x] Write `safety.{h,cpp}` — tilt limit, motor disarm on tip-over, watchdog
- [x] Write `src/actuators/l298n_motor_driver.{h,cpp}` — generic dual-channel PWM with stiction deadband
- [ ] `src/navigation/balance_kalman.{h,cpp}` — 2-state Kalman (pitch + gyro-bias) — deferred, raw-gyro D-term is the current alternative
- [x] New build envs: `arduino_uno_balancing` + `arduino_mega_balancing` in `platformio.ini`
- [ ] Scenario test: replay `tests/data/balancing_reference_trajectory.csv` — partial; deferred
- [x] Phase 4.7a (state machine), 4.7b (HELD detection), 4.7-soft-cutoff (tip-over auto-recover) all landed

### 4.7c — Multi-axis anomaly detection (designed, not coded)

- [ ] `findings/multi_axis_anomaly_handling_detection.md` — per-axis Welford z-scores with Mahalanobis upgrade path
- [ ] Replaces the current 2-signal HELD detector; ~50 LOC, ~50 B RAM
- [ ] Phase 4.7c work — coding deferred

### 4.10 — Universal zero-knowledge auto-tune — LANDED 2026-05-12

- [x] `findings/dynamic_pwm_accel_learning.md` design (scalar RLS for K_motor)
- [x] `src/control/plant_identifier.{h,cpp}` — RLS + σ-modification projection + MIN_PHI excitation gate
- [x] Closed-form PD-from-K_motor gain mapping (ω_n = 4/ts, ts = 0.5 s, ζ = 0.7)
- [x] Rate-limited gain application (5%/s ramp) inside `BalanceApp::step_run_`
- [x] Freeze gates: 5 s bootstrap window, lateral-gyro > 30 dps, windup_active
- [x] `tests/test_plant_identifier.cpp` — 7 native tests pass, K_est=K_true (0.0% error) on synthetic data
- [x] `s` serial command extended with ADAPT/BOOT tag + K_motor + target gains
- [ ] Phase 4.10c — full 5-stage bootstrap state machine (designed, deferred)
- [ ] Motor-polarity sanity check at adaptation start (designed, deferred)

### 4.11 — Multi-orientation balance (Level 2 — designed, not coded)

- [ ] `MULTI_ORIENTATION_BALANCE_VISION.md` + `findings/research_multi_orientation_balance_feasibility.md` design
- [ ] `src/control/balance_frame.{h,cpp}` — body→balance frame quaternion from boot-time gravity detection
- [ ] BalanceApp consumes `BalanceFrame::tilt_error()` instead of `pitch_deg` directly
- [ ] EEPROM mount blob: add 2-byte wheel-axis field
- [ ] Estimated: ~1 week, firmware-only, no new hardware. Next priority after hw validation of 4.10.

### 4.8 — Tetherless workflow for balancing robot

- [ ] Read `findings/tetherless_operation_strategy.md` (forthcoming)
- [ ] Wire up `src/sensors/button_input.cpp` as the capture trigger
- [ ] Add LED feedback codes (state machine indicator)
- [ ] Optional: piezo buzzer driver for audible state feedback
- [ ] Battery-low detection + safe-shutdown path
- [ ] Document in `docs/applications/balancing_robot/tetherless_workflow.md`

### 4.9 — Phase 4 documentation

- [ ] Per-module implementation notes in `docs/implementation/`
- [ ] User-facing guide in `docs/applications/balancing_robot/USER_GUIDE.md`
- [ ] Hands-off calibration walkthrough with photos (TBD when hardware is set up)
- [ ] Phase 4 completion summary in `docs/phases/PHASE_4_COMPLETION_SUMMARY.md`

---

## Phase 5 — Multi-MCU port (queued)

See [roadmap.md#phase-5](roadmap.md#phase-5--multi-mcu-port). Highlights:

- [ ] Split `src/config/pins.h` into per-platform files
- [ ] New build envs for Nano, Teensy 4.0/4.1, ESP32, ESP32-S3
- [ ] HAL backends for Teensy emulated-EEPROM, ESP32 Preferences
- [ ] MPU6050 + external magnetometer + Madgwick fusion stack
- [ ] Multi-MCU CI matrix (build everything every push, report flash/RAM)
- [ ] Document any cross-platform pitfalls discovered as `docs/findings/`

---

## Phase 6 — WiFi + browser dashboard (queued, ESP32 family only)

See [roadmap.md#phase-6](roadmap.md#phase-6--wifi-telemetry--browser-dashboard-esp32-family-only). Highlights:

- [ ] `src/network/` module tree
- [ ] WiFi STA + mDNS hostname
- [ ] REST + WebSocket API server
- [ ] Three.js dashboard with calibration wizard, balance-capture, PID-tune visualizer, OTA page
- [ ] LittleFS asset pipeline

---

## Phase 7 — Application catalog expansion (queued)

See [roadmap.md#phase-7](roadmap.md#phase-7--application-catalog-expansion). Top 3 per [findings/application_catalog.md](findings/application_catalog.md):

- [ ] **Multirotor bridge** — I2C slave for flight_controller
- [ ] **Photogrammetry snapshot polish** — wire up existing snapshot recorder to a polished app interface
- [ ] **Camera mount / gimbal** — 2-axis first, 3-axis later
- [ ] Educational kit (Nano + MPU6050) — documentation-heavy
- [ ] Robot arm pose feedback (roll/pitch only — yaw mag-derived is too noisy for ±0.1°)

---

## Cross-cutting (no specific phase)

### Tooling

- [ ] `tools/replay_trajectory.py` — feed recorded pitch CSV to firmware over serial (for scenario tests and HIL emulation)
- [ ] `tools/auto_calibrate.py` — host-side magnetometer ellipsoid fit
- [ ] `tools/quaternion_viewer.py` — desktop 3D quaternion viewer (pre-dashboard fallback)
- [ ] `tools/balance_tune_visualizer.py` — auto-PID-tune convergence plot
- [ ] `tools/build_matrix.sh` — wrap `pio run -e <env>` for every env; summarize flash/RAM

### Documentation cross-cutting

- [ ] Update `FOLDER_STRUCTURE.md` to reflect new docs layout + planned src/ additions (storage/, control/, navigation/mounting_calibration, applications/balancing_robot, actuators/, network/)
- [ ] Add per-application `docs/applications/<app>/` folders as applications are added
- [ ] Migrate flat session summaries in `docs/archive/` into `docs/archive/session_records/` over time

### Known issues (live)

| ID | Description | Fix in phase |
|----|-------------|--------------|
| KI-1 | `EEPROM.h` silently fails to persist on ESP32 | 4.1 |
| KI-2 | BNO085 driver collapses 4 cal accuracies to 1 | 4.6 |
| KI-3 | Calibration blob format lacks sensor tag | 4.2 |
| KI-4 | Doc drift in `roadmap.md` / `todo.md` | ✅ done 2026-05-12 |

---

## Recently completed

- [x] **Phase 4.7 balancing-robot reference app** — state machine, motor driver, safety, BNO055 driver, HELD detection, mounting offset capture + persistence, OnlineMountingEstimator, MsTimer2 hardware ISR, soft-cutoff at ±25°. See [archive/session_records/2026-05-12_evening_phase4_landing.md](archive/session_records/2026-05-12_evening_phase4_landing.md) — 2026-05-12
- [x] **Phase 4.10 universal RLS auto-tune** — scalar RLS plant identifier + closed-form PD-from-K_motor + rate-limited ramp. 7 native tests pass. Build at 99.9% flash. See [PHASE_4_STRUCTURAL_FIXES.md](PHASE_4_STRUCTURAL_FIXES.md) item 5 — 2026-05-12
- [x] **Phase 4.5b auto-PID-tuner** — Åström-Hägglund relay feedback, operator-triggered via `t` — 2026-05-12
- [x] Reorganize `docs/` folder (7 new thematic subfolders, INDEX.md everywhere, root re-indexed) — 2026-05-12
- [x] Dissect & archive `SelfBallancingRobot3.ino` — 2026-05-12
- [x] Rewrite `scope.md` as framework vision — 2026-05-12
- [x] Rewrite `roadmap.md` Phase 4-8 — 2026-05-12
- [x] Phase 3 (EKF sensor fusion) — see `docs/phases/PHASE_3_COMPLETION_SUMMARY.md`
- [x] Phase 2 (GPS integration) — see `docs/phases/PHASE_2_COMPLETION_SUMMARY.md`
- [x] Phase 1 (math foundation) — see `docs/phases/PHASE_1_TEST_RESULTS.md`

---

## Notes & assumptions

- **No git commits in the planning session** (user directive 2026-05-12). All work landed in working tree only.
- **No source code modifications yet** — Phase 4 implementation kicks off in a follow-up session once master design doc is reviewed.
- **Session record is a living document** — will be updated as understanding evolves.
- **Active hardware**: Arduino Mega + BNO085 on bench, plugged in via USB. Self-balancing robot with BNO055 sits assembled but unpowered nearby.

---

*Last updated: 2026-05-12. When you finish an item, move it to "Recently completed" with date. When you start a new item, mark it in-progress in `TodoWrite`.*
