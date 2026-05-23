/**
 * Noise-floor estimator tests.
 *
 * Covers two layers:
 *
 *   A. NoiseFloorEstimator (header-only, noise_floor_estimator.h) in isolation:
 *      feed known-variance sample sets, assert the computed std-dev matches the
 *      analytic value, assert settled() latches at exactly kWindowSamples,
 *      assert post-settle pushes are ignored, and assert abort_window() discards
 *      a partial accumulation but preserves a latched floor.
 *
 *   B. BalanceApp integration: a still IMU drives both estimators to settle
 *      during the IDLE quiet window; a noisy/moving IMU prevents settling; and
 *      the captured gyro/accel floors match the injected sample statistics.
 *
 * This is a PURE-MEASUREMENT layer — it exists to unblock the noise-floor-
 * derived scope violations (#4 STUCK_GYRO_DPS, #7 ext_motion gyro, #9 HELD lift
 * gate, the HELD→RUN quiet gate, #8 HELD dwell sigma) called out in
 * docs/findings/mega_scope_violation_triage_2026-05-22.md §"doubly blocked".
 * It must NOT change any control behaviour; these tests therefore only inspect
 * getters, never assert on motor output / gains / state transitions caused by
 * the noise floor.
 *
 * Compile (from auto_orientation/):
 *
 *   g++ -std=c++11 -O2 -fpermissive -DUNIT_TEST \
 *       -o tests/test_noise_floor_estimator \
 *       tests/test_noise_floor_estimator.cpp \
 *       src/applications/balancing_robot/balance_app.cpp \
 *       src/applications/balancing_robot/safety.cpp \
 *       src/control/pid_controller.cpp \
 *       src/control/auto_pid_tuner.cpp \
 *       src/control/plant_identifier.cpp \
 *       src/navigation/mounting_calibration.cpp \
 *       src/navigation/online_mounting_estimator.cpp
 *   ./tests/test_noise_floor_estimator
 */

#include <cstdio>
#include <cstdint>
#include <cmath>

#include "../src/applications/balancing_robot/noise_floor_estimator.h"
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

static bool approx(float a, float b, float tol) {
    float d = a - b;
    if (d < 0.0f) d = -d;
    return d <= tol;
}

// ============================================================================
// A. NoiseFloorEstimator in isolation
// ============================================================================

// (1) A constant signal has exactly zero variance → std_dev == 0, settled true.
static void test_constant_signal_zero_std() {
    printf("\nTest 1: constant signal → std_dev 0, settles at kWindowSamples\n");
    NoiseFloorEstimator e;
    bool premature_settle = false;
    for (uint16_t i = 0; i < NoiseFloorEstimator::kWindowSamples; ++i) {
        if (e.settled()) premature_settle = true;  // must not settle early
        e.push(2.5f);
    }
    TEST_ASSERT(!premature_settle, "does not settle before the window fills");
    TEST_ASSERT(e.settled(), "settled() latches after exactly kWindowSamples");
    TEST_ASSERT(approx(e.std_dev(), 0.0f, 1e-4f), "constant signal std_dev == 0");
    TEST_ASSERT(approx(e.mean(), 2.5f, 1e-4f), "mean equals the constant value");
}

// (2) Two-level square wave: alternating ±A around mean M has population
//     variance A². For a large even window the sample variance (n-1) is very
//     close to A², so std_dev ≈ A.
static void test_alternating_known_std() {
    printf("\nTest 2: alternating +/-A signal → std_dev ~= A\n");
    NoiseFloorEstimator e;
    const float M = 1.0f;
    const float A = 0.5f;
    for (uint16_t i = 0; i < NoiseFloorEstimator::kWindowSamples; ++i) {
        e.push((i & 1) ? (M + A) : (M - A));
    }
    TEST_ASSERT(e.settled(), "settles after the window");
    // Analytic sample std-dev for an even split of n samples at ±A:
    //   variance = n/(n-1) * A²  → std = A * sqrt(n/(n-1))  ≈ A for n=200.
    const float n = (float)NoiseFloorEstimator::kWindowSamples;
    const float expected = A * sqrtf(n / (n - 1.0f));
    TEST_ASSERT(approx(e.std_dev(), expected, 1e-3f),
                "alternating-signal std_dev matches analytic value");
    TEST_ASSERT(approx(e.mean(), M, 1e-3f), "mean is the midpoint");
}

