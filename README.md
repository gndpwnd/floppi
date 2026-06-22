# floppi

Two embedded projects under one roof:

- **`auto_orientation/`** — a research-tier self-balancing-robot framework built on a portable orientation + calibration stack (BNO055 / BNO085, AVR / ESP32 / Teensy).
- **`flight_controller/`** — a bare-bones VTOL flight controller firmware (dRehmFlight-derived) for Teensy 4.x and ESP32, with optional WiFi telemetry and a sibling swarm-API.

Both projects are **code-complete on the no-hardware axis** as of 2026-05-27 — every native test suite passes, every feature has been compiled and reviewed. Neither has been bench-validated end-to-end on real hardware. **Bench validation is the long pole.** Each project ships a consolidated bench runbook (linked below) that indexes every hardware-gated item into a safe-first order.

---

## Pick your project

| You have… | Go to | First read |
|---|---|---|
| An Arduino **Mega** or **Uno** + an L298N + a BNO055 (or BNO085) and want a self-balancing bot | `auto_orientation/` | [auto_orientation/docs/applications/CHOOSE_YOUR_TIER.md](auto_orientation/docs/applications/CHOOSE_YOUR_TIER.md) |
| A **Teensy 4.0/4.1** or **ESP32** + MPU6050 + SBUS/iBUS receiver and want a quadcopter flight controller | `flight_controller/` | [flight_controller/docs/0_quickstart.md](flight_controller/docs/0_quickstart.md) |

The two projects are intentionally decoupled — auto_orientation is the layer underneath any system that needs to know its orientation; flight_controller is the layer that turns that knowledge into stable rotors.

---

## auto_orientation — balancing-robot framework

- **Two-tier platform bifurcation (2026-05-19).** Mega-class boards host the full *universal/adaptive* stack (BOOTSTRAP gain derivation, RLS auto-tune, OnlineMountingEstimator, encoders, position outer loop). Uno-class boards host a *manual operator-guided* tier — calibrate IMU, walk a guided P→D→I tuning session, persist to EEPROM, flash a lean flight build. The split is memory-driven, not sensor-driven.
- **IMU choice is orthogonal to MCU choice.** BNO055 and BNO085 are both valid; today's envs default to BNO055 and the build system rejects BNO085-on-Uno (`#error`) because the SH-2 library footprint overruns Uno flash.
- **Landed features.** Calibration HAL with CRC-8-CCITT (vendored into the flight_controller as a sibling lib), photo-backup printer (every persisted value is printable + paste-able into `balance_constants.h` to survive an EEPROM wipe), guided P→D→I tuning, BOOTSTRAP K-pulse pole-placement, Phase 4M.14 analytical outer-loop gain derivation.
- **Native test suite: 20/20 green** via `pio test -e native_test` (Unity, host PC).
- **Entry points:**
  - [auto_orientation/docs/applications/CHOOSE_YOUR_TIER.md](auto_orientation/docs/applications/CHOOSE_YOUR_TIER.md) — 4-question decision tree (MCU, IMU, build env, crystal flag).
  - [auto_orientation/docs/applications/balancing_robot/FIRST_SUCCESS_MEGA.md](auto_orientation/docs/applications/balancing_robot/FIRST_SUCCESS_MEGA.md) — Mega first-success walk.
  - [auto_orientation/docs/applications/balancing_robot_uno/FIRST_SUCCESS_UNO.md](auto_orientation/docs/applications/balancing_robot_uno/FIRST_SUCCESS_UNO.md) — Uno setup→operational walk.
  - [auto_orientation/docs/applications/balancing_robot_uno/CHEATSHEET.md](auto_orientation/docs/applications/balancing_robot_uno/CHEATSHEET.md) — one-page bench card.
  - [auto_orientation/docs/findings/bench_validation_runbook_2026-05-27.md](auto_orientation/docs/findings/bench_validation_runbook_2026-05-27.md) — consolidated 24-item bench runbook, safe-first order.

---

## flight_controller — bare-bones VTOL firmware

