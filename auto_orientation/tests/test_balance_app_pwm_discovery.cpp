/**
 * Native unit tests for the Phase 4M.12 PWM range auto-discovery state in
 * BalanceApp (MEGA_UNIVERSAL_PLAN.md §7d).
 *
 * Covers the observable contract of the PWM_DISCOVERY state:
 *
 *   1. Entry guard      — enter_pwm_discovery() is a no-op outside IDLE.
 *   2. State transition — IDLE → PWM_DISCOVERY on call (from IDLE).
 *   3. Motors stopped on entry; last_output_ == 0 right after the call.
 *   4. Result struct is zeroed on entry so a re-run never sees stale values.
 *   5. Pre-discovery inspector accessors return 0 (discovered=false).
 *   6. Timeout path  — encoders stuck at 0 ticks (no motion) → state returns
 *                       to IDLE after PWM_DISC_TIMEOUT_MS with
 *                       failure_reason=8 (pwm_discovery_timeout) and
 *                       discovered=false.
 *   7. Abort path    — safety.request_abort() mid-sweep returns to IDLE with
 *                       failure_reason=4 (user_abort) and discovered=false.
 *   8. Inspectors after failure — get_discovered_min_pwm() /
 *                       get_discovered_max_pwm() return 0 (the inspectors gate
 *                       on `discovered` and stay safe even if the result
 *                       struct has nonzero intermediate values).
 *   9. PWM_DISCOVERY rejected from non-IDLE — enter_pwm_discovery() called
 *                       from RUN is a no-op (state stays RUN, no PWMD entry).
 *  10. Collision during discovery does NOT abort — the as-built implementation
 *                       intentionally only checks safety_.abort_requested()
 *                       and elapsed-time, NOT the collision latch (see
 *                       step_pwm_discovery_ in balance_app.cpp:1740-1855).
 *                       This test documents the current behaviour so a future
 *                       collision-abort change is caught.
 *
 * What is NOT covered (limitations):
 *   - Synthetic encoder-tick injection to drive the algorithm to a clean
 *     MIN/MAX lock. The WheelEncoder members inside BalanceApp are private
 *     value members; the test fixture has no friend-equivalent path to call
 *     set_ticks_for_test() on them. Direct driver behaviour (velocity
 *     forward-difference, stall, plateau math) is unit-tested separately in
 *     tests/test_wheel_encoder.cpp; the convergence-from-synthetic-plant test
 *     belongs in a bench/scenario harness, not a host unit test.
 *
 * Compile (matches the style of tests/test_balance_app_encoder.cpp):
 *
 *   g++ -std=c++11 -O2 -DUNIT_TEST -DNATIVE_TEST -DUSE_WHEEL_ENCODERS \
 *       -fpermissive \
 *       -o tests/test_balance_app_pwm_discovery \
 *       tests/test_balance_app_pwm_discovery.cpp \
 *       src/applications/balancing_robot/balance_app.cpp \
 *       src/applications/balancing_robot/safety.cpp \
 *       src/control/pid_controller.cpp \
 *       src/control/auto_pid_tuner.cpp \
 *       src/control/plant_identifier.cpp \
 *       src/navigation/mounting_calibration.cpp \
 *       src/navigation/online_mounting_estimator.cpp \
 *       src/sensors/wheel_encoder.cpp
 *   ./tests/test_balance_app_pwm_discovery
 *
 * -fpermissive is needed because balance_app.h's template drain_state_log() /
 * drain_pulse_log() use Arduino's F() macro for PROGMEM strings. F() is not
 * declared on host builds; without -fpermissive g++ flags the implicit
 * "argument-dependent lookup" name. The same flag is used by the encoder
 * test for the same reason.
 *
 * NATIVE_TEST switches WheelEncoder to its mock-tick backend (no Arduino
 * <Encoder.h>). USE_WHEEL_ENCODERS pulls the PWM_DISCOVERY hooks into
 * BalanceApp.
 */

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include "../src/applications/balancing_robot/balance_app.h"
#include "../src/applications/balancing_robot/safety.h"
#include "../src/control/pid_controller.h"
#include "../src/control/auto_pid_tuner.h"
#include "../src/control/plant_identifier.h"
#include "../src/control/tuning_strategy.h"
#include "../src/navigation/mounting_calibration.h"
#include "../src/navigation/online_mounting_estimator.h"
#include "../src/sensors/sensor_base.h"
#include "../src/sensors/wheel_encoder.h"
#include "../src/actuators/motor_driver.h"

