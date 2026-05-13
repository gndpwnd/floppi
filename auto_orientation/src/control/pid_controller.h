/**
 * Generic Single-Axis PID Controller
 *
 * Phase 4.5a of the auto_orientation framework. Sensor-agnostic, AVR-friendly,
 * no dynamic allocation, no STL. The API surface intentionally mirrors PID_v1
 * (the legacy library used by SelfBallancingRobot3.ino) so the balancing-robot
 * reference application can migrate without behavioural surprises.
 *
 * Features:
 *   - Time-scaled coefficients (kp, ki, kd are user-facing; internal math uses
 *     dt in seconds, so changing the sample period is safe and re-tuning the
 *     gains at runtime is supported).
 *   - Anti-windup via integral clamping (back-calculation style — the integral
 *     itself is bounded so that ki * integral stays inside the output limits).
 *   - Derivative on measurement by default (suppresses derivative-kick on
 *     setpoint changes). Can be switched to derivative-on-error for cases
 *     where setpoint tracking response matters.
 *   - First-compute guard skips the D term on the first call (no prior
 *     measurement to differentiate against).
 *   - All math in float — matches the rest of the auto_orientation project
 *     and works on AVR (where double == float anyway).
 *   - Inspection accessors for p/i/d/output/error — needed for the auto-tuner
 *     feedback loop and dashboard telemetry.
 *
 * Reference behaviour: PID_v1 with Kp=65 Ki=12 Kd=38 sample=5 ms output=±255
 * (see docs/archive/balancing_robot_reference/SelfBallancingRobot3.ino).
 *
 * See: docs/findings/MASTER_DESIGN.md §4.5
 *      docs/findings/auto_pid_tuning_research.md
 *      docs/findings/disturbance_compensation_research.md
 */

#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include <stdint.h>

class PIDController {
public:
    /**
     * Construct a PID controller with initial gains and output limits.
     *
     * @param kp          Proportional gain
     * @param ki          Integral gain (per second)
     * @param kd          Derivative gain (per second)
     * @param output_min  Lower bound for output (e.g., -255 for PWM)
     * @param output_max  Upper bound for output (e.g., +255 for PWM)
     */
    PIDController(float kp, float ki, float kd,
                  float output_min, float output_max);

    /**
     * Set the nominal sample period in milliseconds.
     *
     * The coefficients are time-scaled in compute() using the *actual* dt
     * passed in, so this is purely informational / used by auto-tuners that
     * want to know the intended cadence. Defaults to 5 ms (matches the
     * legacy SelfBallancingRobot3.ino).
     */
    void set_sample_time(uint16_t sample_ms);
    uint16_t get_sample_time() const;

    /**
     * Set the output clamps. Anti-windup uses these to bound the integral.
     */
    void set_output_limits(float min, float max);

    /**
     * Re-tune the gains at runtime (e.g., from the auto-tuner).
     */
    void set_tunings(float kp, float ki, float kd);
    void get_tunings(float& kp, float& ki, float& kd) const;

    /**
     * Set the target setpoint (in measurement units).
     */
    void set_setpoint(float setpoint);
    float get_setpoint() const;

    /**
     * Choose derivative source:
     *   true  (default) -> D on measurement; suppresses derivative-kick
     *   false           -> D on error;       responds to setpoint changes
     */
    void set_d_on_measurement(bool d_on_measurement);
    bool is_d_on_measurement() const;

    /**
     * D-term measurement low-pass time constant in seconds.
     *
     * One-pole IIR applied to the measurement that feeds the D-term only —
     * the P-term continues to use the raw measurement so step response is
     * not delayed. Kills BNO055 fused-Euler quantization noise (~0.05°/step)
     * that would otherwise be amplified into ~250 PWM/quantum spikes when
     * differentiated at 200 Hz with Kd=8.
     *
     * Default: 0.015 s (~10 Hz cutoff at 5 ms dt). Set to 0 to disable
     * filtering (D-term operates on raw measurement — matches PID_v1).
     */
    void set_d_term_lpf_tau_sec(float tau_sec);
    float get_d_term_lpf_tau_sec() const;

