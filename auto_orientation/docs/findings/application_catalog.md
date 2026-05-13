# Application Catalog — `auto_orientation` Framework

**Date**: 2026-05-12
**Status**: Planning input — feeds the scope rewrite and roadmap reshuffle
**Scope**: Enumerate downstream applications so we can prioritize framework features (orientation rate, fusion accuracy, persistence, telemetry, calibration UX, latency, mounting flexibility) instead of building speculatively.

---

## Recommendation Summary

- **Build first** — **(1) Balancing robot**, **(2) Flight-controller I2C bridge**, **(3) Snapshot/photogrammetry rig**. Together they exercise persistence, mounting calibration, high-rate quaternion output, GPS+IMU fusion, and the serial/I2C/WiFi transports — ~80 % of the framework's intended surface.
- **Build second** — **(4) Camera/gimbal stabilization** and **(5) Educational Nano+MPU6050 kit**. They reuse the balancing-robot control loop and the multi-IMU abstraction respectively, so marginal cost is small.
- **Defer** — VTOL transition, AR/VR head tracking, robot-arm tool pose, autonomous boat. Each introduces one hard new constraint (sub-frame latency, marine ruggedization, ±0.1° industrial accuracy, transition-point angle wrapping) that doesn't pay back until the framework is mature.

---

## 1. Inverted Pendulum / Self-Balancing Robot (PRIMARY)

The archived `auto_orientation/SelfBallancingRobot3.ino` is the working reference: BNO055 + L298N + PID at ~200 Hz with hand-tuned `PITCH_OFFSET = -8.6°` and hand-tuned gains. The framework replaces those constants with **auto-captured mounting angle** (one-shot at rest) and **relay-feedback auto-tune**, keeping the same loop discipline.

**State**: `pitch_deg`, `pitch_rate_dps`, optionally `wheel_velocity`/`position` for outer loops. Yaw not needed for balance.
**Latency**: IMU-to-PWM under 5 ms. At 200 Hz the loop period *is* 5 ms — one tick of jitter is the entire budget. Blocking serial, slow I2C, magnetometer reports are forbidden in the hot path.
**Accuracy**: ±0.5° pitch. Worse and the bot oscillates: D-term can't separate real lean from noise.
**Calibration**: one-shot mounting capture; relay test logs oscillation period, applies Ziegler-Nichols, writes Kp/Ki/Kd to EEPROM next to the BNO calibration profile.

| Metric | Value |
|--------|-------|
| Priority | High |
| Orientation rate | 200 Hz |
| Accuracy | ±0.5° pitch |
| Persistence | Yes (mounting + PID + mag cal) |
| Telemetry | Serial (debug) + optional WiFi |
| Calibration UX | One-shot capture + one-shot auto-tune |
| MCU floor | Mega (ref); Teensy 4.0 preferred |
| Implementation order | 1 |
| Effort | 4–6 sessions |

---

## 2. Multirotor Flight-Controller Bridge (HIGH VALUE)

`flight_controller/` already runs Madgwick 6DOF on MPU6050 at 1–2 kHz. So when does it want `auto_orientation`? Two clean answers:

1. **Pre-flight calibration host** — auto_orientation runs slow stuff (mag figure-8, mounting-axis detection, declination lookup), prints `#define` values, user pastes into FC `config.h`. Workflow integration, no runtime wire.
2. **External absolute-orientation sensor head** — auto_orientation streams BNO085 quaternions to the FC as an outer-loop heading reference that bounds Madgwick drift. The FC's MPU6050 stays in the inner loop (latency).

Transport for (2) reuses the FC's existing external-source pattern. The FC is already an I2C slave on Wire1 at 0x42; mirror it: **auto_orientation as I2C slave at 0x4A, FC polls 100–200 Hz for a 20-byte packet** (4 quaternion floats + cal status + timestamp). 500–1000 Hz over I2C is both unnecessary (BNO085 only updates at 400 Hz) and would saturate Wire1.

| Metric | Value |
|--------|-------|
| Priority | High |
| Orientation rate | 100–200 Hz |
| Accuracy | ±1° (drift-bounded by mag) |
| Persistence | Yes (mag cal + airframe mounting) |
| Telemetry | I2C primary, UART backup |
| Calibration UX | Per-deploy (on actual airframe) |
| MCU floor | Mega works; Teensy/ESP32 lets it co-locate |
| Implementation order | 2 |
| Effort | 3–5 sessions |

---

## 3. Camera Mount / Gimbal Stabilization

Two-axis gimbals (roll+pitch) need only accelerometer-half fusion — trivial. Three-axis gimbals need a calibrated magnetometer to lock yaw; without it the camera slowly pans off-target. That's where the framework's mag-cal persistence directly buys the user something a Nano sketch can't easily provide.

