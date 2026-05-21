/**
 * Unit tests for the Phase 4M.14 outer-loop gain auto-derivation, as exposed
 * through the cleanly host-testable surface of PositionLoop.
 *
 * Phase 4M.14 (Workstream F.3 — phase_4m14_landed_2026-05-20.md) split the
 * five Phase 4M.13 hardcoded PositionLoop constants: K_POS / K_VEL / POS_LEAK
 * became *_FALLBACK constants plus live member variables installed at BOOTSTRAP
 * finalise via the new set_gains() / set_pos_leak() API; MAX_NUDGE_DEG and
 * SLEW_DEG_S stayed hardcoded safety saturations.
 *
 * The actual closed-form derivation lives in BalanceApp::derive_position_gains_(),
 * which is a PRIVATE method of a class that drags in the full Arduino-dependent
 * balance stack — it is NOT host-reachable from a control-layer unit test. So
 * this file tests the parts that ARE cleanly host-testable:
 *   - the new PositionLoop set_gains() / set_pos_leak() / inspector API,
 *   - the *_FALLBACK constants and their sane range,
 *   - reset()'s 4M.14 contract (preserves gains, clears integrator state),
 *   - the magnitude clamp + slew limit still holding after set_gains().
 * and, for the derivation itself, MIRRORS (does NOT invoke) the pure
 * pole-placement arithmetic with the SAME named constants the source uses
 * (see derive_position_gains_mirror() below — clearly commented as a mirror).
 *
 * PositionLoop is a pure control module (no <Arduino.h>, no STL, no dynamic
 * allocation), so it is cleanly host-testable: just compile this file against
 * src/control/position_loop.cpp.
 *
 * Compile + run (resource-capped — O1, single g++, timeouts):
 *   cd /home/devel/floppi/auto_orientation && \
 *   timeout 90 g++ -std=c++11 -O1 -fpermissive -Iinclude -Isrc \
 *       -o /tmp/test_position_gain_derivation_run \
 *       tests/test_position_gain_derivation.cpp src/control/position_loop.cpp && \
 *   timeout 30 /tmp/test_position_gain_derivation_run
 *
 * This test deliberately uses a tiny self-contained assertion harness rather
 * than Unity: the verification recipe compiles only this file + the module
 * under test, with no Unity object on the link line.
 *
 * Coverage (one function per scenario, all run from main()):
 *   1. test_default_gains_are_fallback   - fresh loop reports the *_FALLBACK gains
 *   2. test_set_gains_applies            - set_gains() updates inspectors + the law
 *   3. test_set_pos_leak_guard           - (0,1) accepted; <=0 / >=1 rejected
 *   4. test_reset_preserves_gains        - reset() keeps gains, clears integrator
 *   5. test_fallback_values_sane         - *_FALLBACK constants positive + plausible
 *   6. test_derived_gain_plausibility    - MIRRORED pole-placement lands in range
 *   7. test_clamp_and_slew_after_set     - 4M.13 clamp + slew still hold post-set
 *
 * Author: ao-test-4m14@floppi:1
 * Date:   2026-05-20
 */

#include <cmath>
#include <cstdio>

#include "control/position_loop.h"

// ---------------------------------------------------------------------------
// Tiny assertion harness
// ---------------------------------------------------------------------------

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_checks_failed_this_test = 0;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            ++g_checks_failed_this_test;                                      \
            std::printf("    FAIL: %s  (%s:%d)\n", (msg), __FILE__, __LINE__); \
        }                                                                     \
    } while (0)

#define CHECK_NEAR(actual, expected, tol, msg)                                \
    do {                                                                      \
        float a_ = (actual), e_ = (expected), t_ = (tol);                     \
        if (!(std::fabs(a_ - e_) <= t_)) {                                    \
            ++g_checks_failed_this_test;                                      \
            std::printf("    FAIL: %s  expected ~%.6f got %.6f  (%s:%d)\n",   \
                        (msg), e_, a_, __FILE__, __LINE__);                   \
        }                                                                     \
    } while (0)

