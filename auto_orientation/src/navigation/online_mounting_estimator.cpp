/**
 * Online Adaptive Mounting-Offset Estimator — AVR LPF implementation
 *
 * See online_mounting_estimator.h for design notes and references.
 */

#include "online_mounting_estimator.h"

namespace {

// Pitch-rate above which we consider the chassis to be in transient motion
// (a kick, a bump, a tipover swing). Adaptation is frozen above this.
// Matches the "rate gate" in online_adaptive_balance_tracking.md §5 (the
// research suggests 1°/s for explicit upright-stillness gating; we use a
// looser 60°/s here because the LPF time constant already filters out
// shorter excursions, and the windup_active flag covers the tighter case).
constexpr float HIGH_GYRO_THRESHOLD_DPS = 60.0f;

// Below this elapsed time we skip the update entirely — protects the
// rate-limit divide-by-zero edge case when update() is called back-to-back
// within a single millis() tick.
constexpr uint32_t MIN_DT_MS = 1u;

// Defaults documented in the header.
constexpr float DEFAULT_REFERENCE_DEG       = 0.0f;
constexpr float DEFAULT_MAX_DEVIATION_DEG   = 5.0f;
constexpr float DEFAULT_MAX_DRIFT_RATE_DPS  = 0.5f;
constexpr float DEFAULT_GAIN_TO_ANGLE       = 0.01f;
constexpr float DEFAULT_LPF_TC_SEC          = 300.0f;  // 5 min

} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction / lifecycle
// ---------------------------------------------------------------------------

OnlineMountingEstimator::OnlineMountingEstimator()
    : reference_deg_(DEFAULT_REFERENCE_DEG),
      max_deviation_deg_(DEFAULT_MAX_DEVIATION_DEG),
      max_drift_rate_dps_(DEFAULT_MAX_DRIFT_RATE_DPS),
      gain_to_angle_(DEFAULT_GAIN_TO_ANGLE),
      lpf_time_constant_sec_(DEFAULT_LPF_TC_SEC),
      estimate_deg_(DEFAULT_REFERENCE_DEG),
      drift_rate_dps_(0.0f),
      user_frozen_(false),
      last_freeze_reason_(FREEZE_NONE),
      last_update_ms_(0u),
      last_save_ms_(0u),
      initialized_(false) {
}

void OnlineMountingEstimator::initialize(float reference_deg, uint32_t now_ms) {
    reference_deg_      = reference_deg;
    estimate_deg_       = reference_deg;
    drift_rate_dps_     = 0.0f;
    last_freeze_reason_ = FREEZE_NONE;
    last_update_ms_     = now_ms;
    last_save_ms_       = now_ms;
    initialized_        = true;
}

