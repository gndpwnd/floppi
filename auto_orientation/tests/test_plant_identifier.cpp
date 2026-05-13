/**
 * Native unit tests for PlantIdentifier (Phase 4.10 / Item 5).
 *
 * Mirrors the printf+exit(1) test pattern used by tests/test_online_mounting_
 * estimator.cpp and tests/test_pid_controller.cpp — no Unity dependency so it
 * compiles standalone on any host with g++.
 *
 * Tests cover:
 *   1. Construction defaults — sane prior + non-zero targets.
 *   2. RLS convergence on synthetic data — feed
 *        α = K_true * pwm + g_eff * sin(pitch) + noise
 *      and verify θ → K_true within tolerance.
 *   3. Freeze gate holds state — caller-set freeze=true must leave θ,P untouched.
 *   4. σ-modification projection — driving θ outside k_min/k_max snaps it back.
 *   5. Target-gain math — Kp = ω_n²/K, Kd = 2ζω_n/K verified against numerical
 *      values from the published formulae.
 *   6. Reset back-solves prior from kp_initial.
 *   7. Below MIN_PHI excitation — update is skipped (θ unchanged).
 *
 * Compile (from auto_orientation/):
 *   g++ -std=c++11 -O2 -o tests/test_plant_identifier \
 *     tests/test_plant_identifier.cpp \
 *     src/control/plant_identifier.cpp
 *   ./tests/test_plant_identifier
 */

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "../src/control/plant_identifier.h"

// ----- Test helpers -------------------------------------------------------

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
    if (!cond) { printf("FAIL: %s\n", msg); std::exit(1); }
}

// ----- 1. Construction defaults -------------------------------------------

static void test_construction() {
    printf("\nTest 1: construction defaults\n");
    PlantIdentifier pi;
    const PlantIdentifierStatus& s = pi.get_status();

    assert_true(s.k_motor > 0.0f, "k_motor positive prior");
    assert_true(s.covariance > 0.0f, "covariance positive");
    assert_true(s.kp_target > 0.0f, "kp_target computed");
    assert_true(s.kd_target > 0.0f, "kd_target computed");
    assert_true(s.ki_target > 0.0f, "ki_target computed");
    assert_true(!s.frozen, "frozen=false at construction");
    assert_true(s.samples == 0, "samples=0 at construction");
    ++g_passes;
    printf("  PASS (K=%.3f Kp=%.2f Kd=%.2f)\n",
           s.k_motor, s.kp_target, s.kd_target);
}

// ----- 2. RLS convergence on synthetic data -------------------------------

