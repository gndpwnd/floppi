# Bench-Validation Runbook — Flight Controller, 2026-05-27

> Project: `flight_controller/` (Teensy + ESP32 firmware)
> Author: `fc-bench-runbook@flight_controller:1`
> Status: Consolidation runbook — orders the 17 hardware-deferred items into a single safe-first bench sequence
> Companion: AO sibling runbook at `auto_orientation/docs/findings/bench_validation_runbook_2026-05-27.md`

---

## 1. What this document is — and is not

After the 2026-05-27 finishing-plan wave, the FC firmware is **feature-complete for its declared scope** and the plug-and-play onboarding gap is closed. What remains is **hardware-gated**: 17 deferred items spread across calibration, sensor verification, networking, security, and the unimplemented motor-test framework. They are listed flat in [`docs/todo.md`](../todo.md) ("Hardware-gated next steps") and were enumerated by the synthesizer in [`docs/archive/session_records/2026-05-27_fc_finishing.md`](../archive/session_records/2026-05-27_fc_finishing.md) §6.

This document is **the safe-first ordering**, not a procedure. Every step here is a single-line item + cross-link + success criterion. The actual procedure detail lives in the existing numbered docs (`0_quickstart.md` / `1_hardware_setup.md` / `2_calibration_guide.md` / `3_troubleshooting.md`) and in `docs/plans/motor-test-framework-plan.md`. **Do not duplicate procedure detail here** — if a step's "how" drifts from this runbook, the numbered doc wins.

**This is a quadcopter.** Unlike the AO sibling (a balancing bot — a single-axis system on the floor), the FC drives four spinning rotors that can leave the ground and inflict serious injury. Two non-negotiables follow:

1. **Props OFF** for every step in this runbook. Visual confirmation, not assumption. If you read "props off" twice in a section it is because skipping it once is fatal.
2. **ESC endpoint calibration precedes anything that arms motors.** Uncalibrated ESCs can refuse to disarm at low throttle, or treat min-throttle as low-thrust hover. Endpoint cal is item Phase 4.5, before failsafe verification (Phase 4.6) and tethered hover (Phase 6).

The 17 items are NOT a serial dependency chain — many are independent and could be parallelised across sessions. The ordering below is **safe-first**: catastrophic-risk items get the most verification before them; non-motor items (sensor and networking validation) sit toward the front because they cost nothing to fail.

---

## 2. Pre-bench checklist

Do not power the rig until every box is checked. These gate the validity of everything downstream.

### Hardware

- [ ] Bot wired per [`docs/1_hardware_setup.md`](../1_hardware_setup.md) (Parts 1-3: IMU, receiver, ESCs). Linux preflight (`sudo ../../tools/setup_permissions.sh`) done once.
- [ ] Battery topped to the airframe's nominal voltage — 1S / 2S / 3S as appropriate for the build. Check resting cell voltage.
- [ ] **PROPS OFF. Visually verify by lifting each motor in turn. Do not proceed to any motor-related phase if a single prop is on.**
- [ ] Receiver bound to TX (`1_hardware_setup.md` "Bind Receiver"). TX powered on, sticks centered, throttle at minimum.
- [ ] USB serial cable connected from host to FC (Teensy or ESP32). Use `tools/serial_monitor.py` or `pio device monitor -b 115200` — never raw `cat /dev/ttyACM*` (`docs/todo.md` SERIAL POLICY).
- [ ] ESC tester or multimeter on hand for Phase 4.5 (ESC endpoint cal) — not strictly required, but useful for catching wiring faults before they become motor-on faults.

### Software

- [ ] Working tree is the post-2026-05-27 finishing-plan checkout (see [`docs/build_matrix.md`](../build_matrix.md) for what was last compiled).
- [ ] Calibration build is the **target build** for Phases 1-6, not the live build (`teensy40_calibration` or `esp32_calibration`).
- [ ] Session record template prepared at `docs/archive/session_records/YYYY-MM-DD_fc_bench_<session_id>.md`.

