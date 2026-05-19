# Theoretical Soundness Audit — Balance-Bot Control Stack
Status: COMPLETE. Performed 2026-05-18 against HEAD = 9a47518. Motors disconnected.
Last updated: 2026-05-18

## Scope and method

After a bench day confounded by (1) a frozen BNO055 (wrong crystal flag) and (2) unpowered motors, the user demanded the algorithm be proven sound before re-engaging hardware. This is a static, line-by-line audit of `PIDController::compute_with_rate`, `PlantIdentifier::update`, `OnlineMountingEstimator::update`, `BalanceApp::step_run_`/`tick`/`read_sensors`, `BalanceSafety`, and the ISR wiring in `main.cpp`. The linearised pitch dynamics used throughout are `θ̈ = K_motor·u + g_eff·sin(θ)` with `g_eff = 50 deg/s²` (plant_identifier.cpp:34) and flashed gains `Kp=50, Ki=1, Kd=20, i_term_limit=40 PWM` (main.cpp:290-296).

## Section 1 — Stability

Linearising sin(θ) ≈ θ (radian) and routing `u = -Kp·θ - Kd·θ̇` (from `compute_with_rate` at pid_controller.cpp:222,237), the closed loop is:

```
θ̈ + (K·Kd)·θ̇ + (K·Kp − g_eff_rad)·θ = 0
```

LHP requires `K·Kd > 0` and `K·Kp > g_eff_rad ≈ 0.873 rad/s²`.

**Seed (Kp=50, Kd=20)**, representative bench K ≈ 0.3 deg/s² per PWM. Working in degree units, `g_eff/180·π = 0.87 deg/s²/deg`, K·Kp = 15 deg/s²/deg, K·Kd = 6 deg/s²/(deg/s). Polynomial `s² + 6s + 14.13`; roots `−3 ± j2.27`; ζ ≈ 0.80; ω_n ≈ 3.76 rad/s; settling ≈ 1.3 s. **Stable, slightly overdamped — design intent.**

**Converged (pole-placement, plant_identifier.cpp:240-261)**: `Kp = ω_n²/K`, `Kd = 2ζω_n/K`, ω_n = 4/ts = 8 rad/s, ζ = 0.7. By construction `K·Kp = 64` and `K·Kd = 11.2` **for any K > 0**. Characteristic poly `s² + 11.2s + 63.1`; roots `−5.6 ± j5.74`; ζ = 0.703; ω_n = 7.94. **K-invariant LHP** across the entire projection band [0.02, 5.0].

**During the 5%/s ramp**: per-tick `Δapplied_kp = max(0.05·dt·|kp|, 0.001)`. Both gains stay positive and monotonic between seed and target; Routh–Hurwitz holds pointwise. **Ramp is safe.**

**Projection boundaries**: K=0.02 ⇒ Kp_target=3200, instant ±255 saturation → windup_active trips after 2 ticks → RLS freezes (balance_app.cpp:493) → σ-leak pulls θ back. K=5.0 ⇒ Kp_target=12.8, modest authority, same converged poles. Both rails self-contain.

**Section 1: no defects.**

## Section 2 — Boundedness

- **integral_** (pid_controller.cpp:287-310): clamped to `±output_max/ki_` or tighter `±i_term_limit/ki_` when `ki_>0`. With current config: ±40 PWM. When `ki_==0` the clamp returns early (line 292-294) and integral is unbounded except by `error·dt` accumulation. **TA-1 (minor).**
- **theta_** (plant_identifier.cpp:210-216): soft σ-leak then hard clamp to [k_min, k_max] = [0.02, 5.0]. **Provably bounded.**
- **P_** (plant_identifier.cpp:198-204): steady state `P_ss = (1−λ)/φ² ≈ 5e-8` under persistent excitation. Under no-excitation, `phi_abs < MIN_PHI` returns early at line 188 BEFORE the P-inflation step, so P is held constant during gates. Bounded above by INITIAL_P=1.0. **Sound.**
- **OnlineMountingEstimator** (online_mounting_estimator.cpp:181-198): hard clamp ±5° + rate-limit ±0.5°/s (default; main.cpp:303 overrides to 2°/s). **Provably bounded.**
- **applied_kp_/kd_/ki_** (balance_app.cpp:857-868): tracks kp_target, which is bounded by ω_n²/k_min = 3200. Reachable only if RLS misidentifies to the floor. **TA-2 (minor).**

## Section 3 — Non-degeneracy

### 3.1 Division by zero

