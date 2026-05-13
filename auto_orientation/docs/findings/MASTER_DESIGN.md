# Master Design Synthesis — Auto Orientation Framework

**Status**: Living document, current as of 2026-05-12
**Audience**: implementers picking up Phase 4+ work
**Source**: synthesizes all 12 research findings in this folder

This document is the **single entry point for "what gets built and in what order"** across Phases 4–7 of the framework. It does not repeat the depth of the individual findings — read those for the why. This document captures the **what**: file paths, class names, compile flags, and the ordered task list across phases.

For framework mission, see [../scope.md](../scope.md).
For the phase plan, see [../roadmap.md](../roadmap.md).
For findings detail, see [INDEX.md](INDEX.md).
For the balancing-robot design direction (Phase 4.7), see [../MINIMIZE_ACCELERATIONS_PHILOSOPHY.md](../MINIMIZE_ACCELERATIONS_PHILOSOPHY.md).

---

## Top-level decisions (final, as of 2026-05-12)

| # | Decision | Choice | Source finding |
|---|----------|--------|----------------|
| D1 | Persistent storage strategy | HAL with 3 backends (AVR EEPROM / Teensy emulated EEPROM / ESP32 Preferences); fix KI-1 before any new MCU work | [multi_mcu_port_strategy.md](multi_mcu_port_strategy.md) |
| D2 | Calibration blob format | Tagged header: magic + version + sensor-ID byte + length + CRC8 + payload (size = sensor-specific); blob refuses to load if sensor-ID mismatches | [bno055_driver_and_multi_imu_strategy.md](bno055_driver_and_multi_imu_strategy.md) |
| D3 | IMU swap strategy | Runtime polymorphism on Teensy/ESP32 (vtable cost is free — base class already virtual); compile-time on Nano | [bno055_driver_and_multi_imu_strategy.md](bno055_driver_and_multi_imu_strategy.md) |
| D4 | BNO055 Euler derivation | Read quaternion via `getQuat()`, derive Euler through existing `quaternion_conversions.h`; do NOT use VECTOR_EULER (90° discontinuity) | [bno055_driver_and_multi_imu_strategy.md](bno055_driver_and_multi_imu_strategy.md) |
| D5 | Mounting capture algorithm | Hybrid: gyro-stillness gate + gravity-vector low-pass + button trigger; output is shortest-arc quaternion from observed `g` to `[0,0,-1]` | [balance_point_and_mounting_research.md](balance_point_and_mounting_research.md) |
| D6 | Balance-loop estimator | 2-state Kalman (pitch + gyro-bias). NOT the 16-state EKF — that is for GPS fusion paths, too heavy at 200 Hz | [balance_point_and_mounting_research.md](balance_point_and_mounting_research.md) |
| D7 | Online drift tracking algorithm | Slow LPF of integral term on AVR Mega; 3-state Kalman extension (pitch + gyro-bias + mounting-offset) on Teensy/ESP32 | [online_adaptive_balance_tracking.md](online_adaptive_balance_tracking.md) |
| D8 | Online drift safety bound | Hard refuse to apply >±5° change from one-shot reference; rate-limit to 0.5°/s; freeze adaptation during tipover / windup | [online_adaptive_balance_tracking.md](online_adaptive_balance_tracking.md) |
| D9 | Auto-PID default strategy | Amplitude-limited relay feedback (Åström-Hägglund 1984); twiddle as fallback; RLS for drone (Phase 7) | [auto_pid_tuning_research.md](auto_pid_tuning_research.md) |
| D10 | Tuner strategy interface | `ITuningStrategy` virtual base; per-algo .cpp in `src/control/tuners/`; selected at compile time via `USE_TUNER_RELAY` / `USE_TUNER_TWIDDLE` / `USE_TUNER_RLS` | [auto_pid_tuning_research.md](auto_pid_tuning_research.md) |
| D11 | Disturbance compensation v1 | Push-detect + linear-accel feedforward (BNO085 reports it natively); cascade with wheel encoders deferred to Phase 7 | [disturbance_compensation_research.md](disturbance_compensation_research.md) |
| D12 | MPU6050 + mag fusion | Madgwick (not Mahony); MPU9250 (one-chip) recommended over MPU6050 + HMC5883L for new builds | [mpu6050_external_mag_pipeline.md](mpu6050_external_mag_pipeline.md) |
| D13 | Magnetometer calibration UX | Hybrid: firmware streams raw samples → host Python `tools/auto_calibrate.py` runs Renaudin ellipsoid fit → upload 48 B back | [mpu6050_external_mag_pipeline.md](mpu6050_external_mag_pipeline.md) |
| D14 | WiFi compile-flag cascade | `USE_WIFI` → auto-enables `USE_WEB_SERVER` + `USE_API_SERVER` + `USE_OTA`; mirrors `flight_controller/include/config.h:54-58` exactly | [wifi_telemetry_integration_design.md](wifi_telemetry_integration_design.md) |
| D15 | Dashboard tech | Vanilla HTML/JS + vendored Three.js + LittleFS asset partition. No build step | [browser_dashboard_architecture.md](browser_dashboard_architecture.md) |
| D16 | WebSocket protocol | Tagged-JSON multiplex: `{t: "fused"\|"cal.progress"\|"pid.curve"\|"cmd", ...}` | [browser_dashboard_architecture.md](browser_dashboard_architecture.md) |
| D17 | Tetherless feedback floor | Physical button + LED + piezo buzzer is the minimum kit (works on every MCU). OLED is recommended add-on | [tetherless_operation_strategy.md](tetherless_operation_strategy.md) |
| D18 | First app to ship | Self-balancing robot reference under `src/applications/balancing_robot/` — exercises mounting capture + auto-PID + disturbance comp + tetherless workflow in one application | [application_catalog.md](application_catalog.md) |
| D19 | First scenario test | Replay archived `.ino` pitch trajectory CSV through new code; assert PWM matches within ±5 of legacy output when given the same offset | [test_infrastructure_expansion.md](test_infrastructure_expansion.md) |
| D20 | CI matrix | Local + GitHub Actions multi-MCU compile matrix; reports flash/RAM delta-vs-main per env | [test_infrastructure_expansion.md](test_infrastructure_expansion.md) |

