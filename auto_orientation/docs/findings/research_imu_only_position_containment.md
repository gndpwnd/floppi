# IMU-Only Position Containment for the Balance Bot
Status: RESEARCH — supports [operator_ideas_backlog.md] (next entry). The bot currently balances pitch but wanders, accumulates translational drift, and collides with walls during bench tests. This document evaluates how to add a *position-restoring outer loop* that uses only the BNO055 — no encoders, no ToF, no optical flow.
Last updated: 2026-05-19

---

## 1. Problem statement

A pure pitch controller is **translationally underdetermined**: infinitely many wheel-velocity profiles hold `pitch ≈ 0`, and gain / mount / floor asymmetries pick whichever drifts. Cornell ECE4760 ([Chen, Niu & Zhu 2015][cornell]) names the failure mode: without speed feedback the bot "sometimes begins to accumulate speed … until the motor speed saturates, the wheels cannot keep up with the body of the robot, and it falls over." Matches our bench. The standard fix is a *cascade*: an outer position/velocity loop nudges the inner pitch setpoint, the bot leans the opposite way to brake ([Velazquez et al. 2016][cascade]). Open question: what feeds the outer loop with a BNO055 only and a 500 B flash cap.

---

## 2. Algorithm comparison

| # | Approach | Flash (B) | RAM (B) | Drift before 30 cm error | Complexity | Risk |
|---|----------|----------:|--------:|--------------------------|------------|------|
| A | Double-integrate `LINEARACCEL` (BNO055 reg 0x28-0x2D) | ~250 | ~24 | **3-10 s** (MEMS bias, see §3) | Low — one `getVector` call + two integrators | Drift hits 30 cm in single-digit seconds; pure A is useless beyond ~5 s. |
| B | Double-integrate `pitch_deg` (RCmags trick) | ~150 | ~16 | **30-60 s** with leak; effectively bounded | Lowest — `pitch` already exists, two scalar integrators | Bias = mount offset; couples to mounting estimator. |
| C | A + ZUPT when `\|out\| < CMD_QUIET` ∧ `\|pitch\| < ε` | ~350 | ~32 | **20-40 s** (resets between corrections) | Medium — needs reliable "still" detector | Detector hysteresis is the hard part; false positives lock vel to zero mid-cruise. |
| D | A + complementary high-pass washout of position (HP corner ≈ 1/τ_wash) | ~400 | ~36 | **Bounded by τ_wash** (typically 10-30 s) | Medium — one extra IIR per axis | Tunable but adds latency to genuine position changes. |
| E | Kalman / EKF on `[pos, vel, accel_bias]` per axis | ~1200+ | ~120+ | Tens of seconds | High — 3-state EKF × 2 axes | Doesn't fit Uno. Teensy/ESP32 only. |

(All flash figures are estimates from comparable patches; verify with `pio run -e arduino_uno_balancing` after implementing.)

**Why option B beats A for our use case.** For small angles `α_translational ≈ g · sin(pitch) ≈ g · pitch_rad`, so the *pitch signal itself* is a proxy for translational acceleration with a known scale and a bias = mount offset. Integrating *pitch* once already gives a velocity proxy in `(deg·s)` units; integrating twice gives a position proxy in `(deg·s²)`. The constants `g`/`L`/wheel-radius drop out — they only set the dimensionless gain that maps "position-proxy error" to "setpoint nudge in degrees," which is a single tuned (or RLS-derived) scalar. This is exactly the architecture in [RCmags' SB-1 robot][rcmags-blog] and is endorsed by [Land's Cornell course notes][cornell] as the IMU-only escape from the speed-accumulation failure mode.

**Why not option A (raw LINEARACCEL).** The BNO055's gravity-removed LIA output has the same MEMS bias problem any consumer accel does: after double integration position error grows as `½·b·t²`. Numerical Double Integration of Acceleration Measurements in Noise ([Thong et al. 2004][thong]) and the Strathclyde-Pratl pedestrian study ([Arraigada & Pratl 2006][arraigada]) both report position drift exceeding 30 cm in **3-10 s** for consumer-grade MEMS without aiding — well inside the wall-collision window. The BNO055's NDOF fusion does NOT subtract pitch-axis bias; that is a separate problem the chip cannot solve on its own. We measured a `LinAcc_x` bias of ~0.04 m/s² standing still on the bench (audit needed — order of magnitude), which after 5 s integrates to ~0.5 m position drift.

**Why options C/D are layered on top of A, not B.** They suppress accel bias by either resetting velocity (C) or high-pass-filtering position (D). Both are *upgrades* to an existing accel-based estimator. If we use B, we already have implicit bias rejection because the mount estimator slowly tracks pitch offset — the same `OnlineMountingEstimator` we already ship.

