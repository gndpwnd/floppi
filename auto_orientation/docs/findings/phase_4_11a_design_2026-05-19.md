# Phase 4.11a Design — Encoder-First Position Containment Outer Loop
Status: DESIGN — implementation pending (next session; would conflict with Phase 4M.12 agent currently in `balance_app.cpp`). Encoder-primary counterpart to `research_imu_only_position_containment.md`. With the wheel-encoder driver landed in 4M.10/4M.11, the outer loop USES encoders for position containment and falls back to the IMU-only RCmags SB-1 pitch-double-integration trick when encoders are unhealthy.
Last updated: 2026-05-19

---

## 1. Recap — why position containment matters

A pure pitch-stabilising loop is *translationally underdetermined*: infinitely many wheel-velocity profiles satisfy `pitch ≈ 0`, so floor asymmetries, mount drift, and gain bias pick whichever profile wanders. Bench 2026-05-18 PM: the bot held pitch then walked into a wall in 8-12 s. Cornell ECE4760 ([Chen 2015][cornell]) names this the *speed-accumulation* failure mode.

The collision detector (`COLLISION_PEAK_MPS2 = 12.0`) is *reactive*. Position containment is *preventive* — keeps the bot inside a bounded region in the first place. Together: safety net for unattended bench runs (K-validation sweeps) without the bot migrating into cables or off the workbench edge.

Encoders make this dimensional. The IMU-only path integrates pitch as a *proxy* for translational acceleration (mount-bias caveats); encoders give a direct, drift-bounded wheel-position measurement. Trade-off: encoder hardware fails (magnet, wires, ESD), CPM goes stale on surface change, one wheel can be on a different patch than the other. Both paths must ship, with a runtime gate.

---

## 2. Encoder odometry design

### Tick-to-position math

Standard differential-drive odometry from raw quadrature ticks:

```text
distance_m  = (ticks / cpr) * 2 * π * wheel_radius_m
forward_m   = (dist_left_m + dist_right_m) / 2
heading_rad = (dist_right_m - dist_left_m) / wheel_base_m
```

`WheelEncoder::distance_m()` already implements the per-wheel half; per-wheel velocity is `read_velocity_dps()` / `read_velocity_mps()` (forward-difference over 100 ms window, see `wheel_encoder.h:78-82`).

We **only use forward_m** on the bench balance bot. Heading is intentionally discarded — operator places the bot facing a known direction; outer loop only keeps it from migrating along that axis. Phase 4M.16 (lateral cascade, deferred) reintroduces heading-aware containment once Phase 2.7's `body_heading_unit` ships.

### Position estimator wiring

Integrate per-tick (5 ms @ 200 Hz):

```text
v_forward_mps = 0.5 * (enc_L.read_velocity_mps(now) + enc_R.read_velocity_mps(now))
position_m_  = (position_m_ + v_forward_mps * dt_s) * POS_LEAK
```

`POS_LEAK = 0.9995` @ 200 Hz → ~20 s half-life, matching the mount-estimator LPF. Bounds integration drift to ~10 m even with zero controller action (well beyond bench envelope). Velocity uses `read_velocity_dps()` directly — the driver already does the 100 ms forward-difference; re-differentiating at 200 Hz would amplify quantisation noise.

---

## 3. Outer loop cascade

Standard PI-on-position / P-on-velocity cascade, saturated and slewed:

```text
position_error    = position_setpoint - position_m        (m)
velocity_setpoint = K_POS * position_error                (m/s)
velocity_error    = velocity_setpoint - v_forward_mps     (m/s)
pitch_nudge       = K_VEL * velocity_error                (deg)
pitch_nudge       = saturate(pitch_nudge, ±MAX_NUDGE_DEG)
pitch_nudge       = slew_limit(pitch_nudge, MAX_SLEW_DEG_S * dt)
```

Bot tilts **forward** to accelerate forward, **backward** to decelerate — so if `position_m_ > 0` (drifted forward), pitch_nudge is *negative* (lean back to brake). Sign is hardware-dependent; verify on bench at Phase 4.11a-2 (§11).

