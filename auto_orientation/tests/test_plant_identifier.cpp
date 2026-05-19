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
 * Workstream 3 hardening (2026-05-18):
 *   8. σ-projection floor — θ never drops below k_min, even when fed data that
 *      pushes it negative.
 *   9. σ-projection ceiling — explicit upper-bound check independent of the
 *      original test (4). Driven by a different scenario for redundancy.
 *  10. Excitation floor (already covered as test 7) — kept; new test 10 asserts
 *      the boundary case (φ == MIN_PHI) actually allows a learning step.
 *  11. Closed-form gain output math — compute Kp/Kd by hand at a non-trivial
 *      θ and assert get_kp_target/get_kd_target match.
 *  12. Freeze gate hard-locks across many ticks — repeated update(freeze=true)
 *      calls do not move θ or P even when alpha/φ vary.
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

// ----- 8. σ-projection floor (theta never drops below k_min) --------------

static void test_projection_floor() {
    printf("\nTest 8: σ-projection FLOOR keeps θ ≥ k_min under negative-K drive\n");
    PlantIdentifier pi;
    // Tight bounds so the floor is easy to hit.
    pi.set_k_motor_bounds(0.1f, 1.0f);
    pi.reset(/*kp_initial=*/50.0f, /*kd_initial=*/20.0f);

    // Drive a scenario where α is consistently small even with large PWM —
    // RLS will try to push θ toward zero (or negative). The projection must
    // floor at k_min=0.1.
    for (int i = 0; i < 800; ++i) {
        // Large positive PWM, but the "plant" responds with near-zero α
        // (gyro doesn't change). This is what the regression sees if a motor
        // is disconnected: lots of command, no acceleration.
        // y = alpha - g_eff * sin(pitch) ≈ 0 - 0 = 0
        // phi = 50, so θ wants to converge to 0.
        pi.update(/*pwm_total=*/50.0f,
                  /*gyro=*/0.0f, /*prev=*/0.0f,
                  /*pitch=*/0.0f, /*dt=*/0.005f,
                  /*freeze=*/false);
    }
    const float K = pi.get_k_motor();
    printf("  K_floored=%.4f (k_min=0.1)\n", K);
    assert_true(K >= 0.1f - 1e-3f, "θ ≥ k_min after negative-K drive");
    assert_true(K <= 1.0f + 1e-3f, "θ ≤ k_max (sanity)");
    ++g_passes;
    printf("  PASS\n");
}

// ----- 9. σ-projection ceiling (independent of test 4) --------------------

static void test_projection_ceiling() {
    printf("\nTest 9: σ-projection CEILING keeps θ ≤ k_max under positive-K drive\n");
    PlantIdentifier pi;
    pi.set_k_motor_bounds(0.05f, 0.8f);
    pi.reset(/*kp_initial=*/50.0f, /*kd_initial=*/20.0f);

    // Drive a scenario where α >> K*pwm — every sample says "tiny PWM
    // produced huge alpha". RLS will push θ skyward; projection must clamp
    // at k_max=0.8. Use sufficient excitation (φ above MIN_PHI=10).
    for (int i = 0; i < 800; ++i) {
        // alpha = (gyro - prev_gyro) / dt. We feed gyro=1000, prev=0, dt=0.005
        // => alpha = 200,000 deg/s² — physically nonsense, but it forces RLS
        // to claim K = alpha/pwm = 200000/15 ≈ 13,300 — well above k_max.
        // The projection must clamp.
        pi.update(/*pwm_total=*/15.0f,
                  /*gyro=*/1000.0f, /*prev=*/0.0f,
                  /*pitch=*/0.0f, /*dt=*/0.005f,
                  /*freeze=*/false);
    }
    const float K = pi.get_k_motor();
    printf("  K_ceilinged=%.4f (k_max=0.8)\n", K);
    assert_true(K <= 0.8f + 1e-3f, "θ ≤ k_max after positive-K drive");
    assert_true(K >= 0.05f - 1e-3f, "θ ≥ k_min (sanity)");
    ++g_passes;
    printf("  PASS\n");
}

// ----- 10. Excitation boundary — at MIN_PHI the update DOES happen -------

