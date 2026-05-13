/**
 * AutoPIDTuner — implementation
 *
 * See auto_pid_tuner.h for the API contract and design rationale.
 *
 * Safety policy: the coordinator pre-checks tripwires *before* calling the
 * strategy each step. If a tripwire fires, the strategy is never told to do
 * anything dangerous — we just stamp a failure result and stop. Strategies
 * also see the SafetyLimits and may end themselves; whichever path triggers
 * first wins.
 *
 * The coordinator deliberately does NOT modify the PIDController during a
 * tuning run. The user (or the application's state machine) is expected to
 * disconnect the PID from the actuator while tuning and let the tuner output
 * drive the plant directly. After the run, apply_to() or restore_original()
 * decides whether the new gains go in.
 */

#include "auto_pid_tuner.h"

#include <math.h>

// Default safety: practically disabled angle limit, generous duration cap,
// no pending abort. Callers should override via set_safety_limits().
static const float kDefaultMaxAngleDeg     = 1.0e6f;
static const float kDefaultMaxDurationSec  = 30.0f;

AutoPIDTuner::AutoPIDTuner(ITuningStrategy& strategy)
    : strategy_(strategy),
      abort_requested_(false),
      done_(false),
      original_kp_(0.0f),
      original_ki_(0.0f),
      original_kd_(0.0f),
      start_ms_(0) {
    safety_.max_angle_deg    = kDefaultMaxAngleDeg;
    safety_.max_duration_sec = kDefaultMaxDurationSec;
    safety_.abort_requested  = false;

    result_.kp                       = 0.0f;
    result_.ki                       = 0.0f;
    result_.kd                       = 0.0f;
    result_.ultimate_gain            = 0.0f;
    result_.ultimate_period_sec      = 0.0f;
    result_.phase_margin_estimate_deg = 0.0f;
    result_.converged                = false;
    result_.failure_reason           = "not_started";
}

void AutoPIDTuner::set_safety_limits(const SafetyLimits& limits) {
    safety_ = limits;
    // Track the externally-supplied abort flag so callers can flip it from an
    // ISR without us losing it on the next set_safety_limits() call.
    if (limits.abort_requested) {
        abort_requested_ = true;
    }
}

void AutoPIDTuner::begin(PIDController& pid,
                         float setpoint,
                         float output_min,
                         float output_max,
                         uint32_t now_ms) {
    // Cache current gains so restore_original() can roll back on failure.
    pid.get_tunings(original_kp_, original_ki_, original_kd_);

    abort_requested_ = false;
    done_            = false;
    start_ms_        = now_ms;

    // Forward to the strategy.
    strategy_.begin(setpoint, output_min, output_max, now_ms);

    // Reset result to a not-yet-converged placeholder.
    result_ = make_failure_result_("in_progress");
}

float AutoPIDTuner::step(float measurement, uint32_t now_ms) {
    // If we already finished, hold a neutral output. Strategy is allowed to
    // be called again but we shortcut here so callers don't accidentally
    // drive the plant after the tuner thinks it's done.
    if (done_) {
        return 0.0f;
    }

    // Build the safety view we'll hand to the strategy. We OR in our own
    // abort flag (set via request_abort()) so external callers don't need to
    // mutate the SafetyLimits struct themselves.
    SafetyLimits view = safety_;
    if (abort_requested_) {
        view.abort_requested = true;
    }

    // ---- Pre-check coordinator-level tripwires ----
    // These are redundant with the strategy's own checks, but we run them
    // here so a misbehaving strategy can't ignore them.
    const float elapsed_sec = (float)(now_ms - start_ms_) * 0.001f;

    if (view.abort_requested) {
        result_ = make_failure_result_("user_aborted");
        done_   = true;
        return 0.0f;
    }

    if (elapsed_sec > view.max_duration_sec) {
        result_ = make_failure_result_("max_duration_exceeded");
        done_   = true;
        return 0.0f;
    }

    // ---- Run the strategy ----
    const float output = strategy_.step(measurement, view, now_ms);

    // ---- Post-check: did the strategy finish? ----
    if (strategy_.is_done()) {
        result_ = strategy_.get_result();
        done_   = true;
    }

    return output;
}

bool AutoPIDTuner::is_done() const {
    return done_;
}

bool AutoPIDTuner::succeeded() const {
    return done_ && result_.converged;
}

const TuningResult& AutoPIDTuner::result() const {
    return result_;
}

void AutoPIDTuner::apply_to(PIDController& pid) const {
    pid.set_tunings(result_.kp, result_.ki, result_.kd);
}

void AutoPIDTuner::restore_original(PIDController& pid) const {
    pid.set_tunings(original_kp_, original_ki_, original_kd_);
}

void AutoPIDTuner::request_abort() {
    abort_requested_ = true;
}

TuningResult AutoPIDTuner::make_failure_result_(const char* reason) {
    TuningResult r;
    r.kp                        = 0.0f;
    r.ki                        = 0.0f;
    r.kd                        = 0.0f;
    r.ultimate_gain             = 0.0f;
    r.ultimate_period_sec       = 0.0f;
    r.phase_margin_estimate_deg = 0.0f;
    r.converged                 = false;
    r.failure_reason            = reason;
    return r;
}