#ifndef USE_WHEEL_ENCODERS
#error "test_balance_app_pwm_discovery.cpp requires -DUSE_WHEEL_ENCODERS"
#endif

// ============================================================================
// Test infrastructure
// ============================================================================

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg)                                                 \
    do {                                                                       \
        tests_run++;                                                           \
        if (cond) { tests_passed++; printf("  PASS: %s\n", msg); }             \
        else { tests_failed++; printf("  FAIL (%s:%d): %s\n",                  \
                                      __FILE__, __LINE__, msg); }              \
    } while (0)

#define TEST_EQ_INT(actual, expected, msg)                                     \
    do {                                                                       \
        tests_run++;                                                           \
        long _a = (long)(actual);                                              \
        long _e = (long)(expected);                                            \
        if (_a == _e) { tests_passed++; printf("  PASS: %s\n", msg); }         \
        else { tests_failed++;                                                 \
               printf("  FAIL (%s:%d): %s (expected %ld, got %ld)\n",          \
                      __FILE__, __LINE__, msg, _e, _a); }                      \
    } while (0)

// ============================================================================
// Mocks (mirrors test_balance_app_encoder.cpp's lightweight mocks)
// ============================================================================

class GyroIMU : public OrientationSensor {
public:
    GyroIMU() : initialized_(true), have_la_(false) {
        od_.pitch_deg = 0.0f; od_.roll_deg = 0.0f; od_.yaw_deg = 0.0f;
        od_.w = 1.0f; od_.x = 0.0f; od_.y = 0.0f; od_.z = 0.0f;
        gyro_y_ = 0.0f;
        la_[0] = 0.0f; la_[1] = 0.0f; la_[2] = 0.0f;
    }
    void set_pitch(float deg)        { od_.pitch_deg = deg; }
    void set_gyro_y(float dps)       { gyro_y_ = dps; }
    void set_linear_accel_mag(float m) {
        la_[0] = m; la_[1] = 0.0f; la_[2] = 0.0f; have_la_ = true;
    }

    bool begin() override { initialized_ = true; return true; }
    void end() override   { initialized_ = false; }
    bool isInitialized() const override { return initialized_; }
    bool read() override  { return true; }
    bool hasNewData() const override { return true; }
    const char* name() const override { return "GyroIMU"; }
    bool isHealthy() const override { return true; }
    const char* getStatusString() const override { return "OK"; }

    const OrientationData& getOrientation() const override { return od_; }
    bool setCalibrationProfile(const uint8_t*, uint16_t) override { return true; }
    bool getCalibrationProfile(uint8_t*, uint16_t*) override { return true; }
    bool getRawGyro(float xyz[3]) override {
        xyz[0] = 0.0f; xyz[1] = gyro_y_; xyz[2] = 0.0f; return true;
    }
    bool getRawAccel(float xyz[3]) override {
        xyz[0] = 0.0f; xyz[1] = 0.0f; xyz[2] = 9.81f; return true;
    }
    bool getLinearAccel(float xyz[3]) override {
        if (!have_la_) return false;
        xyz[0] = la_[0]; xyz[1] = la_[1]; xyz[2] = la_[2]; return true;
    }

private:
    OrientationData od_;
    bool  initialized_;
    bool  have_la_;
    float gyro_y_;
    float la_[3];
};