**Why E is on the table at all.** A 3-state EKF (`pos`, `vel`, `accel_bias`) per axis is the textbook solution — but at ~1200 B flash + ~120 B RAM it cannot fit our 1652 B Uno headroom alongside Phase 2.7 (which already wants ~200 B). On Teensy 4.0 / ESP32-S3 it is essentially free. The framework's design intent is to support that variant — but Uno gets option B.

---

## 3. Drift bound for naive double integration (option A)

Accelerometer bias `b` produces position error `e(t) = ½·b·t²`. For BNO055 LIA the residual bias after NDOF fusion is empirically 0.02-0.10 m/s² (Bosch datasheet §3.6.5 spec; consumer-MEMS reviews put consumer parts at the upper end). Solving for `e = 0.30 m`:

| b (m/s²) | t at 30 cm error |
|---------:|-----------------:|
| 0.02     | 5.5 s |
| 0.05     | 3.5 s |
| 0.10     | 2.5 s |

These are *quiet-bench* numbers. Add a noise term `σ·t^1.5` (random walk; [Woodman 2007][woodman]) and the practical bound is closer to 3-5 s for a bot expected to be balancing, not stationary. **Option A alone is not sufficient for tens-of-seconds containment.** It needs aiding — that's options C, D, E.

Option B, by contrast, washes its bias through the mount estimator's existing offset-tracking loop. Drift bound is set not by accel noise but by *how slowly the mount estimator absorbs new pitch-offset bias*, which we already operate at 20 s LPF (`main.cpp:302`). Position drift in option B accumulates only via mount-offset rate-of-change, which we deliberately rate-limit to 2°/s. That is one to two orders of magnitude better than option A in the regime we care about.

---

## 4. Recommendation

**Ship option B (double-integrate pitch) as Phase 4.11.** It is the smallest patch (~150 B flash, ~16 B RAM), fits Uno trivially, hijacks the existing pitch signal so no new sensor I/O is needed, and is structurally compatible with what `OnlineMountingEstimator` already does (slow bias tracking). The "RCmags trick" has the cleanest empirical track record for IMU-only balance bots that don't fall over from drift.

**On Teensy/ESP32 builds, layer option D on top** — a complementary washout filter that lets a slower drift-correcting loop survive longer-term excursions. Wall-clock containment improves from ~30 s (option B alone) to ~minutes (B+D). Not Uno-feasible.

**Option E (full EKF) is the framework's long-term answer** for sensor-rich platforms and the multi-IMU upgrade path. Designed-not-implemented, deferred.

---

## 5. Pseudocode sketch (option B), slotting into `step_run_`

```cpp
// New module: src/control/position_containment.{h,cpp}
class PositionContainment {
 public:
  // Push corrected pitch (already mount-corrected) each tick.
  // Returns a SMALL pitch-setpoint nudge in degrees (signed).
  // Always |nudge| <= MAX_NUDGE_DEG.
  float update(float pitch_deg, float dt_sec, bool freeze);

  void reset() { vel_ = 0.0f; pos_ = 0.0f; }

 private:
  float vel_ = 0.0f;             // integral of pitch_deg  (deg·s)
  float pos_ = 0.0f;             // integral of vel_       (deg·s²)
  // Tuning constants — all scalars; tune on bench.
  static constexpr float K_POS       = 0.005f;   // deg/(deg·s²)
  static constexpr float K_VEL       = 0.020f;   // deg/(deg·s)
  static constexpr float LEAK_VEL    = 0.995f;   // ~200-tick half-life (1 s @200 Hz)
  static constexpr float LEAK_POS    = 0.9995f;  // ~20-s half-life
  static constexpr float MAX_NUDGE_DEG = 1.5f;   // hard cap, well under tip envelope
  static constexpr float SLEW_DEG_S    = 2.0f;   // setpoint nudge rate-limit
  float last_nudge_deg_ = 0.0f;
};
```

```cpp
float PositionContainment::update(float pitch_deg, float dt_sec, bool freeze) {
    if (isnan(pitch_deg) || dt_sec <= 0.0f) return last_nudge_deg_;

    // Freeze integration during HELD / BOOTSTRAP / large-tilt to avoid
    // poisoning the estimator with operator-induced or near-fall motion.
    // Same gating pattern as PlantIdentifier (balance_app.cpp:546-547).
    if (!freeze) {
        vel_ = LEAK_VEL * vel_ + pitch_deg * dt_sec;        // washout integrator
        pos_ = LEAK_POS * pos_ + vel_       * dt_sec;
    }

    // Cascade output: pitch setpoint = +K_POS*pos + K_VEL*vel.
    // Sign convention: if bot drifted forward (+pos), we want to lean BACK
    // (positive pitch nudge) to brake. Verify on bench - mount axis dictates.
    float nudge = K_POS * pos_ + K_VEL * vel_;

    // Hard saturation - never request a tilt the bot can't recover from.
    if (nudge >  MAX_NUDGE_DEG) nudge =  MAX_NUDGE_DEG;
    if (nudge < -MAX_NUDGE_DEG) nudge = -MAX_NUDGE_DEG;

    // Slew limit - setpoint changes propagate as commands, hard steps cause
    // visible jerks. SLEW_DEG_S * dt is at most 0.01 deg per tick @200 Hz.
    const float max_step = SLEW_DEG_S * dt_sec;
    float delta = nudge - last_nudge_deg_;
    if (delta >  max_step) nudge = last_nudge_deg_ + max_step;
    if (delta < -max_step) nudge = last_nudge_deg_ - max_step;

    last_nudge_deg_ = nudge;
    return nudge;
}
```

