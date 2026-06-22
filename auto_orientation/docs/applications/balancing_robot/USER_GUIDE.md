# Self-Balancing Robot — User Guide (Mega tier)

The Phase 4 reference application — a two-wheel inverted-pendulum robot whose inner PID gains are **measured at boot** by short PWM pulses, not hand-tuned. This document covers the `mega_balance` build (Arduino Mega 2560 + BNO055 + L298N + 2× TT motors + wheel encoders).

For the lean Uno tier (hardcoded gains + guided P→I→D tuning bench) see `../balancing_robot_uno/`. The two share design DNA but the Mega tier is what this guide documents.

---

## STATUS BANNER (read this first)

> **The Mega balance loop has NOT yet successfully balanced on hardware.**
>
> - Compile + unit-test: PASS. The BOOTSTRAP / RUN / HELD / FALLEN state machine, the Phase 4M.2 K cross-check, the Phase 4M.13 velocity/position outer loop and the Phase 4M.14 analytical outer-loop gain derivation all build and pass `native_test`.
> - **Hardware bench: PENDING.** Last bench attempt (2026-05-18 PM, Uno predecessor with the same control stack) twitched once and fell within ~1 s. The cascade of issues that produced that result is documented in [`../../KNOWN_ISSUES.md`](../../KNOWN_ISSUES.md) (most are now addressed in code but the result has not been re-verified on the bench).
> - Workstream G (the bench-tuning protocol the operator follows when they do attempt a bring-up) is documented in [`../../findings/workstream_g_bench_protocol_2026-05-21.md`](../../findings/workstream_g_bench_protocol_2026-05-21.md).
> - A dedicated `FIRST_SUCCESS_MEGA.md` will land in wave 3 of this doc effort to capture the first hardware-validated bring-up walkthrough end-to-end. Until then, this guide describes **what the firmware does** — not what the firmware has *proven* it can do on a physical bot.
>
> Use the guide to understand the operating model and the operator surface; **do not** treat it as evidence that the Mega bot balances today.

---

## Table of contents

