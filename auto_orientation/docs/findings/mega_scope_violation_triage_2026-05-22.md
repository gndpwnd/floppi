# Mega scope-violation triage — STATIC-RETIREABLE vs BENCH-GATED

**Date**: 2026-05-22
**Author**: triage agent (read-only over `src/`, single new findings file)
**Scope governed**: Mega-universal balance stack only — `src/applications/balancing_robot/` and `src/control/`. The Uno-minimal program (`src/applications/balancing_robot_uno/`) intentionally hardcodes; those rows are out of scope here.
**Inputs**: `docs/scope.md` (Current scope violations audit), `docs/archive/session_records/2026-05-21_multi_agent_workstream_g_security.md`, and the live source as of this date.

> **Purpose**: For each remaining Mega hardcoded-constant scope violation, separate what a future *static* (no-hardware) code agent could safely retire **right now** from what genuinely must wait for bench data the bot can only produce once it balances. Bias is **conservative**: if retiring a control-loop constant without hardware validation could destabilise the tuned loop, it is classified BENCH-GATED.

---

## Important: line numbers in `scope.md` are stale

`balance_app.cpp` has grown to ~102 KB since the audit was written, so the audit's line numbers no longer point at the constants. This triage re-located every constant from current source. Where the *value* or *form* has drifted from the audit, that is noted in the row. The HELD detector in particular was re-worked (the 2026-05-18 PM "lateral-gyro 90 dps" path was removed; the surviving triggers are `ext_motion` and `lift_detected`), so the audit's "Phase 2.5" rows now map onto slightly different literals.

---

## Quantities the bot already measures (the "static-retireable" feedstock)

A constant is **STATIC-RETIREABLE** only if it can be re-expressed in terms of a quantity the firmware *already* produces, with no new measurement phase and no risk of changing closed-loop behaviour in a way that needs validation. The available feedstock today:

| Measured/derived quantity | Source | Exposed via |
|---|---|---|
| `K_motor` (plant gain, deg/s²/PWM) | BOOTSTRAP pulse response + RLS | `PlantIdentifier::get_k_motor()`, `BootstrapResult.k_motor` |
| `Kp_target` / `Kd_target` / `Ki_target` | closed-form from `K_motor` | `PlantIdentifier::get_kp_target()` etc. |
| `ω_n` (inner-loop natural freq) | derived from settling-time target `POSLOOP_INNER_TS_SEC=0.5s` (ω_n ≈ 4/Ts) | computed in `balance_app.cpp:1619` as `wn_inner` |
| `ζ` (damping ratio) | `PlantIdentifier` default / setter | `set_damping_ratio()` |
| **stiction floor** `discovered_min_pwm` | PWM-discovery (encoders) | `BalanceApp::get_discovered_min_pwm()` |
| **saturation point** `discovered_max_pwm` | PWM-discovery (encoders) | `BalanceApp::get_discovered_max_pwm()` |
| PID sample period | MsTimer2 hardware period (5 ms) | structural, not a tuning |

**Critical gap**: there is **no baseline gyro-noise / accel-noise measurement** exposed anywhere in `balance_app.{h,cpp}` (grep for `baseline_gyro` / `noise_floor` / `gyro_baseline` returns nothing in the balance app). CHARACTERISE Phase 2.1 noise-floor capture, referenced repeatedly in `scope.md` as the source for the "3 × baseline gyro noise" derivations, **is not implemented in the current tree**. Every violation whose replacement plan reads "3 × / 5 × baseline gyro (or accel) noise" is therefore **doubly blocked**: it needs both a balancing bot *and* a measurement layer that does not yet exist. Those are classified BENCH-GATED with that note.

---

## Per-violation triage

The audit listed 14 remaining `⏳`/`🔄 [mega]` rows. Each is below with its **current** file:line and value.

