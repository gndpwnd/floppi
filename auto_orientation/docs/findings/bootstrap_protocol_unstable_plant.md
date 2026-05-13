# Bootstrap Protocol for an Unstable Plant

Status: PROPOSAL — implementation across Phase 4.5d, 4.7d, 4.10 (new sub-phases).
Last updated: 2026-05-12

## 1. The problem in one paragraph

An inverted pendulum is open-loop unstable: with no controller it falls in
under a second, and there is no quiescent regime in which we can sit and
gather plant-ID data. Every byte of information about the plant we will
ever have comes out *while the plant is being controlled by a controller
that already exists*. That makes single-pass auto-tuning impossible — we
cannot identify K_motor before the bot is alive, and the bot cannot be
alive without gains. The only solution is to **layer** the tuning. Start
with a deliberately conservative seed that won't damage anything, survive
the first five seconds, then learn one thing at a time, in an order where
each learned quantity makes the next one identifiable. The protocol is
sequential because the plant gives us no other option.

## 2. The protocol — 6 stages

### Stage 0: COLD BOOT — what we know and don't

Known at power-on (from EEPROM): BNO055 sensor calibration, reference
mounting offset (last `c` capture, loaded in `main.cpp`), and optionally a
refined mounting offset from a prior Stage 2 convergence (NEW — §5).

Unknown: motor PWM→acceleration scalar `K_motor` (drifts 10-25% with
battery / wear / surface), true dynamic balance point (tether and payload
shift it ≤5° from captured reference), and effective gain change since
the last session.

The operator just pressed power. They will prop the bot upright in the
next few seconds. Everything after that is firmware's problem.

### Stage 1: SEED GAINS — keep alive long enough to learn

Hardcoded at `balance_app.cpp:62-64`:

```text
Kp = 50.0    Ki = 2.0    Kd = 20.0    I-term clamp = ±40 PWM
```

Reasoning: Kp=50 means a 1° error produces 50 PWM, comfortably linear
(L298N saturation at ~200, stiction wall at ~25). Ki=2 is small but
non-zero — it's the *signal* the OnlineMountingEstimator reads to converge
Stage 2 (`balance_app.cpp:417-424`). Ki=0 would stall Stage 2 forever. Kd=20
is the primary damping; the BNO055 NDOF Euler has ~0.05° quantization, and
the 15 ms measurement LPF in `pid_controller.cpp` keeps Kd × dθ/dt below
spiking thresholds. The **I-term clamp at ±40 PWM** (anti-windup cap that
landed 2026-05-12 evening) is the seed's most important safety property:
without it, a 2 s tipover winds the integrator to several hundred PWM and
the bot launches on the next zero crossing.

These are a class-typical seed for small chassis (mass ≤ 1 kg, height
≤ 30 cm, cheap brushed motors, L298N). Bots outside that envelope will
need a different seed; this document targets the small class.

**Stage 1 completion**: bot in RUN with `|pitch| < 8°` continuously for
≥ 5 s and no saturation events > 500 ms.

### Stage 2: MOUNT-OFFSET CONVERGENCE — fix the "always falls forward" bug

`OnlineMountingEstimator` is already running during RUN. It samples
`pid_.get_i_term()` each tick and updates a 60 s LPF on the implied
mounting offset. As the LPF converges, `corrected_pitch_()` subtracts the
estimate, the I-term drives back toward zero, and the energy that was
being spent against gravity moves into the geometric offset where it
belongs. Time constant: 60 s; a 3° initial error needs ~90 s of stable
balancing to drive below 0.5°.

**Stage 2 → 3 completion rule** (NEW work, Phase 4.5d):

> `mean(|I-term|) < 10 PWM` over the last 10 s AND `|d(offset)/dt| <
> 0.05°/s` AND `time in RUN ≥ 30 s`.

Both magnitude AND derivative must hold. Magnitude alone is fooled by a
quiet pickup-replace (I-term resets); derivative alone is fooled at the
slow zero-crossing of a drift.

**On convergence**: write the refined offset to EEPROM, replacing the
reference. Next cold boot starts with Stage 2 already complete — protocol
degrades from 90 s on a fresh bot to ~5 s on re-power.

