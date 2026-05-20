/**
 * Soft-cutoff tests for BalanceApp (Workstream 3 — native test hardening).
 *
 * Verifies the |pitch| > 25° soft-cutoff behaviour added in Phase 4.10's Item 4:
 *
 *   - Above the cutoff threshold, motors must be stopped (last_output == 0)
 *     even though the PID is still computing internally.
 *   - Below the threshold (and below the FALL_DETECTION limit, which is
 *     disabled by default per "balance forever" operator preference), motors
 *     are driven by the PID.
 *   - Cutoff is NOT a sticky state transition — when pitch drops back below
 *     the threshold, motors resume automatically (no state change).
 *   - PID I-term continues to accumulate during cutoff so it stays coherent
 *     with the OnlineMountingEstimator across the cutoff event.
 *
 * Source under test: src/applications/balancing_robot/balance_app.cpp,
 * step_run_() — see the "Item 4 — soft-cutoff" comment block (line ~422).
 *
 * Reuses the mock infrastructure pattern from tests/test_balance_app.cpp.
 *
 * Compile (from auto_orientation/):
 *
 *   g++ -std=c++11 -O2 -fpermissive -DUNIT_TEST \
 *       -o tests/test_balance_app_soft_cutoff \
 *       tests/test_balance_app_soft_cutoff.cpp \
 *       src/applications/balancing_robot/balance_app.cpp \
 *       src/applications/balancing_robot/safety.cpp \
 *       src/control/pid_controller.cpp \
 *       src/control/auto_pid_tuner.cpp \
 *       src/control/plant_identifier.cpp \
 *       src/navigation/mounting_calibration.cpp \
 *       src/navigation/online_mounting_estimator.cpp
 *   ./tests/test_balance_app_soft_cutoff
 *
 * (-fpermissive is required because src/applications/balancing_robot/balance_app.h
 * uses Arduino's F("...") string macro inside an inline template member that
 * the native test never instantiates — see the workaround note at the top of
 * tests/scenario_test_balance_closed_loop.cpp.)
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>

#include "../src/applications/balancing_robot/balance_app.h"
#include "../src/applications/balancing_robot/safety.h"
#include "../src/control/pid_controller.h"
#include "../src/control/auto_pid_tuner.h"
#include "../src/control/plant_identifier.h"
#include "../src/control/tuning_strategy.h"
#include "../src/navigation/mounting_calibration.h"
#include "../src/navigation/online_mounting_estimator.h"
#include "../src/sensors/sensor_base.h"
#include "../src/actuators/motor_driver.h"

// ============================================================================
// Tiny test harness (printf + counters; main returns 0 on success)
// ============================================================================

static int g_passes = 0;
static int g_fails  = 0;

#define TEST_ASSERT(cond, msg)                                                  \
    do {                                                                        \
        if (cond) { g_passes++; printf("  PASS: %s\n", msg); }                  \
        else      { g_fails++;  printf("  FAIL (%s:%d): %s\n",                  \
                                       __FILE__, __LINE__, msg); }              \
    } while (0)

// ============================================================================
// Mocks (slim copies — soft-cutoff tests don't need the full mock surface)
// ============================================================================

class MockIMU : public OrientationSensor {
public:
    MockIMU() : initialized_(true) {
        od_.pitch_deg = 0.0f;
        od_.roll_deg = 0.0f;
        od_.yaw_deg = 0.0f;
        od_.w = 1.0f; od_.x = 0.0f; od_.y = 0.0f; od_.z = 0.0f;
    }
    void set_pitch(float p) { od_.pitch_deg = p; }

    bool begin() override { initialized_ = true; return true; }
    void end() override { initialized_ = false; }
    bool isInitialized() const override { return initialized_; }
    bool read() override { return true; }
    bool hasNewData() const override { return true; }
    const char* name() const override { return "MockIMU"; }
    bool isHealthy() const override { return true; }
    const char* getStatusString() const override { return "OK"; }

    const OrientationData& getOrientation() const override { return od_; }
    bool setCalibrationProfile(const uint8_t*, uint16_t) override { return true; }
    bool getCalibrationProfile(uint8_t*, uint16_t*) override { return true; }
private:
    OrientationData od_;
    bool initialized_;
};

class MockMotors : public DualMotorDriver {
public:
    MockMotors() : last_left_(0), last_right_(0), stops_(0) {}
    bool begin() override { return true; }
    void set_speeds(int16_t l, int16_t r) override {
        last_left_ = l;
        last_right_ = r;
    }
    void stop() override {
        last_left_ = 0;
        last_right_ = 0;
        stops_++;
    }
    int16_t last_left() const override { return last_left_; }
    int16_t last_right() const override { return last_right_; }
    uint32_t stops() const { return stops_; }
private:
    int16_t last_left_;
    int16_t last_right_;
    uint32_t stops_;
};

// MockTuningStrategy — converges in 1 step so we can drop straight into RUN.
class MockTuningStrategy : public ITuningStrategy {
public:
    MockTuningStrategy() : done_(false), steps_(0) {
        result_.kp = 65.0f; result_.ki = 12.0f; result_.kd = 38.0f;
        result_.ultimate_gain = 0.0f;
        result_.ultimate_period_sec = 0.0f;
        result_.phase_margin_estimate_deg = 0.0f;
        result_.converged = true;
        result_.failure_reason = nullptr;
    }
    void begin(float, float, float, uint32_t) override {
        done_ = false; steps_ = 0;
    }
    float step(float, const SafetyLimits&, uint32_t) override {
        steps_++;
        if (steps_ >= 1) done_ = true;
        return 0.0f;
    }
    bool is_done() const override { return done_; }
    TuningResult get_result() const override { return result_; }
    const char* name() const override { return "MockTuning"; }
private:
    bool done_;
    int steps_;
    TuningResult result_;
};

// Fixture
struct Fixture {
    MockIMU                  imu;
    MockMotors               motors;
    PIDController            pid;
    MockTuningStrategy       strategy;
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

    // Drive the app into RUN starting from IDLE. Phase 4.10c moved
    // long_press(IDLE) to BOOTSTRAP; this test fixture only cares about
    // RUN-state behaviour, so use the direct entry helper that skips both
    // CAPTURE_MOUNTING and BOOTSTRAP. PID gains are already 0/0/0 from the
    // constructor, which is fine for soft-cutoff/HELD tests that don't depend
    // on PID output magnitude.
    void enter_run(uint32_t now_ms) {
        BalanceAppConfig cfg = BalanceApp::default_config();
        cfg.tilt_limit_deg = 90.0f;
        app.begin(cfg, now_ms);
        imu.set_pitch(0.0f);
        app.enter_run_with_current_gains(now_ms);
    }
};

// ============================================================================
// Tests
// ============================================================================

static void test_pitch_above_threshold_stops_motors() {
    printf("\nTest: pitch = 30° -> motors stopped (soft cutoff)\n");
    Fixture f;
    f.enter_run(1000);
    if (f.app.get_state() != BalanceAppState::RUN) {
        printf("  FAIL: setup did not reach RUN (got %s)\n", f.app.state_name());
        g_fails++;
        return;
    }

    // Pitch jumps to 30° — above the 25° soft-cutoff threshold.
    f.imu.set_pitch(30.0f);
    // Advance several ticks beyond the 5 s bootstrap freeze so the cutoff is
    // the only active gate. Default cfg.pid_sample_ms = 5.
    uint32_t t = 1010;
    for (int i = 0; i < 30; ++i) {
        f.app.step(t);
        t += 5;
    }
    TEST_ASSERT(f.app.get_state() == BalanceAppState::RUN,
                "still in RUN (cutoff is NOT a state transition)");
    TEST_ASSERT(f.app.get_last_output() == 0,
                "last_output == 0 at pitch=30° (soft cutoff active)");
    TEST_ASSERT(f.motors.last_left() == 0,
                "motors.left == 0 at pitch=30°");
    TEST_ASSERT(f.motors.last_right() == 0,
                "motors.right == 0 at pitch=30°");
}

static void test_pitch_below_threshold_drives_motors() {
    printf("\nTest: pitch = 20° -> motors NOT stopped (under soft cutoff)\n");
    Fixture f;
    f.enter_run(1000);
    if (f.app.get_state() != BalanceAppState::RUN) {
        printf("  FAIL: setup did not reach RUN\n");
        g_fails++;
        return;
    }

    // Hold pitch=20° for ~1.0 s — under the soft-cutoff threshold of 25°
    // but enough ticks for the PID to land a stable output. Kept under
    // STUCK_TIMEOUT_MS (1500 ms) so the STUCK detector (which fires when
    // motors saturate without any rotation, e.g. mock-IMU gyro=0) doesn't
    // bounce us out of RUN.
    uint32_t t = 1010;
    f.imu.set_pitch(20.0f);
    for (int i = 0; i < 200; ++i) {
        f.app.step(t);
        t += 5;
    }
    TEST_ASSERT(f.app.get_state() == BalanceAppState::RUN,
                "still in RUN");
    // Under cutoff: the PID at 20° error with Kp=50 produces output well past
    // the 255 PWM clamp. We don't assert a specific value (depends on plant
    // identifier adaptation), only that motors are NOT zeroed.
    const bool any_motor_drive =
        (f.motors.last_left() != 0) || (f.motors.last_right() != 0) ||
        (f.app.get_last_output() != 0);
    TEST_ASSERT(any_motor_drive,
                "motors driven (or last_output != 0) at pitch=20°");
}

static void test_pitch_recovers_motors_resume() {
    printf("\nTest: pitch 30° -> 5° -> motors resume (no sticky state)\n");
    Fixture f;
    f.enter_run(1000);
    if (f.app.get_state() != BalanceAppState::RUN) {
        printf("  FAIL: setup did not reach RUN\n");
        g_fails++;
        return;
    }

    // Stage 1: pitch = 30° for ~1.0 s. Motors must be zeroed by the soft
    // cutoff. Kept under STUCK_TIMEOUT_MS (1500 ms) — soft cutoff already
    // zeros motors so STUCK can't fire here anyway, but symmetry with the
    // sibling test keeps the structure obvious.
    uint32_t t = 1010;
    f.imu.set_pitch(30.0f);
    for (int i = 0; i < 200; ++i) {
        f.app.step(t);
        t += 5;
    }
    TEST_ASSERT(f.motors.last_left() == 0, "stage 1: motors stopped at 30°");
    TEST_ASSERT(f.app.get_state() == BalanceAppState::RUN,
                "stage 1: still in RUN despite cutoff");

    // Stage 2: pitch drops to 5° — well inside the linear region. The PID
    // should compute a non-zero command at the next tick (the I-term has
    // accumulated during cutoff, but even at I=0 the P-term at 5° with Kp~50
    // ~ 250 PWM).
    f.imu.set_pitch(5.0f);
    for (int i = 0; i < 5; ++i) {
        f.app.step(t);
        t += 5;
    }
    TEST_ASSERT(f.app.get_state() == BalanceAppState::RUN,
                "stage 2: still in RUN");
    const bool resumed =
        (f.motors.last_left() != 0) || (f.motors.last_right() != 0) ||
        (f.app.get_last_output() != 0);
    TEST_ASSERT(resumed,
                "stage 2: motors resumed automatically at pitch=5°");
}

static void test_pid_compute_continues_during_cutoff() {
    printf("\nTest: PID compute still ticks during soft cutoff (I-term moves)\n");
    Fixture f;
    f.enter_run(1000);
    if (f.app.get_state() != BalanceAppState::RUN) {
        printf("  FAIL: setup did not reach RUN\n");
        g_fails++;
        return;
    }

    // Drive past the bootstrap window first with the bot level.
    uint32_t t = 1010;
    f.imu.set_pitch(0.0f);
    for (int i = 0; i < 1100; ++i) {
        f.app.step(t);
        t += 5;
    }
    // Snapshot I-term BEFORE the cutoff event.
    const float i_term_before = f.pid.get_i_term();

    // Cutoff stage: pitch=30° for 100 ticks. Motors must be zero but I-term
    // must still evolve (the integrator gets fed every tick because step_run_
    // calls compute_with_rate() before the soft-cutoff branch).
    f.imu.set_pitch(30.0f);
    for (int i = 0; i < 100; ++i) {
        f.app.step(t);
        t += 5;
    }
    const float i_term_after = f.pid.get_i_term();

    // Motors must be zero throughout.
    TEST_ASSERT(f.motors.last_left() == 0, "motors zero during cutoff");
    TEST_ASSERT(f.app.get_last_output() == 0, "last_output zero during cutoff");

    // I-term must have advanced (this proves compute() is still being called).
    // With pitch=30° and ki=2, the integrator accumulates 30*0.005*2 = 0.3 PWM
    // per tick, so over 100 ticks we should see >> 0.01 PWM of movement (the
    // anti-windup clamp may saturate it, but it must NOT equal the pre-snapshot).
    const float delta = std::fabs(i_term_after - i_term_before);
    printf("  i_term: before=%.3f after=%.3f delta=%.3f\n",
           i_term_before, i_term_after, delta);
    TEST_ASSERT(delta > 1e-3f, "PID I-term advanced during cutoff");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    printf("====================================================\n");
    printf("BalanceApp soft-cutoff tests (Workstream 3)\n");
    printf("====================================================\n");

    test_pitch_above_threshold_stops_motors();
    test_pitch_below_threshold_drives_motors();
    test_pitch_recovers_motors_resume();
    test_pid_compute_continues_during_cutoff();

    printf("\n====================================================\n");
    printf("Results: %d passed, %d failed\n", g_passes, g_fails);
    printf("====================================================\n");
    return (g_fails == 0) ? 0 : 1;
}