All divides are guarded:
- `clamp_integral_`: `ki_ <= 0` early-return (line 292). **Safe.**
- LPF alpha `dt/(τ+dt)`: τ≥0, dt>0. **Safe.**
- `(rate − prev)/dt`: `dt_sec < 1e-4f` early-return (plant_identifier.cpp:159). **Safe.**
- RLS `phi_P/denom`: `denom = λ + φ²·P ≥ λ > 0`. **Safe.**
- `1/theta_`: floored at 1e-3 (line 248) AND projection-clamped at k_min=0.02 (line 214). Double-protected. **Safe.**
- LPF `1/tc`: tc floored at 0.1 s (online_mounting_estimator.cpp:76). **Safe.**
- Welford `m2/(n-1)`: gated by `n >= 2` (balance_app.cpp:292). **Safe.**

### 3.2 NaN propagation

- **PIDController** (pid_controller.cpp:131,210): explicit `isnan(measurement)` / `isnan(rate_dps)` guards return last_output_. **Good.**
- **PlantIdentifier**: **no NaN guards on any input.** A NaN pwm_total, gyro, prev_gyro, or pitch_deg contaminates θ. Once θ is NaN, kp_target is NaN, the rate-limit ramp propagates NaN to applied_kp, and `pid_.set_tunings(NaN,...)` accepts it (the `(kp < 0.0f) ? 0.0f : kp` guard at pid_controller.cpp:80 fails because `NaN < 0.0f` is false). **TA-3 (important).**
- **OnlineMountingEstimator**: **no NaN guards.** `clamp_(NaN, lo, hi)` returns NaN (both compares false). Once `estimate_deg_` is NaN, `corrected_pitch_()` produces NaN, which the PID's isnan guard catches — but the bot then runs at `last_output_` indefinitely. Stays NaN forever because the LPF update `NaN + alpha·(target − NaN) = NaN`. **TA-4 (important).**

### 3.3 Sign conventions

`compute_with_rate` with setpoint=0: `error = −pitch`, `p_term = −kp·pitch`, `d_term = −kd·rate`. Positive pitch and positive rate (bot falling forward) → output negative → motor PWM negative → `drive_channel_` sets `in_a LOW, in_b HIGH` (l298n_motor_driver.cpp:129-131) — opposite rotation from positive. main.cpp:80-85 explicitly swapped IN1/IN2 and IN3/IN4 vs the legacy .ino so that positive PWM = forward; therefore negative PWM = backward = wheels drive backward = base translates back = chassis pitches **back toward upright**. **Damping correct.** For pure rate (no error), d_term < 0 drives wheels backward → reaction torque corrects forward pitch motion. **Consistent throughout.**

### 3.4 RLS collapse under no excitation

MIN_PHI = 10 PWM total (plant_identifier.cpp:70). With motors stopped (HELD/soft-cutoff/IDLE), `pwm_total = 2·0 = 0 < 10` → line 188 returns with theta_ and P_ untouched. **No collapse — θ stays at the prior.** σ-leak fires only outside the band, so the prior is not actively restored under no-excitation, but the band guarantees k_min ≤ θ ≤ k_max throughout.

## Section 4 — Liveness / timing

### 4.1 ISR cost

`app.tick()` at 200 Hz via MsTimer2 (main.cpp:251-253). Per-RUN-tick: pid.compute_with_rate ~95 µs, online_est.update ~95 µs, plant_id.update ~190 µs, three ramp_gain_ calls ~60 µs, motors.set_speed ~3 µs, state machine ~2 µs. **Total ≈ 445 µs ≈ 9% of the 5 ms budget.** Generous headroom on 16 MHz AVR.

### 4.2 Loop / I²C contention

`read_sensors` calls `imu.read()` (8 B quat + 4 B cal), `getRawGyro` (6 B), `getRawAccel` (6 B). At **100 kHz** I²C (bno055.cpp:88-93 — setClock(400000) is commented out per pull-up concerns), each byte ≈ 90 µs, plus 4× START/STOP at ~100 µs. Total wall-clock ≈ **3.3 ms per read_sensors call**.

Wire is interrupt-driven on AVR; CPU work during the transfer is ~290 µs (TWI ISR fires per byte). MsTimer2 ISR is NOT blocked — it preempts cleanly because Wire doesn't disable global interrupts in its TWI ISR. `app.tick()` never touches Wire (explicit contract at balance_app.h:175-182) so no deadlock.

