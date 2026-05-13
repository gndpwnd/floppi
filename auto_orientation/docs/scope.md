# Project Scope: Auto Orientation Framework

**Status**: Framework expansion (Phase 3 of original plan complete: BNO085 + GPS + EKF, 143+ tests passing)
**Last updated**: 2026-05-12

> **Design direction**: see [MINIMIZE_ACCELERATIONS_PHILOSOPHY.md](MINIMIZE_ACCELERATIONS_PHILOSOPHY.md) for the project's current direction on the balancing-robot reference application.

---

## Mission

Auto Orientation is an **open-source 3D-orientation framework** for embedded systems. It provides:

- A **portable sensor abstraction** that swaps freely between IMU chips (BNO085, BNO055, MPU6050+external-mag, future ICM-20948 etc.) and microcontrollers (Arduino Nano/Mega, Teensy 4.0/4.1, ESP32, ESP32-S3).
- **Automatic, hands-off calibration**: mounting-angle capture, magnetometer ellipsoid fit, persistent storage — so end users don't hand-tune compile-time offsets.
- **Automatic PID tuning** as a generic single-axis library, with application-specific entry points for pendulums, drones, gimbals, and other control loops.
- **Optional WiFi telemetry** on ESP32/ESP32-S3 builds: live dashboard, browser calibration wizard, OTA updates — mirroring the conventions of the sister `flight_controller/` project.
- **A growing catalog of reference applications** built on the framework, starting with a self-balancing robot and reaching out to drones, gimbals, photogrammetry rigs, and educational kits.

The framework is **not** a flight controller. It is the layer underneath flight controllers, balance bots, gimbals, and any other system that needs to know its orientation accurately and persistently.

---

## Why this is its own project (not a sub-module of `flight_controller/`)

3D orientation is foundational and reusable. Embedding it inside the flight controller would couple every application that needs orientation to the flight controller's release cadence and feature set. By keeping it standalone:

- The sister `flight_controller/` project can depend on a stable orientation interface without owning its calibration UX.
- Educational and budget builds (Nano + MPU6050) can use the same architecture as research-grade builds (Teensy 4.1 + BNO085 + WiFi).
- 3D scanning, gimbal, and AR/VR applications — which have nothing to do with flight — can build on it directly.
- The framework can evolve its own sensor matrix, dashboard, and calibration tooling without affecting downstream consumers.

---

## Current state (2026-05-12)

| Layer | Status | Tests |
|-------|--------|-------|
| Math (quaternion, coordinates, magnetic declination) | Complete | 100+ |
| Sensor abstraction (`OrientationSensor`, `PositionSensor`) | Complete | — |
| BNO085 driver (I2C) | Complete | 26 (incl. extensions) |
| Ublox NEO-M9N GPS driver | Complete | 46 |
| EEPROM calibration persistence | Complete (AVR; **bug on ESP32** — see Known Issues) | — |
| Coordinate frame manager (NED) | Complete | 70 |
| Extended Kalman Filter (16-state, GPS+IMU fusion) | Complete | 70+ |
| JSON output formatter | Complete | 17 |
| SD-card snapshot recorder | Complete | 16 |
| Build envs (8 Arduino Mega variants) | Complete | — |

Original v1.0 milestone (BNO085 + GPS + persistent cal + serial output) is **done**. The framework now expands into Phases 4–7 below.

---

## Framework objectives

### O1 — Sensor portability
Support any IMU that can produce gyro + accel + optional magnetometer, and any persistent-storage primitive (EEPROM, EEPROM-emulated flash, ESP32 NVS, SD card). The `OrientationSensor` base class is the contract; concrete drivers plug in.

### O2 — MCU portability
Support Arduino Nano (budget/edu), Arduino Mega (current), Teensy 4.0/4.1 (high-rate research), ESP32 (WiFi-capable), ESP32-S3 (PSRAM + WiFi). Compile-time platform selection, per-platform pin maps, persistent-storage HAL.

### O3 — Hands-off calibration
A user with no embedded experience should be able to:
1. Power on the device.
2. Rotate it through all orientations (figure-8 magnetometer dance).
3. Hold it at the desired "zero" orientation and press a button (or tap a browser button on WiFi builds).
4. Let it run an auto-PID-tune (for control-loop applications).