class MockMotors : public DualMotorDriver {
public:
    MockMotors() : last_left_(0), last_right_(0), peak_left_(0), peak_right_(0) {}
    bool begin() override { return true; }
    void set_speeds(int16_t l, int16_t r) override {
        last_left_ = l; last_right_ = r;
        int16_t la = l < 0 ? -l : l;
        int16_t ra = r < 0 ? -r : r;
        if (la > peak_left_)  peak_left_  = la;
        if (ra > peak_right_) peak_right_ = ra;
    }
    void stop() override { last_left_ = 0; last_right_ = 0; }
    int16_t last_left() const override { return last_left_; }
    int16_t last_right() const override { return last_right_; }
    int16_t peak_left() const { return peak_left_; }
    int16_t peak_right() const { return peak_right_; }
    void reset_peaks() { peak_left_ = 0; peak_right_ = 0; }
private:
    int16_t last_left_, last_right_;
    int16_t peak_left_, peak_right_;
};

class NoOpStrategy : public ITuningStrategy {
public:
    void begin(float, float, float, uint32_t) override {}
    float step(float, const SafetyLimits&, uint32_t) override { return 0.0f; }
    bool is_done() const override { return true; }
    TuningResult get_result() const override {
        TuningResult r{}; r.failure_reason = "noop"; return r;
    }
    const char* name() const override { return "noop"; }
};

struct Fixture {
    GyroIMU                  imu;
    MockMotors               motors;
    PIDController            pid;
    NoOpStrategy             strategy;
    AutoPIDTuner             tuner;
    MountingCalibration      mounting;
    OnlineMountingEstimator  online_est;
    BalanceSafety            safety;
    PlantIdentifier          plant_id;
    BalanceApp               app;

    Fixture()
        : pid(50.0f, 2.0f, 20.0f, -255.0f, 255.0f),
          tuner(strategy),
          app(imu, motors, pid, tuner, mounting, online_est, safety, plant_id) {}
};

// Convenience — run the app for n ticks at 5 ms cadence starting now+5ms.
static uint32_t advance_ticks(Fixture& f, uint32_t start_ms, int n_ticks) {
    uint32_t t = start_ms;
    for (int i = 0; i < n_ticks; ++i) {
        t = start_ms + (uint32_t)(i + 1) * 5u;
        f.app.step(t);
    }
    return t;
}

// ============================================================================
// Tests
// ============================================================================

// (1) Entry transition — IDLE → PWM_DISCOVERY on enter_pwm_discovery().
static void test_idle_to_pwm_discovery() {
    printf("\nTest 1: IDLE → PWM_DISCOVERY transition\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::IDLE,
                "fixture starts in IDLE");

    f.app.enter_pwm_discovery(1000);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::PWM_DISCOVERY,
                "after enter_pwm_discovery from IDLE → PWM_DISCOVERY");
}

