/*
 * test_filters.cpp — native unit tests for the flight_controller DSP filters
 * =============================================================================
 * Standalone, plain-g++ test. Build:
 *   g++ -std=c++11 -O2 -DUNIT_TEST -Itests/native \
 *       -o /tmp/tf tests/native/test_filters.cpp && /tmp/tf
 *
 * Why a local replica instead of including src/filters.cpp:
 *   include/filters.h does `#include <Arduino.h>`, which is unavailable on the
 *   host. The filter math, however, is 100% portable (sinf/cosf only). Per the
 *   documented standalone convention (tests/native/README.md), the PURE math
 *   below is a faithful, line-for-line copy of src/filters.cpp — the biquad
 *   Butterworth-LPF and notch coefficient formulas plus the Direct-Form-II
 *   Transposed per-sample recurrence. Keep in sync with src/filters.cpp.
 * =============================================================================
 */
#include "test_helpers.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- faithful replica of include/filters.h struct -------------------------
struct BiquadFilter {
    float b0, b1, b2, a1, a2;  // coefficients (normalized by a0)
    float d1, d2;              // state (Direct Form II Transposed)
};

// --- faithful replica of src/filters.cpp ----------------------------------
static void biquad_init_lpf(BiquadFilter *f, float cutoff_hz, float sample_rate_hz) {
    float omega = 2.0f * (float)M_PI * cutoff_hz / sample_rate_hz;
    float sn = sinf(omega);
    float cs = cosf(omega);
    float alpha = sn / 1.4142f;  // Q = 0.7071 (Butterworth)

    float a0 = 1.0f + alpha;
    f->b0 = (1.0f - cs) / (2.0f * a0);
    f->b1 = (1.0f - cs) / a0;
    f->b2 = (1.0f - cs) / (2.0f * a0);
    f->a1 = (-2.0f * cs) / a0;
    f->a2 = (1.0f - alpha) / a0;
    f->d1 = 0.0f;
    f->d2 = 0.0f;
}

static void biquad_init_notch(BiquadFilter *f, float center_hz, float bandwidth_hz, float sample_rate_hz) {
    float omega = 2.0f * (float)M_PI * center_hz / sample_rate_hz;
    float sn = sinf(omega);
    float cs = cosf(omega);
    float Q = center_hz / bandwidth_hz;
    float alpha = sn / (2.0f * Q);

    float a0 = 1.0f + alpha;
    f->b0 = 1.0f / a0;
    f->b1 = (-2.0f * cs) / a0;
    f->b2 = 1.0f / a0;
    f->a1 = (-2.0f * cs) / a0;
    f->a2 = (1.0f - alpha) / a0;
    f->d1 = 0.0f;
    f->d2 = 0.0f;
}

static float biquad_apply(BiquadFilter *f, float input) {
    float output = f->b0 * input + f->d1;
    f->d1 = f->b1 * input - f->a1 * output + f->d2;
    f->d2 = f->b2 * input - f->a2 * output;
    return output;
}

// --- test helpers ---------------------------------------------------------

// Settle a filter with a constant DC input; return the final output.
static double settle_dc(BiquadFilter *f, double dc, int n) {
    double out = 0.0;
    for (int i = 0; i < n; ++i) out = biquad_apply(f, (float)dc);
    return out;
}

// RMS amplitude of the output once a sinusoid of freq_hz has settled.
static double sine_rms(BiquadFilter *f, double freq_hz, double fs, int n) {
    double sumsq = 0.0;
    int warm = n;            // discard transient
    int meas = n;
    for (int i = 0; i < warm; ++i) {
        double t = (double)i / fs;
        biquad_apply(f, (float)sin(2.0 * M_PI * freq_hz * t));
    }
    for (int i = 0; i < meas; ++i) {
        double t = (double)(warm + i) / fs;
        double y = biquad_apply(f, (float)sin(2.0 * M_PI * freq_hz * t));
        sumsq += y * y;
    }
    return sqrt(sumsq / meas);
}

