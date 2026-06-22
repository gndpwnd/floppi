/**
 * CascadeSelfAudit — implementation. See cascade_self_audit.h for the design,
 * the threshold rationale, and the honest-caveat about bench tuning.
 *
 * Implementation notes that did not fit cleanly in the header:
 *
 * 1. STEP DETECTION
 *    A "step event" is declared when the new sample's deviation from the
 *    smoothed mean of the previous LOOKBACK_TICKS samples exceeds
 *    STEP_THRESHOLD_M. We require the lookback buffer to be full first so the
 *    mean is well-conditioned (a one-sample mean is just the sample itself
 *    and would never trip the threshold).
 *
 * 2. ONLINE CROSSING COUNT
 *    The textbook way to classify step-response damping is to fit zeta from
 *    overshoot/undershoot. On the ATmega2560 a curve fit is not happening. We
 *    use a much simpler proxy: count how many times the response signal
 *    crosses its eventual settled level within an observation window. For a
 *    second-order step response:
 *      - zeta > 1 (overdamped): 0 crossings  - the signal approaches the new
 *        level asymptotically without reaching it within the window.
 *      - zeta ~= 1 (critically damped): 1 crossing - the signal reaches and
 *        slightly overshoots the new level once, then settles.
 *      - zeta < 1 (underdamped): >=2 crossings - the signal oscillates around
 *        the new level.
 *    (Technically a perfectly critically damped step has *zero* overshoot, so
 *    a perfect zeta=1 sees 0 crossings too. We treat 0 crossings as HIGH not
 *    OK because the bench question is "is K_VEL too high?" — and any real
 *    plant has enough non-modelled effects that critical damping in practice
 *    means a small bounded overshoot, which crosses exactly once. A real
 *    bot showing 0 crossings is essentially saying "K_VEL is high enough
 *    that the system is over-damped relative to the analytic target".)
 *
 * 3. THE "SETTLED VALUE" CHOICE
 *    The settled value matters for the crossing count. We use the midpoint
 *    of (window_min, window_max) as a proxy — robust against the final
 *    sample being non-representative (a tail oscillation peak, say) but
 *    cheap. A more sophisticated implementation would use the trailing-mean
 *    of the last N samples; we accept the simpler choice given the verdict
 *    is informational. The first crossing-count pass uses an EVOLVING
 *    midpoint (recomputed every sample), so the count is a coarse-grained
 *    estimate that the .h docstring warns about. For the OK/HIGH/LOW boundary
 *    this is adequate.
 *
 * 4. NaN/Inf POISONING
 *    isnan/isinf on a freshly-pushed sample → silent ignore (preserves prior
 *    state, no event start). Mirrors PositionLoop::update()'s neutral
 *    handling. We never copy a non-finite value into lookback_buf_ or into
 *    the running min/max.
 */

#include "cascade_self_audit.h"

#include <math.h>  // isfinite

namespace {

inline float fminf_local(float a, float b) { return (a < b) ? a : b; }
inline float fmaxf_local(float a, float b) { return (a > b) ? a : b; }

}  // namespace

CascadeSelfAudit::CascadeSelfAudit() {
    // Construct in the same state reset() leaves us. Explicitly initialise
    // every member — no implicit zero-init relied on (the buffers are POD
    // and would otherwise hold whatever was on the stack/in .bss).
    reset();
}

void CascadeSelfAudit::reset() {
    for (uint8_t i = 0; i < POSLOOP_AUDIT_LOOKBACK_TICKS; ++i) {
        lookback_buf_[i] = 0.0f;
    }
    lookback_idx_         = 0;
    lookback_fill_        = 0;
    prev_position_m_      = 0.0f;
    have_prev_            = false;

    event_active_         = false;
    event_ticks_left_     = 0;
    event_settled_value_  = 0.0f;
    event_window_min_     = 0.0f;
    event_window_max_     = 0.0f;
    last_sign_            = 0;
    crossing_count_       = 0;

    quiet_ticks_left_     = 0;

    events_classified_    = 0;
    ok_count_             = 0;
    high_count_           = 0;
    low_count_            = 0;
    verdict_              = CascadeAuditVerdict::UNKNOWN;
}

