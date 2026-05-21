/**
 * Unit tests for BalanceSafety — Phase 4.7a tilt + watchdog + abort module.
 *
 * BalanceSafety (src/applications/balancing_robot/safety.{h,cpp}) is a pure
 * data + comparisons module with no Arduino dependencies. The only foreign
 * type it touches is `SafetyLimits`, which is declared in the header-only
 * tuning_strategy.h. So host-compile is straightforward: pull the .cpp under
 * test plus the include path for tuning_strategy.h.
 *
 * Compile + run:
 *   cd /home/devel/floppi/auto_orientation
 *   timeout 90 g++ -std=c++11 -O1 -fpermissive -Iinclude -Isrc \
 *       -o /tmp/test_safety_run \
 *       tests/test_safety.cpp src/applications/balancing_robot/safety.cpp
 *   timeout 30 /tmp/test_safety_run
 *
 * Coverage (one function per scenario, all run from main()):
 *   1. test_defaults_after_construction       - defaults match documented values
 *   2. test_tilt_limit_setter_normalises_sign - negative input stored as magnitude
 *   3. test_is_tipover_threshold              - boundary at |pitch| == limit
 *   4. test_is_recovered_threshold            - boundary at |pitch| == recovery
 *   5. test_tipover_recovery_hysteresis       - asymmetric thresholds avoid flapping
 *   6. test_tipover_handles_extreme_inputs    - very large / very small magnitudes
 *   7. test_watchdog_initial_state            - unbumped watchdog is NOT starved
 *   8. test_watchdog_starves_after_timeout    - elapsed > timeout => starved
 *   9. test_watchdog_feed_unstarves           - feeding resets the starvation timer
 *  10. test_watchdog_timeout_setter           - changing timeout takes effect
 *  11. test_abort_flag_sticky_until_cleared   - request/clear semantics
 *  12. test_populate_tuner_safety             - tuner gets 90% of tilt limit
 *  13. test_populate_tuner_safety_abort_flag  - abort flag propagates
 *
 * Author: ao-test-extender-v2@floppi:1
 * Date:   2026-05-20
 */

#include <cmath>
#include <cstdio>
#include <cstdint>

#include "applications/balancing_robot/safety.h"
#include "control/tuning_strategy.h"  // for SafetyLimits in the populate test

// ---------------------------------------------------------------------------
// Tiny assertion harness (matches tests/test_position_loop.cpp style)
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

static const float kEps = 1e-4f;

// Documented defaults from safety.cpp:
//   kDefaultTiltLimitDeg     = 35.0f
//   kDefaultTiltRecoveryDeg  = 15.0f
//   kDefaultWatchdogMs       = 200u
static const float    kDocTiltLimitDeg    = 35.0f;
static const float    kDocTiltRecoveryDeg = 15.0f;
static const uint32_t kDocWatchdogMs      = 200u;

// ---------------------------------------------------------------------------
// 1. Defaults
// ---------------------------------------------------------------------------

void test_defaults_after_construction(void) {
    BalanceSafety s;
    CHECK_NEAR(s.tilt_limit_deg(),    kDocTiltLimitDeg,    kEps,
               "default tilt_limit_deg must equal documented 35 deg");
    CHECK_NEAR(s.tilt_recovery_deg(), kDocTiltRecoveryDeg, kEps,
               "default tilt_recovery_deg must equal documented 15 deg");
    CHECK(s.watchdog_timeout_ms() == kDocWatchdogMs,
          "default watchdog timeout must equal documented 200 ms");
    CHECK(!s.abort_requested(),
          "fresh BalanceSafety must not have an abort pending");
    CHECK(s.last_feed_ms() == 0u,
          "fresh BalanceSafety reports zero last_feed_ms");
}

// ---------------------------------------------------------------------------
// 2. Setters normalise sign (store as positive magnitude)
// ---------------------------------------------------------------------------

void test_tilt_limit_setter_normalises_sign(void) {
    BalanceSafety s;
    s.set_tilt_limit(-40.0f);
    CHECK_NEAR(s.tilt_limit_deg(), 40.0f, kEps,
               "negative tilt limit must be stored as positive magnitude");
    s.set_tilt_recovery(-12.5f);
    CHECK_NEAR(s.tilt_recovery_deg(), 12.5f, kEps,
               "negative tilt recovery must be stored as positive magnitude");

    // Positive inputs untouched.
    s.set_tilt_limit(33.0f);
    CHECK_NEAR(s.tilt_limit_deg(), 33.0f, kEps,
               "positive tilt limit must round-trip");
    s.set_tilt_recovery(10.0f);
    CHECK_NEAR(s.tilt_recovery_deg(), 10.0f, kEps,
               "positive tilt recovery must round-trip");

    // Zero is a legal (degenerate) value.
    s.set_tilt_limit(0.0f);
    CHECK_NEAR(s.tilt_limit_deg(), 0.0f, kEps, "zero tilt limit allowed");
}

