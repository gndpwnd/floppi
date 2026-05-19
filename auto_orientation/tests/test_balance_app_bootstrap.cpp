/**
 * Native unit tests for the BOOTSTRAP state (Phase 4.10c).
 *
 * BOOTSTRAP measures K_motor from controlled ±PWM pulses, derives Kp/Kd/Ki
 * via pole-placement (Kp = ω_n²/K, Kd = 2ζω_n/K), pushes the result to the
 * PID, and enters RUN already tuned. This file pins the contract:
 *
 *  1. Entry guard — bot must be approximately upright (|pitch| < 10°) when
 *     BOOTSTRAP starts. If not, fail with reason=pitch_out_of_range and
 *     return to IDLE without ever firing the motors.
 *  2. Disconnected motors — pulses produce no gyro response → fail with
 *     reason=no_response. Bot returns to IDLE with last_output=0 throughout.
 *  3. Successful identification — synthetic plant produces gyro Δω = K·pwm·dt
 *     during each pulse. After 4 pulses the measured K should land within
 *     ~25% of K_true, derived Kp = ω_n²/K_est should be sane, and the app
 *     should land in RUN with PID actually re-tuned.
 *  4. CAPTURE_MOUNTING auto-chains to BOOTSTRAP on success (not IDLE).
 *  5. Long-press in IDLE enters BOOTSTRAP (was AUTO_TUNE before Phase 4.10c).
 *  6. Operator abort during BOOTSTRAP returns to IDLE with motors stopped.
 *
 * Compile:
 *   g++ -std=c++11 -O2 -DUNIT_TEST \
 *       -o tests/test_balance_app_bootstrap \
 *       tests/test_balance_app_bootstrap.cpp \
 *       src/applications/balancing_robot/balance_app.cpp \
 *       src/applications/balancing_robot/safety.cpp \
 *       src/control/pid_controller.cpp \
 *       src/control/auto_pid_tuner.cpp \
 *       src/control/plant_identifier.cpp \
 *       src/navigation/mounting_calibration.cpp \
 *       src/navigation/online_mounting_estimator.cpp
 *   ./tests/test_balance_app_bootstrap
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
// Mocks
// ============================================================================

// GyroIMU — extends the base OrientationSensor with programmable pitch +
// programmable raw gyro Y. The bootstrap measurement reads
// raw_gyro_dps_[1] each tick; tests use set_gyro_y() to script the synthetic
// plant's instantaneous angular rate.
class GyroIMU : public OrientationSensor {
public:
    GyroIMU() : initialized_(true) {
        od_.pitch_deg = 0.0f; od_.roll_deg = 0.0f; od_.yaw_deg = 0.0f;
        od_.w = 1.0f; od_.x = 0.0f; od_.y = 0.0f; od_.z = 0.0f;
        gyro_y_ = 0.0f;
    }
    void set_pitch(float deg) { od_.pitch_deg = deg; }
    void set_gyro_y(float dps) { gyro_y_ = dps; }

    // Sensor base
    bool begin() override { initialized_ = true; return true; }
    void end() override { initialized_ = false; }
    bool isInitialized() const override { return initialized_; }
    bool read() override { return true; }
    bool hasNewData() const override { return true; }
    const char* name() const override { return "GyroIMU"; }
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

// Advance the app's clock through `n_ticks` step()s, each `dt_ms` apart.
// Returns the final time-millis value handed to step().
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

// (1) BOOTSTRAP refuses to start if the bot isn't upright.
static void test_bootstrap_pitch_out_of_range() {
    printf("\nTest 1: BOOTSTRAP entry guard — pitch > 10° → IDLE no-fire\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);

    // Tilt the bot to 12° (outside the ±10° entry envelope).
    f.imu.set_pitch(12.0f);
    f.imu.set_gyro_y(0.0f);

    f.app.enter_bootstrap(2000);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::BOOTSTRAP,
                "entered BOOTSTRAP on enter_bootstrap()");

    // First step should detect pitch_out_of_range and bail.
    f.app.step(2005);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::IDLE,
                "BOOTSTRAP exits to IDLE on first tick when pitch>10°");
    TEST_ASSERT(f.app.get_bootstrap_result().failure_reason == 1,
                "failure_reason = pitch_out_of_range (1)");
    TEST_ASSERT(!f.app.get_bootstrap_result().converged,
                "converged=false on pitch-out-of-range exit");
    TEST_ASSERT(f.motors.last_left() == 0,
                "motors never fired during failed bootstrap");
}

// (2) Disconnected motors — pulses produce no gyro response.
static void test_bootstrap_no_response() {
    printf("\nTest 2: BOOTSTRAP no-response (disconnected motors) → IDLE fail\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);

    // Pitch=0 (entry guard OK), gyro stays at 0 regardless of pulses.
    f.imu.set_pitch(0.0f);
    f.imu.set_gyro_y(0.0f);

    f.app.enter_bootstrap(2000);
    // Drive through the entire bootstrap window (1500+ ms). At 5 ms/tick that
    // is 300 ticks; allow extra headroom.
    advance_ticks(f, 2000, 5, 340);

    TEST_ASSERT(f.app.get_state() == BalanceAppState::IDLE,
                "bootstrap with no gyro response exits to IDLE");
    const BootstrapResult& r = f.app.get_bootstrap_result();
    TEST_ASSERT(r.failure_reason == 2,
                "failure_reason = no_response (2)");
    TEST_ASSERT(!r.converged, "converged=false on no_response");
    TEST_ASSERT(r.pulses_valid == 0,
                "pulses_valid=0 when no gyro response detected");
    TEST_ASSERT(r.pulses_total == 4,
                "pulses_total=4 (all four attempted)");
}

// (3) Successful identification with a synthetic plant.
//
// The synthetic plant produces gyro_y proportional to motor command:
//   gyro_y(t) = K_TRUE × pwm_total × elapsed_in_pulse_sec
// which approximates α = K·pwm × t for a frictionless pendulum near zero pitch.
// After PULSE_MS=100ms, Δgyro = K × pwm_total × 0.1 → K_est = K_TRUE.
static void test_bootstrap_success() {
    printf("\nTest 3: BOOTSTRAP measures K_motor, derives gains, enters RUN\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    // Tighten plant_id bounds so the test can verify K_est is reasonable.
    f.plant_id.set_k_motor_bounds(0.05f, 2.0f);
    f.plant_id.set_target_settling_time_sec(0.5f);
    f.plant_id.set_damping_ratio(0.7f);

    const float K_TRUE = 0.4f;   // deg/s² per PWM unit

    f.imu.set_pitch(0.0f);
    f.imu.set_gyro_y(0.0f);
    f.app.enter_bootstrap(0);

    // Walk the simulator at 5ms ticks through the entire 1100 ms bootstrap.
    // The bootstrap state machine drives motors at known signed PWM during
    // each pulse window; the test synthesises gyro_y the way a frictionless
    // pendulum near zero pitch would respond:
    //
    //     gyro_y(t) = ω_pulse_start + K_TRUE × pwm_total × (t − pulse_start)
    //
    // The test mirrors step_bootstrap_'s phase calendar — same BASELINE_MS,
    // PULSE_MS, COOLDOWN_MS, and per-pulse magnitude/sign table.
    const uint16_t BASELINE_MS = 300;
    const uint16_t PULSE_MS    = 150;
    const uint16_t COOLDOWN_MS = 150;
    const uint8_t  PULSE_PWMS_PER_WHEEL[4] = {180, 180, 240, 240};
    // Signs match step_bootstrap_'s alternation: even idx +, odd idx −.
    auto signed_pulse_pwm_total = [&](int idx) -> int16_t {
        const int16_t mag = (int16_t)(2 * PULSE_PWMS_PER_WHEEL[idx]);
        return (idx & 1) ? -mag : mag;
    };

    uint32_t t = 0;
    int last_pulse_idx = -1;
    uint32_t pulse_t0_ms = 0;
    float    pulse_final_gyro_dps = 0.0f;

    // Total bootstrap window: 300 + 4*(150+150) = 1500 ms = 300 ticks at 5 ms.
    // Add headroom for FINALISE / state transition.
    for (int i = 0; i < 340; ++i) {
        t = 5u * (uint32_t)(i + 1);
        const uint16_t elapsed = (uint16_t)t;

        if (elapsed >= BASELINE_MS && elapsed < BASELINE_MS + 4 * (PULSE_MS + COOLDOWN_MS)) {
            const uint16_t since_baseline = elapsed - BASELINE_MS;
            const int      cur_pulse      = since_baseline / (PULSE_MS + COOLDOWN_MS);
            const uint16_t cycle_pos      = since_baseline - (uint16_t)cur_pulse * (PULSE_MS + COOLDOWN_MS);
            const int16_t  pwm_total      = signed_pulse_pwm_total(cur_pulse);

            if (cycle_pos < PULSE_MS) {
                // Pulse window — synthesize ramping gyro proportional to commanded PWM.
                if (last_pulse_idx != cur_pulse) {
                    last_pulse_idx = cur_pulse;
                    pulse_t0_ms    = elapsed;
                }
                const float dt_pulse_sec = ((float)elapsed - (float)pulse_t0_ms) * 0.001f;
                const float synthesized  = K_TRUE * (float)pwm_total * dt_pulse_sec;
                f.imu.set_gyro_y(synthesized);
                // Record where the pulse will end so the cooldown branch can hold it.
                const float pulse_dur_sec = (float)PULSE_MS * 0.001f;
                pulse_final_gyro_dps = K_TRUE * (float)pwm_total * pulse_dur_sec;
            } else {
                // Cooldown: gyro holds at the peak the pulse achieved. Bootstrap
                // samples gyro at first cooldown tick to compute Δω → K_i.
                f.imu.set_gyro_y(pulse_final_gyro_dps);
            }
        } else {
            f.imu.set_gyro_y(0.0f);
        }
        f.app.step(t);
    }

    const BalanceAppState st = f.app.get_state();
    TEST_ASSERT(st == BalanceAppState::RUN,
                "bootstrap with synthetic plant transitions to RUN");

    const BootstrapResult& r = f.app.get_bootstrap_result();
    printf("  K_TRUE=%.3f  K_est=%.3f  Kp=%.2f Kd=%.2f Ki=%.2f  valid=%u/%u\n",
           K_TRUE, r.k_motor, r.derived_kp, r.derived_kd, r.derived_ki,
           (unsigned)r.pulses_valid, (unsigned)r.pulses_total);

    TEST_ASSERT(r.converged, "BootstrapResult.converged = true");
    TEST_ASSERT(r.failure_reason == 0, "failure_reason = ok (0)");
    TEST_ASSERT(r.pulses_valid >= 2,
                "at least half the pulses produced detectable response");

    // K within ±25% of truth (matches RLS convergence tolerance pattern).
    TEST_ASSERT(std::fabs(r.k_motor - K_TRUE) <= 0.25f * K_TRUE,
                "K_est within ±25% of K_true");

    // Derived gains follow pole-placement formulas. ts=0.5 → ω_n=8.
    // Kp = ω_n²/K_est = 64/K_est. With K_est ≈ 0.4 → Kp ≈ 160.
    // Verify positivity + ballpark; tighter checks live in plant_identifier tests.
    TEST_ASSERT(r.derived_kp > 0.0f, "derived_kp > 0");
    TEST_ASSERT(r.derived_kd > 0.0f, "derived_kd > 0");
    TEST_ASSERT(r.derived_ki > 0.0f, "derived_ki > 0");

    // The PID was actually re-tuned — pulled the derived values, not the
    // (0,0,0) constructor defaults.
    float kp_live, ki_live, kd_live;
    f.pid.get_tunings(kp_live, ki_live, kd_live);
    TEST_ASSERT(kp_live > 0.0f, "PID.Kp pushed from BOOTSTRAP > 0");
    TEST_ASSERT(kd_live > 0.0f, "PID.Kd pushed from BOOTSTRAP > 0");
}

// (4) CAPTURE_MOUNTING success → BOOTSTRAP (not IDLE).
static void test_capture_chains_to_bootstrap() {
    printf("\nTest 4: CAPTURE_MOUNTING success auto-chains to BOOTSTRAP\n");
    Fixture f;
    BalanceAppConfig cfg = BalanceApp::default_config();
    cfg.capture_duration_ms = 100;       // short window
    cfg.capture_pitch_var_deg = 0.5f;
    f.app.begin(cfg, 0);

    f.app.on_short_press(0);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::CAPTURE_MOUNTING,
                "short-press IDLE → CAPTURE_MOUNTING");

    // Feed stable pitch=0 so the capture variance gate passes.
    f.imu.set_pitch(0.0f);
    f.imu.set_gyro_y(0.0f);
    bool reached_bootstrap = false;
    for (int i = 0; i < 30; ++i) {
        f.app.step(10u + (uint32_t)i * 10u);
        if (f.app.get_state() == BalanceAppState::BOOTSTRAP) {
            reached_bootstrap = true;
            break;
        }
    }
    TEST_ASSERT(reached_bootstrap,
                "stable capture → BOOTSTRAP (not IDLE, not AUTO_TUNE)");
}

// (5) Long-press in IDLE enters BOOTSTRAP.
static void test_long_press_idle_to_bootstrap() {
    printf("\nTest 5: long-press in IDLE → BOOTSTRAP (was AUTO_TUNE pre-4.10c)\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.imu.set_pitch(0.0f);

    f.app.on_long_press(1100);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::BOOTSTRAP,
                "long-press in IDLE → BOOTSTRAP");
}

// (6) Operator abort during BOOTSTRAP returns to IDLE with motors stopped.
static void test_bootstrap_user_abort() {
    printf("\nTest 6: abort during BOOTSTRAP → IDLE with motors stopped\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.imu.set_pitch(0.0f);
    f.imu.set_gyro_y(0.0f);

    f.app.enter_bootstrap(2000);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::BOOTSTRAP, "entered BOOTSTRAP");

    // Run a few ticks into the baseline window, then abort.
    advance_ticks(f, 2000, 5, 10);   // 50ms in (still in baseline)
    f.safety.request_abort();
    f.app.step(2055);

    TEST_ASSERT(f.app.get_state() == BalanceAppState::IDLE,
                "abort during BOOTSTRAP → IDLE");
    TEST_ASSERT(f.app.get_bootstrap_result().failure_reason == 4,
                "failure_reason = user_abort (4)");
    TEST_ASSERT(f.motors.last_left() == 0,
                "motors stopped after abort");
}

// ============================================================================
// main
// ============================================================================

int main() {
    printf("BalanceApp BOOTSTRAP unit tests (Phase 4.10c)\n");
    printf("=============================================\n");

    test_bootstrap_pitch_out_of_range();
    test_bootstrap_no_response();
    test_bootstrap_success();
    test_capture_chains_to_bootstrap();
    test_long_press_idle_to_bootstrap();
    test_bootstrap_user_abort();

    printf("\n--- Summary ---\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