### Pseudocode — full step_run_ integration

```cpp
// In BalanceApp::step_run_(), replacing pid_.set_setpoint(0.0f) at line 525:

float BalanceApp::compute_outer_loop_nudge_(uint32_t now_ms) {
    const float dt_s = cfg_.pid_sample_ms * 0.001f;

    // Freeze gate (see §5) — reset on freeze, not just pause.
    if (outer_loop_frozen_(now_ms)) {
        position_m_ = 0.0f;
        last_nudge_deg_ = 0.0f;
        return 0.0f;
    }

    // Runtime ENC-vs-IMU branch (§6).
    if (!encoders_healthy_(now_ms)) {
        outer_loop_mode_ = OuterLoopMode::IMU;
        last_nudge_deg_  = pos_containment_.update(pitch_deg_, dt_s, false);
        return last_nudge_deg_;
    }
    outer_loop_mode_ = OuterLoopMode::ENC;

    // Forward velocity = average of L/R wheel-tread velocities.
    const float v_L = dps_to_mps_(enc_left_.read_velocity_dps(now_ms),
                                  enc_left_.wheel_radius_m());
    const float v_R = dps_to_mps_(enc_right_.read_velocity_dps(now_ms),
                                  enc_right_.wheel_radius_m());
    const float v_forward = 0.5f * (v_L + v_R);
    position_m_ = (position_m_ + v_forward * dt_s) * POS_LEAK;

    // Cascade: PI position → P velocity → pitch nudge.
    const float vel_set = K_POS * (POSITION_SETPOINT_M - position_m_);
    float       nudge   = K_VEL * (vel_set - v_forward);

    // Saturate + slew-limit.
    if (nudge >  MAX_NUDGE_DEG) nudge =  MAX_NUDGE_DEG;
    if (nudge < -MAX_NUDGE_DEG) nudge = -MAX_NUDGE_DEG;
    const float max_step = MAX_SLEW_DEG_S * dt_s;
    const float delta    = nudge - last_nudge_deg_;
    if (delta >  max_step) nudge = last_nudge_deg_ + max_step;
    if (delta < -max_step) nudge = last_nudge_deg_ - max_step;

    last_nudge_deg_ = nudge;
    return nudge;
}

// Caller (balance_app.cpp:525):
pid_.set_setpoint(compute_outer_loop_nudge_(now_ms));
```

---

## 4. Gain derivation

### Path 1 — hardcoded starter gains

For first bring-up; validates the cascade independently of auto-tune.

| Gain | Default value | Units | Derivation |
|---|---:|---|---|
| `K_POS` | **0.2** | (m/s) per m | Time constant ~5 s for position error to translate to velocity command. Far slower than inner pitch loop. |
| `K_VEL` | **0.5** | deg per (m/s) | At 0.1 m/s drift, requests 0.05° tilt — well under MAX_NUDGE_DEG. |
| `MAX_NUDGE_DEG` | **1.5** | deg | Same as IMU-only design; deeper tilts approach linear-region edge (~5-8°). |
| `MAX_SLEW_DEG_S` | **5.0** | deg/s | Cap on setpoint rate-of-change. Human-visible, not motor-jerky. |
| `POS_LEAK` | **0.9995** | per tick @200 Hz | ~20 s half-life; matches mount estimator. |
| `POSITION_SETPOINT_M` | **0.0** | m | Origin = wherever RUN was entered. |

Start gentle — cascade only *nudges* the inner loop. Oscillation → halve gains. Wander >0.5 m in 30 s → double them.

### Path 2 — auto-derived from `PlantIdentifier::K_motor`

Once BOOTSTRAP measures K_motor, derive outer gains by pole-placement (same trick as inner Kp/Kd):

```text
ω_n_outer = (4 / ts_inner) / 5            (outer ≥ 5× slower for stability)
K_POS     = ω_n_outer² / K_motor / C_BOT  (m/s)/m
K_VEL     = 2ζω_n_outer / K_motor / C_BOT (deg)/(m/s)
```