But: the loop iteration takes ~3.5-4 ms minimum, while MsTimer2 fires every 5 ms. One loop iteration per ISR. If the operator runs the `s` status command (main.cpp:347-362 prints 9 floats + 7 strings), a single iteration can exceed 10 ms — during which `raw_gyro_dps_` is stale by 10 ms (two ISR ticks reading the same value). At 60° NDOF group delay this is tolerable but erodes phase margin. **TA-5 (important):** enable Wire.setClock(400000UL); spec'd to save ~3 ms per read, recovers ~6° phase margin per latency_budget_2026-05-12.md.

### 4.3 Volatile / atomicity

`raw_gyro_dps_[3]` (balance_app.h:298) is **not declared volatile** and the three writes at balance_app.cpp:812-814 are not in an ATOMIC_BLOCK. AVR float stores decompose to four `st` instructions; MsTimer2 ISR can preempt mid-store. tick() reads `raw_gyro_dps_[1]` at line 415 — it may see a torn float (mixed-byte garbage), often a wild magnitude or NaN. The header comment at balance_app.h:174-182 claims "naturally safe because main-loop writes happen with this ISR paused" — **this claim is false**. `read_imu_` does not pause the ISR. **TA-6 (important).** Combined with TA-3 (no NaN guards in RLS) this is a credible silent-failure pathway: one torn read latches θ → NaN.

Same hazard for `g_lateral_dps_lpf_`, `a_dev_lpf_`, `a_align_` (lines 842-844), used by tick() at lines 396, 533, 587. Lower severity because the 120 ms LPF smooths anomalies before they trip HELD; a single bad sample is below the threshold by orders of magnitude.

## Section 5 — Algorithmic edge cases

**PIDController**:
- dt_ms == 0: returns last_output_ (line 137). **OK.**
- First compute: D-term skipped in `compute`; `compute_with_rate` uses external rate (always valid) and re-arms `first_compute_=true` for any later switch back (line 252). **OK.**
- Tunings changed at runtime: `set_tunings` re-clamps integral against new effective bound. **OK.**

**PlantIdentifier**:
- ki_target=0: never happens (KI_FRACTION=0.05 × always-positive kp_target). **OK.**
- Covariance underflow: P_ss ≈ 5e-8, well within float32 denormal range; near-zero phi_P just means RLS is sluggish, not unstable. **OK.**

**OnlineMountingEstimator**:
- Large initial pitch error: estimate rail-pins at ±5°, confidence drops to 0.0. But host (main.cpp:411-416) saves to EEPROM every 60 s with no confidence check — bad capture persists across boots. **TA-7 (minor).**
- Permanent freeze: if windup_active stays high forever (controller stuck saturated), adaptation is permanently frozen. Acceptable failure mode.

**BalanceApp**:
- HELD entered with stale motion filters: filters retain HELD-era values briefly into RUN; could re-trigger HELD spuriously on first tick. **Edge case, not a defect.**
- Soft-cutoff at |pitch|>25°: zeros motors → MIN_PHI gate suppresses RLS update. `gyro_pitch_prev_dps_` IS updated at line 562 even during cutoff — first non-cutoff tick sees one-step-stale prev (one bogus α sample, freeze-suppressed). **OK.**
- enter_state_(RUN): plant_id_.reset uses CURRENT gains as prior (line 762). If re-entering RUN after a HELD bout in which adaptive_active had moved gains, the new prior reflects the last learned θ, not the seed. **Reasonable.**

## Section 6 — Cross-module interactions

### 6.1 5%/s ramp vs 200 Hz RLS

RLS can swing kp_target across the projection band in seconds; the ramp lags badly. Going from applied_kp=50 to target=213 (K=0.3 case) takes ~65 s. But the target stops moving once RLS converges (P → 5e-8); the gap closes. **By design** — "transient bad estimates can't slam the loop" (plant_identifier.h:35-36). **Not a defect.**

### 6.2 Mount offset drift vs K_motor estimate

OnlineMountingEstimator subtracts `estimate_deg_` from `pitch_deg_` (balance_app.cpp:854). But RLS feeds on **raw** `pitch_deg_` (line 544), not corrected. So RLS sees physical pitch vs physical α — correct. Mount estimator gets a small biased i_term and adapts on its own ~20 s LPF (main.cpp:302). Time-scale separation between mount LPF (20 s) and RLS memory (25 s) means they can interact but neither destabilises the other. **Sound architecture.**

### 6.3 HELD freezes RLS

`step_held_` never calls `run_plant_id_`. θ, P preserved. On HELD→RUN, `enter_state_(RUN)` resets `gyro_pitch_prev_dps_ = 0.0f` (line 766) — first RLS sample post-resume sees α = gyro/dt (potentially huge), BUT the bootstrap-freeze window (5000 ms, line 530) covers this. **Safe.**

