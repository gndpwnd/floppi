/*
 * DSP Filter Primitives
 * Biquad low-pass and notch filters for USE_OPTIMIZATION
 */

#ifndef FILTERS_H
#define FILTERS_H

#include <Arduino.h>

struct BiquadFilter {
    float b0, b1, b2, a1, a2;  // Coefficients (normalized by a0)
    float d1, d2;               // State (Direct Form II Transposed)
};

// Initialize biquad as Butterworth low-pass filter
void biquad_init_lpf(BiquadFilter *f, float cutoff_hz, float sample_rate_hz);

// Initialize biquad as notch (band-reject) filter
void biquad_init_notch(BiquadFilter *f, float center_hz, float bandwidth_hz, float sample_rate_hz);

// Apply biquad filter to one sample, returns filtered value
float biquad_apply(BiquadFilter *f, float input);

#endif // FILTERS_H
