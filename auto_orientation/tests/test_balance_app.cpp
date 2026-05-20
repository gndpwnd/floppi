/**
 * Native Unit Tests for BalanceApp (Phase 4.7a)
 *
 * Verifies the top-level state machine that orchestrates mounting capture,
 * auto-PID tuning, balance-loop run, and safe-fall handling.
 *
 * Mocking strategy:
 *   - MockIMU implements OrientationSensor. Tests poke pitch_deg via
 *     set_pitch() between step() calls.
 *   - MockMotors implements DualMotorDriver. Captures last_left/right.
 *   - MockTuningStrategy implements ITuningStrategy. Tests script the
 *     outcome (succeeded / failed / converged-after-N-steps).
 *   - The real AutoPIDTuner, PIDController, MountingCalibration,
 *     OnlineMountingEstimator, and BalanceSafety are used unmodified — we
 *     only swap out the *hardware-facing* boundaries.
 *
 * Test runner style follows tests/test_l298n_motor.cpp (printf + TEST_ASSERT,
 * tests_run / tests_passed / tests_failed counters). Returns 0 on all-pass.
 *
 * Compile (from repo root):
 *
 *   g++ -std=c++11 -O2 -DUNIT_TEST \
 *       -o tests/test_balance_app \
 *       tests/test_balance_app.cpp \
 *       src/applications/balancing_robot/balance_app.cpp \
 *       src/applications/balancing_robot/safety.cpp \
 *       src/control/pid_controller.cpp \
 *       src/control/auto_pid_tuner.cpp \
 *       src/navigation/mounting_calibration.cpp \
 *       src/navigation/online_mounting_estimator.cpp
 *   ./tests/test_balance_app
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
#include "../src/control/tuning_strategy.h"
#include "../src/navigation/mounting_calibration.h"
#include "../src/navigation/online_mounting_estimator.h"
#include "../src/sensors/sensor_base.h"
#include "../src/actuators/motor_driver.h"

// ============================================================================
// Test infrastructure
// ============================================================================

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message)                                       \
  do {                                                                        \
    tests_run++;                                                              \
    if (condition) {                                                          \
      tests_passed++;                                                         \
      printf("  PASS: %s\n", message);                                        \
    } else {                                                                  \
      tests_failed++;                                                         \
      printf("  FAIL (%s:%d): %s\n", __FILE__, __LINE__, message);            \
    }                                                                         \
  } while (0)

#define TEST_EQ_INT(actual, expected, message)                                \
  do {                                                                        \
    tests_run++;                                                              \
    long _a = static_cast<long>(actual);                                      \
    long _e = static_cast<long>(expected);                                    \
    if (_a == _e) {                                                           \
      tests_passed++;                                                         \
      printf("  PASS: %s\n", message);                                        \
    } else {                                                                  \
      tests_failed++;                                                         \
      printf("  FAIL (%s:%d): %s (expected %ld, got %ld)\n",                  \
             __FILE__, __LINE__, message, _e, _a);                            \
    }                                                                         \
  } while (0)

// ============================================================================
// Mocks
// ============================================================================

class MockIMU : public OrientationSensor {
public:
    MockIMU() : initialized_(true), reads_(0) {
        od_.pitch_deg = 0.0f;
        od_.roll_deg = 0.0f;
        od_.yaw_deg = 0.0f;
        od_.w = 1.0f; od_.x = 0.0f; od_.y = 0.0f; od_.z = 0.0f;
    }

    void set_pitch(float pitch_deg) { od_.pitch_deg = pitch_deg; }

    // Sensor
    bool begin() override { initialized_ = true; return true; }
    void end() override { initialized_ = false; }
    bool isInitialized() const override { return initialized_; }
    bool read() override { reads_++; return true; }
    bool hasNewData() const override { return true; }
    const char* name() const override { return "MockIMU"; }
    bool isHealthy() const override { return true; }
    const char* getStatusString() const override { return "OK"; }

    // OrientationSensor
    const OrientationData& getOrientation() const override { return od_; }
    bool setCalibrationProfile(const uint8_t*, uint16_t) override { return true; }
    bool getCalibrationProfile(uint8_t*, uint16_t*) override { return true; }

    uint32_t reads() const { return reads_; }

private:
    OrientationData od_;
    bool initialized_;
    uint32_t reads_;
};

class MockMotors : public DualMotorDriver {
public:
    MockMotors() : last_left_(0), last_right_(0), stops_(0), started_(false) {}

    bool begin() override { started_ = true; return true; }
    void set_speeds(int16_t left, int16_t right) override {
        last_left_ = left;
        last_right_ = right;
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
    bool started_;
};

/**
 * Configurable mock tuning strategy. The test scripts:
 *   - steps_until_done : how many step() calls until is_done() flips true
 *   - converged        : whether the final result reports success
 *   - output_per_step  : value returned from each step()
 *   - tuned_kp/ki/kd   : gains in the result struct on success
 */
