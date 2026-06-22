/**
 * Generic Single-Axis PID Controller — implementation
 *
 * See pid_controller.h for the API contract and design rationale.
 *
 * Anti-windup strategy: integral clamping (back-calculation style).
 * Each step the integral term is bounded so that |ki * integral| <= output
 * span. This is simpler than conditional integration, costs ~2 floats of
 * state, and behaves correctly when ki is changed at runtime (the auto-tuner
 * does exactly that). If ki is zero we still keep the accumulator valid in
 * case it's re-enabled later.
 *
 * Floating-point notes for AVR: <math.h> isnan() is available via gcc and
 * works on float. We avoid <cmath> to keep the implementation portable to
 * the AVR toolchain (which has only a partial C++ standard library).
 */

#include "pid_controller.h"

#include <math.h>

// Treat any dt below this as zero (prevents divide-by-zero in the D term).
static const float kMinDtSeconds = 1e-6f;

PIDController::PIDController(float kp, float ki, float kd,
                             float output_min, float output_max)
    : kp_(kp), ki_(ki), kd_(kd),
      output_min_(output_min), output_max_(output_max),
      setpoint_(0.0f),
      integral_(0.0f),
      last_measurement_(0.0f),
      last_error_(0.0f),
      last_output_(0.0f),
      measurement_lpf_(0.0f),
      measurement_lpf_init_(false),
      d_term_lpf_tau_sec_(0.005f),    // 5 ms — less phase lag for delayed bots
      i_term_limit_pwm_(0.0f),        // 0 = no extra limit (default)
      d_on_measurement_(true),
      first_compute_(true),
      sample_ms_(5),
      last_p_term_(0.0f),
      last_i_term_(0.0f),
      last_d_term_(0.0f),
      last_error_value_(0.0f) {
    // Defensive: if caller passed inverted limits, swap them so the clamp
    // still behaves. This costs one branch in the constructor.
    if (output_min_ > output_max_) {
        float tmp = output_min_;
        output_min_ = output_max_;
        output_max_ = tmp;
    }
}

void PIDController::set_sample_time(uint16_t sample_ms) {
    if (sample_ms == 0) {
        sample_ms = 1;
    }
    sample_ms_ = sample_ms;
}

uint16_t PIDController::get_sample_time() const {
    return sample_ms_;
}

void PIDController::set_output_limits(float min, float max) {
    if (min > max) {
        // Reject inverted ranges silently — keep old limits.
        return;
    }
    output_min_ = min;
    output_max_ = max;
    // Re-bound any existing integral and last_output against the new range.
    clamp_integral_();
    last_output_ = clamp_(last_output_, output_min_, output_max_);
}

void PIDController::set_tunings(float kp, float ki, float kd) {
    // Reject negative gains — they invert the loop sign and are almost
    // certainly a bug from the caller. Saturate at 0 instead of asserting.
    // Also reject non-finite (NaN/Inf) gains (audit P1-003): (NaN < 0.0f) is
    // false, so without the isnan() guard a NaN gain would be stored and later
    // produce a NaN output that defeats the output clamp (NaN comparisons are
    // all false) and reaches the (int16_t) motor cast as undefined behaviour.
    kp_ = (isnan(kp) || isinf(kp) || kp < 0.0f) ? 0.0f : kp;
    ki_ = (isnan(ki) || isinf(ki) || ki < 0.0f) ? 0.0f : ki;
    kd_ = (isnan(kd) || isinf(kd) || kd < 0.0f) ? 0.0f : kd;
    // ki changed -> re-bound the integral against the new effective limit.
    clamp_integral_();
}

void PIDController::get_tunings(float& kp, float& ki, float& kd) const {
    kp = kp_;
    ki = ki_;
    kd = kd_;
}

void PIDController::set_setpoint(float setpoint) {
    // Reject non-finite (NaN/Inf) setpoints (audit P3 / P1-003 parity): a NaN
    // setpoint poisons every subsequent error = setpoint_ - measurement, which
    // propagates through P/I/D and would historically defeat the output clamp
    // (NaN comparisons all false). clamp_() now catches the output, but the
    // integrator still accumulates garbage every tick until reset(). Drop the
    // bad value at the boundary instead — keep the previous setpoint.
    if (isnan(setpoint) || isinf(setpoint)) {
        return;
    }
    setpoint_ = setpoint;
}