---

## Phase 4 implementation order

This is the canonical order. Each task is small enough to fit in a focused work session.

### 4.1 — Persistent storage HAL (foundational, fixes KI-1)

**Why first**: every Phase 4-6 module that persists data depends on it; the silent ESP32 failure is also blocking that platform.

Files to create:
- `src/storage/persistent_storage.h` — single header API
- `src/storage/persistent_storage_avr.cpp` — wraps `<EEPROM.h>`
- `src/storage/persistent_storage_teensy.cpp` — uses Teensy's emulated EEPROM (or skip until hardware available)
- `src/storage/persistent_storage_esp32.cpp` — uses Preferences/NVS with `begin()` + `commit()`

API (from [multi_mcu_port_strategy.md](multi_mcu_port_strategy.md)):

```cpp
namespace ps {
  bool begin(size_t capacity_hint = 512);
  bool read(size_t offset, uint8_t* buf, size_t len);
  bool write(size_t offset, const uint8_t* buf, size_t len);
  bool commit();
  void clear(size_t offset, size_t len);
  size_t capacity();
}
```

Modify:
- `src/config/calibration_storage.cpp` — replace direct `<EEPROM.h>` with `ps::` calls
- `platformio.ini` — pick the right .cpp via `build_src_filter` per env (one per platform family)

Tests:
- `tests/test_persistent_storage_avr.cpp` — round-trip write/read; capacity boundary; CRC after power-loss simulation
- Defer Teensy/ESP32 tests until hardware is available

### 4.2 — Sensor-tagged calibration blobs (fixes KI-3)

Files to modify:
- `src/config/calibration_storage.h` — add `CAL_EEPROM_SENSOR_OFFSET` byte at 4; payload shifts to offset 5
- Define `CAL_SENSOR_BNO085 = 0x85`, `CAL_SENSOR_BNO055 = 0x55`, `CAL_SENSOR_MPU6050_HMC = 0x60`, `CAL_SENSOR_MPU9250 = 0x95`
- Bump `CAL_FORMAT_VERSION = 0x02`
- `restoreFromEEPROM` refuses mismatched sensor IDs; legacy `0x01` blobs treated as `CAL_SENSOR_BNO085` for backward compatibility

### 4.3 — One-shot mounting capture

Files to create:
- `src/navigation/mounting_calibration.{h,cpp}`

Class `MountingCalibration`:
- `void start_capture()` — enters capture state
- `bool is_stable() const` — gyro-stillness gate: `< 0.5°/s` on each axis for 3 samples
- `bool capture()` — averages accel over 200 ms when stable; returns true if QC passes
- `const Quaternion& get_offset_quaternion() const`
- `bool save_to_eeprom()` / `bool load_from_eeprom()`

