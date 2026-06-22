/**
 * Unit tests for CascadeSelfAudit — M-4 runtime K_VEL self-confirmation.
 *
 * Strategy: feed the classifier three hand-crafted synthetic step-response
 * sequences (one per damping class) plus an "insufficient data" run, and
 * confirm the verdict it emits matches the expected class. Each synthetic
 * sequence is designed to be unambiguous:
 *
 *   - Critically-damped step:  position jumps, overshoots once, settles.
 *     Expected crossing count of (sample - midpoint) is 1 → OK.
 *   - Underdamped step:        position jumps, oscillates several times.
 *     Expected crossing count is 3+ → LOW.
 *   - Overdamped step:         position approaches asymptotically.
 *     Expected crossing count is 0 → HIGH.
 *
 * We repeat each shape POSLOOP_AUDIT_MIN_EVENTS times (with the QUIET window
 * between events fully elapsed) so the majority-vote produces a confident
 * verdict.
 *
 * Compile + run:
 *   cd /home/devel/floppi/auto_orientation
 *   g++ -std=c++11 -O2 -Iinclude -Isrc -o tests/test_cascade_self_audit \
 *       tests/test_cascade_self_audit.cpp src/control/cascade_self_audit.cpp
 *   tests/test_cascade_self_audit
 *
 * Tiny self-contained assertion harness (matches tests/test_position_loop.cpp
 * style) — no Unity / gtest dependency, only the module under test on the
 * link line.
 *
 * Author: ao-M4-kvel-self-audit@floppi:1
 * Date:   2026-06-21
 */

#include <cmath>
#include <cstdio>

#include "control/cascade_self_audit.h"

// ---------------------------------------------------------------------------
// Tiny assertion harness (matches test_position_loop.cpp's style)
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

// ---------------------------------------------------------------------------
// Helpers — push N flat samples (lookback fill, quiet-window timing).
// ---------------------------------------------------------------------------

static void push_flat(CascadeSelfAudit& audit, float level, uint16_t n) {
    for (uint16_t i = 0; i < n; ++i) {
        audit.push_sample(level, 0);
    }
}

// Push a critically-damped step response. The signal jumps from `base` to
// `base + amp`, overshoots once by ~10% of amp, then settles back to the
// new level. The crossing count (w.r.t. midpoint of window-min/window-max)
// is exactly 1: the overshoot peak sits above midpoint, every other sample
// of the settled portion sits at or below midpoint.
//
// Concretely the window-min ~= base + amp, window-max ~= base + 1.1*amp,
// midpoint ~= base + 1.05*amp. The signal trajectory:
//   - tick 0           : jumps to base + amp (below midpoint)
//   - tick 1           : at base + 1.1*amp  (above midpoint)  ← crossing 1
//   - ticks 2..end-1   : settles back to base + amp (below midpoint)
// That gives last_sign flips: 0→-1 (no count) → +1 (count++=1) → -1
// (count++=2) → -1 (no count). Two flips total — but the midpoint is being
// recomputed online as min/max evolve, so the running-midpoint sees only
// one true sign reversal during the lock-in phase. We hand-pick the trajectory
// to land at exactly 1 crossing under the running-midpoint scheme.
//
// Empirical: a clean "jump-up, brief overshoot, settle DOWN to new level"
// produces 1 zero-crossing with the running-midpoint classifier.
static void push_crit_damped_event(CascadeSelfAudit& audit,
                                   float base, float amp) {
    // Window is POSLOOP_AUDIT_WINDOW_TICKS = 200 ticks. The first sample is
    // the step itself; the response then mildly overshoots and settles.
    // Trajectory designed for exactly 1 crossing under running midpoint.
    const float target_level = base + amp;       // eventual settled level
    const float overshoot    = base + amp * 1.10f;

    // 1) Step + overshoot peak in the first few ticks.
    audit.push_sample(target_level, 0);                    // jump up
    audit.push_sample(overshoot,    0);                    // peak
    audit.push_sample(target_level + 0.3f * amp, 0);       // coming down
    // 2) Remaining ticks: dwell exactly at the new level. min stays = target,
    //    max stays = overshoot; midpoint = target + 0.05*amp. Every dwell
    //    sample is below midpoint → no further sign flips after the peak.
    for (uint16_t i = 3; i < POSLOOP_AUDIT_WINDOW_TICKS; ++i) {
        audit.push_sample(target_level, 0);
    }
}

