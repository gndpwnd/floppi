/**
 * Unit tests for AutoPIDTuner — Phase 4.5b coordinator that owns POLICY
 * (safety, abort, gain save/restore) while delegating the ALGORITHM to an
 * ITuningStrategy implementation.
 *
 * To exercise the coordinator in isolation we drive it with a FakeStrategy
 * that lets each test script:
 *   - when to declare convergence (after N steps)
 *   - the exact TuningResult to return
 *   - whether to misbehave (e.g. ignore safety, keep running past is_done)
 *
 * The real PIDController is linked in so the gain save/restore path is
 * tested against the actual implementation (not a stub).
 *
 * Compile + run:
 *   cd /home/devel/floppi/auto_orientation
 *   timeout 90 g++ -std=c++11 -O1 -fpermissive -Iinclude -Isrc \
 *       -o /tmp/test_auto_pid_tuner_run \
 *       tests/test_auto_pid_tuner.cpp \
 *       src/control/auto_pid_tuner.cpp \
 *       src/control/pid_controller.cpp
 *   timeout 30 /tmp/test_auto_pid_tuner_run
 *
 * Coverage:
 *   1. test_initial_state_before_begin       - is_done() false, result.reason="not_started"
 *   2. test_begin_caches_original_gains      - restore_original recovers them on failure
 *   3. test_step_returns_strategy_output     - happy-path forwarding
 *   4. test_strategy_done_finalises_result   - success path copies strategy result
 *   5. test_apply_to_writes_tuned_gains      - success then apply_to overwrites PID gains
 *   6. test_restore_original_after_failure   - failure then restore brings gains back
 *   7. test_request_abort_trips_failure      - request_abort -> failure_reason="user_aborted"
 *   8. test_safety_abort_flag_trips_failure  - SafetyLimits.abort_requested triggers same
 *   9. test_max_duration_tripwire            - elapsed > max_duration => failure
 *  10. test_done_short_circuits_step         - step() after done returns 0, no strategy call
 *  11. test_succeeded_requires_converged     - done && converged distinction
 *  12. test_set_safety_limits_captures_abort - abort_requested flag in limits is OR'd in
 *
 * Author: ao-test-extender-v2@floppi:1
 * Date:   2026-05-20
 */

#include <cmath>
#include <cstdio>
#include <cstdint>

#include "control/auto_pid_tuner.h"
#include "control/pid_controller.h"
#include "control/tuning_strategy.h"

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

