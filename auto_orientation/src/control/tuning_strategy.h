/**
 * Auto-Tuning Strategy Interface (Phase 4.5b)
 *
 * ITuningStrategy is the abstract base for every auto-tune algorithm
 * (relay-feedback, twiddle, RLS system-ID, ...). The AutoPIDTuner coordinator
 * drives any concrete strategy through this interface, so the *which* and the
 * *how* are decoupled: each strategy lives in its own .cpp under
 * `src/control/tuners/` and is compile-selected via `#ifdef USE_TUNER_xxx`
 * (Decision D10 in MASTER_DESIGN.md).
 *
 * Design notes:
 *   - No virtual destructors with bodies — keep the vtable small for AVR.
 *   - All data passed by value or const-ref; no dynamic allocation.
 *   - SafetyLimits is a plain struct so the application can flip
 *     `abort_requested` from a button ISR or watchdog without touching the
 *     tuner internals.
 *   - TuningResult carries both the gains and enough diagnostic data
 *     (ultimate_gain, ultimate_period_sec, phase_margin_estimate_deg) for the
 *     dashboard and the regression tests to make sense of the run.
 *
 * See: docs/findings/MASTER_DESIGN.md §4.5
 *      docs/findings/auto_pid_tuning_research.md
 */

#ifndef TUNING_STRATEGY_H
#define TUNING_STRATEGY_H

#include <stdint.h>

// ---------------------------------------------------------------------------
// Result of an auto-tuning run.
//
// Always valid after the strategy reports is_done() == true, regardless of
// whether the run converged. On failure, `failure_reason` points to a static
// C-string explaining why (e.g., "max_duration_exceeded"). On success it is
// nullptr.
//
// The phase-margin field is a *post-hoc* estimate from the chosen formula,
// not a measured quantity; relay-feedback tuners typically target ~45-60°.
// ---------------------------------------------------------------------------
struct TuningResult {
    float kp;
    float ki;
    float kd;
    float ultimate_gain;             // Ku (relay-feedback) or 0 if N/A
    float ultimate_period_sec;       // Tu (relay-feedback) or 0 if N/A
    float phase_margin_estimate_deg; // approximate phase margin of resulting PID
    bool  converged;                 // true on success, false otherwise
    const char* failure_reason;      // nullptr on success
};

// ---------------------------------------------------------------------------
// Safety tripwires honoured by every strategy.
//
//   max_angle_deg     : abort if |measurement - setpoint| > this. Pendulum
//                       guidance is 10°; drones typically 30°. Set to a large
//                       value (e.g., 1e6) to disable.
//   max_duration_sec  : abort if elapsed > this. Hard upper bound on a tuning
//                       run; relay-feedback usually finishes inside 15 s.
//   abort_requested   : caller flips this true from a button ISR / dashboard
//                       to force the tuner to bail out at the next step.
// ---------------------------------------------------------------------------
struct SafetyLimits {
    float max_angle_deg;
    float max_duration_sec;
    bool  abort_requested;
};

// ---------------------------------------------------------------------------
// Abstract tuning strategy.
//
// Lifecycle:
//   1. begin(setpoint, output_min, output_max, now_ms)
//   2. Repeatedly: step(measurement, safety, now_ms) -> control output
//   3. When is_done() returns true, fetch get_result().
//
// Implementations MUST tolerate being called past is_done() — they should
// keep returning a safe output (typically 0) and not mutate the result.
// ---------------------------------------------------------------------------
class ITuningStrategy {
public:
    virtual ~ITuningStrategy() {}

    // Called once at the start of a tuning session. The strategy must reset
    // its internal state here.
    virtual void begin(float setpoint,
                       float output_min,
                       float output_max,
                       uint32_t now_ms) = 0;

    // Called every PID step. Returns the desired plant input (e.g. relay
    // output) and updates internal state. The strategy is responsible for
    // detecting safety violations from `safety` and ending the run.
    virtual float step(float measurement,
                       const SafetyLimits& safety,
                       uint32_t now_ms) = 0;

    // True once the strategy has finished (success or failure).
    virtual bool is_done() const = 0;

    // Final result. Stable to call after is_done() turns true.
    virtual TuningResult get_result() const = 0;

    // Human-readable strategy name for logging.
    virtual const char* name() const = 0;
};

#endif // TUNING_STRATEGY_H
