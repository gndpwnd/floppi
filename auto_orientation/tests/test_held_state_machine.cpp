/**
 * HELD-state tests for BalanceApp (Workstream 3 — native test hardening).
 *
 * Verifies the RUN <-> HELD transitions gated by motion signals:
 *
 *   - High lateral gyro (sqrt(gx² + gz²) > 30 dps sustained for ~30 ticks @
 *     5 ms = 150 ms) triggers RUN -> HELD.
 *   - Once in HELD, motors must be zero regardless of pitch.
 *   - HELD with quiet motion (g_lateral < 12 dps AND a_dev < 1.5 m/s²) AND
 *     pitch < 8° for 40 ticks (200 ms) auto-resumes back to RUN.
 *
 * This test must be compiled with -DUSE_BALANCE_HELD_DETECTION so the
 * #ifdef block in step_run_() is enabled (default-off in some build envs).
 *
 * Compile (from auto_orientation/):
 *
 *   g++ -std=c++11 -O2 -fpermissive -DUNIT_TEST -DUSE_BALANCE_HELD_DETECTION \
 *       -o tests/test_held_state_machine \
 *       tests/test_held_state_machine.cpp \
 *       src/applications/balancing_robot/balance_app.cpp \
 *       src/applications/balancing_robot/safety.cpp \
 *       src/control/pid_controller.cpp \
 *       src/control/auto_pid_tuner.cpp \
 *       src/control/plant_identifier.cpp \
 *       src/navigation/mounting_calibration.cpp \
 *       src/navigation/online_mounting_estimator.cpp
 *   ./tests/test_held_state_machine
 */

#include <cstdio>
#include <cstdlib>
#include <cstdint>
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
#include "../src/actuators/motor_driver.h"

// ============================================================================
// Tiny test harness
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
// Mock IMU with raw-gyro / raw-accel support
// ============================================================================

class MockIMU : public OrientationSensor {
public:
    MockIMU() : initialized_(true) {
        od_.pitch_deg = 0.0f;
        od_.roll_deg = 0.0f;
        od_.yaw_deg = 0.0f;
        // Upright at rest: gravity aligned with body +Z axis.
        a_[0] = 0.0f; a_[1] = 0.0f; a_[2] = 9.81f;
        g_[0] = 0.0f; g_[1] = 0.0f; g_[2] = 0.0f;
    }

    void set_pitch(float p) { od_.pitch_deg = p; }
    void set_gyro(float gx, float gy, float gz) {
        g_[0] = gx; g_[1] = gy; g_[2] = gz;
    }
    void set_accel(float ax, float ay, float az) {
        a_[0] = ax; a_[1] = ay; a_[2] = az;
    }

    bool begin() override { return true; }
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

    bool getRawGyro(float xyz[3]) override {
        xyz[0] = g_[0]; xyz[1] = g_[1]; xyz[2] = g_[2];
        return true;
    }
    bool getRawAccel(float xyz[3]) override {
        xyz[0] = a_[0]; xyz[1] = a_[1]; xyz[2] = a_[2];
        return true;
    }
private:
    OrientationData od_;
    bool initialized_;
    float a_[3];
    float g_[3];
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

class MockTuningStrategy : public ITuningStrategy {
public:
    MockTuningStrategy() : done_(false) {
        result_.kp = 50.0f; result_.ki = 2.0f; result_.kd = 20.0f;
        result_.ultimate_gain = 0.0f;
        result_.ultimate_period_sec = 0.0f;
        result_.phase_margin_estimate_deg = 0.0f;
        result_.converged = true;
        result_.failure_reason = nullptr;
    }
    void begin(float, float, float, uint32_t) override { done_ = false; }
    float step(float, const SafetyLimits&, uint32_t) override {
        done_ = true; return 0.0f;
    }
    bool is_done() const override { return done_; }
    TuningResult get_result() const override { return result_; }
    const char* name() const override { return "MockTuning"; }
private:
    bool done_;
    TuningResult result_;
};

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

