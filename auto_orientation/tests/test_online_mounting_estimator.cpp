/**
 * Unit Tests — OnlineMountingEstimator (Phase 4.4)
 *
 * Tests cover:
 *   - Initialization & reference handling
 *   - No-drift behaviour when I-term is zero
 *   - Slow LPF convergence toward an I-term target
 *   - Hard bound (±max_deviation) clamping
 *   - Rate limit (max_drift_rate_dps)
 *   - All four freeze reasons (tipover, windup, user, high gyro)
 *   - Reset-to-reference
 *   - Confidence heuristic monotonicity
 *   - Status struct integrity
 *   - millis() rollover behaviour
 *
 * Time is fully simulated — we pass now_ms explicitly, never call millis().
 * Follows the same printf/exit(1) test pattern as tests/test_quaternion.cpp.
 */

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "../src/navigation/online_mounting_estimator.h"

// ============================================================================
// TEST UTILITIES
// ============================================================================

const float EPSILON = 1e-5f;     // tight tolerance for "no drift" checks
const float LOOSE   = 0.05f;     // looser tolerance for convergence checks

static int g_passes = 0;

static void assert_near(float actual, float expected, float tolerance,
                        const char* msg) {
    if (std::fabs(actual - expected) > tolerance) {
        printf("FAIL: %s (expected %.6f, got %.6f, diff %.6f)\n",
               msg, expected, actual, std::fabs(actual - expected));
        std::exit(1);
    }
}

static void assert_true(bool cond, const char* msg) {
    if (!cond) {
        printf("FAIL: %s\n", msg);
        std::exit(1);
    }
}

static void assert_le(float a, float b, const char* msg) {
    if (!(a <= b + EPSILON)) {
        printf("FAIL: %s (%.6f > %.6f)\n", msg, a, b);
        std::exit(1);
    }
}

// Run update() N times at a fixed dt, accumulating simulated milliseconds.
// Returns the final estimate. Useful for "soak" tests where we want to see
// where the LPF converges.
static float simulate_run(OnlineMountingEstimator& est,
                          float i_term, float pitch_deg,
                          bool tipover, bool windup, float gyro_dps,
                          uint32_t step_ms, int n_steps, uint32_t start_ms) {
    uint32_t t = start_ms;
    float result = est.get_estimate_deg();
    for (int i = 0; i < n_steps; ++i) {
        t += step_ms;
        result = est.update(i_term, pitch_deg, tipover, windup, gyro_dps, t);
    }
    return result;
}

// ============================================================================
// TEST 1: INITIALIZATION
// ============================================================================

static void test_initialization() {
    printf("\nTest 1: Initialization\n");

    OnlineMountingEstimator est;
    est.initialize(-8.6f, 1000u);

    assert_near(est.get_estimate_deg(), -8.6f, EPSILON, "estimate == reference at init");
    assert_near(est.get_reference_deg(), -8.6f, EPSILON, "reference stored");
    assert_near(est.get_drift_rate_dps(), 0.0f, EPSILON, "drift rate zero at init");

    MountingCalibrationStatus s = est.get_status();
    assert_near(s.estimate_deg, -8.6f, EPSILON, "status.estimate_deg");
    assert_true(!s.adaptation_frozen, "not frozen at init");
    assert_true(s.freeze_reason == FREEZE_NONE, "freeze_reason == FREEZE_NONE");
    assert_near(s.confidence_0_to_1, 1.0f, EPSILON, "confidence == 1.0 at reference");

    ++g_passes;
    printf("  PASS\n");
}

// ============================================================================
// TEST 2: NO DRIFT WHEN I-TERM IS ZERO
// ============================================================================

