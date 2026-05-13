/**
 * BalanceSafety — tilt limit + watchdog + abort flag for the balancing-robot
 * application (Phase 4.7a).
 *
 * Three jobs in one tiny module:
 *   1. Compare the live pitch angle against tipover / recovery thresholds so
 *      the top-level state machine knows when to enter / leave FALLEN.
 *   2. Maintain a millis()-based watchdog that the main loop feeds on every
 *      healthy tick. If the loop stalls (sensor hang, blocking I2C, etc.) the
 *      watchdog goes hungry and the caller can disarm the motors.
 *   3. Carry a single-shot abort flag that user input (long-press, WiFi
 *      command, etc.) sets to request a return to IDLE. The state machine
 *      polls this flag and clears it after honoring it.
 *
 * Design choices:
 *   - Pure data + comparisons, no Arduino dependencies. Compiles cleanly on
 *     the native test host with no stubs.
 *   - SafetyLimits-for-the-tuner is populated via a separate method (rather
 *     than inheriting from / embedding SafetyLimits) so the tuner header is
 *     only included by the .cpp. Keeps compile-time coupling minimal.
 *   - No dynamic allocation; all state is plain scalars.
 *
 * See: docs/findings/MASTER_DESIGN.md §4.7
 *      docs/findings/disturbance_compensation_research.md §3 (tipover policy)
 */

#ifndef BALANCE_SAFETY_H
#define BALANCE_SAFETY_H

#include <stdint.h>

// Forward declaration — populate_tuner_safety() fills one of these. We avoid
// including tuning_strategy.h here so applications that don't auto-tune don't
// pay for the header.
struct SafetyLimits;

class BalanceSafety {
public:
    BalanceSafety();

    // Tilt thresholds (degrees, absolute value comparison).
    //   tilt_limit    : entering FALLEN beyond this magnitude
    //   tilt_recovery : leaving FALLEN when below this magnitude
    // Recovery should be strictly less than limit (no hysteresis is enforced
    // here, but the state machine relies on it for stable transitions).
    void set_tilt_limit(float deg);
    void set_tilt_recovery(float deg);

    float tilt_limit_deg() const { return tilt_limit_deg_; }
    float tilt_recovery_deg() const { return tilt_recovery_deg_; }

    bool is_tipover(float pitch_deg) const;
    bool is_recovered(float pitch_deg) const;

    // Watchdog: caller bumps on every healthy main-loop tick. If not bumped
    // for `watchdog_timeout_ms_` milliseconds, watchdog_starved() returns
    // true and the caller should disarm motors / drop to a safe state.
    void feed_watchdog(uint32_t now_ms);
    bool watchdog_starved(uint32_t now_ms) const;
    void set_watchdog_timeout(uint32_t ms);   // default 200
    uint32_t watchdog_timeout_ms() const { return watchdog_timeout_ms_; }
    uint32_t last_feed_ms() const { return last_feed_ms_; }

    // Abort flag (user-triggered: button long-press, WiFi command, etc.)
    // The flag is sticky until clear_abort() — callers check it from the
    // state machine each step, then clear it after acting.
    void request_abort();
    bool abort_requested() const;
    void clear_abort();

    // Convenience: build a SafetyLimits suitable for the auto-tuner. The
    // tuner's max_angle_deg is set to the tilt limit (we definitely don't
    // want to wiggle past tipover during relay feedback), max_duration_sec
    // is left up to the caller (BalanceApp passes the configured value
    // separately by overwriting that field after calling this helper).
    void populate_tuner_safety(SafetyLimits& out) const;

private:
    float    tilt_limit_deg_;
    float    tilt_recovery_deg_;
    uint32_t watchdog_timeout_ms_;
    uint32_t last_feed_ms_;
    bool     watchdog_armed_;
    bool     abort_;
};

#endif  // BALANCE_SAFETY_H
