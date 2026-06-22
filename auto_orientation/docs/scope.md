# Project Scope: Auto Orientation Framework

**Status**: Framework expansion (Phase 3 of original plan complete: BNO085 + GPS + EKF, 143+ tests passing)
**Last updated**: 2026-05-26 (wave 6: Uno IMU selection wired at build level — `#error` on USE_BNO085+Uno; cal-blob slot widened to 72 B for future BNO085)

> **Design direction**: see [MINIMIZE_ACCELERATIONS_PHILOSOPHY.md](MINIMIZE_ACCELERATIONS_PHILOSOPHY.md) for the project's current direction on the balancing-robot reference application.

---

## Platform bifurcation (2026-05-19, clarified 2026-05-26) — Mega-universal vs Uno-minimal

After the 2026-05-18 PM-late bench run that left Uno at 97.5 % flash with the universal stack still unable to balance reliably, the balance-bot reference application **splits in two**. This is a strategic pivot, not a scope deletion — the universal vision continues, but it migrates exclusively to Mega-class targets where the flash and GPIO/interrupt budget can host the sensors it actually needs. The pivot is captured in detail in the operator-memory note `project_strategic_pivot_2026-05-19.md`.

**The bifurcation is memory-driven, not sensor-driven.** The split is about which **capability tier** each MCU can host, not which IMU it talks to:

- **Mega-class (lots of flash + RAM)** → **universal "plug-and-play" auto** tier. BOOTSTRAP + K cross-check + analytical gain auto-derivation (Phase 4M.14) all live here because the heavyweight measurement + identification code only fits on Mega.
- **Uno-class (tight flash + RAM)** → **manual operator-guided** tier. Calibrate the IMU, then a guided P→D→I PID tuning session, then persist + flash a lean flight build. The Uno's small build is lean by intent precisely because the auto-tuning logic is deliberately absent — that absence is the design, not a deficiency.

**IMU choice is orthogonal to MCU choice.** Both **BNO055 and BNO085** are valid IMUs on **either MCU**. Today's source defaults vary by build env (most balance envs ship BNO055; orientation framework envs ship BNO085) and the BNO085 path on the Uno minimal program is a research stub today, but the architecture is **not** coupled — the sensor abstraction (`OrientationSensor`) compiles either driver in or out via `USE_BNO055` / `USE_BNO085`, and there is no design reason a Mega cannot run BNO055 or a Uno cannot run BNO085. See [IMU selection (compile-time)](#imu-selection-compile-time) below.

| Build | Target | Source tree | Philosophy | Flash budget |
|---|---|---|---|---|
| **Mega-balance (universal/adaptive)** | Arduino Mega 2560 + BNO055 or BNO085 (current envs default to BNO055) + **wheel encoders** (+ future sensors) | `src/applications/balancing_robot/` (existing) | BOOTSTRAP + RLS + collision detection + OnlineMountingEstimator + position containment + auto-PID + everything in [UNIVERSAL_BALANCE_BOT_VISION.md](UNIVERSAL_BALANCE_BOT_VISION.md) | **Generous** — Mega has 256 KB. Optimize for clarity, not size. New code lands here by default. |
| **Uno-balance (guided-tuning + minimal flight)** | Arduino Uno + BNO055 or BNO085 (current code defaults to BNO055; BNO085 path is a research stub) | `src/applications/balancing_robot_uno/` (new — owned by sibling agent) | Single-purpose self-balancer. `pitch → PID(Kp,Ki,Kd) → PWM`. **No** on-MCU adaptation (no RLS, no BOOTSTRAP, no OnlineMountingEstimator). Gains set via an on-device guided P→D→I tuning session (`arduino_uno_tuning` env), persisted to EEPROM; the lean flight build (`arduino_uno_minimal`) reads them at boot. Reference: `archive/balancing_robot_reference/SelfBallancingRobot3.ino`. See [findings/uno_guided_tuning_design_2026-05-20.md](findings/uno_guided_tuning_design_2026-05-20.md). | **Tight** (32 KB on Uno) — handled via a two-env split: guided-tuning firmware and lean flight build are flashed separately. |

### How the Uno gets its constants