**State**: full quaternion in a *target frame*. User aims the camera once, presses a button, that pose becomes identity; subsequent output is `q_current · q_target⁻¹`.
**Latency vs smoothness**: <10 ms is enough, but **smoothness matters more than latency**. A jittery gimbal is unwatchable; a slightly laggy one is fine. Tune the filter for low jerk, opposite of the balancing robot.
**Calibration**: one-shot zero-axis capture + mag figure-8. Mounting angle is implicit in the zero capture.

| Metric | Value |
|--------|-------|
| Priority | Medium |
| Orientation rate | 100 Hz |
| Accuracy | ±0.5° roll/pitch, ±2° yaw |
| Persistence | Yes (zero pose + mag cal) |
| Telemetry | None (or serial for tuning) |
| Calibration UX | One-shot zero + figure-8 |
| MCU floor | Nano (2-axis), Mega/Teensy (3-axis) |
| Implementation order | 4 |
| Effort | 3 sessions |

---

## 4. VTOL Transition Orientation (BORROWED FROM FLIGHT CONTROLLER)

`literature/dRehmFlight VTOL Documentation.pdf` covers the airframe pitching from hover (nose up) to forward flight (level) — a 90° pitch swing where Euler angles wrap at the `pitch = ±90°` gimbal-lock singularity. Quaternions are non-negotiable.

During transition, body-relative magnetic field changes rapidly but **gyro-integrated short-term attitude stays reliable**. Fusion trusts gyro more (low mag weight) during transition, re-trusts mag once steady. The BNO085 firmware handles this internally; the framework just needs to expose `cal_status` so the FC can detect stale fusion.

This is **borrowed scope**: `flight_controller/` owns the VTOL math; auto_orientation must not break during fast pitch sweeps and must expose mag-trust. No new framework code, only a regression test.

| Metric | Value |
|--------|-------|
| Priority | Low (covered by FC bridge) |
| Orientation rate | 200–400 Hz |
| Accuracy | ±2° during, ±1° steady |
| Persistence | Yes (airframe cal) |
| Telemetry | I2C/UART to FC |
| Calibration UX | Per-airframe |
| MCU floor | Teensy 4.x or ESP32 |
| Implementation order | 7 |
| Effort | 1–2 sessions (regression test) |

---

## 5. 3D Scanner / Photogrammetry Rig

`src/snapshot/` (see `docs/build/SNAPSHOT_FEATURE_GUIDE.md`) is already the embryonic version: on command, record synchronized `{quaternion, lat, lon, alt, accuracy, timestamp}` for later pairing with a camera frame. Cleanest user-visible product the framework has — needs polish, not architecture.

**Critical**: timestamp accuracy. Downstream photogrammetry needs "this pose belongs to *this* image" within ~10 ms. Expose monotonic millisecond timestamps; ideally GPS-PPS sync for sub-ms alignment. Feeds `skytracker_algorithm` and `engineering360` (per scope.md integration points).

| Metric | Value |
|--------|-------|
| Priority | High |
| Orientation rate | 10 Hz (sample on shutter, not continuous) |
| Accuracy | ±0.5° all axes (mag = 3 required) |
| Persistence | Yes (mag cal + SD log) |
| Telemetry | Serial (host PC pulls) |
| Calibration UX | Per-deployment-site mag figure-8 |
| MCU floor | Mega (current); Teensy for SD speed |
| Implementation order | 3 |
| Effort | 2–3 sessions (polish + timestamping) |

---

## 6. AR/VR Head Tracking

Latency budget is one 60 Hz frame: **<16 ms motion-to-photon**, of which orientation gets ~5 ms. Drift correction is critical — 1°/min is invisible to a balancing robot but immediately nauseating in VR. Needs ESP32 + WiFi (BLE possible but higher latency).

Commercial reference: Oculus DK2 fused IMU + camera fiducials at ~75 Hz; modern HMDs use inside-out optical at 90+ Hz. A pure-IMU framework can do **3DoF orientation only** — no positional tracking (that needs visual SLAM, out of scope). Useful for cardboard-class headsets and seated experiences, not room-scale.

| Metric | Value |
|--------|-------|
| Priority | Low |
| Orientation rate | 200–400 Hz |
| Accuracy | ±0.5°, <1°/min drift |
| Persistence | Yes (mag cal) |
| Telemetry | WiFi (low latency) or USB |
| Calibration UX | One-shot at boot |
| MCU floor | ESP32 |
| Implementation order | 8 |
| Effort | 4 sessions |

---

## 7. Robot-Arm End-Effector Orientation

Industrial/educational tool-pose feedback to a host PC running arm kinematics. Lower rate is fine (arms are slow). Hard requirement is **±0.1° accuracy** — an order of magnitude tighter than anything else in this catalog. Beyond what a calibrated BNO085 delivers via magnetometer-derived yaw. So this is **roll/pitch only**, or supplemented by an external reference (camera, fiducial) for yaw.