// Generate α = K_true * pwm_total + g_eff * sin(pitch). Returns the
// instantaneous gyro rate as the running integral of α (so the identifier
// receives consistent (rate, prev_rate, pitch, pwm) triples).
static void test_rls_convergence() {
    printf("\nTest 2: RLS convergence to K_true on synthetic data\n");

    const float K_TRUE        = 0.4f;     // deg/s² per PWM (mid-band)
    const float G_EFF         = 50.0f;    // matches PlantIdentifier default
    const float DEG_TO_RAD    = 0.01745329252f;
    const float DT            = 0.005f;   // 200 Hz
    const int   N_STEPS       = 4000;     // 20 s of data

    PlantIdentifier pi;
    // Seed the identifier with a deliberately-wrong prior so we can see it
    // converge to the truth.
    pi.reset(/*kp_initial=*/30.0f, /*kd_initial=*/12.0f);

    float pitch_deg   = 2.0f;     // start tilted forward 2°
    float gyro_dps    = 0.0f;     // initial pitch rate
    float prev_gyro   = 0.0f;

    // Tiny PRNG for excitation noise on PWM. The bot's natural disturbance
    // response provides this in practice; the test simulates it.
    unsigned rng_state = 1u;
    auto urand = [&]() -> float {
        rng_state = rng_state * 1103515245u + 12345u;
        return ((float)((rng_state >> 16) & 0x7FFF)) / 32767.0f - 0.5f;
    };

    for (int i = 0; i < N_STEPS; ++i) {
        // Synthetic controller: simple P + D on pitch to keep the simulated
        // plant from running away. Provides regression excitation.
        const float pwm_per_wheel = -8.0f * pitch_deg - 0.5f * gyro_dps
                                    + 40.0f * urand();
        const float pwm_total     = 2.0f * pwm_per_wheel;

        // Forward-simulate the rigid-body equation:
        //   α = K_true * pwm_total + g_eff * sin(pitch_rad)
        // Note: real bot has gravity opposing the tilt, so the sign of the
        // gravity term here matches the PlantIdentifier regression form
        // y = α − g_eff * sin(pitch).
        const float pitch_rad = pitch_deg * DEG_TO_RAD;
        const float alpha     = K_TRUE * pwm_total + G_EFF * sinf(pitch_rad);

        // Integrate forward: gyro += α * dt; pitch += gyro * dt.
        prev_gyro  = gyro_dps;
        gyro_dps  += alpha * DT;
        pitch_deg += gyro_dps * DT;

        // Soft saturation so the synthetic plant doesn't blow up in the
        // pathological case where our placeholder controller fails. Real bot
        // hits the soft-cutoff in BalanceApp.
        if (pitch_deg >  10.0f) pitch_deg =  10.0f;
        if (pitch_deg < -10.0f) pitch_deg = -10.0f;

        pi.update(pwm_total, gyro_dps, prev_gyro, pitch_deg, DT,
                  /*freeze=*/false);
    }

    const float K_est = pi.get_k_motor();
    printf("  K_true=%.3f  K_est=%.3f  err=%.1f%%\n",
           K_TRUE, K_est, 100.0f * std::fabs(K_est - K_TRUE) / K_TRUE);
    // Tolerance ±25% — generous, accounts for the linearised sin and the
    // noisy excitation. The design doc target is ±20% after 30 s; we get
    // close in 20 s of synthetic data.
    assert_near(K_est, K_TRUE, 0.25f * K_TRUE, "K_est within 25% of K_true");
    ++g_passes;
    printf("  PASS\n");
}

// ----- 3. Freeze gate holds state -----------------------------------------

static void test_freeze_holds_state() {
    printf("\nTest 3: freeze=true holds θ and P\n");
    PlantIdentifier pi;
    pi.reset(/*kp_initial=*/50.0f, /*kd_initial=*/20.0f);

    const float theta0 = pi.get_k_motor();
    const float P0     = pi.get_covariance();

    // Feed 100 ticks of strong excitation while frozen — should not move θ.
    for (int i = 0; i < 100; ++i) {
        pi.update(/*pwm_total=*/200.0f,
                  /*gyro=*/30.0f, /*prev=*/0.0f,
                  /*pitch=*/3.0f, /*dt=*/0.005f,
                  /*freeze=*/true);
    }
    assert_near(pi.get_k_motor(), theta0, 1e-6f, "θ unchanged while frozen");
    assert_near(pi.get_covariance(), P0, 1e-6f, "P unchanged while frozen");
    assert_true(pi.get_status().frozen, "status.frozen reflects gate");
    ++g_passes;
    printf("  PASS\n");
}

// ----- 4. σ-modification projection ---------------------------------------

static void test_projection_clamps() {
    printf("\nTest 4: σ-modification keeps θ in (k_min, k_max)\n");
    PlantIdentifier pi;
    pi.set_k_motor_bounds(0.1f, 1.0f);
    pi.reset(/*kp_initial=*/50.0f, /*kd_initial=*/20.0f);

    // Drive θ huge by feeding a scenario where every sample says "tiny PWM
    // produces enormous α" — RLS will try to push θ skyward.
    for (int i = 0; i < 500; ++i) {
        const float gyro     = 200.0f * (float)i / 500.0f;
        const float prev     = 200.0f * (float)(i - 1) / 500.0f;
        // Note: α implied = (gyro − prev)/dt = (200/500)/0.005 = 80 deg/s² per
        // step. With pwm_total = 50, that's an implied K of 80/50 = 1.6 —
        // above k_max=1.0, so projection must clamp.
        pi.update(/*pwm_total=*/50.0f, gyro, prev, /*pitch=*/0.0f,
                  /*dt=*/0.005f, /*freeze=*/false);
    }
    const float K = pi.get_k_motor();
    printf("  K_clamped=%.3f (k_max=1.0)\n", K);
    assert_true(K <= 1.0f + 1e-3f, "θ ≤ k_max");
    assert_true(K >= 0.1f - 1e-3f, "θ ≥ k_min");
    ++g_passes;
    printf("  PASS\n");
}