static void test_no_drift_when_i_term_zero() {
    printf("\nTest 2: No drift when I-term is zero\n");

    OnlineMountingEstimator est;
    est.initialize(0.0f, 0u);

    // Default tc = 300 s, gain = 0.01. With i_term = 0, target = reference,
    // so the LPF residual is zero and the estimate should not move at all.
    float result = simulate_run(est,
                                /*i_term=*/0.0f, /*pitch=*/0.0f,
                                /*tipover=*/false, /*windup=*/false,
                                /*gyro=*/0.0f,
                                /*dt_ms=*/5u, /*n_steps=*/2000,  // 10 s at 200 Hz
                                /*start_ms=*/0u);

    assert_near(result, 0.0f, EPSILON, "estimate stays at reference");
    assert_near(est.get_drift_rate_dps(), 0.0f, EPSILON, "drift rate stays zero");

    ++g_passes;
    printf("  PASS\n");
}

// ============================================================================
// TEST 3: SLOW DRIFT TOWARD I-TERM TARGET
// ============================================================================

static void test_slow_drift_toward_i_term() {
    printf("\nTest 3: Slow drift toward I-term target\n");

    OnlineMountingEstimator est;
    est.initialize(0.0f, 0u);
    // Use a moderate time constant so the test runs reasonably fast.
    est.set_lpf_time_constant_sec(10.0f);   // tc = 10 s
    est.set_gain_to_angle(0.01f);            // i_term * 0.01 = target deg

    // With i_term = 100, target = 1.0°. Apply for 30 s — about 3 time
    // constants, so the LPF should land within ~5% of the target.
    // BUT: the rate limit is 0.5°/s by default; over 30 s that allows
    // up to 15° of change, so it does not bind here.
    float result = simulate_run(est,
                                /*i_term=*/100.0f, /*pitch=*/0.0f,
                                /*tipover=*/false, /*windup=*/false,
                                /*gyro=*/0.0f,
                                /*dt_ms=*/5u, /*n_steps=*/6000,  // 30 s at 200 Hz
                                /*start_ms=*/0u);

    // Should be close to 1.0 but not exactly — exponential approach.
    // After 3 tc, residual ≈ exp(-3) ≈ 0.05 → estimate ≈ 0.95.
    assert_true(result > 0.85f, "estimate moved a meaningful fraction toward target");
    assert_true(result < 1.0f + EPSILON, "estimate has not overshot target");

    // And it must NOT have moved there in one step — check that the first
    // few samples produce a small movement.
    OnlineMountingEstimator est2;
    est2.initialize(0.0f, 0u);
    est2.set_lpf_time_constant_sec(10.0f);
    est2.set_gain_to_angle(0.01f);
    float one_step = est2.update(100.0f, 0.0f, false, false, 0.0f, 5u);
    assert_true(one_step < 0.01f, "one update step moves only tiny amount");

    ++g_passes;
    printf("  PASS\n");
}

// ============================================================================
// TEST 4: HARD BOUND CLAMPS LARGE I-TERM
// ============================================================================

static void test_hard_bound_clamps() {
    printf("\nTest 4: Hard bound clamps at ±max_deviation\n");

    OnlineMountingEstimator est;
    est.initialize(0.0f, 0u);
    est.set_lpf_time_constant_sec(1.0f);     // fast — push hard
    est.set_max_deviation_deg(5.0f);
    est.set_max_drift_rate_dps(10.0f);       // allow big steps so the bound is what binds
    est.set_gain_to_angle(0.01f);

    // i_term = 10000 → target = 100° (way past the 5° bound).
    // After many seconds, the LPF wants 100°, but the clamp must hold at 5°.
    float result = simulate_run(est,
                                /*i_term=*/10000.0f, /*pitch=*/0.0f,
                                /*tipover=*/false, /*windup=*/false,
                                /*gyro=*/0.0f,
                                /*dt_ms=*/5u, /*n_steps=*/4000,  // 20 s
                                /*start_ms=*/0u);

    assert_le(result, 5.0f, "estimate clamped at +max_deviation");
    // And the negative side:
    OnlineMountingEstimator est2;
    est2.initialize(0.0f, 0u);
    est2.set_lpf_time_constant_sec(1.0f);
    est2.set_max_deviation_deg(5.0f);
    est2.set_max_drift_rate_dps(10.0f);
    est2.set_gain_to_angle(0.01f);
    float result2 = simulate_run(est2,
                                 /*i_term=*/-10000.0f, /*pitch=*/0.0f,
                                 /*tipover=*/false, /*windup=*/false,
                                 /*gyro=*/0.0f,
                                 /*dt_ms=*/5u, /*n_steps=*/4000,
                                 /*start_ms=*/0u);
    assert_true(result2 >= -5.0f - EPSILON, "estimate clamped at -max_deviation");
    assert_true(result2 <= -4.9f, "estimate reached the lower clamp");

    ++g_passes;
    printf("  PASS\n");
}