Units aren't dimensionally automatic — K_motor is (deg/s²)/PWM; K_POS wants (m/s)/m. Scaling constant `C_BOT ≈ wheel_radius_m * PWM_PER_DEG_TILT` bridges them. Defer to Phase 4.11a-3 post-empirical: bench-measure working K_POS/K_VEL, back-solve C_BOT from BOOTSTRAP K_motor. Once C_BOT is stable across two or three robots, the formula replaces hardcoded gains. Mirrors `bootstrap_protocol_unstable_plant.md` — bot learns its own dynamics, no operator-provided gains. Matches [universal balance vision][vision].

---

## 5. Activation conditions / freeze gates

The outer loop is **disabled** (returns nudge=0, resets `position_m_`) in any of these conditions. Same pattern as `PlantIdentifier::freeze_` (`balance_app.cpp:546-547`):

| Condition | Why | Implementation |
|---|---|---|
| State is not RUN | Outer loop only runs during balance | `state_ != BalanceAppState::RUN` |
| BOOTSTRAP recently exited (< 500 ms ago) | Inner loop hasn't settled | `(now_ms - run_entered_ms_) < OUTER_LOOP_BOOTSTRAP_FREEZE_MS` |
| `soft_cutoff` active | Inner PID in saturation/recovery | `pid_.in_soft_cutoff()` (existing) |
| `sat_consecutive_ticks_ > 0` (windup_active) | Inner loop saturated; cascade would slow recovery | check `adaptive_active_` + `sat_consecutive_ticks_` |
| HELD state | Bot picked up; encoder readings meaningless | `state_ == HELD` already excluded by state gate |
| FALLEN state | Bot tipped; no recovery without operator | excluded by state gate |
| CHAR_ACT / BOOTSTRAP / CHAR_PWM_RANGE | Motors driven for measurement, not balance | excluded by state gate |
| Encoder data invalid | NaN, |velocity| > 100× expected | `!std::isfinite(v_forward_mps) \|\| fabs(v_forward_mps) > MAX_PLAUSIBLE_MPS` |
| Collision detected (< 200 ms ago) | Recovery in progress; cascade would fight | check `collision_latched_` + recency |

`POS_RESET_ON_FREEZE = true`: entering a frozen condition **resets `position_m_` to 0** (not just pauses). Avoids stale value preservation across HELD→RUN bounce when operator physically translates the bot.

Gate impl:

```cpp
bool BalanceApp::outer_loop_frozen_(uint32_t now_ms) const {
    if (state_ != BalanceAppState::RUN) return true;
    if ((now_ms - run_entered_ms_) < OUTER_LOOP_BOOTSTRAP_FREEZE_MS) return true;
    if (sat_consecutive_ticks_ >= 2) return true;       // windup
    if (collision_latched_ &&
        (now_ms - collision_latched_ms_) < OUTER_LOOP_COLLISION_FREEZE_MS) return true;
    return false;
}
```

---

## 6. Fallback to IMU-only mode

If `!defined(USE_WHEEL_ENCODERS)` OR `!encoders_healthy_()`, fall through to option-B RCmags SB-1 pitch double-integrator (`research_imu_only_position_containment.md`). Same input→nudge contract; cascade unchanged.

```cpp
bool BalanceApp::encoders_healthy_(uint32_t now_ms) {
#ifndef USE_WHEEL_ENCODERS
    return false;
#else
    // 1: stall (PWM>100 for >500ms but no ticks) → dead encoder
    if (enc_left_.stalled(100, 500) || enc_right_.stalled(100, 500)) return false;
    // 2: calibration valid (CPM not stale)
    if (!encoder_cal_valid_) return false;
    // 3: velocity not absurd (>5000 dps ≈ 14 rev/s)
    if (abs(enc_left_.read_velocity_dps(now_ms))  > 5000) return false;
    if (abs(enc_right_.read_velocity_dps(now_ms)) > 5000) return false;
    return true;
#endif
}
```

Telemetry tags `mode=IMU` vs `mode=ENC` for operator confirmation. §9 scenario 4 exercises mid-run mode flip. Matches `MEGA_UNIVERSAL_PLAN.md §8`: ship both paths because encoders fail, cal goes stale, and A/B flag-flip is the only way to validate encoder tuning against the IMU baseline.