Universal *on-MCU adaptation* (RLS, BOOTSTRAP, OnlineMountingEstimator) is gone on Uno. The PID gains are tuned instead through an **on-device guided P→D→I tuning session** (operator clarification 2026-05-20; stage order corrected 2026-05-26): a serial-driven interactive walkthrough — hosted by a dedicated `arduino_uno_tuning` build env — that steps through Kp, Kd, Ki one term at a time while the bot balances live, then persists the result to **EEPROM**. The lean flight build (`arduino_uno_minimal`) reads the tuned gains from EEPROM at boot. `balance_constants.h` is a hand-editable **cold-start seed** used only when EEPROM holds no tune; it is no longer auto-generated.

The offline Python brute-force / evolutionary tuner (`tools/sim/`, wrapping the `balance_bot_sim.py` plant model) is **demoted to an optional cold-start seed generator** for a brand-new chassis — it is no longer the operational tuning loop. When the bot needs re-tuning (new battery, new wheels, different surface) the operator runs another guided on-device session; no recompile, no host tooling required.

See [docs/findings/uno_guided_tuning_design_2026-05-20.md](findings/uno_guided_tuning_design_2026-05-20.md) for the design and [docs/applications/balancing_robot_uno/README.md](applications/balancing_robot_uno/README.md) for the end-to-end workflow.

### Uno modes: SETUP vs OPERATIONAL

The Uno program is best understood as two distinct modes living in two separate build envs:

- **SETUP MODE — `arduino_uno_tuning`.** Hosts the operator-facing flow: IMU calibration step, guided P→D→I PID tuning, persist-and-print-for-photo-backup. This is where every "manual operator step" lives — the deliberate small/cheap tier of the platform bifurcation. The flow ORDER is: open serial → calibrate the IMU → guided P → D → I tuning → save (which prints all values in a copy-paste / photo-friendly form).
- **OPERATIONAL MODE — `arduino_uno_minimal`.** Lean flight build. Reads the stored values at boot (EEPROM tune block + IMU cal blob; falls back to the `balance_constants.h` cold-start seed if EEPROM is empty) and runs the `pitch → PID → PWM` loop. Carries none of the SETUP-MODE strings or state machines, so the flash budget stays comfortable.

A typical lifecycle is therefore: flash SETUP MODE → run the guided session → save → flash OPERATIONAL MODE → operate. Re-tunes (new battery, new wheels, different surface) repeat that loop; no recompile of OPERATIONAL MODE is required.

### Robustness against value loss — every persisted value is photographable

Across the entire balance-bot reference application, **every value that gets persisted must also be printable to serial in a copy-paste-ready form** so the operator can photograph the serial console and hardcode the values back into source later if EEPROM is wiped or the chip is replaced. This is a **first-class design principle**, not an afterthought, and applies symmetrically:

- **Uno side** — the SETUP MODE save step prints the IMU calibration blob, PID gains, and pitch offset in a form that pastes directly into `src/applications/balancing_robot_uno/balance_constants.h`. That header is the canonical hardcode-from-photo target — a wiped EEPROM is recoverable with one paste and a recompile.
- **Mega side** — the equivalent live readout is the `g` serial telemetry command (BOOTSTRAP-derived gains streamed live during operation) plus the standard status output. The same paste-into-source recovery path applies if the operator wants to pin a known-good tune.

The implication for any future feature that adds a persisted value: it must also be added to the print path. A persisted value with no printable form is a gap in the recovery story.

**Wave-6 (2026-05-26) — cal-blob slot is now variable-length to accommodate BNO085.** `tune_storage`'s on-EEPROM cal slot at base `0x220` was widened from a fixed 22-byte BNO055 layout to a length-prefixed variable-length payload reserving up to 72 B (BNO085 SH-2 DYNAMIC_CALIBRATION FRS worst-case). The block-format version byte was bumped `0x02 → 0x03`; a pre-existing fixed-22-byte v2 blob from earlier firmware will reject-on-load cleanly (operator re-runs `'c'` to re-cal). The photo-backup printer schema continues to emit the array as `<imu_tag>_CAL_BLOB[<len>] = { ... }`, so a future operator on a BNO085-capable target may want to add a matching `BNO085_CAL_BLOB[<len>]` declaration to `balance_constants.h` alongside the existing `BNO055_CAL_BLOB[22]` hardcode site.