// ============================================================================
// TEST 5: RATE LIMIT
// ============================================================================

static void test_rate_limit() {
    printf("\nTest 5: Rate limit caps |d/dt estimate|\n");

    OnlineMountingEstimator est;
    est.initialize(0.0f, 0u);
    est.set_lpf_time_constant_sec(0.1f);     // very fast LPF
    est.set_max_drift_rate_dps(0.1f);        // very tight rate limit
    est.set_max_deviation_deg(5.0f);
    est.set_gain_to_angle(0.01f);

    // Target = 5°. Without rate limit the LPF would slam there in ~0.3 s.
    // Rate limit allows at most 0.1°/s, so after 10 s we should be at ≤1.0°.
    uint32_t t = 0u;
    uint32_t prev_t = 0u;
    float prev_estimate = 0.0f;
    for (int i = 0; i < 200; ++i) {     // 1 s at 200 Hz
        t += 5u;
        float current = est.update(500.0f, 0.0f, false, false, 0.0f, t);
        float dt_s = static_cast<float>(t - prev_t) * 0.001f;
        float step = current - prev_estimate;
        float step_per_s = step / dt_s;
        // Allow tiny float slop above the bound.
        assert_le(std::fabs(step_per_s), 0.1f + 1e-3f,
                  "per-step rate stays within max_drift_rate_dps");
        prev_t = t;
        prev_estimate = current;
    }

    // After 1 s of pushing hard, estimate should be near 0.1° (rate-limited
    // approach) — not the LPF's natural target of ~5°.
    assert_true(est.get_estimate_deg() > 0.05f, "made some progress under rate limit");
    assert_true(est.get_estimate_deg() < 0.15f, "rate limit prevented fast convergence");

    ++g_passes;
    printf("  PASS\n");
}

// ============================================================================
// TEST 6: FREEZE — TIPOVER
// ============================================================================

static void test_freeze_tipover() {
    printf("\nTest 6: Freeze during tipover recovery\n");

    OnlineMountingEstimator est;
    est.initialize(0.0f, 0u);
    est.set_lpf_time_constant_sec(1.0f);
    est.set_gain_to_angle(0.01f);

    // First, let the estimate drift to ~0.5° without the freeze.
    simulate_run(est, 100.0f, 0.0f, false, false, 0.0f, 5u, 200, 0u);
    float pre_freeze = est.get_estimate_deg();
    assert_true(pre_freeze > 0.1f, "pre-freeze estimate moved off reference");

    // Now activate tipover and pound with large i_term — should not move.
    float result = simulate_run(est,
                                /*i_term=*/10000.0f, /*pitch=*/45.0f,
                                /*tipover=*/true, /*windup=*/false,
                                /*gyro=*/0.0f,
                                /*dt_ms=*/5u, /*n_steps=*/1000,
                                /*start_ms=*/2000u);
    assert_near(result, pre_freeze, EPSILON, "estimate frozen during tipover");
    assert_near(est.get_drift_rate_dps(), 0.0f, EPSILON, "drift rate reports zero when frozen");

    MountingCalibrationStatus s = est.get_status();
    assert_true(s.adaptation_frozen, "status.adaptation_frozen is true");
    assert_true(s.freeze_reason == FREEZE_TIPOVER, "freeze_reason == FREEZE_TIPOVER");

    ++g_passes;
    printf("  PASS\n");
}