// (3) Post-settle pushes are ignored — the latched floor cannot be corrupted
//     by later large samples.
static void test_post_settle_ignored() {
    printf("\nTest 3: pushes after settle are ignored (latch is immutable)\n");
    NoiseFloorEstimator e;
    for (uint16_t i = 0; i < NoiseFloorEstimator::kWindowSamples; ++i) e.push(0.0f);
    const float settled_std = e.std_dev();
    for (int i = 0; i < 50; ++i) e.push(1000.0f);  // huge motion AFTER settle
    TEST_ASSERT(approx(e.std_dev(), settled_std, 1e-6f),
                "std_dev frozen at settle value despite later huge samples");
    TEST_ASSERT(e.sample_count() == NoiseFloorEstimator::kWindowSamples,
                "sample_count does not advance past kWindowSamples");
}

// (4) abort_window() discards a partial (unsettled) accumulation; a fresh full
//     quiet window then settles correctly.
static void test_abort_window_discards_partial() {
    printf("\nTest 4: abort_window discards partial, fresh window still settles\n");
    NoiseFloorEstimator e;
    // Half a window of contaminated samples...
    for (uint16_t i = 0; i < NoiseFloorEstimator::kWindowSamples / 2; ++i) e.push(99.0f);
    e.abort_window();
    TEST_ASSERT(e.sample_count() == 0, "abort_window resets the sample count");
    TEST_ASSERT(!e.settled(), "abort_window leaves the estimator unsettled");
    // ...then a clean full window of a constant signal.
    for (uint16_t i = 0; i < NoiseFloorEstimator::kWindowSamples; ++i) e.push(3.0f);
    TEST_ASSERT(e.settled(), "fresh full window settles");
    TEST_ASSERT(approx(e.std_dev(), 0.0f, 1e-4f),
                "captured floor reflects only the clean window (std 0)");
    TEST_ASSERT(approx(e.mean(), 3.0f, 1e-4f),
                "captured mean reflects only the clean window");
}

// (5) abort_window() does NOT clear an already-latched floor.
static void test_abort_preserves_latched() {
    printf("\nTest 5: abort_window after settle preserves the latched floor\n");
    NoiseFloorEstimator e;
    for (uint16_t i = 0; i < NoiseFloorEstimator::kWindowSamples; ++i) e.push(5.0f);
    TEST_ASSERT(e.settled(), "settled before abort");
    e.abort_window();
    TEST_ASSERT(e.settled(), "still settled after abort_window");
    TEST_ASSERT(approx(e.mean(), 5.0f, 1e-4f), "latched mean preserved");
}

// (6) reset() fully re-arms the estimator.
static void test_reset_rearms() {
    printf("\nTest 6: reset() re-arms a settled estimator\n");
    NoiseFloorEstimator e;
    for (uint16_t i = 0; i < NoiseFloorEstimator::kWindowSamples; ++i) e.push(7.0f);
    TEST_ASSERT(e.settled(), "settled before reset");
    e.reset();
    TEST_ASSERT(!e.settled(), "reset clears the latch");
    TEST_ASSERT(e.sample_count() == 0, "reset clears the count");
    TEST_ASSERT(approx(e.std_dev(), 0.0f, 1e-6f), "reset clears std_dev");
}

// (11) Audit P2-002/P2-003: a NaN sample is rejected (abort_window semantics)
//      rather than poisoning the Welford recurrence. A single NaN mid-window
//      discards the partial accumulation; a clean window after still settles to
//      the correct floor, and the latched std_dev is finite.
static void test_nan_sample_rejected() {
    printf("\nTest 11: NaN sample rejected (does not poison mean/m2/std_dev)\n");
    NoiseFloorEstimator e;
    for (uint16_t i = 0; i < 100; ++i) e.push(1.0f);      // partial window
    e.push(NAN);                                          // glitch
    TEST_ASSERT(e.sample_count() == 0,
                "NaN push aborts the in-progress window (count reset)");
    TEST_ASSERT(!e.settled(), "NaN push does not settle");
    // A clean full window after the glitch still produces a finite, correct floor.
    for (uint16_t i = 0; i < NoiseFloorEstimator::kWindowSamples; ++i) e.push(4.0f);
    TEST_ASSERT(e.settled(), "fresh clean window settles after a NaN glitch");
    TEST_ASSERT(isfinite(e.std_dev()), "latched std_dev is finite (not NaN)");
    TEST_ASSERT(approx(e.std_dev(), 0.0f, 1e-4f),
                "captured floor reflects only the clean window");
    TEST_ASSERT(approx(e.mean(), 4.0f, 1e-4f), "captured mean is finite/correct");
}