### 6.4 Ki = 0.05·Kp scaling vs clamp_integral_

When ki_ rises via the ramp, `set_tunings` calls `clamp_integral_` (pid_controller.cpp:84) which SHRINKS the bound `±limit/ki`, immediately trimming integral. When ki_ falls, the bound expands but integral is unchanged. Control output remains correct (`ki·integral` is continuous in `integral`), but the displayed i_term jumps because `ki` itself changed. **TA-8 (minor, cosmetic).**

## Section 7 — Verdict + ranked defect list

| ID | Severity | Location | Description | Fix |
|---|---|---|---|---|
| TA-1 | minor | pid_controller.cpp:292-294 | When ki==0 the integral isn't clamped; can drift via error·dt accumulation; jumps i_term on re-enable. | Clamp against `output_max` directly when ki==0, or zero the integral on ki transition through zero. |
| TA-2 | minor | balance_app.cpp:865-868 | Applied gains rate-limited but not absolute-clamped. Misidentified θ at k_min=0.02 ⇒ Kp=3200. | Add `applied_kp_ = min(applied_kp_, output_max/MIN_PITCH_DEG)`. |
| TA-3 | **important** | plant_identifier.cpp:146-220 | No NaN guards. Single torn gyro read or sensor hiccup latches θ→NaN→kp_target→applied_kp permanently. The negative-gain guard at pid_controller.cpp:80 fails for NaN (`NaN<0` is false). | `if (isnan(...)) return;` at function head. Also harden `set_tunings` with an isnan check. |
| TA-4 | **important** | online_mounting_estimator.cpp:112-198 | Same NaN-latch. `clamp_(NaN,...)` returns NaN. Once estimate_deg_=NaN, corrected_pitch=NaN, PID returns last_output_ forever → runaway motor. | isnan guards at function head; return prior estimate. |
| TA-5 | important | bno055.cpp:88-93 | I²C at 100 kHz ⇒ ~3.3 ms per read_sensors blocking the loop. Erodes phase margin. | Enable Wire.setClock(400000UL) after verifying pull-up resistors on the breakout. |
| TA-6 | **important** | balance_app.cpp:812-814 + balance_app.h:298 | raw_gyro_dps_ writes (loop) and reads (ISR) are non-atomic. AVR float stores can be torn mid-store by MsTimer2 ISR. Header comment claiming the writes happen with ISR paused is false. | Wrap writes (and reads) in `ATOMIC_BLOCK(ATOMIC_RESTORESTATE)`. Or use a flip-buffer with a single volatile uint8_t index. |
| TA-7 | minor | main.cpp:411-416 | Rail-pinned mount estimator saves to EEPROM every 60 s regardless of confidence. Bad capture persists across boots. | Gate save on `confidence > 0.3` AND `!adaptation_frozen` for a sustained period. |
| TA-8 | minor | pid_controller.cpp:77-85 | When ki rises via auto-tune ramp, clamp_integral_ shrinks integral instantly; telemetry shows non-physical i_term jumps. Cosmetic only. | Scale integral inversely on ki change to preserve `ki·integral` continuity. |
| TA-11 | **important** | balance_app.cpp:530 | BOOTSTRAP_FREEZE_MS = 5000 is shorter than the mount-estimator settling time (~30 s at the current 20 s LPF). RLS sees mount-bias-contaminated regression data during the first 25 s of RUN. | Extend BOOTSTRAP_FREEZE_MS to 30000, OR gate adaptive_active on mount-settling completion per bootstrap_protocol_unstable_plant.md §3 (`mean(|I-term|)<10 AND |d(offset)/dt|<0.05 AND time≥30s`). |

### Sections with no defects

- §1 (stability) — poles LHP for seed, ramp, and converged.
- §3.1 (division by zero) — all paths protected.
- §3.3 (sign conventions) — consistent end-to-end.
- §3.4 (RLS collapse) — MIN_PHI gate prevents collapse to k_min.
- §6.1-6.3 (cross-module timing) — time-scale separation properly architected.

## Section 8 — Design-doc cross-reference