| Metric | Value |
|--------|-------|
| Priority | Low |
| Orientation rate | 50 Hz |
| Accuracy | ±0.1° roll/pitch, ±0.5° yaw |
| Persistence | Yes |
| Telemetry | Serial / USB |
| Calibration UX | Per-deploy |
| MCU floor | Mega / Teensy |
| Implementation order | 9 |
| Effort | 3 sessions |

---

## 8. Autonomous Boat / Surface Vehicle Attitude

Roll/pitch monitoring + heading for course correction. Marine environment introduces two real problems: **(a)** large ferrous mass nearby (motor, hull) producing severe hard-iron offsets, and **(b)** EM noise from brushed thrust motors. Calibration must be done **with motors running** for any yaw accuracy, sensor mounted as far from motors as possible.

No `floppi/marine_core_drone.md` exists yet — this is a placeholder. If a marine subproject materializes, the framework should support **per-throttle-bucket mag calibration** (cal stored as function of throttle %).

| Metric | Value |
|--------|-------|
| Priority | Low |
| Orientation rate | 20 Hz |
| Accuracy | ±2° |
| Persistence | Yes (rugged, throttle-dependent) |
| Telemetry | WiFi / radio |
| Calibration UX | Per-hull, motors-on |
| MCU floor | ESP32 |
| Implementation order | 6 |
| Effort | 4 sessions |

---

## 9. Educational / Classroom Kit (Nano + MPU6050)

Cheapest possible build — Nano + GY-521 — teaches concepts: quaternion math, calibration, complementary fusion, why mag matters. Framework's role is **documentation + reference sketches**, not new firmware features. MPU6050 has no magnetometer, so yaw is unreliable; this is a teaching moment, not a bug. `docs/research/MPU6050_RESEARCH_COMPILATION.md` plus the v1.1 MPU6050 driver work serves this directly.

| Metric | Value |
|--------|-------|
| Priority | Medium |
| Orientation rate | 50 Hz |
| Accuracy | ±3° pitch/roll (no yaw) |
| Persistence | No (cal recomputed each lesson) |
| Telemetry | Serial (USB) |
| Calibration UX | Continuous / each power-up (deliberately exposed) |
| MCU floor | Nano |
| Implementation order | 5 |
| Effort | 2 sessions (docs + examples) |

---

## Cross-Cutting Requirements

Framework-core (every app needs):

- **Sensor abstraction** — `OrientationSensor` interface; BNO085, BNO055, MPU6050 swappable (spec in `bno055_driver_and_multi_imu_strategy.md`).
- **Calibration persistence** — sensor-native flash (BNO085) or EEPROM (BNO055, MPU6050), plus a separate slot for mounting-angle.
- **Mounting-axis capture** — one-shot "record current pose as level." Universal primitive.
- **Output formats** — quaternion (primary), Euler (debug), rotation matrix — all derivable from one `OrientationData` struct.
- **Timestamping** — monotonic ms minimum, GPS-aligned when GPS present.
- **Cal-status surfacing** — apps must know "is mag still cal=3?" to degrade gracefully.
- **Sanity gate** — promote the balance-bot's `!isnan && abs(pitch)<90` check to `OrientationData::isValid()`.

Application-specific (do NOT promote to core):

- PID loops (balancing robot, gimbal only)
- Motor drivers (balancing robot, gimbal only)
- Shutter sync (snapshot only)
- WiFi command transport (FC bridge, head tracker, boat)
- Auto-tune (balancing robot only — too narrow to generalize)

---

## Prioritization Recommendation

Implement these three first, in order:

1. **Balancing robot** (`src/applications/balancing_robot/`) — already planned. Exercises mounting-angle EEPROM, auto-tune state machine, safety gate, 200 Hz hot path. Pays back the dissection notes directly.
2. **Snapshot/photogrammetry polish** — lowest marginal cost. Code mostly exists; needs better timestamping and a few UX commands. Validates the GPS+IMU+persistence triad end-to-end, feeds sister sky-tracker.
3. **Flight-controller I2C bridge** — high strategic value: makes auto_orientation a *peer* of the FC rather than a duplicate. Reuses the FC's existing I2C-slave pattern, so protocol design is half-done. Forces steady-rate quaternion publishing, exercising the output abstraction.

Together these cover: persistent calibration, mounting capture, one-shot UX, GPS fusion, high-rate quaternion streaming, two transport protocols (serial + I2C), and integration with both sister projects (`flight_controller`, `skytracker_algorithm`). After they land, the framework will have shaken out its real cross-cutting needs and the remaining six applications become small additions, not architectural changes.
