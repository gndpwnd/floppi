/**
 * Native unit test — BOOTSTRAP K_motor preservation across RUN entry.
 *
 * Pinpoints audit P1-COV-1 (audit_code_quality_balance_stack_2026-05-19.md §7)
 * and the underlying P1-SM-1 bug:
 *
 *   When BOOTSTRAP succeeds it calls plant_id_.seed_k_motor(K_measured) and
 *   pushes the derived Kp/Ki/Kd into the PID. The state machine then
 *   transitions to RUN. The RUN entry side-effect (enter_state_ at the
 *   case RUN: block) calls plant_id_.reset(kp_now, kd_now) to seed the RLS
 *   prior from the live PID gains — which BACK-DERIVES K from
 *   Kp = ω_n²/K AND inflates the RLS covariance P_ to INITIAL_P=1.0,
 *   discarding the freshly-measured K and letting early noise drown it.
 *
 *   Fix B (audit P1-SM-1) skips the reset() when
 *   bootstrap_result_.converged is true so the seeded K (covariance =
 *   SEED_P=0.05) survives RUN entry.
 *
 * This test is a focused regression check for that single property — it
 * synthesizes a plant with known K_TRUE, walks BOOTSTRAP to completion, then
 * spies on plant_id_.get_k_motor() immediately before vs. immediately after
 * the RUN-entry side-effect block runs. The two readings must match.
 *
 * (test_balance_app_bootstrap.cpp Test 7 already covers the same property
 * inline as part of the broader bootstrap suite; this dedicated file makes
 * the regression visible as its own binary so it cannot silently break when
 * the bootstrap suite is rearranged.)
 *
 * Compile (from auto_orientation/):
 *   g++ -std=c++11 -O2 -fpermissive -DUNIT_TEST \
 *       -Iinclude -Isrc \
 *       -o tests/test_bootstrap_k_preservation \
 *       tests/test_bootstrap_k_preservation.cpp \
 *       src/applications/balancing_robot/balance_app.cpp \
 *       src/applications/balancing_robot/safety.cpp \
 *       src/control/pid_controller.cpp \
 *       src/control/auto_pid_tuner.cpp \
 *       src/control/plant_identifier.cpp \
 *       src/navigation/mounting_calibration.cpp \
 *       src/navigation/online_mounting_estimator.cpp
 *   ./tests/test_bootstrap_k_preservation
 */

#include <cstdio>
#include <cstdint>
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
// Test harness — printf + per-call counter, same convention as siblings.
// ============================================================================

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
// Mocks (mirrors test_balance_app_bootstrap.cpp — minimal IMU, motors, tuner)
// ============================================================================

class GyroIMU : public OrientationSensor {
public:
    GyroIMU() : initialized_(true), gyro_y_(0.0f) {
        od_.pitch_deg = 0.0f; od_.roll_deg = 0.0f; od_.yaw_deg = 0.0f;
        od_.w = 1.0f; od_.x = 0.0f; od_.y = 0.0f; od_.z = 0.0f;
    }
    void set_pitch(float deg) { od_.pitch_deg = deg; }
    void set_gyro_y(float dps) { gyro_y_ = dps; }

    bool begin() override { initialized_ = true; return true; }
    void end() override { initialized_ = false; }
    bool isInitialized() const override { return initialized_; }
    bool read() override { return true; }
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
        : pid(0.0f, 0.0f, 0.0f, -255.0f, 255.0f),
          tuner(strategy),
          app(imu, motors, pid, tuner, mounting, online_est, safety, plant_id) {}
};

// ============================================================================
// Synthetic-plant driver — same model as
// test_balance_app_bootstrap.cpp::test_bootstrap_success: gyro_y ramps linearly
// with applied PWM over each pulse window, then exponentially decays during
// cooldown so the next pulse starts at ≈0 dps (passing Fix C's |g0|<5 dps gate).
// ============================================================================

static void run_bootstrap_with_synthetic_plant(Fixture& f, float K_TRUE) {
    f.imu.set_pitch(0.0f);
    f.imu.set_gyro_y(0.0f);
    f.app.enter_bootstrap(0);

    const uint16_t BASELINE_MS = 300;
    const uint16_t PULSE_MS    = 150;
    const uint16_t COOLDOWN_MS = 400;
    const uint8_t  PULSE_PWMS_PER_WHEEL[4] = {180, 180, 240, 240};
    auto signed_pulse_pwm_total = [&](int idx) -> int16_t {
        const int16_t mag = (int16_t)(2 * PULSE_PWMS_PER_WHEEL[idx]);
        return (idx & 1) ? -mag : mag;
    };

    uint32_t t = 0;
    int last_pulse_idx = -1;
    uint32_t pulse_t0_ms = 0;
    float pulse_final_gyro_dps = 0.0f;

    // Total bootstrap span: 300 + 4 × (150 + 400) = 2500 ms = 500 ticks at
    // 5 ms; allow ample headroom for FINALISE + RUN transition.
    for (int i = 0; i < 560; ++i) {
        t = 5u * (uint32_t)(i + 1);
        const uint16_t elapsed = (uint16_t)t;
        if (elapsed >= BASELINE_MS &&
            elapsed <  BASELINE_MS + 4u * (PULSE_MS + COOLDOWN_MS)) {
            const uint16_t since_baseline = elapsed - BASELINE_MS;
            const int      cur_pulse      = since_baseline / (PULSE_MS + COOLDOWN_MS);
            const uint16_t cycle_pos      = since_baseline
                                            - (uint16_t)cur_pulse * (PULSE_MS + COOLDOWN_MS);
            const int16_t  pwm_total      = signed_pulse_pwm_total(cur_pulse);

            if (cycle_pos < PULSE_MS) {
                if (last_pulse_idx != cur_pulse) {
                    last_pulse_idx = cur_pulse;
                    pulse_t0_ms    = elapsed;
                }
                const float dt_pulse_sec =
                    ((float)elapsed - (float)pulse_t0_ms) * 0.001f;
                f.imu.set_gyro_y(K_TRUE * (float)pwm_total * dt_pulse_sec);
                pulse_final_gyro_dps =
                    K_TRUE * (float)pwm_total * ((float)PULSE_MS * 0.001f);
            } else {
                const float cool_ms = (float)(cycle_pos - PULSE_MS);
                if (cool_ms < 10.0f) {
                    f.imu.set_gyro_y(pulse_final_gyro_dps);
                } else {
                    const float settle_ms = cool_ms - 10.0f;
                    const float decay = std::exp(-settle_ms / 80.0f);
                    f.imu.set_gyro_y(pulse_final_gyro_dps * decay);
                }
            }
        } else {
            f.imu.set_gyro_y(0.0f);
        }
        f.app.step(t);
    }
}