…and have the system save everything to persistent storage so the next boot just works.

### O4 — Automatic PID tuning (generic single-axis)
A reusable `AutoPIDTuner` module — algorithm-selectable at compile time (relay feedback for pendulums, in-flight relay with throttle hold for drones, twiddle/coordinate-descent for generic plants). Safety-tripwire matrix per application class.

### O5 — Optional WiFi telemetry & dashboard
On ESP32/ESP32-S3 builds: WiFi STA mode, mDNS hostname, REST + WebSocket API, browser dashboard with Three.js quaternion visualizer, calibration wizard, OTA. Compile-flag cascade matches `flight_controller/` (USE_WIFI → USE_WEB_SERVER + USE_API_SERVER).

### O6 — Application catalog
A growing set of reference applications under `src/applications/`, each compile-gated by `USE_<APP>` so a build only includes what it needs. Initial roster: self-balancing robot, multirotor bridge (I2C slave to flight_controller), camera mount, photogrammetry rig snapshot recorder, AR/VR head tracker, educational kit.

### O7 — Test discipline
Maintain 100% pass rate on unit tests. Add scenario tests for control loops (replay recorded trajectories). Add multi-MCU compile-matrix CI so cross-platform breakage is caught at PR time.

---

## What IS in scope

### Sensors (planned)
- ✅ Adafruit BNO085 (I2C SH-2) — done
- 📋 Adafruit BNO055 (I2C 0x28/0x29) — Phase 4
- 📋 InvenSense MPU6050 + HMC5883L/QMC5883L/LIS3MDL external magnetometer — Phase 4/5
- 📋 InvenSense MPU9250 (one-chip 9-DOF) — Phase 5
- 📋 InvenSense ICM-20948 — Phase 5
- 📋 Ublox NEO-M9N GPS (UART) — done
- 📋 Any UART/I2C GPS via NMEA — done

### Microcontrollers (planned)
- ✅ Arduino Mega — current target
- 📋 Arduino Nano — budget builds, MPU6050-only, no EKF (Phase 5)
- 📋 Teensy 4.0 / 4.1 — high-rate research, FPU-accelerated EKF (Phase 5)
- 📋 ESP32 (WROOM-32) — WiFi telemetry, dual-core split (Phase 6)
- 📋 ESP32-S3 — PSRAM, WiFi, BLE (Phase 6)

### Capabilities (planned)
- ✅ Absolute orientation in NED frame (quaternion primary, Euler/matrix derived)
- ✅ Persistent calibration across power cycles
- ✅ GPS+IMU sensor fusion via EKF
- ✅ Structured serial output (JSON)
- 📋 Automatic mounting-angle capture (shortest-arc quaternion from observed gravity)
- 📋 Magnetometer ellipsoid calibration (hard+soft iron)
- 📋 Generic auto-PID-tuner with multiple strategies
- 📋 WiFi STA + mDNS + REST/WebSocket API + browser dashboard (ESP32+)
- 📋 OTA firmware updates (ESP32+)
- 📋 Hardware-in-the-loop test harness

### Applications (planned reference implementations)
- 📋 Self-balancing robot (`src/applications/balancing_robot/`) — primary Phase 4 target
- 📋 Multirotor bridge (I2C slave for `flight_controller/`) — Phase 7
- 📋 Camera mount / 2- or 3-axis gimbal — Phase 7+
- 📋 Photogrammetry/3D-scanner snapshot recorder — Phase 7+ (snapshot recorder already exists, needs wiring)
- 📋 Educational kit (Nano + MPU6050 + minimal dashboard) — Phase 7+
- 📋 AR/VR head tracker — Phase 8 (latency-critical, ESP32+)
- 📋 Robot-arm end-effector pose — Phase 8
- 📋 Autonomous surface vehicle attitude — Phase 8

See [findings/application_catalog.md](findings/application_catalog.md) for the full per-application requirements analysis.

---

## What is OUT of scope

### Permanently
- ❌ Built-in flight control (use `flight_controller/`)
- ❌ Trajectory planning / autonomy
- ❌ Multi-bot fleet coordination (use `swarm_api/`)
- ❌ Cloud connectivity or public-Internet exposure
- ❌ Proprietary IMU SDK integrations that require closed-source binaries