void CascadeSelfAudit::push_sample(float position_m, uint32_t /*now_ms*/) {
    // --- NaN/Inf guard -----------------------------------------------------
    // A non-finite sample poisons running min/max comparisons and would
    // permanently corrupt the lookback mean. Silently skip — caller sees
    // no verdict change.
    if (!isfinite(position_m)) {
        return;
    }

    // --- Cool-down decrement ----------------------------------------------
    if (quiet_ticks_left_ > 0) {
        --quiet_ticks_left_;
    }

    // --- Active-event update ----------------------------------------------
    if (event_active_) {
        // Update running min/max — the midpoint serves as the running
        // reference level for crossing detection.
        event_window_min_ = fminf_local(event_window_min_, position_m);
        event_window_max_ = fmaxf_local(event_window_max_, position_m);
        const float mid   = 0.5f * (event_window_min_ + event_window_max_);

        // Sign of deviation from the current midpoint estimate.
        const float dev = position_m - mid;
        int8_t cur_sign;
        if (dev > 0.0f)      cur_sign = +1;
        else if (dev < 0.0f) cur_sign = -1;
        else                 cur_sign = 0;

        // Count a crossing whenever the sign FLIPS between -1 and +1.
        // (0 → +/- or +/- → 0 does NOT count: a sample exactly at the
        // midpoint is not a crossing in itself.) Saturate at 255 to keep
        // the byte counter safe even on pathological inputs.
        if (last_sign_ != 0 && cur_sign != 0 && cur_sign != last_sign_) {
            if (crossing_count_ < 255) {
                ++crossing_count_;
            }
        }
        if (cur_sign != 0) {
            last_sign_ = cur_sign;
        }

        // Window expiry — classify and tally.
        if (event_ticks_left_ > 0) {
            --event_ticks_left_;
        }
        if (event_ticks_left_ == 0) {
            // Classify by crossing count.
            //   0  → HIGH (overdamped: never returned through the settled level)
            //   1  → OK   (one clean settling pass)
            //   2+ → LOW  (oscillation)
            if (crossing_count_ == 0) {
                if (high_count_ < 255) ++high_count_;
            } else if (crossing_count_ == 1) {
                if (ok_count_   < 255) ++ok_count_;
            } else {
                if (low_count_  < 255) ++low_count_;
            }
            if (events_classified_ < 255) ++events_classified_;

            event_active_        = false;
            // Lock event_settled_value_ for inspection (debug); the value
            // itself is no longer used after classification.
            event_settled_value_ = 0.5f * (event_window_min_ +
                                           event_window_max_);

            // Start a quiet period so the tail of this event cannot seed a
            // spurious next-event detection.
            quiet_ticks_left_    = POSLOOP_AUDIT_QUIET_TICKS;

            recompute_verdict_();
        }
        // Whether or not the event ended this tick, also keep the lookback
        // buffer fresh so a post-quiet detection has a current pre-step mean.
    }

    // --- Step-event detection (only when no event active AND quiet-clear) -
    if (!event_active_ && quiet_ticks_left_ == 0 &&
        lookback_fill_ >= POSLOOP_AUDIT_LOOKBACK_TICKS) {
        // Compute the lookback mean (pre-step reference).
        float mean = 0.0f;
        for (uint8_t i = 0; i < POSLOOP_AUDIT_LOOKBACK_TICKS; ++i) {
            mean += lookback_buf_[i];
        }
        mean /= (float)POSLOOP_AUDIT_LOOKBACK_TICKS;

        // Single-tick delta against the lookback mean. The threshold is on
        // the deviation from the pre-step level, NOT on tick-to-tick delta —
        // a slow drift won't trip it, only a fast jump.
        const float dev_from_mean = position_m - mean;
        const float abs_dev = (dev_from_mean < 0.0f) ? -dev_from_mean
                                                     :  dev_from_mean;
        if (abs_dev > POSLOOP_AUDIT_STEP_THRESHOLD_M) {
            // Step detected — open an observation event.
            event_active_       = true;
            event_ticks_left_   = POSLOOP_AUDIT_WINDOW_TICKS;
            event_window_min_   = position_m;
            event_window_max_   = position_m;
            event_settled_value_= position_m;
            last_sign_          = 0;
            crossing_count_     = 0;
        }
    }

    // --- Lookback buffer update -------------------------------------------
    // Always write the freshest sample; the buffer is small and we want the
    // post-quiet pre-step mean to reflect the most recent state.
    lookback_buf_[lookback_idx_] = position_m;
    lookback_idx_ = (uint8_t)((lookback_idx_ + 1) % POSLOOP_AUDIT_LOOKBACK_TICKS);
    if (lookback_fill_ < POSLOOP_AUDIT_LOOKBACK_TICKS) {
        ++lookback_fill_;
    }

    prev_position_m_ = position_m;
    have_prev_       = true;
}

void CascadeSelfAudit::recompute_verdict_() {
    // Wait until enough events are in the tally — otherwise UNKNOWN.
    if (events_classified_ < POSLOOP_AUDIT_MIN_EVENTS) {
        verdict_ = CascadeAuditVerdict::UNKNOWN;
        return;
    }
    // Majority vote — tie breaks to the more conservative verdict (HIGH > LOW
    // > OK), which matches the operational priority: an unrecognised plant
    // change is worse than under-damping, which is worse than nominal.
    // Implementation: pick the largest tally; on a tie prefer HIGH, then LOW.
    const uint8_t h = high_count_;
    const uint8_t l = low_count_;
    const uint8_t o = ok_count_;
    if (h >= l && h >= o && h > 0) {
        verdict_ = CascadeAuditVerdict::KVEL_HIGH;
    } else if (l >= o && l > 0) {
        verdict_ = CascadeAuditVerdict::KVEL_LOW;
    } else if (o > 0) {
        verdict_ = CascadeAuditVerdict::KVEL_OK;
    } else {
        verdict_ = CascadeAuditVerdict::UNKNOWN;
    }
}
