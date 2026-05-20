/**
 * Collision detection tests for BalanceApp.
 *
 * The balance bot has no encoders / optical flow / bumpers. It wanders during
 * testing and can collide with walls or cables. Any collision derails:
 *
 *   - BOOTSTRAP: impact's |Δω| gets attributed to motor force → wrong K_est
 *   - CHARACTERISE: same mechanism at lower PWM → wrong stiction floor
 *   - RUN: external impulse the PID can't recover from at current gains
 *
 * The detector reads BNO055 VECTOR_LINEARACCEL (gravity-removed body accel)
 * each tick, computes magnitude, and latches a "collision_detected" flag when
 * ANY of three OR-fire gates trips
 * (research_collision_signature_bno055.md §3 / §5):
 *
 *   PEAK    — |a| > COLLISION_PEAK_MPS2 (12 m/s²)   single tick → latch
 *   SUSTAIN — |a| > COLLISION_SUSTAIN_MPS2 (8 m/s²) for >= COLLISION_SUSTAIN_TICKS
 *             (3 ticks @ 5 ms = 15 ms ≈ 1.5 unique LIA samples) → latch
 *   KICK    — |a| > COLLISION_KICK_MPS2 (6 m/s²) AND |gyro_y| > COLLISION_KICK_GYRO_DPS
 *             (200 deg/s) → latch
 *
 * Tests verify (each gate gets one positive + one just-below-threshold negative):
 *
 *   1. PEAK fires on a single tick at 13 m/s² → latches.
 *   2. PEAK does NOT fire at 11.9 m/s² for one tick (just below peak, also
 *      below sustain-3 timing) → no latch.
 *   3. SUSTAIN fires after 3 ticks at 10 m/s² (above 8, below 12).
 *   4. SUSTAIN does NOT fire after only 2 ticks at 10 m/s² → no latch.
 *   5. KICK fires at 7 m/s² when |gyro_y| = 250 deg/s → latch (cross-arm).
 *   6. KICK does NOT fire at 7 m/s² when |gyro_y| = 150 deg/s → no latch.
 *   7. Latch persists; clear_collision() resets it.
 *   8. BOOTSTRAP aborts to IDLE with failure_reason=5 (collision) during
 *      baseline.
 *   9. BOOTSTRAP aborts to IDLE with failure_reason=5 during pulse, motors
 *      stopped, no false K_motor pushed to PID.
 *   10. CHARACTERISE aborts to IDLE on collision mid-sweep.
 *   11. RUN drops to HELD when a spike occurs (operator preference: lenient
 *       HELD, not sticky FALLEN — bot resumes once motion is quiet+level).
 *   12. RUN stays in RUN at 7 m/s² steady recovery transient (below all gates
 *       without gyro coincidence).
 *
 * Reuses the mock pattern from test_balance_app.cpp + test_balance_app_bootstrap.cpp.
 *
 * Compile (from auto_orientation/):
 *
 *   g++ -std=c++11 -O2 -fpermissive -DUNIT_TEST \
 *       -o tests/test_balance_app_collision \
 *       tests/test_balance_app_collision.cpp \
 *       src/applications/balancing_robot/balance_app.cpp \
 *       src/applications/balancing_robot/safety.cpp \
 *       src/control/pid_controller.cpp \
 *       src/control/auto_pid_tuner.cpp \
 *       src/control/plant_identifier.cpp \
 *       src/navigation/mounting_calibration.cpp \
 *       src/navigation/online_mounting_estimator.cpp
 *   ./tests/test_balance_app_collision
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
#include "../src/actuators/motor_driver.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg)                                                 \
    do {                                                                       \
        tests_run++;                                                           \
        if (cond) { tests_passed++; printf("  PASS: %s\n", msg); }             \
        else { tests_failed++; printf("  FAIL (%s:%d): %s\n",                  \
                                      __FILE__, __LINE__, msg); }              \
    } while (0)

// ============================================================================
// Mock with programmable linear-accel
// ============================================================================

class CollisionIMU : public OrientationSensor {
public:
    CollisionIMU() : initialized_(true) {
        od_.pitch_deg = 0.0f; od_.roll_deg = 0.0f; od_.yaw_deg = 0.0f;
        od_.w = 1.0f; od_.x = 0.0f; od_.y = 0.0f; od_.z = 0.0f;
        gyro_y_ = 0.0f;
        la_[0] = 0.0f; la_[1] = 0.0f; la_[2] = 0.0f;
    }
    void set_pitch(float deg)        { od_.pitch_deg = deg; }
    void set_gyro_y(float dps)       { gyro_y_ = dps; }
    void set_linear_accel(float x, float y, float z) {
        la_[0] = x; la_[1] = y; la_[2] = z;
    }
    // Convenience — magnitude-only spike on the X axis.
    void set_linear_accel_mag(float m) { la_[0] = m; la_[1] = 0.0f; la_[2] = 0.0f; }

    // Sensor base
    bool begin() override { initialized_ = true; return true; }
    void end() override { initialized_ = false; }
    bool isInitialized() const override { return initialized_; }
    bool read() override { return true; }
    bool hasNewData() const override { return true; }
    const char* name() const override { return "CollisionIMU"; }
    bool isHealthy() const override { return true; }
    const char* getStatusString() const override { return "OK"; }

    // OrientationSensor
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
        xyz[0] = la_[0]; xyz[1] = la_[1]; xyz[2] = la_[2]; return true;
    }

private:
    OrientationData od_;
    bool  initialized_;
    float gyro_y_;
    float la_[3];
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
    CollisionIMU             imu;
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
        : pid(0.0f, 0.0f, 0.0f, -255.0f, 255.0f),
          tuner(strategy),
          app(imu, motors, pid, tuner, mounting, online_est, safety, plant_id) {}
};

static uint32_t advance_ticks(Fixture& f, uint32_t start_ms,
                              uint32_t dt_ms, int n_ticks) {
    uint32_t t = start_ms;
    for (int i = 0; i < n_ticks; ++i) {
        t = start_ms + (uint32_t)(i + 1) * dt_ms;
        f.app.step(t);
    }
    return t;
}

// ============================================================================
// Tests
// ============================================================================

// (1) PEAK gate POSITIVE: single tick at 13 m/s² (> COLLISION_PEAK_MPS2=12) latches.
static void test_collision_peak_single_tick_latches() {
    printf("\nTest 1: PEAK — single tick 13 m/s² latches collision_detected\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.imu.set_pitch(0.0f);

    f.imu.set_linear_accel_mag(13.0f);   // above PEAK=12
    f.app.step(1005);                    // one tick is enough for PEAK
    TEST_ASSERT(f.app.collision_detected(),
                "PEAK fires on single tick above COLLISION_PEAK_MPS2");
}

// (2) PEAK gate NEGATIVE: single tick at 11.9 m/s² (just below PEAK) does NOT latch.
//     11.9 > 8 (SUSTAIN floor) so the SUSTAIN counter increments to 1 only —
//     also below the 3-tick sustain threshold. No gyro coincidence either.
static void test_collision_peak_just_below_no_latch() {
    printf("\nTest 2: PEAK — single tick 11.9 m/s² does NOT latch\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.imu.set_pitch(0.0f);
    f.imu.set_gyro_y(0.0f);

    f.imu.set_linear_accel_mag(11.9f);   // just under PEAK=12
    f.app.step(1005);
    f.imu.set_linear_accel_mag(0.0f);    // drop back below SUSTAIN floor
    f.app.step(1010);

    TEST_ASSERT(!f.app.collision_detected(),
                "single tick at 11.9 m/s² stays under PEAK and SUSTAIN");
}

// (3) SUSTAIN gate POSITIVE: 3 ticks at 10 m/s² (above SUSTAIN=8, below PEAK=12)
//     latches after the 3rd tick. Stays in IDLE so latch persists for inspection.
static void test_collision_sustain_3_ticks_latches() {
    printf("\nTest 3: SUSTAIN — 3 ticks @ 10 m/s² latches collision_detected\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.imu.set_pitch(0.0f);
    f.imu.set_gyro_y(0.0f);

    f.imu.set_linear_accel_mag(10.0f);            // > SUSTAIN, < PEAK
    f.app.step(1005);                             // consec=1
    TEST_ASSERT(!f.app.collision_detected(), "after 1 tick — no latch");
    f.app.step(1010);                             // consec=2
    TEST_ASSERT(!f.app.collision_detected(), "after 2 ticks — no latch");
    f.app.step(1015);                             // consec=3 → LATCH
    TEST_ASSERT(f.app.collision_detected(),
                "3rd consecutive tick above SUSTAIN latches");

    f.app.clear_collision();
    TEST_ASSERT(!f.app.collision_detected(),
                "clear_collision() resets the latch");
}

// (4) SUSTAIN gate NEGATIVE: only 2 ticks at 10 m/s² (still below the 3-tick
//     gate, below PEAK) does NOT latch.
static void test_collision_sustain_2_ticks_no_latch() {
    printf("\nTest 4: SUSTAIN — only 2 ticks @ 10 m/s² does NOT latch\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.imu.set_pitch(0.0f);
    f.imu.set_gyro_y(0.0f);

    f.imu.set_linear_accel_mag(10.0f);
    f.app.step(1005);
    f.app.step(1010);
    f.imu.set_linear_accel_mag(0.0f);
    f.app.step(1015);
    TEST_ASSERT(!f.app.collision_detected(),
                "2 ticks @ 10 m/s² stay below 3-tick SUSTAIN gate");
}

// (5) KICK gate POSITIVE: 7 m/s² accel + 250 deg/s pitch rate latches.
//     7 > KICK=6 AND 250 > KICK_GYRO=200 — single tick.
static void test_collision_kick_latches() {
    printf("\nTest 5: KICK — 7 m/s² + 250 deg/s gyro latches\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.imu.set_pitch(0.0f);

    f.imu.set_linear_accel_mag(7.0f);     // > KICK=6, < PEAK=12, < SUSTAIN=8 floor
    f.imu.set_gyro_y(250.0f);             // > KICK_GYRO=200
    f.app.step(1005);
    TEST_ASSERT(f.app.collision_detected(),
                "KICK fires on accel>6 AND |gyro|>200");
}

// (6) KICK gate NEGATIVE: 7 m/s² accel but only 150 deg/s gyro does NOT latch.
//     accel < SUSTAIN floor so counter doesn't accumulate; PEAK not reached;
//     gyro coincidence not met → all three gates closed.
static void test_collision_kick_just_below_no_latch() {
    printf("\nTest 6: KICK — 7 m/s² + 150 deg/s gyro does NOT latch\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.imu.set_pitch(0.0f);

    f.imu.set_linear_accel_mag(7.0f);     // > KICK=6, < SUSTAIN=8
    f.imu.set_gyro_y(150.0f);             // < KICK_GYRO=200
    for (int i = 0; i < 5; ++i) {         // sustained 5 ticks
        f.app.step(1000 + (uint32_t)i * 5);
    }
    TEST_ASSERT(!f.app.collision_detected(),
                "KICK requires |gyro|>200, 150 dps is below");
}

// (7) Below-threshold for all three gates does NOT latch (false-positive guard).
static void test_collision_below_all_no_latch() {
    printf("\nTest 7: below-all-gates accel (recovery transient) does NOT latch\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.imu.set_pitch(0.0f);
    f.imu.set_gyro_y(0.0f);

    // 5.5 m/s² — below KICK floor (6), below SUSTAIN floor (8), below PEAK (12).
    // Represents a vigorous balance recovery, NOT a collision.
    for (int i = 0; i < 20; ++i) {
        f.imu.set_linear_accel_mag(5.5f);
        f.app.step(1000 + (uint32_t)i * 5);
    }
    TEST_ASSERT(!f.app.collision_detected(),
                "5.5 m/s² sustained stays below all three gates");
}

// (8) BOOTSTRAP: collision during baseline → IDLE with failure_reason=5.
// Uses a single-tick PEAK above 12 m/s² (sharpest fire path).
static void test_bootstrap_collision_during_baseline() {
    printf("\nTest 8: BOOTSTRAP collision during baseline → IDLE reason=5\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.imu.set_pitch(0.0f);
    f.imu.set_gyro_y(0.0f);

    f.app.enter_bootstrap(2000);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::BOOTSTRAP,
                "entered BOOTSTRAP");

    // 50 ms into baseline (still motors off), simulate hard impact.
    advance_ticks(f, 2000, 5, 10);
    f.imu.set_linear_accel_mag(20.0f);   // > PEAK=12, latches on first tick
    f.app.step(2055);

    TEST_ASSERT(f.app.get_state() == BalanceAppState::IDLE,
                "BOOTSTRAP exits to IDLE on collision");
    TEST_ASSERT(f.app.get_bootstrap_result().failure_reason == 5,
                "failure_reason = collision (5)");
    TEST_ASSERT(!f.app.get_bootstrap_result().converged,
                "converged=false on collision exit");
    TEST_ASSERT(f.motors.last_left() == 0,
                "motors stopped after collision-abort");
}

// (9) BOOTSTRAP: collision during pulse → IDLE with failure_reason=5.
// PID gains must NOT be updated since K_motor estimate would be corrupt.
static void test_bootstrap_collision_during_pulse() {
    printf("\nTest 9: BOOTSTRAP collision during pulse → IDLE no gain push\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.imu.set_pitch(0.0f);
    f.imu.set_gyro_y(0.0f);

    // Capture PID gains before bootstrap.
    float kp_before, ki_before, kd_before;
    f.pid.get_tunings(kp_before, ki_before, kd_before);

    f.app.enter_bootstrap(0);
    // Run past baseline (300 ms) into pulse 0 (300-450 ms).
    advance_ticks(f, 0, 5, 70);   // 350 ms — mid-pulse-0
    TEST_ASSERT(f.app.get_state() == BalanceAppState::BOOTSTRAP,
                "still in BOOTSTRAP mid-pulse");

    // Impact during the pulse — PEAK gate single tick.
    f.imu.set_linear_accel_mag(25.0f);
    f.app.step(355);

    TEST_ASSERT(f.app.get_state() == BalanceAppState::IDLE,
                "BOOTSTRAP exits to IDLE on pulse-collision");
    TEST_ASSERT(f.app.get_bootstrap_result().failure_reason == 5,
                "failure_reason = collision (5)");

    // PID gains must NOT have been pushed (the K_est would be garbage).
    float kp_after, ki_after, kd_after;
    f.pid.get_tunings(kp_after, ki_after, kd_after);
    TEST_ASSERT(kp_after == kp_before && kd_after == kd_before,
                "PID gains unchanged after collision abort");
}

// (10) CHARACTERISE: collision mid-sweep → IDLE.
static void test_characterise_collision_aborts() {
    printf("\nTest 10: CHARACTERISE collision mid-sweep → IDLE\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.imu.set_pitch(0.0f);

    f.app.enter_characterise_actuator(0);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::CHAR_ACT,
                "entered CHAR_ACT");

    // 100 ms in — past baseline (200 ms in CHAR_ACT, but we just need to be in state).
    advance_ticks(f, 0, 5, 30);

    // Inject PEAK-class collision.
    f.imu.set_linear_accel_mag(18.0f);
    f.app.step(155);

    TEST_ASSERT(f.app.get_state() == BalanceAppState::IDLE,
                "CHARACTERISE exits to IDLE on collision");
    TEST_ASSERT(f.motors.last_left() == 0, "motors stopped after collision");
}

// (11) RUN: collision → HELD (NOT sticky FALLEN — operator preference is lenient).
static void test_run_collision_drops_to_held() {
    printf("\nTest 11: RUN collision → HELD (lenient, not sticky FALLEN)\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.imu.set_pitch(0.0f);
    f.imu.set_gyro_y(0.0f);

    f.app.enter_run_with_current_gains(2000);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::RUN, "entered RUN");

    // Step a few ticks at zero accel to stabilize the detector state.
    advance_ticks(f, 2000, 5, 5);

    // Sudden PEAK-class impact (single tick latches).
    f.imu.set_linear_accel_mag(25.0f);
    f.app.step(2030);

    TEST_ASSERT(f.app.get_state() == BalanceAppState::HELD,
                "RUN drops to HELD on collision spike");
    TEST_ASSERT(f.motors.last_left() == 0, "motors stopped in HELD");

    // Clear the spike, simulate quiet+level for resume — HELD must not be sticky.
    f.imu.set_linear_accel_mag(0.0f);
    advance_ticks(f, 2030, 5, 60);   // 300 ms of quiet — > 200 ms resume dwell
    TEST_ASSERT(f.app.get_state() == BalanceAppState::RUN,
                "HELD auto-resumes to RUN once motion is quiet (lenient)");
}

// (12) RUN without collision: stays in RUN at recovery-transient accel level.
//      7 m/s² is below KICK floor (6 — wait, above kick floor of 6), but without
//      gyro coincidence (kept at 0) it can't fire KICK; below SUSTAIN floor (8)
//      so SUSTAIN counter never accumulates; below PEAK (12).
static void test_run_no_false_positive_at_recovery() {
    printf("\nTest 12: RUN at 7 m/s² recovery accel (no gyro) does NOT drop to HELD\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.imu.set_pitch(0.0f);
    f.imu.set_gyro_y(0.0f);

    f.app.enter_run_with_current_gains(2000);

    // 50 ticks of 7 m/s² accel, no gyro motion — vigorous recovery, not collision.
    for (int i = 0; i < 50; ++i) {
        f.imu.set_linear_accel_mag(7.0f);
        f.app.step(2000 + (uint32_t)i * 5);
    }

    TEST_ASSERT(f.app.get_state() == BalanceAppState::RUN,
                "RUN stays in RUN under recovery transients below all gates");
}

// ============================================================================
// main
// ============================================================================

int main() {
    printf("BalanceApp COLLISION-DETECTION unit tests\n");
    printf("=========================================\n");

    test_collision_peak_single_tick_latches();
    test_collision_peak_just_below_no_latch();
    test_collision_sustain_3_ticks_latches();
    test_collision_sustain_2_ticks_no_latch();
    test_collision_kick_latches();
    test_collision_kick_just_below_no_latch();
    test_collision_below_all_no_latch();
    test_bootstrap_collision_during_baseline();
    test_bootstrap_collision_during_pulse();
    test_characterise_collision_aborts();
    test_run_collision_drops_to_held();
    test_run_no_false_positive_at_recovery();

    printf("\n--- Summary ---\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
