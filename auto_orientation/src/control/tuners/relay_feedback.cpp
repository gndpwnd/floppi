/**
 * RelayFeedbackStrategy — implementation.
 *
 * Algorithm (Åström-Hägglund 1984, with hysteretic relay):
 *
 *   1. Replace the controller with a relay:
 *        output = +amplitude when error >  +hysteresis
 *               = -amplitude when error <  -hysteresis
 *      (error = setpoint - measurement, same convention as PIDController).
 *
 *   2. The plant settles into a stable limit-cycle oscillation at its
 *      ultimate frequency ωu. We measure two quantities per *full* period:
 *
 *        - Tu (sec)   : time between two consecutive HIGH->LOW relay
 *                       transitions.
 *        - a  (units) : (max - min) / 2 of the measurement over that
 *                       period. This is the amplitude of the fundamental
 *                       harmonic of the output.
 *
 *      We avoid the half-cycle peak-tracking trap by instead tracking the
 *      running max/min across the entire period window. This is robust to
 *      the phase relationship between relay sign and plant response: it
 *      doesn't matter whether the plant peak occurs during the HIGH or
 *      LOW half of the relay cycle — we just see the global extrema over
 *      one full period.
 *
 *   3. Describing-function ultimate gain:
 *
 *        Ku = (4 * amplitude) / (π * a)
 *
 *      where `amplitude` is the relay output magnitude.
 *
 *   4. PID gains via AMIGO / Ziegler-Nichols-relay (same coefficients in
 *      this library — both formulas are reported with these values in the
 *      cited references; the toggle exists for future divergence):
 *
 *        Kp = 0.6   * Ku
 *        Ti = 0.5   * Tu     -> Ki = Kp / Ti = 1.2  * Ku / Tu
 *        Td = 0.125 * Tu     -> Kd = Kp * Td = 0.075 * Ku * Tu
 *
 *   5. Phase margin: ~45-60° for AMIGO. We publish a static 60° estimate
 *      because the algorithm doesn't measure the closed-loop margin
 *      directly. See auto_pid_tuning_research.md §4.
 *
 * Termination:
 *   - Success when `cycles_to_average` full periods have been recorded.
 *     We average period_ring_ and amplitude_ring_ over those samples.
 *   - Failure if safety.abort_requested, max_duration, or max_angle trip,
 *     or if the plant never reaches a swing >= 4×hysteresis (recorded as
 *     "no_oscillation").
 *
 * Notes for AVR:
 *   - No STL, no dynamic allocation; ring buffers are fixed-size float
 *     arrays.
 *   - `float` math only; we don't rely on double precision.
 */

#include "relay_feedback.h"

#include <math.h>

// ---- Constants ----------------------------------------------------------

static const float kPi = 3.14159265358979323846f;

// If the average half-peak-to-peak `a` is below this many hysteresis-widths,
// the plant didn't oscillate properly. 2× is the theoretical minimum; we set
// 2× as a hard floor (peak-to-peak >= 2×hysteresis means each half-cycle
// actually crossed the band).
static const float kMinAmplitudeMultiplier = 2.0f;

// Published phase-margin estimate for AMIGO-tuned PIDs. The relay method
// targets the +180° crossover, so post-Kp/Ki/Kd we typically observe
// 45-60° PM on representative plants. Static placeholder — see header.
static const float kAmigoPhaseMarginDeg = 60.0f;

// ---- Construction & configuration --------------------------------------

RelayFeedbackStrategy::RelayFeedbackStrategy(float amplitude, float hysteresis)
    : amplitude_(amplitude < 0.0f ? -amplitude : amplitude),
      hysteresis_(hysteresis < 0.0f ? -hysteresis : hysteresis),
      cycles_to_average_(4),
      use_classic_zn_(false),
      setpoint_(0.0f),
      output_min_(0.0f),
      output_max_(0.0f),
      start_ms_(0) {
    reset_state_();
}

void RelayFeedbackStrategy::set_cycles_to_average(uint8_t n) {
    if (n < 1) n = 1;
    if (n > kMaxCycles) n = kMaxCycles;
    cycles_to_average_ = n;
}

void RelayFeedbackStrategy::set_use_classic_zn(bool yes) {
    use_classic_zn_ = yes;
}