Data record (24 bytes; from [balance_point_and_mounting_research.md](balance_point_and_mounting_research.md)):
- `magic` (1 B) = 0xAB
- `version` (1 B) = 0x01
- `q_mount[4]` (16 B) — float32 LE
- `qc_stillness_var` (4 B) — float32
- `qc_capture_age_sec` (2 B) — uint16
- `crc8` (1 B)

Tests:
- Synthetic gravity inputs at known angles → captured quaternion matches expected within 0.1°
- Stillness gate rejects motion samples

### 4.4 — Online adaptive drift tracking (NEW from 2026-05-12 user insight)

Files to create:
- `src/navigation/online_mounting_estimator.{h,cpp}`

AVR Mega path (D7): slow LPF of PID integral term. Update equation:
```
mount_offset_estimate += alpha * (i_term_LP * gain_to_angle)
```
Time constant ~minutes; `alpha` small enough that 1° accumulation takes ~5 min of stable runtime.

Teensy/ESP32 path: extend the 2-state balance Kalman to 3-state by adding mounting offset as a slow-varying state. Q matrix has very small process noise on that state.

Common safety logic:
- Hard refuse |estimate - reference| > 5°
- Rate limit `d/dt mount_estimate` to 0.5°/s
- Freeze adaptation when |i_term| > windup_threshold OR during tip-over recovery
- EEPROM auto-save every N minutes of stable runtime (N=10 to avoid wearing out cells)
- Expose `MountingCalibrationStatus { offset, drift_rate, confidence, last_save_ms }` to API/dashboard

Tests:
- Inject simulated cable drag → estimate converges within 30 s
- Inject step disturbance → estimate does NOT lock in transient
- Long-running drift over simulated battery discharge → tracks slowly without instability

### 4.5 — Generic PID + auto-tuner

Files to create:
- `src/control/pid_controller.{h,cpp}` — clean PID (P + I with anti-windup + D-on-measurement + output clamp). API surface matches `PID_v1` for migration ease.
- `src/control/auto_pid_tuner.h` — coordinator
- `src/control/tuning_strategy.h` — `ITuningStrategy` + `TuningResult` + `SafetyLimits`
- `src/control/tuners/relay_feedback.cpp` (gated by `USE_TUNER_RELAY`)
- `src/control/tuners/twiddle.cpp` (gated by `USE_TUNER_TWIDDLE`)
- `src/control/tuners/rls_systemid.cpp` (gated by `USE_TUNER_RLS`) — Phase 7 trigger

Relay-feedback inputs/outputs (from [auto_pid_tuning_research.md](auto_pid_tuning_research.md)):
- Inputs: setpoint, measurement, amplitude_limit, max_duration_sec, abort_signal
- Outputs: Kp, Ki, Kd via Åström-Hägglund formulas; achieved phase margin estimate; convergence flag

Safety tripwire (per [auto_pid_tuning_research.md](auto_pid_tuning_research.md) §5):
- Pendulum: tilt > 10° → abort
- Drone: tilt > 30° OR altitude drop → abort + land
- Generic: divergence detector + user-button-abort

Tests:
- Simulated plant: relay tuner converges to known reference gains within 30 s
- Tripwire activates correctly under simulated runaway

### 4.6 — BNO055 driver (fixes KI-2 along the way)

Files to create:
- `src/sensors/bno055.{h,cpp}` implementing `OrientationSensor`

Header sketch in [bno055_driver_and_multi_imu_strategy.md](bno055_driver_and_multi_imu_strategy.md).

Key implementation notes:
- Use `bno.getQuat()` → quaternion → existing `quaternion_conversions.h` for Euler
- `getCalibration(&sys, &accel, &gyro, &mag)` populates all 4 `OrientationData` fields
- `getSensorOffsets()`/`setSensorOffsets()` for the 22-byte blob; encode via the new tagged header (D2)
- **While here, fix KI-2 on the BNO085 side**: stop collapsing all 4 accuracies into one (`bno085.cpp:219-222`)

Build env addition:
- `arduino_mega_bno055` — `build_flags = -DUSE_BNO055`; `lib_deps += Adafruit BNO055`

Hardware test (when user swaps in BNO055): orientation streams correctly, calibration persists across reboot, save/restore swap detected.

### 4.7 — Self-balancing robot reference application

