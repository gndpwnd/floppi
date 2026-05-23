/**
 * NoiseFloorEstimator — baseline gyro + accel sensor noise-floor capture.
 *
 * Purpose (Phase: noise-floor measurement layer, 2026-05-22):
 *   The framework's "measure, don't hardcode" doctrine wants several control
 *   constants re-expressed as multiples of the sensor noise floor (e.g.
 *   STUCK_GYRO_DPS = 3·σ_gyro, HELD lift gate = 3·σ_accel). The triage doc
 *   docs/findings/mega_scope_violation_triage_2026-05-22.md found those rows
 *   "doubly blocked": they need both a balancing bot AND a noise-floor
 *   measurement that did not exist anywhere in the tree. This module is that
 *   missing measurement layer.
 *
 * What it does:
 *   Accumulates a running mean + variance (Welford's online algorithm) of a
 *   scalar signal over a fixed sample window while the bot is quiet/stationary.
 *   Two independent estimators are intended — one fed the gyro pitch-rate
 *   magnitude, one fed the accel-magnitude deviation from gravity. Once
 *   `kWindowSamples` quiet samples have been collected, the standard deviation
 *   is frozen and `settled()` latches true.
 *
 * What it deliberately does NOT do:
 *   - It does NOT touch motors, PID gains, thresholds, or state transitions.
 *   - It does NOT decide what is "quiet" — the caller gates pushing samples on
 *     its own stillness signal (BalanceApp reuses the LP-filtered lateral-gyro
 *     magnitude). Non-quiet ticks simply aren't fed in, and any feed during a
 *     non-quiet window resets the in-progress accumulation (see push()).
 *   - It is PURE observation. Nothing downstream reads it yet; the getters
 *     exist so a future (bench-gated) step can derive thresholds from σ.
 *
 * Numerics: Welford keeps a running mean and M2 (sum of squared deviations)
 * so variance = M2 / (n - 1) without storing samples. Float-only; ~16 bytes
 * of state per estimator — cheap on AVR. The window count latches the result
 * so later motion (post-settle) cannot corrupt the captured floor.
 *
 * Header-only: tiny enough that a .cpp adds more ceremony than it saves, and
 * the native test links it without an extra translation unit.
 */
#ifndef AUTO_ORIENTATION_NOISE_FLOOR_ESTIMATOR_H
#define AUTO_ORIENTATION_NOISE_FLOOR_ESTIMATOR_H

#include <stdint.h>
#include <math.h>

class NoiseFloorEstimator {
public:
    // Number of consecutive quiet samples required before the floor is
    // considered valid. At the 5 ms PID tick this is 1.0 s of stillness —
    // long enough for a stable mean/variance, short enough to capture during
    // a startup/IDLE quiet window before BOOTSTRAP/RUN.
    static const uint16_t kWindowSamples = 200;

    NoiseFloorEstimator() { reset(); }

    // Discard any in-progress accumulation and clear the latched result.
    void reset() {
        count_     = 0;
        mean_      = 0.0f;
        m2_        = 0.0f;
        std_dev_   = 0.0f;
        settled_   = false;
    }

    // Feed one sample collected during a confirmed-quiet tick. Welford update.
    // Once kWindowSamples have been gathered the std-dev is frozen and the
    // estimator latches settled(); further push()es are ignored so post-settle
    // motion can never corrupt the captured floor. Re-arm with reset().
    void push(float sample) {
        if (settled_) return;
        // Audit P2-002 / P2-003: reject a non-finite sample (gyro/accel glitch)
        // before it enters the Welford recurrence — a single NaN would poison
        // mean_/m2_ and latch a NaN std_dev_, and an Inf would push m2_ to +Inf.
        // The window's quiet assumption is already broken by such a sample, so
        // throw away the in-progress accumulation (matches abort_window) rather
        // than counting it. Future threshold consumers (3·σ gates) then never
        // see a non-finite floor.
        if (isnan(sample) || isinf(sample)) {
            abort_window();
            return;
        }
        count_++;
        const float delta = sample - mean_;
        mean_ += delta / static_cast<float>(count_);
        const float delta2 = sample - mean_;
        m2_ += delta * delta2;
        if (count_ >= kWindowSamples) {
            // Sample variance (n-1 denominator). count_ >= 200 here so the
            // divisor is always positive.
            const float variance = m2_ / static_cast<float>(count_ - 1);
            std_dev_ = (variance > 0.0f) ? sqrtf(variance) : 0.0f;
            settled_ = true;
        }
    }

    // Abandon the in-progress window WITHOUT clearing a previously latched
    // result. Call this when the quiet assumption is broken mid-window (motion
    // detected) so a partial, motion-contaminated accumulation is thrown away.
    // A floor that already settled stays settled.
    void abort_window() {
        if (settled_) return;
        count_ = 0;
        mean_  = 0.0f;
        m2_    = 0.0f;
    }

    // The captured standard deviation of the signal (same units as the fed
    // samples). Zero until settled(); the design intent is that callers read
    // this only once settled() is true.
    float std_dev() const { return std_dev_; }

    // The captured mean of the signal over the quiet window (same units).
    // Useful for sanity checks (e.g. accel-deviation mean should be ~0).
    float mean() const { return mean_; }

    // True once kWindowSamples quiet samples have been gathered and the floor
    // is frozen. Latches — stays true until reset().
    bool settled() const { return settled_; }

    // Number of quiet samples accumulated in the current (or completed) window.
    uint16_t sample_count() const { return count_; }

private:
    uint16_t count_;
    float    mean_;
    float    m2_;        // running sum of squared deviations (Welford)
    float    std_dev_;   // latched standard deviation once settled
    bool     settled_;
};

#endif  // AUTO_ORIENTATION_NOISE_FLOOR_ESTIMATOR_H