---

## 3. Choose your platform — Teensy vs ESP32

The FC supports both targets, but they are **not at the same maturity**.

| Target | Status | Recommendation |
|---|---|---|
| **Teensy 4.0** (`teensy40` / `teensy40_calibration`) | Fully shipped. All 7 calibration phases plus the live flight loop have been compiled green and the calibration command surface has been exercised in earlier bench sessions (42/42 test suite pass per `tests/test_calibration.sh`). | **Default. Use this for first flight.** |
| **ESP32** (`esp32` / `esp32_calibration`) | Calibration and telemetry are shipped. **The flight loop on ESP32 is scaffolded but bench-untested** — per `docs/build_matrix.md`, only the build (581 KB flash / 35 KB RAM) is verified; no recorded session has flown an airframe off ESP32. | Use ONLY for calibration + telemetry validation (Phases 3, 4, plus all WiFi / security work). **Do not attempt first flight on ESP32 in this wave.** |

The two-target split exists because the ESP32 carries the WiFi/telemetry/security surface that Teensy does not. Pick Teensy for any phase that arms motors, pick ESP32 only for the things Teensy cannot do (WiFi-mode validation, NVS power-cycle, security E2E).

---

## 4. The runbook — 17 hardware-deferred items, safe-first

Each item gives: cross-link to the procedure doc, concrete success criterion, why it matters, rough bench time. Items are clustered by phase; phases are ordered by escalating risk (sensor-only → channel-only → motor-arm → motor-spin → flight).

### Phase 1 — Connect and smoke test (no motor risk)

- **Item 1.1: Flash + boot** — [`docs/0_quickstart.md`](../0_quickstart.md) Part 2 (Upload Calibration Build) and Part 3 (Launch Calibration Tool).
  - **Success:** OLED shows status, serial prints `FLIGHT CONTROLLER READY`.
  - **Why:** Confirms toolchain, USB serial, OLED I2C, and the calibration binary all came up clean on real silicon. Catches a stale build / wrong env before you trust any later data.
  - **Time:** 5 min.

- **Item 1.2: Automated test suite** — `tools/dev.sh test` (wraps `tests/test_calibration.sh`).
  - **Success:** 42/42 assertions pass.
  - **Why:** Validates the calibration command parser + persistence + EEPROM/NVS plumbing without any motors. Single best signal that the firmware-side of every subsequent phase is wired correctly.
  - **Time:** 5 min.

### Phase 2 — Orientation detection ('o' command)

- **Item 2.1: Run `o` and paste axis transform** — [`docs/todo.md`](../todo.md) "Phase 2: Orientation Detection".
  - **Success:** AccZ reads ~1.0 g when the board is level after the transform is applied and the firmware re-flashed.
  - **Why:** Establishes the board-frame ↔ body-frame mapping. Every downstream IMU + PID step is meaningless without it (uncalibrated boards typically show one of the X/Y axes pointing down at ~1 g).
  - **Time:** 10 min.

### Phase 3 — IMU calibration ('i' command) + persistence

- **Item 3.1: Run `i` and copy gyro/accel offsets** — [`docs/2_calibration_guide.md`](../2_calibration_guide.md) "Part: IMU Calibration" + [`docs/todo.md`](../todo.md) Phase 3.
  - **Success:** Quality check passes; `t` telemetry shows AccZ ≈ 1.0 g, Gyro X/Y/Z ≈ 0 deg/s, Roll/Pitch ≈ 0° when level.
  - **Why:** Zeros the gyro bias (currently ~4-11 deg/s per axis uncalibrated — see `docs/todo.md` Notes) and the accel offset. Sets the floor for Madgwick attitude estimation.
  - **Time:** 10 min.