1. [What this firmware actually does](#what-this-firmware-actually-does)
2. [Hardware](#hardware)
3. [First flash](#first-flash)
4. [Power-on flow](#power-on-flow)
5. [State machine](#state-machine)
6. [Operator command surface](#operator-command-surface)
7. [What the firmware does NOT do (any more)](#what-the-firmware-does-not-do-any-more)
8. [Where to go next](#where-to-go-next)

---

## What this firmware actually does

The Mega balance app is built around a single, real state machine in `src/applications/balancing_robot/balance_app.{h,cpp}`. The lifecycle is:

```
        IDLE  ─c(or short-press)─▶  CAPTURE_MOUNTING ─stillness OK─▶  BOOTSTRAP ─K OK─▶  RUN
         ▲                                                                                │ │
         │                                                                          collision/handling
         │                                                                                │ │
         │                                                              ┌────────────────┘ │
         │                                                              ▼                  │
         │                                                            HELD ──quiet+level──┘
         │                                                              │
         └────────  (operator abort 'a' from any state) ◀────────────  any failure
```

There is also a sticky `FALLEN` branch from RUN, but it is **off by default on this build** (`USE_BALANCE_FALL_DETECTION` is undefined — the PID is allowed to run forever and the bot lies on its side rather than entering FALLEN). Two Mega-only states extend the lifecycle: `CHAR_ACT` (one-shot stiction-floor sweep — `k`) and `PWM_DISCOVERY` (encoder-based PWM range auto-discovery — `p`).

The Mega `g` serial command emits a flat machine-parsable telemetry line per request; the Uno-style "fast LED blink during AUTO_TUNE" is gone (that state is unreachable — see [§7](#what-the-firmware-does-not-do-any-more)).

Gains come from **BOOTSTRAP**, not relay-feedback. BOOTSTRAP applies four short symmetric ±PWM pulses (±180, ±180, ±240, ±240, 150 ms each, separated by 400 ms cooldowns), measures the gyro pitch-rate response per pulse, computes `K_motor = |Δω/τ| / pwm_total` per pulse and means them. On Mega the encoder-derived `K_encoder` is computed over the same pulses and the two must agree to within ±30% (Phase 4M.2) or BOOTSTRAP aborts with `k_disagreement`. The inner PID gains are then derived in closed form (`Kp = ω_n²/K_motor`, `Kd = 2ζω_n/K_motor`, `Ki = 0.05·Kp`). On encoder builds the outer position/velocity loop's `K_POS`/`K_VEL`/`POS_LEAK` are also auto-derived analytically at FINALISE (Phase 4M.14).

The RUN loop runs a continuous scalar RLS plant identifier (5 %/s gain ramp, σ-modification) plus an online mounting-offset tracker, indefinitely.

---

## Hardware

The full BOM + ASCII wiring lives in [`HARDWARE_SETUP.md`](HARDWARE_SETUP.md). Minimum for the Mega tier:

| Item | Notes |
|------|-------|
| Arduino Mega 2560 | Required — `mega_balance` env. Uno tier is a separate build. |
| BNO055 (Adafruit) | Default. Set `-D BNO055_NO_EXT_CRYSTAL` for non-Adafruit clones. |
| L298N | ENA=5, IN1=7, IN2=6, ENB=10, IN3=8, IN4=9 (IN1/IN2 swapped to invert direction at driver level). |
| 2× TT DC motors | Yellow gearbox style. |
| 2× quadrature wheel encoders | L A/B = pins 18/19 (INT5/INT4); R A/B = 2/3 (INT0/INT1). Mega-only; Uno has too few INT pins. |
| Button on D4 | `INPUT_PULLUP`, momentary, leg → GND. |
| 18650 + boost to 5 V | See `HARDWARE_SETUP.md` for the bench rig that's been used. |

Pins 20/21 are reserved for I²C (BNO055) and must not be repurposed for encoders.

---

## First flash

```bash
cd auto_orientation
pio run -e mega_balance -t upload
```

The `mega_balance` env defines:

- `USE_BALANCING_ROBOT` — selects the balance app in `src/main.cpp`'s dispatcher.
- `USE_BNO055` — the IMU driver.
- `USE_WHEEL_ENCODERS` — Mega-only; enables encoder cal (`e`), PWM discovery (`p`), the K cross-check, and the outer position/velocity loop.
- `USE_BALANCE_HELD_DETECTION` — RUN→HELD on operator handling / collision.
- `USE_BALANCE_AUTO_BOOTSTRAP` — power-on prop-and-go (see [§4](#power-on-flow)).

The relay-feedback tuner is **explicitly excluded** from the build via the `-<control/tuners/relay_feedback.cpp>` filter in `platformio.ini`. A `NoOpStrategy` satisfies the `AutoPIDTuner` constructor; no operator gesture reaches `AUTO_TUNE`.

To disable prop-and-go (keep IDLE until you manually press `b`), build with `-U USE_BALANCE_AUTO_BOOTSTRAP`. Useful when you need to position the bot in a safe test area before motors engage.

---

## Power-on flow

```
power on
   │
   ▼
"B"   (boot banner, 115200 baud)
   │
   ▼
IMU.begin()  ────fail────▶  "BF" + halt
   │ OK
   ▼
restore BNO055 cal from EEPROM 0x000  ──fail──▶  run cal wizard (see CALIBRATION_WORKFLOW.md)
   │ OK
   ▼
load actuator stiction floor (EEPROM 0x210, optional)
load encoder cal           (EEPROM 0x220, Mega only — applies wheel radius)
load PWM discovery bounds  (EEPROM 0x230, Mega only — overrides stiction floor)
load mount offset          (EEPROM 0x200, optional)
   │
   ▼
"READY"
   │
   ▼
USE_BALANCE_AUTO_BOOTSTRAP set AND mount loaded?
   │ yes                                              │ no
   ▼                                                  ▼
delay 2 s (operator clears hands)            stay in IDLE — operator
read pitch                                   triggers via 'c' / 'b' /
|pitch − mount| < 5° ?                       button.
   │ yes              │ no
   ▼                  ▼
enter_bootstrap()    print "stale_mount p=… m=…"
                     stay in IDLE — operator
                     recaptures with 'c' or
                     overrides with 'b'.
```

The grace period exists because the IMU read inside `setup()` happens immediately after `READY`; without the 2 s pause an operator's hand on the bot would be sampled as a 10° tilt and prop-and-go would fall through to the stale-mount branch.

---

## State machine

| State | Entered by | Exits to | Notes |
|-------|------------|----------|-------|
| `IDLE` | power-on / abort / failed BOOTSTRAP / completed `e`/`p`/`k` | CAPTURE_MOUNTING (`c`/short-press), BOOTSTRAP (`b`/long-press/auto-BS), CHAR_ACT (`k`), PWM_DISCOVERY (`p`, Mega) | Motors stopped. PID quiet. No online_est update. |
| `CAPTURE_MOUNTING` | `c` / short-press from IDLE | BOOTSTRAP on stillness (auto-chain), IDLE on jitter / abort | Welford running variance on pitch; succeeds when σ ≤ `capture_pitch_var_deg` (0.5°) inside `capture_duration_ms` (2000 ms). Mean pitch becomes the mount reference. |
| `BOOTSTRAP` | `b` / long-press / capture success / auto-boot / FALLEN→short-press | RUN on success, IDLE on every failure_reason | ~2.5 s of motor activity: 300 ms baseline + 4 pulses × (150 ms PULSE + 400 ms COOLDOWN). Sees [§5.1 failure_reason table](#51-bootstrap-failure_reason-codes). |
| `RUN` | BOOTSTRAP success | HELD (collision / handling / encoder stall), IDLE (abort, stuck-motor timeout), FALLEN (only if `USE_BALANCE_FALL_DETECTION` defined — off here) | Inner pitch PID + (Mega) outer position/velocity nudge. Continuous RLS adapts gains. |
| `HELD` | RUN exit on collision (`held_entry_reason=1`), encoder stall (`=2`), operator handling (`=3`) | RUN on quiet+level for 200 ms (40 ticks), IDLE on abort | Motors stopped. No HELD timeout — bot stays HELD indefinitely if left on a shelf. |
| `FALLEN` | RUN exit on tipover (only when `USE_BALANCE_FALL_DETECTION` is defined — disabled in `mega_balance`) | Operator-only: short-press → BOOTSTRAP; `c` → CAPTURE_MOUNTING; long-press → abort to IDLE | Sticky. The current Mega build does NOT enter FALLEN — when fall detection is off, the bot just lies on its side with the PID still computing (motors soft-cut above ±25° pitch). |
| `CHAR_ACT` | `k` from IDLE | IDLE | One-shot stiction-floor sweep (Phase 2). Result saved to EEPROM 0x210 and applied to live driver. |
| `PWM_DISCOVERY` | `p` from IDLE (Mega only) | IDLE | Bot must be lifted off the ground. Ramps PWM 0→255 in 5-PWM steps, watches encoder velocity, records MIN (stiction) + MAX (saturation) to EEPROM 0x230. |
| `AUTO_TUNE` | **unreachable** | n/a | Enum value retained for ABI; the relay-feedback handler is removed. See [§7](#what-the-firmware-does-not-do-any-more). |

### 5.1 BOOTSTRAP failure_reason codes

These come from `BootstrapResult.failure_reason` in `balance_app.h`. Every value is also surfaced in `TROUBLESHOOTING.md` with a diagnostic remedy.

| Code | Name | What it means (operator-facing) |
|------|------|----------------------------------|
| 0 | `ok` | Pulses produced a clean K; gains derived; transitioned to RUN. |
| 1 | `pitch_out_of_range` | Pitch left ±10° during baseline, or ±15° during a pulse. Bot wasn't propped upright, or tipped mid-BOOTSTRAP. |
| 2 | `no_response` | Fewer than `N_PULSES / 2 = 2` pulses produced detectable Δω. Motors disconnected, axis swapped, stiction too high, or battery sagging. |
| 3 | `k_out_of_bounds` | Measured K was clamped by ≥ 50 % by `PlantIdentifier::seed_k_motor()`. Almost always means measurement was garbage (motors didn't actually move). |
| 4 | `user_abort` | Operator sent `a` (or `t`/long-press) during BOOTSTRAP. |
| 5 | `collision` | Three-gate LIA detector latched during baseline or a pulse — external impact contaminated the measurement. |
| 6 | `baseline_noisy` | Peak-to-peak gyro in the 300 ms baseline exceeded 5 dps. Operator was still touching the bot. |
| 7 | `k_disagreement` | **Mega/encoder only.** Gyro-derived K and encoder-derived K differ by > 30 %. Wheel slip, mechanical bind, or encoder fault. |
| 8 | `pwm_discovery_timeout` | Reserved for PWM_DISCOVERY result struct — never set on BOOTSTRAP. |

A separate field, `posgains_failure_reason_`, reports the outer-loop gain derivation outcome (Mega only):

| Code | Meaning |
|------|---------|
| 0 | Outer-loop gains were derived and pushed to `PositionLoop`. |
| 9 | `derived_gains_oob` — at least one of `K_POS`/`K_VEL`/`POS_LEAK` fell outside its sanity envelope, OR no trusted wheel radius. The conservative `*_FALLBACK` gains (6.0 / 3.0 / 0.999) are used. **BOOTSTRAP still succeeds — this is a non-fatal "degraded session".** Re-check encoder calibration. |

### 5.2 HELD entry reasons

`held_entry_reason_` is stamped each time RUN→HELD fires:

| Code | Name | Trigger |
|------|------|---------|
| 0 | `NONE` | Cleared on RUN entry. |
| 1 | `COLLISION` | Three-gate LIA detector latched during RUN. |
| 2 | `GYRO_ANOMALY` | Encoder stall (commanded PWM > 100 but no wheel motion for 300 ms). Mega only. |
| 3 | `OPERATOR_HANDLING` | Either `ext_motion` (cmd quiet + pitch gyro > 30 dps) or `lift_detected` (|accel|−g > 6 m/s²). |

---

## Operator command surface

The Mega serial command set (`src/main.cpp` loop, 115200 baud). The button on D4 is debounced in firmware: SHORT < 1000 ms, LONG ≥ 1000 ms.

| Input | State semantics | Notes |
|-------|-----------------|-------|
| `c` (or short-press) | `IDLE` → CAPTURE_MOUNTING. `HELD` → force-resume to RUN (skip 200 ms quiet+level dwell). `FALLEN` → BOOTSTRAP. Other states: no-op. | Capture takes ≥ 2 s; bot must be still (σ_pitch ≤ 0.5°). |
| `t` (or long-press) | Anywhere: request abort. `IDLE` specifically: skip CAPTURE_MOUNTING and enter BOOTSTRAP with the loaded mount offset. | The IDLE+long-press path used to enter AUTO_TUNE; that wiring is gone. |
| `a` | Anywhere: request abort. State machine converts on the next tick. | Universal emergency exit. |
| `s` | Print one-shot status line: `<state> <pitch> <mount> <output> <stiction>`. | Drain-on-demand, never periodic. |
| `g` | Print one-shot telemetry line: `G,<millis>,<pitch>,<pitch_sp>,<wheel_vel_mps>,<position_m>,<nudge_deg>,<k_pos>,<k_vel>,<pos_leak>`. | Workstream G machine-parsable telemetry. On Uno-class builds without encoders the four outer-loop fields are zero. |
| `b` | `IDLE` → BOOTSTRAP. Otherwise: no-op. | Same as long-press from IDLE; provided so the operator can drive from a non-button console. |
| `k` | `IDLE` → CHAR_ACT. | One-shot stiction-floor sweep (Phase 2). Result auto-saves to EEPROM 0x210 if 30 ≤ stiction ≤ 200 PWM. |
| `e` | Blocking encoder-calibration wizard. Mega only. | Bot in IDLE, motors off. Operator hand-rolls the bot 1.000 m. Press button to capture; `a` to abort. Saves CPM L/R + radius to EEPROM 0x220. |
| `p` | `IDLE` → PWM_DISCOVERY. Mega only. | Operator MUST lift the bot off the ground before issuing. |

State transitions and per-pulse telemetry are drained from the ISR side via `drain_state_log()` and `drain_pulse_log()`; the `[state] -> X` and `bs#…` / `ch#…` / `pd#…` lines are emitted from `loop()` so `Serial.print` never runs in interrupt context.

The boot path also prints `READY`, `sv m=…` after a mount save, `sv pd min=… max=…` after PWM discovery save, and `stale_mount p=… m=…` when prop-and-go declines to auto-BS.

---

## What the firmware does NOT do (any more)

If you have read older docs or archive notes, the following are gone:

- **No relay-feedback `AUTO_TUNE`.** The state enum value is retained for ABI/telemetry compatibility, but no operator gesture reaches it, the handler is empty, and `relay_feedback.cpp` is excluded from the build (`platformio.ini`). The replacement is BOOTSTRAP + RLS plant-ID + 5 %/s gain ramp.
- **No `RELAY_AMPLITUDE_PWM` knob.** That constant lived in `relay_feedback.cpp`; the file isn't built. There is nothing to twiddle.
- **No `USE_TUNER_RELAY` build flag.** The current build defines `USE_BALANCE_HELD_DETECTION` and `USE_BALANCE_AUTO_BOOTSTRAP` (Mega) — not the relay flag.
- **No "bolt-flash-press-twice-it-stands" story.** That described an envisioned Phase 4 UX that the bench attempts never reached. The honest framing is in the STATUS BANNER above.
- **No `AutoPIDTuner` / `RelayFeedbackStrategy` activity.** `AutoPIDTuner` is constructed with a `NoOpStrategy` purely to satisfy the `BalanceApp` constructor signature.
- **No `SAFE_FALL` LED wail.** The legacy `SAFE_FALL` enum is replaced by `FALLEN`, and FALLEN is gated by `USE_BALANCE_FALL_DETECTION` (off in `mega_balance`). The bot doesn't enter a sticky fall state by default — the PID computes through tipover with motors soft-cut above ±25° pitch (see `step_run_` in `balance_app.cpp`).
- **No EEPROM-persisted PID gains.** Gains are re-derived from BOOTSTRAP every session. The only persisted things are physical/hardware properties: BNO055 sensor cal (0x000), mount offset (0x200), actuator stiction (0x210), encoder cal (0x220), PWM-discovery bounds (0x230). See [`CALIBRATION_WORKFLOW.md`](CALIBRATION_WORKFLOW.md) for the slot map.

---

## Where to go next

- [`CALIBRATION_WORKFLOW.md`](CALIBRATION_WORKFLOW.md) — full procedure for `c` / `e` / `p` / `b`, EEPROM persistence semantics, when to re-run each step.
- [`TROUBLESHOOTING.md`](TROUBLESHOOTING.md) — symptom → fix index keyed off the real failure modes (every `failure_reason`, every `held_entry_reason_`, stuck-motor timeout, mount-recapture criteria, encoder pulse loss).
- [`HARDWARE_SETUP.md`](HARDWARE_SETUP.md) — wiring + power rig.
- [`../../KNOWN_ISSUES.md`](../../KNOWN_ISSUES.md) — the unresolved-on-hardware items the STATUS BANNER refers to.
- [`../../findings/workstream_g_bench_protocol_2026-05-21.md`](../../findings/workstream_g_bench_protocol_2026-05-21.md) — the verification-not-tuning protocol the operator follows during a bench session (this is the document a `FIRST_SUCCESS_MEGA.md` will eventually consume).
- [`../../findings/bootstrap_protocol_unstable_plant.md`](../../findings/bootstrap_protocol_unstable_plant.md) — design rationale (read §11 AS-BUILT first; the earlier staged-protocol sections describe a design that was collapsed, not what runs).

---

*Last updated: 2026-06-21 — full rewrite to match the BOOTSTRAP/RUN/HELD state machine. AUTO_TUNE / SAFE_FALL / `RELAY_AMPLITUDE_PWM` / "press-it-twice-it-stands" content removed; STATUS BANNER added; operator command surface and `failure_reason` enums documented exhaustively.*
