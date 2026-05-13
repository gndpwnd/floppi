/**
 * Unit tests for the generic single-axis PIDController (Phase 4.5a).
 *
 * Framework: Unity (http://www.throwtheswitch.org/unity)
 * Build:     native env or arduino_mega test env (per PlatformIO test runner)
 *
 * Test coverage:
 *   1. test_pid_p_only                  - ki=0, kd=0 -> output == kp * error
 *   2. test_pid_anti_windup             - sustained error bounded by integral clamp
 *   3. test_pid_output_clamp            - final output respects [min, max]
 *   4. test_pid_d_on_measurement        - setpoint step does NOT kick the D term
 *   5. test_pid_d_on_error              - setpoint step DOES kick the D term
 *   6. test_pid_reset                   - reset() clears integral and history
 *   7. test_pid_settling_step_response  - Kp=1, Ki=0.1, Kd=0 converges to setpoint
 *   8. test_pid_known_gains_match_ino   - matches PID_v1 reference within ±2 PWM
 *
 * The last test is the regression contract for SelfBallancingRobot3.ino. It
 * replays a synthetic pitch trajectory (step + sinusoid + small noise) through
 * both the new PIDController and an inline reference that mimics PID_v1's
 * arithmetic exactly. They must agree to within ±2 PWM units at every step.
 */

#include <unity.h>

#include <math.h>
#include <stdint.h>

#include "../src/control/pid_controller.h"

// ============================================================================
// Unity scaffolding
// ============================================================================

void setUp(void)    {}
void tearDown(void) {}

// Common float tolerance for analytic checks.
static const float kEps = 1e-4f;

// ============================================================================
// Test 1: P-only behaviour
// ============================================================================

void test_pid_p_only(void) {
    PIDController pid(2.0f, 0.0f, 0.0f, -1000.0f, 1000.0f);
    pid.set_setpoint(10.0f);

    // First compute: D is skipped, I is 0 (ki=0), so output = kp * error.
    float out = pid.compute(/*measurement=*/4.0f, /*dt_ms=*/5);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 12.0f, out);   // 2 * (10 - 4)
    TEST_ASSERT_FLOAT_WITHIN(kEps, 12.0f, pid.get_p_term());
    TEST_ASSERT_FLOAT_WITHIN(kEps,  0.0f, pid.get_i_term());
    TEST_ASSERT_FLOAT_WITHIN(kEps,  0.0f, pid.get_d_term());

    // Second compute: D term active but kd=0, so still kp * error.
    out = pid.compute(7.0f, 5);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 6.0f, out);    // 2 * (10 - 7)

    // Negative error -> negative output.
    out = pid.compute(15.0f, 5);
    TEST_ASSERT_FLOAT_WITHIN(kEps, -10.0f, out);  // 2 * (10 - 15)
}

// ============================================================================
// Test 2: Anti-windup
// ============================================================================

void test_pid_anti_windup(void) {
    // Output range is small relative to the gains, so a sustained error
    // would otherwise drive the integral to infinity. Verify it stays bounded.
    PIDController pid(0.0f, 10.0f, 0.0f, -100.0f, 100.0f);
    pid.set_setpoint(0.0f);

    // Drive a steady-state error of -50 (measurement above setpoint) for
    // 10 seconds at 5 ms intervals = 2000 iterations.
    for (int i = 0; i < 2000; ++i) {
        pid.compute(/*measurement=*/50.0f, /*dt_ms=*/5);
    }
    // The I term is ki * integral; with anti-windup it must be inside the
    // output range. With error = -50 sustained, the I term saturates negative.
    const float i_term = pid.get_i_term();
    TEST_ASSERT_TRUE_MESSAGE(i_term >= -100.0f - kEps, "I term below output_min");
    TEST_ASSERT_TRUE_MESSAGE(i_term <=  100.0f + kEps, "I term above output_max");
    TEST_ASSERT_FLOAT_WITHIN(kEps, -100.0f, i_term);   // saturated negative

    // Now flip the sign: the integral must be able to recover, not be stuck.
    // Drive measurement = -50 (positive error) for 1 second. After recovery
    // the I term should be positive (no permanent windup).
    for (int i = 0; i < 200; ++i) {
        pid.compute(/*measurement=*/-50.0f, /*dt_ms=*/5);
    }
    TEST_ASSERT_TRUE_MESSAGE(pid.get_i_term() > 0.0f,
                             "Integral failed to unwind after sign flip");
}