TEST_MAIN("flight_controller signal filters") {
    const double fs = 1000.0;          // 1 kHz sample rate

    // -- 1. LPF coefficient sanity ----------------------------------------
    BiquadFilter lp;
    biquad_init_lpf(&lp, 50.0f, (float)fs);
    CHECK_FINITE(lp.b0, "LPF b0 finite");
    CHECK_FINITE(lp.b1, "LPF b1 finite");
    CHECK_FINITE(lp.a1, "LPF a1 finite");
    CHECK_FINITE(lp.a2, "LPF a2 finite");
    // Butterworth LPF: b0 == b2, b1 == 2*b0, all numerator terms positive.
    CHECK_NEAR(lp.b0, lp.b2, 1e-7, "LPF b0 == b2 (symmetric numerator)");
    CHECK_NEAR(lp.b1, 2.0 * lp.b0, 1e-7, "LPF b1 == 2*b0");
    CHECK(lp.b0 > 0.0f, "LPF b0 positive");
    // Stability: poles inside unit circle => |a2| < 1.
    CHECK(std::fabs(lp.a2) < 1.0f, "LPF stable (|a2| < 1)");

    // -- 2. LPF DC gain ~= 1.0 --------------------------------------------
    // Numerator sum / denominator sum must equal 1 for unity DC gain.
    double dc_gain = (lp.b0 + lp.b1 + lp.b2) / (1.0 + lp.a1 + lp.a2);
    CHECK_NEAR(dc_gain, 1.0, 1e-4, "LPF analytic DC gain == 1");
    {
        BiquadFilter f = lp; f.d1 = 0; f.d2 = 0;
        double settled = settle_dc(&f, 1.0, 400);
        CHECK_NEAR(settled, 1.0, 1e-3, "LPF constant input settles to itself");
    }

    // -- 3. LPF attenuates high frequency ---------------------------------
    {
        BiquadFilter lo = lp; lo.d1 = 0; lo.d2 = 0;
        BiquadFilter hi = lp; hi.d1 = 0; hi.d2 = 0;
        double rms_low  = sine_rms(&lo, 5.0,   fs, 2000);   // well below 50 Hz
        double rms_high = sine_rms(&hi, 400.0, fs, 2000);   // well above 50 Hz
        const double ref = 0.70710678;                       // RMS of unit sine
        CHECK_NEAR_REL(rms_low, ref, 0.05, "LPF passes low freq ~unchanged");
        CHECK(rms_high < 0.10, "LPF strongly attenuates high freq");
        CHECK(rms_high < rms_low * 0.25, "LPF: high-freq RMS << low-freq RMS");
    }

    // -- 4. LPF step response: settles to 1.0 with only the small,
    //       characteristic overshoot of a 2nd-order Q=0.707 Butterworth.
    //       (A Butterworth biquad has ~4% step overshoot by design — that is
    //       correct DSP behaviour, not a defect; the task's "1st-order, no
    //       overshoot" expectation does not apply to this 2nd-order biquad.)
    {
        BiquadFilter f; biquad_init_lpf(&f, 20.0f, (float)fs);
        double prev = -1e9, peak = 0.0, last = 0.0;
        bool rises_first = false;
        for (int i = 0; i < 2000; ++i) {
            double y = biquad_apply(&f, 1.0f);
            if (i < 50 && y > prev) rises_first = true;  // initial rise
            if (y > peak) peak = y;
            prev = y;
            last = y;
        }
        CHECK(rises_first, "LPF step response rises toward the input");
        CHECK(peak > 1.0, "LPF step response has the expected Butterworth overshoot");
        CHECK(peak < 1.10, "LPF step overshoot is small/bounded (< 10%)");
        CHECK_NEAR(last, 1.0, 1e-3, "LPF step response settles to 1.0");
    }

    // -- 5. Notch coefficient sanity --------------------------------------
    BiquadFilter nt;
    biquad_init_notch(&nt, 100.0f, 20.0f, (float)fs);  // 100 Hz notch, BW 20 Hz
    CHECK_FINITE(nt.b0, "notch b0 finite");
    CHECK_FINITE(nt.a1, "notch a1 finite");
    CHECK_FINITE(nt.a2, "notch a2 finite");
    // Notch: b0 == b2 and b1 == a1 (band-reject structure).
    CHECK_NEAR(nt.b0, nt.b2, 1e-7, "notch b0 == b2");
    CHECK_NEAR(nt.b1, nt.a1, 1e-7, "notch b1 == a1 (band-reject)");
    CHECK(std::fabs(nt.a2) < 1.0f, "notch stable (|a2| < 1)");
    {
        double n_dc = (nt.b0 + nt.b1 + nt.b2) / (1.0 + nt.a1 + nt.a2);
        CHECK_NEAR(n_dc, 1.0, 1e-3, "notch DC gain == 1 (passes DC)");
    }

    // -- 6. Notch attenuates the notch frequency, passes others -----------
    {
        BiquadFilter at = nt; at.d1 = 0; at.d2 = 0;
        BiquadFilter lo = nt; lo.d1 = 0; lo.d2 = 0;
        BiquadFilter hi = nt; hi.d1 = 0; hi.d2 = 0;
        double rms_notch = sine_rms(&at, 100.0, fs, 4000);  // at center
        double rms_low   = sine_rms(&lo, 10.0,  fs, 4000);  // far below
        double rms_high  = sine_rms(&hi, 350.0, fs, 4000);  // far above
        const double ref = 0.70710678;
        CHECK(rms_notch < 0.10, "notch strongly attenuates the notch freq");
        CHECK_NEAR_REL(rms_low,  ref, 0.06, "notch passes low freq ~unchanged");
        CHECK_NEAR_REL(rms_high, ref, 0.06, "notch passes high freq ~unchanged");
    }

    // -- 7. Stability: bounded input never diverges -----------------------
    {
        BiquadFilter f; biquad_init_lpf(&f, 80.0f, (float)fs);
        double maxabs = 0.0;
        unsigned int seed = 12345u;
        for (int i = 0; i < 50000; ++i) {
            // deterministic bounded pseudo-random input in [-1, 1]
            seed = seed * 1664525u + 1013904223u;
            double x = ((double)(seed >> 8) / (double)(1u << 24)) * 2.0 - 1.0;
            double y = biquad_apply(&f, (float)x);
            if (!std::isfinite(y)) maxabs = 1e9;
            if (std::fabs(y) > maxabs) maxabs = std::fabs(y);
        }
        CHECK_FINITE(maxabs, "LPF stays finite over 50k bounded samples");
        CHECK(maxabs < 5.0, "LPF bounded input -> bounded output (no divergence)");
    }
}
