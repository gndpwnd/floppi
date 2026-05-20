# Roadmap: Auto Orientation Framework

**Current phase**: Phase 4 — Auto-orientation framework + balancing-robot reference application (now bifurcated into 4M Mega-universal and 4U Uno-minimal — see below)
**Last updated**: 2026-05-19 (platform-bifurcation pivot — Mega-only universal stack + Uno-minimal hardcoded program with offline Python brute-force tuner)

This roadmap describes the framework's evolution from the just-completed BNO085 + GPS + EKF stack (Phase 3) toward a multi-MCU, multi-IMU, optionally-WiFi-connected platform with a catalog of reference applications.

For project bounds and rationale, see [scope.md](scope.md).
For current actionable items, see [todo.md](todo.md).

---

## Sequencing discipline

**This project has a recurring failure mode**: skipping ahead to bench-iterate on hardcoded gains/thresholds when the planned phase work is to *eliminate the hardcoded value* in the first place. Every session that produces a "new tuned constant" instead of "a new measurement-driven replacement for a constant" is a session that regressed the universal vision.

The cure is sequencing discipline. When a phase says "implement CHARACTERISE state" and you find yourself instead changing `stiction_min_pwm = 30` to `stiction_min_pwm = 80`, **stop**. The next move is always: *(a)* land the planned measurement infrastructure, *(b)* then validate at the bench. Bench iteration *before* the infrastructure is in produces session-specific patches, not framework progress.