**Critical freeze gate**: the estimator MUST freeze during HELD AND
during transients (`|gyro_pitch| > 60 °/s` OR `|pitch| > 3°`). Without
this gate, a 2 s pickup contaminates the LPF with the recovery transient
and the bot chases a wrong offset for the next minute. This is the
highest-risk piece of plumbing in the protocol (see §4 and §6 audit).

### Stage 3: PLANT IDENTIFICATION — learn K_motor

Scalar RLS regression on the pitch equation of motion
(`dynamic_pwm_accel_learning.md` §4a):

```text
α_pitch_measured  ≈  K_motor · (pwm_L + pwm_R)/2  +  g_eff · sin(pitch)
```

`g_eff = m·g·d/I` is a chassis geometry constant — hardcoded to a
class-typical value (~50-80 rad/s² for small chassis, CoM ~8 cm above
axle). We learn ONLY `K_motor`. RLS with forgetting factor λ = 0.998
(~25 s memory) converges from a sensible prior to within ±10% of true in
**10-30 s** of natural balance operation. Disturbances and PID reactions
provide sufficient excitation — no special maneuver needed.

Cost: 3 floats of state, ~30 µs/tick on AVR. Trivial.

**Stage 3 → 4 completion**: RLS covariance `P[n] < 0.05 · P[0]` (variance
dropped to 5% of prior — concretely, with `P[0] = 1.0` we want `P[n] <
0.05`), AND running-mean K_motor change < 5% over last 5 s.

### Stage 4: GAIN REFINEMENT — apply learned plant

With `K_motor` and known `g_eff` we have enough plant to pick gains
analytically. AMIGO heuristic (Hägglund & Åström 2002 — less aggressive
than classic Z-N, appropriate when overshoot means "bot falls over"):

```text
Ku  ≈  g_eff / K_motor          Tu  ≈  2π · sqrt(1 / g_eff)   (~0.7-1.0 s)
Kp  =  0.45 · Ku
Ki  =  Kp / (0.85 · Tu)
Kd  =  Kp · 0.125 · Tu
```

**One method, deliberately chosen.** No family of options exposed.

**Slow application**: gains ramp at **5%/s** from current to target. A
misestimate cannot suddenly destabilize because the ramp is slower than
the plant's own time constant. Worst-case 20 s for full ramp.

The relay-feedback tuner (`AutoPIDTuner` + `RelayFeedbackStrategy`,
already implemented) is an OPTIONAL Stage 4 verification — operator
presses `t`, relay runs ~30 s, result either replaces analytical gains
(if tighter and inside bounds) or is discarded. Refinement, not
substitute.

**Stage 4 → 5 completion**: ramp complete for ≥ 10 s AND pitch RMS over
those 10 s hasn't increased > 50% vs pre-ramp baseline. If it did, revert
one step (not all the way to seed) and mark `gain_refinement_status =
"reverted"`.

### Stage 5: CONTINUOUS ADAPTATION

Steady state. Three things keep running:

1. RLS on K_motor with λ = 0.998 — tracks battery sag, motor wear,
   payload changes; gains slowly re-applied (5%/s) as K_motor moves.
2. OnlineMountingEstimator 60 s LPF — slow offset drift (tether, packing).
3. HELD detection — when entered, both adaptive estimators freeze
   immediately. Resume on RUN re-entry.

No transition out except: operator presses `t` to force a relay tune
(returns to Stage 4 path); operator presses `a` to abort to IDLE.

## 3. Detection criteria summary

| Transition | Rule |
| --- | --- |
| 0 → 1 | Implicit on power-on. Seed gains active by `begin()`. |
| 1 → 2 | RUN with `\|pitch\| < 8°` continuously ≥ 5 s, no saturation > 500 ms. |
| 2 → 3 | `mean(\|I-term\|) < 10 PWM` over last 10 s AND `\|d(offset)/dt\| < 0.05°/s` AND time-in-RUN ≥ 30 s. |
| 3 → 4 | RLS `P[n] < 0.05` AND running-mean K_motor change < 5% over last 5 s. |
| 4 → 5 | Ramp complete ≥ 10 s AND pitch RMS not > 1.5× pre-ramp baseline. |

Each rule produces a boolean every PID tick. A "consecutive ticks" counter
(same pattern as `hold_enter_count_` at `balance_app.cpp:362-371`) requires
the rule continuously true for **1 s (200 ticks)** before firing,
preventing glitch transitions.