- **Item 3.2: CalibrationStorage HAL persistence via power-cycle (Teensy EEPROM)** — wave-6 HAL port; see [`docs/archive/session_records/2026-05-26_calibration_storage_port.md`](../archive/session_records/2026-05-26_calibration_storage_port.md) §9.
  - **Success:** After `i` completes, serial prints `MPU6050 cal saved to EEPROM (24 bytes)`. After a hard power-cycle (unplug, not just reset), boot prints `MPU6050 cal restored from EEPROM`.
  - **Why:** The 2026-05-26 port is byte-identical to the AO original on AVR/Teensy, but the FC has never executed it on real EEPROM.
  - **Time:** 10 min.

### Phase 3.5 — ESP32 NVS persistence (cross-project hardware-gated item)

- **Item 3.5.1: ESP32 NVS save → hard power-cycle → restore** — [`docs/archive/session_records/2026-05-26_calibration_storage_port.md`](../archive/session_records/2026-05-26_calibration_storage_port.md) §5 + §9.
  - **Success:** Same as 3.2 above, but on `esp32_calibration` and with a **hard** power-cycle (battery-disconnect / USB-unplug, NOT `EN`-button reset). `cs_save()` must call `EEPROM.commit()` — verify in serial output.
  - **Why:** ESP32 NVS writes are silently dropped on reset if `commit()` is not called. The HAL port specifically handled this, but the codepath is untested on hardware. This is a **separate item from 3.2** because the failure mode is ESP32-specific.
  - **Time:** 15 min.

### Phase 4 — Radio calibration ('r' command)

- **Item 4.1: Run `r` and paste channel mapping** — [`docs/todo.md`](../todo.md) Phase 4.
  - **Success:** `s` command shows CH1-6 responding to TX sticks; channel mapping in `config.h` matches sticks (throttle on CH3, etc.).
  - **Why:** Receivers map physical channels to logical roles differently per TX brand. Without this, throttle could be on CH2 and roll on CH3 — a recipe for instant crash.
  - **Time:** 10 min.

### Phase 4.5 — ESC endpoint calibration ('e' command) — PROPS OFF, EARLIEST MOTOR PHASE

- **Item 4.5.1: Run `e` end-to-end** — [`docs/2_calibration_guide.md`](../2_calibration_guide.md) "Part: ESC Endpoint Calibration" + [`docs/0_quickstart.md`](../0_quickstart.md) Part 4b.
  - **Success:** ESCs emit MAX-throttle confirmation tone (ascending beeps), then MIN-throttle confirmation tone (descending beeps); throttle sweep produces linear ramp on all 4 motors with no dead band at the bottom or clipping at the top.
  - **Why:** This is the **earliest motor-arming step**. It precedes failsafe verification (Phase 4.6) because uncalibrated ESCs can refuse to honor the failsafe min-throttle command, defeating the failsafe test. The procedure doc has the full PROPS-OFF precondition + tone walkthrough + common-failure table.
  - **Time:** 20 min.

### Phase 4.6 — Failsafe detection ('f' command) — PROPS OFF

- **Item 4.6.1: Run `f` and paste FAILSAFE_* block** — [`docs/2_calibration_guide.md`](../2_calibration_guide.md) "Part: Failsafe Detection" + [`docs/0_quickstart.md`](../0_quickstart.md) Part 4a.
  - **Success:** All 6 channels read distinct failsafe values when TX is powered off; throttle channel = `FAILSAFE_THROTTLE` reads < 1100 µs (or per-receiver low value); `#define CALIBRATED_FAILSAFE` uncommented in `config.h`.
  - **Why:** Without measured failsafe values, the firmware cannot distinguish "TX off" from "throttle held high" — the runaway path. The 2026-05-27 calibration-guide wave added the full TX-on → TX-off → paste-and-uncomment procedure with the verification + common-failure tables.
  - **Time:** 15 min.

### Phase 5 — Failsafe end-to-end bench verification — PROPS OFF, MOTORS-ARMED