// (12) Audit P2-003: an Inf sample is rejected the same way (would otherwise
//      drive m2_ to +Inf and latch an Inf std_dev).
static void test_inf_sample_rejected() {
    printf("\nTest 12: Inf sample rejected (does not drive m2_/std_dev to Inf)\n");
    NoiseFloorEstimator e;
    for (uint16_t i = 0; i < 50; ++i) e.push(0.5f);
    e.push(INFINITY);
    TEST_ASSERT(e.sample_count() == 0, "Inf push aborts the in-progress window");
    e.push(-INFINITY);
    TEST_ASSERT(e.sample_count() == 0, "-Inf push aborts the in-progress window");
    for (uint16_t i = 0; i < NoiseFloorEstimator::kWindowSamples; ++i) e.push(2.0f);
    TEST_ASSERT(e.settled() && isfinite(e.std_dev()),
                "settles with a finite std_dev after Inf glitches");
}

// (13) Audit P2-002: an Inf/NaN push AFTER settle is still ignored (latch is
//      immutable and stays finite).
static void test_nonfinite_after_settle_ignored() {
    printf("\nTest 13: non-finite push after settle leaves latch finite\n");
    NoiseFloorEstimator e;
    for (uint16_t i = 0; i < NoiseFloorEstimator::kWindowSamples; ++i) e.push(1.0f);
    const float settled_std = e.std_dev();
    e.push(NAN);
    e.push(INFINITY);
    TEST_ASSERT(isfinite(e.std_dev()), "std_dev still finite after non-finite pushes");
    TEST_ASSERT(approx(e.std_dev(), settled_std, 1e-6f),
                "latched std_dev unchanged by post-settle non-finite pushes");
}

// ============================================================================
// B. BalanceApp integration — programmable IMU
// ============================================================================

// IMU whose raw gyro/accel can be set per tick so we can drive the noise-floor
// capture from the app's read_imu_() path. Pitch held at 0 so the bot reads
// "level"; lateral-gyro magnitude is sqrt(gx²+gz²) — keep gx/gz tiny for quiet.
class NoiseIMU : public OrientationSensor {
public:
    NoiseIMU() : initialized_(true) {
        od_.pitch_deg = 0.0f; od_.roll_deg = 0.0f; od_.yaw_deg = 0.0f;
        od_.w = 1.0f; od_.x = 0.0f; od_.y = 0.0f; od_.z = 0.0f;
        g_[0] = g_[1] = g_[2] = 0.0f;
        a_[0] = 0.0f; a_[1] = 0.0f; a_[2] = 9.81f;
    }
    void set_gyro(float x, float y, float z) { g_[0]=x; g_[1]=y; g_[2]=z; }
    void set_accel(float x, float y, float z) { a_[0]=x; a_[1]=y; a_[2]=z; }
    void set_pitch(float deg) { od_.pitch_deg = deg; }

    bool begin() override { initialized_ = true; return true; }
    void end() override { initialized_ = false; }
    bool isInitialized() const override { return initialized_; }
    bool read() override { return true; }
    bool hasNewData() const override { return true; }
    const char* name() const override { return "NoiseIMU"; }
    bool isHealthy() const override { return true; }
    const char* getStatusString() const override { return "OK"; }
    const OrientationData& getOrientation() const override { return od_; }
    bool setCalibrationProfile(const uint8_t*, uint16_t) override { return true; }
    bool getCalibrationProfile(uint8_t*, uint16_t*) override { return true; }
    bool getRawGyro(float xyz[3]) override { xyz[0]=g_[0]; xyz[1]=g_[1]; xyz[2]=g_[2]; return true; }
    bool getRawAccel(float xyz[3]) override { xyz[0]=a_[0]; xyz[1]=a_[1]; xyz[2]=a_[2]; return true; }
    bool getLinearAccel(float xyz[3]) override { xyz[0]=0.0f; xyz[1]=0.0f; xyz[2]=0.0f; return true; }
private:
    OrientationData od_;
    bool  initialized_;
    float g_[3];
    float a_[3];
};