---

## 7. Telemetry

Live (5 Hz, throttled by existing loop-side print scheduler):

```text
pos: 0.123m vel: 0.04m/s nudge: -0.3deg mode: ENC freeze: 0
```

`freeze: 1` reports gate closed — useful for diagnosing why containment isn't responding.

Long (30 s rolling stats, printed on demand via new `?pos` serial command):

```text
pos_window_30s: min=-0.08m max=+0.21m mean=0.04m max_excursion=0.21m
mode_breakdown: ENC=180.0s IMU=0.0s frozen=12.5s
nudge_window_30s: min=-1.20deg max=+0.50deg mean=-0.05deg
```

Bench acceptance (§9 scenario 1): `max_excursion < 0.30m` over 30 s quiet bench run.

---

## 8. EEPROM persistence

New slot `0x230` (next free after `0x220` which is the encoder calibration from Phase 4M.11):

| Offset | Bytes | Field | Notes |
|--:|--:|---|---|
| 0 | 1 | magic = 0xAE | Identifies the slot |
| 1 | 1 | version = 0x01 | Bump on schema change |
| 2 | 4 | K_POS (float LE) | Position gain |
| 6 | 4 | K_VEL (float LE) | Velocity gain |
| 10 | 4 | wheel_base_m (float LE) | For Phase 4M.16 lateral cascade |
| 14 | 4 | position_bound_m (float LE) | Default 1.0 m |
| 18 | 4 | max_nudge_deg (float LE) | Default 1.5° |
| 22 | 4 | max_slew_deg_s (float LE) | Default 5.0°/s |
| 26 | 4 | C_BOT (float LE) | Phase 4.11a-3 auto-derivation constant |
| 30 | 1 | crc8 | XOR of bytes 0-29 |

