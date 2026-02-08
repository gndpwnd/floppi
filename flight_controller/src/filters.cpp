/*
 * DSP Filter Primitives
 * Biquad low-pass and notch filters for USE_OPTIMIZATION
 */

#include "filters.h"
#include <math.h>

void biquad_init_lpf(BiquadFilter *f, float cutoff_hz, float sample_rate_hz) {
    float omega = 2.0f * M_PI * cutoff_hz / sample_rate_hz;
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

void biquad_init_notch(BiquadFilter *f, float center_hz, float bandwidth_hz, float sample_rate_hz) {
    float omega = 2.0f * M_PI * center_hz / sample_rate_hz;
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

float biquad_apply(BiquadFilter *f, float input) {
    float output = f->b0 * input + f->d1;
    f->d1 = f->b1 * input - f->a1 * output + f->d2;
    f->d2 = f->b2 * input - f->a2 * output;
    return output;
}