class MockTuningStrategy : public ITuningStrategy {
public:
    MockTuningStrategy()
        : begun_(false),
          done_(false),
          steps_(0),
          steps_until_done_(3),
          output_per_step_(50.0f),
          last_measurement_(0.0f) {
        result_.kp = 0.0f;
        result_.ki = 0.0f;
        result_.kd = 0.0f;
        result_.ultimate_gain = 0.0f;
        result_.ultimate_period_sec = 0.0f;
        result_.phase_margin_estimate_deg = 0.0f;
        result_.converged = false;
        result_.failure_reason = "not_started";
        // Default: succeeds in 3 steps with placeholder gains
        set_success(80.0f, 5.0f, 12.0f);
    }

    void set_success(float kp, float ki, float kd) {
        result_.kp = kp; result_.ki = ki; result_.kd = kd;
        result_.converged = true;
        result_.failure_reason = nullptr;
    }
    void set_failure(const char* reason) {
        result_.kp = 0.0f; result_.ki = 0.0f; result_.kd = 0.0f;
        result_.converged = false;
        result_.failure_reason = reason ? reason : "mock_failure";
    }
    void set_steps_until_done(int n) { steps_until_done_ = n; }
    void set_output(float v) { output_per_step_ = v; }

    // ITuningStrategy
    void begin(float /*setpoint*/, float /*omin*/, float /*omax*/, uint32_t /*now_ms*/) override {
        begun_ = true;
        done_ = false;
        steps_ = 0;
    }
    float step(float measurement, const SafetyLimits& safety, uint32_t /*now_ms*/) override {
        last_measurement_ = measurement;
        if (safety.abort_requested) {
            done_ = true;
            set_failure("user_aborted");
            return 0.0f;
        }
        steps_++;
        if (steps_ >= steps_until_done_) {
            done_ = true;
        }
        return output_per_step_;
    }
    bool is_done() const override { return done_; }
    TuningResult get_result() const override { return result_; }
    const char* name() const override { return "MockTuningStrategy"; }

    bool begun() const { return begun_; }
    int steps() const { return steps_; }
    float last_measurement() const { return last_measurement_; }

private:
    bool begun_;
    bool done_;
    int  steps_;
    int  steps_until_done_;
    float output_per_step_;
    float last_measurement_;
    TuningResult result_;
};

// ============================================================================
// Fixture builder — fresh hardware + app on every test
// ============================================================================

struct AppFixture {
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

    AppFixture()
        : pid(65.0f, 12.0f, 38.0f, -255.0f, 255.0f),
          tuner(strategy),
          app(imu, motors, pid, tuner, mounting, online_est, safety, plant_id) {
    }
};

// ============================================================================
// Tests
// ============================================================================

static void test_starts_in_idle() {
    printf("\nTest: BalanceApp starts in IDLE after begin()\n");
    AppFixture f;
    BalanceAppConfig cfg = BalanceApp::default_config();
    f.app.begin(cfg, 1000);

    TEST_ASSERT(f.app.get_state() == BalanceAppState::IDLE,
                "state == IDLE after begin");
    TEST_EQ_INT(f.app.get_last_output(), 0, "last_output == 0 after begin");
    TEST_EQ_INT(f.motors.last_left(), 0, "motors left == 0 after begin");
    TEST_EQ_INT(f.motors.last_right(), 0, "motors right == 0 after begin");
}

static void test_short_press_idle_to_capture() {
    printf("\nTest: short press in IDLE transitions to CAPTURE_MOUNTING\n");
    AppFixture f;
    f.app.begin(BalanceApp::default_config(), 1000);

    f.app.on_short_press(1100);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::CAPTURE_MOUNTING,
                "state == CAPTURE_MOUNTING after short press from IDLE");

    // A step() in CAPTURE_MOUNTING should keep motors stopped.
    f.imu.set_pitch(0.5f);
    f.app.step(1105);
    TEST_EQ_INT(f.motors.last_left(), 0, "motors stopped during CAPTURE");
}