### Until specified phase
- ❌ Non-Arduino platforms (Raspberry Pi, microPython, etc.) — possible later, not Phase 4-8
- ❌ Real-time 3D-visualization desktop apps — browser dashboard covers this
- ❌ Custom sensor fusion algorithms beyond Madgwick/Mahony/EKF — sufficient for stated applications

---

## Technical decisions & rationale

| Decision | Choice | Why |
|----------|--------|-----|
| Build system | PlatformIO | Reproducible, multi-platform, CI-friendly |
| Sensor contract | `OrientationSensor` base class (virtual) | Same code paths regardless of IMU; runtime polymorphism on chips with vtable budget |
| Primary orientation rep | Quaternion (w,x,y,z) | No gimbal lock; aligns with ROS / standard control conventions |
| Calibration storage | HAL with EEPROM/Flash-emulated/NVS backends | Same API across all MCUs |
| Calibration blob format | Tagged with sensor ID byte + format version | Allows IMU swap without bricking persistence |
| Auto-tune strategy interface | `ITuningStrategy` virtual base | Per-algorithm `.cpp`, compile-selected via `#ifdef` |
| WiFi flag cascade | `USE_WIFI` → `USE_WEB_SERVER` + `USE_API_SERVER` (+ `USE_OTA`) | Mirror `flight_controller/` to keep mental model consistent across projects |
| Dashboard tech | Vanilla HTML/JS + Three.js, stored in LittleFS | No build step, easy to vendor, three.js is the industry default for quaternion viz |
| Application gating | `USE_<APP>` flags + `src/applications/<app>/` | Single binary contains only the application(s) the user enables |
| Library vendoring | Local `lib/` clones (no `lib_deps` cloud fetch in field) | Reliable offline builds |

---

## Constraints & assumptions

### Hardware
- Lowest-spec target: Arduino Nano (2 KB RAM, 32 KB flash, no FPU). On Nano: only MPU6050 + Madgwick, no EKF, no GPS, no SD card. Budget educational build.
- Highest-spec target: ESP32-S3 with PSRAM. All features, WiFi, dual-core.
- I2C bus is the default sensor interface; UART for GPS.
- 3.3V logic; voltage dividers required for 5V receivers/sensors mixed in.

### Environment
- Magnetometer calibration is location-sensitive (local declination matters for true-north heading). The framework persists declination as part of the calibration blob.
- Magnetic interference (motors, speakers, ferromagnetic mounting) requires re-calibration. The dashboard exposes a "re-cal" trigger.
- GPS accuracy: ±1m nominal, ~0.1m achievable with multi-sample averaging while stationary.

### Development
- All libraries cloned locally to `lib/`. No cloud dependency during field deployment or CI.
- All build environments listed in `platformio.ini` must compile cleanly — caught by the multi-MCU CI matrix (Phase 6).

---

## Known issues (active as of 2026-05-12)

### KI-1 — `EEPROM.h` does not persist on ESP32
**Location**: `src/config/calibration_storage.cpp:13` includes `<EEPROM.h>`.
**Problem**: On ESP32, this maps to a deprecated wrapper that requires `EEPROM.begin(size)` + `EEPROM.commit()` after every write — not currently called. Writes appear to succeed but do not survive reboot.
**Impact**: `[env:esp32dev]` builds compile but calibration persistence is broken.
**Fix plan**: Phase 5 — introduce `persistent_storage` HAL with AVR-EEPROM / Teensy-emulated-EEPROM / ESP32-Preferences backends.
**Reference**: [findings/bno055_driver_and_multi_imu_strategy.md](findings/bno055_driver_and_multi_imu_strategy.md).

### KI-2 — BNO085 driver collapses 4 calibration accuracies into 1
**Location**: `src/sensors/bno085.cpp:219-222`.
**Problem**: BNO085 reports separate accuracies for system, accel, gyro, magnetometer. Current code keeps only one composite. BNO055 supports the full 4 via `getCalibration()` — the new driver will be more accurate by default.
**Impact**: User-facing calibration status is less informative than the sensor allows.
**Fix plan**: Phase 4 — update `OrientationData` fields (already has cal_status, cal_accel, cal_gyro, cal_mag fields — wire them up).
**Reference**: [findings/bno055_driver_and_multi_imu_strategy.md](findings/bno055_driver_and_multi_imu_strategy.md).