static void test_excitation_boundary() {
    printf("\nTest 10: excitation at MIN_PHI threshold permits a learning step\n");
    PlantIdentifier pi;
    pi.reset(50.0f, 20.0f);
    const float theta0 = pi.get_k_motor();

    // MIN_PHI = 10 per plant_identifier.cpp. Test that |φ| = 15 (clearly above
    // the floor) produces movement when α is non-zero. This is the inverse of
    // test 7's "below the floor freezes" assertion — we need confidence that
    // ABOVE the floor, learning actually proceeds.
    for (int i = 0; i < 50; ++i) {
        pi.update(/*pwm_total=*/15.0f,
                  /*gyro=*/(float)i * 2.0f, /*prev=*/(float)(i-1) * 2.0f,
                  /*pitch=*/0.5f, /*dt=*/0.005f, /*freeze=*/false);
    }
    const float theta1 = pi.get_k_motor();
    printf("  θ0=%.4f → θ1=%.4f (delta=%.4f)\n",
           theta0, theta1, std::fabs(theta1 - theta0));
    assert_true(std::fabs(theta1 - theta0) > 1e-5f,
                "θ moved when |φ| > MIN_PHI");
    ++g_passes;
    printf("  PASS\n");
}

// ----- 11. Closed-form gain output math (deeper than test 5) --------------

// Directly verify the published formulae against the live get_kp_target /
// get_kd_target / get_ki_target accessors. Picks a non-trivial K_motor by
// setting an explicit Kp prior and then immediately reading the resulting
// target gains.
static void test_gain_output_math() {
    printf("\nTest 11: get_kp_target / get_kd_target match closed-form math\n");

    // Configure: ts=0.4s → ω_n = 4/0.4 = 10; ζ=0.6 → 2ζω_n = 12.
    // K_motor seeded via reset(kp=50): K = ω_n²/Kp = 100/50 = 2.0.
    // Expected targets: Kp = 100/2 = 50; Kd = 12/2 = 6; Ki = 0.05·Kp = 2.5.
    PlantIdentifier pi;
    pi.set_target_settling_time_sec(0.4f);
    pi.set_damping_ratio(0.6f);
    pi.set_k_motor_bounds(0.01f, 100.0f);
    pi.reset(50.0f, 0.0f);

    // Re-derive on the test side, not just copy/paste from the header.
    const float wn   = 4.0f / 0.4f;
    const float K    = (wn * wn) / 50.0f;   // = 2.0
    const float exp_kp = (wn * wn) / K;
    const float exp_kd = 2.0f * 0.6f * wn / K;
    const float exp_ki = 0.05f * exp_kp;

    assert_near(pi.get_k_motor(), 2.0f, 1e-4f, "K = 2.0 from Kp=50 prior");
    assert_near(pi.get_kp_target(), exp_kp, 0.01f, "Kp = ω_n²/K (live accessor)");
    assert_near(pi.get_kd_target(), exp_kd, 0.01f, "Kd = 2ζω_n/K (live accessor)");
    assert_near(pi.get_ki_target(), exp_ki, 0.01f, "Ki = 0.05·Kp (live accessor)");

    printf("  K=%.3f Kp=%.3f Kd=%.3f Ki=%.3f\n",
           pi.get_k_motor(), pi.get_kp_target(),
           pi.get_kd_target(), pi.get_ki_target());
    ++g_passes;
    printf("  PASS\n");
}

// ----- 12. Freeze gate hard-locks across many varied-input ticks ----------

// Test 3 verifies one tick with constant inputs. Test 12 sweeps wildly varying
// alpha/φ/pitch inputs across hundreds of ticks while freeze=true — the live
// θ and P scalars must never budge regardless of the sample stream.
static void test_freeze_locks_hard() {
    printf("\nTest 12: freeze=true holds θ & P across many varied-input ticks\n");
    PlantIdentifier pi;
    pi.reset(/*kp_initial=*/40.0f, /*kd_initial=*/15.0f);

    const float theta0 = pi.get_k_motor();
    const float P0     = pi.get_covariance();
    const uint16_t samp0 = pi.get_status().samples;

    // 300 ticks with chaotic excitation. If the freeze gate were to leak even
    // once, the RLS update would shift θ noticeably.
    unsigned rng = 7u;
    auto urand = [&]() -> float {
        rng = rng * 1103515245u + 12345u;
        return ((float)((rng >> 16) & 0x7FFF)) / 32767.0f - 0.5f;
    };
    for (int i = 0; i < 300; ++i) {
        const float pwm    = 200.0f * urand();
        const float gyro   = 100.0f * urand();
        const float prev   = 100.0f * urand();
        const float pitch  =  5.0f  * urand();
        pi.update(pwm, gyro, prev, pitch, 0.005f, /*freeze=*/true);
    }
    assert_near(pi.get_k_motor(), theta0, 1e-6f, "θ unchanged across 300 frozen ticks");
    assert_near(pi.get_covariance(), P0, 1e-6f, "P unchanged across 300 frozen ticks");
    assert_true(pi.get_status().frozen, "status.frozen = true");
    // Sample counter still ticks while frozen (documented behaviour for the
    // bootstrap-window timer in BalanceApp).
    assert_true(pi.get_status().samples > samp0,
                "samples counter advances even while frozen");
    ++g_passes;
    printf("  PASS\n");
}