static void test_long_press_anywhere_sets_abort() {
    printf("\nTest: long press always sets abort flag\n");

    // From RUN: abort should be set, then next step() returns to IDLE.
    // Drop straight into RUN via enter_run_with_current_gains() — Phase 4.10c
    // moved long-press in IDLE to BOOTSTRAP, so the test's previous
    // IDLE→AUTO_TUNE→RUN shortcut is no longer reachable from the public API.
    {
        AppFixture f;
        BalanceAppConfig cfg = BalanceApp::default_config();
        f.app.begin(cfg, 1000);
        f.app.enter_run_with_current_gains(1000);
        TEST_ASSERT(f.app.get_state() == BalanceAppState::RUN,
                    "enter_run_with_current_gains → RUN");

        // Now long-press from RUN
        f.app.on_long_press(1020);
        TEST_ASSERT(f.safety.abort_requested(),
                    "abort flag set after long press in RUN");
        // Next step should observe abort and drop to IDLE
        f.app.step(1025);
        TEST_ASSERT(f.app.get_state() == BalanceAppState::IDLE,
                    "state == IDLE after abort step from RUN");
    }

    // From FALLEN: abort drops to IDLE.
    // Requires USE_BALANCE_FALL_DETECTION to trip the tipover transition.
#ifdef USE_BALANCE_FALL_DETECTION
    {
        AppFixture f;
        BalanceAppConfig cfg = BalanceApp::default_config();
        f.app.begin(cfg, 1000);
        f.app.enter_run_with_current_gains(1000);
        f.imu.set_pitch(0.0f);
        f.app.step(1010);
        TEST_ASSERT(f.app.get_state() == BalanceAppState::RUN, "in RUN");
        f.imu.set_pitch(40.0f);
        f.app.step(1020);
        TEST_ASSERT(f.app.get_state() == BalanceAppState::FALLEN,
                    "tipover -> FALLEN");

        f.app.on_long_press(1030);
        TEST_ASSERT(f.safety.abort_requested(),
                    "abort flag set after long press in FALLEN");
        f.app.step(1035);
        TEST_ASSERT(f.app.get_state() == BalanceAppState::IDLE,
                    "abort from FALLEN -> IDLE");
    }
#endif
}

static void test_capture_to_tune_transition() {
    printf("\nTest: stable pitch during CAPTURE_MOUNTING -> BOOTSTRAP\n");
    AppFixture f;
    BalanceAppConfig cfg = BalanceApp::default_config();
    cfg.capture_duration_ms = 200;       // short window for the test
    cfg.capture_pitch_var_deg = 0.5f;    // tight enough
    f.app.begin(cfg, 0);

    f.app.on_short_press(0);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::CAPTURE_MOUNTING,
                "entered CAPTURE_MOUNTING");

    // Feed stable samples at pitch=1.5° (stillness OK). Step exactly until
    // the capture window closes (~210 ms in) and the transition fires.
    // Phase 4.10c: capture success auto-chains to BOOTSTRAP (was IDLE/AUTO_TUNE).
    f.imu.set_pitch(1.5f);
    bool reached_bootstrap = false;
    for (int i = 0; i < 30; ++i) {
        f.app.step(10u + (uint32_t)i * 10u);
        if (f.app.get_state() == BalanceAppState::BOOTSTRAP) {
            reached_bootstrap = true;
            break;
        }
    }
    TEST_ASSERT(reached_bootstrap,
                "stable pitch capture -> BOOTSTRAP");
    // The mounting offset should now reflect ~1.5°
    const float off = f.app.get_mount_offset_deg();
    TEST_ASSERT(off > 1.4f && off < 1.6f,
                "mount offset captured ~1.5°");
}