    void enter_run(uint32_t now_ms) {
        BalanceAppConfig cfg = BalanceApp::default_config();
        cfg.tilt_limit_deg = 90.0f;
        app.begin(cfg, now_ms);
        imu.set_pitch(0.0f);
        // Phase 4.10c: long_press(IDLE) now enters BOOTSTRAP. HELD tests only
        // need RUN-state behaviour, so use the direct entry helper.
        app.enter_run_with_current_gains(now_ms);
    }
};

// Warm up the motion-filter LPF (alpha=0.04, τ≈120 ms) at quiet baseline so
// subsequent transients pass through filtered values that match expectations.
static void warm_quiet_baseline(Fixture& f, uint32_t& t) {
    f.imu.set_pitch(0.0f);
    f.imu.set_gyro(0.0f, 0.0f, 0.0f);
    f.imu.set_accel(0.0f, 0.0f, 9.81f);
    // 200 ticks @ 5 ms = 1 s. The LPF settles in ~3τ ≈ 360 ms.
    for (int i = 0; i < 200; ++i) {
        f.app.step(t);
        t += 5;
    }
}

// ============================================================================
// Tests
// ============================================================================

static void test_high_lateral_gyro_triggers_held() {
    printf("\nTest: HELD via Phase 2.5 ext_motion (cmd-quiet + pitch-gyro-fast)\n");
    Fixture f;
    f.enter_run(1000);
    if (f.app.get_state() != BalanceAppState::RUN) {
        printf("  FAIL: did not reach RUN\n"); g_fails++; return;
    }
    uint32_t t = 1010;
    warm_quiet_baseline(f, t);

    // 2026-05-18 bench session removed the lateral-gyro HELD trigger because
    // it false-fired on every recovery transient. Phase 2.5 ext_motion
    // replaced it: HELD fires when (a) motor command magnitude < 20 AND
    // (b) pitch-axis gyro > 30 dps. Semantics: "bot is rotating but motors
    // aren't pushing it → external force." Reproduce that here.
    //
    // pitch=0 → PID output near zero → cmd_quiet. gyro_y=200 dps → above 30.
    // Dwell for ext_motion is 20 ticks (100 ms). Allow generous headroom.
    f.imu.set_pitch(0.0f);
    f.imu.set_gyro(0.0f, 200.0f, 0.0f);   // Y-axis = pitch rate
    bool entered_held = false;
    int  ticks_to_held = -1;
    for (int i = 0; i < 100; ++i) {
        f.app.step(t);
        t += 5;
        if (f.app.get_state() == BalanceAppState::HELD) {
            entered_held = true;
            ticks_to_held = i + 1;
            break;
        }
    }
    printf("  entered HELD: %s (after %d ticks = %d ms)\n",
           entered_held ? "yes" : "no",
           ticks_to_held, ticks_to_held * 5);
    TEST_ASSERT(entered_held, "RUN -> HELD on ext_motion (cmd quiet + pitch_gyro fast)");
    if (entered_held) {
        TEST_ASSERT(ticks_to_held * 5 <= 250,
                    "HELD entry within 250 ms");
    }
    TEST_ASSERT(f.motors.last_left() == 0, "motors.left == 0 in HELD");
    TEST_ASSERT(f.motors.last_right() == 0, "motors.right == 0 in HELD");
    TEST_ASSERT(f.app.get_last_output() == 0, "last_output == 0 in HELD");
}