Wire-in at `balance_app.cpp:step_run_()`:

```cpp
// Just before pid_.set_setpoint(0.0f) at line 485:
const bool pc_freeze = soft_cutoff
                    || (now_ms - run_entry_ms_) < BOOTSTRAP_FREEZE_MS;
const float setpoint_deg = pos_containment_.update(meas, cfg_.pid_sample_ms * 0.001f,
                                                   pc_freeze);
pid_.set_setpoint(setpoint_deg);   // was 0.0f
```

Reset on every IDLE→BOOTSTRAP and HELD→RUN transition (same place `pid_.reset()` is called in `enter_state_(RUN)`).

---

## 6. Phase plan

| Phase | What | Where it runs |
|-------|------|---------------|
| **4.11a** Option B, hardcoded gains | This document's pseudocode, ~150 B flash, ~16 B RAM | Uno + all targets |
| **4.11b** Sign + gain bench-tune | Operator hand-tunes K_POS, K_VEL via `t`-style command | Uno + all targets |
| **4.11c** Auto-derive K_POS, K_VEL from K_motor | Once `PlantIdentifier` has K, derive outer-loop gains by pole-placement (same trick we use for inner Kp/Kd) | Uno + all targets |
| **4.12a** Option D washout filter | One extra IIR per axis, adds ~100 B flash | Teensy / ESP32 |
| **4.12b** Lateral-axis (yaw-induced) containment | Use `body_heading_unit` from Phase 2.7 to disentangle forward/lateral drift | Teensy / ESP32 |
| **4.13** Option E full EKF | 3-state per axis, supports raw 6-DoF IMUs (MPU6050 path) too | Teensy / ESP32 |

Phase 4.11a is the unblocking bench work. It is one new translation-unit (~120 lines), one new module member of `BalanceApp`, and one new line in `step_run_`. No EEPROM changes. No new sensor I/O. No external dependencies.