## 4. Failure modes and fallbacks

In every failure case the principle is the same: **the bot keeps
balancing on whatever was last good**. We never fail-stop into FALLEN
because a tuning estimator hit a snag. Seed gains are always the parachute.

- **Stage 1 stuck (never gets 5 s upright)**: seed gains wrong for this
  chassis, OR mount offset wildly off (operator captured against a tilted
  surface), OR motor lead reversed, OR IMU axis mismatch. Surface:
  `STAGE 1 STUCK — check motors, IMU mounting, mount-offset capture`. No
  auto-retry; operator inspects.

- **Stage 2 not converged in 90 s**: mount offset > ±5° from reference,
  which is the hard clamp `online_adaptive_balance_tracking.md` §6
  enforces. Beyond ±5° the discrepancy is physical and operator must
  re-capture. Surface: `STAGE 2 STUCK — re-capture mount (press c)`.
  Estimator freezes at clamp boundary; bot keeps balancing on seed gains
  forever. "Good enough", just not optimal.

- **Stage 3 RLS variance won't drop**: bot too quiet (small Kp, low
  excitation, poor identifiability) or too noisy (constant disturbance,
  estimator chases noise). After 60 s with no convergence: surface
  `STAGE 3 STUCK — running on seed gains, K_motor not identified`. Bot
  continues balancing; auto-retry every 60 s.

- **Stage 4 ramp degrades performance** (pitch RMS doubles in 10 s
  post-ramp window): revert to previous gains immediately — one tick,
  not a ramp-back; instability won't wait. Re-attempt only on operator
  request.

- **Stage 5 anomaly (K_motor leaves [0.5, 2.0] × nominal)**: freeze RLS,
  fall back to last-known-good gains, surface `PLANT_OUT_OF_BOUNDS —
  wheel slipping? loose lead?`. Bot keeps balancing. This is the "free
  hardware-fault indicator" from `dynamic_pwm_accel_learning.md` §2.

## 5. Required infrastructure

- **Bootstrap stage tracking**: new enum `BootstrapStage { SEED,
  MOUNT_CONVERGED, PLANT_IDENTIFIED, GAINS_REFINED, ADAPTIVE }` on
  `BalanceApp`. Orthogonal to `BalanceAppState` (IDLE / RUN / HELD) —
  bootstrap stage describes *what we know*, app state describes *what
  we're doing*. You can be in any bootstrap stage while in HELD; freeze
  gates handle the interaction.

- **Transition method**: `try_advance_bootstrap_(now_ms)` called from
  `step_run_()` once per tick. Checks current stage's completion rule,
  advances if met, persists side-products (Stage 2 → write refined
  offset; Stage 3 → write learned K_motor as next-boot RLS prior).

- **Persistence extension**: existing EEPROM mounting-calibration record
  (Phase 4.3, 34 B) grows to include `refined_mount_offset_rad` (4 B,
  Stage 2 product) and `learned_K_motor` (4 B, Stage 3 product), each
  with a 1 B confidence/flags byte. Wear-levelled across two slots
  (already designed). Total record ~48 B.

- **Operator UX**: new `b` serial command (print current bootstrap stage
  and which completion rules are passing/failing). Optional LED color
  encoding (red Stage 0-1, yellow Stage 2-3, green Stage 4-5).

- **Logging**: each transition writes one `[bootstrap]`-tagged Serial
  line so we have a forensic trail when something goes wrong on the
  bench.

## 6. What's already in the codebase

Audit-style status (line references as of 2026-05-12):

- **Stage 1 seed gains**: hardcoded at `balance_app.cpp:62-64`. I-term
  clamp ±40 PWM landed this evening. DONE.
- **Stage 2 estimator**: `OnlineMountingEstimator` running in RUN at
  `balance_app.cpp:416-425`. DONE — but no completion detection, no
  persistence write-back. That's the Phase 4.5d work.
- **Stage 2 freeze gate**: PRESENT but currently passes
  `windup_active=false, gyro_pitch_dps=0.0f` placeholders
  (`balance_app.cpp:420-422`). Mathematically correct, receiving
  constant inputs. **This is the most important open bug for the
  protocol's correctness.** Fix is wired into Phase 4.6.5 (raw gyro on
  `OrientationSensor`).
