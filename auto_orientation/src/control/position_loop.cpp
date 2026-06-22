/**
 * PositionLoop — implementation. See position_loop.h for the cascade design,
 * the units contract, and the Phase 4M.13 hardcoded-gain sequencing flag.
 */

#include "position_loop.h"

#include <math.h>  // isnan, isinf — defensive NaN/Inf guard in update()

namespace {

inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

}  // namespace

PositionLoop::PositionLoop()
    : position_m_(0.0f),
      last_nudge_deg_(0.0f),
      // Phase 4M.14 — seed the operating gains from the conservative 4M.13
      // fallback values. BalanceApp overwrites them at BOOTSTRAP finalise with
      // the pole-placement derivation; if the derivation is rejected these
      // fallback values stay in force (phase_4m14_design §7).
      k_pos_(POSLOOP_K_POS_FALLBACK),
      k_vel_(POSLOOP_K_VEL_FALLBACK),
      pos_leak_(POSLOOP_POS_LEAK_FALLBACK) {}

void PositionLoop::reset() {
    // Only the integrator + slew memory are cleared. The derived gains
    // (k_pos_ / k_vel_ / pos_leak_) deliberately survive reset(): they are set
    // once at BOOTSTRAP finalise, while reset() runs on every RUN entry.
    position_m_     = 0.0f;
    last_nudge_deg_ = 0.0f;
    // M-4 — also reset the damping audit so every fresh RUN re-confirms
    // K_VEL from scratch (verdict starts at UNKNOWN, becomes OK/HIGH/LOW
    // after POSLOOP_AUDIT_MIN_EVENTS step events). Lives in the same
    // lifecycle bucket as the integrator: per-run, not per-bootstrap.
    audit_.reset();
}

void PositionLoop::set_gains(float k_pos, float k_vel) {
    k_pos_ = k_pos;
    k_vel_ = k_vel;
}

void PositionLoop::set_pos_leak(float pos_leak) {
    // Guard: a leak outside (0,1) would either freeze the integrator (>=1,
    // unbounded wind-up) or invert/zero it (<=0). Reject silently — the
    // current value (fallback or a prior valid set) is kept.
    if (pos_leak > 0.0f && pos_leak < 1.0f) {
        pos_leak_ = pos_leak;
    }
}

float PositionLoop::update(float wheel_vel, float dt) {
    // Guard: a zero / negative dt (clock not advanced, or caller mis-ordered)
    // would corrupt the integrator and the slew limit. Hold the last value.
    // Also reject non-finite (NaN/Inf) wheel_vel or dt (audit P3 / P1-003
    // parity): a NaN encoder reading would poison position_m_ permanently
    // (NaN propagates through *= and +=) and break the slew/clamp comparisons
    // (all NaN comparisons are false). Hold the cached nudge, no integrator
    // advance, no fault — same neutral pattern as PID::compute() on NaN input.
    if (!isfinite(wheel_vel) || !isfinite(dt) || dt <= 0.0f) {
        return last_nudge_deg_;
    }

    // --- Leaky position integrator -----------------------------------------
    // Accumulate drift = ∫v dt, then bleed it toward zero. The leak is what
    // keeps the integrator bounded so a small encoder bias cannot wind up a
    // permanent setpoint offset (see header — POS_LEAK rationale).
    position_m_ += wheel_vel * dt;
    position_m_ *= pos_leak_;

    // --- Cascade control law -----------------------------------------------
    // Lean back when drifted / moving forward, lean forward when drifted /
    // moving back. Negative sign: a positive forward velocity must produce a
    // negative (backward-lean) pitch setpoint so the inner loop drives the
    // wheels to arrest the drift.
    float nudge = -(k_pos_ * position_m_) - (k_vel_ * wheel_vel);

    // --- Magnitude clamp ---------------------------------------------------
    // Keep the nudge inside the inner loop's linear region.
    nudge = clampf(nudge, -POSLOOP_MAX_NUDGE_DEG, POSLOOP_MAX_NUDGE_DEG);

    // --- Slew limit --------------------------------------------------------
    // Bound how fast the setpoint can move so the outer loop's bandwidth
    // stays well below the inner pitch loop's — a setpoint that only crawls
    // cannot fight the PID.
    const float max_step = POSLOOP_SLEW_DEG_S * dt;
    const float delta    = nudge - last_nudge_deg_;
    if (delta > max_step) {
        nudge = last_nudge_deg_ + max_step;
    } else if (delta < -max_step) {
        nudge = last_nudge_deg_ - max_step;
    }

    last_nudge_deg_ = nudge;

    // M-4 — feed the (post-leak) position to the runtime damping audit so it
    // observes the same trajectory the outer loop is shaping. The now_ms
    // argument is currently unused inside the classifier (window counted in
    // ticks); pass 0 to keep update()'s signature unchanged for all callers.
    // Done AFTER the integrator update so we record what the bot will actually
    // see this tick.
    audit_.push_sample(position_m_, 0);

    return nudge;
}
