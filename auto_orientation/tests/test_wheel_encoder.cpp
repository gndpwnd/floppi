/**
 * Native unit tests for WheelEncoder (Phase 4M.10).
 *
 * Mocks the underlying tick counter via set_ticks_for_test() — the PJRC
 * Encoder library is replaced at compile time (NATIVE_TEST guard in
 * wheel_encoder.cpp) so this test never tries to attach interrupts.
 *
 * Test coverage:
 *   1. Default construction state.
 *   2. CPR / radius getters and setters round-trip + reject non-positive.
 *   3. begin() succeeds (no-op on NATIVE_TEST) and is idempotent.
 *   4. read_ticks() reflects the mocked tick value.
 *   5. reset_ticks() zeros both ticks and velocity-window state.
 *   6. distance_m() = (ticks/cpr) * 2π * radius — one full revolution.
 *   7. distance_m() at 65 mm wheel matches expected circumference.
 *   8. read_velocity_dps() returns 0 on the priming call.
 *   9. read_velocity_dps() holds previous sample inside the window.
 *  10. read_velocity_dps() correct value after a full window elapses.
 *  11. read_velocity_dps() sign reverses for negative tick delta.
 *  12. read_velocity_mps() matches dps × radius × π/180.
 *  13. stalled(): returns false with no PWM report.
 *  14. stalled(): returns false when PWM > threshold but time not elapsed.
 *  15. stalled(): returns true when PWM > threshold, time elapsed, no motion.
 *  16. stalled(): returns false when wheel is moving above kStallVelocityDps.
 *  17. stalled(): resets timer when PWM drops below threshold.
 *
 * Build (PlatformIO):
 *   pio test -e native_test -f test_wheel_encoder
 *
 * Direct g++ smoke build (matches style of other native tests):
 *   g++ -std=c++11 -O2 -DNATIVE_TEST -Isrc \
 *       -o tests/test_wheel_encoder \
 *       tests/test_wheel_encoder.cpp \
 *       src/sensors/wheel_encoder.cpp \
 *       -lunity
 */

#include <unity.h>

#include <cmath>
#include <cstdint>

#include "../src/sensors/wheel_encoder.h"

// Mega encoder pins from the research doc. The pin numbers themselves don't
// matter on NATIVE_TEST builds (no interrupts attached) but using the real
// values keeps the test self-documenting.
static const uint8_t kPinA = 18;
static const uint8_t kPinB = 19;

static const float kEps = 1e-3f;
static const float kPi  = 3.14159265358979323846f;

