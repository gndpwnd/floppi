/**
 * Native unit tests for the Workstream G (item G4) telemetry accessors on
 * BalanceApp — the read-through getters that back the 'g' serial telemetry
 * command (main.cpp).
 *
 * Getters under test (balance_app.h):
 *   get_pitch_setpoint_deg()            — always available; reads pid_.get_setpoint()
 *   get_wheel_velocity_mps(now_ms)      — USE_WHEEL_ENCODERS; mean tread velocity
 *   get_position_m()                    — USE_WHEEL_ENCODERS; PositionLoop drift
 *   get_pos_nudge_deg()                 — USE_WHEEL_ENCODERS; PositionLoop last nudge
 *   get_k_pos() / get_k_vel() / get_pos_leak()
 *                                       — USE_WHEEL_ENCODERS; outer-loop gains
 *
 * What this file pins (the *contract*, not PositionLoop's internals — those
 * are covered directly by tests/test_position_loop.cpp and
 * tests/test_position_gain_derivation.cpp):
 *   1. On a fresh fixture the gain getters expose the documented 4M.13
 *      *_FALLBACK seed values (the 4M.14 derivation has not run).
 *   2. RUN entry via enter_run_with_current_gains() calls position_loop_.reset()
 *      so get_position_m() / get_pos_nudge_deg() read zero at the start of a
 *      balance session.
 *   3. get_pitch_setpoint_deg() faithfully mirrors the live inner PID setpoint
 *      across a set_setpoint / step cycle.
 *   4. get_wheel_velocity_mps() returns 0 m/s with no encoder motion injected
 *      and the gain getters are stable (reset() does not clear gains) across a
 *      RUN step cycle.
 *
 * The fixture (mocks + BalanceApp construction) is copied verbatim from
 * tests/test_balance_app_encoder.cpp — no extra Arduino mocking is needed:
 * balance_app.h compiles natively in the g++ harness exactly as it does for
 * the existing encoder test.
 *
 * Compile (matches tests/test_balance_app_encoder.cpp):
 *
 *   g++ -std=c++11 -O2 -DUNIT_TEST -DNATIVE_TEST -DUSE_WHEEL_ENCODERS \
 *       -o tests/test_balance_telemetry \
 *       tests/test_balance_telemetry.cpp \
 *       src/applications/balancing_robot/balance_app.cpp \
 *       src/applications/balancing_robot/safety.cpp \
 *       src/control/pid_controller.cpp \
 *       src/control/auto_pid_tuner.cpp \
 *       src/control/plant_identifier.cpp \
 *       src/navigation/mounting_calibration.cpp \
 *       src/navigation/online_mounting_estimator.cpp \
 *       src/sensors/wheel_encoder.cpp \
 *       src/control/position_loop.cpp
 *   ./tests/test_balance_telemetry
 *
 * NATIVE_TEST switches WheelEncoder to its mock-tick backend (no Arduino
 * <Encoder.h>). USE_WHEEL_ENCODERS pulls the encoder + outer-loop telemetry
 * getters into BalanceApp.
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
#include "../src/control/position_loop.h"
#include "../src/navigation/mounting_calibration.h"
#include "../src/navigation/online_mounting_estimator.h"
#include "../src/sensors/sensor_base.h"
#include "../src/sensors/wheel_encoder.h"
#include "../src/actuators/motor_driver.h"

#ifndef USE_WHEEL_ENCODERS
#error "test_balance_telemetry.cpp requires -DUSE_WHEEL_ENCODERS"
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

#define TEST_NEAR(actual, expected, tol, msg)                                  \
    do {                                                                       \
        tests_run++;                                                           \
        double _a = (double)(actual);                                          \
        double _e = (double)(expected);                                        \
        if (fabs(_a - _e) <= (double)(tol)) {                                  \
            tests_passed++; printf("  PASS: %s\n", msg); }                     \
        else { tests_failed++;                                                 \
               printf("  FAIL (%s:%d): %s (expected %g, got %g)\n",            \
                      __FILE__, __LINE__, msg, _e, _a); }                      \
    } while (0)

// ============================================================================
// Mocks (mirrors tests/test_balance_app_encoder.cpp's lightweight mocks)
// ============================================================================

class GyroIMU : public OrientationSensor {
public:
    GyroIMU() : initialized_(true) {
        od_.pitch_deg = 0.0f; od_.roll_deg = 0.0f; od_.yaw_deg = 0.0f;
        od_.w = 1.0f; od_.x = 0.0f; od_.y = 0.0f; od_.z = 0.0f;
        gyro_y_ = 0.0f;
    }
    void set_pitch(float deg)   { od_.pitch_deg = deg; }
    void set_gyro_y(float dps)  { gyro_y_ = dps; }

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

private:
    OrientationData od_;
    bool initialized_;
    float gyro_y_;
};

class MockMotors : public DualMotorDriver {
public:
    MockMotors() : last_left_(0), last_right_(0) {}
    bool begin() override { return true; }
    void set_speeds(int16_t l, int16_t r) override { last_left_ = l; last_right_ = r; }
    void stop() override { last_left_ = 0; last_right_ = 0; }
    int16_t last_left() const override { return last_left_; }
    int16_t last_right() const override { return last_right_; }
private:
    int16_t last_left_, last_right_;
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

// ============================================================================
// Tests
// ============================================================================

// (1) Fresh-fixture gain getters expose the documented 4M.13 *_FALLBACK seeds.
// The 4M.14 derivation only runs at BOOTSTRAP finalise; a fixture that never
// bootstraps must read the conservative fallback gains straight from
// PositionLoop's constructor seed.
static void test_gain_getters_report_fallback_seed() {
    printf("\nTest 1: gain getters report 4M.13 *_FALLBACK seed values\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);

    TEST_NEAR(f.app.get_k_pos(),    POSLOOP_K_POS_FALLBACK,    1e-6f,
              "get_k_pos() == POSLOOP_K_POS_FALLBACK");
    TEST_NEAR(f.app.get_k_vel(),    POSLOOP_K_VEL_FALLBACK,    1e-6f,
              "get_k_vel() == POSLOOP_K_VEL_FALLBACK");
    TEST_NEAR(f.app.get_pos_leak(), POSLOOP_POS_LEAK_FALLBACK, 1e-6f,
              "get_pos_leak() == POSLOOP_POS_LEAK_FALLBACK");
    // pos_leak must be a valid leak (strictly inside (0,1)).
    TEST_ASSERT(f.app.get_pos_leak() > 0.0f && f.app.get_pos_leak() < 1.0f,
                "get_pos_leak() is a valid leak in (0,1)");
}

// (2) RUN entry resets the PositionLoop integrator + slew memory, so the drift
// and last-nudge getters read zero at the start of a balance session.
static void test_run_entry_zeroes_position_telemetry() {
    printf("\nTest 2: enter_run resets get_position_m()/get_pos_nudge_deg()\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.app.enter_run_with_current_gains(1000);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::RUN, "fixture entered RUN");

    TEST_NEAR(f.app.get_position_m(),   0.0f, 1e-6f,
              "get_position_m() == 0 right after RUN entry");
    TEST_NEAR(f.app.get_pos_nudge_deg(), 0.0f, 1e-6f,
              "get_pos_nudge_deg() == 0 right after RUN entry");
}

// (3) get_pitch_setpoint_deg() faithfully mirrors the live inner PID setpoint.
// Driving the PID setpoint directly and reading it back through the BalanceApp
// getter pins that the accessor is a true read-through (no caching, no stale
// copy) of pid_.get_setpoint().
static void test_pitch_setpoint_getter_mirrors_pid() {
    printf("\nTest 3: get_pitch_setpoint_deg() mirrors pid_.get_setpoint()\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);

    // Default setpoint is the upright target (0 deg) until the outer loop
    // nudges it.
    TEST_NEAR(f.app.get_pitch_setpoint_deg(), f.pid.get_setpoint(), 1e-6f,
              "getter == pid setpoint at construction");

    f.pid.set_setpoint(1.25f);
    TEST_NEAR(f.app.get_pitch_setpoint_deg(), 1.25f, 1e-6f,
              "getter tracks an explicit set_setpoint(1.25)");

    f.pid.set_setpoint(-0.5f);
    TEST_NEAR(f.app.get_pitch_setpoint_deg(), -0.5f, 1e-6f,
              "getter tracks an explicit set_setpoint(-0.5)");
}

// (4) get_wheel_velocity_mps() reads 0 m/s with no encoder motion injected,
// and the gain getters stay stable across a RUN step cycle — reset() clears
// only the integrator + slew memory, never the gains (PositionLoop contract).
static void test_velocity_zero_and_gains_stable_through_run() {
    printf("\nTest 4: get_wheel_velocity_mps()==0, gains stable through RUN\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);

    // No ticks injected (mock encoder backend) → both wheels read 0 m/s.
    // Prime the windowed sample, then advance past the velocity window.
    (void)f.app.get_wheel_velocity_mps(1000);
    TEST_NEAR(f.app.get_wheel_velocity_mps(1000 + 200), 0.0f, 1e-6f,
              "get_wheel_velocity_mps() == 0 with no encoder motion");

    const float k_pos_before  = f.app.get_k_pos();
    const float k_vel_before  = f.app.get_k_vel();
    const float leak_before   = f.app.get_pos_leak();

    // Run a clean balance session at pitch 0 — outer loop sees 0 m/s, so the
    // drift estimate and nudge must stay pinned at zero, and the gains must be
    // untouched by the RUN-entry reset().
    f.app.enter_run_with_current_gains(1000);
    f.imu.set_pitch(0.0f);
    f.imu.set_gyro_y(0.0f);
    for (int i = 0; i < 40; ++i) {
        f.app.step(1000u + (uint32_t)(i + 1) * 5u);
    }

    TEST_NEAR(f.app.get_k_pos(),    k_pos_before, 1e-6f,
              "get_k_pos() unchanged across RUN (reset() keeps gains)");
    TEST_NEAR(f.app.get_k_vel(),    k_vel_before, 1e-6f,
              "get_k_vel() unchanged across RUN (reset() keeps gains)");
    TEST_NEAR(f.app.get_pos_leak(), leak_before,  1e-6f,
              "get_pos_leak() unchanged across RUN (reset() keeps gains)");
    TEST_NEAR(f.app.get_position_m(),    0.0f, 1e-4f,
              "get_position_m() stays ~0 with no wheel motion");
    TEST_NEAR(f.app.get_pos_nudge_deg(), 0.0f, 1e-4f,
              "get_pos_nudge_deg() stays ~0 with no wheel motion");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    printf("======================================================\n");
    printf("BalanceApp Telemetry Accessor Tests (Workstream G / G4)\n");
    printf("======================================================\n");

    test_gain_getters_report_fallback_seed();
    test_run_entry_zeroes_position_telemetry();
    test_pitch_setpoint_getter_mirrors_pid();
    test_velocity_zero_and_gains_stable_through_run();

    printf("\n======================================================\n");
    printf("Results: %d run, %d passed, %d failed\n",
           tests_run, tests_passed, tests_failed);
    printf("======================================================\n");

    return (tests_failed == 0) ? 0 : 1;
}
