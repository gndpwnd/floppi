# Phase 4 Structural Fixes — Coordinating Document
Last updated: 2026-05-12

## Why this doc exists

After the 2026-05-12 bench session and five parallel research agents, the conclusion is unambiguous: the balance-bot reference app needs **structural fixes**, not more PID tuning. This doc tracks the work to actually implement those fixes, with each change mapped to its source-of-truth design doc.

## The five items, in execution order

### Item 1 — D-term uses raw gyro, not numerical derivative of fused pitch

**Why:** Osoyoo's working reference (and Lauszus, Balanduino — every successful AVR balance bot) uses raw gyro as the D-term measurement. Our current PID computes `(filtered_pitch[t] - filtered_pitch[t-1]) / dt` where filtered_pitch went through BNO055 NDOF (20-40 ms group delay) + 3 ms LPF + numerical differentiation. The derivative reacts ~25 ms after the actual motion. Raw gyro is the instantaneous physical measurement.

**Where:**
- `src/control/pid_controller.{h,cpp}` — add `compute_with_explicit_derivative(measurement, gyro_rate_dps, dt_ms)` overload.
- `src/applications/balancing_robot/balance_app.cpp:step_run_` — call the new overload, passing `imu.getRawGyro()` Y-component.

**Source doc:** `findings/research_osoyoo_reference_implementation.md` §3 & §9 (concrete change #1).

### Item 2 — Wire real gyro + windup signals into OnlineMountingEstimator

**Why:** The estimator's `update()` call at `balance_app.cpp:418-423` passes hardcoded `windup_active=false, gyro_pitch_dps=0.0f`. The estimator's disturbance-freeze logic has been broken this entire session. Every pickup contaminates the offset estimate with recovery-transient I-term values. Mount offset drifts wrong → bot falls forward → operator picks up → contamination compounds.

**Where:**
- `src/applications/balancing_robot/balance_app.cpp:step_run_` — compute real `windup_active` (PID output near saturation for sustained time) and pass `g_lateral_dps_lpf_` (already plumbed) as `gyro_pitch_dps`.

**Source doc:** `findings/bootstrap_protocol_unstable_plant.md` — flagged as the HIGHEST-RISK pre-existing bug.

### Item 3 — PID into hardware-timer ISR (TimerOne on Uno)

**Why:** The current `loop()` polls `app.step(now)` after `Serial.println` calls and serial parsing. `dt` jitters by 1-3 ms per iteration. PID time-scaling is correct but the noise dominates the D-term. Osoyoo runs PID in a hardware timer ISR — exactly 5 ms cadence, always.

**Where:**
- `src/main.cpp` — add TimerOne setup; move `app.step(...)` into the ISR; keep `Serial.println` / button / serial parsing in `loop()`.
- Be careful about ISR safety: PID itself does no allocation; motor output is just `analogWrite`/`digitalWrite` which is interrupt-safe; Serial.print MUST stay out of the ISR. Use a volatile flag if any logging is needed.

**Source doc:** `findings/research_osoyoo_reference_implementation.md` §3 (concrete change #2). Also `findings/latency_budget_2026-05-12.md`.

### Item 4 — Soft-cutoff when tipped, instead of sticky FALLEN

**Why:** User feedback: "knocked over but motors keep going." With FALL_DETECTION disabled (current default), pitch=90° produces max output, motors spin uselessly on the side. The Osoyoo pattern: stay in RUN, but inside `step_run_()` zero motor output when `|pitch| > tilt_limit`. Bot auto-recovers the moment it's righted. No sticky state, no manual restart.

**Where:**
- `src/applications/balancing_robot/balance_app.cpp:step_run_` — at the top, if `|pitch| > cfg.tilt_limit_deg` (default 35°? or 25°?), set `motors_.stop()` and return. PID still computes (to keep I-term and OnlineMountingEstimator integrated, with windup_active set high).
- Existing `step_fallen_()` can stay for the `USE_BALANCE_FALL_DETECTION` opt-in build.

**Source doc:** `findings/research_osoyoo_reference_implementation.md` §3 (concrete change #3).

### Item 5 — Scalar RLS for K_motor + closed-form PD-from-K_motor + rate-limited application — DONE

**Why:** This is the universal-auto-tune answer that all five research agents converged on. Eliminates hand-tuning forever — within a declared bench-scale class (0.1-1 kg, 0.1-1 m, brushed DC + L298N). Phase 4.10.

**What landed:**
- New `src/control/plant_identifier.{h,cpp}` — scalar RLS estimating `K_motor` from `α_pitch = K_motor·pwm_total + g_eff·pitch_rad` (linearised; soft-cutoff keeps regression in linear region). Forgetting factor λ=0.998, σ-modification soft projection over (0.02, 5.0) deg/s²/PWM, MIN_PHI=10 excitation gate. Closed-form pole-placement targets emitted every tick: `Kp = ω_n²/K`, `Kd = 2ζω_n/K`, `Ki = 0.05·Kp` (ω_n = 4/ts, ts=0.5 s default, ζ=0.7 default).
- `src/applications/balancing_robot/balance_app.{h,cpp}` — PlantIdentifier injected (matches existing dependency-injection style), ticked in `step_run_()` via `run_plant_id_()`. 5%/s rate-limited ramp of live PID gains toward identifier targets. Freeze gates: 5 s bootstrap window after RUN entry, lateral-gyro > 30 dps, windup_active. `BalanceApp::get_plant_status()` and `is_adaptive_active()` accessors added for telemetry.
- `src/main.cpp` — extended `s` status command prints state, ADAPT/BOOT tag, pitch, mount-offset, output, live Kp/Kd, learned K_motor + covariance, target Kp/Kd. Verbose log strings trimmed across boot/calibration paths to keep flash under 32 KB on Uno.
- `tests/test_plant_identifier.cpp` — 7 native unit tests: construction, RLS convergence on synthetic α data, freeze gate, σ-projection, gain-target math, reset() prior, MIN_PHI excitation gate. All pass.
- `tests/test_balance_app.cpp` — updated fixture to inject `PlantIdentifier`. Pre-existing test_capture_to_auto_tune failure is unrelated to this work.

**Flash impact (arduino_uno_balancing):** 31574 B → 32216 B (97.9% → 99.9% of 32256 B). 28 B headroom remaining. Trimming the boot/cal/status log strings was necessary.

**Follow-up (NOT landed):** Stage 4.10c — bootstrap stage machine (`BootstrapStage` enum + `try_advance_bootstrap_()` with the 5-stage rule set from `bootstrap_protocol_unstable_plant.md` §3) is still on paper. The current implementation uses a single time-based 5 s bootstrap freeze; the full stage machine would gate transitions on the convergence rules (Stage 2: I-term magnitude + derivative; Stage 3: RLS P[n] threshold + θ stability; Stage 4: pitch-RMS pre/post comparison). Defer until hardware validation drives the requirement.

**Source docs:** `findings/dynamic_pwm_accel_learning.md` (the full design), `findings/research_universal_zero_knowledge_tuning.md` (the algorithm: RLS + σ-modification + rate-limited update), `findings/bootstrap_protocol_unstable_plant.md` (the staging machine).

## Execution plan

Two phases, sequential:

- **Phase A (items 1-4)** — structural fixes. One coding agent. ~4 hours of focused work.
- **Phase B (item 5)** — universal auto-tune. Separate coding agent, after Phase A lands. ~2 days.

User does not currently have the bot plugged in, so we cannot bench-validate. The acceptance criterion for both phases is **`pio run -e arduino_uno_balancing` succeeds** and **native test suite changes are consistent**. Hardware validation happens later.

## What this does NOT change

- The compile flags (`USE_BALANCE_HELD_DETECTION` on, `USE_BALANCE_FALL_DETECTION` off) remain as they are.
- The EEPROM persistence policy (BNO055 cal + mount offset, no gains) stays.
- The user-facing serial command set stays.
- The `MINIMIZE_ACCELERATIONS_PHILOSOPHY.md` and `UNIVERSAL_BALANCE_BOT_VISION.md` doctrines remain authoritative.

## See also

- [research_osoyoo_reference_implementation.md](findings/research_osoyoo_reference_implementation.md)
- [research_inverted_pendulum_control_methods.md](findings/research_inverted_pendulum_control_methods.md)
- [research_open_source_balance_bots.md](findings/research_open_source_balance_bots.md)
- [research_universal_zero_knowledge_tuning.md](findings/research_universal_zero_knowledge_tuning.md)
- [bootstrap_protocol_unstable_plant.md](findings/bootstrap_protocol_unstable_plant.md)
- [dynamic_pwm_accel_learning.md](findings/dynamic_pwm_accel_learning.md)
- [UNIVERSAL_BALANCE_BOT_VISION.md](UNIVERSAL_BALANCE_BOT_VISION.md)
- [AUTO_TUNING_REALITY_CHECK.md](AUTO_TUNING_REALITY_CHECK.md)