float PIDController::get_setpoint() const {
    return setpoint_;
}

void PIDController::set_d_on_measurement(bool d_on_measurement) {
    d_on_measurement_ = d_on_measurement;
}

bool PIDController::is_d_on_measurement() const {
    return d_on_measurement_;
}

void PIDController::set_d_term_lpf_tau_sec(float tau_sec) {
    d_term_lpf_tau_sec_ = (tau_sec < 0.0f) ? 0.0f : tau_sec;
    measurement_lpf_init_ = false;  // re-arm on next compute()
}

float PIDController::get_d_term_lpf_tau_sec() const {
    return d_term_lpf_tau_sec_;
}

void PIDController::set_i_term_limit(float pwm) {
    i_term_limit_pwm_ = (pwm < 0.0f) ? 0.0f : pwm;
    clamp_integral_();
}

float PIDController::get_i_term_limit() const {
    return i_term_limit_pwm_;
}

float PIDController::compute(float measurement, uint16_t dt_ms) {
    // Sanity: drop NaN inputs to last-known-good. Caller (or upstream sensor)
    // should be filtering NaN already, but PID is a safety-critical loop so
    // we double-check rather than propagate.
    if (isnan(measurement)) {
        return last_output_;
    }

    // dt == 0 means "no time has passed" — return previous output unchanged.
    // This keeps callers that poll faster than the scheduler safe.
    if (dt_ms == 0) {
        return last_output_;
    }

    const float dt_seconds = (float)dt_ms * 0.001f;

    // ---- Error ----
    const float error = setpoint_ - measurement;
    last_error_value_ = error;

    // ---- P term ----
    const float p_term = kp_ * error;
    last_p_term_ = p_term;

    // ---- I term ----
    integral_ += error * dt_seconds;
    clamp_integral_();
    const float i_term = ki_ * integral_;
    last_i_term_ = i_term;

    // ---- Measurement LPF for D-term ----
    // One-pole IIR with configurable τ (default 15 ms via constructor):
    // alpha = dt / (τ + dt). Independent of the raw measurement that feeds P
    // (so step response is not delayed). When τ == 0 the LPF is bypassed —
    // measurement_lpf_ tracks measurement exactly and PID_v1 parity holds.
    if (d_term_lpf_tau_sec_ <= 0.0f) {
        measurement_lpf_      = measurement;
        measurement_lpf_init_ = true;
    } else {
        const float alpha = dt_seconds / (d_term_lpf_tau_sec_ + dt_seconds);
        if (!measurement_lpf_init_) {
            measurement_lpf_      = measurement;
            measurement_lpf_init_ = true;
        } else {
            measurement_lpf_ += alpha * (measurement - measurement_lpf_);
        }
    }

    // ---- D term ----
    float d_term = 0.0f;
    if (first_compute_) {
        // No prior reference — skip D for this step and arm the history.
        first_compute_ = false;
    } else if (dt_seconds > kMinDtSeconds) {
        if (d_on_measurement_) {
            // Derivative of (filtered) measurement; negated because
            // error = sp - meas. last_measurement_ holds the previous
            // LPF'd value so the derivative is over the smoothed signal.
            const float d_meas = measurement_lpf_ - last_measurement_;
            d_term = -kd_ * (d_meas / dt_seconds);
        } else {
            // Derivative of error; reacts to setpoint changes.
            const float d_err = error - last_error_;
            d_term = kd_ * (d_err / dt_seconds);
        }
    }
    last_d_term_ = d_term;

    // ---- Sum & clamp ----
    float output = p_term + i_term + d_term;
    output = clamp_(output, output_min_, output_max_);

    // ---- Persist state for next call ----
    last_measurement_ = measurement_lpf_;
    last_error_ = error;
    last_output_ = output;

    return output;
}

