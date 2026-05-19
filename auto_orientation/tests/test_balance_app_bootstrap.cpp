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
    // is 300 ticks; allow extra headroom. After Fix D (2026-05-19) cooldown
    // extended 150→400 ms so total bootstrap is now 300 + 4×(150+400) = 2500
    // ms = 500 ticks at 5 ms; allow extra headroom.
    advance_ticks(f, 2000, 5, 560);

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
    // After Fix D (2026-05-19) cooldown is 400 ms (was 150).
    const uint16_t BASELINE_MS = 300;
    const uint16_t PULSE_MS    = 150;
    const uint16_t COOLDOWN_MS = 400;
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

    // Total bootstrap window: 300 + 4*(150+400) = 2500 ms = 500 ticks at 5 ms.
    // Add headroom for FINALISE / state transition.
    for (int i = 0; i < 560; ++i) {
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
                // Cooldown: hold the pulse peak for the first 10 ms (so the
                // BOOTSTRAP code samples Δω against the held value), then
                // exponentially decay to ~0 over the remaining 390 ms so
                // pulse N+1 starts at rest. Without this decay, Fix C's
                // |g0| < 5 dps sample-quality gate skips every pulse after
                // the first. τ = 80 ms gives ~99% decay in 400 ms.
                const float cool_ms = (float)(cycle_pos - PULSE_MS);
                if (cool_ms < 10.0f) {
                    f.imu.set_gyro_y(pulse_final_gyro_dps);
                } else {
                    const float settle_ms = cool_ms - 10.0f;
                    const float tau_ms = 80.0f;
                    const float decay = std::exp(-settle_ms / tau_ms);
                    f.imu.set_gyro_y(pulse_final_gyro_dps * decay);
                }
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

// (7) Fix B (audit P1-SM-1) — plant_id K_motor measured by BOOTSTRAP must
// survive RUN entry. Before the fix, RUN's enter_state_ side effect called
// plant_id_.reset(kp,kd) which back-derived K from Kp = ω_n²/K and reset the
// RLS covariance to INITIAL_P (1.0), erasing the just-measured K. After the
// fix, the guarded reset() skips when bootstrap_result_.converged is true so
// the seed_k_motor() value (P_=SEED_P=0.05) survives.
static void test_plant_id_preserved_on_run_entry() {
    printf("\nTest 7: Fix B — BOOTSTRAP K_motor survives RUN entry (no clobber)\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.plant_id.set_k_motor_bounds(0.05f, 2.0f);
    f.plant_id.set_target_settling_time_sec(0.5f);

    const float K_TRUE = 0.5f;
    f.imu.set_pitch(0.0f);
    f.imu.set_gyro_y(0.0f);
    f.app.enter_bootstrap(0);

    // Reuse the synthetic-plant driver from Test 3 (same constants).
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
    for (int i = 0; i < 560; ++i) {
        t = 5u * (uint32_t)(i + 1);
        const uint16_t elapsed = (uint16_t)t;
        if (elapsed >= BASELINE_MS && elapsed < BASELINE_MS + 4 * (PULSE_MS + COOLDOWN_MS)) {
            const uint16_t since_baseline = elapsed - BASELINE_MS;
            const int      cur_pulse      = since_baseline / (PULSE_MS + COOLDOWN_MS);
            const uint16_t cycle_pos      = since_baseline - (uint16_t)cur_pulse * (PULSE_MS + COOLDOWN_MS);
            const int16_t  pwm_total      = signed_pulse_pwm_total(cur_pulse);
            if (cycle_pos < PULSE_MS) {
                if (last_pulse_idx != cur_pulse) {
                    last_pulse_idx = cur_pulse;
                    pulse_t0_ms = elapsed;
                }
                const float dt_pulse_sec = ((float)elapsed - (float)pulse_t0_ms) * 0.001f;
                f.imu.set_gyro_y(K_TRUE * (float)pwm_total * dt_pulse_sec);
                pulse_final_gyro_dps = K_TRUE * (float)pwm_total * ((float)PULSE_MS * 0.001f);
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

    TEST_ASSERT(f.app.get_state() == BalanceAppState::RUN,
                "BOOTSTRAP succeeded and transitioned to RUN");

    // The plant identifier should still report K_motor close to K_TRUE
    // immediately after RUN entry. If reset() had run, K would have been
    // back-derived from Kp (which equals ω_n²/K_seed) and rounded through
    // float, but more importantly P_ would have inflated to INITIAL_P=1.0
    // making early adaptation noisy. Snapshot K immediately.
    const float k_after_run = f.plant_id.get_k_motor();
    printf("  K_TRUE=%.3f  K_after_run_entry=%.3f\n", K_TRUE, k_after_run);

    // K should still be within tight bounds of K_TRUE (RLS is frozen during
    // BOOTSTRAP freeze window, and we just seeded it, so nothing perturbs).
    TEST_ASSERT(std::fabs(k_after_run - K_TRUE) < 0.10f,
                "K_motor preserved within ±0.10 of measured K after RUN entry");

    // Tick the app a few times through early RUN (synthetic plant goes quiet)
    // and confirm K doesn't drift away from the measurement. The previous
    // bug would have allowed early noise to dominate immediately.
    f.imu.set_gyro_y(0.0f);
    f.imu.set_pitch(0.0f);
    for (int i = 0; i < 40; ++i) {       // 200 ms at 5 ms ticks
        f.app.step(t);
        t += 5;
    }
    const float k_after_run_200ms = f.plant_id.get_k_motor();
    printf("  K_after_200ms_RUN=%.3f\n", k_after_run_200ms);
    TEST_ASSERT(std::fabs(k_after_run_200ms - K_TRUE) < 0.10f,
                "K_motor still preserved 200 ms into RUN");
}

// (8) Fix C — pulses with |g0| > 5 dps are skipped from the K estimate.
// Bench 2026-05-18 PM late: g0 values -0.1, -25.5, +20.3, +1.9 produced
// K spread 0.09-0.74 (8× range). The g0-quality gate skips contaminated
// pulses; the K mean comes only from clean ones.
static void test_bootstrap_g0_sample_quality_gate() {
    printf("\nTest 8: Fix C — pulses with |g0| > 5 dps are skipped\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.plant_id.set_k_motor_bounds(0.05f, 2.0f);

    const float K_TRUE = 0.4f;
    f.imu.set_pitch(0.0f);
    f.imu.set_gyro_y(0.0f);
    f.app.enter_bootstrap(0);

    // Synthetic plant: same as Test 3 but the cooldown HOLDS the peak gyro
    // (no decay) so every pulse N>0 starts with |g0| > 5 dps. After the
    // first pulse there's a real Δω during pulses 1-3 (synthesized), but
    // Fix C should skip pulses 1-3 because g0 starts at a held peak (e.g.
    // pulse 0 leaves gyro at ≈21.6 dps which is way over 5).
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
    for (int i = 0; i < 560; ++i) {
        t = 5u * (uint32_t)(i + 1);
        const uint16_t elapsed = (uint16_t)t;
        if (elapsed >= BASELINE_MS && elapsed < BASELINE_MS + 4 * (PULSE_MS + COOLDOWN_MS)) {
            const uint16_t since_baseline = elapsed - BASELINE_MS;
            const int      cur_pulse      = since_baseline / (PULSE_MS + COOLDOWN_MS);
            const uint16_t cycle_pos      = since_baseline - (uint16_t)cur_pulse * (PULSE_MS + COOLDOWN_MS);
            const int16_t  pwm_total      = signed_pulse_pwm_total(cur_pulse);
            if (cycle_pos < PULSE_MS) {
                if (last_pulse_idx != cur_pulse) {
                    last_pulse_idx = cur_pulse;
                    pulse_t0_ms = elapsed;
                }
                const float dt_pulse_sec = ((float)elapsed - (float)pulse_t0_ms) * 0.001f;
                // Start each pulse from the held value to simulate prior-pulse
                // momentum that has not decayed (this is what bench 2026-05-18
                // PM late saw with 150 ms cooldown).
                f.imu.set_gyro_y(pulse_final_gyro_dps
                                 + K_TRUE * (float)pwm_total * dt_pulse_sec);
                pulse_final_gyro_dps += K_TRUE * (float)pwm_total
                                        * ((float)PULSE_MS * 0.001f);
            } else {
                // Cooldown HOLDS at peak (no settling).
                f.imu.set_gyro_y(pulse_final_gyro_dps);
            }
        } else {
            f.imu.set_gyro_y(0.0f);
        }
        f.app.step(t);
    }

    const BootstrapResult& r = f.app.get_bootstrap_result();
    printf("  pulses_valid=%u/%u  (expect 1 — only pulse 0 starts clean)\n",
           (unsigned)r.pulses_valid, (unsigned)r.pulses_total);
    // We expect ~1 valid pulse out of 4 — only pulse 0 starts with g0≈0.
    // The N_PULSES/2 = 2 floor will then fail BOOTSTRAP (no_response).
    TEST_ASSERT(r.pulses_valid <= 1,
                "g0 gate skipped contaminated pulses (≤1 valid out of 4)");
    TEST_ASSERT(f.app.get_state() == BalanceAppState::IDLE,
                "BOOTSTRAP bails to IDLE when too few clean pulses");
    TEST_ASSERT(r.failure_reason == 2,
                "failure_reason = no_response (only 1 clean sample, need 2)");
}

// (9) Fix E — operator handling during baseline produces failure_reason=6
// (baseline_noisy), not silent no_response or threshold-balloon-then-fail.
static void test_bootstrap_baseline_noisy() {
    printf("\nTest 9: Fix E — noisy baseline → failure_reason=6 (baseline_noisy)\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);

    f.imu.set_pitch(0.0f);
    f.app.enter_bootstrap(0);

    // Drive baseline with significant gyro variance — simulate operator
    // touching/placing the bot. Peak-to-peak ~30 dps, well over the 5 dps
    // BOOTSTRAP_NOISE_FLOOR_MAX_DPS cap.
    uint32_t t = 0;
    for (int i = 0; i < 60; ++i) {           // 300 ms baseline at 5 ms ticks
        t = 5u * (uint32_t)(i + 1);
        // Oscillate gyro with peak-to-peak 30 dps
        const float gy = (i & 1) ? 15.0f : -15.0f;
        f.imu.set_gyro_y(gy);
        f.app.step(t);
    }
    // One more tick past BASELINE_MS to trigger the check
    t += 5;
    f.imu.set_gyro_y(0.0f);
    f.app.step(t);

    TEST_ASSERT(f.app.get_state() == BalanceAppState::IDLE,
                "noisy baseline → IDLE");
    TEST_ASSERT(f.app.get_bootstrap_result().failure_reason == 6,
                "failure_reason = baseline_noisy (6)");
    TEST_ASSERT(!f.app.get_bootstrap_result().converged,
                "converged=false on baseline_noisy");
    TEST_ASSERT(f.motors.last_left() == 0,
                "motors stopped after baseline_noisy abort");
}

// (10) Fix A — atomic-block protection for raw_gyro_dps_ / pitch_deg_ writes.
// On native (non-AVR) ATOMIC_BLOCK is a no-op, so this test verifies the
// read/write behavior is preserved (no off-by-one bugs from the refactor):
// after step_run_ has happened the pitch_deg_ getter and the gyro index 1
// match what the IMU mock returned.
static void test_atomic_block_pitch_and_gyro_consistency() {
    printf("\nTest 10: Fix A — pitch_deg_ + raw_gyro_dps_ writes round-trip correctly\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 0);

    f.imu.set_pitch(1.23f);
    f.imu.set_gyro_y(7.5f);
    f.app.step(5);   // one tick — reads sensors, ticks state machine
    TEST_ASSERT(std::fabs(f.app.get_pitch_deg() - 1.23f) < 1e-4f,
                "pitch_deg_ round-trips through atomic write");

    // Tick a few times and confirm pitch tracks IMU mutations through
    // multiple ATOMIC_BLOCK writes.
    f.imu.set_pitch(-2.5f);
    for (int i = 0; i < 5; ++i) f.app.step(10 + (uint32_t)i * 5);
    TEST_ASSERT(std::fabs(f.app.get_pitch_deg() + 2.5f) < 1e-4f,
                "pitch_deg_ tracks IMU after 5 ATOMIC_BLOCK cycles");

    f.imu.set_pitch(0.0f);
    f.imu.set_gyro_y(-12.0f);
    f.app.step(50);
    // No direct accessor for raw_gyro_dps_[1], but the BOOTSTRAP entry seeds
    // bs_prev_gyro_ from raw_gyro_dps_[1] — exercise that path indirectly by
    // entering bootstrap and confirming it doesn't immediately fail with a
    // garbage value (which it would if the atomic refactor torn-wrote NaN).
    f.app.enter_bootstrap(55);
    f.app.step(60);
    TEST_ASSERT(f.app.get_state() == BalanceAppState::BOOTSTRAP,
                "raw_gyro_dps_[1] write→read coherent (BOOTSTRAP did not bail)");
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
    test_plant_id_preserved_on_run_entry();
    test_bootstrap_g0_sample_quality_gate();
    test_bootstrap_baseline_noisy();
    test_atomic_block_pitch_and_gyro_consistency();

    printf("\n--- Summary ---\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