// ============================================================================
// TEST 7: FREEZE — WINDUP
// ============================================================================

static void test_freeze_windup() {
    printf("\nTest 7: Freeze during I-term windup\n");

    OnlineMountingEstimator est;
    est.initialize(0.0f, 0u);
    est.set_lpf_time_constant_sec(1.0f);
    est.set_gain_to_angle(0.01f);

    // Push with windup_active=true. Estimate must not move.
    float result = simulate_run(est,
                                /*i_term=*/10000.0f, /*pitch=*/0.0f,
                                /*tipover=*/false, /*windup=*/true,
                                /*gyro=*/0.0f,
                                /*dt_ms=*/5u, /*n_steps=*/2000,
                                /*start_ms=*/0u);
    assert_near(result, 0.0f, EPSILON, "estimate frozen during windup");
    assert_true(est.get_status().freeze_reason == FREEZE_WINDUP, "freeze_reason == FREEZE_WINDUP");

    ++g_passes;
    printf("  PASS\n");
}

// ============================================================================
// TEST 8: FREEZE — USER
// ============================================================================

static void test_freeze_user() {
    printf("\nTest 8: Freeze when user disables adaptation\n");

    OnlineMountingEstimator est;
    est.initialize(0.0f, 0u);
    est.set_lpf_time_constant_sec(1.0f);
    est.set_gain_to_angle(0.01f);
    est.set_user_freeze(true);

    float result = simulate_run(est,
                                /*i_term=*/10000.0f, /*pitch=*/0.0f,
                                /*tipover=*/false, /*windup=*/false,
                                /*gyro=*/0.0f,
                                /*dt_ms=*/5u, /*n_steps=*/2000,
                                /*start_ms=*/0u);
    assert_near(result, 0.0f, EPSILON, "estimate frozen on user request");
    assert_true(est.get_status().freeze_reason == FREEZE_USER, "freeze_reason == FREEZE_USER");
    assert_true(est.get_status().adaptation_frozen, "adaptation_frozen flag set");

    // Unfreeze; adaptation should resume.
    est.set_user_freeze(false);
    simulate_run(est, 100.0f, 0.0f, false, false, 0.0f, 5u, 600, 10000u);
    assert_true(est.get_estimate_deg() > 0.05f, "estimate resumes after unfreeze");
    assert_true(est.get_status().freeze_reason == FREEZE_NONE, "freeze cleared after unfreeze");

    ++g_passes;
    printf("  PASS\n");
}

// ============================================================================
// TEST 9: FREEZE — HIGH GYRO
// ============================================================================

static void test_freeze_high_gyro() {
    printf("\nTest 9: Freeze under high gyro rate\n");

    OnlineMountingEstimator est;
    est.initialize(0.0f, 0u);
    est.set_lpf_time_constant_sec(1.0f);
    est.set_gain_to_angle(0.01f);

    // 100 °/s exceeds the 60 °/s threshold.
    float result = simulate_run(est,
                                /*i_term=*/10000.0f, /*pitch=*/0.0f,
                                /*tipover=*/false, /*windup=*/false,
                                /*gyro=*/100.0f,
                                /*dt_ms=*/5u, /*n_steps=*/2000,
                                /*start_ms=*/0u);
    assert_near(result, 0.0f, EPSILON, "estimate frozen with high positive gyro");
    assert_true(est.get_status().freeze_reason == FREEZE_HIGH_GYRO, "freeze == FREEZE_HIGH_GYRO");

    // Symmetric: negative gyro rate too.
    OnlineMountingEstimator est2;
    est2.initialize(0.0f, 0u);
    est2.set_lpf_time_constant_sec(1.0f);
    est2.set_gain_to_angle(0.01f);
    float result2 = simulate_run(est2,
                                 /*i_term=*/10000.0f, /*pitch=*/0.0f,
                                 /*tipover=*/false, /*windup=*/false,
                                 /*gyro=*/-100.0f,
                                 /*dt_ms=*/5u, /*n_steps=*/2000,
                                 /*start_ms=*/0u);
    assert_near(result2, 0.0f, EPSILON, "estimate frozen with high negative gyro");

    // And it should NOT freeze just below the threshold.
    OnlineMountingEstimator est3;
    est3.initialize(0.0f, 0u);
    est3.set_lpf_time_constant_sec(1.0f);
    est3.set_gain_to_angle(0.01f);
    simulate_run(est3, 100.0f, 0.0f, false, false, /*gyro=*/30.0f,
                 5u, 600, 0u);
    assert_true(est3.get_estimate_deg() > 0.01f, "estimate moves at moderate gyro rate");

    ++g_passes;
    printf("  PASS\n");
}