- **Stage 3 RLS K_motor**: designed in `dynamic_pwm_accel_learning.md`
  §4a. NOT implemented. Phase 4.10a.
- **Stage 4 gain refinement**: not designed in detail until this doc.
  Phase 4.10b.
- **Stage 5 continuous adaptation**: partial via OnlineMountingEstimator;
  not implemented for gains.
- **Relay tuner**: implemented (`AutoPIDTuner` + `RelayFeedbackStrategy`).
  Available as optional Stage 4 verification.
- **HELD detection**: implemented at `balance_app.cpp:362-371`. Will
  additionally need to signal "freeze bootstrap estimators" once 4.10a
  lands.

## 7. Concrete implementation order

Estimated **~13 hours of focused work**, four new sub-phases. Order is
strict — each depends on the previous landing and being verified on hardware.

- **Phase 4.5d "Stage 2 convergence + persistence" (3 h)** — completion
  rule (§3 row 2→3), persist refined offset to EEPROM, load preferentially
  over reference on next boot. Wire real `windup_active` and
  `gyro_pitch_dps` into `online_est_.update()`. **BLOCKS all subsequent
  stages** — Stage 3's RLS needs a stable balance regime to identify
  K_motor cleanly, and that regime is exactly what Stage 2 establishes.

- **Phase 4.10a "Scalar RLS for K_motor" (4 h)** — implement
  `PlantIdentifier` per `dynamic_pwm_accel_learning.md` §8 step 1. Sibling
  to `OnlineMountingEstimator`; same freeze gates, separate state. Runs in
  RUN only. Surfaces `K_motor` and covariance; does NOT yet affect gains.

- **Phase 4.10b "Closed-form gain mapping" (3 h)** — AMIGO mapping from
  learned `K_motor` to target Kp/Ki/Kd, the 5%/s rate-limited ramp, the
  pitch-RMS pre/post comparison, and the revert path. Depends on 4.10a
  producing a converged K_motor.

- **Phase 4.10c "Bootstrap stage machine" (3 h)** — orchestration:
  `BootstrapStage` enum, `try_advance_bootstrap_()`, transition logging,
  operator `b` command, LED hookup, EEPROM record extension. Wires it
  all together. Depends on 4.10b.

Dependency block:

```text
Phase 4.6.5 (raw gyro)  ─┐
                         ├──>  4.5d  ──>  4.10a  ──>  4.10b  ──>  4.10c
```

Phase 4.6.5 (raw gyro on `OrientationSensor`) is already on the critical
path for the 2-state Kalman in `MASTER_DESIGN.md` — not new work for this
protocol, but a hard prerequisite for the Stage 2 freeze-gate fix.

## 8. What we are NOT doing

- **Neural-network policies / reinforcement learning / end-to-end deep
  control**: not on a 2 KB AVR. Documented out-of-scope in
  `dynamic_pwm_accel_learning.md` §4c. Revisit on Teensy / ESP32-S3 as
  far-future research.
- **Per-bot config files** (mass, height, CoM, wheel radius, motor Kv):
  explicitly forbidden by `UNIVERSAL_BALANCE_BOT_VISION.md`. Whole point
  of the protocol is to avoid these.
- **Operator-triggered tuning sequence**: operator's role is power-on and
  prop-upright, full stop. `t` (force relay tune), `c` (re-capture), `b`
  (status), `a` (abort) exist for development and edge cases — not the
  baseline workflow.
- **Single-pass auto-tune**: not possible on an unstable plant. The whole
  document is the justification for why we don't try.
- **Loss of seed gains**: Stage 1 seeds stay in flash forever as the
  immutable fallback. Adaptive gains live in RAM only (operator memory
  preference: "gains do NOT persist, but mount offset DOES"). Refined
  gains are NOT written to EEPROM.
- **Adapting during HELD or FALLEN**: all bootstrap estimators freeze on
  HELD entry and stay frozen until RUN resumes. Prevents pickup-and-
  replace transients from poisoning the model.

## 9. Open questions for the user

1. **Boot pose tolerance**: must the bot be propped at approximately
   balance (±10° upright) on power-on, or should firmware boot from any
   angle and self-prop (new pre-Stage-1 wait-for-upright state)? Current
   behavior is the former; latter is easy to add.

