/**
 * RelayFeedbackStrategy — Åström-Hägglund relay auto-tuning.
 *
 * Replaces the PID temporarily with a hysteretic relay (PID-convention
 * error = setpoint - measurement, negative-feedback relay):
 *   output = +amplitude  when (setpoint - measurement) >  +hysteresis
 *          = -amplitude  when (setpoint - measurement) <  -hysteresis
 *
 * The plant settles into a self-sustained limit cycle at its ultimate
 * frequency ωu. Measuring the period Tu and the peak deflection `a` yields
 *
 *   Ku = (4 * amplitude) / (π * a)
 *
 * From Ku and Tu we derive PID gains via either the AMIGO formulas
 * (Åström-Hägglund 2002, default — better phase margin) or the classic
 * Ziegler-Nichols relay formulas. See set_use_classic_zn().
 *
 * Termination:
 *   - Success: `cycles_to_average` complete oscillation periods recorded;
 *     Ku, Tu, and gains are written to TuningResult; is_done() returns true
 *     with converged == true.
 *   - Failure: safety tripwire (abort, max_duration, max_angle) → is_done()
 *     true with converged == false and a failure_reason string.
 *
 * Notes on units:
 *   - `amplitude` is in plant-input units (e.g., PWM counts).
 *   - `a` is measured in plant-output units (e.g., degrees of pitch).
 *   - Therefore Ku has units (input / output), which matches a PID Kp.
 *   - `hysteresis` is in plant-output units. Typical: 1-2% of expected swing
 *     to avoid sensor-noise chatter.
 *
 * The implementation uses raw arrays (no STL) and only `float` math, so it
 * compiles on AVR (Arduino Mega/Nano) and any 32-bit MCU we target later.
 *
 * Reference: K.J. Åström, T. Hägglund, "Automatic Tuning of Simple
 * Regulators with Specifications on Phase and Amplitude Margins" (1984);
 * "Revisiting the Ziegler-Nichols step response method for PID control"
 * (2004).
 *
 * See: docs/findings/auto_pid_tuning_research.md
 */

#ifndef RELAY_FEEDBACK_H
#define RELAY_FEEDBACK_H

#include "../tuning_strategy.h"

class RelayFeedbackStrategy : public ITuningStrategy {
public:
    // amplitude:   relay output magnitude (plant-input units, e.g. ±255 PWM)
    // hysteresis:  deadband around setpoint, in plant-output units. Should
    //              be a few times the sensor noise floor.
    RelayFeedbackStrategy(float amplitude, float hysteresis);

    // Number of complete oscillation cycles to average before declaring
    // convergence. Bigger = noise-rejection ↑, time-to-tune ↑. Clamped to
    // [1, kMaxCycles]. Default: 4.
    void set_cycles_to_average(uint8_t n);

    // Switch the gain formula:
    //   false (default) : AMIGO     — Kp=0.6Ku, Ti=0.5Tu, Td=0.125Tu
    //   true            : classic ZN-relay variant — same coefficients in
    //                     this library (we document the choice; both
    //                     formulas have been historically reported as
    //                     equivalent for the relay method).
    // We keep the toggle so future revisions can diverge the formulas
    // without breaking the API.
    void set_use_classic_zn(bool yes);

    // ITuningStrategy interface
    void begin(float setpoint,
               float output_min,
               float output_max,
               uint32_t now_ms) override;
    float step(float measurement,
               const SafetyLimits& safety,
               uint32_t now_ms) override;
    bool is_done() const override { return done_; }
    TuningResult get_result() const override { return result_; }
    const char* name() const override { return "relay_feedback"; }

    // Maximum number of cycles we'll retain in the ring buffer. Anything
    // higher just overwrites older entries (we always average over the most
    // recent cycles_to_average_).
    static const uint8_t kMaxCycles = 8;

private:
    // ---- Configuration ----
    float   amplitude_;
    float   hysteresis_;
    uint8_t cycles_to_average_;
    bool    use_classic_zn_;

    // ---- Session inputs (captured in begin()) ----
    float   setpoint_;
    float   output_min_;
    float   output_max_;
    uint32_t start_ms_;

    // ---- State ----
    bool     done_;
    bool     relay_state_high_;     // true => output = +amplitude
    uint32_t last_high_to_low_ms_;  // last HIGH->LOW transition timestamp
    bool     have_high_to_low_;     // have we recorded one yet?

    // Peak tracking. We continuously update the max and min measurement
    // observed during the current "measurement window". A window starts on
    // a HIGH->LOW transition and ends on the next HIGH->LOW transition (one
    // full period). At the end of the window, peak-to-peak = max - min and
    // amplitude a = (max - min) / 2.
    float    cycle_max_;            // max measurement since window start
    float    cycle_min_;            // min measurement since window start
    bool     window_open_;          // is a measurement window currently active?

    uint8_t  cycles_recorded_;
    float    period_ring_[kMaxCycles];   // most recent period samples (sec)
    float    amplitude_ring_[kMaxCycles]; // most recent half-peak-to-peak (a)

    TuningResult result_;

    // ---- Helpers ----
    void   reset_state_();
    void   finalize_failure_(const char* reason);
    void   finalize_success_();
    float  ring_mean_(const float* ring, uint8_t count) const;
};

#endif // RELAY_FEEDBACK_H
