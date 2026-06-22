/**
 * CascadeSelfAudit — runtime K_VEL self-confirmation for the position cascade.
 *
 * Workstream M-4 (roadmap item; closes deferred bench observation F-3 from
 * docs/findings/workstream_g_bench_protocol_2026-05-21.md §6).
 *
 * THE PROBLEM
 * -----------
 * Phase 4M.14 derives the outer-loop velocity gain K_VEL analytically via
 * pole-placement on the linearised cascade plant — the nominal target is a
 * CRITICALLY DAMPED (zeta = 1.0) closed-loop position step response. The
 * derivation lands at K_VEL ~= 11.68 for the production geometry. Whether the
 * plant model is right enough that this K_VEL actually delivers critical
 * damping on the real bot, though, is a bench-observation question — F-3 in
 * the workstream-G protocol asks an operator to eyeball a CSV plot of
 * position_m over the first 30 s of RUN and judge "looks damped?".
 *
 * THE SOLUTION
 * ------------
 * Replace operator judgement with on-bot measurement. CascadeSelfAudit
 * watches position_m, detects natural "step events" (small perturbations that
 * shift position by more than POSLOOP_AUDIT_STEP_THRESHOLD_M within one tick),
 * follows the response over POSLOOP_AUDIT_WINDOW_TICKS, and counts the number
 * of zero-crossings of (position - settled_value) within that window.
 *
 *   - 0 zero crossings   → response never returned to (or crossed) the new
 *                          settled value → OVERDAMPED  (K_VEL too high)
 *   - exactly 1 crossing → response settled cleanly through the level once →
 *                          ~CRITICALLY DAMPED          (K_VEL OK)
 *   - 2+ crossings       → response oscillated         (UNDERDAMPED, K_VEL low)
 *
 * After POSLOOP_AUDIT_MIN_EVENTS events have been classified, a majority vote
 * produces the verdict; before that the verdict is UNKNOWN.
 *
 * The math is a damping proxy, not a true zeta fit: a single-step zero-
 * crossing count is the simplest classifier that distinguishes the three
 * regimes without a curve-fitter, and that fits in the ATmega2560's RAM.
 *
 * MEMORY DISCIPLINE
 * -----------------
 * Total per-instance state: ~36 bytes of metadata + the window-tail circular
 * buffer (POSLOOP_AUDIT_WINDOW_TICKS floats = 800 B at the default 200-tick
 * window). All math is float, no dynamic allocation, no STL — same constraints
 * as the rest of the control/ layer.
 *
 * Actually we DO NOT need to buffer the whole window: zero-crossings can be
 * counted *incrementally* by remembering only the previous sample's signed
 * deviation from the candidate settled value. That collapses the state to a
 * few floats per active event; we only buffer the LAST POSLOOP_AUDIT_LOOKBACK
 * samples (default 8) to detect the step itself (sudden delta from a smoothed
 * recent mean). See the .cpp for the running-mean update.
 *
 * HONEST CAVEAT
 * -------------
 * The thresholds (STEP_THRESHOLD_M, WINDOW_TICKS, MIN_EVENTS) and the 1-vs-2
 * crossing-count classifier boundary are bench-calibration questions. The
 * defaults are conservative starting points chosen so a stationary bot with
 * normal encoder noise (~1 mm/s standard deviation) does not produce spurious
 * step events, but real-bench validation may want them tightened or relaxed.
 * The verdict is INFORMATIONAL only — nothing in the control law branches on
 * it. Bad thresholds mean "wrong K_VEL_* on the telemetry line", not unsafe
 * motion.
 *
 * Author: ao-M4-kvel-self-audit@floppi:1
 * Date:   2026-06-21
 */

#ifndef CASCADE_SELF_AUDIT_H
#define CASCADE_SELF_AUDIT_H

#include <stdint.h>

// ---------------------------------------------------------------------------
// Tuning constants — informational thresholds, NOT control gains (no scope.md
// "derive every gain" obligation applies). Defaults picked to be conservative
// on the production geometry; bench-tunable.
// ---------------------------------------------------------------------------

// Minimum |delta-position| within one tick to register a "step event".
// Chosen so encoder noise (~1 mm/s RMS at 200 Hz = ~5 um/tick) cannot trip it
// but a real ~5 mm shove will. About 1000x the noise floor.
constexpr float POSLOOP_AUDIT_STEP_THRESHOLD_M = 0.005f;

// How many ticks after a step event the response is observed.
// 200 ticks at 200 Hz = 1.0 s — long enough to bracket the closed-loop
// settling time at the derived gains (omega_o ~= a few rad/s).
constexpr uint16_t POSLOOP_AUDIT_WINDOW_TICKS = 200;

// Lookback for step detection — running mean of the last LOOKBACK samples is
// the "settled-before" reference. Small so spurious mean drift cannot mask a
// real step.
constexpr uint8_t POSLOOP_AUDIT_LOOKBACK_TICKS = 8;

// How many classified events to aggregate before emitting a confident verdict.
// 5 is a balance between "quick verdict" and "robust to one weird event".
constexpr uint8_t POSLOOP_AUDIT_MIN_EVENTS = 5;

// Quiet period after one event completes before a new event can begin
// (ticks). Prevents counting the tail of one transient as the head of the
// next. One window-width is the safe default.
constexpr uint16_t POSLOOP_AUDIT_QUIET_TICKS = POSLOOP_AUDIT_WINDOW_TICKS;