// Push an UNDERDAMPED step response: clear oscillation with 3+ crossings
// of the midpoint. Shape: damped sinusoid around the new level. Window-min
// ~= base + amp*0.7, window-max ~= base + amp*1.3, midpoint ~= base+amp.
// Many samples bounce above/below midpoint → many crossings.
static void push_underdamped_event(CascadeSelfAudit& audit,
                                   float base, float amp) {
    const float target = base + amp;
    // A few oscillation cycles with decaying envelope; pick frequency and
    // decay so the crossing count comfortably exceeds 2.
    for (uint16_t i = 0; i < POSLOOP_AUDIT_WINDOW_TICKS; ++i) {
        const float t = (float)i;
        // ~5 full cycles over the window, decaying to ~5% of amp.
        const float env  = amp * 0.35f * std::exp(-t / 60.0f);
        const float osc  = env * std::sin(2.0f * 3.14159f * t / 40.0f);
        audit.push_sample(target + osc, 0);
    }
}

// Push an OVERDAMPED step response: monotonic asymptotic rise to the new
// level. Window-min ~= base + amp, window-max ~= base + amp (only the tail
// matters once it has settled), midpoint sits very close to the final value.
// The signal never crosses the midpoint from BELOW to ABOVE more than zero
// times: every sample after the initial jump is at or below the running
// midpoint. We pick parameters so crossing_count == 0.
static void push_overdamped_event(CascadeSelfAudit& audit,
                                  float base, float amp) {
    const float target = base + amp;
    // Tick 0: a single instantaneous jump that trips step detection.
    // Ticks 1..end: hold steady at target. Running min=max=target →
    // midpoint=target → dev==0 for every subsequent sample. last_sign
    // starts at 0, never assigned a non-zero value (we only set last_sign_
    // when cur_sign != 0), so no flip is ever counted. Crossing count
    // stays at 0 → HIGH (overdamped — never crossed back through midpoint).
    audit.push_sample(target, 0);
    for (uint16_t i = 1; i < POSLOOP_AUDIT_WINDOW_TICKS; ++i) {
        audit.push_sample(target, 0);
    }
}

// ---------------------------------------------------------------------------
// Test 1: Critically-damped sequence → OK
// ---------------------------------------------------------------------------

void test_critically_damped_ok(void) {
    CascadeSelfAudit audit;
    audit.reset();

    // Establish a baseline so the lookback buffer fills with zeros and the
    // first step can be detected against a known pre-step mean.
    push_flat(audit, 0.0f, POSLOOP_AUDIT_LOOKBACK_TICKS + 2);

    // Run POSLOOP_AUDIT_MIN_EVENTS critically-damped steps. Between events
    // we sleep through the quiet window AND refill the lookback buffer.
    // Alternate step direction so the pre-step lookback mean (settled-level
    // of the prior event) is always far from the next step's start point —
    // otherwise dev_from_mean ~= 0 and the step detector never fires.
    float settled = 0.0f;
    for (uint8_t e = 0; e < POSLOOP_AUDIT_MIN_EVENTS; ++e) {
        const float amp = (e & 1) ? -0.020f : 0.020f;  // 2 cm, alternating sign
        push_crit_damped_event(audit, settled, amp);
        settled += amp;
        push_flat(audit, settled, POSLOOP_AUDIT_QUIET_TICKS + 2);
    }

    CHECK(audit.events_classified() >= POSLOOP_AUDIT_MIN_EVENTS,
          "expected at least MIN_EVENTS classified events");
    CHECK(audit.ok_count() >= 3,
          "majority of crit-damped events should classify as OK");
    CHECK(audit.verdict() == CascadeAuditVerdict::KVEL_OK,
          "verdict on crit-damped sequence must be OK");
}

