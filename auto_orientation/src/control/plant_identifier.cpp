/**
 * Plant Identifier — implementation. See plant_identifier.h for the algorithm.
 *
 * Implementation notes:
 *
 *  - All math is single-precision float. AVR does double==float anyway; on
 *    Teensy/ESP32 the scalar arithmetic is cheap enough that keeping float
 *    here matches the rest of the framework.
 *
 *  - No <math.h> sin() in the regression: we use a small-angle Taylor
 *    (x − x³/6) good to < 0.4% for |pitch| < 30°. Saves ~250 B of flash on
 *    AVR (avr-libc's sinf is large) and we're inside the linear-region tilt
 *    envelope by construction (the controller saturates output above ±10–25°
 *    per balance_app.cpp's soft-cutoff). For larger angles the regression is
 *    already meaningless (saturated PWM, nonlinear motor regime).
 *
 *  - σ-modification (Ioannou & Sun §8) is implemented as a "soft" projection:
 *    a small leak term that pulls θ back toward a class-typical prior when
 *    |θ| exits the configured (k_min, k_max) band. Avoids the discontinuity
 *    of a hard clamp, which can excite limit cycles when the estimate hovers
 *    at the boundary.
 *
 *  - The recompute_targets_() call lives inside update() so the status struct
 *    is always coherent with the current θ. Cost is ~2 divisions per tick —
 *    fine at 200 Hz on a 16 MHz AVR.
 */

#include "plant_identifier.h"

namespace {

// Defaults documented in the header.
constexpr float DEFAULT_LAMBDA   = 0.998f;   // ~25 s memory at 200 Hz
constexpr float DEFAULT_G_EFF    = 50.0f;    // deg/s² for ~20 cm bench bot
constexpr float DEFAULT_TS       = 0.5f;     // settling-time target (s)
constexpr float DEFAULT_ZETA     = 0.7f;     // damping ratio
constexpr float DEFAULT_K_MIN    = 0.02f;    // K_motor lower bound (bench class)
constexpr float DEFAULT_K_MAX    = 5.0f;     // K_motor upper bound (bench class)

// Initial RLS covariance — large means "first sample carries strong weight"
// (per Ljung 1999, §11). 1.0 is the textbook prior when no information is
// available. We start higher so the very first valid sample shifts θ
// meaningfully toward the data without being dominated by the seed prior.
constexpr float INITIAL_P        = 1.0f;

// Covariance after seed_k_motor() pushes a directly-measured K. Much smaller
// than INITIAL_P — we trust the bootstrap measurement and only let the RLS
// nudge θ over time (battery sag, surface change). Sized so a single noisy
// sample can't undo a clean pulse measurement.
constexpr float SEED_P           = 0.05f;

// σ-modification leak coefficient. Multiplied by (θ − θ_prior) when outside
// the band — small enough to be invisible during normal operation, large
// enough to drag the estimate back from a bad sample over a few seconds.
constexpr float SIGMA_LEAK       = 0.05f;
constexpr float K_PRIOR          = 0.2f;     // class-typical K_motor (mid-band)

// Ki:Kp ratio for the emitted target. Per design doc: "Ki = 0.05 * Kp" — a
// small integral that keeps the OnlineMountingEstimator signal alive without
// dominating the loop. Trading-off: tighter coupling with the I-term-driven
// mounting estimator vs. faster offset response on its own LPF time constant.
constexpr float KI_FRACTION      = 0.05f;

// deg → rad without pulling M_PI from <math.h>. We use the linear sin(x) ≈ x
// approximation: in the controller's linear region (|pitch| < 10°) the cubic
// Taylor term is < 0.5% — well under the noise floor of the regression.
// Outside the linear region the regressor saturates anyway (soft-cutoff zeros
// motors above 25°), so the regression doesn't run there.
constexpr float DEG_TO_RAD = 0.01745329252f;

// Minimum magnitude of φ that contributes to a learning step. Below this,
// excitation is too small to identify K_motor reliably — the RLS would chase
// noise instead. ~10 PWM total (= 5 PWM per wheel) is the typical floor where
// brushed motors break free of stiction; below that, α is dominated by
// gravity not the input.
constexpr float MIN_PHI          = 10.0f;

} // anonymous namespace