// ---- ITuningStrategy ---------------------------------------------------

void RelayFeedbackStrategy::begin(float setpoint,
                                  float output_min,
                                  float output_max,
                                  uint32_t now_ms) {
    setpoint_   = setpoint;
    output_min_ = output_min;
    output_max_ = output_max;
    start_ms_   = now_ms;

    reset_state_();

    // Initial result placeholder until we either succeed or fail.
    result_.kp                        = 0.0f;
    result_.ki                        = 0.0f;
    result_.kd                        = 0.0f;
    result_.ultimate_gain             = 0.0f;
    result_.ultimate_period_sec       = 0.0f;
    result_.phase_margin_estimate_deg = 0.0f;
    result_.converged                 = false;
    result_.failure_reason            = "in_progress";
}

float RelayFeedbackStrategy::step(float measurement,
                                  const SafetyLimits& safety,
                                  uint32_t now_ms) {
    if (done_) {
        return 0.0f;
    }

    // PID convention for error: error = setpoint - measurement. This matches
    // PIDController, so callers reasoning about sign get the same result
    // regardless of which side they're driving (auto-tune vs runtime PID).
    const float error = setpoint_ - measurement;

    // ---- Safety tripwires (strategy-side mirror of the coordinator) -----
    if (safety.abort_requested) {
        finalize_failure_("user_aborted");
        return 0.0f;
    }
    const float elapsed_sec = (float)(now_ms - start_ms_) * 0.001f;
    if (elapsed_sec > safety.max_duration_sec) {
        finalize_failure_("max_duration_exceeded");
        return 0.0f;
    }
    if (fabsf(error) > safety.max_angle_deg) {
        finalize_failure_("max_angle_exceeded");
        return 0.0f;
    }

    // ---- Update peak tracking (always, regardless of relay state) -------
    if (window_open_) {
        if (measurement > cycle_max_) cycle_max_ = measurement;
        if (measurement < cycle_min_) cycle_min_ = measurement;
    }

    // ---- Detect relay transitions --------------------------------------
    // Negative-feedback relay convention (matches the PID it replaces):
    //   error > +hysteresis  => measurement BELOW setpoint => relay HIGH (+amp)
    //   error < -hysteresis  => measurement ABOVE setpoint => relay LOW  (-amp)
    //
    // i.e., the relay pushes the plant toward setpoint, overshoots, flips,
    // pushes the other way, overshoots, ... — the canonical limit cycle.
    //
    // Transitions of interest:
    //   relay HIGH -> LOW : error fell past -hysteresis (measurement
    //                      overshot above setpoint). End of a half-cycle.
    //   relay LOW  -> HIGH: error rose past +hysteresis.
    //
    // We record one (period, amplitude) pair per HIGH->LOW transition,
    // because that's a unique landmark we see once per limit-cycle period.

    if (relay_state_high_ && error < -hysteresis_) {
        // HIGH -> LOW transition.
        if (have_high_to_low_) {
            // We have a previous HIGH->LOW timestamp, so we've just
            // completed one full period.
            const uint32_t dt_ms     = now_ms - last_high_to_low_ms_;
            const float period_sec  = (float)dt_ms * 0.001f;
            const float peak_to_peak = cycle_max_ - cycle_min_;
            const float amp_half     = peak_to_peak * 0.5f;

            // Append to the ring (with overwrite-oldest behaviour if the
            // ring is full — we always end up averaging the last N).
            if (cycles_recorded_ < kMaxCycles) {
                period_ring_[cycles_recorded_]    = period_sec;
                amplitude_ring_[cycles_recorded_] = amp_half;
                cycles_recorded_++;
            } else {
                // Shift left and drop the oldest.
                for (uint8_t i = 1; i < kMaxCycles; ++i) {
                    period_ring_[i - 1]    = period_ring_[i];
                    amplitude_ring_[i - 1] = amplitude_ring_[i];
                }
                period_ring_[kMaxCycles - 1]    = period_sec;
                amplitude_ring_[kMaxCycles - 1] = amp_half;
            }
        }

        // Open / restart the measurement window. Seed the extrema from
        // the current measurement so the comparisons converge from the
        // first sample of the new window.
        cycle_max_ = measurement;
        cycle_min_ = measurement;
        window_open_ = true;

        last_high_to_low_ms_ = now_ms;
        have_high_to_low_    = true;
        relay_state_high_    = false;
    } else if (!relay_state_high_ && error > hysteresis_) {
        // LOW -> HIGH transition. No period boundary here, but we still
        // want to extend the running extrema (already done above in the
        // unconditional update). Just flip the relay state.
        relay_state_high_ = true;
    }

    // ---- Convergence check ----------------------------------------------
    if (cycles_recorded_ >= cycles_to_average_) {
        finalize_success_();
        return 0.0f;
    }

    // ---- Compute output -------------------------------------------------
    float out = relay_state_high_ ? amplitude_ : -amplitude_;
    if (out > output_max_) out = output_max_;
    if (out < output_min_) out = output_min_;
    return out;
}