void setUp(void)    {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// 1. Default construction state
// ---------------------------------------------------------------------------

void test_construction_defaults(void) {
  WheelEncoder enc(kPinA, kPinB);

  // PJRC Encoder isn't allocated until begin(); but read_ticks() should be
  // safe to call at any time and return 0 from the mock counter.
  TEST_ASSERT_EQUAL_INT32(0, enc.read_ticks());
  TEST_ASSERT_EQUAL_INT(WheelEncoder::kDefaultCPR, enc.counts_per_rev());
  TEST_ASSERT_FLOAT_WITHIN(kEps, WheelEncoder::kDefaultRadiusM,
                           enc.wheel_radius_m());
  TEST_ASSERT_EQUAL_UINT16(WheelEncoder::kDefaultVelocityWindowMs,
                           enc.velocity_window_ms());
}

// ---------------------------------------------------------------------------
// 2. Setters / getters
// ---------------------------------------------------------------------------

void test_setters_roundtrip_and_reject_non_positive(void) {
  WheelEncoder enc(kPinA, kPinB);

  enc.set_counts_per_rev(2800);
  TEST_ASSERT_EQUAL_INT(2800, enc.counts_per_rev());

  enc.set_wheel_radius_m(0.04f);
  TEST_ASSERT_FLOAT_WITHIN(kEps, 0.04f, enc.wheel_radius_m());

  enc.set_velocity_window_ms(50);
  TEST_ASSERT_EQUAL_UINT16(50, enc.velocity_window_ms());

  // Non-positive inputs fall back to defaults.
  enc.set_counts_per_rev(0);
  TEST_ASSERT_EQUAL_INT(WheelEncoder::kDefaultCPR, enc.counts_per_rev());

  enc.set_counts_per_rev(-7);
  TEST_ASSERT_EQUAL_INT(WheelEncoder::kDefaultCPR, enc.counts_per_rev());

  enc.set_wheel_radius_m(-0.01f);
  TEST_ASSERT_FLOAT_WITHIN(kEps, WheelEncoder::kDefaultRadiusM,
                           enc.wheel_radius_m());

  enc.set_velocity_window_ms(0);
  TEST_ASSERT_EQUAL_UINT16(WheelEncoder::kDefaultVelocityWindowMs,
                           enc.velocity_window_ms());
}

// ---------------------------------------------------------------------------
// 3. begin() is idempotent
// ---------------------------------------------------------------------------

void test_begin_idempotent(void) {
  WheelEncoder enc(kPinA, kPinB);
  TEST_ASSERT_TRUE(enc.begin());
  TEST_ASSERT_TRUE(enc.begin());   // second call must also return true
  TEST_ASSERT_TRUE(enc.begin());
}

// ---------------------------------------------------------------------------
// 4. read_ticks() reflects mocked value
// ---------------------------------------------------------------------------

void test_read_ticks_reflects_mock(void) {
  WheelEncoder enc(kPinA, kPinB);
  enc.begin();

  TEST_ASSERT_EQUAL_INT32(0, enc.read_ticks());

  enc.set_ticks_for_test(1000);
  TEST_ASSERT_EQUAL_INT32(1000, enc.read_ticks());

  enc.set_ticks_for_test(-12345);
  TEST_ASSERT_EQUAL_INT32(-12345, enc.read_ticks());

  enc.set_ticks_for_test(960);  // one full TT-motor revolution
  TEST_ASSERT_EQUAL_INT32(960, enc.read_ticks());
}

// ---------------------------------------------------------------------------
// 5. reset_ticks() zeros everything
// ---------------------------------------------------------------------------

void test_reset_ticks_zeros_state(void) {
  WheelEncoder enc(kPinA, kPinB);
  enc.begin();

  // Drive some ticks and complete one velocity window so internal state moves.
  enc.set_ticks_for_test(500);
  enc.read_velocity_dps(0);            // prime
  enc.set_ticks_for_test(960);
  (void)enc.read_velocity_dps(100);    // window elapses, state advances

  enc.reset_ticks();

  TEST_ASSERT_EQUAL_INT32(0, enc.read_ticks());
  // After reset, the very next velocity call is again a priming call.
  TEST_ASSERT_EQUAL_INT32(0, enc.read_velocity_dps(0));
}

// ---------------------------------------------------------------------------
// 6. distance_m() — one full revolution
// ---------------------------------------------------------------------------

void test_distance_m_one_revolution(void) {
  WheelEncoder enc(kPinA, kPinB);
  enc.begin();
  enc.set_counts_per_rev(960);
  enc.set_wheel_radius_m(0.033f);  // 33 mm radius, 66 mm-diam wheel

  enc.set_ticks_for_test(960);  // exactly one revolution
  const float circumference = 2.0f * kPi * 0.033f;  // ≈ 0.20735 m
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, circumference, enc.distance_m());

  // Half a revolution, in the negative direction.
  enc.set_ticks_for_test(-480);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, -circumference * 0.5f, enc.distance_m());

  // Zero ticks → zero distance.
  enc.set_ticks_for_test(0);
  TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, enc.distance_m());
}

// ---------------------------------------------------------------------------
// 7. distance_m() — 65 mm-diameter yellow-TT wheel
// ---------------------------------------------------------------------------

void test_distance_m_65mm_wheel(void) {
  WheelEncoder enc(kPinA, kPinB);
  enc.begin();
  enc.set_counts_per_rev(960);
  enc.set_wheel_radius_m(0.0325f);  // 65 mm diameter / 2

  enc.set_ticks_for_test(960);
  // Spec: ~0.207 m for a 65 mm wheel
  TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.207f, enc.distance_m());
}

// ---------------------------------------------------------------------------
// 8-11. read_velocity_dps()
// ---------------------------------------------------------------------------