float PIDController::compute_with_rate(float measurement, float rate_dps,
                                       uint16_t dt_ms) {
    // Same NaN / dt guards as compute().
    if (isnan(measurement) || isnan(rate_dps)) {
        return last_output_;
    }
    if (dt_ms == 0) {
        return last_output_;
    }

    const float dt_seconds = (float)dt_ms * 0.001f;

    // ---- Error / P term ----
    const float error = setpoint_ - measurement;
    last_error_value_ = error;
    const float p_term = kp_ * error;
    last_p_term_ = p_term;

    // ---- I term (identical to compute()) ----
    integral_ += error * dt_seconds;
    clamp_integral_();
    const float i_term = ki_ * integral_;
    last_i_term_ = i_term;

    // ---- D term from explicit rate ----
    // No numerical differentiation, no measurement LPF, no first-compute
    // skip — rate_dps is already the instantaneous derivative of the
    // measurement and is presumed clean (gyro is a physical measurement).
    // Sign matches D-on-measurement in compute(): a positive rate (meas
    // moving up) subtracts from the output to damp the motion.
    const float d_term = -kd_ * rate_dps;
    last_d_term_ = d_term;

    // ---- Sum & clamp ----
    float output = p_term + i_term + d_term;
    output = clamp_(output, output_min_, output_max_);

    // Persist enough state that a subsequent compute() call still works
    // correctly (we keep last_measurement_ / last_error_ in sync so a
    // caller can switch back to compute() without a derivative kick).
    last_measurement_  = measurement;
    last_error_        = error;
    last_output_       = output;
    // Re-arm the numerical-derivative first-compute guard so compute()'s
    // D path skips its first sample after the switchover.
    first_compute_     = true;

    return output;
}

void PIDController::reset() {
    integral_ = 0.0f;
    last_measurement_ = 0.0f;
    last_error_ = 0.0f;
    last_output_ = 0.0f;
    first_compute_ = true;
    measurement_lpf_ = 0.0f;
    measurement_lpf_init_ = false;
    last_p_term_ = 0.0f;
    last_i_term_ = 0.0f;
    last_d_term_ = 0.0f;
    last_error_value_ = 0.0f;
}

float PIDController::get_p_term() const     { return last_p_term_;     }
float PIDController::get_i_term() const     { return last_i_term_;     }
float PIDController::get_d_term() const     { return last_d_term_;     }
float PIDController::get_output() const     { return last_output_;     }
float PIDController::get_error() const      { return last_error_value_;}

// ------------------------------------------------------------------------- //
// Helpers                                                                   //
// ------------------------------------------------------------------------- //

float PIDController::clamp_(float v, float lo, float hi) {
    // Fail safe on non-finite input (audit P1-003 / P2-001): a NaN or Inf
    // value fails BOTH (v < lo) and (v > hi) (all NaN comparisons are false;
    // +Inf > hi catches Inf but NaN slips through the magnitude branches), so
    // without this guard a NaN sum/gain would pass straight through and reach
    // the (int16_t) motor cast — undefined behaviour, unpredictable PWM. Treat
    // any non-finite value as 0 output (motors neutral). This single guard
    // closes the NaN→motor class for EVERY output path that funnels through
    // clamp_(), not just the gain path.
    if (isnan(v) || isinf(v)) return 0.0f;
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void PIDController::clamp_integral_() {
    // Bound the integral so that ki * integral fits within the output span.
    // When ki == 0 we leave the integral untouched (it contributes nothing
    // either way, and we don't want to lose accumulated history if ki is
    // re-enabled by an auto-tuner mid-run).
    if (ki_ <= 0.0f) {
        return;
    }
    // Two bounds — use the tighter one:
    //   (a) output-range bound: |Ki * integral| ≤ output_max
    //   (b) explicit I-term limit: |Ki * integral| ≤ i_term_limit_pwm_
    // The explicit limit (when set) is the right tool for unstable plants
    // where the integrator can dominate the output and cause windup-driven
    // saturation before the system recovers.
    float effective_max = output_max_;
    float effective_min = output_min_;
    if (i_term_limit_pwm_ > 0.0f) {
        if (i_term_limit_pwm_ < effective_max) effective_max = i_term_limit_pwm_;
        if (-i_term_limit_pwm_ > effective_min) effective_min = -i_term_limit_pwm_;
    }
    const float i_max = effective_max / ki_;
    const float i_min = effective_min / ki_;
    integral_ = clamp_(integral_, i_min, i_max);
}