### 1. `kDefaultTiltLimitDeg = 35.0f` — `safety.cpp:10`
- **Controls**: the tip-over angle past which `BalanceSafety` declares FALLEN (`> tilt_limit_deg_`); also feeds `max_angle_deg = tilt_limit_deg_ * 0.9f` at `safety.cpp:81`.
- **Audit plan**: "Compute from accel quaternion at the pitch where lateral accel hits 0.5 g" (geometric CoG-above-axis).
- **Classification**: **BENCH-GATED**.
- **Why**: The 0.5 g lateral-accel crossing is a *geometric property of the assembled chassis* (CoG height vs wheel axis). The firmware does not currently sample lateral accel across a pitch sweep, and you cannot synthesise the crossing angle without either a CAD CoG figure or a controlled tilt sweep on hardware. Retiring it statically would mean inventing a formula with an unmeasured CoG — exactly the random-walk failure mode `scope.md` §Process doctrine warns against. **Unblock**: a one-shot tilt-sweep (manually tip the powered bot, log pitch at which `|lateral accel| = 0.5 g`), or a declared CoG height from the chassis design.

### 2. `SOFT_ZONE_DEG = 1.0f` — `balance_app.cpp:567`
- **Controls**: gain-scheduling soft zone. Inside ±1°, output is linearly scaled down (`out *= abs_pitch / SOFT_ZONE_DEG`) to suppress D-term PWM bursts on micro gyro noise near balance.
- **Audit plan**: "3 × LP-filtered std-dev of pitch_deg over the most recent quiet RUN window."
- **Classification**: **BENCH-GATED**.
- **Why**: The replacement is explicitly a function of the *observed pitch noise during a quiet RUN window* — data that only exists once the bot balances. No such running-variance estimator is implemented today. Statically picking any value (even the current 1°) is a guess that directly shapes near-balance authority; the 2026-05-18 PM note records that 2° was already empirically too wide. Changing it blind risks re-introducing that regression. **Unblock**: implement a pitch running-variance estimator over quiet RUN, then set `SOFT_ZONE_DEG = 3·σ_pitch`.

### 3. `SAT_THRESHOLD_PWM = 180` — `balance_app.cpp:651`
- **Controls**: STUCK detector — PWM magnitude at/above which output is considered "saturated" while checking for no rotation.
- **Audit plan**: "0.7 × measured saturation_pwm from CHARACTERISE."
- **Classification**: **STATIC-RETIREABLE (encoder builds) / BENCH-GATED (non-encoder builds)** — net recommendation: **HOLD, lean BENCH-GATED**.
- **Why**: `get_discovered_max_pwm()` *does* expose a measured saturation point on `USE_WHEEL_ENCODERS` builds, so `0.7 × get_discovered_max_pwm()` is mechanically derivable with no new bench session — this is the closest of all 14 to genuinely retireable. **However**: (a) it is a *safety detector* threshold; setting it from a discovery value that itself has never been validated on real motors (the 2026-05-21 session lists "Real-motor PWM-discovery validation" as an open bench blocker) means the STUCK trip point would inherit unvalidated data; (b) on non-encoder builds `discovered_max_pwm` is unavailable and there is no fallback source. Conservative call: the *mechanism* (read the getter, multiply by 0.7, guard for `discovered==false`) is safe to write, but it should not be the sole gate until PWM-discovery is bench-confirmed. See recommendation.

### 4. `STUCK_GYRO_DPS = 5.0f` — `balance_app.cpp:652`
- **Controls**: STUCK detector — gyro-pitch magnitude below which "no rotation is actually happening."
- **Audit plan**: "3 × baseline gyro noise from CHARACTERISE Phase 2.1."
- **Classification**: **BENCH-GATED** (doubly blocked).
- **Why**: Needs a baseline gyro-noise measurement that **does not exist in the tree**. No static derivation possible without inventing the noise figure. **Unblock**: implement gyro noise-floor capture (running variance over a still window), expose it, then `STUCK_GYRO_DPS = 3·σ_gyro`.

### 5. `STUCK_TIMEOUT_MS = 1500` — `balance_app.cpp:653`
- **Controls**: STUCK detector dwell — how long saturated-and-still must persist before abort→IDLE.
- **Audit plan**: "5 × expected PID response time = 5 × (2π / ω_n)."
- **Classification**: **STATIC-RETIREABLE (weak) — recommend HOLD**.
- **Why**: ω_n is derivable (4/`POSLOOP_INNER_TS_SEC` = 8 rad/s on the current 0.5 s settling target), so `5 · 2π/ω_n ≈ 5 · 0.785 ≈ 3.9 s` is computable statically. *But* note this would **roughly triple** the current 1500 ms timeout — i.e. retiring it changes a safety-abort latency by a large factor, with no hardware to confirm the bot doesn't sit saturated-and-stalled for ~4 s damaging motors. The derivation also presumes ω_n from the *position* loop settling target is the right time-base for the *attitude* STUCK detector, which is not obviously correct. Conservative call: hold until the STUCK detector can be observed firing on the bench.