- **Item 5.1: Confirm motors stay safe when TX powered off (live build)** — [`docs/0_quickstart.md`](../0_quickstart.md) Part 4a step 7 ("Bench-verify").
  - **Success:** With throttle previously commanded high, TX powered OFF → motors stop within one failsafe sample (~50 ms). With throttle high and TX still ON → motors continue spinning (sanity, proves the test is meaningful).
  - **Why:** This is the **single most important safety test in the runbook**. Without it confirmed on hardware, every later flight step is unsafe. Item 4.6 measures the failsafe values; this item proves the firmware acts on them.
  - **Time:** 10 min. ABORT THE SESSION if motors do not stop on TX-off.

### Phase 6 — PID sanity check — tethered hover ('g' command) — PROPS ON, TETHERED

- **Item 6.1: Tethered hover with `g`** — [`docs/0_quickstart.md`](../0_quickstart.md) Part 4c + [`docs/pid-tuning-guide.md`](../pid-tuning-guide.md) §3 and §5.
  - **Success:** Bot lifts on tether; `g`-streamed telemetry shows pitch/roll holding within ±5° of setpoint; no sustained oscillation; conservative defaults per airframe class.
  - **Why:** This is the **first flight-loop test on real motors**. The PID defaults in `config.h` are tuned for a 5" quad-X — other airframes need the §4 rescale FIRST.
  - **Time:** 30 min (includes gain adjustment via live serial + `d`-dump → paste).

### Phase 7 — Live build verification (no calibration overhead)

- **Item 7.1: Flash `teensy40` (live build) and confirm arming behavior** — [`docs/todo.md`](../todo.md) Phase 7.
  - **Success:** Clean boot (OLED status + telemetry sane). Arming via throttle-low + CH5 transition works; disarming via CH5 reverse works. Motor outputs around 1000-1500 µs with sticks centered, not pegged at extremes.
  - **Why:** Confirms the live binary (no calibration interactive surface, no serial-blocking prompts) behaves the same as the calibration build did under telemetry. Catches `USE_CALIBRATION_MODE` flag drift.
  - **Time:** 15 min.

### Sensor-driver bench-validation (independent — can interleave anywhere after Phase 1)

- **Item S.1: BMP388 driver against real sensor** — drivers per [`docs/findings/barometer_integration_spec_2026-05-20.md`](barometer_integration_spec_2026-05-20.md); enable via `-DBAROMETER_BMP388`.
  - **Success:** Pressure trace reads within ±1 hPa of a reference barometer at bench altitude; altitude rate stays < 10 cm/s while sensor is stationary.
  - **Why:** BMP280 is the only baro ever exercised on hardware. BMP388 and MS5611 are datasheet-reviewed but unverified.
  - **Time:** 15 min.

- **Item S.2: MS5611 driver against real sensor** — same spec, `-DBAROMETER_MS5611`.
  - **Success:** Same as S.1.
  - **Why:** Same as S.1 — second of the two unexercised baro drivers.
  - **Time:** 15 min.

- **Item S.3: `b` sea-level calibration end-to-end vs `swarm_api`** — see [`docs/findings/swarm_api_contract_2026-05-20.md`](swarm_api_contract_2026-05-20.md).
  - **Success:** `b` command sets sea-level pressure; subsequent telemetry POSTs to `swarm_api` reflect calibrated altitude; the server-side log shows the calibrated value within the expected envelope.
  - **Why:** The wire contract is SHA-stamped but has never been exercised end-to-end. Server-side acceptance is the only proof of contract.
  - **Time:** 30 min (requires `swarm_api` server reachable).

### Networking validation (ESP32-only; independent of the motor sequence)

- **Item N.1: WiFi OPEN against real OPEN AP** — `WIFI_AUTH_MODE_OPEN`; see [`docs/archive/session_records/2026-05-22_wifi_network_modes.md`](../archive/session_records/2026-05-22_wifi_network_modes.md) §5.
  - **Success:** Connects without credentials; IP assigned; `/api/status` reachable from a co-located host.