### KI-3 — Calibration blob format is sensor-specific but EEPROM header is not
**Location**: `src/config/calibration_storage.h` header has no sensor-ID byte.
**Problem**: A 256-byte BNO085 blob and a 22-byte BNO055 blob have nothing in common, but the EEPROM marker is identical (0xCA). Swapping IMUs would attempt to load the wrong blob.
**Fix plan**: Phase 4 — add sensor-ID byte to header; bump `CAL_FORMAT_VERSION`.

### KI-4 — Documentation drift: stale `roadmap.md` and `todo.md`
**Status**: Being addressed in the same session as this scope rewrite (2026-05-12).

---

## Integration points

### Upstream (the framework is used by)
- `flight_controller/` — receives orientation over I2C or UART for swarm/external-sensor use cases (Phase 7)
- `swarm_api/` — already exists at the network layer; auto_orientation devices can expose `/api/status` consumable by the swarm API
- `skytracker_algorithm` (separate repo, future) — camera orientation context for 3D reconstruction
- `engineering360` (separate repo, mentioned in floppi root) — physical drone design uses calibrated IMU mounts

### Downstream (the framework depends on)
- Adafruit BNO08x library (vendored)
- Adafruit BNO055 + Adafruit_Sensor libraries (Phase 4)
- HMC5883L / QMC5883L / LIS3MDL libraries (Phase 5)
- PlatformIO build system
- Standard Arduino core for each MCU family

---

## Success metrics

### Phase 4 (Auto-orientation + balancing robot reference) — by next major work session
- BNO055 driver compiles and matches `OrientationSensor` API
- Auto-mounting-angle capture saves a quaternion to EEPROM and restores on boot
- Generic `AutoPIDTuner` with relay-feedback strategy compiles on Mega
- `src/applications/balancing_robot/` skeleton builds under `arduino_mega_balancing` env
- All existing 143+ tests still pass
- Scenario test using replayed `.ino` trajectory passes

### Phase 5 (Multi-MCU)
- Builds clean on Nano, Mega, Teensy 4.0, Teensy 4.1, ESP32, ESP32-S3
- Persistent-storage HAL passes round-trip test on all platforms
- Flash usage report per platform in CI output
- Madgwick fusion replaces direct sensor read for MPU6050 stack

### Phase 6 (WiFi + dashboard)
- ESP32 build connects to WiFi, reachable via mDNS hostname
- Live orientation visible in browser at 30 Hz
- Calibration wizard end-to-end (no serial cable needed)
- OTA update from browser succeeds

### Phase 7+ (Applications)
- Self-balancing robot stands up and balances after a single hands-off calibration session
- Multirotor bridge passes I2C compliance test with `flight_controller/`
- Educational kit boots, calibrates, and visualizes in <5 min for a new user

---

## Non-goals

- **High-end navigation system**: This is not Pixhawk / ArduPilot replacement. Aim is research-grade portable framework.
- **Sub-millimeter accuracy**: "Good enough for the application" is the bar.
- **General-purpose embedded sensor library**: Specifically a 3D-orientation framework with application catalog. Not a fork-and-rebrand utility kit.
- **Production-grade safety certification**: Hobby and research use. Safety-critical applications must add their own redundancy.

---

## Documentation references

- [roadmap.md](roadmap.md) — Phase-by-phase plan
- [todo.md](todo.md) — Current actionable items
- [INDEX.md](INDEX.md) — Full documentation navigation
- [findings/INDEX.md](findings/INDEX.md) — Research findings driving design decisions
- [archive/balancing_robot_reference/DISSECTION_NOTES.md](archive/balancing_robot_reference/DISSECTION_NOTES.md) — Reverse-engineered .ino reference
- Sister projects: `../flight_controller/docs/scope.md`, `../swarm_api/docs/scope.md`, `../fc_tool/docs/scope.md`

---

*Last updated: 2026-05-12. This document is the source of truth for what the framework is and is not. When in doubt, scope it against this file; if the answer isn't here, raise it in a session and update accordingly.*