// ----- 5. Target-gain math ------------------------------------------------

static void test_target_gain_math() {
    printf("\nTest 5: closed-form Kp/Kd math\n");
    PlantIdentifier pi;
    pi.set_target_settling_time_sec(0.5f);    // ω_n = 4/0.5 = 8 rad/s
    pi.set_damping_ratio(0.7f);
    pi.set_k_motor_bounds(0.01f, 100.0f);
    // Seed prior to K=1.0 exactly so the math is verifiable by hand.
    // reset() back-solves θ from kp_initial: K = ω_n²/Kp = 64/Kp.
    // For K=1.0 we want Kp_initial = 64.
    pi.reset(/*kp_initial=*/64.0f, /*kd_initial=*/0.0f);

    const PlantIdentifierStatus& s = pi.get_status();
    // Expected: Kp = ω_n²/K = 64/1 = 64; Kd = 2ζω_n/K = 2*0.7*8/1 = 11.2.
    assert_near(s.k_motor, 1.0f, 1e-4f, "K_motor back-solved");
    assert_near(s.kp_target, 64.0f, 0.1f, "Kp_target = ω_n²/K");
    assert_near(s.kd_target, 11.2f, 0.1f, "Kd_target = 2ζω_n/K");
    assert_near(s.ki_target, 0.05f * 64.0f, 0.1f, "Ki_target = 0.05·Kp");
    ++g_passes;
    printf("  PASS (Kp=%.2f Kd=%.2f Ki=%.2f)\n",
           s.kp_target, s.kd_target, s.ki_target);
}

// ----- 6. reset() back-solves prior ---------------------------------------

static void test_reset_priors() {
    printf("\nTest 6: reset() seeds θ from kp_initial\n");
    PlantIdentifier pi;
    pi.set_target_settling_time_sec(0.5f);    // ω_n = 8
    // Kp=32 → K = 64/32 = 2.0.
    pi.reset(32.0f, 12.0f);
    assert_near(pi.get_k_motor(), 2.0f, 1e-3f, "θ = ω_n²/Kp");

    // Kp=8 → K = 64/8 = 8.0, but k_max=5 default → clamp to 5.
    pi.set_k_motor_bounds(0.02f, 5.0f);
    pi.reset(8.0f, 0.0f);
    assert_true(pi.get_k_motor() <= 5.0f + 1e-3f, "θ clamped to k_max on reset");

    // Kp = 0 → fallback to class-prior mid-band (defined as 0.2f).
    pi.reset(0.0f, 0.0f);
    assert_true(pi.get_k_motor() > 0.0f, "θ > 0 on zero kp_initial");
    ++g_passes;
    printf("  PASS\n");
}

// ----- 7. Excitation gate skips update below MIN_PHI ----------------------

static void test_excitation_gate() {
    printf("\nTest 7: below-MIN_PHI excitation does not move θ\n");
    PlantIdentifier pi;
    pi.reset(50.0f, 20.0f);
    const float theta0 = pi.get_k_motor();

    // pwm_total below the excitation floor (MIN_PHI = 10) — RLS should skip.
    for (int i = 0; i < 100; ++i) {
        pi.update(/*pwm_total=*/5.0f, /*gyro=*/5.0f, /*prev=*/0.0f,
                  /*pitch=*/2.0f, /*dt=*/0.005f, /*freeze=*/false);
    }
    assert_near(pi.get_k_motor(), theta0, 1e-6f, "θ frozen below MIN_PHI");
    ++g_passes;
    printf("  PASS\n");
}

// ----- main ---------------------------------------------------------------

int main() {
    printf("PlantIdentifier unit tests\n");
    printf("==========================\n");
    test_construction();
    test_rls_convergence();
    test_freeze_holds_state();
    test_projection_clamps();
    test_target_gain_math();
    test_reset_priors();
    test_excitation_gate();
    printf("\nAll %d tests passed.\n", g_passes);
    return 0;
}