    /**
     * I-term contribution limit (PWM, NOT integral value). Bounds
     * |ki * integral| ≤ this value. Use to prevent integral windup from
     * dominating the output during sustained errors — critical for unstable
     * plants where the integrator can saturate before the bot recovers.
     *
     * Default: equal to output_max (no extra limit). Set to e.g. 50 to
     * cap the integral's authority to 50 PWM regardless of output range.
     * 0 or negative disables the explicit limit (falls back to output range).
     */
    void set_i_term_limit(float pwm);
    float get_i_term_limit() const;

    /**
     * Run one PID step.
     *
     * @param measurement  Current sensor reading (e.g., pitch in degrees)
     * @param dt_ms        Elapsed milliseconds since the previous compute().
     *                     If 0, the previous output is returned unchanged
     *                     (caller should not call compute() faster than the
     *                     scheduler can resolve).
     * @return             New control output, clamped to [output_min, output_max]
     */
    float compute(float measurement, uint16_t dt_ms);

    /**
     * Variant that takes an explicitly-measured rate (e.g., gyro deg/s) and
     * uses it as the D-term source instead of numerically differentiating
     * `measurement`. P-term, I-term and anti-windup are identical to compute().
     *
     * D-term semantics: d_term = -kd_ * rate_dps  (the sign matches D-on-
     * measurement: a positive rate moving the measurement away from setpoint
     * should subtract from the output, so the controller damps motion). No
     * division by dt, no LPF — the rate IS the derivative.
     *
     * Units consistency: in compute(), `(measurement[t]-measurement[t-1])/dt`
     * has the same units as `rate_dps` (deg/s for a pitch loop), so kd_ keeps
     * the same units (PWM per deg/s) and numerical magnitudes are roughly
     * compatible between the two paths.
     *
     * Source: docs/findings/research_osoyoo_reference_implementation.md §3 &
     * §9 — every successful AVR balance bot (Osoyoo, Lauszus, Balanduino)
     * uses raw gyro as the D-term source, not numerical differentiation of
     * a fused/filtered angle. Numerical differentiation of BNO055 NDOF pitch
     * adds ~25 ms of effective phase lag (NDOF group delay + LPF + diff).
     * The raw gyro is the instantaneous physical measurement; using it as
     * D buys back that phase margin.
     */
    float compute_with_rate(float measurement, float rate_dps,
                            uint16_t dt_ms);

    /**
     * Reset internal state. Use when entering RUN state from IDLE, or after
     * a long pause where the integral may be stale. Clears integral,
     * derivative history, and re-arms the first-compute guard.
     */
    void reset();

    /**
     * Inspection accessors for the auto-tuner and dashboard.
     * Values reflect the most recent compute() call.
     */
    float get_p_term() const;
    float get_i_term() const;
    float get_d_term() const;
    float get_output() const;
    float get_error() const;

private:
    // Gains (user-facing, "per second" semantics)
    float kp_;
    float ki_;
    float kd_;

    // Output clamps
    float output_min_;
    float output_max_;

    // Setpoint
    float setpoint_;

    // Persistent state
    float integral_;            // accumulated error * dt_seconds
    float last_measurement_;    // for D-on-measurement
    float last_error_;          // for D-on-error
    float last_output_;         // returned when dt_ms == 0

    // D-term measurement low-pass (Tier 1.3, IMPLEMENTATION_PLAN.md §1.3).
    // ~15 ms time constant default — kills BNO055 fused-Euler quantization
    // noise (~0.05°/step) that gets amplified into ~250 PWM/quantum spikes
    // by Kd when differentiating a quantized signal at 200 Hz. The LPF feeds
    // the D-term only; P-term stays on raw measurement so step response is
    // not delayed. Set tau_sec to 0 to disable (PID_v1 parity).
    float measurement_lpf_;
    bool  measurement_lpf_init_;
    float d_term_lpf_tau_sec_;

    // I-term contribution limit (PWM). 0 = use output_max range. Prevents
    // integral windup from dominating the output on unstable plants.
    float i_term_limit_pwm_;

    // Mode flags
    bool  d_on_measurement_;
    bool  first_compute_;       // skip D on first sample
    uint16_t sample_ms_;        // informational

    // Diagnostics (last-step snapshot for the inspection accessors)
    float last_p_term_;
    float last_i_term_;
    float last_d_term_;
    float last_error_value_;

    // Helpers
    static float clamp_(float v, float lo, float hi);
    void clamp_integral_();     // bound integral so ki * integral fits in output range
};

#endif // PID_CONTROLLER_H