// ============================================================================
// Test 3: Output clamp
// ============================================================================

void test_pid_output_clamp(void) {
    PIDController pid(1000.0f, 0.0f, 0.0f, -50.0f, 50.0f);
    pid.set_setpoint(0.0f);

    // Large positive error -> P would be -1000*100 = -100000, must clamp to -50.
    float out = pid.compute(100.0f, 5);
    TEST_ASSERT_FLOAT_WITHIN(kEps, -50.0f, out);
    TEST_ASSERT_FLOAT_WITHIN(kEps, -50.0f, pid.get_output());

    // Large negative error -> P would be +100000, must clamp to +50.
    out = pid.compute(-100.0f, 5);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 50.0f, out);

    // Asymmetric range works too.
    pid.set_output_limits(-10.0f, 200.0f);
    out = pid.compute(100.0f, 5);
    TEST_ASSERT_FLOAT_WITHIN(kEps, -10.0f, out);
    out = pid.compute(-100.0f, 5);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 200.0f, out);
}

// ============================================================================
// Test 4: D-on-measurement suppresses derivative kick
// ============================================================================

void test_pid_d_on_measurement(void) {
    // kd is large to make derivative-kick easy to detect.
    PIDController pid(0.0f, 0.0f, 100.0f, -10000.0f, 10000.0f);
    pid.set_d_on_measurement(true);
    pid.set_setpoint(0.0f);

    // Two steady samples at measurement = 5 -> establishes history, no D yet.
    pid.compute(5.0f, 10);  // first compute: D skipped
    float out_steady = pid.compute(5.0f, 10);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, out_steady);  // no error/measurement change

    // Now change the setpoint dramatically. Measurement is still 5.
    // With D-on-measurement, the change in measurement is 0 -> D term is 0.
    pid.set_setpoint(50.0f);
    float out_after_setpoint = pid.compute(5.0f, 10);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, pid.get_d_term());
    // P=0, I=0 (ki=0) so total output is 0 even though error jumped from -5 to 45.
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, out_after_setpoint);

    // A real measurement change DOES produce a D term (sanity check).
    float out_moving = pid.compute(10.0f, 10);
    // measurement went 5 -> 10 in 10 ms = +500/sec; d_term = -kd * 500 = -50000
    // but kp/ki = 0 and the output is clamped to ±10000. So:
    TEST_ASSERT_FLOAT_WITHIN(kEps, -10000.0f, out_moving);
}

// ============================================================================
// Test 5: D-on-error reacts to setpoint change
// ============================================================================

void test_pid_d_on_error(void) {
    PIDController pid(0.0f, 0.0f, 100.0f, -10000.0f, 10000.0f);
    pid.set_d_on_measurement(false);   // d on error
    pid.set_setpoint(0.0f);

    // Establish history.
    pid.compute(5.0f, 10);   // first compute: D skipped, history armed
    float out_steady = pid.compute(5.0f, 10);
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, out_steady);

    // Setpoint step: error jumps from -5 to +45 in one cycle.
    // d_err = (45 - (-5)) / 0.010 s = 5000 /sec
    // d_term = kd * 5000 = +500000 -> clamped to +10000.
    pid.set_setpoint(50.0f);
    float out_after_setpoint = pid.compute(5.0f, 10);
    TEST_ASSERT_TRUE_MESSAGE(pid.get_d_term() > 0.0f,
        "D-on-error should produce a positive kick on a positive setpoint step");
    TEST_ASSERT_FLOAT_WITHIN(kEps, 10000.0f, out_after_setpoint);
}

// ============================================================================
// Test 6: reset()
// ============================================================================

void test_pid_reset(void) {
    PIDController pid(2.0f, 10.0f, 5.0f, -500.0f, 500.0f);
    pid.set_setpoint(0.0f);

    // Build up some state.
    for (int i = 0; i < 50; ++i) {
        pid.compute(20.0f, 5);
    }
    // Integral should be non-zero now.
    TEST_ASSERT_TRUE_MESSAGE(pid.get_i_term() != 0.0f,
                             "I term should be non-zero before reset");

    pid.reset();

    // After reset: integral cleared, output zero, first compute armed again.
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, pid.get_i_term());
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, pid.get_output());

    // The first compute after reset should skip D (no history) regardless of
    // the size of the measurement change. Verify by setting a measurement
    // very different from the prior steady-state.
    float out = pid.compute(100.0f, 5);
    // P = 2 * (0 - 100) = -200
    // I = 10 * (-100 * 0.005) = -5
    // D = 0 (first compute)
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, pid.get_d_term());
    TEST_ASSERT_FLOAT_WITHIN(kEps, -200.0f, pid.get_p_term());
    TEST_ASSERT_FLOAT_WITHIN(kEps, -5.0f, pid.get_i_term());
    TEST_ASSERT_FLOAT_WITHIN(kEps, -205.0f, out);
}