- **Two MCU families.** Teensy 4.0/4.1 (ARM Cortex-M7 @ 600 MHz, 2 kHz flight loop) is the **recommended platform for first flight**. ESP32 / ESP32-S3 (240 MHz dual-core, 1 kHz flight loop) carries the WiFi/telemetry/security surface but the **flight loop on ESP32 is scaffolded and bench-untested** — use ESP32 today only for calibration + telemetry validation.
- **Landed features.** Madgwick 6-DOF attitude filter + PID stabilizer + user-customizable VTOL mixer; receiver protocols **SBUS / iBUS / DSM / PPM / PWM**; serial + I²C + WiFi command sources (RadioComm unifies all six); optional ESP32 WiFi STA (compile-time auth selector: OPEN / WPA2-PSK / WPA3-SAE / WPA2-Enterprise PEAP/TTLS/TLS), web dashboard, OTA, opt-in API token (`USE_API_AUTH`); `CalibrationStorage` HAL (Teensy EEPROM + ESP32 NVS-backed EEPROM, vendored from auto_orientation with 2026-05-20 P1 security fixes); barometer drivers (**BMP280** verified, **BMP388 / MS5611** scaffolded + datasheet-reviewed, telemetry-only — no altitude hold); raw-NMEA GPS passthrough (firmware parses nothing, no navigation).
- **Native test suite: 5/5 green** (firmware-internal compile + `tests/test_calibration.sh` exercises 18 calibration commands / 42 assertions over the serial command surface).
- **Two-build workflow.** Flash `*_calibration` env → run guided calibration (`./tools/calibrate.sh`) → copy printed `#define` values to `include/config.h` (IMU offsets also auto-persist to EEPROM/NVS via the HAL) → flash the live env → fly.
- **Entry points:**
  - [flight_controller/docs/0_quickstart.md](flight_controller/docs/0_quickstart.md) — 60-minute Teensy-first setup.
  - [flight_controller/docs/2_calibration_guide.md](flight_controller/docs/2_calibration_guide.md) — every calibration command in detail.
  - [flight_controller/docs/findings/bench_validation_runbook_2026-05-27.md](flight_controller/docs/findings/bench_validation_runbook_2026-05-27.md) — consolidated 17-item bench runbook, safe-first order (Phase 1 smoke test → Phase 4.5 ESC endpoints → Phase 5 failsafe → Phase 6 tethered hover).
  - [flight_controller/docs/build_matrix.md](flight_controller/docs/build_matrix.md) — what was last actually compiled, no carry-forward.

---

## Honest status (as of 2026-05-27)

Both projects are code-complete on the no-hardware axis. **41 items are deferred to hardware** and captured in the two bench runbooks:

- **24 items in auto_orientation** — including the first-ever Mega balance attempt (M7). The Mega bot has **never balanced successfully on a bench**; the last attempt (2026-05-18 PM late) was twitch-and-fall in ~1 s. The Uno SETUP-mode `'c'` calibration + `'t'` P→D→I tuning flow has **never been driven on real hardware** (added to the bench gate 2026-05-26).
- **17 items in flight_controller** — including failsafe end-to-end verification, ESC endpoint cal on real motors, BMP388/MS5611 driver verification, WiFi auth-mode runtime confirmation against real APs, security E2E (API-auth token, OTA password), and the unimplemented motor-test framework.

Nothing here is "production." Read the bench runbook for the project you're using **before** powering on hardware.

---

## Anti-scope — what these projects do NOT do

These are deliberately small stabilizers, not autopilots. **flight_controller has no altitude hold** (barometer is telemetry-only), **no GPS navigation** (GPS is raw NMEA passthrough — bytes relayed to an external flight computer, no waypoints, no return-to-home, no position hold), **no DSHOT** (PWM and OneShot125 only), **no battery monitoring**, **no SD card / black-box logging**, **no autotune**. **auto_orientation is not a flight controller** — it is the orientation + calibration layer underneath one. Multi-bot fleet coordination, cloud connectivity, and trajectory planning are out of scope for both. New builders: read each project's `docs/scope.md` before buying hardware, because the silent absence of a feature you assumed is the most dangerous kind of bug.

---

## Working with the repo

- **No git commits during multi-agent dev sessions** per operator preference — agents work in exclusive write zones and report back; commits are operator-driven, never automated.
- **Orchestration via the Claude Code Orchestra Workflow** (see [`/home/devel/palletai/claude_code_orchestra/`](/home/devel/palletai/claude_code_orchestra/) for the multi-agent framework powering work in this repo).
- **Session records** for both projects live under `auto_orientation/docs/archive/session_records/` and `flight_controller/docs/archive/session_records/` — each multi-agent session writes one even if the session was a 5-minute abort.
- **Native test suites** run on host PC, no hardware required: `pio test -e native_test` (auto_orientation), `tools/dev.sh test` (flight_controller, wraps the calibration command harness).
- **Build system** is PlatformIO across both projects; vendored libraries under each project's `lib/` (no cloud dependency at build/flash time).
- **Sibling tools** (not the focus of this README): `swarm_api/` — Python FastAPI ground-station for ESP32 drones; `fc_tool/` — Rust/Tauri serial-visualization desktop tool.

---

## Cross-references

| Project | scope | roadmap | todo | tier-decision | bench runbook |
|---|---|---|---|---|---|
| auto_orientation | [scope.md](auto_orientation/docs/scope.md) | [roadmap.md](auto_orientation/docs/roadmap.md) | [todo.md](auto_orientation/docs/todo.md) | [CHOOSE_YOUR_TIER.md](auto_orientation/docs/applications/CHOOSE_YOUR_TIER.md) | [bench_validation_runbook_2026-05-27.md](auto_orientation/docs/findings/bench_validation_runbook_2026-05-27.md) |
| flight_controller | [scope.md](flight_controller/docs/scope.md) | [roadmap.md](flight_controller/docs/roadmap.md) | [todo.md](flight_controller/docs/todo.md) | n/a (Teensy-first per [README](flight_controller/README.md)) | [bench_validation_runbook_2026-05-27.md](flight_controller/docs/findings/bench_validation_runbook_2026-05-27.md) |

---

*Last refreshed: 2026-06-21. Both bench runbooks date from 2026-05-27 — they are the authoritative "when hardware arrives" gate for either project.*