class MockMotors : public DualMotorDriver {
public:
    MockMotors() : l_(0), r_(0) {}
    bool begin() override { return true; }
    void set_speeds(int16_t l, int16_t r) override { l_ = l; r_ = r; }
    void stop() override { l_ = 0; r_ = 0; }
    int16_t last_left() const override { return l_; }
    int16_t last_right() const override { return r_; }
private:
    int16_t l_, r_;
};

class NoOpStrategy : public ITuningStrategy {
public:
    void begin(float, float, float, uint32_t) override {}
    float step(float, const SafetyLimits&, uint32_t) override { return 0.0f; }
    bool is_done() const override { return true; }
    TuningResult get_result() const override { TuningResult r{}; r.failure_reason="noop"; return r; }
    const char* name() const override { return "noop"; }
};

struct Fixture {
    NoiseIMU                 imu;
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

// (7) A perfectly still bot in IDLE settles BOTH floors within kWindowSamples
//     quiet ticks, with near-zero captured noise.
static void test_app_still_idle_settles() {
    printf("\nTest 7: BalanceApp — still IDLE settles noise floor, ~0 noise\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.imu.set_gyro(0.0f, 0.0f, 0.0f);
    f.imu.set_accel(0.0f, 0.0f, 9.81f);

    TEST_ASSERT(!f.app.noise_floor_settled(), "not settled at start of IDLE");
    uint32_t t = 1000;
    for (uint16_t i = 0; i < NoiseFloorEstimator::kWindowSamples; ++i) {
        t += 5;
        f.app.step(t);
    }
    TEST_ASSERT(f.app.noise_floor_settled(),
                "still IDLE settles after kWindowSamples quiet ticks");
    TEST_ASSERT(approx(f.app.gyro_noise_floor(), 0.0f, 0.05f),
                "captured gyro floor ~0 for a still bot");
    TEST_ASSERT(approx(f.app.accel_noise_floor(), 0.0f, 0.05f),
                "captured accel floor ~0 for a still bot");
}

// (8) The captured gyro floor reflects the injected pitch-rate variance: an
//     alternating ±A pitch-rate (gyro Y) with quiet lateral axes settles to
//     std_dev ≈ A. This proves the app feeds the pitch-axis magnitude.
static void test_app_captures_injected_gyro_std() {
    printf("\nTest 8: BalanceApp — captured gyro floor matches injected std\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.imu.set_accel(0.0f, 0.0f, 9.81f);
    const float A = 0.4f;  // pitch-rate amplitude (well below the 3 dps quiet gate)
    uint32_t t = 1000;
    for (uint16_t i = 0; i < NoiseFloorEstimator::kWindowSamples; ++i) {
        // gyro Y alternates +/-A; lateral axes (x,z) zero → lateral mag 0 → quiet.
        // The estimator accumulates |gyro_y|, which is the CONSTANT A here, so
        // its std-dev is ~0 — that itself confirms the magnitude path. To get a
        // non-zero gyro floor we vary the magnitude instead:
        f.imu.set_gyro(0.0f, (i & 1) ? (1.0f + A) : (1.0f - A), 0.0f);
        t += 5;
        f.app.step(t);
    }
    TEST_ASSERT(f.app.noise_floor_settled(), "settles under small pitch-rate jitter");
    const float n = (float)NoiseFloorEstimator::kWindowSamples;
    const float expected = A * sqrtf(n / (n - 1.0f));
    TEST_ASSERT(approx(f.app.gyro_noise_floor(), expected, 1e-2f),
                "captured gyro floor matches injected magnitude std");
}

// (9) Lateral motion above the quiet gate prevents settling: the in-progress
//     window keeps getting aborted, so the floor never latches.
static void test_app_motion_prevents_settle() {
    printf("\nTest 9: BalanceApp — lateral motion prevents noise-floor settle\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.imu.set_accel(0.0f, 0.0f, 9.81f);
    // Large lateral-gyro magnitude (gx = 50 dps >> 3 dps quiet gate) every tick.
    f.imu.set_gyro(50.0f, 0.0f, 0.0f);
    uint32_t t = 1000;
    for (uint16_t i = 0; i < NoiseFloorEstimator::kWindowSamples * 2; ++i) {
        t += 5;
        f.app.step(t);
    }
    TEST_ASSERT(!f.app.noise_floor_settled(),
                "persistent lateral motion never settles the floor");
    TEST_ASSERT(f.app.noise_floor_progress() == 0,
                "aborted windows leave progress at 0 under continuous motion");
}

// (10) begin() re-arms the estimators (re-measure after a board reset).
static void test_app_begin_rearms() {
    printf("\nTest 10: BalanceApp — begin() re-arms the noise floor\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.imu.set_gyro(0.0f, 0.0f, 0.0f);
    f.imu.set_accel(0.0f, 0.0f, 9.81f);
    uint32_t t = 1000;
    for (uint16_t i = 0; i < NoiseFloorEstimator::kWindowSamples; ++i) { t += 5; f.app.step(t); }
    TEST_ASSERT(f.app.noise_floor_settled(), "settled before re-begin");
    f.app.begin(BalanceApp::default_config(), 50000);
    TEST_ASSERT(!f.app.noise_floor_settled(), "begin() clears the latched floor");
    TEST_ASSERT(f.app.noise_floor_progress() == 0, "begin() resets progress");
}

// (14) Audit P1-001: a non-finite IMU pitch is treated as a fault. read_imu_()
//      must NOT publish the NaN (pitch_deg_ holds the last known-good value)
//      and must request an abort so the loop drops to a safe state.
static void test_app_nan_pitch_failsafe() {
    printf("\nTest 14: BalanceApp — NaN IMU pitch holds last-good + requests abort\n");
    Fixture f;
    f.app.begin(BalanceApp::default_config(), 1000);
    f.imu.set_accel(0.0f, 0.0f, 9.81f);
    f.imu.set_gyro(0.0f, 0.0f, 0.0f);

    // Feed a few good ticks at a known pitch so last-good is well defined.
    f.imu.set_pitch(3.0f);
    uint32_t t = 1000;
    for (int i = 0; i < 5; ++i) { t += 5; f.app.step(t); }
    f.safety.clear_abort();
    TEST_ASSERT(approx(f.app.get_pitch_deg(), 3.0f, 1e-3f),
                "pitch tracks the good reading before the glitch");

    // Inject a NaN pitch.
    f.imu.set_pitch(NAN);
    t += 5; f.app.step(t);
    TEST_ASSERT(isfinite(f.app.get_pitch_deg()),
                "pitch_deg_ stays finite (NaN not published)");
    TEST_ASSERT(approx(f.app.get_pitch_deg(), 3.0f, 1e-3f),
                "pitch_deg_ holds the last known-good value on NaN read");
    TEST_ASSERT(f.safety.abort_requested(),
                "non-finite IMU pitch requests a failsafe abort");

    // Inject an Inf pitch — same fault handling.
    f.safety.clear_abort();
    f.imu.set_pitch(INFINITY);
    t += 5; f.app.step(t);
    TEST_ASSERT(isfinite(f.app.get_pitch_deg()),
                "pitch_deg_ stays finite on +Inf read");
    TEST_ASSERT(f.safety.abort_requested(),
                "+Inf IMU pitch requests a failsafe abort");
}

int main() {
    printf("BalanceApp NOISE-FLOOR estimator unit tests\n");
    printf("===========================================\n");

    test_constant_signal_zero_std();
    test_alternating_known_std();
    test_post_settle_ignored();
    test_abort_window_discards_partial();
    test_abort_preserves_latched();
    test_reset_rearms();
    test_nan_sample_rejected();
    test_inf_sample_rejected();
    test_nonfinite_after_settle_ignored();
    test_app_still_idle_settles();
    test_app_captures_injected_gyro_std();
    test_app_motion_prevents_settle();
    test_app_begin_rearms();
    test_app_nan_pitch_failsafe();

    printf("\n--- Summary ---\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