- **Item N.2: WiFi WPA3-SAE against real WPA3 AP** — `WIFI_AUTH_MODE_WPA3_SAE`.
  - **Success:** Connects with SAE handshake; behaves like PSK from the firmware's perspective.
- **Item N.3: WiFi Enterprise PEAP against eduroam/RADIUS** — `WIFI_AUTH_MODE_ENTERPRISE`, PEAP credentials.
  - **Success:** Connects via PEAP; survives a re-association.
- **Item N.4: WiFi Enterprise EAP-TLS with real client certs** — `USE_WIFI_CERTS`.
  - **Success:** Connects via EAP-TLS using cert/key from `wifi_credentials.h`.
  - **Why (N.1-N.4):** The compile-time selector is QA-greenlit (`findings/wifi_modes_qa_2026-05-22.md` GO), but no real-AP runtime confirmation exists. Enterprise modes especially — eduroam and EAP-TLS have RADIUS-side gotchas the firmware cannot anticipate.
  - **Time:** 30 min each. Independent — do whichever a real AP exists for.

### Security end-to-end (depends on Phase 1; independent of motor sequence)

- **Item Sec.1: arm-over-WiFi rejection with `USE_API_AUTH` + real token** — [`docs/archive/session_records/2026-05-22_security_correctness_docs.md`](../archive/session_records/2026-05-22_security_correctness_docs.md) §7.
  - **Success:** Without token → 401; with wrong token → 401; with right token → arming succeeds (only if all other arm preconditions met).
- **Item Sec.2: OTA reject-on-bad-password** on a real LAN.
  - **Success:** Wrong OTA password → upload rejected, no flash write; correct password → OTA succeeds.
- **Item Sec.3: worst-case WS frame** with both GPS and baro present.
  - **Success:** Dynamic-`String` path (W6) does not truncate; broadcast frame parses cleanly client-side at maximum field width.
- **Item Sec.4: I2C XOR checksum** against a real I2C master.
  - **Success:** Bad checksum → command rejected; good checksum → command accepted.
  - **Time:** 30 min each.

### ESP32 GPIO conflict A bench resolution (paper-resolved; hardware-confirm)

- **Item G.1: SBUS_RX/TX vs SERVO_PIN_4/5 on GPIO 16/17** — resolved in source by 2026-05-20 wave; see [`docs/findings/esp32_gpio_conflict_resolution_2026-05-20.md`](esp32_gpio_conflict_resolution_2026-05-20.md) §Conflict A.
  - **Success:** With a 4+ servo airframe on ESP32, neither SBUS RX nor servo channels glitch under sustained load.
  - **Why:** The pin reassignments are compiled green, but no hardware has exercised both the SBUS receiver and >=4 servo channels on the post-2026-05-20 ESP32 binary. This is a bench-confirmation, not a redesign.
  - **Time:** 15 min.

### Motor-test framework (gated on operator ESC-protocol decision FIRST)

- **Item M.1: Execute the motor-test framework** — see [`docs/plans/motor-test-framework-plan.md`](../plans/motor-test-framework-plan.md). **Unimplemented spec only.**
  - **Operator gate:** ESC-protocol decision must be made before any code lands (PWM 1000-2000 µs / OneShot125 / DShot — affects `motors.cpp` end-to-end). Without that decision, this item cannot be planned, let alone benched.
  - **Success:** All 6 phases of the spec pass (PWM endpoint hit, per-motor spin + mixer-position map, mixer differential under simulated tilt, failsafe-cut latency, arming-gate non-bypass, throttle-cut zero).
  - **Why:** This is the gold-standard motor test sequence the project has spec'd but never implemented. Independent of first flight — first flight (Phase 6) needs `g`-tethered hover, not the framework; the framework is what you reach for the second time you're at the bench wanting to test mixer math or failsafe latency in isolation.
  - **Time:** Implementation is a separate session; bench execution after that is 60-90 min.