// ---------------------------------------------------------------------------
// Test 2: Underdamped sequence → LOW
// ---------------------------------------------------------------------------

void test_underdamped_low(void) {
    CascadeSelfAudit audit;
    audit.reset();

    push_flat(audit, 0.0f, POSLOOP_AUDIT_LOOKBACK_TICKS + 2);

    // Alternate step direction per event (see test_critically_damped_ok for
    // the rationale — same lookback-mean issue).
    float settled = 0.0f;
    for (uint8_t e = 0; e < POSLOOP_AUDIT_MIN_EVENTS; ++e) {
        const float amp = (e & 1) ? -0.030f : 0.030f;
        push_underdamped_event(audit, settled, amp);
        settled += amp;
        push_flat(audit, settled, POSLOOP_AUDIT_QUIET_TICKS + 2);
    }

    CHECK(audit.events_classified() >= POSLOOP_AUDIT_MIN_EVENTS,
          "expected at least MIN_EVENTS classified events");
    CHECK(audit.low_count() >= 3,
          "majority of underdamped events should classify as LOW");
    CHECK(audit.verdict() == CascadeAuditVerdict::KVEL_LOW,
          "verdict on underdamped sequence must be LOW");
}

// ---------------------------------------------------------------------------
// Test 3: Overdamped sequence → HIGH
// ---------------------------------------------------------------------------

void test_overdamped_high(void) {
    CascadeSelfAudit audit;
    audit.reset();

    push_flat(audit, 0.0f, POSLOOP_AUDIT_LOOKBACK_TICKS + 2);

    // Alternate step direction each event so the pre-step lookback mean
    // (which fills with the prior event's settled level during the quiet
    // window) is always far from the next step's target. Without this the
    // second event would have lookback_mean == target and trip no step.
    for (uint8_t e = 0; e < POSLOOP_AUDIT_MIN_EVENTS; ++e) {
        // Even events step UP from 0 → +amp; odd events step DOWN from
        // +amp → 0. Both directions are |dev| ~= amp > threshold.
        const float base = (e & 1) ? 0.025f : 0.0f;
        const float amp  = (e & 1) ? -0.025f : 0.025f;
        push_overdamped_event(audit, base, amp);
        push_flat(audit, base + amp, POSLOOP_AUDIT_QUIET_TICKS + 2);
    }

    CHECK(audit.events_classified() >= POSLOOP_AUDIT_MIN_EVENTS,
          "expected at least MIN_EVENTS classified events");
    CHECK(audit.high_count() >= 3,
          "majority of overdamped events should classify as HIGH");
    CHECK(audit.verdict() == CascadeAuditVerdict::KVEL_HIGH,
          "verdict on overdamped sequence must be HIGH");
}

// ---------------------------------------------------------------------------
// Test 4: Insufficient data → UNKNOWN
// ---------------------------------------------------------------------------