// ---------------------------------------------------------------------------
// 3. is_tipover boundary semantics
// ---------------------------------------------------------------------------

void test_is_tipover_threshold(void) {
    BalanceSafety s;
    s.set_tilt_limit(30.0f);

    // At the limit exactly the predicate returns false (>, not >=).
    CHECK(!s.is_tipover(30.0f),  "|pitch| == limit must NOT be tipover (strict >)");
    CHECK(!s.is_tipover(-30.0f), "negative pitch at -limit must NOT be tipover");

    // Just inside: not tipover.
    CHECK(!s.is_tipover(29.99f),  "|pitch| just below limit is not tipover");
    CHECK(!s.is_tipover(-29.99f), "negative pitch just inside is not tipover");

    // Just past: tipover, both signs.
    CHECK(s.is_tipover(30.01f),  "|pitch| just past limit IS tipover");
    CHECK(s.is_tipover(-30.01f), "negative pitch just past -limit IS tipover");
    CHECK(s.is_tipover(90.0f),   "extreme positive pitch is tipover");
    CHECK(s.is_tipover(-90.0f),  "extreme negative pitch is tipover");

    // Zero pitch never tipover.
    CHECK(!s.is_tipover(0.0f), "zero pitch is never tipover");
}

// ---------------------------------------------------------------------------
// 4. is_recovered boundary semantics
// ---------------------------------------------------------------------------

void test_is_recovered_threshold(void) {
    BalanceSafety s;
    s.set_tilt_recovery(10.0f);

    // At the recovery threshold exactly the predicate returns false (<, not <=).
    CHECK(!s.is_recovered(10.0f),  "|pitch| == recovery must NOT be recovered (strict <)");
    CHECK(!s.is_recovered(-10.0f), "negative pitch at -recovery must NOT be recovered");

    // Inside: recovered.
    CHECK(s.is_recovered(0.0f),    "zero pitch IS recovered");
    CHECK(s.is_recovered(9.99f),   "|pitch| just below recovery IS recovered");
    CHECK(s.is_recovered(-9.99f),  "negative pitch just inside IS recovered");

    // Outside: not recovered.
    CHECK(!s.is_recovered(10.01f),  "|pitch| just past recovery is not recovered");
    CHECK(!s.is_recovered(-10.01f), "negative pitch just past -recovery is not recovered");
    CHECK(!s.is_recovered(45.0f),   "large pitch is not recovered");
}

// ---------------------------------------------------------------------------
// 5. Asymmetric hysteresis: limit > recovery prevents flapping
// ---------------------------------------------------------------------------

void test_tipover_recovery_hysteresis(void) {
    // Typical config from balancing-robot: limit=35°, recovery=15°.
    // A pitch sweep from 0 to 40 to 0 must:
    //   - become tipover somewhere above 35°
    //   - stay NOT-recovered between 15° and 35° on the way back down (gap)
    //   - become recovered below 15°
    BalanceSafety s;
    s.set_tilt_limit(35.0f);
    s.set_tilt_recovery(15.0f);

    // While ascending past the limit:
    CHECK(!s.is_tipover(34.5f),  "below limit on ascent: not tipover");
    CHECK(s.is_tipover(35.5f),   "above limit on ascent: tipover");

    // In the hysteresis gap [recovery, limit] the bot is neither tipover nor
    // recovered — caller is expected to keep the FALLEN flag latched.
    CHECK(!s.is_tipover(20.0f),   "within gap: not tipover");
    CHECK(!s.is_recovered(20.0f), "within gap: not recovered (FALLEN stays latched)");

    // Below recovery threshold the bot has recovered.
    CHECK(s.is_recovered(5.0f),  "well below recovery: recovered");
    CHECK(!s.is_tipover(5.0f),   "well below recovery: not tipover");

    // Sanity: the documented invariant (recovery < limit) is upheld here.
    CHECK(s.tilt_recovery_deg() < s.tilt_limit_deg(),
          "recovery threshold must be strictly less than tilt limit");
}