// ============================================================================
// The single regression check.
// ============================================================================

static void test_bootstrap_k_preserved_into_run() {
    printf("\nTest: BOOTSTRAP-measured K_motor survives RUN-entry side effects\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    // Match the bounds used in test_balance_app_bootstrap.cpp Test 3 so the
    // plant-id classifier accepts the synthesized K without clamping.
    f.plant_id.set_k_motor_bounds(0.05f, 2.0f);
    f.plant_id.set_target_settling_time_sec(0.5f);
    f.plant_id.set_damping_ratio(0.7f);

    const float K_TRUE = 0.4f;
    run_bootstrap_with_synthetic_plant(f, K_TRUE);

    // BOOTSTRAP must have transitioned through FINALISE to RUN (this is the
    // path that exercises P1-SM-1).
    TEST_ASSERT(f.app.get_state() == BalanceAppState::RUN,
                "BOOTSTRAP synthetic plant → RUN");
    const BootstrapResult& r = f.app.get_bootstrap_result();
    TEST_ASSERT(r.converged,
                "BootstrapResult.converged = true");

    // The K reported by the BootstrapResult — what BOOTSTRAP CLAIMS it
    // measured, prior to any RUN-entry side effect.
    const float k_measured_by_bootstrap = r.k_motor;
    // The K currently sitting in the plant identifier RIGHT AFTER the
    // enter_state_(RUN) side-effect block ran. The whole point of Fix B is
    // that these two numbers must be equal — if plant_id_.reset() had run
    // unconditionally, the live K would now be the back-derived prior
    // (ω_n²/Kp_seed), not the measured K.
    const float k_after_run_entry = f.plant_id.get_k_motor();

    printf("  K_TRUE = %.4f   K_measured_by_bootstrap = %.4f   "
           "K_in_plant_id_after_run = %.4f\n",
           K_TRUE, k_measured_by_bootstrap, k_after_run_entry);

    // Both values come from the same float pipeline (seed_k_motor stores
    // directly into k_motor_); after RUN entry they should be bit-identical
    // modulo the plant_identifier's internal clamps. Use a tight tolerance —
    // anything more than ~1e-4 dps²/PWM means a reset() snuck in.
    TEST_ASSERT(std::fabs(k_after_run_entry - k_measured_by_bootstrap) < 1e-4f,
                "K in plant_id after RUN entry == K measured by BOOTSTRAP "
                "(no clobber by plant_id_.reset())");

    // And as a sanity belt-and-suspenders check vs. the synthetic plant's
    // truth — the value must still be in the ±25 % band that Test 3 uses.
    TEST_ASSERT(std::fabs(k_after_run_entry - K_TRUE) <= 0.25f * K_TRUE,
                "K in plant_id after RUN entry within ±25 % of K_TRUE");

    // Tick a few more ms of quiet RUN and confirm K stays preserved. If
    // reset() had run, P_ would be 1.0 here and early-RUN noise would already
    // be dragging K toward something else; with the fix, P_=SEED_P=0.05 and K
    // stays nailed to the seeded value during the bootstrap-freeze window.
    f.imu.set_gyro_y(0.0f);
    f.imu.set_pitch(0.0f);
    uint32_t t = 3000;
    for (int i = 0; i < 40; ++i) {     // 200 ms of post-RUN ticks
        f.app.step(t);
        t += 5;
    }
    const float k_after_run_200ms = f.plant_id.get_k_motor();
    printf("  K_in_plant_id_200ms_into_run = %.4f\n", k_after_run_200ms);
    TEST_ASSERT(std::fabs(k_after_run_200ms - k_measured_by_bootstrap) < 1e-3f,
                "K still preserved 200 ms into RUN (bootstrap-freeze window)");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    printf("==============================================\n");
    printf("BOOTSTRAP K-preservation regression (P1-COV-1)\n");
    printf("==============================================\n");

    test_bootstrap_k_preserved_into_run();

    printf("\n==============================================\n");
    printf("Tests run: %d, passed: %d, failed: %d\n",
           tests_run, tests_passed, tests_failed);
    printf("==============================================\n");
    return (tests_failed == 0) ? 0 : 1;
}