Files to create:
- `src/applications/balancing_robot/balance_app.{h,cpp}` — state machine
- `src/applications/balancing_robot/safety.{h,cpp}` — tilt limit, motor disarm, watchdog
- `src/actuators/motor_driver.h` — base interface
- `src/actuators/l298n_motor_driver.{h,cpp}` — concrete L298N implementation (port the stiction deadband + direction logic from the .ino)
- `src/navigation/balance_kalman.{h,cpp}` — 2-state Kalman (Lauszus pattern)

State machine: `IDLE → CAPTURE_MOUNTING → AUTO_TUNE → RUN → SAFE_FALL → IDLE` (loops back on tipover-recovery if tilt comes back within bounds).

Compile gate: `USE_BALANCING_ROBOT` in `src/config/mode.h`.

Build env: `arduino_mega_balancing` with `-DUSE_BALANCING_ROBOT -DUSE_BNO055 -DUSE_TUNER_RELAY` and the L298N pin constants.

### 4.8 — Tetherless workflow

Files to modify/create:
- `src/sensors/button_input.cpp` — remove the SNAPSHOT_MODE compile guard so it's always available; add long-press / short-press distinction
- `src/output/feedback.{h,cpp}` (new) — drives LED state codes + piezo buzzer
- `src/lifecycle/state_machine.{h,cpp}` (new) — top-level FSM coordinating CAPTURE / TUNE / RUN with the application state

State-machine diagram in [tetherless_operation_strategy.md](tetherless_operation_strategy.md) §5.

### 4.9 — Phase 4 wrap-up

- All previous 143+ tests still pass on Mega
- New scenario test `tests/scenario_test_balancing.cpp` passes (PWM within ±5 of legacy `.ino` output given same offset)
- Hardware bring-up: BNO055 + L298N + Mega rolls under PID with tetherless capture and auto-tune
- Write `docs/phases/PHASE_4_COMPLETION_SUMMARY.md`
- Update [../roadmap.md](../roadmap.md): mark Phase 4 complete, move to Phase 5

---

## Phase 5 implementation order (multi-MCU)

Detailed in [multi_mcu_port_strategy.md](multi_mcu_port_strategy.md) and [mpu6050_external_mag_pipeline.md](mpu6050_external_mag_pipeline.md).

Ordered tasks:

1. Pin assignment split — refactor `src/config/pins.h` per platform
2. Teensy 4.0 + 4.1 ports — recompile, run unit tests, hardware bring-up
3. ESP32 + ESP32-S3 ports (no WiFi yet) — recompile, use Preferences/NVS backend
4. Arduino Nano budget build — disable EKF, snapshot, fast GPS, verbose logging
5. MPU6050 + magnetometer + Madgwick stack
6. Build matrix tooling (`tools/build_matrix.sh`) + CI workflow
7. Document migration outcomes in `docs/phases/PHASE_5_COMPLETION_SUMMARY.md`

---

## Phase 6 implementation order (WiFi + dashboard)

Detailed in [wifi_telemetry_integration_design.md](wifi_telemetry_integration_design.md), [browser_dashboard_architecture.md](browser_dashboard_architecture.md).

Ordered tasks:

1. `src/network/wifi_manager.{h,cpp}` — STA mode with AP fallback for first-time WiFi config
2. `src/network/web_server.{h,cpp}` + LittleFS partition + `data/www/` asset tree (vanilla HTML + Three.js + WebSocket client)
3. `src/network/api_server.{h,cpp}` — REST endpoints + WebSocket multiplexed-by-`t` protocol
4. `src/network/ota.{h,cpp}` — ArduinoOTA primary + HTTP-pull fallback
5. Browser dashboard pages: home (3D quaternion viewer), calibrate (mag wizard), balance-capture, pid-tune, telemetry, ota, settings
6. Mobile-friendly UX pass (portrait layouts, large touch targets)
7. Document in `docs/phases/PHASE_6_COMPLETION_SUMMARY.md`

---

## Phase 7 implementation order (applications)

Detailed in [application_catalog.md](application_catalog.md). Priority order from that finding:

1. **Multirotor bridge** — I2C slave for `flight_controller/` (uses flight_controller's existing arbitration patterns: Wire1, address 0x4A, 20-byte packets at 100-200 Hz)
2. **Photogrammetry snapshot polish** — wire up the existing snapshot recorder into a focused application interface (mostly UI/docs, minimal new code)
3. **Camera mount / gimbal** — 2-axis first, 3-axis later; servo or brushless via PWM
4. **Educational kit** (Nano + MPU6050) — documentation-heavy, classroom-ready
5. **Robot arm pose feedback** — roll/pitch only (yaw via mag is too noisy for ±0.1°)
6. **Autonomous surface vehicle** — placeholder; depends on user input

Each gets its own compile flag (`USE_MULTIROTOR_BRIDGE`, `USE_PHOTOGRAMMETRY`, etc.) and own `src/applications/<name>/` tree.

---

## Cross-cutting concerns

### Compile flag inventory

The framework controls everything through compile flags defined in `src/config/mode.h`:

| Flag | Effect |
|------|--------|
| `USE_BNO085` | Enable BNO085 driver |
| `USE_BNO055` | Enable BNO055 driver |
| `USE_MPU6050` | Enable MPU6050 + magnetometer stack |
| `USE_GPS` | Enable GPS driver |
| `USE_EKF` | Enable 16-state EKF (disabled on Nano) |
| `USE_SNAPSHOT_RECORDER` | Enable SD-card snapshot feature |
| `USE_WIFI` | Enable WiFi (ESP32-class only) — auto-cascades |
| `USE_WEB_SERVER` | (auto from `USE_WIFI`) browser dashboard |
| `USE_API_SERVER` | (auto from `USE_WIFI`) REST + WebSocket |
| `USE_OTA` | (auto from `USE_WIFI`) OTA updates |
| `USE_BALANCING_ROBOT` | Enable balancing-robot application |
| `USE_MULTIROTOR_BRIDGE` | Enable multirotor-bridge application |
| `USE_CAMERA_MOUNT` | Enable camera-mount application |
| `USE_PHOTOGRAMMETRY` | Enable photogrammetry application |
| `USE_EDU_KIT` | Enable educational kit |
| `USE_TUNER_RELAY` | Compile in relay-feedback strategy |
| `USE_TUNER_TWIDDLE` | Compile in twiddle strategy |
| `USE_TUNER_RLS` | Compile in RLS strategy |
| `USE_COMMAND_ARBITRATION` | Multi-source command arbitration (planned) |

A build only includes the code its flags select. Default builds (no extra flags) get the v1.0 BNO085+GPS+EKF behavior.

### Known issues being fixed (live tracker — also in [../scope.md](../scope.md))

| ID | Issue | Fix in task |
|----|-------|-------------|
| KI-1 | `<EEPROM.h>` silently fails on ESP32 | 4.1 |
| KI-2 | BNO085 driver collapses 4 cal accuracies to 1 | 4.6 |
| KI-3 | Calibration blob format lacks sensor tag | 4.2 |
| KI-4 | Doc drift in roadmap/todo | ✅ done 2026-05-12 |

### Open questions (need user/hardware confirmation)

1. Which MCU hosts the first auto-PID-tune validation? Default: Mega (matches the .ino reference). Confirm.
2. Wheel encoders for the balance robot v1, or defer to v2? Default: defer (D11).
3. Synthetic scenario CSV or real recording for the first regression test? Default: generate synthetic from a pendulum model in `tools/`, with a TODO to replace with a real recording once hardware is bench-ready.
4. BLE pendant button: where does its driver code live? Proposed: `src/input/ble_pendant.{h,cpp}` (new subtree, not under `network/`).
5. Magnetometer ellipsoid fit: ship `tools/auto_calibrate.py` in Phase 4 (for BNO085 mag re-calibration UX) or wait until MPU6050 stack lands in Phase 5? Default: Phase 5.

---

## Quick navigation

| Goal | Read |
|------|------|
| Framework mission | [../scope.md](../scope.md) |
| Phase plan | [../roadmap.md](../roadmap.md) |
| Active task list | [../todo.md](../todo.md) |
| Why this decision? | The specific finding in [INDEX.md](INDEX.md) |
| Original .ino reference | [../archive/balancing_robot_reference/DISSECTION_NOTES.md](../archive/balancing_robot_reference/DISSECTION_NOTES.md) |
| Session log | [../archive/session_records/2026-05-12_framework-planning.md](../archive/session_records/2026-05-12_framework-planning.md) |
| Full file tree | [../../FOLDER_STRUCTURE.md](../../FOLDER_STRUCTURE.md) |

---

*Last updated: 2026-05-12. This document is the canonical "what gets built and in what order" reference. When a decision changes, update the relevant row in the Top-level decisions table and note the date.*
