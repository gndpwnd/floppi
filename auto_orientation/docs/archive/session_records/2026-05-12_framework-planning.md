# Session Record — 2026-05-12 — Framework Planning

**Type**: Living document; will continue to evolve as the session progresses and follow-up sessions add context.
**Topic**: Re-scope `auto_orientation/` from a "BNO085 + GPS sensor toolkit" into a multi-IMU, multi-MCU, optionally-WiFi-enabled 3D-orientation framework with an application catalog.
**Outcome (so far)**: 12 research agents spawned (8 complete, 4 running at time of writing), scope/roadmap/todo rewritten, `.ino` reference dissected and archived, docs folder reorganized, INDEX.md added to every folder.
**Source code touched**: none yet — research and planning session.

---

## What triggered this session

The user dropped `SelfBallancingRobot3.ino` into the project root — a working balance bot built on Arduino Mega + BNO055 + L298N motor driver, with manual `PITCH_OFFSET = -8.6°` and hand-tuned PID gains. They asked to:

1. Dissect and archive that sketch.
2. Add BNO055 support alongside BNO085.
3. Automate mounting-angle detection and calibration value storage.
4. Add automatic PID tuning, generic enough to apply to inverted pendulums, drones, gimbals, and any single-axis loop.
5. Make the inverted-pendulum / self-balancing-robot application a compile-optional reference.
6. Spawn research agents to do the leg-work.

Over the session the user expanded scope further:

- Incorporate Teensy 4.0 / 4.1 and ESP32 / ESP32-S3 (mirroring the sister `flight_controller/` project's MCU matrix).
- Add WiFi support (web server, telemetry, OTA) on ESP32-class builds.
- Make `auto_orientation/` an explicit research framework, not just a sensor toolkit.
- Update `scope.md` to reflect that framework vision.
- Permit lots of documentation — split any single doc that goes over 1000 lines.
- Don't make git commits (live working tree only).
- The session record itself is allowed to keep evolving.

Then they made an important engineering observation: hanging USB cables physically bias balance-bot equilibrium, and the "best" mounting offset drifts over time as physical aspects change (cable, battery, payload, wear). So one-shot mounting capture is necessary but not sufficient — the framework also needs adaptive online estimation and disturbance rejection.

---

## What was decided

### Architecture-level

1. **Promote `auto_orientation/` to a framework.** Its mission statement is now in [scope.md](../../scope.md): a portable 3D-orientation framework with multi-IMU, multi-MCU support, automatic calibration, optional WiFi telemetry, and a reference-application catalog. It is NOT a flight controller, but it serves any system that needs to know its orientation.

2. **Three operational tiers**:
   - **Educational**: Nano + MPU6050 + Madgwick fusion, no GPS, no EKF.
   - **Research**: Mega / Teensy 4.x + BNO085 + GPS + EKF (current Phase 3 stack).
   - **Connected**: ESP32 / ESP32-S3 + WiFi dashboard + OTA + everything else.

3. **Sensor abstraction is the contract.** The `OrientationSensor` virtual base already in `src/sensors/sensor_base.h` is the contract. Concrete IMU drivers plug in. Fusion of raw sensors (MPU6050 + external magnetometer → Madgwick) happens inside a `FusedIMU` adapter that also implements `OrientationSensor`, so the rest of the framework doesn't change.

4. **Three application categories of auto-PID tuning** — generic strategy interface (`ITuningStrategy`), three concrete strategies selectable at compile time:
   - Relay-feedback (Åström–Hägglund 1984) — default for pendulums.
   - In-flight relay with throttle hold — for drones.
   - Twiddle / coordinate descent — fallback generic.

5. **Compile-flag cascade for WiFi** — mirror `flight_controller/`. `USE_WIFI` → auto-enables `USE_WEB_SERVER` + `USE_API_SERVER` + `USE_OTA`.

6. **Persistent storage HAL** must precede multi-MCU work. The current `<EEPROM.h>` direct usage in `src/config/calibration_storage.cpp:13` silently fails on ESP32 (no `begin()`/`commit()`). Phase 4.1 fixes this.

### Implementation-level

7. **Phase 4 plan** (the one to execute next):
   - 4.1 Persistent storage HAL (fixes KI-1)
   - 4.2 Sensor-tagged calibration blobs (fixes KI-3)
   - 4.3 One-shot mounting-angle capture (shortest-arc quaternion)
   - 4.4 Online adaptive mounting-offset tracking (new, from this session's insight)
   - 4.5 Generic auto-PID tuner with strategy interface
   - 4.6 BNO055 driver (and fix KI-2 along the way)
   - 4.7 Self-balancing robot reference application under `src/applications/balancing_robot/`
   - 4.8 Tetherless workflow (button + LED + buzzer + battery monitor)
   - 4.9 Phase 4 documentation

8. **Scenario regression test pattern** — before changing behavior with auto-tune, prove architectural equivalence against the archived `.ino` reference using a recorded trajectory CSV and ±5 PWM tolerance.

9. **Three priority applications for Phase 7**: self-balancing robot → photogrammetry snapshot polish → multirotor I2C bridge to `flight_controller/`. These exercise the most framework code with the highest user value (per the application-catalog research finding).

### Workflow-level

10. **No commits this session.** Local working tree only.
11. **Session record is a living document** — will keep being updated as new sessions come in.
12. **Docs > 1000 lines should be split**; per-folder INDEX.md is mandatory.

---

## What was done in concrete terms

### Files created

| Path | Purpose |
|------|---------|
| `docs/archive/balancing_robot_reference/DISSECTION_NOTES.md` | Reverse-engineered notes on the .ino reference |
| `docs/archive/balancing_robot_reference/INDEX.md` | Subfolder index |
| `docs/archive/session_records/INDEX.md` | Subfolder index (new pattern) |
| `docs/archive/session_records/2026-05-12_framework-planning.md` | This file |
| `docs/INDEX.md` (rewritten by reorg agent) | Root docs index |
| `docs/archive/INDEX.md` | Archive folder index |
| `docs/findings/INDEX.md` | Findings folder index |
| `docs/guides/INDEX.md` | Guides folder index |
| `docs/implementation/INDEX.md` | Implementation folder index |
| `docs/reference/INDEX.md` | Reference folder index |
| `docs/setup/INDEX.md` | Setup folder index |
| `docs/testing/INDEX.md` | Testing folder index |
| `docs/todo/INDEX.md` | Todo folder index |
| `docs/getting_started/INDEX.md` | (new folder, by reorg agent) |
| `docs/theory/INDEX.md` | (new folder, by reorg agent) |
| `docs/build/INDEX.md` | (new folder, by reorg agent) |
| `docs/hardware/INDEX.md` | (new folder, by reorg agent) |
| `docs/calibration/INDEX.md` | (new folder, by reorg agent) |
| `docs/phases/INDEX.md` | (new folder, by reorg agent) |
| `docs/research/INDEX.md` | (new folder, by reorg agent) |
| `docs/findings/auto_pid_tuning_research.md` | Research output |
| `docs/findings/balance_point_and_mounting_research.md` | Research output |
| `docs/findings/bno055_driver_and_multi_imu_strategy.md` | Research output |
| `docs/findings/multi_mcu_port_strategy.md` | Research output |
| `docs/findings/wifi_telemetry_integration_design.md` | Research output |
| `docs/findings/mpu6050_external_mag_pipeline.md` | Research output |
| `docs/findings/application_catalog.md` | Research output |
| `docs/findings/test_infrastructure_expansion.md` | Research output |
| `docs/findings/browser_dashboard_architecture.md` | _(pending — agent still running)_ |
| `docs/findings/online_adaptive_balance_tracking.md` | _(pending — agent still running)_ |
| `docs/findings/disturbance_compensation_research.md` | _(pending — agent still running)_ |
| `docs/findings/tetherless_operation_strategy.md` | _(pending — agent still running)_ |

### Files moved

The docs reorganization agent moved 32 files from `docs/` root into 7 new thematic subfolders:
- `docs/getting_started/` — onboarding (GETTING_STARTED, FAQS, ARCHITECTURE)
- `docs/theory/` — math & concept background
- `docs/build/` — build guides + feature flags + snapshot guide
- `docs/hardware/` — wiring + GPS hardware + troubleshooting
- `docs/calibration/` — end-user calibration procedure + impl notes
- `docs/phases/` — phase plans, test results, completion summaries, release checklists
- `docs/research/` — long-form research compilations
- `docs/reference/` — added 7 newly moved API references

### Files rewritten

| Path | Change |
|------|--------|
| `docs/scope.md` | Rewrote as framework vision (mission, objectives, sensor/MCU/application matrices, known issues, integration points) |
| `docs/roadmap.md` | Rewrote with Phase 4-8 plan (auto-orient → multi-MCU → WiFi → applications → advanced) |
| `docs/todo.md` | Rewrote as live planning + Phase 4 task breakdown |

### Files archived (moved out of root)

| Path | New location |
|------|--------------|
| `auto_orientation/SelfBallancingRobot3.ino` | `docs/archive/balancing_robot_reference/SelfBallancingRobot3.ino` |

---

## Research agents launched in this session

| ID | Topic | Status | Output |
|----|-------|--------|--------|
| 1 | Self-balancing dynamics + one-shot balance-point capture | ✅ | `findings/balance_point_and_mounting_research.md` |
| 2 | Auto-PID tuning algorithm comparison | ✅ | `findings/auto_pid_tuning_research.md` |
| 3 | BNO055 driver + multi-IMU strategy | ✅ | `findings/bno055_driver_and_multi_imu_strategy.md` |
| 4 | Multi-MCU port strategy | ✅ | `findings/multi_mcu_port_strategy.md` |
| 5 | WiFi/telemetry integration design | ✅ | `findings/wifi_telemetry_integration_design.md` |
| 6 | Docs folder reorganization (no research; file moves + INDEX writing) | ✅ | rewrote root + 7 INDEX.md files |
| 7 | MPU6050 + external magnetometer pipeline | ✅ | `findings/mpu6050_external_mag_pipeline.md` |
| 8 | Application catalog (9 apps profiled) | ✅ | `findings/application_catalog.md` |
| 9 | Test infrastructure expansion | ✅ | `findings/test_infrastructure_expansion.md` |
| 10 | Browser dashboard architecture | ✅ | `findings/browser_dashboard_architecture.md` |
| 11 | Online adaptive balance tracking (drift handling) | ✅ | `findings/online_adaptive_balance_tracking.md` |
| 12 | Disturbance compensation (cable drag, push, cascade) | ✅ | `findings/disturbance_compensation_research.md` |
| 13 | Tetherless operation strategy | ✅ | `findings/tetherless_operation_strategy.md` |

All 12 research-output agents complete (agent 6 was the docs reorganization, no markdown finding). Master design synthesis at [`../../findings/MASTER_DESIGN.md`](../../findings/MASTER_DESIGN.md) ties all 12 findings into a phase-ordered action plan with 20 top-level design decisions.

---

## Open work for next session

Most planning work is now done. The remaining items are:

1. Hardware reality-check on the planned task ordering: does Phase 4.1 (storage HAL) need any tooling before it can land, or is it pure software?
2. Generate or record a real pitch-trajectory CSV for the balance-robot scenario regression test.
3. Confirm the open questions in [`../../findings/MASTER_DESIGN.md`](../../findings/MASTER_DESIGN.md) Open questions section (5 items).
4. Decide on the BLE pendant inclusion in Phase 4.8 or defer to a later phase.
5. Begin Phase 4.1 implementation: `persistent_storage` HAL skeleton + AVR backend + native test.

---

## Open questions

- Which MCU should host the first auto-PID-tune validation? Likely Mega (where the existing .ino runs). Confirm with user.
- Does the balance robot need wheel encoders for v1 of the auto-tune loop, or can we ship without and add encoders in a later iteration? The disturbance research will inform this.
- Should the framework provide a sample-balancing-bot CSV (synthetic, generated from a pendulum model) as the scenario-test fixture, or wait for the user to record one on real hardware?
- Where should the BLE pendant button design live in the framework — `src/network/` (despite being non-WiFi) or a new `src/input/` subtree? The tetherless research will inform this.

---

## Lessons / patterns to keep

- Spawning 6-10 parallel research agents on a 15GB / 8-core box is comfortable (load avg stayed below 2 even with Chrome eating cores). User permission expanded to 10+, ceiling not yet hit.
- Marking a session record as a "living document" upfront frees it from needing to be polished/final.
- Multi-step planning sessions can write zero source code and still meaningfully advance the project, IF the findings are concretely actionable (file paths, file sketches, build envs, costs in bytes).
- Doc reorganization works well as a single-agent task with strict constraints (don't delete, don't edit content, only mv + INDEX).

---

*Last updated: 2026-05-12 during the session. This file will be updated as more agents return findings, more decisions are made, and the design synthesizes. When the session "ends" (user signals done or context closes), add an `End-of-session summary` at the bottom.*