// ---------------------------------------------------------------------------
// 6. Extreme inputs (large, tiny, very large) — predicate stays sane
// ---------------------------------------------------------------------------

void test_tipover_handles_extreme_inputs(void) {
    BalanceSafety s;
    s.set_tilt_limit(35.0f);
    s.set_tilt_recovery(15.0f);

    // Enormous pitch — module just compares magnitudes with >, no overflow.
    CHECK(s.is_tipover(1.0e6f),  "very large positive pitch is tipover");
    CHECK(s.is_tipover(-1.0e6f), "very large negative pitch is tipover");
    CHECK(!s.is_recovered(1.0e6f),
          "very large pitch is not recovered");

    // Tiny near-zero pitch — recovered, not tipover.
    CHECK(!s.is_tipover(1.0e-6f),  "subdegree pitch is not tipover");
    CHECK(s.is_recovered(1.0e-6f), "subdegree pitch is recovered");
    CHECK(!s.is_tipover(-1.0e-6f), "subdegree negative pitch is not tipover");

    // Pure zero handled as both "not tipover" and "recovered" — caller relies
    // on this so a perfectly upright sample doesn't mark the bot fallen.
    CHECK(!s.is_tipover(0.0f),   "zero pitch is not tipover");
    CHECK(s.is_recovered(0.0f),  "zero pitch is recovered");
}

// ---------------------------------------------------------------------------
// 7. Watchdog initial state: unbumped => NOT starved (lets first sample pass)
// ---------------------------------------------------------------------------

void test_watchdog_initial_state(void) {
    BalanceSafety s;
    // Per safety.cpp:53-57 — until feed_watchdog() is called once, the watchdog
    // is NOT armed, so starved() returns false regardless of how much time has
    // elapsed. This is important so the very first main-loop iteration doesn't
    // trip the safety system before it has had a chance to feed.
    CHECK(!s.watchdog_starved(0u),
          "unarmed watchdog must not be starved at t=0");
    CHECK(!s.watchdog_starved(10u * 1000u * 1000u),
          "unarmed watchdog must not be starved even after a long elapsed time");
}

// ---------------------------------------------------------------------------
// 8. Watchdog starves once elapsed > timeout
// ---------------------------------------------------------------------------

void test_watchdog_starves_after_timeout(void) {
    BalanceSafety s;
    s.set_watchdog_timeout(100u);  // 100 ms

    // Bump at t=1000 ms.
    s.feed_watchdog(1000u);
    CHECK(s.last_feed_ms() == 1000u, "feed must store the timestamp it received");

    // Equal to timeout: NOT starved (strict > in safety.cpp).
    CHECK(!s.watchdog_starved(1100u), "elapsed == timeout must NOT be starved");

    // Just past timeout: starved.
    CHECK(s.watchdog_starved(1101u),  "elapsed > timeout must be starved");

    // Way past timeout: still starved.
    CHECK(s.watchdog_starved(10000u), "long after timeout: still starved");

    // Just before timeout: not starved.
    CHECK(!s.watchdog_starved(1099u), "before timeout elapsed: not starved");
}

// ---------------------------------------------------------------------------
// 9. Feeding the watchdog while starved un-starves it
// ---------------------------------------------------------------------------

void test_watchdog_feed_unstarves(void) {
    BalanceSafety s;
    s.set_watchdog_timeout(50u);

    s.feed_watchdog(0u);
    CHECK(!s.watchdog_starved(40u),  "fresh feed: not starved");
    CHECK(s.watchdog_starved(60u),   "stale feed: starved");

    // Feed again and verify recovery.
    s.feed_watchdog(60u);
    CHECK(!s.watchdog_starved(60u),  "right after re-feed: not starved");
    CHECK(!s.watchdog_starved(110u), "50 ms later: still not starved");
    CHECK(s.watchdog_starved(111u),  "51 ms later: starved again");
}

// ---------------------------------------------------------------------------
// 10. Watchdog timeout setter takes effect
// ---------------------------------------------------------------------------

void test_watchdog_timeout_setter(void) {
    BalanceSafety s;
    s.feed_watchdog(0u);

    s.set_watchdog_timeout(10u);
    CHECK(s.watchdog_timeout_ms() == 10u, "set_watchdog_timeout reflected in getter");
    CHECK(s.watchdog_starved(11u),  "after shrinking timeout, already-stale feed is starved");

    s.set_watchdog_timeout(10000u);
    CHECK(s.watchdog_timeout_ms() == 10000u, "growing timeout reflected in getter");
    CHECK(!s.watchdog_starved(11u),
          "after growing timeout, previously-starved feed is no longer starved");
}

