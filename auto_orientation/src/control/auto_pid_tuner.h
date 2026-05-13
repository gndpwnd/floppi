/**
 * AutoPIDTuner — coordinator that drives an ITuningStrategy against a
 * PIDController.
 *
 * Role: the strategy implements the *algorithm* (relay feedback, twiddle, …);
 * the coordinator owns *policy* — safety limits, abort handling, saving the
 * original gains so they can be restored on failure, and writing the tuned
 * gains back to the PID on success.
 *
 * Typical usage:
 *
 *     RelayFeedbackStrategy strategy(255.0f, 0.5f);
 *     AutoPIDTuner tuner(strategy);
 *     SafetyLimits limits = { 10.0f, 15.0f, false };
 *     tuner.set_safety_limits(limits);
 *     tuner.begin(pid, 0.0f, -255.0f, 255.0f, millis());
 *
 *     while (!tuner.is_done()) {
 *         float output = tuner.step(read_pitch(), millis());
 *         drive_motors(output);
 *         delay(5);
 *     }
 *
 *     if (tuner.succeeded()) {
 *         tuner.apply_to(pid);
 *     } else {
 *         tuner.restore_original(pid);
 *     }
 *
 * See: docs/findings/MASTER_DESIGN.md §4.5
 *      docs/findings/auto_pid_tuning_research.md
 */

#ifndef AUTO_PID_TUNER_H
#define AUTO_PID_TUNER_H

#include "tuning_strategy.h"
#include "pid_controller.h"

class AutoPIDTuner {
public:
    explicit AutoPIDTuner(ITuningStrategy& strategy);

    // Override the default (very permissive) safety limits. Defaults:
    //   max_angle_deg    = 1e6 (effectively disabled)
    //   max_duration_sec = 30.0
    //   abort_requested  = false
    void set_safety_limits(const SafetyLimits& limits);

    // Cache the PID's current gains, then start the strategy. The caller is
    // responsible for actually clamping the PID outputs to [output_min,
    // output_max] (the strategy returns control outputs in that range, but
    // the PID itself is untouched until apply_to() is called).
    void begin(PIDController& pid,
               float setpoint,
               float output_min,
               float output_max,
               uint32_t now_ms);

    // Run one step. Returns the output the strategy wants applied to the
    // plant *now* (e.g., relay amplitude ±255). Pre-checks safety tripwires
    // and stamps the result with a failure_reason if any tripped.
    float step(float measurement, uint32_t now_ms);

    bool is_done() const;
    bool succeeded() const;
    const TuningResult& result() const;

    // Overwrite the PID's gains with the tuned values. Safe to call only
    // after is_done(); recommended only when succeeded().
    void apply_to(PIDController& pid) const;

    // Restore the gains cached in begin(). Use after a failed tune.
    void restore_original(PIDController& pid) const;

    // Tell the tuner to stop at the next step (e.g., user pressed abort).
    void request_abort();

private:
    ITuningStrategy& strategy_;
    SafetyLimits     safety_;
    bool             abort_requested_;
    bool             done_;
    TuningResult     result_;

    float    original_kp_;
    float    original_ki_;
    float    original_kd_;
    uint32_t start_ms_;

    // Build a TuningResult representing an early-failure (no strategy result
    // available yet). Used when safety tripwires fire before the strategy
    // has had a chance to produce its own diagnostics.
    static TuningResult make_failure_result_(const char* reason);
};

#endif // AUTO_PID_TUNER_H