void OnlineMountingEstimator::reset_to_reference(float new_ref_deg, uint32_t now_ms) {
    // Same as initialize() — kept as a separate method for call-site clarity
    // ("recapture happened" reads better than "re-initialize").
    initialize(new_ref_deg, now_ms);
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void OnlineMountingEstimator::set_lpf_time_constant_sec(float tc) {
    // Guard against degenerate values (tc <= 0 would divide by zero / make
    // alpha infinite). Smallest sensible tc is a few sample periods; pick
    // 0.1 s as a generous floor.
    if (tc < 0.1f) {
        tc = 0.1f;
    }
    lpf_time_constant_sec_ = tc;
}

void OnlineMountingEstimator::set_max_deviation_deg(float deg) {
    if (deg < 0.0f) {
        deg = 0.0f;
    }
    max_deviation_deg_ = deg;
}

void OnlineMountingEstimator::set_max_drift_rate_dps(float dps) {
    if (dps < 0.0f) {
        dps = 0.0f;
    }
    max_drift_rate_dps_ = dps;
}

void OnlineMountingEstimator::set_gain_to_angle(float gain) {
    gain_to_angle_ = gain;
}

void OnlineMountingEstimator::set_user_freeze(bool freeze) {
    user_frozen_ = freeze;
}

void OnlineMountingEstimator::mark_saved(uint32_t now_ms) {
    last_save_ms_ = now_ms;
}

// ---------------------------------------------------------------------------
// Live update
// ---------------------------------------------------------------------------

float OnlineMountingEstimator::update(float i_term, float pitch_deg,
                                      bool tipover_active, bool windup_active,
                                      float gyro_pitch_dps, uint32_t now_ms) {
    // pitch_deg currently unused by the LPF math — reserved for future
    // cross-checks (e.g., reject if pitch is wildly outside the estimate).
    // Cast to void to silence unused-parameter warnings without restructuring
    // the public API.
    (void)pitch_deg;

    if (!initialized_) {
        // Caller forgot to call initialize() — be defensive: latch on the
        // first sample so we don't compute a garbage dt against a stale
        // last_update_ms_ of zero. Don't move the estimate yet.
        initialize(reference_deg_, now_ms);
        return estimate_deg_;
    }

    // Compute dt. Skip too-fast calls to avoid divide-by-zero in the
    // rate-limit branch.
    uint32_t dt_ms;
    if (now_ms >= last_update_ms_) {
        dt_ms = now_ms - last_update_ms_;
    } else {
        // millis() wrapped around (50-day rollover on AVR). Pretend nothing
        // happened this step — the next call will resync the timestamp.
        last_update_ms_ = now_ms;
        return estimate_deg_;
    }
    if (dt_ms < MIN_DT_MS) {
        return estimate_deg_;
    }
    const float dt_s = static_cast<float>(dt_ms) * 0.001f;

    // --- Determine freeze state ------------------------------------------
    uint8_t freeze_reason = FREEZE_NONE;
    if (user_frozen_) {
        freeze_reason = FREEZE_USER;
    } else if (tipover_active) {
        freeze_reason = FREEZE_TIPOVER;
    } else if (windup_active) {
        freeze_reason = FREEZE_WINDUP;
    } else if (gyro_pitch_dps > HIGH_GYRO_THRESHOLD_DPS ||
               gyro_pitch_dps < -HIGH_GYRO_THRESHOLD_DPS) {
        freeze_reason = FREEZE_HIGH_GYRO;
    }
    last_freeze_reason_ = freeze_reason;

    if (freeze_reason != FREEZE_NONE) {
        // Hold the current estimate; report zero drift so the dashboard
        // shows a flat trace instead of stale velocity from before the
        // freeze.
        drift_rate_dps_ = 0.0f;
        last_update_ms_ = now_ms;
        return estimate_deg_;
    }

    // --- Slow LPF toward (reference + i_term * gain_to_angle) ------------
    //
    // Discrete form of dx/dt = (target - x) / tc:
    //   x += (1/tc) * dt_s * (target - x)
    //
    // With tc = 300 s and dt_s = 0.005 s (200 Hz):
    //   alpha_per_step = 0.005 / 300 ≈ 1.67e-5
    // i.e., each sample nudges the estimate by ~0.00002 of the residual,
    // which is exactly the "minutes to drift one degree" behaviour we want.
    const float target_deg = reference_deg_ + i_term * gain_to_angle_;
    const float alpha      = 1.0f / lpf_time_constant_sec_;
    float candidate        = estimate_deg_ + alpha * dt_s * (target_deg - estimate_deg_);

    // --- Hard bound: clamp to [ref - max_dev, ref + max_dev] -------------
    candidate = clamp_(candidate,
                       reference_deg_ - max_deviation_deg_,
                       reference_deg_ + max_deviation_deg_);

    // --- Rate limit: |Δ/dt| ≤ max_drift_rate_dps -------------------------
    // After the rate limit the change-per-second is bounded; we then
    // recompute drift_rate_dps_ from the *applied* change so the reported
    // value is honest about what actually happened.
    const float max_step = max_drift_rate_dps_ * dt_s;
    float delta          = candidate - estimate_deg_;
    if (delta >  max_step) delta =  max_step;
    if (delta < -max_step) delta = -max_step;
    const float new_estimate = estimate_deg_ + delta;

    drift_rate_dps_ = delta / dt_s;
    estimate_deg_   = new_estimate;
    last_update_ms_ = now_ms;
    return estimate_deg_;
}

// ---------------------------------------------------------------------------
// Inspection
// ---------------------------------------------------------------------------

float OnlineMountingEstimator::get_estimate_deg() const {
    return estimate_deg_;
}

float OnlineMountingEstimator::get_drift_rate_dps() const {
    return drift_rate_dps_;
}

float OnlineMountingEstimator::get_reference_deg() const {
    return reference_deg_;
}

MountingCalibrationStatus OnlineMountingEstimator::get_status() const {
    MountingCalibrationStatus s;
    s.estimate_deg   = estimate_deg_;
    s.drift_rate_dps = drift_rate_dps_;

    // Confidence heuristic: 1.0 at the reference, linearly decreasing to
    // 0.0 at the hard bound. Mirrors the "state" table in the research
    // finding (fresh/tracking/creeping/unstable/out_of_bounds) but exposes
    // it as a continuous number so the dashboard can render a gradient.
    float deviation = estimate_deg_ - reference_deg_;
    if (deviation < 0.0f) deviation = -deviation;
    float conf = 0.0f;
    if (max_deviation_deg_ > 0.0f) {
        conf = 1.0f - (deviation / max_deviation_deg_);
    }
    s.confidence_0_to_1 = clamp_(conf, 0.0f, 1.0f);

    s.last_save_ms      = last_save_ms_;
    s.adaptation_frozen = user_frozen_ || (last_freeze_reason_ != FREEZE_NONE);
    s.freeze_reason     = last_freeze_reason_;
    return s;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

float OnlineMountingEstimator::clamp_(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