2. **Total bootstrap time on a brand-new bot**: protocol-paper estimate
   is **60-120 s end-to-end** (Stage 1: 5 s, Stage 2: 30-90 s, Stage 3:
   10-30 s, Stage 4: 30 s). Willing to wait that long? If not we can
   accept lower confidence at each transition and compress to ~30 s at
   the cost of higher revert/retry rates.

3. **Persistence policy** (confirm against memory entry):
   - Refined mounting offset: persist YES (overwrites reference after
     Stage 2 completes).
   - Learned K_motor: persist YES (informs RLS prior on next boot —
     starts from `P[0] = 0.1` instead of 1.0 if saved).
   - Refined PID gains: persist NO (always rebuilt from K_motor in
     Stage 4 each session — matches "dynamic gains" preference).
   Confirm this split.

4. **Failure surfacing**: error strings like "STAGE 2 STUCK" — Serial
   only, optional LED, dashboard (Phase 6), or all? Recommendation: all,
   with Serial as canonical.

## 10. References

### In-repo

- [`../AUTO_TUNING_REALITY_CHECK.md`](../AUTO_TUNING_REALITY_CHECK.md) —
  high-level "why step-by-step" this doc operationalizes.
- [`../UNIVERSAL_BALANCE_BOT_VISION.md`](../UNIVERSAL_BALANCE_BOT_VISION.md) —
  design north star (no per-bot config).
- [`../MINIMIZE_ACCELERATIONS_PHILOSOPHY.md`](../MINIMIZE_ACCELERATIONS_PHILOSOPHY.md) —
  controller-architecture companion. The PID-on-pitch loop being bootstrapped is the controller this doc commits to.
- [`auto_pid_tuning_research.md`](auto_pid_tuning_research.md) — relay-feedback tuner (Phase 4.5b, implemented). Stage 4 optional verification.
- [`dynamic_pwm_accel_learning.md`](dynamic_pwm_accel_learning.md) — Phase 4.10 RLS plant-ID design. Stage 3 implementation reference.
- [`online_adaptive_balance_tracking.md`](online_adaptive_balance_tracking.md) — Stage 2 estimator design (implemented).
- [`balance_failure_diagnosis_2026-05-12.md`](balance_failure_diagnosis_2026-05-12.md) — root-cause work that explains why seed gains had to be conservative and why the freeze gate matters.
- [`latency_budget_2026-05-12.md`](latency_budget_2026-05-12.md) — cap on
  how aggressively any stage can tune given BNO055 NDOF group delay.
- [`../../src/applications/balancing_robot/balance_app.cpp`](../../src/applications/balancing_robot/balance_app.cpp) — current state machine. Lines 62-64 (seed gains), 416-425 (online estimator update), 362-371 (HELD entry).

### External

1. **Åström, K. J. and Hägglund, T. (1984).** *Automatic tuning of simple
   regulators with specifications on phase and amplitude margins.*
   Automatica 20(5), 645–651. Foundation of the relay tuner.
2. **Hägglund, T. and Åström, K. J. (2002).** *Revisiting the
   Ziegler–Nichols Step Response Method for PID Control.* Journal of
   Process Control 14(6), 635–650. AMIGO rules used in Stage 4.
3. **Åström, K. J. and Wittenmark, B. (1995).** *Adaptive Control*, 2nd
   ed., Addison-Wesley. Ch. 2 (RLS), Ch. 5 (self-tuning regulators —
   Stage 3+4 architecture), Ch. 11 (supervision, freeze gates).
4. **Slotine, J.-J. E. and Li, W. (1991).** *Applied Nonlinear Control*,
   Prentice Hall. Ch. 8 — adaptive control of mechanical systems with
   inertial uncertainty. Theoretical justification for K_motor-only
   regression in Stage 3.
5. **Ljung, L. (1999).** *System Identification: Theory for the User*,
   2nd ed., Prentice Hall. RLS with forgetting factor; excitation
   sufficiency for the Stage 3 completion rule.
6. **Anderson, B. D. O. (2005).** "Failures of adaptive control theory
   and their resolution," *Communications in Information and Systems*
   5(1). Bursting and parameter drift when two adaptive loops coexist
   (Stage 2 + Stage 3). Motivates the K_motor freeze gate in §4.