static void test_capture_jitter_rejected() {
    printf("\nTest: jittery pitch during CAPTURE_MOUNTING returns to IDLE\n");
    AppFixture f;
    BalanceAppConfig cfg = BalanceApp::default_config();
    cfg.capture_duration_ms = 200;
    cfg.capture_pitch_var_deg = 0.1f;    // very tight — easy to violate
    f.app.begin(cfg, 0);

    f.app.on_short_press(0);

    // Alternating pitch creates large variance.
    for (int i = 0; i < 30; ++i) {
        f.imu.set_pitch((i & 1) ? 5.0f : -5.0f);
        f.app.step(10u + (uint32_t)i * 10u);
    }
    TEST_ASSERT(f.app.get_state() == BalanceAppState::IDLE,
                "jittery capture -> back to IDLE");
}

static void test_tune_to_run_on_success() {
    printf("\nTest: successful tune transitions to RUN with new gains\n");
    AppFixture f;
    BalanceAppConfig cfg = BalanceApp::default_config();
    f.app.begin(cfg, 1000);
    f.strategy.set_steps_until_done(3);
    (void)f.strategy;
    // Phase 4.10c (2026-05-18): AUTO_TUNE is no longer reachable via the
    // public API — long_press(IDLE) now enters BOOTSTRAP, capture-success
    // chains to BOOTSTRAP. The success path (push gains → enter RUN) is
    // covered by test_balance_app_bootstrap.cpp Test 3. AUTO_TUNE remains
    // in the enum for backwards compatibility but is dev-trigger-only.
    printf("  SKIP: AUTO_TUNE success-path moved to BOOTSTRAP\n");
}

static void test_tune_to_idle_on_failure() {
    printf("\nTest: failed tune restores gains and returns to IDLE\n");
    // Phase 4.10c: same reason as above — AUTO_TUNE failure path obsolete.
    // BOOTSTRAP failure → IDLE is covered by test_balance_app_bootstrap.cpp
    // Tests 1, 2, and 6.
    printf("  SKIP: AUTO_TUNE failure-path moved to BOOTSTRAP\n");
}

// FALLEN tests require fall detection to be compiled in. The default Uno
// build no longer enables it (operator preference: "balance forever").
#ifdef USE_BALANCE_FALL_DETECTION
static void test_run_to_fallen_on_tipover() {
    printf("\nTest: pitch beyond tilt_limit triggers FALLEN\n");
    AppFixture f;
    BalanceAppConfig cfg = BalanceApp::default_config();
    cfg.tilt_limit_deg = 35.0f;
    f.app.begin(cfg, 1000);

    f.strategy.set_steps_until_done(1);
    f.app.on_long_press(1000);
    f.imu.set_pitch(0.0f);
    f.app.step(1010);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::RUN, "in RUN");

    f.imu.set_pitch(40.0f);
    f.app.step(1015);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::FALLEN,
                "pitch=40° -> FALLEN");
    TEST_EQ_INT(f.motors.last_left(), 0, "motors stopped in FALLEN");
    TEST_EQ_INT(f.motors.last_right(), 0, "motors stopped in FALLEN");
}

static void test_fallen_is_sticky() {
    printf("\nTest: FALLEN is sticky — pitch back to level does NOT auto-recover\n");
    AppFixture f;
    BalanceAppConfig cfg = BalanceApp::default_config();
    cfg.tilt_limit_deg = 35.0f;
    f.app.begin(cfg, 1000);

    f.strategy.set_steps_until_done(1);
    f.app.on_long_press(1000);
    f.imu.set_pitch(0.0f);
    f.app.step(1010);
    f.imu.set_pitch(40.0f);
    f.app.step(1015);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::FALLEN, "in FALLEN");

    // Return to level — sticky FALLEN means we stay there.
    f.imu.set_pitch(0.0f);
    for (int i = 0; i < 200; ++i) {
        f.app.step(1020u + (uint32_t)i * 5u);
    }
    TEST_ASSERT(f.app.get_state() == BalanceAppState::FALLEN,
                "still FALLEN after 1 s of level pitch (no auto-recovery)");
}

static void test_motors_stopped_in_idle_and_fallen() {
    printf("\nTest: motors stopped in IDLE and FALLEN\n");
    AppFixture f;
    f.app.begin(BalanceApp::default_config(), 1000);

    f.app.step(1010);
    TEST_EQ_INT(f.motors.last_left(), 0, "IDLE: motors left == 0");
    TEST_EQ_INT(f.motors.last_right(), 0, "IDLE: motors right == 0");
    TEST_EQ_INT(f.app.get_last_output(), 0, "IDLE: last_output == 0");

    f.strategy.set_steps_until_done(1);
    f.app.on_long_press(1020);
    f.imu.set_pitch(0.0f);
    f.app.step(1030);
    f.imu.set_pitch(40.0f);
    f.app.step(1035);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::FALLEN, "in FALLEN");
    TEST_EQ_INT(f.motors.last_left(), 0, "FALLEN: motors left == 0");
    TEST_EQ_INT(f.motors.last_right(), 0, "FALLEN: motors right == 0");
    TEST_EQ_INT(f.app.get_last_output(), 0, "FALLEN: last_output == 0");
}