### How this affects existing scope-violation rows

The [Current scope violations — audit](#current-scope-violations--audit-2026-05-18-updated-pm-evening-phase-410c-landed) table below was written when the universal stack was *also* the Uno target. With the pivot, several rows are re-tagged:

- **`[mega]`** — the row applies on the Mega-universal path. Replacement plan stands; do the work there.
- **`[uno-intentional]`** — the row is a Uno-minimal-program design choice (hardcoded for simplicity, value will be brute-force-tuned offline). Not a violation in the Uno context. Still a violation if the same constant appears in `src/applications/balancing_robot/`.
- **`[deferred-to-mega]`** — the row was a Uno-flash-driven hardcode that was under audit. The Uno path no longer needs to retire it (it's an intentional hardcode there). On Mega, the replacement plan still applies.

The audit table is annotated below.

---

## Build environments (post-2026-05-18 cleanup)

The platformio.ini is intentionally tiny. Three active balance-robot envs (`mega_balance` plus the two Uno builds), one legacy/dead env (`uno_balance`), two scaffolded envs (ESP32/Teensy, for a future port) and two orientation-framework envs.

| Env name | Status | Board | Flash | Default IMU | Notes |
|---|---|---|---|---|---|
| `uno_balance` | **legacy/dead** | Arduino Uno | 32 KB | BNO055 | The old universal-stack-on-Uno path, superseded by the 2026-05-19 platform bifurcation. Kept only so `platformio.ini` history is readable; do not extend it. The live Uno builds are `arduino_uno_minimal` and `arduino_uno_tuning` below. |
| `arduino_uno_minimal` | **active** | Arduino Uno | 32 KB | BNO055 | **Lean flight build** for the minimal program under `src/applications/balancing_robot_uno/`. `pitch → PID(Kp,Ki,Kd) → PWM`, no adaptive layer. Reads tuned gains from EEPROM at boot (falls back to the `balance_constants.h` cold-start seed). Targets <50% Uno flash. |
| `arduino_uno_tuning` | **active** | Arduino Uno | 32 KB | BNO055 | **Guided P→D→I tuning build** — `arduino_uno_minimal` plus a serial-driven `TuningSession` state machine (`-D UNO_GUIDED_TUNING`). Walks the operator through Kp, Kd, Ki one term at a time while the bot balances live, then persists the result to EEPROM via `tune_storage`. Also hosts the `'c'` on-Uno BNO055 calibration wizard (`calibration_session`) — no Mega-side dependency. The flight build compiles all of this out. |
| `mega_balance` | **active** | Arduino Mega 2560 | 256 KB | BNO055 | **Pivot 2026-05-19**: home of the universal/adaptive stack — BOOTSTRAP, RLS, collision detection, OnlineMountingEstimator, position containment (Phase 4M.11). Wheel encoders preferred over IMU-only pitch double-integration. |
| `esp32_balance` | *scaffolded* | ESP32 dev | 1.3 MB | BNO055 | Needs MsTimer2 → esp_timer port; WiFi cascade landed. |
| `teensy_balance` | *scaffolded* | Teensy 4.0 | 1.9 MB | BNO055 | Needs MsTimer2 → IntervalTimer port; FPU + 600 MHz. |
| `mega_orientation` | active | Arduino Mega 2560 | 256 KB | BNO085 | Original Phase 3 framework (orientation + GPS + EKF). |
| `mega_orientation_calibration` | active | Arduino Mega 2560 | 256 KB | BNO085 | Verbose serial for cal wizard. |
| `native_test` | active | host PC | n/a | n/a | Unity unit tests via `pio test -e native_test`. |

### IMU selection (compile-time)

**IMU choice is orthogonal to MCU choice.** Every balance env *defaults* to **BNO055** (matches the reference .ino), but the architecture supports BNO055 or BNO085 on either MCU — the table-column "Default IMU" above describes today's env defaults, not a hardware constraint. Either driver compiles-in or compiles-out per its `USE_<IMU>` flag, so the binary only ships the chip you have wired.

To override the default:

```bash
# Use BNO085 instead of BNO055 on Mega:
pio run -e mega_balance --project-option="build_flags=-D USE_BNO085 -U USE_BNO055"

# Use MPU6050 + external magnetometer (Phase 5+):
pio run -e mega_balance --project-option="build_flags=-D USE_MPU6050 -U USE_BNO055"
```

The same override pattern applies to the Uno envs in principle; today the Uno-minimal program's I²C wiring and the `uno_balance_app.cpp` direct-driver path are hand-rolled against BNO055, so a BNO085-on-Uno path is a doc-architectural intent rather than a fully wired-up build (the BNO085 driver itself does exist in `src/sensors/`). Closing that gap is a small future task, not an architectural change.

**Wave-6 tightening (2026-05-26) — BNO085 on Uno is rejected at build time.** The architectural memory-tier principle ("Uno is the small/cheap tier") is now made concrete: `src/applications/balancing_robot_uno/main.cpp` selects the IMU via `#ifdef USE_BNO085` / `#else default USE_BNO055`, and a hard `#error` trips if a Uno target build smuggles in `-DUSE_BNO085` — the Adafruit BNO08x / SH-2 library footprint exceeds the Uno's 32 KB flash budget. A second `#error` rejects builds that define BOTH `USE_BNO055` and `USE_BNO085`. The `arduino_uno_*` envs continue to ship `-DUSE_BNO055` by default. USE_BNO085 on Mega/Teensy/ESP32 remains architecturally supported (no flash constraint there) but the Mega-side wiring is a future workstream — closing that gap is small once it's needed.

### Flash strategy on Uno (32 KB target)

The 2026-05-18 session bottomed out at 4 bytes free, blocking Phase 2.x implementation. **The fix is not to keep trimming;** it is to delete heavyweight library code paths:

| Save | Mechanism | Implemented |
|---|---|---|
| 1574 B | `BNO055::getStatusString` rewritten to avoid `snprintf` (drops `vfprintf` + `snprintf` from the binary) | ✅ 2026-05-18 |
| ~124 B + 140 B RAM | `Adafruit_BNO055` moved from heap allocation (`new`) to file-scope static instance | ✅ 2026-05-18 |
| ~1.3 KB | Remove `RelayFeedbackStrategy::step` (relay tuner obsoleted by RLS — backlog #7) | pending |
| ~432 B | Replace `Serial.print(float, N)` with `int * 100` + manual decimal formatter | pending |

Current state: 94.7% flash, 1702 B free on Uno. All Phase 2.x features (noise floor + FALLEN heuristic + gain scheduling + motor-null-space HELD) fit comfortably.

The `mega_balance` env exists specifically so future features that *don't* fit Uno can still land; we just gate them on `__AVR_ATmega2560__` or a build-flag.

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

## Non-negotiable design constraints (universal balance vision)

Reinforced during the 2026-05-18 bench session ([session record](archive/session_records/2026-05-18_BENCH_MOTOR_STICTION_DIAGNOSIS.md)). These are **hard constraints, not aspirations** — see [UNIVERSAL_BALANCE_BOT_VISION.md](UNIVERSAL_BALANCE_BOT_VISION.md) for the long-form rationale.

### The control philosophy: more / less, not absolutes

See [UNIVERSAL_BALANCE_BOT_VISION.md §Control philosophy](UNIVERSAL_BALANCE_BOT_VISION.md). The controller reasons in deltas — *"need more torque" / "need less torque" / "need to reverse"* — never in absolute PWM values. CHARACTERISE and online learning discover the **landmarks** (stiction floor, saturation point, K_motor); the balance loop navigates within them via gradients. Any code that asks "is PWM equal to 80?" or "is Kp set to 65?" is asking the wrong question — the right question is always "is the response we're getting more / less than we wanted, and what should the next command look like?"

### The rule

**The ONLY values that may be hardcoded are MCU pin assignments.** Everything else — every threshold, every gain, every limit, every dwell, every filter time-constant, every PWM value — must come from one of:

- **Boot-time automated calibration** (e.g. CHARACTERISE state — pulse PWM, learn stiction + saturation; mount-offset capture — average pitch over still window).
- **Online learning** during normal operation (e.g. RLS K_motor identifier; OnlineMountingEstimator; sensor-noise running variance).
- **Sensor primitives** (e.g. NDOF group delay is a chip property, gravity vector is the accelerometer mean).

If you find yourself writing a literal numeric value into source code that isn't a pin number, that's a **scope violation**. Examples that have all been violations during this project's history and must be removed: `Kp = 65`, `Kd = 38`, `Kd = 10`, `tilt_limit_deg = 8`, `stiction_min_pwm = 80`, `HELD lateral-gyro threshold 90`, `STUCK threshold 180`. *Some* of these still exist in the current source (2026-05-18) — they are stopgaps explicitly tagged for replacement, not features.

### Specific quantities that MUST be auto-measured

- **Stiction floor (per wheel)** — minimum PWM that produces wheel motion. Measured by Phase 2 CHARACTERISE state. **Per-wheel, not combined** — rubber bands shift, brushes wear unevenly, one wheel can have 30% more stiction than the other and this is the operator's typical bench reality.
- **PWM saturation point (per wheel)** — PWM beyond which response stops growing. Measured by CHARACTERISE.
- **Motor direction asymmetry** — forward vs reverse gain ratio per wheel. Discovered, not declared.
- **Initial PID gains** (Kp / Ki / Kd) — seeded from measured K_motor + closed-form PD-from-K_motor, refined online by RLS ([Phase 4.10](findings/dynamic_pwm_accel_learning.md)).
- **Balance point / mount offset** — captured by `MountingCalibration` and tracked online by `OnlineMountingEstimator`.
- **HELD vs INFLUENCED vs STUCK thresholds** — derived from motor-null-space residuals and the bot's own observed noise floor, not constants.
- **Tilt / FALLEN limits** — derived from the operating envelope the controller has demonstrated, not constants.
- **Sensor noise floor** — running variance of each axis, not assumed.

### What changes between sessions (and the bot must handle automatically)

- **Battery state** — voltage sag shifts effective stiction by 30-50 PWM.
- **Surface grip** — carpet, hardwood, rubber bands, drift wheels — change actuator response significantly.
- **Mount drift** — accelerometer bias creep, mechanical settling, payload addition.
- **Motor wear** — brush dust, axle friction, asymmetric wheel wear.
- **Per-wheel stiction divergence** — rubber bands fall off, friction tape peels.

None of these may require the operator to recompile, edit a config file, or hand-tune a knob. The framework's job is to notice and adapt — either continuously, or on operator-triggered re-characterisation via a single serial command.

### Three operating regimes the framework must distinguish

Captured from operator session 2026-05-18 ([backlog row 13](findings/operator_ideas_backlog.md)):

- **BALANCING** — bot self-correcting via wheel torque. The autonomous state.
- **INFLUENCED** — operator places bot at a new pitch but no motor-null-space motion (lift, lateral shove, rotation). Don't disable balance — absorb the new state and keep going.
- **HELD** — motion in motor-null-space axes. External force dominating. Motors off until quiet.
- **STUCK** — output saturated but no motion. External restraint or wheel jam. Motors off immediately.

A naive "HELD = any lateral gyro" detector conflates all four. The proper detector projects IMU motion into the motor-null subspace (Phase 2.7 — research delivered [research_motor_null_space_handling_detection.md](findings/research_motor_null_space_handling_detection.md)).

### Observable + diagnosable from serial

The state machine stays small. Every state name appears in `s` status output and `[state] -> X` transition logs. No hidden modes. No magic numbers in production. If the operator can't see why the bot just made a decision, that's a scope violation.

### Compile-time regression test

Any future PR that adds a literal numeric constant outside of pin assignments must justify why it cannot be measured. CI will eventually grep for the pattern and fail builds that violate this — *that* is the goal state, not a future stretch goal.

---

## Current scope violations — audit (2026-05-18, updated PM-evening Phase 4.10c landed; re-tagged 2026-05-19 for platform bifurcation)

This table lists every hardcoded tuning value still present in the firmware and the *concrete* replacement that retires it. Each violation must have an active replacement plan. **No exceptions for "it works well enough" on the Mega-universal path.** The 2026-05-18 PM bench session demonstrated empirically that even reasonable-looking hardcoded gains (Kp=50 inherited from the reference .ino) produce destructive oscillation on a different bot. **Hand-tuned constants do not generalise — that is the entire point of the universal stack.**

**Pivot caveat (2026-05-19)**: this audit governs the **Mega-universal path** (`src/applications/balancing_robot/`). The new **Uno-minimal program** (`src/applications/balancing_robot_uno/`, scaffold landed 2026-05-19 commit c3c0c6b) **intentionally** uses hardcoded PID + PWM constants generated by the offline Python brute-force tuner (`tools/sim/brute_tune.py`). Constants in the Uno minimal program are not violations of this audit — they are a design choice of that build. See the [Platform bifurcation](#platform-bifurcation-2026-05-19-clarified-2026-05-26--mega-universal-vs-uno-minimal) section above.

**Status legend**: ✅ retired (no longer in source) · 🔄 partially retired (mechanism in place, value now derived) · ⏳ still present, awaiting replacement · `[mega]` row applies on Mega-universal path · `[deferred-to-mega]` Uno-flash workaround that no longer needs retiring on Uno (Uno path is intentionally hardcoded; Mega path still needs the derivation) · `[uno-intentional]` design choice in Uno-minimal program, only a violation if it shows up in the Mega tree.

| Status | Violation | Location | Why it must go | Replacement |
|---|---|---|---|---|
| ✅ | `kDefaultInitialKp = 50.0f` | (was `balance_app.cpp:75` / `main.cpp:323`) | PID gain depends on chassis mass / motor torque / wheel radius — none universal | **BOOTSTRAP state** (Phase 4.10c landed 2026-05-18 PM) measures K_motor from controlled pulses, derives Kp = ω_n²/K_motor analytically. PID gains never set from constants in main.cpp; BOOTSTRAP pushes them on success. |
| ✅ | `kDefaultInitialKi = 2.0f` | (was `balance_app.cpp:76`) | Same | Same; Ki = 0.05·Kp from same K_motor measurement |
| ✅ | `kDefaultInitialKd = 20.0f` | (was `balance_app.cpp:77`) | Same | Kd = 2ζω_n/K_motor |
| ✅ | `R` command gains 65/12/38 | (was `main.cpp:386`) | Legacy .ino-flavour hand-tune | Removed. Operator path is `c` (capture+bootstrap) or `b` (manual bootstrap from IDLE). |
| ✅ | FALLEN restart `±80 PWM` clamp | (was `balance_app.cpp:710`) | Hardcoded balance-mode PWM cap | Removed. FALLEN short-press now re-routes through BOOTSTRAP, so K_motor is re-measured + gains re-derived under current battery/surface conditions. |
| ✅ | Relay tuner amplitude = 150 | (was `main.cpp:103`) | Auto-tuner perturbation magnitude | **Deleted.** RelayFeedbackStrategy excluded from balance build via platformio.ini build_src_filter; main.cpp uses a 10-line `NoOpStrategy` stub. Saves ~1.3 KB flash; AUTO_TUNE state kept in enum but unreachable from public API. |
| ✅ | `tune_max_duration_sec = 30` | (was `balance_app.cpp:86`) | Tuner timeout | Same — relay tuner is gone. |
| 🔄 | `cfg.tilt_limit_deg` (was 8.0f override) | `main.cpp` no longer overrides | Operating envelope of recovery, not universal | Override removed 2026-05-18 PM; falls back to cfg default 10° at `balance_app.cpp` `kDefaultTiltLimitDeg`. Derived value (observed envelope × 1.5) still pending — needs balance data. |
| ⏳ `[mega]` | `kDefaultTiltLimitDeg = 35.0f` | `safety.cpp:10` | Tip-over angle is geometric (CoG above wheel axis) | Compute from accel quaternion at the pitch where lateral accel hits 0.5 g |
| ⏳ `[mega]` `[deferred-to-mega]` | `SOFT_ZONE_DEG = 1.0f` | `balance_app.cpp:477` | Phase 2.6 gain scheduling needs to know noise floor | Derive: 3 × LP-filtered std-dev of pitch_deg over the most recent quiet RUN window. Uno minimal program does not use gain scheduling. |
| ⏳ `[mega]` `[deferred-to-mega]` | `SAT_THRESHOLD_PWM = 180` | `balance_app.cpp:520` | STUCK detector PWM threshold | 0.7 × measured saturation_pwm from CHARACTERISE. Uno minimal program does not implement STUCK detection. |
| ⏳ `[mega]` `[deferred-to-mega]` | `STUCK_GYRO_DPS = 5.0f` | `balance_app.cpp:521` | STUCK no-motion threshold | 3 × baseline gyro noise from CHARACTERISE Phase 2.1. Uno minimal: not applicable. |
| ⏳ `[mega]` `[deferred-to-mega]` | `STUCK_TIMEOUT_MS = 1500` | `balance_app.cpp:522` | Magic latency | Derive: 5 × expected PID response time = 5 × (2π / ω_n). Uno minimal: not applicable. |
| ⏳ `[mega]` `[deferred-to-mega]` | `Phase 2.5: cmd_mag < 20` | `balance_app.cpp:421` | "Quiet motors" threshold | 0.5 × measured stiction_pwm. Uno minimal program does not detect HELD. |
| ⏳ `[mega]` `[deferred-to-mega]` | `Phase 2.5: gyro > 30 dps` | `balance_app.cpp:421` | "Fast rotation" threshold | 5 × baseline gyro noise. Uno minimal: not applicable. |
| ⏳ `[mega]` `[deferred-to-mega]` | `Phase 2.5 dwell = 100 ms` | `balance_app.cpp:424` | Magic latency | 2 × PID sample period × some sigma multiplier. Uno minimal: not applicable. |
| ⏳ `[mega]` `[deferred-to-mega]` | HELD `a_dev_lpf_ > 6.0f` | `balance_app.cpp:421` | Lift-detect threshold | 3 × baseline accel noise (BNO055 LIA — Phase 4.6.5). Uno minimal: no HELD detection. |
| 🔄 `[mega]` | `BOOTSTRAP_FREEZE_MS = 5000` (RLS warmup) | `balance_app.cpp:597` | RLS warmup window for natural-disturbance ID | Bypassed when BOOTSTRAP succeeds (PlantIdentifier seeded with measured K, adaptive_active immediately true). Still applies if BOOTSTRAP fails and operator force-runs anyway. |
| ⏳ `[mega]` | Absolute pitch kill = ±20° | `main.cpp:411` | Magic safety override | 0.8 × derived tilt_limit_deg |
| ⏳ `[mega]` | `online_est max_deviation = 5°` | `online_mounting_estimator.cpp` | Magic clamp | Derive: 3 × pitch oscillation amplitude during RUN |
| ⏳ `[mega]` | `online_est LPF tc = 8 s` | `main.cpp:338` | Filter time constant | Derive from observed pitch dynamics — should match the PID closed-loop time constant |

**Phase 4.10c retirement summary (2026-05-18 PM evening)**: 7 of 21 violations retired (33%). The remaining 14 violations were *blocked by* the PID-gains violations — they all need a balancing bot to derive their measurements, and the bot couldn't balance without measured gains. Now that BOOTSTRAP lands a controlled balance regime, the next sessions can systematically retire the remaining rows by replacing each constant with the measurement it depends on.

**2026-05-19 platform-bifurcation re-tagging**: most remaining rows are tagged `[mega]` because they live in the universal stack — the Uno-minimal program does not host these features at all (no STUCK detection, no HELD detector, no gain scheduling, no online mount estimator). On Uno, the equivalent "constants" are simply absent because the corresponding logic was deleted. On Mega, the replacement plans below remain authoritative.

### What "active replacement plan" means

Every row above has a concrete derivation listed. **A row without a plan is a bug in the audit, not an acceptable violation.** If a value cannot be derived from a measurement, the value should not exist — find a different control architecture.

### What's NOT a violation

- `0.0f` initializers, `memset` clears — these are zeros, not tunings
- Sample-rate-derived constants (`pid_sample_ms = 5` is the timer period, derived from MsTimer2 hardware)
- I²C/UART baud rates (protocol-level, not tuning)
- Loop iteration limits in algorithms (e.g., `for (int i = 0; i < N; ++i)` where N is a structural count, not a tuning value)
- The number of pulses in CHARACTERISE (6 is a structural choice in the algorithm, not a tuning)

### Immediate next step

The single most impactful violation to retire is **PID gains via BOOTSTRAP** (`kDefaultInitialKp/Ki/Kd`). Without measured gains, no amount of mount-offset adaptation or HELD heuristics can stop the bot from oscillating to failure. Implementation: Phase 4.10c — see [bootstrap_protocol_unstable_plant.md](findings/bootstrap_protocol_unstable_plant.md).

After BOOTSTRAP lands, every other violation in the table becomes addressable because the bot will actually balance long enough to collect the data needed for the remaining derivations.

---

## Process doctrine — how NOT to get stuck

This project has been derailed multiple times by **iterating on hardcoded constants instead of building the measurement system that would make those constants unnecessary**. The pattern is seductive: a number looks slightly wrong, you change it, the bot behaves slightly differently, you change it again, hours pass. Each tweak feels like progress. Cumulatively it is a random walk that produces session-specific patches and zero progress toward the universal vision.

Codifying this lesson — bench-session 2026-05-18 distilled the rule the hard way:

1. **If you change a numeric constant in source code, you have just made the framework less universal, not more.** The "right" number doesn't exist. Tomorrow's battery, surface, or rubber-band changes invalidate it.
2. **When tempted to tweak a value, instead ask: what measurement would replace this constant?** Then build the measurement. *Every* such measurement is in the design backlog already ([`findings/operator_ideas_backlog.md`](findings/operator_ideas_backlog.md)) — pick the smallest one and implement it.
3. **Safety bandaids are a category exception, but only briefly.** A kill-switch or STUCK detector with a hardcoded threshold can land *as long as* it's tagged with a TODO pointing to the measurement-derived replacement on the backlog. The hardcoded form must not survive to a release.
4. **The lessons-learned doc ([`archive/LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md`](archive/LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md)) is mandatory reading before any bench session.** It documents this exact failure mode. If you find yourself about to start a bench iteration on gains, re-read it first.
5. **Bench-session-summary documents are not deliverables.** Producing a beautifully-formatted record of an unproductive session is itself the trap. Deliverables are *measurement-replacing implementations* that survive past the session.

Cross-references: [roadmap.md §Sequencing discipline](roadmap.md), [findings/operator_ideas_backlog.md](findings/operator_ideas_backlog.md), [archive/LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md](archive/LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md).

---

## Why this is its own project (not a sub-module of `flight_controller/`)

3D orientation is foundational and reusable. Embedding it inside the flight controller would couple every application that needs orientation to the flight controller's release cadence and feature set. By keeping it standalone:

- The sister `flight_controller/` project can depend on a stable orientation interface without owning its calibration UX.
- Educational and budget builds (Nano + MPU6050) can use the same architecture as research-grade builds (Teensy 4.1 + BNO085 + WiFi).
- 3D scanning, gimbal, and AR/VR applications — which have nothing to do with flight — can build on it directly.
- The framework can evolve its own sensor matrix, dashboard, and calibration tooling without affecting downstream consumers.

---

## Current state (2026-05-20)

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
- `src/applications/balancing_robot/` skeleton builds under `mega_balance` env
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

*Last updated: 2026-05-26 (wave 6: Uno IMU selection wired via `#ifdef USE_BNO085` / `#else USE_BNO055` + hard `#error` on USE_BNO085+Uno [memory-tier principle made concrete] + cal-blob slot widened to 72 B / version 0x03 for future BNO085 SH-2 FRS support; old v2 22-byte blobs reject-on-load cleanly. Prior 2026-05-26: Mega-vs-Uno capability-tier clarification: IMU choice is orthogonal to MCU choice; added SETUP-vs-OPERATIONAL mode framing and value-robustness principle). This document is the source of truth for what the framework is and is not. When in doubt, scope it against this file; if the answer isn't here, raise it in a session and update accordingly.*