#define RUN(testfn)                                                           \
    do {                                                                      \
        ++g_tests_run;                                                        \
        g_checks_failed_this_test = 0;                                        \
        std::printf("[ RUN  ] %s\n", #testfn);                                \
        testfn();                                                             \
        if (g_checks_failed_this_test == 0) {                                 \
            ++g_tests_passed;                                                 \
            std::printf("[ PASS ] %s\n", #testfn);                            \
        } else {                                                              \
            std::printf("[ FAIL ] %s (%d checks failed)\n",                   \
                        #testfn, g_checks_failed_this_test);                  \
        }                                                                     \
    } while (0)

// Common tolerance for analytic checks.
static const float kEps = 1e-4f;

// Realistic PID tick used throughout: 5 ms.
static const float kDt = 0.005f;

// ---------------------------------------------------------------------------
// MIRROR of the Phase 4M.14 pole-placement arithmetic.
//
// This is NOT the source. BalanceApp::derive_position_gains_() is a private
// method on a heavyweight Arduino-dependent class and cannot be linked into a
// control-layer host test. The arithmetic below RE-IMPLEMENTS, for test
// purposes only, the pure closed-form derivation documented in
// phase_4m14_landed_2026-05-20.md "The derivation as implemented" and the
// balance_app.h Phase 4M.14 comment block. The named-constant VALUES are
// copied from balance_app.h (POSLOOP_INNER_OUTER_BW_RATIO, POSLOOP_OUTER_DAMPING,
// POSLOOP_WASHOUT_TAU_S, POSLOOP_INNER_TS_SEC) so a drift in the source
// numerics would show up here as a range failure. If the source's
// derivation home (balance_app) ever becomes cleanly host-linkable, prefer
// invoking the real method over this mirror.
//
// Per the landed doc's DEVIATION note, the implementation uses the
// gravitational lean-to-acceleration gain g_lean = g (9.81 m/s^2 per rad),
// NOT the angular tipping coefficient g_eff. This mirror follows the
// implementation.
//
// The derivation method itself is under independent review (OQ-1 in the
// landed doc), so the assertions on this mirror check RANGES, not exact
// values.
// ---------------------------------------------------------------------------

// Values mirrored from balance_app.h's Phase 4M.14 USE_WHEEL_ENCODERS block.
static const float kMirrorBwRatio    = 8.0f;   // POSLOOP_INNER_OUTER_BW_RATIO
static const float kMirrorOuterZeta  = 1.0f;   // POSLOOP_OUTER_DAMPING
static const float kMirrorWashoutTau = 20.0f;  // POSLOOP_WASHOUT_TAU_S (s)
static const float kMirrorInnerTs    = 0.5f;   // POSLOOP_INNER_TS_SEC (s)
// Gravitational lean-to-acceleration gain (m/s^2 per rad). Per the landed
// doc's DEVIATION note the implementation uses g, not g_eff.
static const float kMirrorGravity    = 9.81f;
static const float kMirrorDegToRad   = 3.14159265358979323846f / 180.0f;

struct MirroredGains {
    float k_pos;
    float k_vel;
    float pos_leak;
};

// Mirrors — does NOT invoke — the source pole-placement derivation.
static MirroredGains derive_position_gains_mirror(float dt_sec) {
    // Inner-loop natural frequency: omega_n,inner = 4 / ts.
    const float omega_n_inner = 4.0f / kMirrorInnerTs;            // 8 rad/s
    // Outer-loop frequency by bandwidth separation.
    const float omega_o = omega_n_inner / kMirrorBwRatio;          // 1 rad/s
    // Outer-plant gain G_outer = g_lean * (pi/180) -- m/s^2 per DEGREE.
    const float g_outer = kMirrorGravity * kMirrorDegToRad;
    // Pole placement: match s^2 + G*K_VEL*s + G*K_POS to
    // s^2 + 2*zeta_o*omega_o*s + omega_o^2.
    MirroredGains g;
    g.k_pos = (omega_o * omega_o) / g_outer;
    g.k_vel = (2.0f * kMirrorOuterZeta * omega_o) / g_outer;
    // POS_LEAK = exp(-dt/tau).
    g.pos_leak = std::exp(-dt_sec / kMirrorWashoutTau);
    return g;
}

// ---------------------------------------------------------------------------
// Test 1: default gains equal the *_FALLBACK constants
// ---------------------------------------------------------------------------

void test_default_gains_are_fallback(void) {
    // A freshly-constructed PositionLoop seeds its operating gains from the
    // *_FALLBACK constants (the conservative Phase 4M.13 hand-picked values).
    PositionLoop loop;

    CHECK_NEAR(loop.k_pos(), POSLOOP_K_POS_FALLBACK, kEps,
               "fresh loop k_pos() must equal POSLOOP_K_POS_FALLBACK");
    CHECK_NEAR(loop.k_vel(), POSLOOP_K_VEL_FALLBACK, kEps,
               "fresh loop k_vel() must equal POSLOOP_K_VEL_FALLBACK");
    CHECK_NEAR(loop.pos_leak(), POSLOOP_POS_LEAK_FALLBACK, kEps,
               "fresh loop pos_leak() must equal POSLOOP_POS_LEAK_FALLBACK");

    // The 4M.13 values, cross-checked against phase_4m14_landed_2026-05-20.md
    // ("the 4M.13 values 6.0 / 3.0 / 0.999").
    CHECK_NEAR(POSLOOP_K_POS_FALLBACK, 6.0f, kEps,
               "POSLOOP_K_POS_FALLBACK must be the 4M.13 value 6.0");
    CHECK_NEAR(POSLOOP_K_VEL_FALLBACK, 3.0f, kEps,
               "POSLOOP_K_VEL_FALLBACK must be the 4M.13 value 3.0");
    CHECK_NEAR(POSLOOP_POS_LEAK_FALLBACK, 0.999f, kEps,
               "POSLOOP_POS_LEAK_FALLBACK must be the 4M.13 value 0.999");

    // reset() must not change the freshly-seeded gains either.
    loop.reset();
    CHECK_NEAR(loop.k_pos(), POSLOOP_K_POS_FALLBACK, kEps,
               "reset() must not change k_pos() on a fresh loop");
    CHECK_NEAR(loop.k_vel(), POSLOOP_K_VEL_FALLBACK, kEps,
               "reset() must not change k_vel() on a fresh loop");
    CHECK_NEAR(loop.pos_leak(), POSLOOP_POS_LEAK_FALLBACK, kEps,
               "reset() must not change pos_leak() on a fresh loop");
}

// ---------------------------------------------------------------------------
// Test 2: set_gains() applies — inspectors AND the control law
// ---------------------------------------------------------------------------

void test_set_gains_applies(void) {
    PositionLoop loop;
    loop.reset();

    // Pick gains plainly distinct from the 6.0 / 3.0 fallbacks.
    const float new_kp = 10.0f;
    const float new_kv = 5.0f;
    loop.set_gains(new_kp, new_kv);

    // Inspectors reflect the new gains; pos_leak is untouched by set_gains().
    CHECK_NEAR(loop.k_pos(), new_kp, kEps, "set_gains() must update k_pos()");
    CHECK_NEAR(loop.k_vel(), new_kv, kEps, "set_gains() must update k_vel()");
    CHECK_NEAR(loop.pos_leak(), POSLOOP_POS_LEAK_FALLBACK, kEps,
               "set_gains() must NOT touch pos_leak()");

    // A subsequent update() must use the new gains. Use a very small velocity
    // so the unclamped/unslewed law value stays inside BOTH the magnitude clamp
    // AND one slew step (max_step = SLEW_DEG_S*dt = 0.01 deg), so update()
    // returns the raw law value rather than the slew-limited value. NOTE: the
    // velocity term alone is -k_vel*v, so v must satisfy |k_vel*v| < max_step
    // with margin for the (tiny) position term — hence v = 0.001 m/s here.
    //   nudge = -k_pos*pos - k_vel*v,  pos = (v*dt)*pos_leak
    const float v   = 0.001f;
    const float L   = loop.pos_leak();
    const float pos_expected = (v * kDt) * L;
    const float law_expected = -(new_kp * pos_expected) - (new_kv * v);
    const float max_step     = POSLOOP_SLEW_DEG_S * kDt;

    // Confirm the test setup keeps the law value inside both saturations so
    // update() returns the raw law output.
    CHECK(std::fabs(law_expected) < POSLOOP_MAX_NUDGE_DEG,
          "test setup: law value must be inside the magnitude clamp");
    CHECK(std::fabs(law_expected) < max_step,
          "test setup: law value must be inside one slew step");

    float nudge = loop.update(v, kDt);
    CHECK_NEAR(nudge, law_expected, kEps,
               "update() after set_gains() must use the NEW K_POS/K_VEL");
    CHECK_NEAR(loop.position_m(), pos_expected, kEps,
               "position integrator: pos = v*dt*pos_leak");

    // Cross-check: the same input under the FALLBACK gains gives a different
    // (smaller-magnitude) nudge — proves set_gains() genuinely took effect.
    PositionLoop ref;
    ref.reset();
    float ref_nudge = ref.update(v, kDt);
    CHECK(std::fabs(nudge) > std::fabs(ref_nudge),
          "kp=10/kv=5 must yield a larger-magnitude nudge than the 6.0/3.0 "
          "fallback for the same input");
}

// ---------------------------------------------------------------------------
// Test 3: set_pos_leak() guard — (0,1) accepted, <=0 / >=1 rejected
// ---------------------------------------------------------------------------

void test_set_pos_leak_guard(void) {
    // position_loop.cpp:46 — the guard accepts a leak strictly inside (0,1)
    // and SILENTLY rejects anything else, keeping the prior value.
    {
        PositionLoop loop;
        // A valid in-range leak is accepted.
        loop.set_pos_leak(0.995f);
        CHECK_NEAR(loop.pos_leak(), 0.995f, kEps,
                   "set_pos_leak(0.995) must be accepted");
        // Another valid value overwrites cleanly.
        loop.set_pos_leak(0.9999f);
        CHECK_NEAR(loop.pos_leak(), 0.9999f, kEps,
                   "set_pos_leak(0.9999) must be accepted");
    }
    {
        // leak == 1.0 (boundary, >= 1) rejected — would freeze the integrator.
        PositionLoop loop;
        loop.set_pos_leak(0.995f);          // known-good prior value
        loop.set_pos_leak(1.0f);
        CHECK_NEAR(loop.pos_leak(), 0.995f, kEps,
                   "set_pos_leak(1.0) must be rejected, prior value kept");
    }
    {
        // leak > 1 rejected — would wind up the integrator unbounded.
        PositionLoop loop;
        loop.set_pos_leak(0.995f);
        loop.set_pos_leak(1.5f);
        CHECK_NEAR(loop.pos_leak(), 0.995f, kEps,
                   "set_pos_leak(1.5) must be rejected, prior value kept");
    }
    {
        // leak == 0.0 (boundary, <= 0) rejected — would zero the integrator.
        PositionLoop loop;
        loop.set_pos_leak(0.995f);
        loop.set_pos_leak(0.0f);
        CHECK_NEAR(loop.pos_leak(), 0.995f, kEps,
                   "set_pos_leak(0.0) must be rejected, prior value kept");
    }
    {
        // leak < 0 rejected — would invert the integrator.
        PositionLoop loop;
        loop.set_pos_leak(0.995f);
        loop.set_pos_leak(-0.5f);
        CHECK_NEAR(loop.pos_leak(), 0.995f, kEps,
                   "set_pos_leak(-0.5) must be rejected, prior value kept");
    }
    {
        // A rejected leak on a fresh loop must leave the *_FALLBACK in force.
        PositionLoop loop;
        loop.set_pos_leak(2.0f);
        CHECK_NEAR(loop.pos_leak(), POSLOOP_POS_LEAK_FALLBACK, kEps,
                   "rejected leak on a fresh loop must keep the fallback leak");
    }
}

// ---------------------------------------------------------------------------
// Test 4: reset() preserves gains, clears integrator state
// ---------------------------------------------------------------------------

void test_reset_preserves_gains(void) {
    // Phase 4M.14 contract (position_loop.cpp:29-35 + the set_gains() docblock):
    // gains are installed once at BOOTSTRAP finalise and MUST survive every
    // RUN-entry reset(); reset() only clears the integrator + slew memory.
    PositionLoop loop;
    loop.reset();

    const float set_kp   = 12.5f;
    const float set_kv   = 7.25f;
    const float set_leak = 0.9975f;
    loop.set_gains(set_kp, set_kv);
    loop.set_pos_leak(set_leak);

    // Build up real integrator + slew state so we can prove reset() clears it.
    for (int i = 0; i < 500; ++i) {
        loop.update(0.30f, kDt);
    }
    CHECK(std::fabs(loop.position_m()) > kEps,
          "position should be non-zero before reset (state actually built up)");
    CHECK(std::fabs(loop.last_nudge_deg()) > kEps,
          "nudge should be non-zero before reset");

    loop.reset();

    // The integrator + slew memory are cleared...
    CHECK_NEAR(loop.position_m(), 0.0f, kEps,
               "reset() must clear the position integrator");
    CHECK_NEAR(loop.last_nudge_deg(), 0.0f, kEps,
               "reset() must clear the cached nudge");

    // ...but the gains installed by set_gains()/set_pos_leak() MUST persist.
    CHECK_NEAR(loop.k_pos(), set_kp, kEps,
               "reset() must PRESERVE the set k_pos (4M.14 contract)");
    CHECK_NEAR(loop.k_vel(), set_kv, kEps,
               "reset() must PRESERVE the set k_vel (4M.14 contract)");
    CHECK_NEAR(loop.pos_leak(), set_leak, kEps,
               "reset() must PRESERVE the set pos_leak (4M.14 contract)");

    // And the preserved gains are still the ones used by update() after reset.
    // v is kept tiny so the first-tick law value stays inside one slew step
    // (|set_kv*v| < SLEW_DEG_S*dt = 0.01), so update() returns the raw law
    // value and the assertion can compare against the closed-form expectation.
    const float v   = 0.001f;
    const float pos_expected = (v * kDt) * set_leak;
    const float law_expected = -(set_kp * pos_expected) - (set_kv * v);
    float nudge = loop.update(v, kDt);
    CHECK_NEAR(nudge, law_expected, kEps,
               "after reset() update() must still use the preserved gains");
}

// ---------------------------------------------------------------------------
// Test 5: *_FALLBACK constants are positive and physically plausible
// ---------------------------------------------------------------------------

void test_fallback_values_sane(void) {
    // The fallbacks must be strictly positive — a non-positive position or
    // velocity gain would invert/disable the corrective control law, and a
    // non-positive leak would break the integrator (see the set_pos_leak guard).
    CHECK(POSLOOP_K_POS_FALLBACK > 0.0f, "POSLOOP_K_POS_FALLBACK must be positive");
    CHECK(POSLOOP_K_VEL_FALLBACK > 0.0f, "POSLOOP_K_VEL_FALLBACK must be positive");
    CHECK(POSLOOP_POS_LEAK_FALLBACK > 0.0f,
          "POSLOOP_POS_LEAK_FALLBACK must be positive");

    // The leak is a per-tick exponential factor — must be strictly inside
    // (0,1): >=1 winds up unbounded, <=0 inverts. (Mirrors the runtime guard.)
    CHECK(POSLOOP_POS_LEAK_FALLBACK < 1.0f,
          "POSLOOP_POS_LEAK_FALLBACK must be < 1 (a leak, not a hold/grow)");

    // Physically-plausible bench-class range. These envelopes mirror the
    // POSLOOP_*_MIN/MAX sanity-clamp windows documented in balance_app.h
    // (K_POS [1,30], K_VEL [0.5,15], LEAK [0.990,0.9999]). The fallbacks are
    // the centre of those windows by design, so they MUST sit inside them.
    CHECK(POSLOOP_K_POS_FALLBACK >= 1.0f && POSLOOP_K_POS_FALLBACK <= 30.0f,
          "POSLOOP_K_POS_FALLBACK must sit inside the [1,30] sanity envelope");
    CHECK(POSLOOP_K_VEL_FALLBACK >= 0.5f && POSLOOP_K_VEL_FALLBACK <= 15.0f,
          "POSLOOP_K_VEL_FALLBACK must sit inside the [0.5,15] sanity envelope");
    CHECK(POSLOOP_POS_LEAK_FALLBACK >= 0.990f &&
          POSLOOP_POS_LEAK_FALLBACK <= 0.9999f,
          "POSLOOP_POS_LEAK_FALLBACK must sit inside the [0.990,0.9999] "
          "sanity envelope");

    // Safety saturations stay hardcoded by design — confirm they remain the
    // 4M.13 values and are positive (they are not loop gains, just bounds).
    CHECK(POSLOOP_MAX_NUDGE_DEG > 0.0f, "POSLOOP_MAX_NUDGE_DEG must be positive");
    CHECK(POSLOOP_SLEW_DEG_S > 0.0f, "POSLOOP_SLEW_DEG_S must be positive");
    CHECK_NEAR(POSLOOP_MAX_NUDGE_DEG, 2.0f, kEps,
               "POSLOOP_MAX_NUDGE_DEG must remain the 4M.13 value 2.0");
    CHECK_NEAR(POSLOOP_SLEW_DEG_S, 2.0f, kEps,
               "POSLOOP_SLEW_DEG_S must remain the 4M.13 value 2.0");

    // Back-compat aliases must still resolve to the fallback values.
    CHECK_NEAR(POSLOOP_K_POS, POSLOOP_K_POS_FALLBACK, kEps,
               "back-compat alias POSLOOP_K_POS must equal the fallback");
    CHECK_NEAR(POSLOOP_K_VEL, POSLOOP_K_VEL_FALLBACK, kEps,
               "back-compat alias POSLOOP_K_VEL must equal the fallback");
    CHECK_NEAR(POSLOOP_POS_LEAK, POSLOOP_POS_LEAK_FALLBACK, kEps,
               "back-compat alias POSLOOP_POS_LEAK must equal the fallback");
}

// ---------------------------------------------------------------------------
// Test 6: derived-gain plausibility — RANGE check on the MIRRORED arithmetic
// ---------------------------------------------------------------------------

void test_derived_gain_plausibility(void) {
    // IMPORTANT: this exercises derive_position_gains_mirror() — a test-local
    // RE-IMPLEMENTATION of the pole-placement arithmetic, NOT the source
    // BalanceApp::derive_position_gains_() (private, not host-linkable). The
    // derivation METHOD is under independent review (OQ-1 in the landed doc),
    // so every assertion below checks a RANGE, never an exact value.
    MirroredGains g = derive_position_gains_mirror(kDt);

    // All three derived quantities must be finite and positive.
    CHECK(std::isfinite(g.k_pos) && g.k_pos > 0.0f,
          "mirrored K_POS must be finite and positive");
    CHECK(std::isfinite(g.k_vel) && g.k_vel > 0.0f,
          "mirrored K_VEL must be finite and positive");
    CHECK(std::isfinite(g.pos_leak) && g.pos_leak > 0.0f,
          "mirrored POS_LEAK must be finite and positive");

    // The derived gains must pass the POSLOOP_*_MIN/MAX sanity envelope from
    // balance_app.h — if they did not, the source would reject them and fall
    // back. The landed doc reports K_POS ~5.84, K_VEL ~11.68, POS_LEAK ~0.99975
    // for the nominal chassis; all three are quoted as passing the §7.1 clamp.
    CHECK(g.k_pos >= 1.0f && g.k_pos <= 30.0f,
          "mirrored K_POS must land inside the [1,30] sanity envelope");
    CHECK(g.k_vel >= 0.5f && g.k_vel <= 15.0f,
          "mirrored K_VEL must land inside the [0.5,15] sanity envelope");
    CHECK(g.pos_leak >= 0.990f && g.pos_leak <= 0.9999f,
          "mirrored POS_LEAK must land inside the [0.990,0.9999] sanity "
          "envelope");

    // Cross-check against the landed-doc nominal values, with generous
    // tolerances — the derivation may yet be corrected under review, so this
    // is a sanity band, not a precise expectation.
    CHECK_NEAR(g.k_pos, 5.84f, 0.5f,
               "mirrored K_POS should be near the landed-doc nominal ~5.84");
    CHECK_NEAR(g.k_vel, 11.68f, 1.0f,
               "mirrored K_VEL should be near the landed-doc nominal ~11.68");
    CHECK_NEAR(g.pos_leak, 0.99975f, 1e-3f,
               "mirrored POS_LEAK should be near the landed-doc nominal "
               "~0.99975");

    // Pole-placement structural relation: with zeta_o = 1 (critical damping)
    // K_VEL = 2*zeta_o*omega_o/G and K_POS = omega_o^2/G, so for omega_o = 1
    // rad/s the ratio K_VEL/K_POS = 2*zeta_o/omega_o = 2. Verify the mirrored
    // arithmetic obeys that relation.
    CHECK_NEAR(g.k_vel / g.k_pos, 2.0f, 1e-3f,
               "mirrored K_VEL/K_POS must equal 2*zeta_o/omega_o = 2");

    // A derived gain set must be installable into PositionLoop and produce a
    // sane, bounded nudge — i.e. the derivation output is API-compatible.
    PositionLoop loop;
    loop.reset();
    loop.set_gains(g.k_pos, g.k_vel);
    loop.set_pos_leak(g.pos_leak);
    CHECK_NEAR(loop.k_pos(), g.k_pos, kEps,
               "derived K_POS must install via set_gains()");
    CHECK_NEAR(loop.k_vel(), g.k_vel, kEps,
               "derived K_VEL must install via set_gains()");
    CHECK_NEAR(loop.pos_leak(), g.pos_leak, kEps,
               "derived POS_LEAK must install via set_pos_leak()");
    for (int i = 0; i < 1000; ++i) {
        float nudge = loop.update(0.15f, kDt);
        CHECK(std::isfinite(nudge), "derived-gain loop nudge must be finite");
        CHECK(std::fabs(nudge) <= POSLOOP_MAX_NUDGE_DEG + kEps,
              "derived-gain loop nudge must respect the magnitude clamp");
    }
}

// ---------------------------------------------------------------------------
// Test 7: 4M.13 magnitude clamp + slew limit still hold after set_gains()
// ---------------------------------------------------------------------------

void test_clamp_and_slew_after_set(void) {
    // The safety saturations are NOT loop gains; set_gains()/set_pos_leak()
    // must not weaken them. Install large gains and confirm update() still
    // magnitude-clamps and slew-limits exactly as in Phase 4M.13.
    PositionLoop loop;
    loop.reset();
    loop.set_gains(25.0f, 12.0f);            // large but inside the sanity envelope
    loop.set_pos_leak(0.999f);

    const float dt       = kDt;
    const float max_step = POSLOOP_SLEW_DEG_S * dt;   // 2 deg/s * 5 ms = 0.01 deg

    // Slew: a step input that WOULD command a huge nudge must not let the
    // output move more than SLEW_DEG_S*dt in a single update().
    float prev = loop.last_nudge_deg();               // 0 after reset
    float nudge = 0.0f;
    for (int i = 0; i < 2000; ++i) {                  // 10 s at 5 ms
        nudge = loop.update(5.0f, dt);                // unrealistically fast on purpose
        float jump = std::fabs(nudge - prev);
        CHECK(jump <= max_step + kEps,
              "post-set_gains: output must not jump more than SLEW_DEG_S*dt");
        CHECK(nudge >= -POSLOOP_MAX_NUDGE_DEG - kEps,
              "post-set_gains: nudge must never go below -MAX_NUDGE_DEG");
        CHECK(nudge <= POSLOOP_MAX_NUDGE_DEG + kEps,
              "post-set_gains: nudge must never go above +MAX_NUDGE_DEG");
        prev = nudge;
    }
    // After a long sustained large drive the nudge must be pinned on the rail.
    CHECK_NEAR(nudge, -POSLOOP_MAX_NUDGE_DEG, kEps,
               "post-set_gains: sustained large +velocity must saturate at "
               "-MAX_NUDGE_DEG");

    // Symmetric: large negative velocity saturates at +MAX_NUDGE_DEG.
    loop.reset();
    for (int i = 0; i < 2000; ++i) {
        nudge = loop.update(-5.0f, dt);
    }
    CHECK_NEAR(nudge, POSLOOP_MAX_NUDGE_DEG, kEps,
               "post-set_gains: sustained large -velocity must saturate at "
               "+MAX_NUDGE_DEG");

    // Slew step must still scale with dt after set_gains().
    loop.reset();
    const float big_dt = 0.050f;                      // 50 ms
    float n0 = loop.last_nudge_deg();
    float n1 = loop.update(10.0f, big_dt);
    CHECK(std::fabs(n1 - n0) <= POSLOOP_SLEW_DEG_S * big_dt + kEps,
          "post-set_gains: slew step must still scale with dt");
}

// ---------------------------------------------------------------------------
// main — run every scenario, report, exit 0/nonzero
// ---------------------------------------------------------------------------

int main(void) {
    std::printf("=== PositionLoop gain-derivation tests (Phase 4M.14) ===\n");
    std::printf("fallbacks: K_POS=%.3f K_VEL=%.3f POS_LEAK=%.4f  "
                "saturations: MAX_NUDGE_DEG=%.3f SLEW_DEG_S=%.3f\n",
                POSLOOP_K_POS_FALLBACK, POSLOOP_K_VEL_FALLBACK,
                POSLOOP_POS_LEAK_FALLBACK, POSLOOP_MAX_NUDGE_DEG,
                POSLOOP_SLEW_DEG_S);
    {
        MirroredGains g = derive_position_gains_mirror(kDt);
        std::printf("mirrored derivation @ dt=5ms: K_POS=%.4f K_VEL=%.4f "
                    "POS_LEAK=%.6f\n\n", g.k_pos, g.k_vel, g.pos_leak);
    }

    RUN(test_default_gains_are_fallback);
    RUN(test_set_gains_applies);
    RUN(test_set_pos_leak_guard);
    RUN(test_reset_preserves_gains);
    RUN(test_fallback_values_sane);
    RUN(test_derived_gain_plausibility);
    RUN(test_clamp_and_slew_after_set);

    int failed = g_tests_run - g_tests_passed;
    std::printf("\nTests run: %d, passed: %d, failed: %d\n",
                g_tests_run, g_tests_passed, failed);
    return (failed == 0) ? 0 : 1;
}