See [scope.md §Process doctrine](scope.md) for the full rule, the [scope.md §Current scope violations — audit](scope.md#current-scope-violations--audit-2026-05-18) for every remaining hardcoded value with its replacement plan, and [archive/LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md](archive/LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md) for the source incidents. Phase entries below are ordered by the right sequence to follow this discipline — *do not skip ahead*.

### Top priority (2026-05-18 PM late — updated 2026-05-19 pivot)

**Strategic pivot landed 2026-05-19: the balance-bot reference application splits into two builds.** The universal/adaptive stack (BOOTSTRAP, RLS, OnlineMountingEstimator, collision detection, position containment) becomes **Mega-only** because it needs wheel encoders and flash headroom that Uno cannot host. The Uno gets a **separate minimal hardcoded program** with PID + PWM constants brute-forced offline via a new Python tuner. New roadmap structure: **Phase 4M** (Mega-universal cleanup) and **Phase 4U** (Uno-minimal + Python tuner) replace what used to be Phase 4.10c+4.11 as the active fronts. See [scope.md §Platform bifurcation](scope.md#platform-bifurcation-2026-05-19--mega-universal-vs-uno-minimal) and the operator-memory note `project_strategic_pivot_2026-05-19.md`.

**Phase 4.10c — BOOTSTRAP state for K_motor identification: LANDED 2026-05-18 PM evening (commit 7a4d27f).** BOOTSTRAP measures K_motor from ±PWM pulses, derives Kp/Kd/Ki via pole-placement, pushes gains to PID before RUN. Hardcoded Kp=50/Ki=2/Kd=20 + `R` command + relay tuner all REMOVED. 7/21 scope violations retired. First bench validation (2026-05-18 PM late) showed bot transitioned IDLE → BOOTSTRAP → RUN with measured K≈0.38 but twitched and fell within ~1 s. The pivot decision came from this outcome: rather than continue debugging BOOTSTRAP on a Uno that's flash-constrained and lacks encoders, BOOTSTRAP work moves to Phase 4M (Mega) where wheel-encoder odometry can validate K_motor directly. See [findings/bootstrap_protocol_unstable_plant.md](findings/bootstrap_protocol_unstable_plant.md), [archive/session_records/2026-05-18_PM_BOOTSTRAP_PHASE_4_10C_LANDED.md](archive/session_records/2026-05-18_PM_BOOTSTRAP_PHASE_4_10C_LANDED.md), and [archive/session_records/2026-05-18_PM_LATE_BOOTSTRAP_FIRST_BENCH.md](archive/session_records/2026-05-18_PM_LATE_BOOTSTRAP_FIRST_BENCH.md).

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

- **Sub-phases done**: 4.1 (persistent storage HAL), 4.2 (cal blob versioning), 4.3 (mounting capture), 4.4 (online estimator), 4.5 (BNO055 driver), 4.5a (PIDController), 4.5b (relay-feedback tuner — DELETED 2026-05-18 PM evening), 4.6 (BNO055 + raw-gyro accessors), 4.7 (balance app), 4.7a (state machine), 4.7b (HELD detection), 4.10 (universal RLS auto-tune), **4.10c (BOOTSTRAP K_motor measurement — LANDED 2026-05-18 PM evening)**, 2.1 (CHARACTERISE measured noise-floor), 2.5 (external-motion HELD), 2.6 (gain scheduling).
- **Designed but not coded**: 4.7c (multi-axis anomaly detector), 4.11 (multi-orientation Level 2), 2.7 (motor-null-space HELD).

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

## Phase 4M — Mega-only universal-stack cleanup

**Goal**: Finish the universal/adaptive balance-bot vision on Mega-class hardware, free from the Uno flash ceiling that derailed the 2026-05-18 bench session. This is the home of BOOTSTRAP, RLS, OnlineMountingEstimator, collision detection, position containment, wheel-encoder odometry, and every future "the bot learns its own dynamics" feature.

**Status**: opened 2026-05-19 by the platform-bifurcation pivot. See `project_strategic_pivot_2026-05-19.md`. The sibling agent's `docs/MEGA_UNIVERSAL_PLAN.md` (forthcoming) holds the detailed plan; this section is the roadmap anchor.

**Working assumption**: Mega's flash budget is generous (~88 % free even with current universal stack). Optimize new code for **clarity**, not size. Don't repeat the Uno-driven micro-optimization patterns that produced unreadable `snprintf`-replacement chains.

### 4M.0 — Restore reverted collision detection

Late in the 2026-05-19 session, a P0/P1 audit-fix agent inadvertently reverted the just-landed collision-detection code in `balance_app.{h,cpp}`. The supporting scaffolding survives — `bno055::getLinearAccel`, the `OrientationSensor::getLinearAccel` virtual in `sensor_base.h`, and the untracked `tests/test_balance_app_collision.cpp`. Re-implement the three-gate detector + state-handler integration cleanly using [findings/research_collision_signature_bno055.md](findings/research_collision_signature_bno055.md) (12 g / 8+3-tick / 6+200 dps thresholds). Apply to BOOTSTRAP, CHARACTERISE, and RUN entry/exit paths.

### 4M.1 — Wheel-encoder hardware abstraction

- New module: `src/sensors/wheel_encoder.{h,cpp}` (or `quadrature_encoder.{h,cpp}`) — generic A/B-phase quadrature interface with platform-specific ISR backends.
- Mega backend uses external-interrupt pins (INT0/INT1/INT4/INT5) for both wheels — Uno cannot host this because it only has two external-interrupt pins, and one is needed for the BNO055 INT line.
- Per-wheel position (ticks), velocity (ticks/s), and direction.
- See [findings/research_wheel_encoders_mega_2026-05-19.md](findings/research_wheel_encoders_mega_2026-05-19.md) (forthcoming, sibling-owned) for hardware recommendations and pin choices.

### 4M.2 — Encoder-driven K_motor verification

- Replace BOOTSTRAP's IMU-only K_motor measurement (Δgyro / ΔPWM) with an encoder-driven version (Δwheel_velocity / ΔPWM) — direct, no plant-coupling noise.
- Keep IMU-only measurement as the fallback when encoders are absent.
- Cross-check: encoder K and IMU K must agree within a tolerance before BOOTSTRAP exits to RUN.

### 4M.3 — Stiction / saturation per-wheel from encoder pulses

- Phase 2 CHARACTERISE upgrade: pulse each wheel independently with ramping PWM; first PWM that produces a non-zero encoder tick = stiction floor; PWM where tick rate stops growing = saturation.
- Per-wheel asymmetry (left vs right) was speculated in [UNIVERSAL_BALANCE_BOT_VISION.md](UNIVERSAL_BALANCE_BOT_VISION.md); encoders make it measurable.

### 4M.11 — Position containment (was Phase 4.11)

The Phase 4.11 position-containment work that was queued for Uno via pitch double-integration now becomes a Mega job with **wheel-encoder odometry preferred** over IMU-only pitch double-integration.

- New module: `src/control/position_loop.{h,cpp}` — cascade outer loop: position error (encoder ticks) → pitch setpoint command → inner balance loop.
- Slow restoring action (~0.5 Hz bandwidth) so the bot drifts back toward the start position without fighting the balance loop.
- Pitch double-integration becomes a documented fallback for IMU-only operation.
- Reference: 2026-05-19 operator observation that the bot wanders during testing and collides with stuff — encoder odometry is the robust fix.

### 4M.k — K-quality + audit-fix follow-through

The P0/P1 audit fixes landed in this session (gyro torn-read atomicity, `plant_id_.reset()` no-overwrite, K-quality gate, baseline cap, BOOTSTRAP per-pulse telemetry) all apply to the Mega-universal code path. Continue retiring the remaining 14 audit rows in [scope.md §Current scope violations — audit](scope.md#current-scope-violations--audit-2026-05-18-updated-pm-evening-phase-410c-landed-re-tagged-2026-05-19-for-platform-bifurcation) — each one becomes addressable once the bot balances long enough on Mega to collect the derivation data.

### Phase 4M success metrics

- Mega build with full universal stack + wheel encoders + collision detection compiles and flashes
- BOOTSTRAP K_motor agrees IMU vs encoder within tolerance
- Bot balances for >60 s without collision-detector false positives
- Position containment keeps the bot within a configurable radius of the start point for >2 min
- Remaining audit rows in scope.md drop below 5

---

## Phase 4U — Uno minimal hardcoded balancer + Python brute-force tuner

**Goal**: A small, single-purpose Uno program that does one thing — balance — using hardcoded PID + PWM constants generated by an offline Python tuner. **No on-MCU learning. No auto-tune. No BOOTSTRAP.** The reference target is `archive/balancing_robot_reference/SelfBallancingRobot3.ino`. When the operator wants to re-tune (new battery, new wheels, new surface), they run the Python tool offline and reflash.

**Status**: opened 2026-05-19 by the platform-bifurcation pivot. Sibling agents own the source scaffold (`src/applications/balancing_robot_uno/`) and the Python tooling (`tools/sim/`).

### 4U.1 — Source scaffold

- New tree: `src/applications/balancing_robot_uno/`
  - `balance_uno_app.{h,cpp}` — read pitch, run PID, drive motors. ~150 LOC ceiling.
  - Consumes a generated header `balance_constants_uno.h` (sibling-owned) with `constexpr` PID + PWM values.
- Reuses existing `src/sensors/bno055.cpp`, `src/actuators/l298n_motor_driver.cpp` — no new sensor or actuator code.
- Compile gate: `USE_BALANCING_ROBOT_UNO` (mutually exclusive with `USE_BALANCING_ROBOT`).

### 4U.2 — Build environment

- New env (or repurpose `uno_balance`): `arduino_uno_minimal` targeting Uno with `USE_BALANCING_ROBOT_UNO` flag.
- Aggressive flash trimming is unnecessary because the universal stack is absent — the program should fit easily under 60 % flash.

### 4U.3 — Python brute-force tuner

- Lives in `auto_orientation/tools/sim/` (sibling-owned this session).
- Wraps existing `balance_bot_sim.py` plant model.
- Grid-search or evolutionary search across PID gain space + PWM scaling space.
- Fitness = time balanced before tip-over under randomized disturbance injection.
- Output: a generated header `balance_constants_uno.h` with `constexpr` Kp/Ki/Kd + stiction_min_pwm + saturation_pwm.

### 4U.4 — Header generator + Uno-side consumer

- Tool emits the header on every tuner run; build script copies it into `src/applications/balancing_robot_uno/generated/`.
- Uno code includes it directly — no JSON, no runtime config, no EEPROM.

### 4U.5 — First brute-force run + bench validation

- Run the Python tuner to convergence on the simulated plant.
- Flash Uno, prop bot upright, release.
- Pass criterion: bot balances ≥ 30 s on a flat indoor surface without operator intervention.

### Phase 4U success metrics

- Uno minimal build at <60 % flash with full balance program
- Python tuner produces stable gains in <10 min of search
- Bot balances ≥ 30 s on first bench try after fresh tune
- Re-tune workflow is documented in `docs/applications/balancing_robot_uno/README.md`

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

*Last updated: 2026-05-19 (doc audit). When a phase enters or exits, update both this file and `todo.md`. Per-session work logs go to `docs/archive/session_records/`.*
