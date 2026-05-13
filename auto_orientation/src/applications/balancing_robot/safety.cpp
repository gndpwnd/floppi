/**
 * BalanceSafety implementation. See safety.h for design notes.
 */

#include "safety.h"

#include "../../control/tuning_strategy.h"  // SafetyLimits (for the tuner)

// Defaults — overridable from BalanceApp::begin() via the config struct.
static const float    kDefaultTiltLimitDeg     = 35.0f;
static const float    kDefaultTiltRecoveryDeg  = 15.0f;
static const uint32_t kDefaultWatchdogMs       = 200u;

BalanceSafety::BalanceSafety()
    : tilt_limit_deg_(kDefaultTiltLimitDeg),
      tilt_recovery_deg_(kDefaultTiltRecoveryDeg),
      watchdog_timeout_ms_(kDefaultWatchdogMs),
      last_feed_ms_(0),
      watchdog_armed_(false),
      abort_(false) {
}

void BalanceSafety::set_tilt_limit(float deg) {
    if (deg < 0.0f) {
        deg = -deg;  // store as positive magnitude
    }
    tilt_limit_deg_ = deg;
}

void BalanceSafety::set_tilt_recovery(float deg) {
    if (deg < 0.0f) {
        deg = -deg;
    }
    tilt_recovery_deg_ = deg;
}

bool BalanceSafety::is_tipover(float pitch_deg) const {
    float m = pitch_deg < 0.0f ? -pitch_deg : pitch_deg;
    return m > tilt_limit_deg_;
}

bool BalanceSafety::is_recovered(float pitch_deg) const {
    float m = pitch_deg < 0.0f ? -pitch_deg : pitch_deg;
    return m < tilt_recovery_deg_;
}

void BalanceSafety::feed_watchdog(uint32_t now_ms) {
    last_feed_ms_ = now_ms;
    watchdog_armed_ = true;
}

bool BalanceSafety::watchdog_starved(uint32_t now_ms) const {
    if (!watchdog_armed_) {
        // No tick yet — treat as not starved so the very first sample isn't
        // flagged. Caller should feed_watchdog() at the start of every step.
        return false;
    }
    return (now_ms - last_feed_ms_) > watchdog_timeout_ms_;
}

void BalanceSafety::set_watchdog_timeout(uint32_t ms) {
    watchdog_timeout_ms_ = ms;
}

void BalanceSafety::request_abort() {
    abort_ = true;
}

bool BalanceSafety::abort_requested() const {
    return abort_;
}

void BalanceSafety::clear_abort() {
    abort_ = false;
}

void BalanceSafety::populate_tuner_safety(SafetyLimits& out) const {
    // Don't let the tuner wiggle past tipover. We leave a small margin
    // (90% of the hard tilt limit) so the tuner stops before the application
    // would trip FALLEN on the same sample.
    out.max_angle_deg    = tilt_limit_deg_ * 0.9f;
    // Default cap. BalanceApp overrides this from BalanceAppConfig.
    out.max_duration_sec = 30.0f;
    out.abort_requested  = abort_;
}