Total: **31 bytes** (within Mega EEPROM's 4096 B; well under headroom).

Hex addresses recap for the codebase audit trail:

| Slot | Address | Owner | Status |
|---|---|---|---|
| Mount | `0x200` | OnlineMountingEstimator offset | DONE |
| Actuator | `0x210` | Stiction floor + PWM range (Phase 4M.12) | DONE/IN-PROGRESS |
| Encoder | `0x220` | CPM left/right + wheel radius (Phase 4M.11) | DONE |
| **Outer loop** | **`0x230`** | **K_POS, K_VEL, bounds, C_BOT** | **Phase 4.11a-5** |
| Reserved | `0x240+` | Phase 4.12+ (washout, lateral cascade) | RESERVED |

Save trigger: any successful BOOTSTRAP that derives new gains, or explicit `?save_outer` serial command after hand-tune.

---

## 9. Native test plan

Every Phase 4.11a-N sub-task lands with its own native test before moving on.

| # | Scenario | Setup | Expected |
|--:|---|---|---|
| 1 | Quiet bench | `position_m_=0`, `v_forward=0`, 6000 ticks, zero disturbance | `\|position_m_\| < 0.10 m`; `last_nudge_deg_ ≈ 0` |
| 2 | Persistent forward tilt | inject `pitch_deg_=+0.3°` (mount-drift sim); inner PID drives wheels forward | cascade computes negative nudge; bot settles bounded `< 0.5 m` |
| 3 | Position step | inject `position_m_=+0.5 m` (push sim) | negative nudge; returns to `\|position_m_\| < 0.10 m` within **5 s** |
| 4 | Encoder fail mid-trial | run scenario 1 for 5 s; flatline `enc_left_` ticks despite PWM | `encoders_healthy_()`=false inside `OUTER_LOOP_STALE_MS`; clean ENC→IMU; no nudge spike |
| 5 | Freeze gates | force each freeze (BOOTSTRAP, soft_cutoff, sat≥2, collision_latched_); inject `position_m_=+1.0 m` | nudge=0.0 AND `position_m_` reset to 0 |
| 6 | Slew limit | compute nudge that wants +5.0° | actual nudge increments by `MAX_SLEW_DEG_S*dt_s` per tick; clamps at MAX_NUDGE_DEG=1.5° |
| 7 | Sign correctness (paired with bench) | inject `position_m_=+0.3 m` (drifted forward) | nudge **negative** (lean back to brake) |

Native harness uses `WheelEncoder::set_ticks_for_test()` hooks (`wheel_encoder.h:204-217`).

---

## 10. Risks + edge cases

### Risk R1 — Bot on a slope

Outer loop fights gravity indefinitely; battery drains. **Mitigation**: timeout. If `|last_nudge_deg_| > 1.0°` for **30 s**, log warning and reset `position_m_` to 0. Operator short-press restarts RUN.

### Risk R2 — Mis-calibrated encoders

Wrong CPM → wrong velocity → wrong nudge → bot drifts the *wrong way*. Worst case: encoder reports negative velocity when bot moves forward; cascade nudges forward; bot accelerates into wall. **Mitigation**: Phase 4M.14 cross-check. BOOTSTRAP compares `K_motor_gyro` vs `K_motor_encoder`; >30% disagreement → fail with `failure_reason=7` (K_disagreement). Operator told to check polarity / CPM / binding.

### Risk R3 — Operator picks up bot (HELD)

Position estimate goes wild. **Mitigation**: `POS_RESET_ON_FREEZE=true` (covered in §5). HELD→RUN re-enters with position origin at the put-down point.

### Risk R4 — Encoder sane but mechanically decoupled

Stripped gear: encoder reports rotation, wheel doesn't push bot. **Mitigation**: hard to detect from encoders alone; Phase 4M.14 K cross-check catches the BOOTSTRAP-pulse case. Runtime: `PlantIdentifier::K_motor` drift vs bench-measured value flags "K drifted" in telemetry.

### Risk R5 — Single-wheel failure with averaging

`(v_L + v_R)/2` masks a single-wheel failure (one wheel stuck, bot actually turning). **Mitigation**: `encoders_healthy_()` checks each wheel's stall flag independently; either-wheel-stalled → IMU fallback. Phase 4M.16 (lateral cascade) properly disambiguates.

### Risk R6 (the one the original research did NOT anticipate)

**Cascade-vs-mount-estimator competition during slow battery sag.** Both loops respond to slow forward drift: the mount estimator (20 s LPF) interprets it as new mount offset and *moves pitch zero forward*; the outer loop interprets it as position error and *requests backward tilt nudge*. Not orthogonal — the mount-offset shift removes the pitch-bias signal driving the cascade's nudge, but on a 20 s timescale.

Result: for ~20 s the outer loop holds the bot via backward nudge. Once the mount estimator catches up and absorbs the forward bias into the zero point, the outer-loop nudge drops to zero, **abruptly releasing the cascade's restoring force**. The bot lurches forward as the inner setpoint snaps to the new mount-corrected zero.

**Mitigation**: gate mount estimator on `|outer_loop_nudge_deg| < 0.3°` (same shape as the existing `windup_active` freeze). Mount estimator only absorbs bias when the cascade isn't fighting drift. If nudge > 0.3° for > 5 s, that's real position drift; let the cascade correct it instead of letting the mount estimator silently move the goalposts.

Not apparent in the IMU-only research: there, *pitch IS the position proxy* — both loops live on the same signal, so mount-bias absorption naturally unwinds the position estimator too. With encoders, position is independent of pitch; the mount estimator can move underneath the cascade without the cascade noticing until its restoring force vanishes.

---

## 11. Implementation phasing

| Phase | Scope | Test gate | Effort |
|---|---|---|--:|
| **4.11a-1** | Odometry math only. Add `position_m_`, `outer_loop_mode_`, leak integrator. No control output. | Scenarios 1, 7. Bench: hand-push 1 m, position reads ~1 m. | S |
| **4.11a-2** | Hardcoded cascade. Math + saturation + slew + freeze gates + IMU fallback. K_POS=0.2, K_VEL=0.5. | Scenarios 1-7 native. Bench: scenario 3 (push returns). | M |
| **4.11a-3** | Auto-derived gains. C_BOT scalar; derive K_POS, K_VEL from BOOTSTRAP K_motor. | Scenarios 2-3 with derived gains; bench A/B. | M |
| **4.11a-4** | Risk polish. R1 slope timeout, R6 mount-gate, R5 per-wheel health. | Add scenarios 8-10. | S |
| **4.11a-5** | EEPROM `0x230` + `?pos` + `?save_outer`. | Reboot test: gains persist. | S |

Each sub-phase ends with its own bench check; don't stack two into one session.

---

## 12. Effort estimate

| Sub-phase | Effort | Cumulative |
|---|--:|--:|
| 4.11a-1 | 0.3 session | 0.3 |
| 4.11a-2 | 0.7 session | 1.0 |
| 4.11a-3 | 0.6 session | 1.6 |
| 4.11a-4 | 0.4 session | 2.0 |
| 4.11a-5 | 0.5 session | 2.5 |

**Total: 2-3 sessions.** 4.11a-1+2 fit in one (estimator-only validates wiring before cascade goes live). 4.11a-3+4 fit in a second (hardcoded vs derived A/B). 4.11a-5 (half session) rides alongside polish.

Largest unknown: bench-tune of K_POS/K_VEL in 4.11a-2. Hardcoded starters are conservative; oscillation → halve, wander → double. 2-3 trials should converge.

---

## See also

- [`research_imu_only_position_containment.md`](research_imu_only_position_containment.md) — IMU-only fallback (option B pitch-double-int).
- [`research_wheel_encoders_mega_2026-05-19.md`](research_wheel_encoders_mega_2026-05-19.md) — encoder hw/driver/cal/integration; §6 cascade sketch this fleshes out.
- [`MEGA_UNIVERSAL_PLAN.md`](../MEGA_UNIVERSAL_PLAN.md) §8 — encoder-first with runtime fallback (the plan this implements).
- [`UNIVERSAL_BALANCE_BOT_VISION.md`](../UNIVERSAL_BALANCE_BOT_VISION.md) — no-per-bot-config north star; gains auto-derive (Phase 4.11a-3).
- [`bootstrap_protocol_unstable_plant.md`](bootstrap_protocol_unstable_plant.md) §6 — K-quality gate; 4M.14 adds encoder cross-check (R2 mitigation).
- [`online_adaptive_balance_tracking.md`](online_adaptive_balance_tracking.md) — mount-estimator time-scales; **R6** cascade-vs-mount competition is the new constraint on mount gating.
- [`research_collision_signature_bno055.md`](research_collision_signature_bno055.md) — collision detector this cascade complements (reactive + preventive pair).
- [`src/sensors/wheel_encoder.h`](../../src/sensors/wheel_encoder.h) — driver API consumed here.
- [`src/applications/balancing_robot/balance_app.cpp:525`](../../src/applications/balancing_robot/balance_app.cpp) — `step_run_` integration point.

---

## References

[cornell]: https://people.ece.cornell.edu/land/courses/ece4760/FinalProjects/f2015/dc686_nn233_hz263/final_project_webpage_v2/dc686_nn233_hz263/index.html
[cascade]: https://journals.sagepub.com/doi/full/10.5772/63933
[rcmags-blog]: https://rcmags.github.io/projects/robots/2019/06/11/self_balancing_robot.html
[vision]: ../UNIVERSAL_BALANCE_BOT_VISION.md

- [Chen, Niu & Zhu (2015) — Cornell ECE4760 Self-Balancing Robot][cornell] — speed-accumulation failure mode.
- [Velazquez et al. (2016) — Cascade control strategy for self-balancing vehicles][cascade] — encoder cascade reference.
- [ArduPilot Balance Bot tuning](https://ardupilot.org/rover/docs/balance_bot-tuning.html) — `BAL_PITCH_TRIM`/`BAL_PITCH_MAX` pattern replicated here.
- [PaulStoffregen/Encoder](https://github.com/PaulStoffregen/Encoder) — quadrature decoder backing `WheelEncoder`.
