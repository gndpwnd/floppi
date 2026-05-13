/**
 * Unit tests for RelayFeedbackStrategy + AutoPIDTuner coordinator (Phase 4.5b).
 *
 * Framework: Unity (http://www.throwtheswitch.org/unity)
 *
 * Test coverage:
 *   1. test_relay_converges_known_plant         - relay tunes a 2nd-order
 *                                                 underdamped oscillator;
 *                                                 expect Ku / Tu within
 *                                                 sensible bounds and
 *                                                 converged == true
 *   2. test_relay_aborts_on_safety_max_duration - max_duration_sec = 0.1
 *                                                 forces timeout
 *   3. test_relay_aborts_on_safety_max_angle    - max_angle_deg very small
 *                                                 trips the angle wire
 *   4. test_relay_aborts_on_user_request        - coordinator.request_abort()
 *                                                 makes the run fail with
 *                                                 reason "user_aborted"
 *   5. test_coordinator_caches_and_restores     - restore_original() puts
 *                                                 the PID's gains back to
 *                                                 their pre-tune values
 *   6. test_coordinator_applies_on_success      - apply_to() writes the
 *                                                 tuned gains into the PID
 *
 * Simulated plant (used by tests 1-4): a 2nd-order underdamped oscillator
 *   x_ddot = -omega0^2 * x - 2 * zeta * omega0 * x_dot + K * u
 * integrated by semi-implicit Euler at dt = 5 ms.
 */

#include <unity.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "../src/control/auto_pid_tuner.h"
#include "../src/control/pid_controller.h"
#include "../src/control/tuners/relay_feedback.h"

// ---------------------------------------------------------------------------
// Unity scaffolding
// ---------------------------------------------------------------------------

