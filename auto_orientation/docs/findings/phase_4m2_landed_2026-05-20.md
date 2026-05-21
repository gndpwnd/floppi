# Phase 4M.2 Landed — Encoder-Driven K_motor Cross-Check in BOOTSTRAP

**Agent:** ao-phase-4m2@floppi:1
**Date:** 2026-05-20
**Workstream:** F.1 (first half of Workstream F, architecture_plan_2026-05-20.md §2.5 / §4)
**Status:** landed, builds green

---

## What the cross-check does

BOOTSTRAP applies a fixed sequence of four alternating ±PWM pulses and, for
each pulse that clears the gyro quality gate, measures `K_gyro` = the chassis
pitch-rate response per unit PWM: `(|Δω_gyro|/τ) / pwm_total`.

Phase 4M.2 adds a **second, independent** estimate from the wheel encoders.
For each of the *same* pulses:

- `bs_pulse_start_vel_` snapshots the mean wheel angular velocity (dps) at
  pulse start (normally ~0 after the 400 ms cooldown).
- At cooldown entry, `K_encoder = (|Δv_wheel|/τ) / pwm_total` — identical
  algebraic form to `K_gyro`, with the chassis pitch rate Δω replaced by the
  mean wheel angular rate Δv.

`K_encoder` is accumulated only inside the existing `if (passed)` branch, so
`K_gyro` and `K_encoder` are means over an **identical pulse set**.

In FINALISE, before the PlantIdentifier is seeded, the two means are compared:

```
k_rel = |K_gyro - K_encoder| / max(K_gyro, K_encoder)
if (k_rel > BOOTSTRAP_K_DISAGREE_FRAC)  ->  abort
```

On disagreement: `motors_.stop()`, `failure_reason = 7`, `converged = false`,
`enter_state_(IDLE)`. On agreement: BOOTSTRAP proceeds exactly as before
(seed K, derive gains, enter RUN).

A `PulseLog` summary record (sentinel `pulse_idx = 0xFD`) is stamped on **both**
paths so the bench `s`/pulse-log drain shows the operator both K values
(`gyro_start_x10` = K_gyro×10, `metric_x10` = K_encoder×10, `thr_x10` =
threshold percent, `passed` = 1 agree / 0 disagree) — bench telemetry per task
step 5.

## The 30% threshold rationale

`BOOTSTRAP_K_DISAGREE_FRAC = 0.30f`. K_gyro and K_encoder are not the same
physical gain — one responds to the chassis pitch moment of inertia, the other
to the wheel+gearbox moment — so a steady 10–20% offset between them is
expected and benign even on a perfectly healthy drivetrain. 30% is the value
called out in `research_wheel_encoders_mega_2026-05-19.md` §6 ("disagree by
more than ~30%"). It is deliberately loose: a genuine wheel-slip or
mechanical-binding regime blows the ratio well past 2× (bench observation in
the research doc), so the gate sits in clear air between the benign band and a
real fault — no realistic false-positive surface.

## The new failure_reason value

`BootstrapResult::failure_reason == 7` → **k_disagreement**.

Existing reasons: 1=pitch_out_of_range, 2=no_response, 3=k_out_of_bounds,
4=user_abort, 5=collision, 6=baseline_noisy. `7` is the next free value (the
adjacent `PwmDiscoveryResult::failure_reason` already uses `8` for
pwm_discovery_timeout, so `7` is unambiguous and unused). The enum-comment in
`balance_app.h` documents `7` as Mega-only / encoder-dependent.

## Encoder gating

All Phase 4M.2 code is wrapped in `#ifdef USE_WHEEL_ENCODERS` (the macro
`wheel_encoder.h` and the rest of the balance stack already use):

- new members `bs_pulse_start_vel_`, `bs_k_enc_sum_` (balance_app.h)
- constructor initializers (balance_app.cpp)
- reset block in `enter_state_(BOOTSTRAP)` — also `reset_ticks()` on both
  encoders so the windowed velocity samples reference a clean origin
- pulse-start velocity snapshot in the pulse window
- per-pulse K_encoder accumulation in the cooldown block
- the FINALISE comparison + abort

With encoders disabled (uno_balance) BOOTSTRAP behaves **exactly** as before —
gyro-only — and `failure_reason = 7` is unreachable.

## Lines changed

- `balance_app.h`: +`BOOTSTRAP_K_DISAGREE_FRAC` constant + doc block (inside
  `USE_WHEEL_ENCODERS`); failure_reason enum-comment extended for `7`; +2
  members `bs_pulse_start_vel_`, `bs_k_enc_sum_` (gated).
- `balance_app.cpp`: +2 constructor initializers (gated); +reset block in
  `enter_state_(BOOTSTRAP)`; +pulse-start velocity snapshot; +K_encoder
  accumulation; +FINALISE cross-check/abort block (~40 LOC, all gated).
- No other files touched. Collision detection, 3-gate detector, PWM_DISCOVERY,
  the `F()` shim and `held_entry_reason_` were not modified.

## Build verification

| Env          | Result   | Flash               | RAM                |
|--------------|----------|---------------------|--------------------|
| mega_balance | SUCCESS  | 37598 B (14.8%)     | 1476 B (18.0%)     |
| uno_balance  | SUCCESS  | 30222 B (93.7%)     | 1273 B (62.2%)     |

`uno_balance` flash/RAM are **byte-identical** to the pre-change baseline
(verified via `git stash` build) — zero regression, confirming the cross-check
is fully `#ifdef`'d out on the Uno path. The Mega cost is absorbed within the
existing footprint; +8 B RAM (two floats) on the Mega only, well inside its
8 KB budget.

## What 4M.13 builds on

Phase 4M.13 (velocity outer loop — the other half of Workstream F) needs the
same encoder velocity reads this phase introduced into the BOOTSTRAP path:

- `0.5f * (enc_left_.read_velocity_dps + enc_right_.read_velocity_dps)` is the
  mean-wheel-velocity expression 4M.13's cascade will reuse in `step_run_` (the
  research §6 sketch uses `read_velocity_mps` — same underlying API).
- The `enc_*.reset_ticks()` discipline on state entry is the pattern 4M.13 will
  follow to freeze/reset position integration on BOOTSTRAP / HELD / FALLEN.
- A BOOTSTRAP that now *fails* (reason 7) on a slipping/binding drivetrain
  means 4M.13's outer loop can trust that, by the time it runs in RUN, the
  encoders genuinely track chassis motion — the velocity cascade is not built
  on an unverified sensor.

4M.13's `K_POS`/`K_VEL`/`MAX_NUDGE_DEG`/`POS_LEAK`/`SLEW_DEG_S` land hardcoded
per RWE §6.1; auto-derivation is deferred to Phase 4M.14 (architecture_plan
§4 sequencing-discipline flag — must stay on roadmap).