void test_velocity_priming_call_returns_zero(void) {
  WheelEncoder enc(kPinA, kPinB);
  enc.begin();
  enc.set_counts_per_rev(960);
  // First call — primes the window, must return 0.
  TEST_ASSERT_EQUAL_INT32(0, enc.read_velocity_dps(0));
  TEST_ASSERT_EQUAL_INT32(0, enc.read_velocity_dps(1000));
  // Wait — a priming call set last_velocity_ms_ to 1 (special-cased to avoid
  // re-priming loops when now_ms == 0). The 1000 ms call is then a real
  // window read but no ticks moved, so velocity is 0.
}

void test_velocity_holds_sample_inside_window(void) {
  WheelEncoder enc(kPinA, kPinB);
  enc.begin();
  enc.set_counts_per_rev(960);
  enc.set_velocity_window_ms(100);

  // Prime the window at t=1000 with 0 ticks.
  enc.set_ticks_for_test(0);
  TEST_ASSERT_EQUAL_INT32(0, enc.read_velocity_dps(1000));

  // Move 240 ticks (90° of wheel rotation). Read at t=1100 — that's exactly
  // 100 ms after the prime, which IS the window threshold (>= 100).
  enc.set_ticks_for_test(240);
  const int32_t v_at_window = enc.read_velocity_dps(1100);
  // 240 ticks / 100 ms = 2400 ticks/s = 2400 * 360/960 = 900 dps
  TEST_ASSERT_INT32_WITHIN(2, 900, v_at_window);

  // Inside the next window (e.g. at t=1150 — only 50 ms after the last
  // sample), even if we move more ticks the cached sample must be returned.
  enc.set_ticks_for_test(99999);
  TEST_ASSERT_EQUAL_INT32(v_at_window, enc.read_velocity_dps(1150));
}

void test_velocity_correct_after_window(void) {
  WheelEncoder enc(kPinA, kPinB);
  enc.begin();
  enc.set_counts_per_rev(960);
  enc.set_velocity_window_ms(100);

  // Prime
  enc.set_ticks_for_test(0);
  enc.read_velocity_dps(2000);

  // After 500 ms with 1200 ticks → 1200/0.5 = 2400 ticks/s = 900 dps
  enc.set_ticks_for_test(1200);
  const int32_t v = enc.read_velocity_dps(2500);
  TEST_ASSERT_INT32_WITHIN(2, 900, v);
}

void test_velocity_negative_direction(void) {
  WheelEncoder enc(kPinA, kPinB);
  enc.begin();
  enc.set_counts_per_rev(960);
  enc.set_velocity_window_ms(100);

  // Prime at t=0 (which becomes t=1 internally to avoid re-prime loops)
  enc.set_ticks_for_test(0);
  enc.read_velocity_dps(0);

  // Negative tick delta — wheel moved backward.
  enc.set_ticks_for_test(-240);
  const int32_t v = enc.read_velocity_dps(101);  // 100 ms after prime
  // -240 ticks / 100 ms = -2400 ticks/s = -900 dps
  TEST_ASSERT_INT32_WITHIN(5, -900, v);
}

// ---------------------------------------------------------------------------
// 12. read_velocity_mps()
// ---------------------------------------------------------------------------

void test_velocity_mps_matches_dps_times_radius(void) {
  WheelEncoder enc(kPinA, kPinB);
  enc.begin();
  enc.set_counts_per_rev(960);
  enc.set_wheel_radius_m(0.0325f);
  enc.set_velocity_window_ms(100);

  // Prime
  enc.set_ticks_for_test(0);
  enc.read_velocity_mps(5000);

  // 240 ticks in 100 ms = 900 dps at the wheel.
  // v_mps = 900 * π/180 * 0.0325 ≈ 0.5105 m/s
  enc.set_ticks_for_test(240);
  const float v_mps = enc.read_velocity_mps(5100);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5105f, v_mps);
}

// ---------------------------------------------------------------------------
// 13-17. stalled()
// ---------------------------------------------------------------------------

void test_stalled_false_with_no_pwm_report(void) {
  WheelEncoder enc(kPinA, kPinB);
  enc.begin();
  TEST_ASSERT_FALSE(enc.stalled());
}