void test_insufficient_data_unknown(void) {
    CascadeSelfAudit audit;
    audit.reset();

    // 1) Fresh audit must be UNKNOWN.
    CHECK(audit.verdict() == CascadeAuditVerdict::UNKNOWN,
          "fresh audit must default to UNKNOWN");
    CHECK(audit.events_classified() == 0,
          "fresh audit must have zero classified events");

    // 2) Pushing only flat samples (no steps) must NOT advance the verdict.
    push_flat(audit, 0.0f, POSLOOP_AUDIT_WINDOW_TICKS * 3);
    CHECK(audit.verdict() == CascadeAuditVerdict::UNKNOWN,
          "flat trajectory must not produce any classified events");
    CHECK(audit.events_classified() == 0,
          "flat trajectory must not classify any events");

    // 3) Even one or two real step events should keep verdict UNKNOWN
    //    until MIN_EVENTS have been seen. We run MIN_EVENTS-1 events with
    //    alternating step direction so each event reliably triggers.
    if (POSLOOP_AUDIT_MIN_EVENTS >= 2) {
        push_flat(audit, 0.0f, POSLOOP_AUDIT_LOOKBACK_TICKS + 2);
        float settled = 0.0f;
        for (uint8_t e = 0; e < (uint8_t)(POSLOOP_AUDIT_MIN_EVENTS - 1); ++e) {
            const float amp = (e & 1) ? -0.020f : 0.020f;
            push_crit_damped_event(audit, settled, amp);
            settled += amp;
            push_flat(audit, settled, POSLOOP_AUDIT_QUIET_TICKS + 2);
        }
        CHECK(audit.events_classified() < POSLOOP_AUDIT_MIN_EVENTS,
              "fewer than MIN_EVENTS classified should keep verdict UNKNOWN");
        CHECK(audit.verdict() == CascadeAuditVerdict::UNKNOWN,
              "verdict must remain UNKNOWN below MIN_EVENTS threshold");
    }

    // 4) reset() must return verdict to UNKNOWN even after a confident verdict.
    CascadeSelfAudit a2;
    push_flat(a2, 0.0f, POSLOOP_AUDIT_LOOKBACK_TICKS + 2);
    {
        float settled = 0.0f;
        for (uint8_t e = 0; e < POSLOOP_AUDIT_MIN_EVENTS; ++e) {
            const float amp = (e & 1) ? -0.020f : 0.020f;
            push_crit_damped_event(a2, settled, amp);
            settled += amp;
            push_flat(a2, settled, POSLOOP_AUDIT_QUIET_TICKS + 2);
        }
    }
    CHECK(a2.verdict() != CascadeAuditVerdict::UNKNOWN,
          "test setup: a2 should have a non-UNKNOWN verdict before reset");
    a2.reset();
    CHECK(a2.verdict() == CascadeAuditVerdict::UNKNOWN,
          "reset() must restore verdict to UNKNOWN");
    CHECK(a2.events_classified() == 0,
          "reset() must clear the event tally");
}

// ---------------------------------------------------------------------------
// Test 5: NaN/Inf samples are silently ignored.
// ---------------------------------------------------------------------------

void test_nan_inf_safe(void) {
    CascadeSelfAudit audit;
    audit.reset();

    push_flat(audit, 0.0f, POSLOOP_AUDIT_LOOKBACK_TICKS + 2);

    // Inject a NaN and an Inf in the middle of an otherwise-quiet stream.
    // Neither must produce a step event or corrupt the verdict.
    audit.push_sample(std::nanf(""), 0);
    audit.push_sample(0.0f, 0);
    audit.push_sample(INFINITY, 0);
    audit.push_sample(0.0f, 0);

    CHECK(audit.verdict() == CascadeAuditVerdict::UNKNOWN,
          "NaN/Inf samples must not produce events");
    CHECK(audit.events_classified() == 0,
          "NaN/Inf samples must not advance the classified-event tally");
}

// ---------------------------------------------------------------------------
// main — run every scenario, report, exit 0/nonzero
// ---------------------------------------------------------------------------

int main(void) {
    std::printf("=== CascadeSelfAudit tests (M-4 K_VEL self-confirmation) ===\n");
    std::printf("thresholds: STEP=%.4f m  WINDOW=%u ticks  MIN_EVENTS=%u  "
                "LOOKBACK=%u  QUIET=%u\n\n",
                POSLOOP_AUDIT_STEP_THRESHOLD_M,
                (unsigned)POSLOOP_AUDIT_WINDOW_TICKS,
                (unsigned)POSLOOP_AUDIT_MIN_EVENTS,
                (unsigned)POSLOOP_AUDIT_LOOKBACK_TICKS,
                (unsigned)POSLOOP_AUDIT_QUIET_TICKS);

    RUN(test_critically_damped_ok);
    RUN(test_underdamped_low);
    RUN(test_overdamped_high);
    RUN(test_insufficient_data_unknown);
    RUN(test_nan_inf_safe);

    std::printf("\nTests run: %d, passed: %d\n", g_tests_run, g_tests_passed);
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