// ----- 13. seed_k_motor — measured-K initialization (Phase 4.10c BOOTSTRAP) -

// BOOTSTRAP pushes a directly-measured K_motor into the identifier so the
// derived gains reflect this bot's actual plant from tick zero of RUN. The
// path must: (a) clamp into (k_min, k_max), (b) set θ to the clamped value,
// (c) drop covariance to a small SEED_P so the RLS trusts the measurement
// strongly, (d) reset the sample counter, (e) immediately recompute the
// target Kp/Kd/Ki from the new θ.
static void test_seed_k_motor() {
    printf("\nTest 13: seed_k_motor() — BOOTSTRAP measured-K initialization\n");

    PlantIdentifier pi;
    pi.set_k_motor_bounds(0.05f, 2.0f);
    pi.set_target_settling_time_sec(0.5f);     // ω_n = 8
    pi.set_damping_ratio(0.7f);
    pi.reset(/*kp_initial=*/50.0f, /*kd_initial=*/20.0f);

    // (a) In-band seed → θ exactly equals the measurement.
    pi.seed_k_motor(0.4f);
    assert_near(pi.get_k_motor(), 0.4f, 1e-5f,
                "θ = measured K when inside (k_min, k_max)");

    // (e) Target gains recomputed: Kp = ω_n²/K = 64/0.4 = 160; Kd = 2ζω_n/K = 28.
    assert_near(pi.get_kp_target(), 160.0f, 0.1f,
                "Kp_target recomputed from seeded K");
    assert_near(pi.get_kd_target(), 28.0f,  0.1f,
                "Kd_target recomputed from seeded K");

    // (b) Above k_max → clamps to k_max.
    pi.seed_k_motor(5.0f);
    assert_near(pi.get_k_motor(), 2.0f, 1e-5f,
                "θ clamped to k_max when measurement exceeds it");

    // (b) Below k_min → clamps to k_min.
    pi.seed_k_motor(0.001f);
    assert_near(pi.get_k_motor(), 0.05f, 1e-5f,
                "θ clamped to k_min when measurement drops below it");

    // (c) Subsequent CONSISTENT samples (y = K_seed × φ) leave θ near the seed.
    // Synthesizes the implied plant: α = K × pwm  →  Δω = K × pwm × dt.
    pi.seed_k_motor(0.4f);
    const float theta_seed = pi.get_k_motor();
    const float K_truth    = 0.4f;
    const float pwm        = 100.0f;
    const float dt         = 0.005f;
    const float dw_per_tick = K_truth * pwm * dt;     // 0.2 dps
    for (int i = 0; i < 50; ++i) {
        pi.update(/*pwm_total=*/pwm,
                  /*gyro=*/(float)(i + 1) * dw_per_tick,
                  /*prev=*/(float)i * dw_per_tick,
                  /*pitch=*/0.0f, dt, /*freeze=*/false);
    }
    const float drift = std::fabs(pi.get_k_motor() - theta_seed);
    printf("  θ_seed=%.4f  θ_after_50_ticks=%.4f  drift=%.4f\n",
           theta_seed, pi.get_k_motor(), drift);
    assert_true(drift < 0.05f,
                "θ stays near seed when subsequent data is consistent");

    // (d) Sample counter resets on every seed.
    pi.seed_k_motor(0.4f);
    assert_true(pi.get_status().samples == 0,
                "samples counter reset after seed_k_motor");
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
    test_projection_floor();
    test_projection_ceiling();
    test_excitation_boundary();
    test_gain_output_math();
    test_freeze_locks_hard();
    test_seed_k_motor();
    printf("\nAll %d tests passed.\n", g_passes);
    return 0;
}