PlantIdentifier::PlantIdentifier()
    : theta_(K_PRIOR),
      P_(INITIAL_P),
      lambda_(DEFAULT_LAMBDA),
      g_eff_(DEFAULT_G_EFF),
      ts_target_(DEFAULT_TS),
      zeta_(DEFAULT_ZETA),
      k_min_(DEFAULT_K_MIN),
      k_max_(DEFAULT_K_MAX) {
    status_.k_motor    = theta_;
    status_.covariance = P_;
    status_.kp_target  = 0.0f;
    status_.kd_target  = 0.0f;
    status_.ki_target  = 0.0f;
    status_.samples    = 0;
    status_.frozen     = false;
    recompute_targets_();
}

void PlantIdentifier::set_forgetting_factor(float lambda) {
    // Clamp to a safe band — λ outside (0.9, 1.0] is either too forgetful
    // (chases noise) or unstable (>1 grows covariance without bound).
    if (lambda < 0.9f) lambda = 0.9f;
    if (lambda > 1.0f) lambda = 1.0f;
    lambda_ = lambda;
}

void PlantIdentifier::set_g_eff(float g_eff_deg_s2) {
    g_eff_ = g_eff_deg_s2;
}

void PlantIdentifier::set_target_settling_time_sec(float ts) {
    // Settling-time floor: BNO055 NDOF group delay is ~25 ms, so ts < 0.1 s
    // is asking the controller to act on stale data. Latency_budget_2026-05-12.
    if (ts < 0.1f) ts = 0.1f;
    ts_target_ = ts;
}

void PlantIdentifier::set_damping_ratio(float zeta) {
    if (zeta < 0.3f) zeta = 0.3f;     // anything less = significant overshoot
    if (zeta > 1.5f) zeta = 1.5f;     // anything more = sluggish
    zeta_ = zeta;
}

void PlantIdentifier::set_k_motor_bounds(float k_min, float k_max) {
    if (k_min > 0.0f && k_max > k_min) {
        k_min_ = k_min;
        k_max_ = k_max;
    }
}

void PlantIdentifier::reset(float kp_initial, float kd_initial) {
    // Back-solve a prior θ from the seed gains so the first emitted target is
    // close to current behaviour: Kp = ω_n²/K  ⇒  K = ω_n²/Kp. We trust Kp
    // because it dominates the controller authority. If kp_initial is zero
    // or unreasonable, fall back to the class-prior mid-band.
    const float wn = 4.0f / ts_target_;
    if (kp_initial > 0.01f) {
        theta_ = (wn * wn) / kp_initial;
        if (theta_ < k_min_) theta_ = k_min_;
        if (theta_ > k_max_) theta_ = k_max_;
    } else {
        theta_ = K_PRIOR;
    }
    (void)kd_initial;   // reserved — currently only Kp shapes the prior

    P_                = INITIAL_P;
    status_.samples   = 0;
    status_.frozen    = false;
    recompute_targets_();
}

void PlantIdentifier::seed_k_motor(float k_measured) {
    // Clamp into the configured bench-class band. A bootstrap measurement
    // outside the band almost certainly means the rig is wrong (motor lead
    // reversed, IMU axis mismatch, no motion at all), not a real plant —
    // but we still want a defined θ for the controller to use rather than
    // a divide-by-near-zero blow-up.
    float k = k_measured;
    if (k < k_min_) k = k_min_;
    if (k > k_max_) k = k_max_;
    theta_            = k;
    P_                = SEED_P;
    status_.samples   = 0;
    status_.frozen    = false;
    recompute_targets_();
}