static void test_held_quiet_motion_resumes_run() {
    printf("\nTest: HELD -> RUN when motion calms (200 ms dwell, lenient)\n");
    Fixture f;
    f.enter_run(1000);
    if (f.app.get_state() != BalanceAppState::RUN) {
        printf("  FAIL: did not reach RUN\n"); g_fails++; return;
    }
    uint32_t t = 1010;
    warm_quiet_baseline(f, t);

    // Force HELD via Phase 2.5 ext_motion (cmd quiet + pitch_gyro fast).
    f.imu.set_pitch(0.0f);
    f.imu.set_gyro(0.0f, 200.0f, 0.0f);
    for (int i = 0; i < 100 && f.app.get_state() != BalanceAppState::HELD; ++i) {
        f.app.step(t);
        t += 5;
    }
    if (f.app.get_state() != BalanceAppState::HELD) {
        printf("  FAIL: did not reach HELD before resume test\n");
        g_fails++; return;
    }

    // Now make everything quiet AND level. Per step_held_():
    //   quiet := (g_lateral_dps_lpf_ < 12.0f) && (a_dev_lpf_ < 1.5f)
    //   level := |pitch| < 8.0f
    // Dwell: 40 consecutive samples (200 ms @ 5 ms).
    f.imu.set_gyro(0.0f, 0.0f, 0.0f);
    f.imu.set_accel(0.0f, 0.0f, 9.81f);
    f.imu.set_pitch(0.0f);

    // The LPF needs time to discharge from 200 dps back below 12 dps. With
    // alpha=0.04 (τ=120 ms), time from 200 to 12 = τ*ln(200/12) ≈ 120*2.81 = 338 ms.
    // After that, 40 ticks (200 ms) of dwell. Allow up to 1.5 s overall.
    bool resumed = false;
    int  ticks_to_resume = -1;
    for (int i = 0; i < 300; ++i) {   // 300 * 5 ms = 1.5 s
        f.app.step(t);
        t += 5;
        if (f.app.get_state() == BalanceAppState::RUN) {
            resumed = true;
            ticks_to_resume = i + 1;
            break;
        }
    }
    printf("  resumed: %s (after %d ticks = %d ms)\n",
           resumed ? "yes" : "no",
           ticks_to_resume, ticks_to_resume * 5);
    TEST_ASSERT(resumed, "HELD -> RUN when motion quiet AND level");
}

static void test_pitch_in_held_does_not_drive_motors() {
    printf("\nTest: pitch in HELD does NOT drive motors\n");
    Fixture f;
    f.enter_run(1000);
    if (f.app.get_state() != BalanceAppState::RUN) {
        printf("  FAIL: did not reach RUN\n"); g_fails++; return;
    }
    uint32_t t = 1010;
    warm_quiet_baseline(f, t);

    // Drive to HELD via Phase 2.5 ext_motion.
    f.imu.set_pitch(0.0f);
    f.imu.set_gyro(0.0f, 200.0f, 0.0f);
    for (int i = 0; i < 100 && f.app.get_state() != BalanceAppState::HELD; ++i) {
        f.app.step(t);
        t += 5;
    }
    if (f.app.get_state() != BalanceAppState::HELD) {
        printf("  FAIL: did not reach HELD\n"); g_fails++; return;
    }

    // Now keep LATERAL gyro high (HELD's exit gate checks g_lateral_lpf,
    // which is sqrt(gx² + gz²) — NOT gy. Pitch_gyro_y was sufficient to
    // ENTER HELD via ext_motion, but to STAY in HELD we need lateral motion
    // active in the quiet-gate axes.) Inject pitch oscillations. Motors must
    // remain zero through every tick — step_held_() unconditionally stops
    // motors, so any non-zero command would be a bug.
    bool any_motor_drive = false;
    for (int i = 0; i < 50; ++i) {
        f.imu.set_pitch((i & 1) ? 5.0f : -5.0f);  // jitter pitch
        f.imu.set_gyro(200.0f, 0.0f, 0.0f);       // lateral keeps HELD active
        f.app.step(t);
        t += 5;
        if (f.motors.last_left() != 0 || f.motors.last_right() != 0 ||
            f.app.get_last_output() != 0) {
            any_motor_drive = true;
            break;
        }
    }
    TEST_ASSERT(!any_motor_drive,
                "motors stay at 0 throughout HELD regardless of pitch");
    TEST_ASSERT(f.app.get_state() == BalanceAppState::HELD,
                "state remained HELD with sustained lateral gyro");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    printf("====================================================\n");
    printf("BalanceApp HELD state-machine tests (Workstream 3)\n");
    printf("====================================================\n");

    test_high_lateral_gyro_triggers_held();
    test_held_quiet_motion_resumes_run();
    test_pitch_in_held_does_not_drive_motors();

    printf("\n====================================================\n");
    printf("Results: %d passed, %d failed\n", g_passes, g_fails);
    printf("====================================================\n");
    return (g_fails == 0) ? 0 : 1;
}