#define CHECK_STR_EQ(actual, expected, msg)                                   \
    do {                                                                      \
        const char* a_ = (actual);                                            \
        const char* e_ = (expected);                                          \
        bool eq_ = (a_ != 0 && e_ != 0 && std::strcmp(a_, e_) == 0)           \
                   || (a_ == 0 && e_ == 0);                                   \
        if (!eq_) {                                                           \
            ++g_checks_failed_this_test;                                      \
            std::printf("    FAIL: %s  expected '%s' got '%s'  (%s:%d)\n",    \
                        (msg),                                                \
                        e_ ? e_ : "<null>",                                   \
                        a_ ? a_ : "<null>",                                   \
                        __FILE__, __LINE__);                                  \
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

#include <cstring>

static const float kEps = 1e-4f;

// ---------------------------------------------------------------------------
// FakeStrategy — programmable ITuningStrategy for isolating the coordinator
//
//   - converge_after_n_steps:  how many step()s before is_done() flips true
//                              (0 = never, simulate a hang for tripwire tests)
//   - result_to_return:        what get_result() returns after convergence
//   - output_each_step:        what step() returns each call
//   - safety_seen / steps_seen:diagnostic counters for the test to inspect
// ---------------------------------------------------------------------------

class FakeStrategy : public ITuningStrategy {
public:
    FakeStrategy()
        : converge_after_n_steps(0),
          output_each_step(7.5f),
          steps_seen(0),
          begin_calls(0),
          done_(false) {
        result_to_return.kp                       = 0.0f;
        result_to_return.ki                       = 0.0f;
        result_to_return.kd                       = 0.0f;
        result_to_return.ultimate_gain            = 0.0f;
        result_to_return.ultimate_period_sec      = 0.0f;
        result_to_return.phase_margin_estimate_deg = 0.0f;
        result_to_return.converged                = false;
        result_to_return.failure_reason           = "fake_default";
        last_safety_seen.max_angle_deg    = 0.0f;
        last_safety_seen.max_duration_sec = 0.0f;
        last_safety_seen.abort_requested  = false;
    }

    int          converge_after_n_steps;
    TuningResult result_to_return;
    float        output_each_step;
    int          steps_seen;
    int          begin_calls;
    SafetyLimits last_safety_seen;

    void begin(float setpoint,
               float output_min,
               float output_max,
               uint32_t now_ms) override {
        (void)setpoint; (void)output_min; (void)output_max; (void)now_ms;
        ++begin_calls;
        done_ = false;
        steps_seen = 0;
    }

    float step(float measurement,
               const SafetyLimits& safety,
               uint32_t now_ms) override {
        (void)measurement; (void)now_ms;
        ++steps_seen;
        last_safety_seen = safety;
        if (converge_after_n_steps > 0 && steps_seen >= converge_after_n_steps) {
            done_ = true;
        }
        return output_each_step;
    }

    bool is_done() const override { return done_; }
    TuningResult get_result() const override { return result_to_return; }
    const char* name() const override { return "fake"; }

private:
    bool done_;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static PIDController make_test_pid(float kp, float ki, float kd) {
    return PIDController(kp, ki, kd, -255.0f, 255.0f);
}

static SafetyLimits permissive_limits(void) {
    SafetyLimits L;
    L.max_angle_deg    = 1.0e6f;
    L.max_duration_sec = 30.0f;
    L.abort_requested  = false;
    return L;
}

// ---------------------------------------------------------------------------
// 1. Initial state before begin()
// ---------------------------------------------------------------------------

void test_initial_state_before_begin(void) {
    FakeStrategy fake;
    AutoPIDTuner tuner(fake);

    CHECK(!tuner.is_done(),
          "fresh tuner is not done");
    CHECK(!tuner.succeeded(),
          "fresh tuner has not succeeded");
    const TuningResult& r = tuner.result();
    CHECK(!r.converged, "fresh tuner result.converged == false");
    CHECK_STR_EQ(r.failure_reason, "not_started",
                 "fresh tuner result.failure_reason == 'not_started'");
}

// ---------------------------------------------------------------------------
// 2. begin() caches the original gains for restore
// ---------------------------------------------------------------------------

void test_begin_caches_original_gains(void) {
    FakeStrategy fake;
    AutoPIDTuner tuner(fake);

    PIDController pid = make_test_pid(11.0f, 22.0f, 33.0f);
    tuner.begin(pid, /*setpoint*/0.0f, /*min*/-255.0f, /*max*/255.0f, /*now*/0u);

    CHECK(fake.begin_calls == 1, "begin forwards exactly once to strategy");

    // Caller may stomp the PID gains while the tune is running (e.g. by
    // accidentally calling set_tunings); restore_original must still bring
    // the ORIGINAL values back.
    pid.set_tunings(0.0f, 0.0f, 0.0f);
    tuner.restore_original(pid);

    float kp = -1.0f, ki = -1.0f, kd = -1.0f;
    pid.get_tunings(kp, ki, kd);
    CHECK_NEAR(kp, 11.0f, kEps, "restore_original recovers original kp");
    CHECK_NEAR(ki, 22.0f, kEps, "restore_original recovers original ki");
    CHECK_NEAR(kd, 33.0f, kEps, "restore_original recovers original kd");

    // After begin() the result is reset to the in-progress placeholder.
    const TuningResult& r = tuner.result();
    CHECK(!r.converged, "after begin(), result is in_progress / not converged");
    CHECK_STR_EQ(r.failure_reason, "in_progress",
                 "after begin(), failure_reason == 'in_progress'");
    CHECK(!tuner.is_done(), "after begin(), tuner is not done");
}

// ---------------------------------------------------------------------------
// 3. step() returns the strategy's output verbatim (happy path)
// ---------------------------------------------------------------------------

void test_step_returns_strategy_output(void) {
    FakeStrategy fake;
    fake.output_each_step      = 42.5f;
    fake.converge_after_n_steps = 0;  // never converges

    AutoPIDTuner tuner(fake);
    tuner.set_safety_limits(permissive_limits());

    PIDController pid = make_test_pid(1.0f, 2.0f, 3.0f);
    tuner.begin(pid, 0.0f, -255.0f, 255.0f, 0u);

    float out = tuner.step(/*measurement*/0.0f, /*now*/10u);
    CHECK_NEAR(out, 42.5f, kEps, "step() returns strategy output (1st call)");
    out = tuner.step(0.0f, 20u);
    CHECK_NEAR(out, 42.5f, kEps, "step() returns strategy output (subsequent)");
    CHECK(fake.steps_seen == 2,
          "strategy.step() was invoked once per coordinator.step()");
    CHECK(!tuner.is_done(),
          "tuner is not done while strategy continues");
}

// ---------------------------------------------------------------------------
// 4. Strategy declaring done finalises the coordinator
// ---------------------------------------------------------------------------

void test_strategy_done_finalises_result(void) {
    FakeStrategy fake;
    fake.converge_after_n_steps      = 1;     // done on first step
    fake.result_to_return.kp         = 99.0f;
    fake.result_to_return.ki         = 88.0f;
    fake.result_to_return.kd         = 77.0f;
    fake.result_to_return.converged  = true;
    fake.result_to_return.failure_reason = 0;
    fake.output_each_step             = 5.0f;

    AutoPIDTuner tuner(fake);
    tuner.set_safety_limits(permissive_limits());

    PIDController pid = make_test_pid(1.0f, 2.0f, 3.0f);
    tuner.begin(pid, 0.0f, -255.0f, 255.0f, 0u);

    float out = tuner.step(0.0f, 5u);
    CHECK_NEAR(out, 5.0f, kEps, "step still returns strategy output on done tick");
    CHECK(tuner.is_done(), "tuner reports done after strategy is_done()");
    CHECK(tuner.succeeded(), "converged result => succeeded()");

    const TuningResult& r = tuner.result();
    CHECK_NEAR(r.kp, 99.0f, kEps, "result.kp copied from strategy");
    CHECK_NEAR(r.ki, 88.0f, kEps, "result.ki copied from strategy");
    CHECK_NEAR(r.kd, 77.0f, kEps, "result.kd copied from strategy");
    CHECK(r.converged, "result.converged copied from strategy");
}

// ---------------------------------------------------------------------------
// 5. apply_to() writes tuned gains into the PID
// ---------------------------------------------------------------------------

void test_apply_to_writes_tuned_gains(void) {
    FakeStrategy fake;
    fake.converge_after_n_steps   = 1;
    fake.result_to_return.kp      = 1.5f;
    fake.result_to_return.ki      = 0.25f;
    fake.result_to_return.kd      = 0.0625f;
    fake.result_to_return.converged = true;
    fake.result_to_return.failure_reason = 0;

    AutoPIDTuner tuner(fake);
    tuner.set_safety_limits(permissive_limits());

    PIDController pid = make_test_pid(7.0f, 7.0f, 7.0f);
    tuner.begin(pid, 0.0f, -255.0f, 255.0f, 0u);
    (void)tuner.step(0.0f, 1u);
    CHECK(tuner.succeeded(), "test setup: tuner converged");

    tuner.apply_to(pid);
    float kp = 0, ki = 0, kd = 0;
    pid.get_tunings(kp, ki, kd);
    CHECK_NEAR(kp, 1.5f,   kEps, "apply_to writes tuned kp");
    CHECK_NEAR(ki, 0.25f,  kEps, "apply_to writes tuned ki");
    CHECK_NEAR(kd, 0.0625f, kEps, "apply_to writes tuned kd");
}

// ---------------------------------------------------------------------------
// 6. After a failed tune, restore_original brings the originals back even if
//    apply_to() was (incorrectly) called
// ---------------------------------------------------------------------------

void test_restore_original_after_failure(void) {
    FakeStrategy fake;
    fake.converge_after_n_steps   = 1;
    fake.result_to_return.kp      = 999.0f;       // garbage gains from a bad tune
    fake.result_to_return.ki      = 999.0f;
    fake.result_to_return.kd      = 999.0f;
    fake.result_to_return.converged = false;       // FAILED
    fake.result_to_return.failure_reason = "fake_failure";

    AutoPIDTuner tuner(fake);
    tuner.set_safety_limits(permissive_limits());

    PIDController pid = make_test_pid(50.0f, 5.0f, 1.0f);
    tuner.begin(pid, 0.0f, -255.0f, 255.0f, 0u);
    (void)tuner.step(0.0f, 1u);
    CHECK(tuner.is_done(),    "tuner finished");
    CHECK(!tuner.succeeded(), "tuner did NOT succeed (converged=false)");

    // Caller mis-uses apply_to even though the run failed.
    tuner.apply_to(pid);
    float kp = 0, ki = 0, kd = 0;
    pid.get_tunings(kp, ki, kd);
    CHECK_NEAR(kp, 999.0f, kEps, "apply_to wrote the (garbage) failed gains");

    // restore_original must bring the originals back, full stop.
    tuner.restore_original(pid);
    pid.get_tunings(kp, ki, kd);
    CHECK_NEAR(kp, 50.0f, kEps, "restore_original recovers kp");
    CHECK_NEAR(ki, 5.0f,  kEps, "restore_original recovers ki");
    CHECK_NEAR(kd, 1.0f,  kEps, "restore_original recovers kd");
}

// ---------------------------------------------------------------------------
// 7. request_abort() trips a "user_aborted" failure on the next step
// ---------------------------------------------------------------------------

void test_request_abort_trips_failure(void) {
    FakeStrategy fake;
    fake.converge_after_n_steps = 0;  // strategy never finishes on its own

    AutoPIDTuner tuner(fake);
    tuner.set_safety_limits(permissive_limits());

    PIDController pid = make_test_pid(1.0f, 2.0f, 3.0f);
    tuner.begin(pid, 0.0f, -255.0f, 255.0f, 0u);

    // A couple of normal steps...
    (void)tuner.step(0.0f, 5u);
    (void)tuner.step(0.0f, 10u);
    CHECK(!tuner.is_done(), "tuner still running before abort");

    tuner.request_abort();
    float out = tuner.step(0.0f, 15u);
    CHECK_NEAR(out, 0.0f, kEps,
               "after abort, step returns 0 (safe output, strategy not called)");
    CHECK(tuner.is_done(),   "after abort, tuner is done");
    CHECK(!tuner.succeeded(), "after abort, tuner did NOT succeed");
    CHECK_STR_EQ(tuner.result().failure_reason, "user_aborted",
                 "failure_reason == 'user_aborted'");

    // The strategy must NOT have been ticked on the aborting step.
    CHECK(fake.steps_seen == 2,
          "strategy.step() was NOT invoked on the aborting tick");
}

// ---------------------------------------------------------------------------
// 8. SafetyLimits.abort_requested at set_safety_limits() time aborts at next step
// ---------------------------------------------------------------------------

void test_safety_abort_flag_trips_failure(void) {
    FakeStrategy fake;
    fake.converge_after_n_steps = 0;

    AutoPIDTuner tuner(fake);

    PIDController pid = make_test_pid(1.0f, 2.0f, 3.0f);
    tuner.begin(pid, 0.0f, -255.0f, 255.0f, 0u);

    // External code (ISR / dashboard) sets the abort flag *via SafetyLimits*.
    // The coordinator must latch it (see auto_pid_tuner.cpp:54-56) so callers
    // can call set_safety_limits() again later without losing the abort.
    SafetyLimits L = permissive_limits();
    L.abort_requested = true;
    tuner.set_safety_limits(L);

    float out = tuner.step(0.0f, 5u);
    CHECK_NEAR(out, 0.0f, kEps,
               "abort via SafetyLimits returns safe (0) output");
    CHECK(tuner.is_done(),    "abort via SafetyLimits finalises tuner");
    CHECK(!tuner.succeeded(), "abort via SafetyLimits => not succeeded");
    CHECK_STR_EQ(tuner.result().failure_reason, "user_aborted",
                 "abort via SafetyLimits sets failure_reason='user_aborted'");
}

// ---------------------------------------------------------------------------
// 9. max_duration_sec tripwire: elapsed > max_duration => failure
// ---------------------------------------------------------------------------

void test_max_duration_tripwire(void) {
    FakeStrategy fake;
    fake.converge_after_n_steps = 0;  // never converges on its own

    AutoPIDTuner tuner(fake);
    SafetyLimits L = permissive_limits();
    L.max_duration_sec = 1.0f;  // 1 second budget
    tuner.set_safety_limits(L);

    PIDController pid = make_test_pid(1.0f, 2.0f, 3.0f);
    tuner.begin(pid, 0.0f, -255.0f, 255.0f, /*start_ms*/0u);

    // 0.5 s elapsed: still within budget.
    float out = tuner.step(0.0f, 500u);
    CHECK_NEAR(out, fake.output_each_step, kEps,
               "within budget: tuner forwards strategy output");
    CHECK(!tuner.is_done(), "within budget: not done");

    // 1.5 s elapsed: over budget, fails.
    out = tuner.step(0.0f, 1500u);
    CHECK_NEAR(out, 0.0f, kEps,
               "over max_duration_sec: tuner returns safe (0) output");
    CHECK(tuner.is_done(),   "over max_duration_sec: tuner is done");
    CHECK(!tuner.succeeded(), "over max_duration_sec: tuner did not succeed");
    CHECK_STR_EQ(tuner.result().failure_reason, "max_duration_exceeded",
                 "failure_reason == 'max_duration_exceeded'");
}

// ---------------------------------------------------------------------------
// 10. After done, additional step() calls return 0 without invoking strategy
// ---------------------------------------------------------------------------

void test_done_short_circuits_step(void) {
    FakeStrategy fake;
    fake.converge_after_n_steps   = 1;
    fake.output_each_step          = 11.0f;
    fake.result_to_return.converged = true;
    fake.result_to_return.failure_reason = 0;

    AutoPIDTuner tuner(fake);
    tuner.set_safety_limits(permissive_limits());

    PIDController pid = make_test_pid(1.0f, 2.0f, 3.0f);
    tuner.begin(pid, 0.0f, -255.0f, 255.0f, 0u);
    (void)tuner.step(0.0f, 1u);  // strategy declares done
    CHECK(tuner.is_done(), "strategy declared done on first step");

    int before = fake.steps_seen;
    float out  = tuner.step(0.0f, 2u);
    CHECK_NEAR(out, 0.0f, kEps,
               "step() after done returns 0 (does NOT invoke strategy)");
    CHECK(fake.steps_seen == before,
          "strategy is NOT ticked once tuner is done");
}

// ---------------------------------------------------------------------------
// 11. succeeded() requires BOTH done && converged
// ---------------------------------------------------------------------------

void test_succeeded_requires_converged(void) {
    {
        FakeStrategy fake;
        fake.converge_after_n_steps   = 1;
        fake.result_to_return.converged = false;
        fake.result_to_return.failure_reason = "fake_no_converge";

        AutoPIDTuner tuner(fake);
        tuner.set_safety_limits(permissive_limits());
        PIDController pid = make_test_pid(0.0f, 0.0f, 0.0f);
        tuner.begin(pid, 0.0f, -255.0f, 255.0f, 0u);
        (void)tuner.step(0.0f, 1u);
        CHECK(tuner.is_done(),     "done after one step");
        CHECK(!tuner.succeeded(),  "done but NOT converged => NOT succeeded");
    }
    {
        FakeStrategy fake;
        fake.converge_after_n_steps   = 1;
        fake.result_to_return.converged = true;
        fake.result_to_return.failure_reason = 0;

        AutoPIDTuner tuner(fake);
        tuner.set_safety_limits(permissive_limits());
        PIDController pid = make_test_pid(0.0f, 0.0f, 0.0f);
        tuner.begin(pid, 0.0f, -255.0f, 255.0f, 0u);
        (void)tuner.step(0.0f, 1u);
        CHECK(tuner.is_done(),     "done after one step");
        CHECK(tuner.succeeded(),   "done && converged => succeeded");
    }
}

// ---------------------------------------------------------------------------
// 12. set_safety_limits captures the abort flag even after the call returns
// (auto_pid_tuner.cpp:54-56 — defends against ISR-set abort being clobbered
// by a subsequent set_safety_limits call that doesn't carry it)
// ---------------------------------------------------------------------------

void test_set_safety_limits_captures_abort(void) {
    FakeStrategy fake;
    fake.converge_after_n_steps = 0;

    AutoPIDTuner tuner(fake);
    PIDController pid = make_test_pid(0.0f, 0.0f, 0.0f);
    tuner.begin(pid, 0.0f, -255.0f, 255.0f, 0u);

    // First push: limits with abort_requested = true.
    SafetyLimits L1 = permissive_limits();
    L1.abort_requested = true;
    tuner.set_safety_limits(L1);

    // Second push: limits WITHOUT abort_requested. The coordinator must
    // retain the latched abort from L1 (otherwise an ISR press that flips the
    // flag could be lost by a subsequent dashboard set_safety_limits update).
    SafetyLimits L2 = permissive_limits();
    L2.abort_requested = false;
    tuner.set_safety_limits(L2);

    float out = tuner.step(0.0f, 5u);
    CHECK_NEAR(out, 0.0f, kEps,
               "abort latched across set_safety_limits: step returns 0");
    CHECK(tuner.is_done(), "abort latched: tuner is done");
    CHECK_STR_EQ(tuner.result().failure_reason, "user_aborted",
                 "abort latched: failure_reason == 'user_aborted'");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void) {
    std::printf("=== AutoPIDTuner tests (Phase 4.5b coordinator) ===\n\n");

    RUN(test_initial_state_before_begin);
    RUN(test_begin_caches_original_gains);
    RUN(test_step_returns_strategy_output);
    RUN(test_strategy_done_finalises_result);
    RUN(test_apply_to_writes_tuned_gains);
    RUN(test_restore_original_after_failure);
    RUN(test_request_abort_trips_failure);
    RUN(test_safety_abort_flag_trips_failure);
    RUN(test_max_duration_tripwire);
    RUN(test_done_short_circuits_step);
    RUN(test_succeeded_requires_converged);
    RUN(test_set_safety_limits_captures_abort);

    std::printf("\nTests run: %d, passed: %d, failed: %d\n",
                g_tests_run, g_tests_passed, g_tests_run - g_tests_passed);
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