// (2) Pre-run inspectors return 0 — get_pwm_discovery_result() is zero-valued
// before the first sweep, and the discovered_min/max convenience inspectors
// return 0 because `discovered` is false.
static void test_pre_run_inspectors_are_zero() {
    printf("\nTest 2: pre-discovery inspectors return zero\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);

    const PwmDiscoveryResult& r = f.app.get_pwm_discovery_result();
    TEST_EQ_INT(r.discovered,             0, "result.discovered == false");
    TEST_EQ_INT(r.discovered_min_pwm,     0, "result.discovered_min_pwm == 0");
    TEST_EQ_INT(r.discovered_max_pwm,     0, "result.discovered_max_pwm == 0");
    TEST_EQ_INT(r.steps_attempted,        0, "result.steps_attempted == 0");
    TEST_EQ_INT(r.failure_reason,         0, "result.failure_reason == 0");
    TEST_EQ_INT(f.app.get_discovered_min_pwm(), 0, "inspector min == 0");
    TEST_EQ_INT(f.app.get_discovered_max_pwm(), 0, "inspector max == 0");
}

// (3) Motors stopped + last_output_ cleared right after enter_pwm_discovery().
// enter_state_(PWM_DISCOVERY) explicitly calls motors_.stop() and sets
// last_output_=0 (balance_app.cpp:1024-1025).
static void test_motors_stopped_on_entry() {
    printf("\nTest 3: motors stopped + last_output cleared on PWMD entry\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);

    // Dirty the motors first so we can prove enter_pwm_discovery wipes them.
    f.motors.set_speeds(123, -45);
    TEST_EQ_INT(f.motors.last_left(),  123, "pre-entry: motors dirty (sanity)");

    f.app.enter_pwm_discovery(1000);
    TEST_EQ_INT(f.motors.last_left(),  0, "left motor stopped on PWMD entry");
    TEST_EQ_INT(f.motors.last_right(), 0, "right motor stopped on PWMD entry");
    TEST_EQ_INT(f.app.get_last_output(), 0, "last_output_ cleared on PWMD entry");
}

// (4) enter_pwm_discovery() from non-IDLE is a no-op. We force the app into RUN
// via enter_run_with_current_gains() and verify the call is silently ignored.
static void test_enter_from_run_is_noop() {
    printf("\nTest 4: enter_pwm_discovery() from RUN is a no-op\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.app.enter_run_with_current_gains(1000);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::RUN,
                "fixture entered RUN as precondition");

    f.app.enter_pwm_discovery(1100);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::RUN,
                "state stays in RUN — enter_pwm_discovery was a no-op");
    TEST_ASSERT(f.app.get_state() != BalanceAppState::PWM_DISCOVERY,
                "did NOT transition to PWM_DISCOVERY from RUN");
}

// (5) Timeout path — encoders never produce motion (mock ticks stay 0); after
// PWM_DISC_TIMEOUT_MS the discovery exits to IDLE with failure_reason=8
// (pwm_discovery_timeout) and discovered=false.
//
// The mock encoders inside BalanceApp report 0 ticks throughout, so velocity
// stays 0 across every step → MIN never locks → no plateau lock either → the
// elapsed-time check inside step_pwm_discovery_ trips first.
static void test_timeout_path_no_motion() {
    printf("\nTest 5: timeout path — no encoder motion → reason=8, IDLE\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.app.enter_pwm_discovery(1000);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::PWM_DISCOVERY,
                "entered PWM_DISCOVERY");

    // PWM_DISC_TIMEOUT_MS = 8000. Step at 5 ms cadence for slightly more than
    // the timeout (1800 ticks × 5 ms = 9000 ms). The exit transition happens
    // inside one of these ticks.
    bool exited_before_timeout = false;
    BalanceAppState final_state = f.app.get_state();
    for (int i = 0; i < 1800; ++i) {
        f.app.step(1000u + (uint32_t)(i + 1) * 5u);
        if (f.app.get_state() != BalanceAppState::PWM_DISCOVERY) {
            final_state = f.app.get_state();
            // Sanity: shouldn't exit before ~PWM_DISC_TIMEOUT_MS unless
            // PWM ran off the top of 255 first (~10.2 s without encoder
            // response, so the timeout fires first within 8 s).
            if ((uint32_t)i * 5u < 7900u) exited_before_timeout = true;
            break;
        }
    }
    TEST_ASSERT(!exited_before_timeout,
                "did not exit before PWM_DISC_TIMEOUT_MS");
    TEST_ASSERT(final_state == BalanceAppState::IDLE,
                "after timeout → IDLE");

    const PwmDiscoveryResult& r = f.app.get_pwm_discovery_result();
    TEST_EQ_INT(r.discovered, 0,
                "result.discovered == false on timeout");
    TEST_EQ_INT(r.failure_reason, 8,
                "result.failure_reason == 8 (pwm_discovery_timeout)");
    TEST_ASSERT(r.steps_attempted > 0,
                "steps_attempted > 0 (the ramp ran for at least one step)");

    TEST_EQ_INT(f.app.get_discovered_min_pwm(), 0,
                "get_discovered_min_pwm() == 0 after failed discovery");
    TEST_EQ_INT(f.app.get_discovered_max_pwm(), 0,
                "get_discovered_max_pwm() == 0 after failed discovery");
}

// (6) Abort path — safety_.request_abort() during PWM_DISCOVERY returns to
// IDLE with failure_reason=4 (user_abort) and discovered=false.
static void test_abort_path() {
    printf("\nTest 6: abort path — request_abort() → reason=4, IDLE\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.app.enter_pwm_discovery(1000);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::PWM_DISCOVERY,
                "entered PWM_DISCOVERY");

    // Run a few ticks to advance partway through the sweep, then abort.
    for (int i = 0; i < 50; ++i) {
        f.app.step(1000u + (uint32_t)(i + 1) * 5u);
    }
    TEST_ASSERT(f.app.get_state() == BalanceAppState::PWM_DISCOVERY,
                "still in PWM_DISCOVERY mid-sweep");

    f.safety.request_abort();
    // One more tick to let step_pwm_discovery_ pick up the abort.
    f.app.step(1000u + 51u * 5u);

    TEST_ASSERT(f.app.get_state() == BalanceAppState::IDLE,
                "abort → IDLE");
    const PwmDiscoveryResult& r = f.app.get_pwm_discovery_result();
    TEST_EQ_INT(r.failure_reason, 4,
                "result.failure_reason == 4 (user_abort)");
    TEST_EQ_INT(r.discovered, 0,
                "result.discovered == false on abort");
    // Motors must end stopped.
    TEST_EQ_INT(f.motors.last_left(),  0, "motors stopped after abort");
    TEST_EQ_INT(f.motors.last_right(), 0, "motors stopped after abort");
}

// (7) Result struct is zeroed on every PWM_DISCOVERY entry — a re-run cannot
// see stale values from the previous timeout/abort outcome.
//
// Sequence: run discovery to timeout (fills failure_reason=8) → IDLE →
// re-enter PWM_DISCOVERY → inspect the struct → all fields are 0 again at
// entry, before any tick has updated them.
static void test_result_struct_resets_on_reentry() {
    printf("\nTest 7: result struct is zeroed on re-entry\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);

    // First run — timeout to populate failure_reason=8.
    f.app.enter_pwm_discovery(1000);
    for (int i = 0; i < 1800; ++i) {
        f.app.step(1000u + (uint32_t)(i + 1) * 5u);
        if (f.app.get_state() != BalanceAppState::PWM_DISCOVERY) break;
    }
    TEST_ASSERT(f.app.get_state() == BalanceAppState::IDLE,
                "first run ended in IDLE");
    TEST_EQ_INT(f.app.get_pwm_discovery_result().failure_reason, 8,
                "first run yielded reason=8 (sanity)");

    // Second run — enter and immediately read the result struct.
    uint32_t t2 = 1000u + 1800u * 5u + 100u;
    f.app.enter_pwm_discovery(t2);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::PWM_DISCOVERY,
                "re-entered PWM_DISCOVERY");

    const PwmDiscoveryResult& r = f.app.get_pwm_discovery_result();
    TEST_EQ_INT(r.failure_reason,    0,
                "failure_reason cleared on re-entry");
    TEST_EQ_INT(r.discovered,        0, "discovered cleared on re-entry");
    TEST_EQ_INT(r.steps_attempted,   0, "steps_attempted cleared on re-entry");
    TEST_EQ_INT(r.discovered_min_pwm, 0,
                "discovered_min_pwm cleared on re-entry");
    TEST_EQ_INT(r.discovered_max_pwm, 0,
                "discovered_max_pwm cleared on re-entry");
}

// (8) Inspectors clamp to 0 when discovered=false — even if the result struct
// had intermediate values written before MAX lock, the convenience inspectors
// gate on the `discovered` flag so callers never see partial bounds.
//
// We can't directly inject a "min locked but max not" intermediate state, but
// after a timeout the struct has steps_attempted > 0 and possibly a partial
// min/max could have been written. The inspectors must still return 0.
static void test_inspectors_gated_on_discovered_flag() {
    printf("\nTest 8: inspectors return 0 when discovered=false\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);

    f.app.enter_pwm_discovery(1000);
    for (int i = 0; i < 1800; ++i) {
        f.app.step(1000u + (uint32_t)(i + 1) * 5u);
        if (f.app.get_state() != BalanceAppState::PWM_DISCOVERY) break;
    }
    const PwmDiscoveryResult& r = f.app.get_pwm_discovery_result();
    TEST_EQ_INT(r.discovered, 0, "discovered=false after timeout (sanity)");
    // Inspectors must clamp to 0 regardless of any partial writes inside
    // discovered_min_pwm / discovered_max_pwm (in the all-zero-velocity case
    // both are 0 anyway, but the inspector guard is the contract we pin).
    TEST_EQ_INT(f.app.get_discovered_min_pwm(), 0,
                "min inspector → 0 when discovered=false");
    TEST_EQ_INT(f.app.get_discovered_max_pwm(), 0,
                "max inspector → 0 when discovered=false");
}

// (9) Collision during PWM_DISCOVERY does NOT abort. The as-built
// step_pwm_discovery_ only checks safety_.abort_requested() and elapsed-time;
// it does NOT consult collision_latched_. This test documents the current
// behaviour so a future change is caught (an intentional design decision is a
// regression-detector, not a bug).
//
// Sequence: enter PWM_DISCOVERY, set a sustained linear-accel spike that
// would normally latch collision_detected, step a few ticks, observe that
// state is still PWM_DISCOVERY (i.e. did NOT abort to IDLE).
static void test_collision_does_not_abort_discovery() {
    printf("\nTest 9: collision during PWMD does NOT abort (as-built)\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.app.enter_pwm_discovery(1000);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::PWM_DISCOVERY,
                "entered PWM_DISCOVERY");

    // Spike LIA well above COLLISION_PEAK_MPS2 (12) so the latch fires on
    // the very first read_imu_().
    f.imu.set_linear_accel_mag(15.0f);

    // Step a handful of ticks — enough that collision_latched_ is observed.
    for (int i = 0; i < 5; ++i) {
        f.app.step(1000u + (uint32_t)(i + 1) * 5u);
    }
    TEST_ASSERT(f.app.collision_detected(),
                "collision latch fires during PWMD (sanity)");
    TEST_ASSERT(f.app.get_state() == BalanceAppState::PWM_DISCOVERY,
                "PWMD state UNCHANGED by collision (as-built; spec gap)");
}

// (10) After PWM_DISCOVERY exits (any reason) the state must be IDLE — never
// HELD, FALLEN, RUN, etc. The single-exit finalize in step_pwm_discovery_
// always calls enter_state_(IDLE) so this is the structural contract.
static void test_pwm_discovery_always_exits_to_idle() {
    printf("\nTest 10: PWM_DISCOVERY always exits to IDLE\n");

    // Path A — timeout.
    {
        Fixture f;
        f.app.begin(BalanceApp::default_config(), 1000);
        f.app.enter_pwm_discovery(1000);
        for (int i = 0; i < 1800; ++i) {
            f.app.step(1000u + (uint32_t)(i + 1) * 5u);
            if (f.app.get_state() != BalanceAppState::PWM_DISCOVERY) break;
        }
        TEST_ASSERT(f.app.get_state() == BalanceAppState::IDLE,
                    "Path A (timeout): exits to IDLE");
    }
    // Path B — abort.
    {
        Fixture f;
        f.app.begin(BalanceApp::default_config(), 1000);
        f.app.enter_pwm_discovery(1000);
        for (int i = 0; i < 20; ++i) {
            f.app.step(1000u + (uint32_t)(i + 1) * 5u);
        }
        f.safety.request_abort();
        f.app.step(1000u + 21u * 5u);
        TEST_ASSERT(f.app.get_state() == BalanceAppState::IDLE,
                    "Path B (abort): exits to IDLE");
    }
}

// ============================================================================
// Main
// ============================================================================

int main() {
    printf("======================================================\n");
    printf("BalanceApp PWM Discovery Tests (Phase 4M.12)\n");
    printf("======================================================\n");

    test_idle_to_pwm_discovery();
    test_pre_run_inspectors_are_zero();
    test_motors_stopped_on_entry();
    test_enter_from_run_is_noop();
    test_timeout_path_no_motion();
    test_abort_path();
    test_result_struct_resets_on_reentry();
    test_inspectors_gated_on_discovered_flag();
    test_collision_does_not_abort_discovery();
    test_pwm_discovery_always_exits_to_idle();

    printf("\n======================================================\n");
    printf("Results: %d run, %d passed, %d failed\n",
           tests_run, tests_passed, tests_failed);
    printf("======================================================\n");

    return (tests_failed == 0) ? 0 : 1;
}