// ============================================================================
// TEST 10: RESET TO REFERENCE
// ============================================================================

static void test_reset_to_reference() {
    printf("\nTest 10: Reset to new reference\n");

    OnlineMountingEstimator est;
    est.initialize(0.0f, 0u);
    est.set_lpf_time_constant_sec(1.0f);
    est.set_gain_to_angle(0.01f);

    // Drift the estimate off reference.
    simulate_run(est, 100.0f, 0.0f, false, false, 0.0f, 5u, 1000, 0u);
    assert_true(est.get_estimate_deg() > 0.1f, "estimate drifted off original reference");

    // Re-capture says the new reference is -3.2°. Estimator must reset.
    est.reset_to_reference(-3.2f, 10000u);
    assert_near(est.get_estimate_deg(), -3.2f, EPSILON, "estimate == new reference after reset");
    assert_near(est.get_reference_deg(), -3.2f, EPSILON, "reference updated");
    assert_near(est.get_drift_rate_dps(), 0.0f, EPSILON, "drift rate cleared on reset");
    assert_true(est.get_status().freeze_reason == FREEZE_NONE, "freeze cleared on reset");

    ++g_passes;
    printf("  PASS\n");
}

// ============================================================================
// TEST 11: CONFIDENCE DECREASES WITH DRIFT
// ============================================================================

static void test_confidence_decreases_with_drift() {
    printf("\nTest 11: Confidence heuristic decreases monotonically with drift\n");

    OnlineMountingEstimator est;
    est.initialize(0.0f, 0u);
    est.set_lpf_time_constant_sec(0.5f);     // fast for the test
    est.set_max_deviation_deg(5.0f);
    est.set_max_drift_rate_dps(10.0f);       // don't let rate limit interfere
    est.set_gain_to_angle(0.01f);

    float conf_at_ref = est.get_status().confidence_0_to_1;
    assert_near(conf_at_ref, 1.0f, EPSILON, "confidence = 1.0 at reference");

    // Drift partway out.
    simulate_run(est, 250.0f, 0.0f, false, false, 0.0f, 5u, 600, 0u);
    float conf_mid = est.get_status().confidence_0_to_1;
    assert_true(conf_mid < conf_at_ref, "confidence drops as estimate drifts");
    assert_true(conf_mid > 0.0f, "confidence stays positive within the bound");

    // Drive all the way to the rail.
    simulate_run(est, 10000.0f, 0.0f, false, false, 0.0f, 5u, 4000, 5000u);
    float conf_rail = est.get_status().confidence_0_to_1;
    assert_true(conf_rail < conf_mid, "confidence keeps dropping toward bound");
    assert_true(conf_rail < 0.05f, "confidence near zero at the hard bound");

    ++g_passes;
    printf("  PASS\n");
}

// ============================================================================
// TEST 12: STATUS STRUCT INTEGRITY
// ============================================================================