// ============================================================================
// Test 7: Step response converges to setpoint
// ============================================================================

// Step-response convergence sanity check.
//
// Toy first-order plant: dx/dt = a * (u - x). Unity-gain at steady state.
// With Kp=1, Ki=0.1, Kd=0 per spec, the small integrator means steady-state
// error closes slowly (Ki * dt accumulation, with dt = 10 ms per step).
// What we *can* assert about a correct PID:
//   1. The plant state monotonically approaches the setpoint (after the
//      first settling transient) — no oscillation, no divergence.
//   2. Within a reasonable horizon, x reaches >= 80% of the setpoint.
//   3. The final state is closer to the setpoint than to the start.
// These together rule out: wrong sign, no integral action, runaway windup,
// or D-term sign errors. They do NOT pin down a specific settling time
// because that's a function of the plant the test author chose, not the
// PID under test.
void test_pid_settling_step_response(void) {
    PIDController pid(1.0f, 0.1f, 0.0f, -100.0f, 100.0f);
    pid.set_setpoint(10.0f);

    float x = 0.0f;
    const float a = 5.0f;       // plant pole (rad/s)
    const float dt = 0.010f;    // 10 ms
    const uint16_t dt_ms = 10;
    const int N = 3000;         // 30 seconds wall time

    float x_prev_peak = 0.0f;
    int overshoot_events = 0;
    bool reached_80pct = false;
    int reached_80pct_at = -1;
    for (int i = 0; i < N; ++i) {
        float u = pid.compute(x, dt_ms);
        x += a * (u - x) * dt;

        // Track how many times the response oscillates past its previous peak
        // (a stable monotonic response should have 0; overshoot followed by
        // damping is acceptable but should not repeat indefinitely).
        if (x > x_prev_peak + 0.01f) {
            x_prev_peak = x;
        } else if (x < x_prev_peak - 1.0f) {
            // Drop > 1.0 below the previous peak counts as an oscillation event.
            overshoot_events++;
            x_prev_peak = x;
        }

        if (!reached_80pct && x >= 8.0f) {
            reached_80pct = true;
            reached_80pct_at = i;
        }
    }

    TEST_ASSERT_TRUE_MESSAGE(reached_80pct,
        "PID step response failed to reach 80% of setpoint within 30 s");
    TEST_ASSERT_TRUE_MESSAGE(x > 5.0f,
        "Final plant state should be more than halfway to setpoint");
    TEST_ASSERT_TRUE_MESSAGE(x <= 12.0f,
        "Final plant state should not blow past the setpoint (divergence guard)");
    TEST_ASSERT_TRUE_MESSAGE(overshoot_events < 3,
        "PID response oscillates — possible D-term sign error or divergence");
    // Silence unused-variable warnings in release builds.
    (void)reached_80pct_at;
}

// ============================================================================
// Test 8: Match PID_v1 reference output within ±2 PWM units
// ============================================================================

/**
 * Inline reference that mimics PID_v1's arithmetic exactly.
 *
 * PID_v1 internals (DIRECT mode, default P-on-error, D-on-measurement):
 *   On SetTunings():
 *     dispKp = Kp; dispKi = Ki; dispKd = Kd;
 *     sampleSec = SampleTime_ms / 1000.0
 *     kp_int = dispKp
 *     ki_int = dispKi * sampleSec
 *     kd_int = dispKd / sampleSec
 *   Each Compute() at the fixed sample tick:
 *     error  = setpoint - input
 *     dInput = input - lastInput
 *     outputSum += ki_int * error      -> clamped to [OutMin, OutMax]
 *     output = kp_int * error + outputSum - kd_int * dInput
 *     output = clamp(output, OutMin, OutMax)
 *     lastInput = input
 *
 * This function reproduces that step-by-step. Both implementations use the
 * same fixed dt, so the time-scaling collapses to the same multipliers.
 */
