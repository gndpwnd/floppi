# Signal Period / Frequency Detection — Research & Implementation Guide

> **Status**: COMPLETE
> Last updated: 2026-02-07

Research on practical signal period and frequency detection techniques for fc_tool's
real-time serial data plotter. All implementations target JavaScript running in a
browser (or Tauri webview) with Chart.js, processing 1000-sample buffers at up to 60fps.

---

## Table of Contents

1. [Zero-Crossing Detection](#1-zero-crossing-detection)
2. [Autocorrelation-Based Period Detection](#2-autocorrelation-based-period-detection)
3. [FFT-Based Frequency Detection](#3-fft-based-frequency-detection)
4. [Threshold Trigger (Oscilloscope-Style)](#4-threshold-trigger-oscilloscope-style)
5. [Peak-to-Peak Detection](#5-peak-to-peak-detection)
6. [JavaScript Signal Processing Libraries](#6-javascript-signal-processing-libraries)
7. [Comparison Table](#7-comparison-table)
8. [Recommendations for fc_tool](#8-recommendations-for-fc_tool)

---

## 1. Zero-Crossing Detection

### Concept

Count the number of times a signal crosses its mean value (or a specified DC level).
The period is derived from the average spacing between crossings. For a sinusoidal
signal, there are exactly 2 zero-crossings per period, so:

```
period = (2 * total_samples) / crossing_count
frequency = crossing_count / (2 * total_duration)
```

### Algorithm

1. Compute the mean of the buffer (DC offset removal).
2. Subtract the mean from all samples (center the signal around zero).
3. Walk through the array and detect sign changes between consecutive samples.
4. Optionally interpolate to find the fractional crossing point.
5. Compute the average interval between consecutive crossings.
6. Period = 2 * average_interval (since 2 crossings per period for a fundamental).

### JavaScript Implementation

```javascript
/**
 * Detect signal period using zero-crossing analysis.
 *
 * @param {Float32Array|number[]} samples - Input signal buffer
 * @param {number} sampleRate - Samples per second (Hz)
 * @param {object} [options]
 * @param {number} [options.dcLevel] - Custom DC level (default: auto-computed mean)
 * @param {boolean} [options.risingOnly] - Count only rising-edge crossings
 * @param {boolean} [options.interpolate] - Use linear interpolation for sub-sample accuracy
 * @returns {{ period: number, frequency: number, crossings: number[], confidence: number }}
 */
function zeroCrossingDetect(samples, sampleRate, options = {}) {
  const n = samples.length;
  if (n < 4) return null;

  // 1. Compute DC level (mean) if not provided
  let dc = options.dcLevel;
  if (dc === undefined) {
    let sum = 0;
    for (let i = 0; i < n; i++) sum += samples[i];
    dc = sum / n;
  }

  // 2. Find crossings
  const crossings = []; // indices (or fractional indices) of crossings
  const risingOnly = options.risingOnly || false;
  const interpolate = options.interpolate !== false; // default true

  for (let i = 1; i < n; i++) {
    const prev = samples[i - 1] - dc;
    const curr = samples[i] - dc;

    // Detect sign change
    if (prev * curr < 0) {
      const isRising = curr > prev;

      if (risingOnly && !isRising) continue;

      if (interpolate) {
        // Linear interpolation for sub-sample accuracy
        const fraction = prev / (prev - curr);
        crossings.push(i - 1 + fraction);
      } else {
        crossings.push(i);
      }
    }
  }

  if (crossings.length < 2) return null;

  // 3. Compute average interval between crossings
  const intervals = [];
  for (let i = 1; i < crossings.length; i++) {
    intervals.push(crossings[i] - crossings[i - 1]);
  }

  const avgInterval = intervals.reduce((a, b) => a + b, 0) / intervals.length;

  // 4. Compute period
  // If risingOnly: each interval IS a full period
  // If both edges: each interval is a half-period
  const multiplier = risingOnly ? 1 : 2;
  const periodSamples = avgInterval * multiplier;
  const periodSeconds = periodSamples / sampleRate;
  const frequency = 1 / periodSeconds;

  // 5. Confidence: based on consistency of intervals
  const intervalVariance = intervals.reduce((acc, val) => {
    return acc + (val - avgInterval) ** 2;
  }, 0) / intervals.length;
  const intervalStdDev = Math.sqrt(intervalVariance);
  const coeffOfVariation = intervalStdDev / avgInterval;
  // confidence: 1.0 = perfectly periodic, 0.0 = random
  const confidence = Math.max(0, 1 - coeffOfVariation);

  return {
    period: periodSamples,
    periodSeconds,
    frequency,
    crossings,
    confidence
  };
}
```

### Usage Example

```javascript
// 1000 samples at 1kHz sample rate
const result = zeroCrossingDetect(buffer, 1000, {
  risingOnly: true,
  interpolate: true
});

if (result && result.confidence > 0.7) {
  console.log(`Period: ${result.period.toFixed(1)} samples`);
  console.log(`Frequency: ${result.frequency.toFixed(2)} Hz`);
  console.log(`Confidence: ${(result.confidence * 100).toFixed(0)}%`);
}
```

### Characteristics

| Property | Value |
|----------|-------|
| **Time complexity** | O(n) — single pass through buffer |
| **Space complexity** | O(k) — where k = number of crossings |
| **Noise tolerance** | Poor to moderate. Noisy signals produce spurious crossings. Hysteresis or pre-filtering helps. |
| **Real-time at 60fps** | Yes, easily. 1000 samples in ~0.05ms. |
| **Best for** | Clean sinusoidal or square wave signals |
| **Worst for** | Noisy signals, signals with multiple frequency components |

### Improving Noise Tolerance

Add hysteresis (Schmitt trigger behavior) to avoid multiple crossings from noise:

```javascript
/**
 * Zero-crossing with hysteresis (Schmitt trigger).
 * Signal must exceed (dc + hysteresis) before a falling crossing is valid,
 * and drop below (dc - hysteresis) before a rising crossing is valid.
 */
function zeroCrossingWithHysteresis(samples, sampleRate, hysteresis = 0) {
  const n = samples.length;
  let sum = 0;
  for (let i = 0; i < n; i++) sum += samples[i];
  const dc = sum / n;

  const crossings = [];
  let state = samples[0] > dc ? 1 : -1; // 1 = above, -1 = below

  for (let i = 1; i < n; i++) {
    const val = samples[i] - dc;

    if (state === -1 && val > hysteresis) {
      // Rising crossing
      crossings.push(i);
      state = 1;
    } else if (state === 1 && val < -hysteresis) {
      // Falling crossing
      crossings.push(i);
      state = -1;
    }
  }

  // Same period calculation as above...
  if (crossings.length < 2) return null;

  const intervals = [];
  for (let i = 1; i < crossings.length; i++) {
    intervals.push(crossings[i] - crossings[i - 1]);
  }

  const avgInterval = intervals.reduce((a, b) => a + b, 0) / intervals.length;
  const periodSamples = avgInterval * 2; // both edges counted
  const periodSeconds = periodSamples / sampleRate;

  return {
    period: periodSamples,
    periodSeconds,
    frequency: 1 / periodSeconds,
    crossings,
    crossingCount: crossings.length
  };
}
```

**Hysteresis value selection**: Set hysteresis to approximately 5-15% of the signal's
peak-to-peak amplitude. This eliminates noise-induced false crossings without missing
legitimate ones.

---

## 2. Autocorrelation-Based Period Detection

### Concept

Autocorrelation measures how similar a signal is to a delayed (shifted) copy of itself.
For a periodic signal, the autocorrelation peaks at lag values equal to the period
(and multiples of the period). This is the most robust method for noisy signals because
noise is uncorrelated with itself and averages out.

The autocorrelation at lag `k` is:

```
R(k) = sum(x[i] * x[i + k]) for i = 0 to N-k-1
```

The first significant peak in R(k) after the zero-lag peak (R(0)) indicates the period.

### Naive Implementation — O(n^2)

```javascript
/**
 * Autocorrelation-based period detection (naive O(n^2) method).
 *
 * @param {Float32Array|number[]} samples - Input signal
 * @param {number} sampleRate - Samples per second
 * @param {object} [options]
 * @param {number} [options.minPeriod] - Minimum expected period in samples (default: 2)
 * @param {number} [options.maxPeriod] - Maximum expected period in samples (default: n/2)
 * @returns {{ period: number, frequency: number, confidence: number, correlation: Float32Array }}
 */
function autocorrelationDetect(samples, sampleRate, options = {}) {
  const n = samples.length;
  const minLag = options.minPeriod || 2;
  const maxLag = options.maxPeriod || Math.floor(n / 2);

  // 1. Remove DC offset
  let sum = 0;
  for (let i = 0; i < n; i++) sum += samples[i];
  const mean = sum / n;

  // 2. Compute autocorrelation for each lag
  const correlation = new Float32Array(maxLag + 1);

  for (let lag = 0; lag <= maxLag; lag++) {
    let acc = 0;
    for (let i = 0; i < n - lag; i++) {
      acc += (samples[i] - mean) * (samples[i + lag] - mean);
    }
    correlation[lag] = acc;
  }

  // Normalize by zero-lag value
  const norm = correlation[0];
  if (norm === 0) return null;
  for (let lag = 0; lag <= maxLag; lag++) {
    correlation[lag] /= norm;
  }

  // 3. Find the first significant peak after minLag
  //    A peak must be > previous and > next value
  let bestLag = 0;
  let bestVal = -Infinity;

  // Walk past the initial descent from lag=0
  let descending = true;
  for (let lag = minLag; lag < maxLag; lag++) {
    if (descending) {
      if (correlation[lag] > correlation[lag - 1]) {
        descending = false; // start of first peak region
      } else {
        continue;
      }
    }

    if (correlation[lag] > bestVal) {
      bestVal = correlation[lag];
      bestLag = lag;
    }

    // Once we've found a peak and start descending, stop
    if (correlation[lag] < correlation[lag - 1] && bestVal > 0.1) {
      break;
    }
  }

  if (bestLag === 0) return null;

  // 4. Parabolic interpolation for sub-sample accuracy
  const y0 = correlation[bestLag - 1];
  const y1 = correlation[bestLag];
  const y2 = correlation[bestLag + 1] || y1;
  const refinedLag = bestLag + 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2);

  const periodSeconds = refinedLag / sampleRate;

  return {
    period: refinedLag,
    periodSeconds,
    frequency: 1 / periodSeconds,
    confidence: bestVal, // 1.0 = perfectly periodic
    correlation
  };
}
```

### FFT-Accelerated Autocorrelation — O(n log n)

The autocorrelation can be computed much faster using the Wiener-Khinchin theorem:
`R(k) = IFFT(|FFT(x)|^2)`. This reduces complexity from O(n^2) to O(n log n).

```javascript
/**
 * FFT-accelerated autocorrelation using fft.js library.
 *
 * @param {Float32Array|number[]} samples
 * @param {number} sampleRate
 * @param {object} [options]
 * @returns {{ period: number, frequency: number, confidence: number }}
 */
function autocorrelationFFT(samples, sampleRate, options = {}) {
  const n = samples.length;

  // fft.js requires power-of-2 size
  const fftSize = nextPowerOf2(n * 2); // zero-pad to avoid circular correlation
  const fft = new FFT(fftSize);

  // 1. Prepare input: remove mean, zero-pad
  const input = new Float32Array(fftSize);
  let sum = 0;
  for (let i = 0; i < n; i++) sum += samples[i];
  const mean = sum / n;
  for (let i = 0; i < n; i++) input[i] = samples[i] - mean;

  // 2. Forward FFT
  const spectrum = fft.createComplexArray(); // interleaved [re, im, re, im, ...]
  fft.realTransform(spectrum, input);
  fft.completeSpectrum(spectrum);

  // 3. Power spectrum: |FFT(x)|^2
  for (let i = 0; i < fftSize; i++) {
    const re = spectrum[2 * i];
    const im = spectrum[2 * i + 1];
    spectrum[2 * i] = re * re + im * im;
    spectrum[2 * i + 1] = 0;
  }

  // 4. Inverse FFT to get autocorrelation
  const autoCorr = new Float32Array(fftSize);
  fft.inverseTransform(autoCorr, spectrum);

  // autoCorr is interleaved complex; take real parts
  const correlation = new Float32Array(fftSize / 2);
  for (let i = 0; i < fftSize / 2; i++) {
    correlation[i] = autoCorr[2 * i]; // real part
  }

  // Normalize
  const norm = correlation[0];
  if (norm === 0) return null;
  for (let i = 0; i < correlation.length; i++) {
    correlation[i] /= norm;
  }

  // 5. Find first peak (same algorithm as naive version)
  const minLag = options.minPeriod || 2;
  const maxLag = options.maxPeriod || Math.floor(n / 2);

  let bestLag = 0;
  let bestVal = -Infinity;
  let descending = true;

  for (let lag = minLag; lag < Math.min(maxLag, correlation.length); lag++) {
    if (descending) {
      if (correlation[lag] > correlation[lag - 1]) {
        descending = false;
      } else {
        continue;
      }
    }

    if (correlation[lag] > bestVal) {
      bestVal = correlation[lag];
      bestLag = lag;
    }

    if (correlation[lag] < correlation[lag - 1] && bestVal > 0.1) {
      break;
    }
  }

  if (bestLag === 0) return null;

  // Parabolic interpolation
  const y0 = correlation[bestLag - 1] || 0;
  const y1 = correlation[bestLag];
  const y2 = correlation[bestLag + 1] || y1;
  const denom = y0 - 2 * y1 + y2;
  const refinedLag = denom !== 0 ? bestLag + 0.5 * (y0 - y2) / denom : bestLag;

  return {
    period: refinedLag,
    periodSeconds: refinedLag / sampleRate,
    frequency: sampleRate / refinedLag,
    confidence: bestVal,
    correlation
  };
}

function nextPowerOf2(n) {
  let p = 1;
  while (p < n) p <<= 1;
  return p;
}
```

### Characteristics

| Property | Value |
|----------|-------|
| **Time complexity** | O(n^2) naive, O(n log n) FFT-accelerated |
| **Space complexity** | O(n) |
| **Noise tolerance** | Excellent. Noise is uncorrelated, so it averages to zero in the sum. |
| **Real-time at 60fps** | Naive: marginal for 1000 samples (~1ms). FFT: yes, ~0.2ms. |
| **Best for** | Noisy periodic signals, unknown frequency |
| **Worst for** | Non-stationary signals (frequency changes over time) |

### Performance Note

For 1000 samples, the naive O(n^2) method performs ~500,000 multiply-adds. On modern
hardware this takes roughly 0.5-2ms in JavaScript. That is borderline for 60fps
(16.6ms budget) when multiple plots need processing. The FFT-accelerated version
is strongly recommended for production use, or offload to a Web Worker.

---

## 3. FFT-Based Frequency Detection

### Concept

Compute the Discrete Fourier Transform of the signal buffer. The frequency bin with the
highest magnitude corresponds to the dominant frequency. This is ideal for identifying
the primary frequency component, even in signals with multiple harmonics.

```
frequency = bin_index * sampleRate / fftSize
period = 1 / frequency
```

### Implementation with fft.js

```javascript
// npm install fft.js
import FFT from 'fft.js';

/**
 * Detect dominant frequency using FFT.
 *
 * @param {Float32Array|number[]} samples - Input signal
 * @param {number} sampleRate - Samples per second
 * @param {object} [options]
 * @param {number} [options.minFreq] - Minimum expected frequency (Hz)
 * @param {number} [options.maxFreq] - Maximum expected frequency (Hz)
 * @param {boolean} [options.window] - Apply Hann window (default: true)
 * @returns {{ frequency: number, period: number, magnitude: number, spectrum: Float32Array }}
 */
function fftFrequencyDetect(samples, sampleRate, options = {}) {
  const n = samples.length;
  const fftSize = nextPowerOf2(n);
  const fft = new FFT(fftSize);

  // 1. Prepare input: remove DC, apply window, zero-pad
  const input = new Float32Array(fftSize);
  let sum = 0;
  for (let i = 0; i < n; i++) sum += samples[i];
  const mean = sum / n;

  const applyWindow = options.window !== false;

  for (let i = 0; i < n; i++) {
    let val = samples[i] - mean;
    if (applyWindow) {
      // Hann window: reduces spectral leakage
      val *= 0.5 * (1 - Math.cos(2 * Math.PI * i / (n - 1)));
    }
    input[i] = val;
  }
  // Remaining elements are already 0 (zero-padding)

  // 2. FFT
  const spectrum = fft.createComplexArray();
  fft.realTransform(spectrum, input);

  // 3. Compute magnitude spectrum
  const halfSize = fftSize / 2;
  const magnitudes = new Float32Array(halfSize);

  for (let i = 0; i < halfSize; i++) {
    const re = spectrum[2 * i];
    const im = spectrum[2 * i + 1];
    magnitudes[i] = Math.sqrt(re * re + im * im);
  }

  // 4. Find peak in valid frequency range
  const minBin = options.minFreq
    ? Math.max(1, Math.floor(options.minFreq * fftSize / sampleRate))
    : 1; // skip DC (bin 0)
  const maxBin = options.maxFreq
    ? Math.min(halfSize - 1, Math.ceil(options.maxFreq * fftSize / sampleRate))
    : halfSize - 1;

  let peakBin = minBin;
  let peakMag = 0;

  for (let i = minBin; i <= maxBin; i++) {
    if (magnitudes[i] > peakMag) {
      peakMag = magnitudes[i];
      peakBin = i;
    }
  }

  // 5. Parabolic interpolation for sub-bin accuracy
  let refinedBin = peakBin;
  if (peakBin > 0 && peakBin < halfSize - 1) {
    const y0 = magnitudes[peakBin - 1];
    const y1 = magnitudes[peakBin];
    const y2 = magnitudes[peakBin + 1];
    const denom = y0 - 2 * y1 + y2;
    if (denom !== 0) {
      refinedBin = peakBin - 0.5 * (y0 - y2) / denom;
    }
  }

  const frequency = refinedBin * sampleRate / fftSize;
  const period = frequency > 0 ? 1 / frequency : Infinity;

  // 6. Confidence: ratio of peak to mean magnitude (SNR proxy)
  let magSum = 0;
  for (let i = minBin; i <= maxBin; i++) magSum += magnitudes[i];
  const magMean = magSum / (maxBin - minBin + 1);
  const snr = magMean > 0 ? peakMag / magMean : 0;
  // Normalize: SNR of 10+ is high confidence
  const confidence = Math.min(1, snr / 10);

  return {
    frequency,
    period,
    periodSamples: period * sampleRate,
    magnitude: peakMag,
    confidence,
    magnitudes // full spectrum for visualization
  };
}
```

### Windowing Functions

Windowing is critical for FFT accuracy. Without it, spectral leakage causes the peak
to spread across multiple bins, reducing frequency resolution.

```javascript
/**
 * Common window functions for FFT preprocessing.
 * All take sample index i and total length N.
 */
const WindowFunctions = {
  hann: (i, N) => 0.5 * (1 - Math.cos(2 * Math.PI * i / (N - 1))),
  hamming: (i, N) => 0.54 - 0.46 * Math.cos(2 * Math.PI * i / (N - 1)),
  blackman: (i, N) => {
    const a0 = 0.42, a1 = 0.5, a2 = 0.08;
    return a0 - a1 * Math.cos(2 * Math.PI * i / (N - 1))
              + a2 * Math.cos(4 * Math.PI * i / (N - 1));
  },
  // Flat-top: best for amplitude accuracy (worst for frequency resolution)
  flatTop: (i, N) => {
    const a = [0.21557895, 0.41663158, 0.277263158, 0.083578947, 0.006947368];
    const w = 2 * Math.PI * i / (N - 1);
    return a[0] - a[1]*Math.cos(w) + a[2]*Math.cos(2*w) - a[3]*Math.cos(3*w) + a[4]*Math.cos(4*w);
  }
};

/**
 * Apply a window function to a signal buffer in-place.
 */
function applyWindow(samples, windowFn) {
  const N = samples.length;
  for (let i = 0; i < N; i++) {
    samples[i] *= windowFn(i, N);
  }
  return samples;
}
```

### Can Web Audio API's AnalyserNode Be Repurposed?

**Short answer: Yes, with caveats.**

The Web Audio API's `AnalyserNode` has a built-in FFT implementation optimized by the
browser (often using native SIMD instructions). It can be repurposed for non-audio
signals, but there are practical limitations:

```javascript
/**
 * Using AnalyserNode for FFT on arbitrary data.
 *
 * Approach: Create an AudioContext, feed data through a buffer source,
 * connect to an AnalyserNode, and read frequency data.
 */
async function analyserNodeFFT(samples, sampleRate) {
  const ctx = new OfflineAudioContext(1, samples.length, sampleRate);
  const buffer = ctx.createBuffer(1, samples.length, sampleRate);

  // Copy samples into audio buffer
  const channelData = buffer.getChannelData(0);
  for (let i = 0; i < samples.length; i++) {
    channelData[i] = samples[i];
  }

  const source = ctx.createBufferSource();
  source.buffer = buffer;

  const analyser = ctx.createAnalyser();
  analyser.fftSize = nextPowerOf2(samples.length);
  analyser.smoothingTimeConstant = 0; // no smoothing

  source.connect(analyser);
  analyser.connect(ctx.destination);
  source.start();

  await ctx.startRendering();

  const freqData = new Float32Array(analyser.frequencyBinCount);
  analyser.getFloatFrequencyData(freqData); // dB scale

  // Find peak
  let peakBin = 0;
  let peakVal = -Infinity;
  for (let i = 1; i < freqData.length; i++) {
    if (freqData[i] > peakVal) {
      peakVal = freqData[i];
      peakBin = i;
    }
  }

  const frequency = peakBin * sampleRate / analyser.fftSize;
  return { frequency, period: 1 / frequency, spectrumDB: freqData };
}
```

**Limitations of AnalyserNode approach:**

| Issue | Detail |
|-------|--------|
| Async only | `OfflineAudioContext.startRendering()` returns a Promise. Not synchronous. |
| Overhead | Creating audio contexts repeatedly is heavy. Not suited for 60fps. |
| Sample rate limits | AudioContext sample rates are typically 8000-96000 Hz. Your serial data may have different effective rates. |
| Fixed smoothing model | AnalyserNode applies its own smoothing and windowing that you cannot fully control. |
| fftSize constraints | Must be power of 2, between 32 and 32768. |

**Verdict**: For a real-time plotter, using `fft.js` directly is faster, synchronous,
and gives you full control. The AnalyserNode approach is better suited for actual audio
applications. **Use fft.js for fc_tool.**

### Characteristics

| Property | Value |
|----------|-------|
| **Time complexity** | O(n log n) |
| **Space complexity** | O(n) |
| **Noise tolerance** | Good. Windowing + averaging improves SNR. |
| **Real-time at 60fps** | Yes. 1024-point FFT takes ~0.1-0.3ms in JS. |
| **Best for** | Clean frequency identification, multi-frequency signals |
| **Worst for** | Very low frequencies (need long buffers for resolution) |

### Frequency Resolution

The frequency resolution of the FFT is:

```
delta_f = sampleRate / fftSize
```

For a 1000-sample buffer at 500 Hz sample rate:
- fftSize = 1024 (next power of 2)
- delta_f = 500 / 1024 = ~0.49 Hz

This means frequencies closer together than 0.49 Hz cannot be distinguished.
Parabolic interpolation improves this to roughly delta_f / 10.

---

## 4. Threshold Trigger (Oscilloscope-Style)

### How Real Oscilloscopes Implement Triggers

Oscilloscopes use hardware comparators and dedicated trigger circuits. The conceptual
model is:

1. **Trigger level**: A voltage threshold set by the user (or auto-detected).
2. **Trigger slope**: Rising edge, falling edge, or either.
3. **Holdoff**: Minimum time between valid trigger events (prevents re-triggering on noise or ringing).
4. **Pre-trigger**: Amount of data to show before the trigger point (typically 10-50% of screen width).
5. **Trigger modes**:
   - **Auto**: Triggers on threshold crossing. If no trigger within timeout, displays anyway (free-running).
   - **Normal**: Only updates display when trigger condition is met. Screen freezes if signal stops.
   - **Single**: Captures one trigger event, then stops. For one-shot signals.

### Software Implementation

```javascript
/**
 * Oscilloscope-style trigger system for Chart.js plotter.
 */
class TriggerSystem {
  /**
   * @param {object} config
   * @param {number} config.level - Trigger threshold value
   * @param {'rising'|'falling'|'either'} config.edge - Trigger edge
   * @param {'auto'|'normal'|'single'} config.mode - Trigger mode
   * @param {number} config.holdoff - Minimum samples between triggers
   * @param {number} config.preTrigger - Fraction of window to show before trigger (0-1)
   * @param {number} config.windowSize - Number of samples to display
   * @param {number} config.autoTimeout - Auto mode timeout (ms) before free-run
   */
  constructor(config = {}) {
    this.level = config.level ?? 0;
    this.edge = config.edge ?? 'rising';
    this.mode = config.mode ?? 'auto';
    this.holdoff = config.holdoff ?? 10;
    this.preTrigger = config.preTrigger ?? 0.1; // 10% pre-trigger
    this.windowSize = config.windowSize ?? 200;
    this.autoTimeout = config.autoTimeout ?? 500;

    // Internal state
    this._lastTriggerIndex = -Infinity;
    this._triggered = false;
    this._capturedWindow = null;
    this._lastTriggerTime = 0;
    this._singleArmed = true; // for single mode
  }

  /**
   * Process a data buffer and extract the triggered window.
   *
   * @param {Float32Array|number[]} buffer - Full circular buffer of recent samples
   * @returns {{ data: number[], triggerIndex: number, triggered: boolean } | null}
   */
  process(buffer) {
    const n = buffer.length;
    const preSamples = Math.floor(this.windowSize * this.preTrigger);
    const postSamples = this.windowSize - preSamples;

    // Single mode: if already triggered and not re-armed, return last capture
    if (this.mode === 'single' && !this._singleArmed && this._capturedWindow) {
      return this._capturedWindow;
    }

    // Search for trigger point (scan from most recent backward)
    let triggerIdx = -1;

    for (let i = n - postSamples; i >= preSamples + 1; i--) {
      // Check holdoff
      if (i - this._lastTriggerIndex < this.holdoff) continue;

      const prev = buffer[i - 1];
      const curr = buffer[i];
      const crossesLevel = this._checkEdge(prev, curr);

      if (crossesLevel) {
        triggerIdx = i;
        break;
      }
    }

    const now = performance.now();

    if (triggerIdx >= 0) {
      // Trigger found
      this._lastTriggerIndex = triggerIdx;
      this._lastTriggerTime = now;
      this._triggered = true;

      const start = triggerIdx - preSamples;
      const end = triggerIdx + postSamples;
      const window = buffer.slice(start, end);

      this._capturedWindow = {
        data: window,
        triggerIndex: preSamples, // trigger point within window
        triggered: true
      };

      if (this.mode === 'single') {
        this._singleArmed = false;
      }

      return this._capturedWindow;

    } else if (this.mode === 'auto') {
      // Auto mode: if no trigger within timeout, free-run
      if (now - this._lastTriggerTime > this.autoTimeout) {
        const start = Math.max(0, n - this.windowSize);
        return {
          data: buffer.slice(start, start + this.windowSize),
          triggerIndex: -1, // no trigger
          triggered: false
        };
      }
      // Within timeout: hold last triggered view
      return this._capturedWindow;

    } else if (this.mode === 'normal') {
      // Normal mode: hold last triggered view (do not update)
      return this._capturedWindow;
    }

    return null;
  }

  /**
   * Check if a crossing matches the configured edge type.
   */
  _checkEdge(prev, curr) {
    const level = this.level;

    switch (this.edge) {
      case 'rising':
        return prev < level && curr >= level;
      case 'falling':
        return prev >= level && curr < level;
      case 'either':
        return (prev < level && curr >= level) ||
               (prev >= level && curr < level);
      default:
        return false;
    }
  }

  /**
   * Auto-detect trigger level from data (set to mean value).
   */
  autoLevel(buffer) {
    let sum = 0;
    for (let i = 0; i < buffer.length; i++) sum += buffer[i];
    this.level = sum / buffer.length;
    return this.level;
  }

  /**
   * Auto-detect trigger level from data (set to midpoint between min and max).
   */
  autoLevelMidpoint(buffer) {
    let min = Infinity, max = -Infinity;
    for (let i = 0; i < buffer.length; i++) {
      if (buffer[i] < min) min = buffer[i];
      if (buffer[i] > max) max = buffer[i];
    }
    this.level = (min + max) / 2;
    return this.level;
  }

  /**
   * Re-arm single trigger mode.
   */
  arm() {
    this._singleArmed = true;
    this._capturedWindow = null;
  }

  /**
   * Update configuration.
   */
  setConfig(config) {
    Object.assign(this, config);
  }
}
```

### Integration with Chart.js

```javascript
// Setup
const trigger = new TriggerSystem({
  level: 512,       // Mid-range for 10-bit ADC
  edge: 'rising',
  mode: 'auto',
  holdoff: 20,
  preTrigger: 0.1,
  windowSize: 200
});

// In the data update loop (called when new serial data arrives):
function updatePlot(chart, fullBuffer) {
  const result = trigger.process(fullBuffer);

  if (result && result.data) {
    // Update chart with triggered window
    chart.data.labels = Array.from({ length: result.data.length }, (_, i) => i);
    chart.data.datasets[0].data = result.data;

    // Draw trigger level line using annotation plugin
    chart.options.plugins.annotation.annotations.triggerLine = {
      type: 'line',
      yMin: trigger.level,
      yMax: trigger.level,
      borderColor: 'rgba(255, 165, 0, 0.8)',
      borderWidth: 1,
      borderDash: [4, 4],
      label: {
        content: `Trigger: ${trigger.level.toFixed(1)}`,
        display: true,
        position: 'start'
      }
    };

    // Draw trigger point marker
    if (result.triggered && result.triggerIndex >= 0) {
      chart.options.plugins.annotation.annotations.triggerPoint = {
        type: 'point',
        xValue: result.triggerIndex,
        yValue: trigger.level,
        backgroundColor: 'rgba(255, 165, 0, 0.8)',
        radius: 5
      };
    }

    chart.update('none'); // no animation
  }
}
```

### UI Controls

```html
<!-- Trigger controls panel -->
<div class="trigger-controls">
  <label>Level:
    <input type="range" id="triggerLevel" min="0" max="1023" value="512">
    <span id="triggerLevelValue">512</span>
  </label>

  <label>Edge:
    <select id="triggerEdge">
      <option value="rising">Rising</option>
      <option value="falling">Falling</option>
      <option value="either">Either</option>
    </select>
  </label>

  <label>Mode:
    <select id="triggerMode">
      <option value="auto">Auto</option>
      <option value="normal">Normal</option>
      <option value="single">Single</option>
    </select>
  </label>

  <button id="triggerArm">Arm (Single)</button>
  <button id="triggerAutoLevel">Auto Level</button>
</div>
```

```javascript
// Wire up controls
document.getElementById('triggerLevel').addEventListener('input', (e) => {
  trigger.level = Number(e.target.value);
  document.getElementById('triggerLevelValue').textContent = e.target.value;
});

document.getElementById('triggerEdge').addEventListener('change', (e) => {
  trigger.edge = e.target.value;
});

document.getElementById('triggerMode').addEventListener('change', (e) => {
  trigger.mode = e.target.value;
});

document.getElementById('triggerArm').addEventListener('click', () => {
  trigger.arm();
});

document.getElementById('triggerAutoLevel').addEventListener('click', () => {
  const level = trigger.autoLevelMidpoint(fullBuffer);
  document.getElementById('triggerLevel').value = level;
  document.getElementById('triggerLevelValue').textContent = level.toFixed(1);
});
```

### Characteristics

| Property | Value |
|----------|-------|
| **Time complexity** | O(n) — single scan for trigger point |
| **Space complexity** | O(w) — where w = window size |
| **Noise tolerance** | Moderate. Holdoff prevents re-triggering on noise. Hysteresis improves it further. |
| **Real-time at 60fps** | Yes, trivially. |
| **Best for** | User-driven analysis, EE-familiar workflow |
| **Worst for** | Automatic period detection (requires manual level setting) |

---

## 5. Peak-to-Peak Detection

### Concept

Find local maxima (peaks) in the signal. The period is the average distance between
consecutive peaks. This works well for signals with distinct, well-separated peaks
(e.g., heartbeat, vibration pulses).

### Algorithm

1. Smooth the signal (optional, for noise reduction).
2. Find all local maxima (points higher than both neighbors).
3. Filter peaks by minimum prominence (height above surrounding valleys).
4. Compute average inter-peak interval.

### JavaScript Implementation

```javascript
/**
 * Detect period via peak detection.
 *
 * @param {Float32Array|number[]} samples - Input signal
 * @param {number} sampleRate - Samples per second
 * @param {object} [options]
 * @param {number} [options.minProminence] - Minimum peak height above neighbors (default: auto)
 * @param {number} [options.minDistance] - Minimum samples between peaks (default: 5)
 * @param {number} [options.smoothing] - Moving average window size (default: 0 = none)
 * @returns {{ period: number, frequency: number, peaks: number[], confidence: number }}
 */
function peakDetect(samples, sampleRate, options = {}) {
  let data = samples;
  const n = data.length;

  // 1. Optional smoothing (simple moving average)
  const smoothWindow = options.smoothing || 0;
  if (smoothWindow > 1) {
    data = movingAverage(data, smoothWindow);
  }

  // 2. Find all local maxima
  const minDistance = options.minDistance || 5;
  const candidates = [];

  for (let i = 1; i < n - 1; i++) {
    if (data[i] > data[i - 1] && data[i] > data[i + 1]) {
      candidates.push({ index: i, value: data[i] });
    }
  }

  if (candidates.length < 2) return null;

  // 3. Compute prominence for each candidate
  //    Prominence = height of peak above the higher of the two
  //    nearest valleys (one on each side)
  for (const peak of candidates) {
    // Find valley to the left
    let leftValley = peak.value;
    for (let j = peak.index - 1; j >= 0; j--) {
      if (data[j] < leftValley) leftValley = data[j];
      if (data[j] > peak.value) break; // hit a higher peak
    }

    // Find valley to the right
    let rightValley = peak.value;
    for (let j = peak.index + 1; j < n; j++) {
      if (data[j] < rightValley) rightValley = data[j];
      if (data[j] > peak.value) break; // hit a higher peak
    }

    peak.prominence = peak.value - Math.max(leftValley, rightValley);
  }

  // 4. Filter by prominence
  let minProminence = options.minProminence;
  if (minProminence === undefined) {
    // Auto: use 25% of the signal's peak-to-peak range
    let min = Infinity, max = -Infinity;
    for (let i = 0; i < n; i++) {
      if (data[i] < min) min = data[i];
      if (data[i] > max) max = data[i];
    }
    minProminence = (max - min) * 0.25;
  }

  let peaks = candidates.filter(p => p.prominence >= minProminence);

  // 5. Enforce minimum distance (keep taller peak in each group)
  peaks.sort((a, b) => a.index - b.index);
  const filtered = [peaks[0]];
  for (let i = 1; i < peaks.length; i++) {
    const last = filtered[filtered.length - 1];
    if (peaks[i].index - last.index < minDistance) {
      // Keep the taller one
      if (peaks[i].value > last.value) {
        filtered[filtered.length - 1] = peaks[i];
      }
    } else {
      filtered.push(peaks[i]);
    }
  }
  peaks = filtered;

  if (peaks.length < 2) return null;

  // 6. Compute average inter-peak interval
  const intervals = [];
  for (let i = 1; i < peaks.length; i++) {
    intervals.push(peaks[i].index - peaks[i - 1].index);
  }

  const avgInterval = intervals.reduce((a, b) => a + b, 0) / intervals.length;
  const periodSeconds = avgInterval / sampleRate;
  const frequency = 1 / periodSeconds;

  // 7. Confidence from interval consistency
  const variance = intervals.reduce((acc, val) =>
    acc + (val - avgInterval) ** 2, 0) / intervals.length;
  const stdDev = Math.sqrt(variance);
  const confidence = Math.max(0, 1 - stdDev / avgInterval);

  return {
    period: avgInterval,
    periodSeconds,
    frequency,
    peaks: peaks.map(p => p.index),
    peakValues: peaks.map(p => p.value),
    confidence
  };
}

/**
 * Simple moving average filter.
 */
function movingAverage(samples, windowSize) {
  const n = samples.length;
  const result = new Float32Array(n);
  const half = Math.floor(windowSize / 2);

  for (let i = 0; i < n; i++) {
    let sum = 0;
    let count = 0;
    for (let j = Math.max(0, i - half); j <= Math.min(n - 1, i + half); j++) {
      sum += samples[j];
      count++;
    }
    result[i] = sum / count;
  }

  return result;
}
```

### Advanced: Sub-Sample Peak Refinement

For higher accuracy, fit a parabola through the peak and its neighbors:

```javascript
/**
 * Refine peak position using parabolic interpolation.
 * Returns fractional sample index of true peak.
 */
function refinePeak(samples, peakIndex) {
  if (peakIndex <= 0 || peakIndex >= samples.length - 1) return peakIndex;

  const y0 = samples[peakIndex - 1];
  const y1 = samples[peakIndex];
  const y2 = samples[peakIndex + 1];

  const denom = y0 - 2 * y1 + y2;
  if (denom === 0) return peakIndex;

  return peakIndex - 0.5 * (y0 - y2) / denom;
}
```

### Characteristics

| Property | Value |
|----------|-------|
| **Time complexity** | O(n) for peak finding, O(n * k) for prominence (k = avg peak width) |
| **Space complexity** | O(k) — where k = number of candidates |
| **Noise tolerance** | Moderate. Prominence filtering helps. Pre-smoothing is often necessary. |
| **Real-time at 60fps** | Yes. Peak finding on 1000 samples is fast. |
| **Best for** | Pulsed signals, signals with clear peaks (heartbeat, vibration) |
| **Worst for** | Sine waves (broad peaks), very noisy signals |

---

## 6. JavaScript Signal Processing Libraries

### fft.js

**Status: Actively maintained. Recommended.**

- **npm**: `fft.js`
- **GitHub**: [nicedoc/fft.js](https://github.com/nicedoc/fft.js) (originally indutny/fft.js)
- **License**: MIT
- **Size**: ~4 KB minified
- **Last publish**: Stable (version 4.0.4)
- **Weekly downloads**: Moderate (~10k-50k range)
- **Power-of-2 only**: Yes, fftSize must be a power of 2

**Key features:**
- Pure JavaScript, no dependencies, no WebAssembly
- `realTransform()` for real-valued signals (2x faster than complex)
- Interleaved complex arrays (`[re0, im0, re1, im1, ...]`)
- Very fast for browser use (radix-4 Cooley-Tukey)

**Usage:**

```javascript
import FFT from 'fft.js';

const fft = new FFT(1024);
const input = new Float32Array(1024); // your samples
const output = fft.createComplexArray();

fft.realTransform(output, input);
fft.completeSpectrum(output); // fill negative frequencies

// Magnitude at bin i:
const re = output[2 * i];
const im = output[2 * i + 1];
const mag = Math.sqrt(re * re + im * im);
```

**Caveat**: The interleaved complex format is unusual. Most code examples for FFT use
separate real/imaginary arrays. Watch for indexing errors.

---

### dsp.js

**Status: Abandoned. Not recommended for new projects.**

- **npm**: `dsp.js`
- **GitHub**: [corbanbrook/dsp.js](https://github.com/corbanbrook/dsp.js)
- **License**: MIT
- **Last meaningful update**: ~2012-2013
- **Weekly downloads**: Very low
- **Issues**: 20+ open issues, no recent activity

**What it provided:**
- FFT, DFT, RFFT
- Windowing functions (Bartlett, Hann, Hamming, Blackman, etc.)
- Oscillators, filters (IIR, FIR, biquad)
- `DSP.FFT`, `DSP.Oscillator`, etc.

**Why not to use it:**
- Not maintained for 10+ years
- Does not use modern JS (no ES modules, no TypedArrays efficiently)
- Performance significantly worse than fft.js
- No npm publishing pipeline or CI/CD
- Known bugs unfixed

**Migration path**: Replace with `fft.js` for FFT and write your own windowing
functions (trivial, shown above) or use `fili` for filters.

---

### Alternatives and Other Libraries

#### ml-signal-processing / ml-savitzky-golay-generalized

- **npm**: `ml-savitzky-golay-generalized`
- **Part of**: mljs ecosystem
- **Use for**: Smoothing/filtering before peak detection
- **Status**: Maintained

```javascript
import SG from 'ml-savitzky-golay-generalized';
const smoothed = SG(rawData, 1, {
  windowSize: 9,
  derivative: 0,
  polynomial: 3
});
```

#### fili (digital filter library)

- **npm**: `fili`
- **License**: MIT
- **Use for**: IIR/FIR digital filters (lowpass, highpass, bandpass)
- **Status**: Maintained, modern JS

```javascript
import { CalcCascades, IirFilter } from 'fili';

const calc = new CalcCascades();
const coeffs = calc.lowpass({
  order: 4,
  characteristic: 'butterworth',
  Fs: 1000,     // sample rate
  Fc: 50,       // cutoff frequency
  gain: 0,
  preGain: false
});

const filter = new IirFilter(coeffs);
const filtered = rawSamples.map(s => filter.singleStep(s));
```

#### Fourier-transform (alternative FFT)

- **npm**: `fourier-transform`
- **License**: MIT
- **Use for**: Simple magnitude spectrum from real input
- **Status**: Maintained
- Returns magnitude array directly (simpler API than fft.js)

```javascript
import ft from 'fourier-transform';

const magnitudes = ft(samples); // input must be power-of-2 length
// magnitudes[i] corresponds to frequency i * sampleRate / samples.length
```

**Advantage**: Simpler API. Returns magnitudes directly.
**Disadvantage**: No inverse FFT. No phase information. Cannot be used for FFT-accelerated autocorrelation.

#### ndarray-fft

- **npm**: `ndarray-fft`
- **Use for**: FFT on ndarray data structures
- **Status**: Maintained within scijs ecosystem
- **Note**: Heavier dependency tree (ndarray ecosystem). Overkill for 1D signals.

#### ml-autocorrelation

**This package does not exist as a standalone npm module.** The `mljs` ecosystem does
not have a dedicated autocorrelation package. Autocorrelation should be implemented
directly using the FFT-accelerated method shown in Section 2, or computed naively for
small buffers.

There is `ml-levinson-durbin` which performs Levinson-Durbin recursion (related to
autocorrelation in the context of linear prediction), but it is not a general
autocorrelation function.

#### essentia.js

- **npm**: `essentia.js`
- **Use for**: Full-featured audio analysis (pitch detection, onset detection, etc.)
- **Status**: Actively maintained by MTG (Music Technology Group)
- **Size**: Large (~2-5 MB, WebAssembly-based)
- **Note**: Extremely powerful but heavy. Includes YIN pitch detection which is
  essentially autocorrelation-based period detection. Overkill for fc_tool's needs
  but worth knowing about.

#### pitchy

- **npm**: `pitchy`
- **License**: MIT
- **Use for**: Real-time pitch (frequency) detection
- **Status**: Maintained
- **Algorithm**: Autocorrelation-based (McLeod Pitch Method)
- **Size**: Small
- **Note**: Designed for audio but the algorithm works on any periodic signal.

```javascript
import { PitchDetector } from 'pitchy';

const detector = PitchDetector.forFloat32Array(bufferSize);
const [pitch, clarity] = detector.findPitch(samples, sampleRate);
// pitch = frequency in Hz
// clarity = 0.0 to 1.0 confidence
```

**This is worth investigating for fc_tool.** The McLeod Pitch Method is a refined
autocorrelation technique specifically designed to find the fundamental frequency
reliably, even in the presence of harmonics. The `clarity` output maps directly to
the confidence metric we need.

---

### Library Comparison

| Library | Size | FFT | Filters | Autocorrelation | Maintained | Recommended |
|---------|------|-----|---------|-----------------|------------|-------------|
| **fft.js** | 4 KB | Yes | No | Via FFT trick | Yes | **Yes** |
| **dsp.js** | 30 KB | Yes | Yes | No | No (2013) | No |
| **fourier-transform** | 2 KB | Magnitude only | No | No | Yes | For simple use |
| **fili** | 15 KB | No | Yes (IIR/FIR) | No | Yes | For filtering |
| **pitchy** | 8 KB | Internal | No | Yes (McLeod) | Yes | For pitch detection |
| **essentia.js** | 2-5 MB | Yes | Yes | Yes | Yes | Overkill |
| **ndarray-fft** | 10 KB+ | Yes | No | Via FFT | Yes | If using ndarray |

---

## 7. Comparison Table — All Detection Techniques

| Technique | Complexity | Noise Tolerance | Accuracy | 60fps / 1000 samples | Best Signal Type | Implementation Effort |
|-----------|-----------|-----------------|----------|----------------------|------------------|----------------------|
| **Zero-crossing** | O(n) | Poor (without hysteresis) / Moderate (with) | Moderate | Yes, trivially | Clean sine/square | Very low |
| **Autocorrelation (naive)** | O(n^2) | Excellent | High | Marginal (~1-2ms) | Noisy periodic | Low |
| **Autocorrelation (FFT)** | O(n log n) | Excellent | High | Yes (~0.2ms) | Noisy periodic | Medium (needs fft.js) |
| **FFT peak** | O(n log n) | Good | High (with window) | Yes (~0.2ms) | Multi-frequency | Medium (needs fft.js) |
| **Threshold trigger** | O(n) | Moderate (holdoff helps) | User-dependent | Yes, trivially | Any (manual control) | Medium (UI work) |
| **Peak-to-peak** | O(n) | Moderate (with smoothing) | Moderate-High | Yes | Pulsed, peaked | Low-Medium |

### Accuracy vs. Noise (Qualitative)

```
Accuracy
  ^
  |  FFT ----*------------ Autocorrelation (FFT)
  |         /           *
  |        /          *
  |  Peak /        * Autocorrelation (naive)
  |      /      *
  |     /    *
  |    / *  Zero-crossing (with hysteresis)
  |   *
  |  Zero-crossing (basic)
  +---------------------------------> Noise Level
        Clean              Noisy
```

### When to Use What

| Scenario | Recommended Method | Why |
|----------|-------------------|-----|
| PWM signal (square wave) | Zero-crossing or Trigger | Clean edges, fast |
| Vibration data (noisy) | Autocorrelation (FFT) | Noise-robust |
| IMU periodic motion | Autocorrelation or FFT | May have harmonics |
| User wants oscilloscope UX | Threshold trigger | Most intuitive |
| Unknown signal, auto-detect | FFT peak detection | General-purpose |
| Heartbeat/pulse signals | Peak-to-peak | Clear peaks |

---

## 8. Recommendations for fc_tool

### Recommended Implementation Strategy

Implement methods in this order of priority:

#### Phase 1: Threshold Trigger (First)

The trigger system gives users immediate, tangible value. It is the most intuitive
approach for EE users and requires no signal processing libraries.

- Implement `TriggerSystem` class (Section 4)
- Add trigger level slider, edge selector, mode selector to per-plot UI
- Add auto-level button (midpoint of min/max)
- Display trigger level as annotation line on Chart.js plot

#### Phase 2: Zero-Crossing Detection (Quick Win)

Add automatic period detection alongside the trigger system. Zero-crossing is trivial
to implement and gives a "detected frequency" readout.

- Implement `zeroCrossingDetect()` with hysteresis (Section 1)
- Display detected frequency/period in plot header
- Use detected period to auto-set trigger window size

#### Phase 3: FFT Frequency Detection (Accuracy Upgrade)

Add `fft.js` as a dependency. Use FFT for more accurate frequency detection and
optional spectrum visualization.

- `npm install fft.js`
- Implement `fftFrequencyDetect()` (Section 3)
- Use FFT result to refine trigger window sizing
- Optional: show mini spectrum plot below main plot

#### Phase 4: Autocorrelation (Noise Robustness)

For noisy signals where zero-crossing and FFT struggle, add FFT-accelerated
autocorrelation as the "heavy-duty" detector.

- Implement `autocorrelationFFT()` using fft.js (Section 2)
- Use as fallback when zero-crossing confidence is low

#### Phase 5: Peak Detection (Specialized)

Add peak detection for pulse-type signals. Lower priority since autocorrelation
handles most cases.

- Implement `peakDetect()` (Section 5)
- Useful for heartbeat, vibration pulse counting

### Architecture: Detection Pipeline

```javascript
/**
 * Unified signal analysis pipeline for fc_tool.
 * Runs all relevant detectors and picks the best result.
 */
class SignalAnalyzer {
  constructor(sampleRate) {
    this.sampleRate = sampleRate;
    this.lastResult = null;
  }

  /**
   * Analyze a buffer and return the best period estimate.
   * Tries multiple methods and returns the one with highest confidence.
   */
  analyze(samples) {
    const results = [];

    // Fast methods first
    const zcResult = zeroCrossingDetect(samples, this.sampleRate, {
      risingOnly: true,
      interpolate: true
    });
    if (zcResult) {
      results.push({ method: 'zero-crossing', ...zcResult });
    }

    // FFT if available
    const fftResult = fftFrequencyDetect(samples, this.sampleRate, {
      window: true
    });
    if (fftResult) {
      results.push({ method: 'fft', ...fftResult });
    }

    // Pick highest confidence
    results.sort((a, b) => b.confidence - a.confidence);
    this.lastResult = results[0] || null;
    return this.lastResult;
  }
}
```

### Web Worker Offloading

For multiple plots running FFT simultaneously, offload analysis to a Web Worker:

```javascript
// signal-worker.js
import FFT from 'fft.js';

self.onmessage = function(e) {
  const { id, samples, sampleRate, method } = e.data;

  let result;
  switch (method) {
    case 'fft':
      result = fftFrequencyDetect(samples, sampleRate);
      break;
    case 'autocorrelation':
      result = autocorrelationFFT(samples, sampleRate);
      break;
    case 'zero-crossing':
      result = zeroCrossingDetect(samples, sampleRate);
      break;
  }

  self.postMessage({ id, result });
};
```

```javascript
// Main thread
const worker = new Worker('signal-worker.js', { type: 'module' });

worker.onmessage = (e) => {
  const { id, result } = e.data;
  updatePlotWithAnalysis(id, result);
};

// Send analysis request (does not block UI)
worker.postMessage({
  id: plotId,
  samples: Float32Array.from(buffer),
  sampleRate: 1000,
  method: 'fft'
});
```

### Minimal Dependency Recommendation

For fc_tool, the minimal set of dependencies:

| Need | Library | Size |
|------|---------|------|
| FFT | `fft.js` | 4 KB |
| Filtering (optional) | `fili` | 15 KB |
| Everything else | Custom implementation (code in this document) | ~2 KB |

Total added JS: **~6-20 KB** — negligible for a Tauri application.

Do NOT use `dsp.js` (abandoned) or `essentia.js` (too heavy for this use case).

Consider `pitchy` as an alternative to writing your own autocorrelation. It implements
the McLeod Pitch Method which is specifically designed for robust fundamental frequency
detection, and it returns a clarity metric that maps to the confidence value needed
for the auto-detection pipeline. It adds ~8 KB.

---

## References

### Algorithms

- Smith, J.O., "Mathematics of the Discrete Fourier Transform (DFT) with Audio Applications", W3K Publishing (online book)
- de Cheveigne, A. and Kawahara, H., "YIN, a fundamental frequency estimator for speech and music", JASA 2002
- McLeod, P. and Wyvill, G., "A smarter way to find pitch", International Computer Music Conference, 2005 (basis for pitchy library)

### Libraries

- fft.js: https://github.com/nicedoc/fft.js (originally indutny/fft.js)
- dsp.js: https://github.com/corbanbrook/dsp.js (historical reference only)
- fili: https://github.com/markert/fili.js
- fourier-transform: https://github.com/scijs/fourier-transform
- pitchy: https://github.com/ianprime0509/pitchy
- essentia.js: https://mtg.github.io/essentia.js/
- ml-savitzky-golay-generalized: https://github.com/mljs/savitzky-golay-generalized

### Chart.js Integration

- chartjs-plugin-annotation: https://www.chartjs.org/chartjs-plugin-annotation/latest/
- Chart.js Performance Guide: https://www.chartjs.org/docs/latest/general/performance.html

---

*This research informs fc_tool's signal analysis and pattern detection implementation.
See also: [chartjs-oscilloscope-research.md](./chartjs-oscilloscope-research.md) and
[signal-analysis-discussion.md](../signal-analysis-discussion.md).*