static void test_status_struct() {
    printf("\nTest 12: Status struct mirrors internal state\n");

    OnlineMountingEstimator est;
    est.initialize(-8.6f, 1000u);
    est.set_lpf_time_constant_sec(1.0f);
    est.set_gain_to_angle(0.01f);

    // Drift a bit.
    simulate_run(est, 50.0f, 0.0f, false, false, 0.0f, 5u, 400, 1000u);

    MountingCalibrationStatus s = est.get_status();
    assert_near(s.estimate_deg, est.get_estimate_deg(), EPSILON,
                "status.estimate_deg matches getter");
    assert_near(s.drift_rate_dps, est.get_drift_rate_dps(), EPSILON,
                "status.drift_rate_dps matches getter");
    assert_true(s.confidence_0_to_1 >= 0.0f && s.confidence_0_to_1 <= 1.0f,
                "confidence within [0,1]");

    // mark_saved updates last_save_ms.
    est.mark_saved(50000u);
    assert_true(est.get_status().last_save_ms == 50000u,
                "mark_saved propagates to status.last_save_ms");

    ++g_passes;
    printf("  PASS\n");
}

// ============================================================================
// TEST 13: MILLIS ROLLOVER SAFETY
// ============================================================================

static void test_millis_rollover() {
    printf("\nTest 13: millis() rollover handled gracefully\n");

    OnlineMountingEstimator est;
    // Initialize near 32-bit max.
    const uint32_t near_max = 0xFFFFFFFFu - 1000u;
    est.initialize(0.0f, near_max);
    est.set_lpf_time_constant_sec(1.0f);
    est.set_gain_to_angle(0.01f);

    // Push forward a few samples, then wrap.
    float before = est.update(100.0f, 0.0f, false, false, 0.0f, near_max + 500u);
    // Wrap: now_ms < last_update_ms_. The estimator must not crash or
    // produce a wild dt; it just no-ops this sample.
    float after = est.update(100.0f, 0.0f, false, false, 0.0f, 100u);
    assert_near(after, before, EPSILON, "no spurious update on wrap");

    // Next sample after the wrap should adapt normally.
    float after2 = est.update(100.0f, 0.0f, false, false, 0.0f, 200u);
    assert_true(after2 >= before, "adaptation resumes post-wrap");

    ++g_passes;
    printf("  PASS\n");
}

// ============================================================================
// TEST 14: TOO-FAST CALLS ARE SKIPPED
// ============================================================================

static void test_dt_less_than_one_ms() {
    printf("\nTest 14: Sub-millisecond calls are no-ops\n");

    OnlineMountingEstimator est;
    est.initialize(0.0f, 0u);
    est.set_lpf_time_constant_sec(1.0f);
    est.set_gain_to_angle(0.01f);

    // First update at t=1ms moves the estimate by some tiny amount.
    float first = est.update(100.0f, 0.0f, false, false, 0.0f, 1u);
    // Second update at the same timestamp must not move the estimate again.
    float second = est.update(100.0f, 0.0f, false, false, 0.0f, 1u);
    assert_near(first, second, EPSILON, "same-timestamp call is a no-op");

    ++g_passes;
    printf("  PASS\n");
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    printf("========================================\n");
    printf("ONLINE MOUNTING ESTIMATOR TEST SUITE\n");
    printf("========================================\n");

    test_initialization();
    test_no_drift_when_i_term_zero();
    test_slow_drift_toward_i_term();
    test_hard_bound_clamps();
    test_rate_limit();
    test_freeze_tipover();
    test_freeze_windup();
    test_freeze_user();
    test_freeze_high_gyro();
    test_reset_to_reference();
    test_confidence_decreases_with_drift();
    test_status_struct();
    test_millis_rollover();
    test_dt_less_than_one_ms();

    printf("\n========================================\n");
    printf("ALL %d TESTS PASSED\n", g_passes);
    printf("========================================\n");
    return 0;
}