struct PidV1Ref {
    float kp, ki_scaled, kd_scaled;
    float out_min, out_max;
    float output_sum;
    float last_input;
    bool  first;

    PidV1Ref(float kp_, float ki_, float kd_, float dt_ms,
             float out_min_, float out_max_)
        : kp(kp_),
          ki_scaled(ki_ * (dt_ms / 1000.0f)),
          kd_scaled(kd_ / (dt_ms / 1000.0f)),
          out_min(out_min_), out_max(out_max_),
          output_sum(0.0f), last_input(0.0f), first(true) {}

    float step(float input, float setpoint) {
        const float error = setpoint - input;

        // Integral accumulator with PID_v1-style clamping.
        output_sum += ki_scaled * error;
        if (output_sum > out_max) output_sum = out_max;
        if (output_sum < out_min) output_sum = out_min;

        // D-on-measurement; first step has no prior input, so dInput = 0.
        float d_input;
        if (first) {
            d_input = 0.0f;
            first = false;
        } else {
            d_input = input - last_input;
        }

        float output = kp * error + output_sum - kd_scaled * d_input;
        if (output > out_max) output = out_max;
        if (output < out_min) output = out_min;

        last_input = input;
        return output;
    }
};

void test_pid_known_gains_match_ino(void) {
    // Same gains as SelfBallancingRobot3.ino:
    //   Kp = 65, Ki = 12, Kd = 38, sample = 5 ms, output = ±255.
    const float Kp = 65.0f;
    const float Ki = 12.0f;
    const float Kd = 38.0f;
    const uint16_t dt_ms = 5;
    const float dt_s = dt_ms / 1000.0f;
    const float out_min = -255.0f;
    const float out_max =  255.0f;

    PIDController pid(Kp, Ki, Kd, out_min, out_max);
    pid.set_sample_time(dt_ms);
    pid.set_setpoint(0.0f);
    pid.set_d_on_measurement(true);
    // PID_v1 has no D-term LPF; disable ours for the strict parity check.
    pid.set_d_term_lpf_tau_sec(0.0f);

    PidV1Ref ref(Kp, Ki, Kd, (float)dt_ms, out_min, out_max);

    // Synthetic input: a step (0 -> 5 deg) followed by a 2 Hz sinusoid riding
    // on top, with mild white-ish ramp to exercise both P and D. 500 samples
    // at 5 ms = 2.5 seconds of trajectory. The .ino has PITCH_OFFSET=-8.6 but
    // that's just a constant bias on the input — irrelevant to PID dynamics
    // when comparing two PIDs receiving the same input, so we omit it here.
    const int N = 500;
    const float setpoint = 0.0f;

    float max_diff = 0.0f;
    for (int i = 0; i < N; ++i) {
        const float t = i * dt_s;
        float pitch;
        if (i < 50) {
            // First 50 samples: hold at 0 to let both PIDs sync history.
            pitch = 0.0f;
        } else {
            // Step to 5°, plus 2 Hz sinusoid of amplitude 2°.
            pitch = 5.0f + 2.0f * sinf(2.0f * (float)M_PI * 2.0f * t);
        }

        const float ours = pid.compute(pitch, dt_ms);
        const float theirs = ref.step(pitch, setpoint);

        const float diff = fabsf(ours - theirs);
        if (diff > max_diff) max_diff = diff;

        // Per-sample tolerance: ±2 PWM units.
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(2.0f, theirs, ours,
            "PID output diverged from PID_v1 reference by > 2 PWM units");
    }

    // Bonus sanity check: max diff over the whole run should also fit.
    TEST_ASSERT_TRUE_MESSAGE(max_diff <= 2.0f,
        "Max PID divergence over the run exceeded 2 PWM units");
}

// ============================================================================
// Unity main
// ============================================================================

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pid_p_only);
    RUN_TEST(test_pid_anti_windup);
    RUN_TEST(test_pid_output_clamp);
    RUN_TEST(test_pid_d_on_measurement);
    RUN_TEST(test_pid_d_on_error);
    RUN_TEST(test_pid_reset);
    RUN_TEST(test_pid_settling_step_response);
    RUN_TEST(test_pid_known_gains_match_ino);
    return UNITY_END();
}