### 6. `ext_motion: last_cmd_mag < 20` — `balance_app.cpp:503` (audit: "Phase 2.5 cmd_mag < 20")
- **Controls**: HELD trigger 1 — "motors quiet" arm of the externally-driven-motion test.
- **Audit plan**: "0.5 × measured stiction_pwm."
- **Classification**: **STATIC-RETIREABLE (encoder builds) — recommend HOLD pending PWM-discovery validation**.
- **Why**: `get_discovered_min_pwm()` exposes the measured stiction floor on encoder builds, so `0.5 × get_discovered_min_pwm()` is mechanically derivable with no new session. Same caveats as #3: it gates a behavioural state transition (HELD), and the underlying discovery is itself bench-unvalidated (2026-05-21 blocker). On non-encoder builds there is no stiction source. The current literal `20` happens to sit right inside the documented 30–80 PWM stiction band, so changing it blind shifts when the bot decides it is being handled. Hold until discovery is confirmed; then this and #3 retire together.

### 7. `ext_motion: abs_pitch_gyro > 30.0f` — `balance_app.cpp:503` (audit: "Phase 2.5 gyro > 30 dps")
- **Controls**: HELD trigger 1 — "fast pitch rotation" arm.
- **Audit plan**: "5 × baseline gyro noise."
- **Classification**: **BENCH-GATED** (doubly blocked — no noise-floor measurement exists).

### 8. HELD dwell (audit: "Phase 2.5 dwell = 100 ms") — now `dwell = ext_motion ? 20 : 60` ticks, `balance_app.cpp:507`
- **Controls**: tick-count debounce before RUN→HELD fires (20 ticks ≈ 100 ms for ext_motion, 60 ticks ≈ 300 ms for lift).
- **Audit plan**: "2 × PID sample period × some sigma multiplier."
- **Classification**: **BENCH-GATED**.
- **Why**: The audit's own plan is vague ("some sigma multiplier") and the "sigma" is a motion-noise figure that isn't measured. The literal already drifted (100 ms → split 20/60 tick values) during bench tuning, which is itself a symptom that this is empirically set, not derivable. No safe static replacement. **Unblock**: same noise-floor layer plus a quiet-RUN debounce-tuning observation.

### 9. HELD `a_dev_lpf_ > 6.0f` — `balance_app.cpp:504` (audit: "HELD a_dev_lpf_ > 6.0f")
- **Controls**: HELD trigger 2 — lift detection via accel-magnitude deviation from gravity (m/s²).
- **Audit plan**: "3 × baseline accel noise (BNO055 LIA — Phase 4.6.5)."
- **Classification**: **BENCH-GATED** (doubly blocked — no accel noise-floor measurement; Phase 4.6.5 LIA not wired).
- **Note**: there is a paired HELD→RUN "quiet" gate at `balance_app.cpp:785` (`g_lateral_dps_lpf_ < 12.0f && a_dev_lpf_ < 1.5f`) and `level: abs_pitch < 8.0f` — same noise-floor dependency, same classification. Not in the original 14 but flagged here as a sibling that retires with the same measurement.

### 10. `BOOTSTRAP_FREEZE_MS = 5000` (RLS warmup) — `balance_app.cpp:728`
- **Controls**: window after RUN entry during which `adaptive_active_` is held false (RLS warmup for natural-disturbance ID). Bypassed entirely when BOOTSTRAP succeeded (`force_adaptive`/seeded path sets `adaptive_active_ = true`, `balance_app.cpp:1528`).
- **Audit plan**: marked `🔄` — "bypassed when BOOTSTRAP succeeds; still applies if BOOTSTRAP fails and operator force-runs anyway."
- **Classification**: **BENCH-GATED (low priority)**.
- **Why**: Only reachable on the BOOTSTRAP-failed fallback path. A principled value would be a multiple of the RLS convergence time, which depends on disturbance richness observed on hardware. Not derivable statically, but also low-stakes (degenerate path). Hold.

