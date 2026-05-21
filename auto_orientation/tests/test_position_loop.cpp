/**
 * Unit tests for PositionLoop — the Phase 4M.13 velocity/position outer loop.
 *
 * PositionLoop is a pure control module (no <Arduino.h>, no STL, no dynamic
 * allocation), so it is cleanly host-testable: just compile this file against
 * src/control/position_loop.cpp.
 *
 * Compile + run:
 *   cd /home/devel/floppi/auto_orientation
 *   g++ -std=c++11 -O2 -fpermissive -Iinclude -Isrc -o /tmp/test_position_loop_run \
 *       tests/test_position_loop.cpp src/control/position_loop.cpp
 *   /tmp/test_position_loop_run
 *
 * This test deliberately uses a tiny self-contained assertion harness rather
 * than Unity: the verification recipe compiles only this file + the module
 * under test, with no Unity object on the link line.
 *
 * Coverage (one function per scenario, all run from main()):
 *   1. test_zero_input_zero_nudge   - update(0,dt) after reset() ~= 0
 *   2. test_sign_convention         - +velocity -> negative (decelerating) nudge
 *   3. test_magnitude_clamp         - sustained drive saturates at +/-MAX_NUDGE_DEG
 *   4. test_slew_limit              - output cannot jump > SLEW_DEG_S*dt per call
 *   5. test_leaky_integrator        - constant small velocity stays bounded
 *   6. test_reset_clears_state      - reset() restores zero-in -> zero-out
 *   7. test_dt_robustness           - realistic 5 ms dt, plus dt<=0 guard, no NaN
 *   8. test_steady_state            - corrective nudge while moving, settles when stopped
 *
 * Author: test-writer-position-loop@floppi:1
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
// Test 1: zero input -> zero nudge
// ---------------------------------------------------------------------------

void test_zero_input_zero_nudge(void) {
    PositionLoop loop;
    loop.reset();

    // A fresh loop fed nothing but zero velocity must emit nothing.
    for (int i = 0; i < 200; ++i) {
        float nudge = loop.update(0.0f, kDt);
        CHECK_NEAR(nudge, 0.0f, kEps, "zero velocity must give zero nudge");
    }
    CHECK_NEAR(loop.position_m(), 0.0f, kEps, "position must stay 0 on zero input");
    CHECK_NEAR(loop.last_nudge_deg(), 0.0f, kEps, "cached nudge must stay 0");
}

// ---------------------------------------------------------------------------
// Test 2: sign convention
// ---------------------------------------------------------------------------

void test_sign_convention(void) {
    // Control law (position_loop.cpp:46):
    //   nudge = -K_POS*position - K_VEL*velocity
    // A positive (forward) wheel velocity must produce a NEGATIVE nudge so the
    // inner pitch loop leans the bot back and arrests the forward drift.
    {
        PositionLoop loop;
        loop.reset();
        // First call: position_m_ starts at 0, after the leaky integrator it is
        // (v*dt)*POS_LEAK -> tiny but positive. nudge = -K_POS*pos - K_VEL*v,
        // both terms negative for v>0.
        float nudge = loop.update(0.10f, kDt);  // 0.1 m/s forward
        CHECK(nudge < 0.0f, "positive velocity must yield a negative (decel) nudge");
    }
    {
        PositionLoop loop;
        loop.reset();
        float nudge = loop.update(-0.10f, kDt);  // backward
        CHECK(nudge > 0.0f, "negative velocity must yield a positive nudge");
    }

    // Quantitative check of the law on the very first update, before the slew
    // limit can bite. v must be small enough that |law| < one slew step
    // (SLEW_DEG_S*dt = 0.01 deg): the K_VEL*v term dominates the law, so
    // v=0.002 keeps |law| ~= 0.006 deg, comfortably inside the 0.01 step.
    // (v=0.02 here was a prior test-math bug — 3.0*0.02 = 0.06 alone busts it.)
    {
        PositionLoop loop;
        loop.reset();
        const float v  = 0.002f;                // small so |nudge| stays well
        const float dt = kDt;                   // inside the slew step
        float pos_expected   = (v * dt) * POSLOOP_POS_LEAK;
        float law_expected   = -(POSLOOP_K_POS * pos_expected)
                               - (POSLOOP_K_VEL * v);
        float max_step       = POSLOOP_SLEW_DEG_S * dt;
        // Confirm the unclamped/unslewed law value is small enough that neither
        // the magnitude clamp nor the slew limit alters it.
        CHECK(std::fabs(law_expected) < POSLOOP_MAX_NUDGE_DEG,
              "test setup: law value should be inside the magnitude clamp");
        CHECK(std::fabs(law_expected) < max_step,
              "test setup: law value should be inside one slew step");
        float nudge = loop.update(v, dt);
        CHECK_NEAR(nudge, law_expected, kEps,
                   "first-step output must equal -K_POS*pos - K_VEL*v");
        CHECK_NEAR(loop.position_m(), pos_expected, kEps,
                   "position integrator: pos = v*dt*POS_LEAK");
    }
}

// ---------------------------------------------------------------------------
// Test 3: magnitude clamp
// ---------------------------------------------------------------------------

void test_magnitude_clamp(void) {
    // A large sustained forward velocity drives the nudge to the negative rail.
    // The slew limiter means it takes several ticks to crawl there, so run long.
    PositionLoop loop;
    loop.reset();
    float nudge = 0.0f;
    for (int i = 0; i < 4000; ++i) {           // 20 s at 5 ms
        nudge = loop.update(5.0f, kDt);         // unrealistically fast on purpose
        CHECK(nudge >= -POSLOOP_MAX_NUDGE_DEG - kEps,
              "nudge must never go below -MAX_NUDGE_DEG");
        CHECK(nudge <=  POSLOOP_MAX_NUDGE_DEG + kEps,
              "nudge must never go above +MAX_NUDGE_DEG");
    }
    // After a long sustained drive it must be sitting exactly on the rail.
    CHECK_NEAR(nudge, -POSLOOP_MAX_NUDGE_DEG, kEps,
               "sustained large +velocity must saturate at -MAX_NUDGE_DEG");

    // Symmetric: large negative velocity saturates at +MAX_NUDGE_DEG.
    loop.reset();
    for (int i = 0; i < 4000; ++i) {
        nudge = loop.update(-5.0f, kDt);
    }
    CHECK_NEAR(nudge, POSLOOP_MAX_NUDGE_DEG, kEps,
               "sustained large -velocity must saturate at +MAX_NUDGE_DEG");
}

// ---------------------------------------------------------------------------
// Test 4: slew limit
// ---------------------------------------------------------------------------

void test_slew_limit(void) {
    // A step input that *would* command a large nudge must not let the output
    // move more than SLEW_DEG_S*dt in a single update().
    PositionLoop loop;
    loop.reset();

    const float dt       = kDt;
    const float max_step = POSLOOP_SLEW_DEG_S * dt;   // 2 deg/s * 5 ms = 0.01 deg

    float prev = loop.last_nudge_deg();               // 0 after reset
    for (int i = 0; i < 1000; ++i) {
        // 10 m/s would command a huge nudge -> magnitude-clamped, then slewed.
        float nudge = loop.update(10.0f, dt);
        float jump  = std::fabs(nudge - prev);
        CHECK(jump <= max_step + kEps,
              "output must not jump more than SLEW_DEG_S*dt per call");
        prev = nudge;
    }

    // Slew also scales with dt: a larger dt permits a proportionally larger
    // single-step move.
    loop.reset();
    const float big_dt = 0.050f;                      // 50 ms
    float n0 = loop.last_nudge_deg();
    float n1 = loop.update(10.0f, big_dt);
    CHECK(std::fabs(n1 - n0) <= POSLOOP_SLEW_DEG_S * big_dt + kEps,
          "slew step must scale with dt");
}

// ---------------------------------------------------------------------------
// Test 5: leaky integrator
// ---------------------------------------------------------------------------

void test_leaky_integrator(void) {
    // A constant small velocity (think: a steady encoder bias) must NOT make
    // the position integrator wind up without bound. POS_LEAK washes it out so
    // it converges to a finite steady-state.
    //
    // Steady state of  p <- (p + v*dt) * L  is  p* = v*dt*L / (1 - L).
    PositionLoop loop;
    loop.reset();

    const float v  = 0.01f;       // 1 cm/s steady bias
    const float dt = kDt;
    const float L  = POSLOOP_POS_LEAK;
    const float pos_steady = (v * dt * L) / (1.0f - L);

    // Run a long time — far longer than the ~5 s washout time constant.
    for (int i = 0; i < 20000; ++i) {    // 100 s at 5 ms
        loop.update(v, dt);
    }
    float pos = loop.position_m();
    CHECK(std::isfinite(pos), "position must remain finite (no windup to inf)");
    CHECK(pos < 10.0f * pos_steady,
          "leaky integrator must stay bounded, not grow without bound");
    CHECK_NEAR(pos, pos_steady, 1e-3f,
               "position must converge to v*dt*L/(1-L) steady state");

    // The emitted nudge is therefore also bounded and finite.
    float nudge = loop.last_nudge_deg();
    CHECK(std::isfinite(nudge), "nudge must remain finite");
    CHECK(std::fabs(nudge) <= POSLOOP_MAX_NUDGE_DEG + kEps,
          "nudge must stay within the magnitude clamp");

    // Contrast: WITHOUT a leak a 1 cm/s bias over 100 s would integrate to ~1 m.
    // With the leak the position term is far smaller than that unbounded value.
    CHECK(pos < 0.10f,
          "POS_LEAK must wash a small steady velocity down well below the "
          "un-leaked integral (v*t = 1 m over 100 s)");
}

// ---------------------------------------------------------------------------
// Test 6: reset() clears state
// ---------------------------------------------------------------------------

void test_reset_clears_state(void) {
    PositionLoop loop;
    loop.reset();

    // Build up real internal state with a sequence of non-trivial updates.
    for (int i = 0; i < 500; ++i) {
        loop.update(0.30f, kDt);
    }
    CHECK(std::fabs(loop.position_m()) > kEps,
          "position should be non-zero before reset (state actually built up)");
    CHECK(std::fabs(loop.last_nudge_deg()) > kEps,
          "nudge should be non-zero before reset");

    loop.reset();

    CHECK_NEAR(loop.position_m(), 0.0f, kEps, "reset must clear the integrator");
    CHECK_NEAR(loop.last_nudge_deg(), 0.0f, kEps,
               "reset must clear the cached nudge");

    // After reset the loop must be back in the zero-in -> zero-out condition.
    float nudge = loop.update(0.0f, kDt);
    CHECK_NEAR(nudge, 0.0f, kEps,
               "after reset, zero input must give zero output again");
}

// ---------------------------------------------------------------------------
// Test 7: dt robustness
// ---------------------------------------------------------------------------

void test_dt_robustness(void) {
    // Realistic 5 ms PID tick: a normal velocity must produce a finite, bounded
    // nudge with no NaN/Inf.
    {
        PositionLoop loop;
        loop.reset();
        for (int i = 0; i < 1000; ++i) {
            float nudge = loop.update(0.15f, 0.005f);
            CHECK(std::isfinite(nudge), "5 ms tick: nudge must be finite");
            CHECK(std::fabs(nudge) <= POSLOOP_MAX_NUDGE_DEG + kEps,
                  "5 ms tick: nudge must stay clamped");
        }
    }

    // dt == 0: clock-not-advanced guard. Must return the cached nudge unchanged
    // and must NOT divide-by-zero or corrupt state.
    {
        PositionLoop loop;
        loop.reset();
        float warm = loop.update(0.20f, kDt);          // warm up a non-zero cache
        float pos_before = loop.position_m();
        float held = loop.update(0.20f, 0.0f);
        CHECK_NEAR(held, warm, kEps,
                   "dt==0 must return the cached nudge unchanged");
        CHECK(std::isfinite(held), "dt==0 result must be finite (no NaN)");
        CHECK_NEAR(loop.position_m(), pos_before, kEps,
                   "dt==0 must not advance the integrator");
    }

    // Negative dt: same guard path.
    {
        PositionLoop loop;
        loop.reset();
        float warm = loop.update(0.20f, kDt);
        float held = loop.update(0.20f, -0.01f);
        CHECK_NEAR(held, warm, kEps,
                   "negative dt must return the cached nudge unchanged");
    }

    // Tiny positive dt: must still be finite (no division blow-up). The module
    // multiplies by dt and never divides by it, so this is well-behaved.
    {
        PositionLoop loop;
        loop.reset();
        float nudge = loop.update(0.20f, 1e-6f);
        CHECK(std::isfinite(nudge), "tiny dt must not produce NaN/Inf");
        // With a microsecond dt the integrator and slew step are both minuscule,
        // so the velocity term dominates: nudge ~= -K_VEL*v, but slew-limited to
        // SLEW_DEG_S*dt. Just confirm it is finite and inside the clamp.
        CHECK(std::fabs(nudge) <= POSLOOP_MAX_NUDGE_DEG + kEps,
              "tiny dt: nudge still within the magnitude clamp");
    }
}

// ---------------------------------------------------------------------------
// Test 8: steady-state — moving then stopped
// ---------------------------------------------------------------------------

void test_steady_state(void) {
    // A bot drifting at a constant velocity, then brought to a stop. While
    // moving the loop should command a corrective (negative, decelerating)
    // nudge; once stopped, the leaky integrator washes the position term out
    // and the nudge settles back toward zero.
    PositionLoop loop;
    loop.reset();

    // Phase A: 4 s of steady forward drift at 0.08 m/s.
    float nudge_moving = 0.0f;
    for (int i = 0; i < 800; ++i) {
        nudge_moving = loop.update(0.08f, kDt);
    }
    CHECK(nudge_moving < 0.0f,
          "while drifting forward the loop must command a corrective "
          "(backward-lean) nudge");
    CHECK(std::fabs(nudge_moving) <= POSLOOP_MAX_NUDGE_DEG + kEps,
          "moving-phase nudge must respect the magnitude clamp");

    // Phase B: bot is stopped. Velocity is now exactly 0. The K_VEL term
    // vanishes immediately; the K_POS term decays as POS_LEAK bleeds the
    // integrator. Run long enough for the ~5 s washout to dominate.
    float nudge_stopped = nudge_moving;
    for (int i = 0; i < 20000; ++i) {    // 100 s — many washout time constants
        nudge_stopped = loop.update(0.0f, kDt);
    }
    CHECK(std::fabs(nudge_stopped) < std::fabs(nudge_moving),
          "once stopped, the nudge must shrink toward zero");
    CHECK_NEAR(nudge_stopped, 0.0f, 1e-3f,
               "after a long stop the loop must settle to ~zero nudge");
    CHECK_NEAR(loop.position_m(), 0.0f, 1e-3f,
               "after a long stop the position integrator must leak to ~zero");
}

// ---------------------------------------------------------------------------
// main — run every scenario, report, exit 0/nonzero
// ---------------------------------------------------------------------------

int main(void) {
    std::printf("=== PositionLoop tests (Phase 4M.13) ===\n");
    std::printf("gains: K_POS=%.3f K_VEL=%.3f MAX_NUDGE_DEG=%.3f "
                "POS_LEAK=%.4f SLEW_DEG_S=%.3f\n\n",
                POSLOOP_K_POS, POSLOOP_K_VEL, POSLOOP_MAX_NUDGE_DEG,
                POSLOOP_POS_LEAK, POSLOOP_SLEW_DEG_S);

    RUN(test_zero_input_zero_nudge);
    RUN(test_sign_convention);
    RUN(test_magnitude_clamp);
    RUN(test_slew_limit);
    RUN(test_leaky_integrator);
    RUN(test_reset_clears_state);
    RUN(test_dt_robustness);
    RUN(test_steady_state);

    std::printf("\nTests run: %d, passed: %d\n", g_tests_run, g_tests_passed);
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