void PlantIdentifier::update(float pwm_total,
                             float gyro_rate_dps,
                             float gyro_rate_prev_dps,
                             float pitch_deg,
                             float dt_sec,
                             bool  freeze) {
    // Always tick the sample counter so the bootstrap-window logic in
    // BalanceApp can use it to decide when to release the freeze gate.
    if (status_.samples < 65535u) status_.samples++;
    status_.frozen = freeze;

    // Degenerate dt — protect the differentiation. Caller should never do this
    // but we're inside an ISR and refuse to divide by ~0.
    if (dt_sec < 1e-4f) {
        return;
    }

    if (freeze) {
        // Hold state. Targets stay valid; covariance and θ untouched.
        return;
    }

    // ---- 1. Numerical pitch acceleration ----
    // α = d(gyro_rate)/dt. The caller stashes prev/current across ticks so
    // this single subtraction is the only differentiation we need.
    const float alpha = (gyro_rate_dps - gyro_rate_prev_dps) / dt_sec;

    // ---- 2. Strip the gravitational restoring term ----
    // The regression target is "what motor action did" after gravity is
    // accounted for. Linearised: sin(pitch_rad) ≈ pitch_rad (<0.5% error
    // for |pitch| < 10°, our linear-region envelope).
    const float y = alpha - g_eff_ * pitch_deg * DEG_TO_RAD;

    // ---- 3. Regressor + excitation gate ----
    const float phi     = pwm_total;
    const float phi_abs = phi < 0.0f ? -phi : phi;
    if (phi_abs < MIN_PHI) {
        // Below the excitation floor — the data is dominated by gravity /
        // noise, not the input. Skip the RLS step; refresh targets in case
        // a parameter setter changed them, and exit. Per Åström & Wittenmark
        // (1995) §11, persistent excitation is the precondition for RLS
        // convergence; below MIN_PHI it is not satisfied.
        recompute_targets_();
        return;
    }

    // ---- 4. Scalar RLS update ----
    // Closed-form Kalman gain for the scalar case:
    //   L = P·φ / (λ + φ·P·φ)
    //   θ ← θ + L·(y − φ·θ)
    //   P ← (P − L·φ·P) / λ
    // Algebraically identical to the matrix RLS specialised to dim=1.
    const float phi_P    = phi * P_;
    const float denom    = lambda_ + phi * phi_P;
    // denom ≥ λ > 0 by construction (P_ ≥ 0, λ > 0), so the divide is safe.
    const float L_gain   = phi_P / denom;
    const float residual = y - phi * theta_;
    theta_              += L_gain * residual;
    P_                   = (P_ - L_gain * phi_P) / lambda_;

    // ---- 5. σ-modification projection ----
    // When θ exits the class bounds, pull it back toward K_PRIOR with a small
    // leak. This is the "soft projection" — see implementation notes at top
    // of file. Hard-clamp would be an alternative; the soft form is smoother.
    if (theta_ < k_min_ || theta_ > k_max_) {
        theta_ -= SIGMA_LEAK * (theta_ - K_PRIOR);
        // If the leak hasn't pulled us back into the band yet, hard-clamp to
        // the nearest edge so the gain map never divides by a wild value.
        if (theta_ < k_min_) theta_ = k_min_;
        if (theta_ > k_max_) theta_ = k_max_;
    }

    // ---- 6. Recompute the analytical target gains ----
    recompute_targets_();
}

// ---------------------------------------------------------------------------
// Inspection
// ---------------------------------------------------------------------------

float PlantIdentifier::get_k_motor()    const { return theta_;            }
float PlantIdentifier::get_covariance() const { return P_;                }
float PlantIdentifier::get_kp_target()  const { return status_.kp_target; }
float PlantIdentifier::get_kd_target()  const { return status_.kd_target; }
float PlantIdentifier::get_ki_target()  const { return status_.ki_target; }

const PlantIdentifierStatus& PlantIdentifier::get_status() const {
    return status_;
}

// ---------------------------------------------------------------------------
// Internals
// ---------------------------------------------------------------------------

void PlantIdentifier::recompute_targets_() {
    // Pole-placement formulas for a critically-damped second-order plant:
    //   Kp = ω_n² / K_motor
    //   Kd = 2 · ζ · ω_n / K_motor
    // ω_n = 4 / ts_target. Ki = KI_FRACTION · Kp.
    // Guard against theta_ near zero (divide protection — should never happen
    // because the projection clamp keeps theta_ ≥ k_min_ > 0).
    float th = theta_;
    if (th < 1e-3f) th = 1e-3f;

    const float wn      = 4.0f / ts_target_;
    const float inv_th  = 1.0f / th;
    const float kp      = (wn * wn) * inv_th;
    const float kd      = (2.0f * zeta_ * wn) * inv_th;
    const float ki      = KI_FRACTION * kp;

    status_.k_motor    = theta_;
    status_.covariance = P_;
    status_.kp_target  = kp;
    status_.kd_target  = kd;
    status_.ki_target  = ki;
}