// ---------------------------------------------------------------------------
// 11. Abort flag is sticky until clear_abort()
// ---------------------------------------------------------------------------

void test_abort_flag_sticky_until_cleared(void) {
    BalanceSafety s;
    CHECK(!s.abort_requested(), "fresh safety: no abort");

    s.request_abort();
    CHECK(s.abort_requested(), "after request_abort: flag set");

    // Multiple reads do NOT clear it — caller must explicitly clear.
    CHECK(s.abort_requested(), "abort flag remains set across reads");
    CHECK(s.abort_requested(), "abort flag still set after second read");

    s.clear_abort();
    CHECK(!s.abort_requested(), "after clear_abort: flag cleared");

    // Idempotent: clearing again is harmless.
    s.clear_abort();
    CHECK(!s.abort_requested(), "double-clear is harmless");

    // Re-request works after clear.
    s.request_abort();
    CHECK(s.abort_requested(), "re-request after clear sets flag again");
}

// ---------------------------------------------------------------------------
// 12. populate_tuner_safety: max_angle is 90% of tilt limit
// ---------------------------------------------------------------------------

void test_populate_tuner_safety(void) {
    BalanceSafety s;
    s.set_tilt_limit(40.0f);

    SafetyLimits out;
    out.max_angle_deg    = -1.0f;  // poison
    out.max_duration_sec = -1.0f;
    out.abort_requested  = true;   // pre-set to verify overwrite path

    s.populate_tuner_safety(out);

    CHECK_NEAR(out.max_angle_deg, 40.0f * 0.9f, kEps,
               "max_angle_deg must be 0.9 * tilt_limit_deg");
    CHECK_NEAR(out.max_duration_sec, 30.0f, kEps,
               "default max_duration_sec must be 30s (caller overrides)");
    // BalanceApp expects the tuner to stop BEFORE the app's own tipover guard
    // would fire — so the margin must be strictly less than the tilt limit.
    CHECK(out.max_angle_deg < s.tilt_limit_deg(),
          "tuner's max_angle must be STRICTLY less than app's tilt_limit "
          "(safety margin invariant from safety.cpp:80-82)");
}

// ---------------------------------------------------------------------------
// 13. populate_tuner_safety propagates the abort flag
// ---------------------------------------------------------------------------

void test_populate_tuner_safety_abort_flag(void) {
    BalanceSafety s;
    s.set_tilt_limit(30.0f);

    SafetyLimits out;
    out.abort_requested = false;
    s.populate_tuner_safety(out);
    CHECK(!out.abort_requested,
          "no abort pending => populate copies false into SafetyLimits");

    s.request_abort();
    SafetyLimits out2;
    out2.abort_requested = false;
    s.populate_tuner_safety(out2);
    CHECK(out2.abort_requested,
          "abort pending => populate copies true into SafetyLimits");

    s.clear_abort();
    SafetyLimits out3;
    out3.abort_requested = true;   // pre-poisoned to confirm it gets overwritten
    s.populate_tuner_safety(out3);
    CHECK(!out3.abort_requested,
          "cleared abort => populate overwrites stale 'true' with 'false'");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void) {
    std::printf("=== BalanceSafety tests (Phase 4.7a) ===\n");
    std::printf("Defaults: tilt_limit=%.1f tilt_recovery=%.1f watchdog=%ums\n\n",
                kDocTiltLimitDeg, kDocTiltRecoveryDeg,
                (unsigned)kDocWatchdogMs);

    RUN(test_defaults_after_construction);
    RUN(test_tilt_limit_setter_normalises_sign);
    RUN(test_is_tipover_threshold);
    RUN(test_is_recovered_threshold);
    RUN(test_tipover_recovery_hysteresis);
    RUN(test_tipover_handles_extreme_inputs);
    RUN(test_watchdog_initial_state);
    RUN(test_watchdog_starves_after_timeout);
    RUN(test_watchdog_feed_unstarves);
    RUN(test_watchdog_timeout_setter);
    RUN(test_abort_flag_sticky_until_cleared);
    RUN(test_populate_tuner_safety);
    RUN(test_populate_tuner_safety_abort_flag);

    std::printf("\nTests run: %d, passed: %d, failed: %d\n",
                g_tests_run, g_tests_passed, g_tests_run - g_tests_passed);
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