### 11. Absolute pitch kill = ±20° — `src/main.cpp:739`
- **Controls**: belt-and-suspenders kill switch — force-stop + abort if `|pitch| > 20°` regardless of state.
- **Audit plan**: "0.8 × derived tilt_limit_deg."
- **Classification**: **BENCH-GATED (transitively)**.
- **Why**: Its replacement is `0.8 × tilt_limit_deg`, and `tilt_limit_deg` itself (#1) is BENCH-GATED. The formula is trivial *once #1 is real*, but retiring it now would couple it to the still-hardcoded 35° default, producing 28° — a silent behavioural change to a top-level safety kill with no validation. **Unblock**: retire #1 first (geometric tilt limit), then this becomes a one-line static change. Until then, hold.

### 12. `online_est max_deviation = 5°` — `online_mounting_estimator.cpp:26` (`DEFAULT_MAX_DEVIATION_DEG`), set via `main.cpp` would call `set_max_deviation_deg`
- **Controls**: hard clamp on how far the online mount estimate may drift from the captured reference (`reference_deg ± max_deviation_deg`, `online_mounting_estimator.cpp:195`); also normalises the confidence metric.
- **Audit plan**: "3 × pitch oscillation amplitude during RUN."
- **Classification**: **BENCH-GATED**.
- **Why**: "Pitch oscillation amplitude during RUN" is balance data the bot only produces once stable. No estimator for it exists. Statically narrowing/widening the clamp changes how aggressively the mount offset can chase a moving balance point — directly affects stability. Hold. **Unblock**: a RUN-window pitch-amplitude observation.

### 13. `online_est LPF tc = 8 s` — set in `src/main.cpp:578` (`online_est.set_lpf_time_constant_sec(8.0f)`; default `DEFAULT_LPF_TC_SEC` in the estimator)
- **Controls**: time constant of the mount-offset low-pass; `alpha = 1/tc` at `online_mounting_estimator.cpp:190`. Governs how slowly the online mount estimate tracks.
- **Audit plan**: "Derive from observed pitch dynamics — should match the PID closed-loop time constant."
- **Classification**: **STATIC-RETIREABLE (weak) — recommend HOLD**.
- **Why**: The PID closed-loop time constant is derivable from ω_n (≈ 1/ω_n or a small multiple), and ω_n is available statically (8 rad/s ⇒ closed-loop τ ≈ 0.125 s). *But* the design intent is for the mount LPF to be **much slower** than the control loop (it tracks slow mechanical/thermal drift, not fast attitude) — a literal "match the closed-loop time constant" would drop tc from 8 s to a fraction of a second and make the mount estimator chase fast pitch transients, which is destabilising and contradicts its purpose. The audit plan's wording is arguably wrong here. Until the correct relationship is settled and observed on hardware, hold. (`set_max_drift_rate_dps(2.0f)` at `main.cpp:579` is a sibling hardcode in the same call block, similarly bench-gated.)

### 14. `cfg.tilt_limit_deg` derived value (audit row `🔄`) — override removed; falls back to `kDefaultTiltLimitDeg` (#1)
- **Controls**: same as #1 (this row is the "derived value still pending" half of the tilt-limit story; the override was already removed, value now falls through to the 35° default).
- **Classification**: **BENCH-GATED** — fully subsumed by #1. Retiring #1 retires this.

---

## Summary table

| # | Constant | File:line | Value | Class |
|---|---|---|---|---|
| 1 | `kDefaultTiltLimitDeg` | `safety.cpp:10` | 35.0° | BENCH-GATED |
| 2 | `SOFT_ZONE_DEG` | `balance_app.cpp:567` | 1.0° | BENCH-GATED |
| 3 | `SAT_THRESHOLD_PWM` | `balance_app.cpp:651` | 180 | STATIC-capable¹ → HOLD |
| 4 | `STUCK_GYRO_DPS` | `balance_app.cpp:652` | 5.0 dps | BENCH-GATED (no noise layer) |
| 5 | `STUCK_TIMEOUT_MS` | `balance_app.cpp:653` | 1500 ms | STATIC-capable² → HOLD |
| 6 | `ext_motion cmd_mag < 20` | `balance_app.cpp:503` | 20 | STATIC-capable¹ → HOLD |
| 7 | `ext_motion gyro > 30` | `balance_app.cpp:503` | 30 dps | BENCH-GATED (no noise layer) |
| 8 | HELD dwell 20/60 ticks | `balance_app.cpp:507` | 20/60 | BENCH-GATED |
| 9 | HELD `a_dev_lpf_ > 6` | `balance_app.cpp:504` | 6.0 m/s² | BENCH-GATED (no noise layer) |
| 10 | `BOOTSTRAP_FREEZE_MS` | `balance_app.cpp:728` | 5000 ms | BENCH-GATED (low pri) |
| 11 | abs pitch kill ±20° | `main.cpp:739` | 20° | BENCH-GATED (transitive on #1) |
| 12 | `DEFAULT_MAX_DEVIATION_DEG` | `online_mounting_estimator.cpp:26` | 5.0° | BENCH-GATED |
| 13 | online_est LPF tc | `main.cpp:578` | 8.0 s | STATIC-capable² → HOLD |
| 14 | `cfg.tilt_limit_deg` derived | (subsumed by #1) | — | BENCH-GATED |

¹ Mechanically derivable from `get_discovered_min/max_pwm()` on `USE_WHEEL_ENCODERS` builds, but the underlying PWM-discovery is itself bench-unvalidated (2026-05-21 open blocker) and these gate safety/state-transition behaviour. No fallback on non-encoder builds.
² Mechanically derivable from ω_n (available statically), but the derivation either changes a safety latency by a large factor (#5) or contradicts the constant's design intent (#13).

**Counts**: of the 14 audited rows, **0 are unconditionally safe to retire statically**. 4 are *mechanically* STATIC-capable (#3, #5, #6, #13) but each carries a stability/safety caveat that makes blind retirement risky; the remaining 10 are BENCH-GATED, of which **5 are doubly blocked** by the absence of any noise-floor measurement layer (#4, #7, #9, plus the HELD→RUN quiet gate and #8's "sigma").

---

## Prioritised recommendation

**Bottom line: do NOT retire any of the 14 control-loop constants in a static-only session.** Every row either depends on data the bot can only produce once balancing, or — for the four "mechanically derivable" rows — touches a safety detector / state-transition threshold in a way that needs hardware to confirm it didn't destabilise the loop. This is precisely the case `scope.md` §Process doctrine flags: changing a numeric constant statically makes the framework *feel* more universal while actually shipping an unvalidated guess.

What a static session *can* productively do instead (highest leverage first):

1. **Build the missing noise-floor measurement layer** (running variance of gyro and accel over a still/quiet window, exposed via getters). This is pure infrastructure with no closed-loop risk, and it is the single blocker on **5 of the 14** rows (#4, #7, #9, the HELD→RUN gate, #8). It is also the "smallest measurement to build" that `scope.md` §Process doctrine point 2 asks for. Once it exists, those rows become bench-validatable on the next hardware session.

2. **Wire the PWM-discovery getters into #3 and #6 *behind a guard*** — i.e. write the `0.7 × get_discovered_max_pwm()` / `0.5 × get_discovered_min_pwm()` derivation with a fallback to the current literal when `discovered==false`. This is mechanism-only and keeps current behaviour when discovery hasn't run, so it does not destabilise anything; it just *enables* retirement the moment "Real-motor PWM-discovery validation" (the 2026-05-21 bench blocker) is checked off. Tag both with a TODO pointing at that bench gate. Treat this as "prepare to retire," not "retired."

3. **Hold #1, #11, #14 together as one bench task** — capture the geometric tilt limit (0.5 g lateral-accel crossing) on hardware; #11 and #14 then fall out as one-line static edits afterward.

4. **Hold #2, #5, #8, #10, #12, #13** until the bot balances long enough to observe the dynamics each one is supposed to be derived from. For #5 and #13 specifically, *re-examine the audit's stated formula on hardware* — both look likely to produce the wrong value (#5 ≈ tripled abort latency; #13 ≈ a mount LPF far too fast for its purpose).

**Single highest-value static action**: implement the noise-floor measurement layer (item 1). It unblocks more rows than anything else, carries zero closed-loop risk, and is the canonical "build the measurement instead of tweaking the constant" move this project's doctrine demands.