**Hard constraints summarised:** `MAX_NUDGE_DEG ≤ 2°` (operator's "balance forever" preference; deeper tilts approach the linear-region edge ~5-8°), `SLEW_DEG_S ≤ 5°/s` (so a full saturation of the position loop unwinds at human-visible speeds, not hardware-jerky), freeze during HELD / BOOTSTRAP / soft_cutoff (same pattern as `PlantIdentifier`). These three rails are the difference between "bot stops wandering" and "bot tips itself with its own setpoint."

---

## 7. Why this fits the universal-balance-bot vision

The "more / less" reframe from [UNIVERSAL_BALANCE_BOT_VISION.md](../UNIVERSAL_BALANCE_BOT_VISION.md) applies directly. The outer loop is not asking for "set position to (0, 0)" — there is no such absolute coordinate IMU-only. It is asking *"if pitch has been positive on average for a few seconds, that means we drifted forward, so lean back a touch."* Pitch-bias-over-time is itself the position proxy; the controller never reasons about metres. K_POS and K_VEL are scalars on bot-specific dynamics that `PlantIdentifier` already knows, so Phase 4.11c can derive them with no user-facing config — matching the vision's "no `MASS_KG = 0.45f`" rule.

The mount estimator and the position-containment loop share the *exact same input signal* (filtered pitch) and partition the time-scales: mount estimator handles **20-s-and-slower** drift (battery sag, payload, mounting wear), position containment handles **0.1-30-s** drift (wandering, bumps, gentle slope). They cannot conflict because they live at different bandwidths.

---

## 8. References

[cascade]: https://journals.sagepub.com/doi/full/10.5772/63933
[rcmags-blog]: https://rcmags.github.io/projects/robots/2019/06/11/self_balancing_robot.html
[rcmags-gh]: https://github.com/RCmags/SelfBalancingRobot
[cornell]: https://people.ece.cornell.edu/land/courses/ece4760/FinalProjects/f2015/dc686_nn233_hz263/final_project_webpage_v2/dc686_nn233_hz263/index.html
[thong]: https://www.sciencedirect.com/science/article/abs/pii/S0263224104000417
[arraigada]: https://www.strc.ch/2006/Arraigada_Pratl_STRC_2006.pdf
[woodman]: https://www.cl.cam.ac.uk/techreports/UCAM-CL-TR-696.pdf
[zupt-review]: https://www.researchgate.net/publication/343337435_A_Review_on_ZUPT-Aided_Pedestrian_Inertial_Navigation
[ardupilot-balance]: https://ardupilot.org/rover/docs/balance_bot-tuning.html
[twip-sensorless]: https://ieeexplore.ieee.org/document/7244492/
[twip-uwb]: https://ncbi.nlm.nih.gov/pmc/articles/PMC5492841

- [Velazquez, M., Cruz, D., Garcia, S., & Bandala, M. (2016) — Velocity and Motion Control of a Self-Balancing Vehicle Based on a Cascade Control Strategy, *International Journal of Advanced Robotic Systems* 13(3)][cascade] — The reference cascade: outer velocity loop biases inner pitch setpoint. Uses encoders, but the architecture is the one we copy.
- [RCmags (2019) — SB-1 Self-Balancing Robot stabilized with a gyroscope][rcmags-blog] — Source of the "double-integrate pitch" trick for IMU-only position estimation. Repo: [RCmags/SelfBalancingRobot][rcmags-gh].
- [Chen, D., Niu, N., & Zhu, H. (2015) — Self-Balancing Robot, Cornell ECE4760 Final Project][cornell] — Explicit description of the speed-accumulation failure mode when no outer loop exists.
- [Thong, Y. K., Woodburn, M. S., Harris, N. D., & Walker, R. T. (2004) — Numerical double integration of acceleration measurements in noise, *Measurement* 36(1) 73-92][thong] — Establishes the `½·b·t²` and random-walk drift bounds.
- [Arraigada, M., & Pratl, M. (2006) — Calculation of displacements of measured accelerations, STRC][arraigada] — Practical observation that MEMS double-integration is useless beyond a few seconds without aiding.
- [Woodman, O. J. (2007) — An introduction to inertial navigation, Univ. of Cambridge UCAM-CL-TR-696][woodman] — The canonical IMU-drift tutorial; gives the `σ·t^1.5` velocity random walk used in §3.
- [Tian, X. et al. (2020) — A Review on ZUPT-Aided Pedestrian Inertial Navigation, *IEEE Sensors*][zupt-review] — ZUPT survey; foot-mounted analogue of our motor-quiet trigger.
- [ArduPilot project — Balance Bot tuning guide][ardupilot-balance] — Production cascade controller; `BAL_PITCH_TRIM`/`BAL_PITCH_MAX`/`ATC_BAL_*` is the same outer-bias / saturation / inner-gain trio we propose.
- [Sensorless estimation of position and velocity of a two-wheeled inverted pendulum mobile robot (IEEE ICCA 2015)][twip-sensorless] — Model-based observer alternative; included for completeness but heavier than what we propose.
- [Indoor Autonomous Control of a Two-Wheeled Inverted Pendulum Vehicle Using Ultra Wide Band Technology (2017)][twip-uwb] — Adds external sensor; cited as the path we explicitly reject (operator constraint: no new hardware).
- Bosch BNO055 datasheet §3.6.5 — Linear Acceleration register definition and accuracy spec.

---

## See also

- [UNIVERSAL_BALANCE_BOT_VISION.md](../UNIVERSAL_BALANCE_BOT_VISION.md) — "more/less" framing, why outer-loop gains will eventually be RLS-derived from K_motor.
- [research_drone_vs_balance_bot_stability.md](research_drone_vs_balance_bot_stability.md) — Drones have a natural translational "drift to where the wind blew you" mode; the balance bot's mode is identical and the fix is the same cascade.
- [research_motor_null_space_handling_detection.md](research_motor_null_space_handling_detection.md) — Phase 2.7 learns `body_heading_unit`, which Phase 4.12b uses to project drift along the bot's actual forward direction (not the IMU's nominal X).
- [online_adaptive_balance_tracking.md](online_adaptive_balance_tracking.md) — The mount estimator's time-scale separation argument carries over: mount = 20 s, position containment = 0.1-30 s; they live at different bandwidths and cannot fight.
- [theoretical_audit_balance_stack.md](theoretical_audit_balance_stack.md) §6.2 — Mount estimator and `PlantIdentifier` already partition time-scales; position containment slots in beneath both.
- [operator_ideas_backlog.md](operator_ideas_backlog.md) — Index entry for the next bench session's "stop wandering" work item.
- `src/applications/balancing_robot/balance_app.cpp:399-489` — `step_run_` — where the new `pos_containment_.update()` call lands, one line above `pid_.set_setpoint(0.0f)`.