static void test_short_press_restarts_from_fallen() {
    printf("\nTest: short press in FALLEN restarts RUN immediately\n");
    AppFixture f;
    BalanceAppConfig cfg = BalanceApp::default_config();
    f.app.begin(cfg, 1000);
    f.strategy.set_steps_until_done(1);
    f.app.on_long_press(1000);
    f.imu.set_pitch(0.0f);
    f.app.step(1010);
    f.imu.set_pitch(40.0f);
    f.app.step(1015);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::FALLEN, "in FALLEN");

    // Operator press restarts directly into RUN regardless of pitch counter.
    f.imu.set_pitch(0.0f);
    f.app.on_short_press(1020);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::RUN,
                "short press in FALLEN -> RUN immediately");
}
#endif  // USE_BALANCE_FALL_DETECTION

// ----------------------------------------------------------------------------
// STUCK detector test (audit P1-COV-3 — audit_code_quality_balance_stack_
// 2026-05-19.md §7).
//
// Condition (balance_app.cpp step_run_, post-collision/HELD checks):
//   |pwm output| >= SAT_THRESHOLD_PWM (180) AND |gyro_pitch_dps| < STUCK_GYRO_DPS
//   (5.0) for >= STUCK_TIMEOUT_MS (1500).
//
// Observable side effect:
//   1. motors_.stop()
//   2. last_output_ = 0
//   3. safety_.request_abort()
//   → next tick: step_run_ observes abort_requested(), clear_abort(), enter
//     IDLE.
//
// The test holds a steady high pitch so the PID command pegs near the output
// limit, while gyro_pitch_dps stays at 0 (default MockIMU does not override
// getRawGyro, so raw_gyro_dps_[1] never moves off zero). After >1500 ms of
// such hold, we expect the state to drop to IDLE and abort to have been
// requested-and-cleared by step_run_.
// ----------------------------------------------------------------------------
static void test_stuck_detector_fires_on_saturated_zero_gyro() {
    printf("\nTest: STUCK detector (output saturated + gyro=0 >1500ms) → IDLE\n");
    AppFixture f;
    BalanceAppConfig cfg = BalanceApp::default_config();
    // Disable adaptive plumbing so applied_* doesn't ramp Kp away from the
    // initial value during the long hold — keeps the saturation signal
    // perfectly steady throughout the 1500 ms window.
    cfg.enable_online_adaptation = false;
    f.app.begin(cfg, 1000);

    // Enter RUN directly so we can observe step_run_'s detector. PID limits
    // default to ±255 per enter_run_with_current_gains.
    f.app.enter_run_with_current_gains(1000);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::RUN,
                "entered RUN for STUCK exercise");

    // Pitch held at 5° (outside the 1° soft zone, inside the 25° soft-cutoff).
    // With cfg.initial_kp = 50, P-term alone produces 5° × 50 = 250 PWM/° which
    // clamps to +255 — well above the 180 SAT_THRESHOLD_PWM. Gyro stays at 0
    // (MockIMU's default get*) so abs_gy is 0 < STUCK_GYRO_DPS.
    f.imu.set_pitch(5.0f);

    // Step for 1600 ms at 5 ms/tick = 320 ticks. After 1500 ms the STUCK
    // detector should fire (motors stop + safety.request_abort()); the very
    // next tick observes abort_requested() and routes RUN → IDLE.
    bool reached_idle = false;
    uint32_t saw_idle_at = 0;
    for (int i = 0; i < 320; ++i) {
        const uint32_t t = 1000u + (uint32_t)(i + 1) * 5u;
        f.app.step(t);
        if (f.app.get_state() == BalanceAppState::IDLE) {
            reached_idle = true;
            saw_idle_at = t;
            break;
        }
    }
    TEST_ASSERT(reached_idle,
                "STUCK detector eventually routes RUN → IDLE");
    // Sanity-check the timing — it should not fire prematurely. 1000 + 1500
    // = 2500 ms is the earliest the timeout can elapse, and step_run_ runs
    // one more tick afterward to observe abort. So we expect >= 2505 ms.
    TEST_ASSERT(saw_idle_at >= 2500u,
                "STUCK transition occurred only after the 1500 ms timeout");
    TEST_EQ_INT(f.motors.last_left(), 0,
                "motors stopped after STUCK fires");
    TEST_EQ_INT(f.app.get_last_output(), 0,
                "last_output zeroed after STUCK fires");
}