void test_stalled_false_before_time_elapsed(void) {
  WheelEncoder enc(kPinA, kPinB);
  enc.begin();

  enc.report_commanded_pwm(150, 1000);   // PWM well above threshold
  TEST_ASSERT_FALSE(enc.stalled(100, 200));  // first call arms the timer

  enc.report_commanded_pwm(150, 1100);   // 100 ms later — still inside window
  TEST_ASSERT_FALSE(enc.stalled(100, 200));
}

void test_stalled_true_when_pwm_sustained_and_no_motion(void) {
  WheelEncoder enc(kPinA, kPinB);
  enc.begin();
  enc.set_counts_per_rev(960);
  enc.set_velocity_window_ms(100);

  // Prime velocity with zero motion: ticks never change → velocity stays 0.
  enc.set_ticks_for_test(0);
  enc.read_velocity_dps(0);
  enc.read_velocity_dps(150);

  enc.report_commanded_pwm(200, 1000);
  (void)enc.stalled(100, 200);             // arm timer at t=1000
  enc.report_commanded_pwm(200, 1300);     // 300 ms later → past 200 ms window
  TEST_ASSERT_TRUE(enc.stalled(100, 200));
}

void test_stalled_false_when_wheel_is_moving(void) {
  WheelEncoder enc(kPinA, kPinB);
  enc.begin();
  enc.set_counts_per_rev(960);
  enc.set_velocity_window_ms(100);

  // Build up a velocity well above kStallVelocityDps (5 dps).
  // Priming sets last_velocity_ms_ = 1 internally (the special "now_ms==0"
  // case). To have the window genuinely elapse on the second call we need
  // (now_ms - 1) >= velocity_window_ms_ → second call at t=101 or later.
  enc.set_ticks_for_test(0);
  enc.read_velocity_dps(0);             // prime (sets last_velocity_ms_ = 1)
  enc.set_ticks_for_test(240);
  enc.read_velocity_dps(101);           // dt = 100 ms → ~900 dps cached

  enc.report_commanded_pwm(200, 1000);
  enc.stalled(100, 200);                // arm timer
  enc.report_commanded_pwm(200, 1300);  // past time window
  TEST_ASSERT_FALSE(enc.stalled(100, 200));
}

void test_stalled_resets_when_pwm_drops(void) {
  WheelEncoder enc(kPinA, kPinB);
  enc.begin();

  // Arm the timer at high PWM…
  enc.report_commanded_pwm(200, 1000);
  enc.stalled(100, 200);

  // …then drop PWM below threshold → timer must reset.
  enc.report_commanded_pwm(50, 1100);
  TEST_ASSERT_FALSE(enc.stalled(100, 200));

  // Re-arm and confirm timer starts fresh.
  enc.report_commanded_pwm(200, 1200);
  TEST_ASSERT_FALSE(enc.stalled(100, 200));  // only just re-armed
  enc.report_commanded_pwm(200, 1500);       // 300 ms later

  // Velocity wasn't ever advanced past prime in this test path, so it's 0 and
  // the bot reads as stalled at this point.
  TEST_ASSERT_TRUE(enc.stalled(100, 200));
}

// ============================================================================
// Unity main
// ============================================================================

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_construction_defaults);
  RUN_TEST(test_setters_roundtrip_and_reject_non_positive);
  RUN_TEST(test_begin_idempotent);
  RUN_TEST(test_read_ticks_reflects_mock);
  RUN_TEST(test_reset_ticks_zeros_state);
  RUN_TEST(test_distance_m_one_revolution);
  RUN_TEST(test_distance_m_65mm_wheel);
  RUN_TEST(test_velocity_priming_call_returns_zero);
  RUN_TEST(test_velocity_holds_sample_inside_window);
  RUN_TEST(test_velocity_correct_after_window);
  RUN_TEST(test_velocity_negative_direction);
  RUN_TEST(test_velocity_mps_matches_dps_times_radius);
  RUN_TEST(test_stalled_false_with_no_pwm_report);
  RUN_TEST(test_stalled_false_before_time_elapsed);
  RUN_TEST(test_stalled_true_when_pwm_sustained_and_no_motion);
  RUN_TEST(test_stalled_false_when_wheel_is_moving);
  RUN_TEST(test_stalled_resets_when_pwm_drops);
  return UNITY_END();
}