| Design doc claim | Code | Match? |
|---|---|---|
| dynamic_pwm_accel_learning.md §4a: "scalar RLS, three floats + multiply-add per tick" | plant_identifier.cpp:165-204 | **Match.** Algorithm verbatim from the doc. |
| Same §5: "exclude dead-band samples (below ~18-25 PWM per wheel)" | MIN_PHI = 10 PWM total (= 5 PWM per wheel) at plant_identifier.cpp:70 | **Threshold is half of doc's recommendation.** Deliberate per comment at line 67-70 (looser to allow learning at small corrections), but worth validating: at PWM=5 per wheel, the motor is well inside the stiction dead-band and α is dominated by gravity. |
| research_universal_zero_knowledge_tuning.md §6.2: "σ-modification: outside [K_min, K_max] → freeze and alarm" | Soft leak then hard clamp at plant_identifier.cpp:210-216 | **Partial.** No alarm signal exposed to host. Operator can't tell from telemetry that projection was active. |
| bootstrap_protocol_unstable_plant.md §3.5: "freeze gates: HELD, large lateral gyro, PID windup, bootstrap window" | balance_app.cpp:546-547 + step_held_ omitting run_plant_id_ | **Match.** All four gates wired. |
| Same Stage 3: "RLS starts after the mounting estimator has settled" (~30 s in current config) | BOOTSTRAP_FREEZE_MS=5000 (5 s) at balance_app.cpp:530 | **Mismatch.** See TA-11. |
| Same Stage 1: "Ki=2 ... small but non-zero feeds the OnlineMountingEstimator" | main.cpp:290 has Ki=1, doc says Ki=2 | **Drift.** Doc lists Ki=2; flashed value is Ki=1. Minor — both small. balance_app.cpp:63 `kDefaultInitialKi=2.0f` matches the doc; main.cpp overrides to 1.0f after begin(). |

## Section 9 — Recommendations

### Must-fix before next bench

1. **TA-3 + TA-4 (NaN guards in PlantIdentifier and OnlineMountingEstimator).** A single sensor or torn-read glitch latches the entire control stack into a permanent-failure mode. 2-line fix each.
2. **TA-6 (raw_gyro_dps_ atomic write).** The torn-read hazard combined with TA-3 produces the most credible silent-failure path. Wrap the loop-side writes and ISR-side reads in `ATOMIC_BLOCK`.
3. **TA-11 (extend BOOTSTRAP_FREEZE_MS to 30 s).** Five seconds is much shorter than the mount estimator's settling time at the current 20 s LPF. RLS feeds on mount-biased data during the first 25 s of every RUN, leading to a biased K_motor estimate.

### Should-fix before next bench

4. **TA-5 (enable I²C 400 kHz).** Recovers ~6° of phase margin. Requires bench check of pull-up resistors per the comment at bno055.cpp:89-93.
5. **TA-1 (Ki=0 integral handling).** Currently latent (Ki is always positive), but the auto-tune ramp could in principle drive ki_target to zero.

### Nice-to-have

6. **TA-2** (absolute clamp on applied gains) — belt-and-brace against rare misidentification at the projection floor.
7. **TA-7** (confidence-gated EEPROM save) — avoid persisting a known-bad mount offset.
8. **TA-8** (i_term continuity on ki change) — telemetry cosmetics only.
9. **Doc-drift fix**: align main.cpp:290 (Ki=1) and the bootstrap-protocol doc (Ki=2), or update the doc.
10. **Expose σ-projection events** to the host via a `last_projection_ms_` field in PlantIdentifierStatus.

---

## Audit summary

- **Defects by severity**: 0 blockers, 5 important (TA-3, TA-4, TA-5, TA-6, TA-11), 4 minor (TA-1, TA-2, TA-7, TA-8).
- **Sections without defects**: §1 (stability), §3.1 (division by zero), §3.3 (sign), §3.4 (RLS collapse), §6.1-6.3 (cross-module).
- **Stability is theoretically sound**: closed-loop poles are in the LHP for the seed, the rate-limited ramp throughout, and the converged configuration K ∈ [k_min, k_max]. Pole-placement is K-invariant by design.

**Single most important finding**: TA-6 + TA-3 + TA-4 together form a credible silent-failure pathway. A torn float read of `raw_gyro_dps_[1]` produces a wild or NaN value; the un-guarded PlantIdentifier propagates it into θ → kp_target → applied_kp; `pid_.set_tunings(NaN,...)` accepts NaN because the negative-gain guard fails (`NaN<0` is false); the OnlineMountingEstimator latches into NaN as well; the PID's own isnan guard then traps output at `last_output_` — a permanent motor command at whatever PWM was last applied. **Fix the atomic write of `raw_gyro_dps_` AND add NaN guards in PlantIdentifier and OnlineMountingEstimator before motors come back on.** Three small edits eliminate the highest-impact silent failure mode in the stack.