static void test_state_name_strings() {
    printf("\nTest: state_name() returns expected strings\n");
    AppFixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    TEST_ASSERT(strcmp(f.app.state_name(), "IDLE") == 0,
                "state_name == IDLE");
    f.app.on_short_press(1000);
    // state_name() returns "CAP" (short form for serial-friendly logging),
    // not the full enum name.
    TEST_ASSERT(strcmp(f.app.state_name(), "CAP") == 0,
                "state_name == CAP");
}

static void test_safety_thresholds_pushed_from_config() {
    printf("\nTest: begin() pushes tilt thresholds into BalanceSafety\n");
    AppFixture f;
    BalanceAppConfig cfg = BalanceApp::default_config();
    cfg.tilt_limit_deg = 25.0f;
    cfg.tilt_recovery_deg = 8.0f;
    f.app.begin(cfg, 1000);

    TEST_ASSERT(fabsf(f.safety.tilt_limit_deg() - 25.0f) < 0.01f,
                "tilt_limit pushed to safety");
    TEST_ASSERT(fabsf(f.safety.tilt_recovery_deg() - 8.0f) < 0.01f,
                "tilt_recovery pushed to safety");
}

// ============================================================================
// Standalone tests for BalanceSafety
// ============================================================================

static void test_safety_basics() {
    printf("\nTest: BalanceSafety tilt + watchdog + abort basics\n");
    BalanceSafety s;
    s.set_tilt_limit(30.0f);
    s.set_tilt_recovery(10.0f);

    TEST_ASSERT(!s.is_tipover(20.0f), "20° not a tipover");
    TEST_ASSERT(s.is_tipover(31.0f), "31° is a tipover");
    TEST_ASSERT(s.is_tipover(-31.0f), "-31° is also a tipover");
    TEST_ASSERT(s.is_recovered(5.0f), "5° is recovered");
    TEST_ASSERT(!s.is_recovered(15.0f), "15° not recovered");

    // Abort cycle
    TEST_ASSERT(!s.abort_requested(), "abort starts cleared");
    s.request_abort();
    TEST_ASSERT(s.abort_requested(), "abort set after request");
    s.clear_abort();
    TEST_ASSERT(!s.abort_requested(), "abort cleared");

    // Watchdog
    s.set_watchdog_timeout(100);
    TEST_ASSERT(!s.watchdog_starved(0),
                "watchdog not starved before any feed");
    s.feed_watchdog(1000);
    TEST_ASSERT(!s.watchdog_starved(1050), "50 ms later: not starved");
    TEST_ASSERT(s.watchdog_starved(1200), "200 ms later: starved");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    printf("==============================================\n");
    printf("BalanceApp Unit Tests (Phase 4.7a)\n");
    printf("==============================================\n");

    test_starts_in_idle();
    test_short_press_idle_to_capture();
    test_long_press_anywhere_sets_abort();
    test_capture_to_tune_transition();
    test_capture_jitter_rejected();
    test_tune_to_run_on_success();
    test_tune_to_idle_on_failure();
#ifdef USE_BALANCE_FALL_DETECTION
    test_run_to_fallen_on_tipover();
    test_fallen_is_sticky();
    test_motors_stopped_in_idle_and_fallen();
    test_short_press_restarts_from_fallen();
#endif
    test_stuck_detector_fires_on_saturated_zero_gyro();
    test_state_name_strings();
    test_safety_thresholds_pushed_from_config();
    test_safety_basics();

    printf("\n==============================================\n");
    printf("Results: %d run, %d passed, %d failed\n",
           tests_run, tests_passed, tests_failed);
    printf("==============================================\n");

    return (tests_failed == 0) ? 0 : 1;
}