void setUp(void)    {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// Simulated plant: 2nd-order underdamped oscillator.
//
// state: x (position), x_dot (velocity)
// input: u (relay output)
// dynamics:  x_ddot = -omega0^2 * x - 2 * zeta * omega0 * x_dot + K * u
//
// omega0 = 2*pi (1 Hz natural)
// zeta   = 0.05 (low damping)
// K      = 100 (input gain)
//
// We deliberately use this near-undamped 2nd-order plant because it has a
// clean phase crossover near omega0 once the relay's hysteresis adds the
// extra phase lag needed to close the loop at -180 degrees.
// ---------------------------------------------------------------------------

struct OscillatorPlant {
    float x;
    float x_dot;
    float omega0;
    float zeta;
    float K;

    OscillatorPlant(float w0 = 2.0f * 3.14159265358979323846f,
                    float z  = 0.05f,
                    float k  = 100.0f)
        : x(0.0f), x_dot(0.0f), omega0(w0), zeta(z), K(k) {}

    // Semi-implicit Euler. dt in seconds.
    void step(float u, float dt) {
        const float x_ddot = -omega0 * omega0 * x
                             - 2.0f * zeta * omega0 * x_dot
                             + K * u;
        x_dot += x_ddot * dt;
        x     += x_dot  * dt;
    }
};

// ---------------------------------------------------------------------------
// Test 1: Relay converges on the simulated plant
// ---------------------------------------------------------------------------

void test_relay_converges_known_plant(void) {
    // Plant: 1 Hz natural frequency, lightly damped.
    OscillatorPlant plant;

    // Relay configuration. We use a modest amplitude (1.0 plant-input units)
    // because the plant's K=100 is large enough that ±1 already saturates
    // the response into a clean ±a oscillation without huge swings.
    const float relay_amp  = 1.0f;
    const float hysteresis = 0.01f;   // small, well below expected swing
    RelayFeedbackStrategy strategy(relay_amp, hysteresis);
    strategy.set_cycles_to_average(4);

    AutoPIDTuner tuner(strategy);

    SafetyLimits limits;
    limits.max_angle_deg    = 1.0e6f;   // disable for this test
    limits.max_duration_sec = 30.0f;
    limits.abort_requested  = false;
    tuner.set_safety_limits(limits);

    // Dummy PID — we only need it for begin()/apply_to()/restore_original().
    PIDController pid(0.0f, 0.0f, 0.0f, -relay_amp, relay_amp);

    const float setpoint   = 0.0f;
    const uint16_t dt_ms   = 5;
    const float    dt_s    = dt_ms / 1000.0f;
    uint32_t now_ms        = 0;
    tuner.begin(pid, setpoint, -relay_amp, relay_amp, now_ms);

    // Disturb the plant slightly so the relay actually has something to
    // chase from t=0. A tiny initial velocity is enough.
    plant.x_dot = 0.01f;

    const int max_steps = (int)(20.0f / dt_s);   // 20 s safety cap
    int steps_taken = 0;

    while (!tuner.is_done() && steps_taken < max_steps) {
        const float u = tuner.step(plant.x, now_ms);
        plant.step(u, dt_s);
        now_ms     += dt_ms;
        steps_taken++;
    }

    // ---- Assertions ----
    TEST_ASSERT_TRUE_MESSAGE(tuner.is_done(),
                             "Tuner failed to terminate within 20 s");
    TEST_ASSERT_TRUE_MESSAGE(tuner.succeeded(),
                             "Tuner did not converge on simple oscillator plant");

    const TuningResult& r = tuner.result();

    // Diagnostic print so the build log shows the actual numbers. Useful
    // when the analytical bounds need re-tuning for a future plant.
    printf("[relay] Ku=%.4f  Tu=%.4f s  Kp=%.4f  Ki=%.4f  Kd=%.4f  steps=%d\n",
           (double)r.ultimate_gain, (double)r.ultimate_period_sec,
           (double)r.kp, (double)r.ki, (double)r.kd, steps_taken);

    // Tu sanity bounds. For this 1 Hz natural / lightly-damped 2nd-order
    // plant the relay loop's phase-crossover lies well *above* ω0 — the
    // ideal 2nd-order only asymptotically approaches -180° phase, so the
    // small hysteresis lag picks a frequency several times ω0. Empirically
    // we see Tu around 0.15 s (~6 Hz). We bound loosely on both sides;
    // what matters is the algorithm produced a finite, positive period.
    TEST_ASSERT_TRUE_MESSAGE(r.ultimate_period_sec > 0.02f,
        "Ultimate period implausibly short (< 50 Hz)");
    TEST_ASSERT_TRUE_MESSAGE(r.ultimate_period_sec < 5.0f,
        "Ultimate period implausibly long (> 5 s)");

    // Ku must be positive and finite.
    TEST_ASSERT_TRUE(r.ultimate_gain > 0.0f);
    TEST_ASSERT_TRUE(isfinite(r.ultimate_gain));

    // Gains must be positive and finite.
    TEST_ASSERT_TRUE(r.kp > 0.0f);
    TEST_ASSERT_TRUE(r.ki > 0.0f);
    TEST_ASSERT_TRUE(r.kd > 0.0f);
    TEST_ASSERT_TRUE(isfinite(r.kp));
    TEST_ASSERT_TRUE(isfinite(r.ki));
    TEST_ASSERT_TRUE(isfinite(r.kd));

    // Coefficient ratios: Kp=0.6Ku, Ki=1.2Ku/Tu, Kd=0.075KuTu.
    const float Kp_expected = 0.6f  * r.ultimate_gain;
    const float Ki_expected = 1.2f  * r.ultimate_gain / r.ultimate_period_sec;
    const float Kd_expected = 0.075f * r.ultimate_gain * r.ultimate_period_sec;
    TEST_ASSERT_FLOAT_WITHIN(1.0e-4f, Kp_expected, r.kp);
    TEST_ASSERT_FLOAT_WITHIN(1.0e-4f, Ki_expected, r.ki);
    TEST_ASSERT_FLOAT_WITHIN(1.0e-4f, Kd_expected, r.kd);

    // failure_reason MUST be null on convergence.
    TEST_ASSERT_NULL(r.failure_reason);
}

// ---------------------------------------------------------------------------
// Test 2: max_duration_sec tripwire
// ---------------------------------------------------------------------------

void test_relay_aborts_on_safety_max_duration(void) {
    OscillatorPlant plant;

    RelayFeedbackStrategy strategy(1.0f, 0.01f);
    AutoPIDTuner tuner(strategy);

    SafetyLimits limits;
    limits.max_angle_deg    = 1.0e6f;
    limits.max_duration_sec = 0.1f;   // 100 ms cap — won't complete 4 cycles
    limits.abort_requested  = false;
    tuner.set_safety_limits(limits);

    PIDController pid(0.0f, 0.0f, 0.0f, -1.0f, 1.0f);

    const uint16_t dt_ms = 5;
    uint32_t now_ms      = 0;
    tuner.begin(pid, /*setpoint=*/0.0f, -1.0f, 1.0f, now_ms);

    plant.x_dot = 0.01f;

    const int max_steps = 1000;   // 5 s wall-clock — way more than 100 ms cap
    int steps_taken = 0;
    while (!tuner.is_done() && steps_taken < max_steps) {
        const float u = tuner.step(plant.x, now_ms);
        plant.step(u, dt_ms / 1000.0f);
        now_ms     += dt_ms;
        steps_taken++;
    }

    TEST_ASSERT_TRUE(tuner.is_done());
    TEST_ASSERT_FALSE_MESSAGE(tuner.succeeded(),
        "Tuner should have aborted on max_duration_sec");
    TEST_ASSERT_NOT_NULL(tuner.result().failure_reason);
    TEST_ASSERT_EQUAL_STRING("max_duration_exceeded",
                             tuner.result().failure_reason);
}

// ---------------------------------------------------------------------------
// Test 3: max_angle_deg tripwire
// ---------------------------------------------------------------------------

void test_relay_aborts_on_safety_max_angle(void) {
    OscillatorPlant plant;

    RelayFeedbackStrategy strategy(1.0f, 0.01f);
    AutoPIDTuner tuner(strategy);

    SafetyLimits limits;
    limits.max_angle_deg    = 0.001f;   // very tight — first decent swing trips
    limits.max_duration_sec = 30.0f;
    limits.abort_requested  = false;
    tuner.set_safety_limits(limits);

    PIDController pid(0.0f, 0.0f, 0.0f, -1.0f, 1.0f);

    const uint16_t dt_ms = 5;
    uint32_t now_ms      = 0;
    tuner.begin(pid, /*setpoint=*/0.0f, -1.0f, 1.0f, now_ms);

    plant.x_dot = 0.01f;

    const int max_steps = 4000;   // 20 s safety
    int steps_taken = 0;
    while (!tuner.is_done() && steps_taken < max_steps) {
        const float u = tuner.step(plant.x, now_ms);
        plant.step(u, dt_ms / 1000.0f);
        now_ms     += dt_ms;
        steps_taken++;
    }

    TEST_ASSERT_TRUE(tuner.is_done());
    TEST_ASSERT_FALSE_MESSAGE(tuner.succeeded(),
        "Tuner should have aborted on max_angle_deg");
    TEST_ASSERT_NOT_NULL(tuner.result().failure_reason);
    TEST_ASSERT_EQUAL_STRING("max_angle_exceeded",
                             tuner.result().failure_reason);
}

// ---------------------------------------------------------------------------
// Test 4: user-requested abort
// ---------------------------------------------------------------------------

void test_relay_aborts_on_user_request(void) {
    OscillatorPlant plant;

    RelayFeedbackStrategy strategy(1.0f, 0.01f);
    AutoPIDTuner tuner(strategy);

    SafetyLimits limits;
    limits.max_angle_deg    = 1.0e6f;
    limits.max_duration_sec = 30.0f;
    limits.abort_requested  = false;
    tuner.set_safety_limits(limits);

    PIDController pid(0.0f, 0.0f, 0.0f, -1.0f, 1.0f);

    const uint16_t dt_ms = 5;
    uint32_t now_ms      = 0;
    tuner.begin(pid, /*setpoint=*/0.0f, -1.0f, 1.0f, now_ms);

    plant.x_dot = 0.01f;

    // Run for 50 ms, then trigger abort.
    for (int i = 0; i < 10; ++i) {
        const float u = tuner.step(plant.x, now_ms);
        plant.step(u, dt_ms / 1000.0f);
        now_ms += dt_ms;
    }
    TEST_ASSERT_FALSE(tuner.is_done());

    tuner.request_abort();

    // Next step should observe the abort and finalize.
    const float u = tuner.step(plant.x, now_ms);
    (void)u;

    TEST_ASSERT_TRUE(tuner.is_done());
    TEST_ASSERT_FALSE(tuner.succeeded());
    TEST_ASSERT_NOT_NULL(tuner.result().failure_reason);
    TEST_ASSERT_EQUAL_STRING("user_aborted", tuner.result().failure_reason);
}

// ---------------------------------------------------------------------------
// Test 5: coordinator caches & restores original gains
// ---------------------------------------------------------------------------

void test_coordinator_caches_and_restores(void) {
    RelayFeedbackStrategy strategy(1.0f, 0.01f);
    AutoPIDTuner tuner(strategy);

    // PID starts with known gains.
    PIDController pid(7.0f, 3.0f, 1.5f, -1.0f, 1.0f);

    tuner.begin(pid, 0.0f, -1.0f, 1.0f, /*now_ms=*/0);

    // Mutate the PID gains as if the tuner had applied something. The
    // coordinator should still hold the pre-tune snapshot internally.
    pid.set_tunings(0.1f, 0.2f, 0.3f);

    tuner.restore_original(pid);

    float kp, ki, kd;
    pid.get_tunings(kp, ki, kd);
    TEST_ASSERT_FLOAT_WITHIN(1.0e-4f, 7.0f, kp);
    TEST_ASSERT_FLOAT_WITHIN(1.0e-4f, 3.0f, ki);
    TEST_ASSERT_FLOAT_WITHIN(1.0e-4f, 1.5f, kd);
}

// ---------------------------------------------------------------------------
// Test 6: coordinator applies tuned gains on success
// ---------------------------------------------------------------------------

void test_coordinator_applies_on_success(void) {
    OscillatorPlant plant;

    RelayFeedbackStrategy strategy(1.0f, 0.01f);
    AutoPIDTuner tuner(strategy);

    SafetyLimits limits;
    limits.max_angle_deg    = 1.0e6f;
    limits.max_duration_sec = 30.0f;
    limits.abort_requested  = false;
    tuner.set_safety_limits(limits);

    PIDController pid(0.0f, 0.0f, 0.0f, -1.0f, 1.0f);

    const uint16_t dt_ms = 5;
    uint32_t now_ms      = 0;
    tuner.begin(pid, /*setpoint=*/0.0f, -1.0f, 1.0f, now_ms);

    plant.x_dot = 0.01f;

    const int max_steps = (int)(20.0f / (dt_ms / 1000.0f));
    int steps_taken = 0;
    while (!tuner.is_done() && steps_taken < max_steps) {
        const float u = tuner.step(plant.x, now_ms);
        plant.step(u, dt_ms / 1000.0f);
        now_ms     += dt_ms;
        steps_taken++;
    }
    TEST_ASSERT_TRUE(tuner.succeeded());

    tuner.apply_to(pid);

    float kp, ki, kd;
    pid.get_tunings(kp, ki, kd);
    const TuningResult& r = tuner.result();
    TEST_ASSERT_FLOAT_WITHIN(1.0e-4f, r.kp, kp);
    TEST_ASSERT_FLOAT_WITHIN(1.0e-4f, r.ki, ki);
    TEST_ASSERT_FLOAT_WITHIN(1.0e-4f, r.kd, kd);
}

// ---------------------------------------------------------------------------
// Unity main
// ---------------------------------------------------------------------------

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_relay_converges_known_plant);
    RUN_TEST(test_relay_aborts_on_safety_max_duration);
    RUN_TEST(test_relay_aborts_on_safety_max_angle);
    RUN_TEST(test_relay_aborts_on_user_request);
    RUN_TEST(test_coordinator_caches_and_restores);
    RUN_TEST(test_coordinator_applies_on_success);
    return UNITY_END();
}