---

## 5. Abort criteria

ABORT the session immediately and walk back from the rig if any of these fire:

- **Props on by mistake** — even one. Stop, remove props, restart the affected phase from Item 4.5.
- **Motor smoke or burning smell** — disconnect battery, do not retry. Diagnose ESC / motor / wiring.
- **ESC tone fault** — sustained continuous tone (not the calibration ascending / descending sequence) indicates ESC fault or miscalibration. Disconnect battery.
- **Failsafe channel drift** — failsafe values change between successive `f` runs. Receiver-side failsafe not configured per TX; fix on TX before continuing (per [`docs/2_calibration_guide.md`](../2_calibration_guide.md) common-failure table).
- **IMU NaN persists > 2 s** — the 2026-05-27 W6 Madgwick `asin()` clamp + the 2026-05-22 gradient-normalization guard should make this physically impossible; if it still fires, the IMU is broken or the I2C bus is corrupted. Power off, diagnose.

---

## 6. Post-bench — what to record

Create a session record at `docs/archive/session_records/YYYY-MM-DD_fc_bench_<session_id>.md`. Include:

- Build identity: PlatformIO env + working-tree commit hash + USE_* flag set.
- Hardware: airframe class, IMU model, receiver model, ESC protocol, motor KV.
- Per-phase outcome: pass / fail / skipped, with the success criterion as observed.
- Any captured telemetry: raw serial logs (`tee` to file per the SERIAL POLICY in [`docs/todo.md`](../todo.md)).
- Any procedure step that failed: file a follow-up under `docs/findings/` describing the failure and its provisional root cause. Update [`docs/todo.md`](../todo.md) "Hardware-gated next steps" to reflect what's now done.
- Update [`docs/build_matrix.md`](../build_matrix.md) only if a new env was actually built this session — per the matrix's no-carry-forward policy.

---

## 7. Troubleshooting pointers

If a step fails, do not iterate blindly. Use [`docs/3_troubleshooting.md`](../3_troubleshooting.md) and [`docs/diagnose_decision_tree.md`](../diagnose_decision_tree.md) (symptom → `dev.sh diagnose` → fix). Common-failure tables are also embedded in [`docs/2_calibration_guide.md`](../2_calibration_guide.md) for the `f` and `e` procedures.

If the failure is repeatable and the troubleshooting docs do not cover it, file a finding under `docs/findings/` rather than tightening a retry loop.

---

## 8. Cross-references

- Source enumeration of the 17 deferred items: [`docs/archive/session_records/2026-05-27_fc_finishing.md`](../archive/session_records/2026-05-27_fc_finishing.md) §6.
- Flat task list: [`docs/todo.md`](../todo.md) "Hardware-gated next steps".
- Phase 1-7 bring-up procedure detail: [`docs/todo.md`](../todo.md) "When Hardware Returns: Calibration Phases".
- Failsafe + ESC procedure detail: [`docs/2_calibration_guide.md`](../2_calibration_guide.md) "Part: Failsafe Detection" and "Part: ESC Endpoint Calibration".
- First-flight walkthrough with 4a/4b/4c prerequisites: [`docs/0_quickstart.md`](../0_quickstart.md) Parts 4a/4b/4c → Part 5.
- Build coverage: [`docs/build_matrix.md`](../build_matrix.md).
- Motor-test framework spec (unimplemented): [`docs/plans/motor-test-framework-plan.md`](../plans/motor-test-framework-plan.md).
- AO sibling runbook: `auto_orientation/docs/findings/bench_validation_runbook_2026-05-27.md`.

---

*Index + ordering only. Procedure detail lives in the numbered docs and the motor-test plan. If this runbook drifts from those, the numbered docs win.*