/**
 * Damping-class verdict reported on the telemetry line. The UNKNOWN sentinel
 * is the boot/post-reset default — emitted until POSLOOP_AUDIT_MIN_EVENTS
 * step events have been classified.
 *
 * Naming note: Arduino.h #defines HIGH and LOW as pin-level constants
 * (0x1 / 0x0) so we cannot use those bare identifiers as enumerators. The
 * KVEL_ prefix avoids that collision while keeping the on-wire label
 * (K_VEL_HIGH / K_VEL_LOW / K_VEL_OK / K_VEL_UNKNOWN) intact when the
 * telemetry emitter formats them.
 */
enum class CascadeAuditVerdict : uint8_t {
    UNKNOWN   = 0,   // insufficient data (default at startup / after reset)
    KVEL_OK   = 1,   // majority of events ~ critically damped (1 zero crossing)
    KVEL_HIGH = 2,   // majority of events overdamped (0 crossings) — K_VEL too high
    KVEL_LOW  = 3    // majority of events underdamped (>=2 crossings) — K_VEL too low
};

/**
 * Stateful runtime damping classifier. One instance per outer loop.
 *
 * Push samples in from the same place that drives position_loop.update(); the
 * audit infers step events from the position trajectory itself — it does not
 * need to know about pitch setpoints or commanded nudges.
 *
 * Lifecycle: construct once; reset() on RUN entry (matches the PositionLoop
 * reset() contract — see position_loop.cpp); push_sample(position_m, millis)
 * on every PID tick; verdict() readable at any time.
 *
 * Thread-safety: NOT thread-safe. The expected call site is the same as
 * position_loop.update() — either the ISR-bound step_run_() (Mega target) or
 * the main loop (native tests). Don't call verdict() from an ISR if a
 * push_sample() can preempt it (none of our targets do this today).
 */
class CascadeSelfAudit {
public:
    CascadeSelfAudit();

    /**
     * Clear all classifier state. Call on RUN entry so each fresh balance
     * session starts from UNKNOWN.
     */
    void reset();

    /**
     * Feed one position sample to the classifier.
     *
     * @param position_m  Current PositionLoop::position_m() value, in metres.
     *                    NaN/Inf are silently ignored (the previous valid
     *                    state is preserved — same neutral pattern as
     *                    PositionLoop::update()).
     * @param now_ms      Wall-clock millis() at the sample. Currently unused
     *                    inside the classifier (the window is counted in
     *                    ticks, not time), but kept in the signature so a
     *                    later upgrade to a time-based window does not need
     *                    a caller change.
     */
    void push_sample(float position_m, uint32_t now_ms);

    /**
     * Current damping-class verdict. Cheap (one byte load), const, side-
     * effect free — safe to call from telemetry emit sites.
     */
    CascadeAuditVerdict verdict() const { return verdict_; }

    // ----- Inspection (tests / future telemetry expansion) ----------------
    uint8_t events_classified() const { return events_classified_; }
    uint8_t ok_count()          const { return ok_count_; }
    uint8_t high_count()        const { return high_count_; }
    uint8_t low_count()         const { return low_count_; }

private:
    // Recompute verdict_ from the running OK/HIGH/LOW tallies. Called at the
    // end of each event-completion path.
    void recompute_verdict_();

    // --- Step-detection lookback (small running mean) ----------------------
    // We need a "what was position before the step" reference. We keep the
    // last LOOKBACK_TICKS samples in a tiny circular buffer and use the mean
    // as the pre-step level. Heap-free; ATmega2560 friendly.
    float    lookback_buf_[POSLOOP_AUDIT_LOOKBACK_TICKS];
    uint8_t  lookback_idx_;     // next write slot (0..LOOKBACK_TICKS-1)
    uint8_t  lookback_fill_;    // 0..LOOKBACK_TICKS — samples actually held
    float    prev_position_m_;  // last seen value, for tick-to-tick delta
    bool     have_prev_;        // false until the first push_sample

    // --- Active-event state ------------------------------------------------
    // Set when a step is detected, cleared when the window expires.
    bool     event_active_;
    uint16_t event_ticks_left_;  // ticks remaining in the observation window
    float    event_settled_value_;  // "after the step" level we count crossings
                                    // against (computed as mean of the LAST
                                    // few samples of the window — set at event
                                    // completion, NOT entry; see .cpp)
    // Running min/max of position within the active window — used to set
    // event_settled_value_ as midpoint(min,max) at window end. Midpoint is a
    // crude but robust proxy for "the response level the bot is converging to"
    // without curve-fitting.
    float    event_window_min_;
    float    event_window_max_;
    // Crossing-count machinery: we track the sign of (sample - reference)
    // where reference is the running midpoint, and increment on every flip.
    // Using midpoint as we go (rather than waiting until the end) makes the
    // count truly online; the count is finalised at window end against the
    // final midpoint by a recount over a small recent buffer.
    // For RAM economy we keep ONLY the count and the last-seen sign;
    // mis-classification edge cases are accepted (see .cpp design note).
    int8_t   last_sign_;          // -1, 0, +1 — sign of (sample - midpoint)
    uint8_t  crossing_count_;     // 0..255 (saturates; we only care about 0/1/2+)

    // Cool-down after an event ends. While > 0 we keep ingesting samples
    // into the lookback buffer but do not start a new event.
    uint16_t quiet_ticks_left_;

    // --- Aggregated tallies + final verdict --------------------------------
    uint8_t  events_classified_;
    uint8_t  ok_count_;
    uint8_t  high_count_;
    uint8_t  low_count_;
    CascadeAuditVerdict verdict_;
};

#endif  // CASCADE_SELF_AUDIT_H
