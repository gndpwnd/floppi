# Roadmap: Auto Orientation Framework

**Current phase**: Phase 4 — Auto-orientation framework + balancing-robot reference application
**Last updated**: 2026-05-18

This roadmap describes the framework's evolution from the just-completed BNO085 + GPS + EKF stack (Phase 3) toward a multi-MCU, multi-IMU, optionally-WiFi-connected platform with a catalog of reference applications.

For project bounds and rationale, see [scope.md](scope.md).
For current actionable items, see [todo.md](todo.md).

---

## Sequencing discipline

**This project has a recurring failure mode**: skipping ahead to bench-iterate on hardcoded gains/thresholds when the planned phase work is to *eliminate the hardcoded value* in the first place. Every session that produces a "new tuned constant" instead of "a new measurement-driven replacement for a constant" is a session that regressed the universal vision.

The cure is sequencing discipline. When a phase says "implement CHARACTERISE state" and you find yourself instead changing `stiction_min_pwm = 30` to `stiction_min_pwm = 80`, **stop**. The next move is always: *(a)* land the planned measurement infrastructure, *(b)* then validate at the bench. Bench iteration *before* the infrastructure is in produces session-specific patches, not framework progress.

See [scope.md §Process doctrine](scope.md) for the full rule, the [scope.md §Current scope violations — audit](scope.md#current-scope-violations--audit-2026-05-18) for every remaining hardcoded value with its replacement plan, and [archive/LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md](archive/LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md) for the source incidents. Phase entries below are ordered by the right sequence to follow this discipline — *do not skip ahead*.

### Top priority (2026-05-18 PM)

**Phase 4.10c — BOOTSTRAP state for K_motor identification.** This is the single highest-leverage piece of remaining work. Without measured K_motor, the PID gains are guessed, the bot oscillates, and *every other dynamic adaptation in the framework fails to converge because the bot doesn't stay upright long enough to gather data.* See the scope violation audit — 19 of the 19 current hardcoded values become addressable once the bot can stand. Reference design: [findings/bootstrap_protocol_unstable_plant.md](findings/bootstrap_protocol_unstable_plant.md).

---

## Completed phases

### Phase 1 — Math foundation ✅
- Quaternion algebra (multiply, rotate, normalize, conjugate, slerp)
- Coordinate conversions (GPS ↔ ECEF ↔ NED)
- Magnetic declination math
- Snapshot recorder primitives
- **100+ unit tests passing**

### Phase 2 — GPS integration ✅
- Ublox NEO-M9N UART driver
- NMEA + (optional) UBX parsing
- Coordinate frame manager with local NED origin
- JSON output format
- GPS + orientation fusion at the output layer
- **42+ unit tests passing**

### Phase 3 — EKF sensor fusion ✅
- 16-state Extended Kalman Filter (quaternion + velocity + position + accel-bias + gyro-bias)
- State dynamics + Jacobians
- GPS measurement model + Jacobians
- Numerical stability (symmetry, covariance monitoring)
- GPS dropout / dead-reckoning handling
- **143+ tests passing total (cumulative)**

---

## Phase 4 — Auto-orientation + auto-PID + balancing-robot reference

**Goal**: Replace every hand-tuned constant with an automated capture or tuning routine. Deliver a working self-balancing robot reference application that anyone can build and have running after a single hands-off calibration session.

**Status (2026-05-12 late evening)**: **Phase 4.1–4.7 + 4.10 LANDED in firmware**. Builds clean on `arduino_uno_balancing` (99.9% flash, 78.4% RAM, 7 PlantIdentifier tests pass). Hardware validation deferred — bot not currently plugged in.

- **Sub-phases done**: 4.1 (persistent storage HAL), 4.2 (cal blob versioning), 4.3 (mounting capture), 4.4 (online estimator), 4.5 (BNO055 driver), 4.5a (PIDController), 4.5b (relay-feedback tuner), 4.6 (BNO055 + raw-gyro accessors), 4.7 (balance app), 4.7a (state machine), 4.7b (HELD detection), 4.10 (universal RLS auto-tune).
- **Designed but not coded**: 4.7c (multi-axis anomaly detector), 4.10c (full 5-stage bootstrap machine), 4.11 (multi-orientation Level 2).

See: [PHASE_4_STRUCTURAL_FIXES.md](PHASE_4_STRUCTURAL_FIXES.md), [archive/session_records/2026-05-12_evening_phase4_landing.md](archive/session_records/2026-05-12_evening_phase4_landing.md), [UNIVERSAL_BALANCE_BOT_VISION.md](UNIVERSAL_BALANCE_BOT_VISION.md), [MINIMIZE_ACCELERATIONS_PHILOSOPHY.md](MINIMIZE_ACCELERATIONS_PHILOSOPHY.md), [MULTI_ORIENTATION_BALANCE_VISION.md](MULTI_ORIENTATION_BALANCE_VISION.md), [AUTO_TUNING_REALITY_CHECK.md](AUTO_TUNING_REALITY_CHECK.md).

### 4.1 — Persistent storage HAL

Underlying many of the other Phase 4 items: introduce a `persistent_storage` HAL so calibration blobs work the same on AVR, Teensy, and ESP32.

- Header: `src/storage/persistent_storage.h` — `begin / read / write / commit / clear / capacity`
- Backends: `persistent_storage_avr.cpp` (uses `<EEPROM.h>`), `persistent_storage_teensy.cpp` (emulated EEPROM), `persistent_storage_esp32.cpp` (Preferences/NVS)
- Migration: refactor `src/config/calibration_storage.cpp` to call the HAL instead of `<EEPROM.h>` directly. **Fixes Known Issue KI-1**.
- Tests: round-trip read/write per backend (native test for AVR backend; hardware tests for Teensy/ESP32).

See: [findings/multi_mcu_port_strategy.md](findings/multi_mcu_port_strategy.md).

### 4.2 — Calibration blob versioning + sensor tagging

- Add a sensor-ID byte to the EEPROM header (`CAL_EEPROM_SENSOR_OFFSET`).
- Bump `CAL_FORMAT_VERSION` to invalidate any pre-existing blobs.
- On restore, refuse blobs from a different sensor and trigger fresh calibration.
- **Fixes Known Issue KI-3**.

### 4.3 — Automatic mounting-angle capture

- New module: `src/navigation/mounting_calibration.{h,cpp}`.
- Captures the gravity vector from accel (gated by gyro-stillness detection) on user trigger.
- Computes the shortest-arc quaternion from observed gravity to `[0, 0, −1]`.
- Stores as 24-byte `AutoOrientRecord` in EEPROM (magic + version + `q_mount[4]` + QC fields + CRC8).
- Replaces the .ino's manual `PITCH_OFFSET = -8.6` constant.

See: [findings/balance_point_and_mounting_research.md](findings/balance_point_and_mounting_research.md).

### 4.4 — Generic auto-PID-tuner

- New module: `src/control/auto_pid_tuner.{h,cpp}`.
- Strategy interface `ITuningStrategy` with three compile-selectable concrete strategies:
  - `USE_TUNER_RELAY`: amplitude-limited relay feedback (Åström-Hägglund 1984); default for pendulums and pure-PID loops
  - `USE_TUNER_TWIDDLE`: twiddle / coordinate-descent; fallback when relay is unsafe
  - `USE_TUNER_RLS`: recursive least-squares system-ID + analytical PID; for drones / known-model plants
- Reports tuning result (Kp, Ki, Kd, achieved phase margin, settling time) for storage to EEPROM.
- Safety-tripwire matrix per application (tilt limits, output clamps, divergence detector, user-abort).

See: [findings/auto_pid_tuning_research.md](findings/auto_pid_tuning_research.md).

### 4.5 — BNO055 driver

- New module: `src/sensors/bno055.{h,cpp}` implementing `OrientationSensor`.
- Reads quaternion via `getQuat()`, derives Euler through existing `quaternion_conversions.h` (avoids the 90° discontinuity bug in direct VECTOR_EULER).
- Supports `getSensorOffsets()` / `setSensorOffsets()` for the 22-byte BNO055 calibration blob.
- Reports 4 independent accuracies (sys/accel/gyro/mag) — also wire these up for BNO085 (**fixes KI-2**).

See: [findings/bno055_driver_and_multi_imu_strategy.md](findings/bno055_driver_and_multi_imu_strategy.md).

### 4.6 — Self-balancing robot reference application

- New tree: `src/applications/balancing_robot/`.
  - `balance_app.{h,cpp}` — top-level state machine (IDLE → CAPTURE → TUNE → RUN → SAFE_FALL)
  - `safety.{h,cpp}` — tilt limit, motor-disarm-on-tipover, watchdog
- New actuator module: `src/actuators/l298n_motor_driver.{h,cpp}` — generic dual-channel PWM motor driver
- Compile gate: `USE_BALANCING_ROBOT` in `src/config/mode.h`
- 2-state Kalman filter for the balance loop (Lauszus-style: pitch + gyro-bias), not the heavy 16-state EKF (which stays for GPS-fusion paths)
- New build env: `arduino_mega_balancing` with `-DUSE_BALANCING_ROBOT -DUSE_BNO055 -DUSE_COMMAND_ARBITRATION`

> **Design direction for 4.7**: see [MINIMIZE_ACCELERATIONS_PHILOSOPHY.md](MINIMIZE_ACCELERATIONS_PHILOSOPHY.md) — minimize-accelerations framing, `USE_BALANCE_HELD_DETECTION` / `USE_BALANCE_FALL_DETECTION` compile switches, conservative-gain rationale.

### 4.7 — Scenario regression test

- `tests/scenario_test_balancing.cpp` — replays a synthetic pitch trajectory CSV through the new PID + auto-mounting code; asserts motor commands match the archived `.ino` reference within ±5 PWM units when given the same offset.
- Proves architectural equivalence before introducing auto-tune behavior.

### Phase 4 success metrics
- BNO055 driver compiles, swap test (BNO085 → BNO055) works on Mega
- Auto-mounting-angle saves & restores; running on Mega without hand-tuned offset
- AutoPIDTuner relay-feedback strategy compiles + passes unit tests with simulated plant
- `arduino_mega_balancing` env builds clean; scenario test passes
- All previous 143+ tests still pass
- Phase 4 completion summary doc written to `docs/phases/`

---

## Phase 5 — Multi-MCU port

**Goal**: Support Nano, Mega, Teensy 4.0, Teensy 4.1, ESP32, ESP32-S3 from a single source tree. Make any feature optional based on what the target MCU can support.

**Status**: Plan documented; implementation pending Phase 4 completion.

### 5.1 — Pin assignment split

- Refactor `src/config/pins.h` into a dispatcher + per-platform files (mirroring `flight_controller/include/pin_definitions.h`).
- Per-platform: `pins_avr.h`, `pins_teensy.h`, `pins_esp32.h`.
- Critical pins documented per MCU: I2C SDA/SCL (primary + secondary bus where present), Serial1, BNO085 INT/RST, SPI CS for SD card.

### 5.2 — Teensy 4.0 / 4.1 ports

- New envs: `teensy40`, `teensy40_calibration`, `teensy41`, `teensy41_calibration`.
- Use emulated EEPROM via the new HAL.
- Move EKF and Madgwick filter to FPU floats — expected 20-100× lift over Mega soft-float.
- Teensy 4.1 only: SD card via SDIO at full speed.

### 5.3 — Arduino Nano (budget build)

- New envs: `arduino_nano`, `arduino_nano_calibration`.
- Disabled features: EKF (too much RAM), SD card, snapshot recorder, fast GPS, verbose logging.
- MPU6050 + Madgwick only — see Phase 5.5.
- Target: minimum-viable educational kit, classroom-grade.

### 5.4 — ESP32 + ESP32-S3 ports (no WiFi yet)

- New envs: `esp32_dev`, `esp32_dev_cal`, `esp32s3`, `esp32s3_cal`.
- Persistent storage via Preferences/NVS backend of the HAL.
- Dual-core foundation: imu/ekf on core 0, gps/output on core 1 (FreeRTOS queues + `xQueueOverwrite` depth-1 pattern from flight_controller).
- WiFi opt-in is Phase 6.

### 5.5 — MPU6050 + external magnetometer stack

- New modules: `src/sensors/mpu6050.{h,cpp}`, `src/sensors/external_magnetometer.{h,cpp}` (abstract) + `hmc5883l.{h,cpp}`, `qmc5883l.{h,cpp}`, `lis3mdl.{h,cpp}`.
- New fusion adapter: `src/sensors/fused_imu.{h,cpp}` — implements `OrientationSensor` by running Madgwick on raw gyro+accel+mag.
- Magnetometer ellipsoid calibration: capture in firmware, fit on host via `tools/auto_calibrate.py`, upload back.
- WMM-2025 (or coarse city-table) magnetic-declination lookup for true-north heading.

See: [findings/mpu6050_external_mag_pipeline.md](findings/mpu6050_external_mag_pipeline.md) (forthcoming).

### 5.6 — Multi-MCU CI matrix

- `tools/build_matrix.sh` — wraps `pio run -e <env>` for every env, summarizes flash/RAM use.
- GitHub Actions workflow (or local equivalent) — compiles every env on every push.
- Catches "this header doesn't compile on AVR" early.

See: [findings/test_infrastructure_expansion.md](findings/test_infrastructure_expansion.md) (forthcoming).

### Phase 5 success metrics
- Clean compile on all 6 MCU families
- Flash/RAM usage report committed to repo per env
- Persistent-storage HAL round-trip test passes on all backends
- MPU6050 + magnetometer stack passes scenario test against recorded BNO085 reference

---

## Phase 6 — WiFi telemetry + browser dashboard (ESP32 family only)

**Goal**: On ESP32/ESP32-S3 builds, expose a browser dashboard that handles calibration, balance-point capture, auto-PID-tune progress visualization, and OTA — over WiFi, with no serial cable.

**Status**: Designed; implementation pending Phase 5.

### 6.1 — WiFi manager

- `src/network/wifi_manager.{h,cpp}` — STA mode, AP-fallback for first-time setup, mDNS hostname (`autoorient-XXXX.local`)
- `src/network/wifi_credentials.h` — placeholder template, gitignored at runtime
- Compile gate: `USE_WIFI` (auto-cascades from `USE_ESP32` + a build flag, mirroring `flight_controller/`)

### 6.2 — Web + API server

- `src/network/web_server.{h,cpp}` — static HTML/JS/CSS from LittleFS
- `src/network/api_server.{h,cpp}` — REST endpoints + WebSocket stream
- Endpoints aligned with `swarm_api/` contract: `GET /api/status`, `POST /api/commands`, `WS /ws`
- Plus framework-specific: `POST /api/calibration/{start,capture,save}`, `POST /api/pid_tune/{start,abort}`, `GET /api/pid_tune/status`
- Auto-cascades from `USE_WIFI`: `USE_WEB_SERVER`, `USE_API_SERVER`

### 6.3 — Browser dashboard

- Single page, vanilla HTML/JS + Three.js (vendored ~150 KB)
- Pages: home (live orientation 3D), calibrate (magnetometer wizard), balance-capture, pid-tune, telemetry, ota, settings
- Stored in LittleFS partition; uploaded via `pio run --target uploadfs`
- Mobile-friendly layout for hands-on calibration UX

### 6.4 — OTA updates

- `src/network/ota.{h,cpp}` — ArduinoOTA + HTTP-pull fallback
- Compile gate: `USE_OTA` (auto-cascades from `USE_WIFI`)

### Phase 6 success metrics
- ESP32 build joins WiFi and is reachable at mDNS hostname
- Live orientation visible in browser at 30 Hz
- End-to-end calibration via browser (no serial cable)
- OTA update from browser succeeds
- Dashboard works in landscape and portrait on mobile

See: [findings/wifi_telemetry_integration_design.md](findings/wifi_telemetry_integration_design.md), [findings/browser_dashboard_architecture.md](findings/browser_dashboard_architecture.md) (forthcoming).

---

## Phase 7 — Application catalog expansion

**Goal**: Beyond the balancing-robot reference, add reference applications that exercise the framework in different ways. Each lives under `src/applications/<app>/` and is gated by `USE_<APP>`.

**Status**: Designed; ordering will be set when Phase 4-6 work concretizes.

### 7.1 — Multirotor bridge (I2C slave to `flight_controller/`)
- `src/applications/multirotor_bridge/`
- Exposes orientation + cal status to flight_controller over I2C (using flight_controller's existing arbitration patterns)
- Use case: external sensor head when flight_controller's onboard IMU is constrained

### 7.2 — Camera mount / 2- or 3-axis gimbal
- `src/applications/camera_mount/`
- Sub-modules: `gimbal_2axis.cpp`, `gimbal_3axis.cpp`
- Servo or brushless motor outputs; orientation-feedback closed loop
- Zero-axis pointing-direction calibration UX

### 7.3 — Photogrammetry / 3D-scanner snapshot rig
- `src/applications/photogrammetry/`
- Triggers snapshot recorder + GPS capture per button press
- Stores quaternion + position metadata in JSON per image

### 7.4 — Educational kit (Nano + MPU6050)
- `src/applications/edu_kit/`
- Minimum-viable build: pitch/roll on a 4-line OLED or via serial
- Designed for classroom use; emphasis on documentation, not features
- Companion docs: a beginner-friendly walkthrough in `docs/guides/`

See: [findings/application_catalog.md](findings/application_catalog.md) (forthcoming).

---

## Phase 8 — Advanced applications

**Goal**: Applications that stretch the framework in new directions. Not committed; depends on Phase 4-7 outcomes.

- AR/VR head tracker (latency-critical, ESP32-S3 + BLE?)
- Robot-arm end-effector pose feedback
- Autonomous surface vehicle attitude (marine; rugged calibration)
- Advanced VTOL transition tracking (back-feed to flight_controller VTOL work)

---

## Cross-cutting work (parallel to all phases)

### Test infrastructure
- HIL harness for balance-robot (cost/value evaluation in Phase 5)
- Scenario test catalog (one per application + one per known-failure mode)
- Pre-merge checklist doc

### Tooling
- `tools/replay_trajectory.py` — feed recorded CSV to firmware over serial
- `tools/auto_calibrate.py` — host-side magnetometer ellipsoid fit
- `tools/quaternion_viewer.py` — desktop 3D viewer (pre-Phase-6 fallback)
- `tools/balance_tune_visualizer.py` — auto-PID-tune convergence plot
- `tools/build_matrix.sh` — multi-MCU compile wrapper

### Documentation
- Per-application user guides in `docs/guides/`
- Per-finding "what we learned" follow-up notes
- Living session records in `docs/archive/session_records/YYYY-MM-DD_topic.md`

---

## Known issues to track

See [scope.md](scope.md#known-issues-active-as-of-2026-05-12). Currently:

- KI-1: `EEPROM.h` does not persist on ESP32 (fixed by Phase 4.1)
- KI-2: BNO085 driver collapses 4 cal accuracies to 1 (fixed by Phase 4.5)
- KI-3: Calibration blob format lacks sensor tag (fixed by Phase 4.2)
- KI-4: Doc drift in roadmap/todo (addressed in this 2026-05-12 session)

---

## Out of band (research / not committed)

These show up in agent findings but are not yet on a phase plan. Promoted to a phase when the case for them is clear.

- Eigen / `BasicLinearAlgebra` for EKF math on ESP32-S3 / Teensy 4.x
- ICM-20948 / LSM9DS1 driver
- BLE telemetry (lower-latency alternative to WiFi for AR/VR head tracking)
- Online (continuous) magnetometer recalibration
- Magnetic-anomaly detection (warn user when local field is corrupted)
- Multi-rate filter banks (raw IMU at 1 kHz on ESP32-S3 + downsample to 200 Hz for control)

---

*Last updated: 2026-05-12. When a phase enters or exits, update both this file and `todo.md`. Per-session work logs go to `docs/archive/session_records/`.*