// ---- Helpers ------------------------------------------------------------

void RelayFeedbackStrategy::reset_state_() {
    done_                = false;
    // Initial relay state: HIGH. The first step() will produce +amp; if
    // that turns out to be the wrong initial direction the plant will
    // quickly cross hysteresis and flip us anyway. This is the standard
    // approach in the literature.
    relay_state_high_    = true;
    last_high_to_low_ms_ = 0;
    have_high_to_low_    = false;
    cycle_max_           = 0.0f;
    cycle_min_           = 0.0f;
    window_open_         = false;
    cycles_recorded_     = 0;
    for (uint8_t i = 0; i < kMaxCycles; ++i) {
        period_ring_[i]    = 0.0f;
        amplitude_ring_[i] = 0.0f;
    }
}

void RelayFeedbackStrategy::finalize_failure_(const char* reason) {
    result_.kp                        = 0.0f;
    result_.ki                        = 0.0f;
    result_.kd                        = 0.0f;
    result_.ultimate_gain             = 0.0f;
    result_.ultimate_period_sec       = 0.0f;
    result_.phase_margin_estimate_deg = 0.0f;
    result_.converged                 = false;
    result_.failure_reason            = reason;
    done_                             = true;
}

void RelayFeedbackStrategy::finalize_success_() {
    // Average over the last `cycles_to_average_` entries. If the ring
    // happens to hold more, we average over what we've got — biased toward
    // the recent samples is fine.
    const uint8_t n = (cycles_recorded_ < cycles_to_average_)
                          ? cycles_recorded_
                          : cycles_to_average_;
    const float Tu = ring_mean_(period_ring_,    n);
    const float a  = ring_mean_(amplitude_ring_, n);

    // Sanity: did we actually oscillate? `a` must clear at least
    // kMinAmplitudeMultiplier × hysteresis (i.e., peak-to-peak >= 2×hyst).
    if (a < kMinAmplitudeMultiplier * hysteresis_ || Tu <= 0.0f) {
        finalize_failure_("no_oscillation");
        return;
    }

    // Ultimate gain from describing-function analysis.
    const float Ku = (4.0f * amplitude_) / (kPi * a);

    // Gain formulas (AMIGO/ZN-relay — same coefficients).
    float Kp, Ki, Kd;
    if (use_classic_zn_) {
        Kp = 0.6f   * Ku;
        Ki = 1.2f   * Ku / Tu;
        Kd = 0.075f * Ku * Tu;
    } else {
        Kp = 0.6f   * Ku;
        Ki = 1.2f   * Ku / Tu;
        Kd = 0.075f * Ku * Tu;
    }

    result_.kp                        = Kp;
    result_.ki                        = Ki;
    result_.kd                        = Kd;
    result_.ultimate_gain             = Ku;
    result_.ultimate_period_sec       = Tu;
    result_.phase_margin_estimate_deg = kAmigoPhaseMarginDeg;
    result_.converged                 = true;
    result_.failure_reason            = 0;   // nullptr
    done_                             = true;
}

float RelayFeedbackStrategy::ring_mean_(const float* ring, uint8_t count) const {
    if (count == 0) return 0.0f;
    float sum = 0.0f;
    for (uint8_t i = 0; i < count; ++i) {
        sum += ring[i];
    }
    return sum / (float)count;
}
