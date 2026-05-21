/**
 * PositionLoop — velocity / position outer loop for the Mega balance bot.
 *
 * Phase 4M.13 (Workstream F, second half — architecture_plan_2026-05-20.md
 * §2.6 + §4; design from research_wheel_encoders_mega_2026-05-19.md §6.1).
 *
 * The balance stack is a cascade:
 *
 *     encoder wheel velocity ─► PositionLoop ─► pitch-setpoint nudge (deg)
 *                                              ─► inner balance PID ─► PWM
 *
 * The INNER loop (the existing pitch PID in balance_app) keeps the bot
 * upright. The OUTER loop (this class) watches the wheel velocity and nudges
 * the pitch setpoint by a small, clamped, slew-limited amount so the bot
 * leans slightly *against* its own drift — driving wheel velocity and the
 * integrated position back toward zero. The net effect: the bot holds
 * station instead of slowly creeping across the floor.
 *
 * Why a leaky integrator. A pure position integrator would wind up without
 * bound if the encoders have any systematic bias, and the bot would chase a
 * phantom target forever. POS_LEAK applies a slow exponential washout each
 * update so the integrator *contains* drift over a ~20 s horizon (same
 * time-scale as the OnlineMountingEstimator) without ever permanently
 * latching a setpoint offset. This is the option-B fallback's structural
 * idea (research_imu_only_position_containment.md §5) re-expressed on real
 * encoder velocity instead of a pitch double-integral.
 *
 * The output is BOTH magnitude-clamped (±MAX_NUDGE_DEG) and slew-limited
 * (±SLEW_DEG_S · dt per update). The slew limit is what keeps the cascade
 * bandwidth well below the inner loop's: a setpoint that can only move at a
 * couple of degrees per second cannot fight the inner pitch loop.
 *
 * Units: update() takes wheel velocity in m/s (linear tread velocity, the
 * WheelEncoder::read_velocity_mps() output) and dt in seconds. It returns a
 * pitch-setpoint nudge in DEGREES, ready to hand to PIDController::
 * set_setpoint().
 *
 * No <Arduino.h>, no dynamic allocation, no STL — same constraints as the
 * rest of the control/ layer. Compiles everywhere; balance_app only *calls*
 * it under USE_WHEEL_ENCODERS.
 */

#ifndef POSITION_LOOP_H
#define POSITION_LOOP_H

// ---------------------------------------------------------------------------
// Phase 4M.14 — the outer-loop gains are now DERIVED, not hardcoded.
//
// K_POS / K_VEL / POS_LEAK are computed at BOOTSTRAP finalise by
// BalanceApp::derive_position_gains() (closed-form pole-placement on the
// linearised cascade plant — see phase_4m14_design_2026-05-20.md §3 / §5.2)
// and pushed into the live loop via set_gains() / set_pos_leak(). They are
// no longer constants: PositionLoop holds them as member variables seeded
// from the *_FALLBACK values below.
//
// The *_FALLBACK constants are the conservative Phase 4M.13 hand-picked values
// (research_wheel_encoders_mega_2026-05-19.md §6.1 — a slow, gentle station-
// keeper). They are retained as the documented safe fallback: BalanceApp
// reverts to them when the derivation cannot be trusted (no valid wheel
// radius, 4M.2 K cross-check did not run, or the derived gains tripped the
// sanity clamp). On that path BOOTSTRAP still succeeds and the bot still
// balances — see phase_4m14_design §7. Because the operating gains are
// derived, the 4M.13 "Do NOT bench-tune" sequencing flag is retired.
//
// MAX_NUDGE_DEG and SLEW_DEG_S stay hardcoded: they are safety / rate
// saturations (an inner-loop linearity bound and a large-signal bandwidth
// guard), not loop gains — scope.md §"The rule" exempts both. See §5.1 / §5.3.
// ---------------------------------------------------------------------------

// Fallback position gain: degrees of pitch nudge per metre of accumulated
// drift. The Phase 4M.13 hand-picked conservative value; used when the
// 4M.14 derivation is rejected. Small — a 0.1 m drift asks for ~0.6° of lean.
constexpr float POSLOOP_K_POS_FALLBACK = 6.0f;

// Fallback velocity gain: degrees of pitch nudge per m/s of wheel velocity.
// The velocity term provides damping so the position term does not overshoot.
constexpr float POSLOOP_K_VEL_FALLBACK = 3.0f;

// Fallback per-update leak applied to the position integrator. 0.999 at a
// 5 ms tick is a ~5 s washout time-constant; over realistic run lengths this
// keeps the integrator bounded so encoder bias cannot wind up a permanent
// setpoint offset. The 4M.14 derivation replaces this with exp(-dt/tau) from
// a named washout tau (POSLOOP_WASHOUT_TAU_S in balance_app.h).
constexpr float POSLOOP_POS_LEAK_FALLBACK = 0.999f;

// Back-compat aliases — the original 4M.13 names kept so existing callers and
// tests still compile. These resolve to the fallback values; the *operating*
// gains live in PositionLoop's members and are set by the 4M.14 derivation.
constexpr float POSLOOP_K_POS   = POSLOOP_K_POS_FALLBACK;
constexpr float POSLOOP_K_VEL   = POSLOOP_K_VEL_FALLBACK;
constexpr float POSLOOP_POS_LEAK = POSLOOP_POS_LEAK_FALLBACK;

// Hard clamp on the nudge magnitude — a SAFETY SATURATION, not a loop gain.
// The inner loop's linear region is only a few degrees wide; a 2° ceiling
// keeps the outer loop firmly inside it so the bot never leans far enough to
// lose balance authority. Stays hardcoded by design (phase_4m14_design §5.1).
constexpr float POSLOOP_MAX_NUDGE_DEG = 2.0f;

// Maximum rate of change of the nudge, degrees per second — a RATE SATURATION,
// not a loop gain. Belt-and-suspenders enforcement of the cascade bandwidth
// separation that pole-placement sets analytically: it guarantees the outer
// loop cannot race the inner pitch PID even for large transients where the
// linear plant model breaks down. Stays hardcoded by design (§5.3).
constexpr float POSLOOP_SLEW_DEG_S = 2.0f;

/**
 * Velocity / position outer loop. One instance per balance bot (both wheels'
 * velocities are averaged by the caller before being handed in).
 *
 * Lifecycle: construct once, call reset() when the inner loop (re)enters RUN,
 * then call update() once per PID tick with the freshly-measured mean wheel
 * velocity. The returned nudge is fed to the inner PID's setpoint.
 */
class PositionLoop {
public:
    PositionLoop();

    /**
     * Clear the position integrator, the slew-limiter memory and the cached
     * nudge. Call on entering RUN so a fresh balance session starts holding
     * station from its current spot — no carry-over drift from a prior run.
     */
    void reset();

    /**
     * Run one outer-loop step.
     *
     * @param wheel_vel  Mean wheel tread velocity in m/s (positive = forward).
     *                   Caller averages left + right encoders.
     * @param dt         Elapsed time since the previous update, in seconds.
     *                   Non-positive dt returns the cached nudge unchanged.
     * @return           Pitch-setpoint nudge in degrees, magnitude-clamped to
     *                   ±POSLOOP_MAX_NUDGE_DEG and slew-limited to
     *                   ±POSLOOP_SLEW_DEG_S per second. Hand straight to
     *                   PIDController::set_setpoint().
     */
    float update(float wheel_vel, float dt);

    /**
     * Phase 4M.14 — install the analytically-derived outer-loop gains.
     *
     * Called once by BalanceApp at BOOTSTRAP finalise (after the 4M.2 K
     * cross-check passes) with the pole-placement result, or with the
     * *_FALLBACK constants on the rejection path. Overwrites the position /
     * velocity gains used by the control law. Independent of reset(): the
     * gains persist across reset() (which only clears the integrator and the
     * slew memory), so the call site may set them at FINALISE while reset()
     * still runs every RUN entry.
     *
     * @param k_pos  Position gain — degrees of nudge per metre of drift.
     * @param k_vel  Velocity gain — degrees of nudge per m/s of wheel velocity.
     */
    void set_gains(float k_pos, float k_vel);

    /**
     * Phase 4M.14 — install the derived per-tick integrator leak.
     *
     * `pos_leak` is POS_LEAK = exp(-dt/tau) for the chosen washout tau (see
     * phase_4m14_design §5.2). Must be in (0,1); out-of-range values are
     * ignored so a bad derivation cannot disable or invert the integrator.
     */
    void set_pos_leak(float pos_leak);

    // ----- Inspection (telemetry / tests) ----------------------------------
    float position_m() const { return position_m_; }
    float last_nudge_deg() const { return last_nudge_deg_; }
    float k_pos() const { return k_pos_; }
    float k_vel() const { return k_vel_; }
    float pos_leak() const { return pos_leak_; }

private:
    // Leaky integral of wheel velocity — an estimate of accumulated drift in
    // metres. update() bleeds it by pos_leak_ every tick; that exponential
    // washout is the *only* thing bounding it — there is NO hard clamp on
    // position_m_. The bound is therefore soft and bias-dependent: against a
    // zero-mean velocity signal the leak holds it near 0, but a *systematic*
    // encoder bias b drives a non-zero steady state of roughly
    // b*dt*pos_leak_/(1-pos_leak_) (the leak balances the bias inflow, it
    // does not reject it). Known TD-7 consideration: over a long (>10 min)
    // RUN a small persistent bias can still settle the integrator at a
    // standing offset despite POS_LEAK — informational only, to be quantified
    // by the bench disturbance benchmark (Workstream K), not a fault here.
    float position_m_;

    // Slew-limiter memory: the nudge actually emitted last update().
    float last_nudge_deg_;

    // Phase 4M.14 — the operating outer-loop gains. Seeded from the *_FALLBACK
    // constants in the constructor; overwritten by set_gains() / set_pos_leak()
    // with the BOOTSTRAP-derived pole-placement values. NOT cleared by reset().
    float k_pos_;
    float k_vel_;
    float pos_leak_;
};

#endif  // POSITION_LOOP_H
