# Real-Time Anomaly Detection for Streaming Sensor Data

> Last updated: 2026-02-07

Research on practical anomaly detection techniques for fc_tool's enhanced plotter. These techniques detect signal changes (amplitude drift, frequency shift, waveform distortion, DC offset change) in real-time streaming IMU, PWM, and other sensor data at 10-100Hz.

---

## Table of Contents

1. [Context and Requirements](#context-and-requirements)
2. [Approach 1: Statistical Anomaly Detection (Rolling Stats + N-Sigma)](#approach-1-statistical-anomaly-detection)
3. [Approach 2: CUSUM (Cumulative Sum)](#approach-2-cusum)
4. [Approach 3: Moving Window Comparison](#approach-3-moving-window-comparison)
5. [Approach 4: EMA-Based Detection](#approach-4-ema-based-detection)
6. [Approach 5: Peak/Trough Tracking](#approach-5-peaktrough-tracking)
7. [Approach 6: Rise/Fall Time Monitoring](#approach-6-risefall-time-monitoring)
8. [Comparison Table](#comparison-table)
9. [Baseline Learning Phase](#baseline-learning-phase)
10. [Chart.js Anomaly Visualization](#chartjs-anomaly-visualization)
11. [Anomaly Trend Sparklines](#anomaly-trend-sparklines)
12. [Integration Architecture](#integration-architecture)
13. [Recommendations for fc_tool](#recommendations-for-fc_tool)

---

## Context and Requirements

### Operating Conditions

| Parameter | Value |
|-----------|-------|
| Data rate | 10-100 Hz (50 Hz typical) |
| Signal types | IMU accel/gyro, PWM, analog sensors |
| Runtime | Hours (must not leak memory) |
| Environment | Browser (Tauri webview), vanilla JS |
| Chart library | Chart.js (already integrated) |
| Buffer size | 100 samples currently (`MAX_DATA_POINTS`) |
| Update method | `chart.update('none')` (no animation) |

### What We Want to Detect

| Change Type | Example | Detection Speed Needed |
|-------------|---------|----------------------|
| Amplitude drift | Accel Z slowly drifting from 1.0g to 1.05g | Seconds |
| Frequency shift | Vibration period changing from 20ms to 22ms | Seconds |
| Waveform distortion | Sine becoming clipped or asymmetric | Sub-second |
| DC offset change | Gyro bias shifting by 0.5 deg/s | Seconds |
| Sudden spike | Single-sample transient | Immediate |
| Intermittent glitch | Occasional wrong readings | Seconds (pattern) |

---

## Approach 1: Statistical Anomaly Detection

### Concept

Track rolling mean, standard deviation, min, and max over a sliding window. Flag when new values deviate beyond N standard deviations from the rolling mean.

This is the most intuitive approach: "is the current value unusual given recent history?"

### Algorithm

1. Maintain a rolling window of the last W samples
2. Compute mean and standard deviation of the window
3. When a new sample arrives, check: `|sample - mean| > N * stddev`
4. If true, flag as anomaly

### JavaScript Implementation

```javascript
/**
 * Rolling statistics anomaly detector.
 * Uses Welford's online algorithm for numerically stable variance.
 * Fixed-size circular buffer — no memory growth.
 */
class RollingStatsDetector {
  constructor(windowSize = 200, sigmaThreshold = 3.0) {
    this.windowSize = windowSize;
    this.sigma = sigmaThreshold;

    // Circular buffer
    this.buffer = new Float64Array(windowSize);
    this.head = 0;
    this.count = 0;

    // Running statistics (Welford's algorithm)
    this.mean = 0;
    this.m2 = 0;       // Sum of squared differences from mean
    this.min = Infinity;
    this.max = -Infinity;

    // Per-window min/max tracking
    this.windowMin = Infinity;
    this.windowMax = -Infinity;
  }

  /**
   * Add a sample and check for anomaly.
   * @param {number} value - New sensor reading
   * @returns {{ isAnomaly: boolean, value: number, mean: number,
   *             stddev: number, deviation: number, min: number, max: number }}
   */
  addSample(value) {
    const isFull = this.count >= this.windowSize;

    if (isFull) {
      // Remove oldest value from running stats
      const oldValue = this.buffer[this.head];
      const oldMean = this.mean;
      this.mean += (value - oldValue) / this.windowSize;
      // Update M2: remove old contribution, add new
      this.m2 += (value - this.mean + value - oldMean) * (value - oldMean)
               - (oldValue - this.mean + oldValue - oldMean) * (oldValue - oldMean);
      // Clamp M2 to avoid floating point drift below zero
      if (this.m2 < 0) this.m2 = 0;
    } else {
      // Welford's online update
      this.count++;
      const delta = value - this.mean;
      this.mean += delta / this.count;
      const delta2 = value - this.mean;
      this.m2 += delta * delta2;
    }

    // Store in circular buffer
    this.buffer[this.head] = value;
    this.head = (this.head + 1) % this.windowSize;

    // Compute current statistics
    const variance = this.count > 1 ? this.m2 / (this.count - 1) : 0;
    const stddev = Math.sqrt(variance);

    // Update running min/max
    if (value < this.min) this.min = value;
    if (value > this.max) this.max = value;

    // Anomaly check: only after we have enough data
    const deviation = stddev > 0 ? Math.abs(value - this.mean) / stddev : 0;
    const isAnomaly = this.count >= 20 && deviation > this.sigma;

    return {
      isAnomaly,
      value,
      mean: this.mean,
      stddev,
      deviation,  // How many sigmas away
      min: this.min,
      max: this.max,
    };
  }

  /**
   * Recompute min/max from the full buffer.
   * Call periodically (e.g., every 1000 samples) to keep
   * window min/max accurate as old extremes age out.
   */
  recomputeMinMax() {
    let min = Infinity;
    let max = -Infinity;
    const n = Math.min(this.count, this.windowSize);
    for (let i = 0; i < n; i++) {
      if (this.buffer[i] < min) min = this.buffer[i];
      if (this.buffer[i] > max) max = this.buffer[i];
    }
    this.min = min;
    this.max = max;
  }

  /** Reset detector (e.g., when user changes signal source). */
  reset() {
    this.head = 0;
    this.count = 0;
    this.mean = 0;
    this.m2 = 0;
    this.min = Infinity;
    this.max = -Infinity;
  }
}
```

### Usage

```javascript
const detector = new RollingStatsDetector(200, 3.0);

// In your data processing loop:
function onNewSample(value) {
  const result = detector.addSample(value);
  if (result.isAnomaly) {
    console.log(`ANOMALY: value=${value.toFixed(3)}, `
      + `${result.deviation.toFixed(1)} sigma from mean`);
    markAnomalyOnChart(currentIndex, value);
  }
}
```

### Characteristics

| Property | Value |
|----------|-------|
| Memory | ~1.6 KB per detector (200 x Float64) |
| CPU per sample | O(1) — constant time |
| Detection latency | Immediate for spikes, slow for gradual drift |
| False positive rate | ~0.27% at 3-sigma, ~0.006% at 4-sigma (Gaussian assumption) |
| Minimum samples | ~20 before reliable detection |
| Best for | Sudden spikes, level shifts |
| Weakness | Gradual drift slowly adjusts the window, masking the change |

### Sigma Threshold Selection Guide

| Sigma | False positive rate (Gaussian) | Use case |
|-------|-------------------------------|----------|
| 2.0 | ~4.6% | Sensitive — catches small changes, noisy |
| 2.5 | ~1.2% | Moderate sensitivity |
| 3.0 | ~0.27% | Standard choice — good balance |
| 3.5 | ~0.047% | Conservative |
| 4.0 | ~0.006% | Very conservative — only large deviations |

For noisy IMU data, 3.0 sigma is a good starting point. For clean PWM signals, 2.5 sigma works well.

---

## Approach 2: CUSUM

### Concept

CUSUM (Cumulative Sum) detects small, sustained shifts in the mean value of a signal. Unlike the rolling stats approach which responds to individual outliers, CUSUM accumulates evidence of a persistent change. It is specifically designed for drift detection.

The idea: if the mean has shifted by some amount delta, individual samples will be slightly biased. No single sample is alarming, but the cumulative sum of these small biases grows steadily, eventually crossing a threshold.

### Algorithm

CUSUM maintains two accumulators:
- **S_high**: Detects upward shifts — accumulates positive deviations from the target mean
- **S_low**: Detects downward shifts — accumulates negative deviations from the target mean

On each sample:
1. `S_high = max(0, S_high + (x - target) - allowance)`
2. `S_low = max(0, S_low - (x - target) - allowance)`
3. If `S_high > threshold` or `S_low > threshold`, signal a change

Parameters:
- **target**: Expected mean (from baseline learning phase)
- **allowance** (k): Minimum shift size to detect (filters noise). Typically `0.5 * delta` where delta is the smallest shift you care about.
- **threshold** (h): Decision threshold. Larger = fewer false alarms but slower detection. Typically `4 * sigma` to `5 * sigma`.

### JavaScript Implementation

```javascript
/**
 * CUSUM (Cumulative Sum) change detector.
 * Detects sustained shifts in mean value — ideal for drift detection.
 * Constant memory, O(1) per sample.
 */
class CUSUMDetector {
  /**
   * @param {number} target - Expected mean value (from baseline)
   * @param {number} allowance - Slack parameter (k). Shift size / 2.
   *                             Smaller = more sensitive to small shifts.
   * @param {number} threshold - Decision threshold (h). Larger = fewer
   *                             false alarms but slower detection.
   */
  constructor(target = 0, allowance = 0.5, threshold = 4.0) {
    this.target = target;
    this.allowance = allowance;
    this.threshold = threshold;

    this.sHigh = 0;   // Cumulative sum for upward shifts
    this.sLow = 0;    // Cumulative sum for downward shifts
    this.sampleCount = 0;

    // Track when the last reset happened (for alarm duration)
    this.lastResetHigh = 0;
    this.lastResetLow = 0;
  }

  /**
   * Process a new sample.
   * @param {number} value
   * @returns {{ alarm: boolean, direction: 'none'|'up'|'down'|'both',
   *             sHigh: number, sLow: number, shift: number }}
   */
  addSample(value) {
    this.sampleCount++;

    const deviation = value - this.target;

    // Update cumulative sums
    this.sHigh = Math.max(0, this.sHigh + deviation - this.allowance);
    this.sLow = Math.max(0, this.sLow - deviation - this.allowance);

    // Track resets for age calculation
    if (this.sHigh === 0) this.lastResetHigh = this.sampleCount;
    if (this.sLow === 0) this.lastResetLow = this.sampleCount;

    // Check thresholds
    const alarmHigh = this.sHigh > this.threshold;
    const alarmLow = this.sLow > this.threshold;

    let direction = 'none';
    if (alarmHigh && alarmLow) direction = 'both';
    else if (alarmHigh) direction = 'up';
    else if (alarmLow) direction = 'down';

    return {
      alarm: alarmHigh || alarmLow,
      direction,
      sHigh: this.sHigh,
      sLow: this.sLow,
      shift: deviation,
      // How many samples since CUSUM started accumulating
      ageHigh: this.sampleCount - this.lastResetHigh,
      ageLow: this.sampleCount - this.lastResetLow,
    };
  }

  /**
   * Reset accumulators after acknowledging an alarm.
   * Without reset, CUSUM stays in alarm state permanently.
   */
  resetAccumulators() {
    this.sHigh = 0;
    this.sLow = 0;
    this.lastResetHigh = this.sampleCount;
    this.lastResetLow = this.sampleCount;
  }

  /**
   * Update the target mean (e.g., after a baseline learning phase
   * or when the user acknowledges a new operating point).
   */
  setTarget(newTarget) {
    this.target = newTarget;
    this.resetAccumulators();
  }

  /**
   * Auto-calibrate from a baseline data array.
   * Sets target to the mean and allowance based on standard deviation.
   * @param {number[]} baseline - Array of baseline samples
   * @param {number} allowanceFactor - Multiplier on stddev (default 0.5)
   * @param {number} thresholdFactor - Multiplier on stddev (default 4.0)
   */
  calibrateFromBaseline(baseline, allowanceFactor = 0.5, thresholdFactor = 4.0) {
    const n = baseline.length;
    if (n < 2) return;

    const mean = baseline.reduce((a, b) => a + b, 0) / n;
    const variance = baseline.reduce((sum, x) => sum + (x - mean) ** 2, 0) / (n - 1);
    const stddev = Math.sqrt(variance);

    this.target = mean;
    this.allowance = allowanceFactor * stddev;
    this.threshold = thresholdFactor * stddev;
    this.resetAccumulators();
  }
}
```

### Usage

```javascript
// Option 1: Manual configuration
const cusum = new CUSUMDetector(
  1.0,   // target: expected accel-Z is ~1.0g
  0.02,  // allowance: ignore shifts smaller than 0.02g
  0.2    // threshold: alarm after cumulative evidence reaches 0.2
);

// Option 2: Auto-calibrate from baseline
const cusum = new CUSUMDetector();
// After collecting baseline samples:
cusum.calibrateFromBaseline(baselineSamples);

// Process incoming data
function onNewSample(value) {
  const result = cusum.addSample(value);
  if (result.alarm) {
    console.log(`DRIFT DETECTED: ${result.direction} shift, `
      + `accumulated over ${result.ageHigh || result.ageLow} samples`);
    // Optionally reset after handling:
    // cusum.resetAccumulators();
  }
}
```

### Characteristics

| Property | Value |
|----------|-------|
| Memory | 48 bytes (6 numbers) |
| CPU per sample | O(1) — two additions, two comparisons |
| Detection latency | Proportional to shift size: large shifts detected in few samples, small shifts take longer |
| False positive rate | Very low when properly tuned (depends on h and k) |
| Best for | Gradual drift, DC offset changes, bias shifts |
| Weakness | Requires known target mean (needs baseline); does not detect shape changes |

### Tuning Guidelines for Sensor Data

| Signal Type | Target | Allowance (k) | Threshold (h) |
|-------------|--------|---------------|---------------|
| Accel Z (stationary) | 1.0g | 0.02g (0.5 * stddev) | 0.2g (4 * stddev) |
| Gyro (stationary) | 0.0 deg/s | 0.25 deg/s | 2.0 deg/s |
| PWM duty cycle | 50% | 0.5% | 5% |
| Temperature | varies | 0.1 C | 1.0 C |

**Key insight**: The ratio h/k determines the average run length (ARL) before a false alarm. Higher h/k = fewer false alarms but slower detection. A ratio of 8-10 is a good starting point.

---

## Approach 3: Moving Window Comparison

### Concept

Compare the shape of the current signal period against a stored baseline period. This detects waveform distortion that the statistical methods miss: clipping, ringing, asymmetry changes, harmonic distortion.

The approach:
1. During baseline learning, store one or more "reference periods" of the signal
2. As new periods arrive, compute a similarity metric between the new period and the reference
3. If similarity drops below a threshold, flag waveform distortion

### Similarity Metrics

| Metric | Formula | Sensitivity | Cost |
|--------|---------|-------------|------|
| Correlation coefficient | Pearson r between reference and current | Shape changes | O(n) |
| Normalized RMS error | sqrt(mean((ref - cur)^2)) / range | Amplitude + shape | O(n) |
| Mean Absolute Error | mean(abs(ref - cur)) | All changes | O(n) |
| Dynamic Time Warping (DTW) | Elastic distance measure | Shape + timing | O(n*m) |

For real-time use at 100Hz, correlation and NRMSE are the best choices. DTW is too expensive.

### JavaScript Implementation

```javascript
/**
 * Moving Window Waveform Comparator.
 * Compares current signal period against a learned baseline shape.
 * Detects distortion, clipping, harmonic changes.
 */
class WaveformComparator {
  /**
   * @param {number} periodLength - Expected samples per period
   * @param {number} correlationThreshold - Below this = anomaly (0.0-1.0)
   * @param {number} nrmseThreshold - Above this = anomaly (0.0-1.0)
   */
  constructor(periodLength = 100, correlationThreshold = 0.95, nrmseThreshold = 0.15) {
    this.periodLength = periodLength;
    this.corrThreshold = correlationThreshold;
    this.nrmseThreshold = nrmseThreshold;

    // Reference waveform (learned from baseline)
    this.reference = null;
    this.refMean = 0;
    this.refStddev = 0;

    // Current period accumulator
    this.currentPeriod = new Float64Array(periodLength);
    this.currentIndex = 0;
    this.periodsCompleted = 0;

    // Baseline learning
    this.baselinePeriods = [];
    this.isLearning = false;
    this.learnCount = 0;
    this.learnTarget = 5;  // Number of periods to average
  }

  /**
   * Start baseline learning phase.
   * @param {number} numPeriods - How many periods to average
   */
  startLearning(numPeriods = 5) {
    this.isLearning = true;
    this.learnTarget = numPeriods;
    this.learnCount = 0;
    this.baselinePeriods = [];
    this.currentIndex = 0;
  }

  /**
   * Add a sample. Returns comparison result when a full period is completed.
   * @param {number} value
   * @returns {null | { anomaly: boolean, correlation: number,
   *           nrmse: number, periodIndex: number }}
   */
  addSample(value) {
    this.currentPeriod[this.currentIndex] = value;
    this.currentIndex++;

    // Period not yet complete
    if (this.currentIndex < this.periodLength) {
      return null;
    }

    // Period complete — process it
    this.currentIndex = 0;
    this.periodsCompleted++;

    if (this.isLearning) {
      return this._learnPeriod();
    }

    if (!this.reference) {
      return null; // No reference yet
    }

    return this._comparePeriod();
  }

  /** @private */
  _learnPeriod() {
    // Store a copy of this period
    this.baselinePeriods.push(new Float64Array(this.currentPeriod));
    this.learnCount++;

    if (this.learnCount >= this.learnTarget) {
      // Average the learned periods into a reference
      this.reference = new Float64Array(this.periodLength);
      for (let i = 0; i < this.periodLength; i++) {
        let sum = 0;
        for (const period of this.baselinePeriods) {
          sum += period[i];
        }
        this.reference[i] = sum / this.baselinePeriods.length;
      }

      // Compute reference statistics
      const stats = this._computeStats(this.reference);
      this.refMean = stats.mean;
      this.refStddev = stats.stddev;

      this.isLearning = false;
      this.baselinePeriods = []; // Free memory

      return { learned: true, periodsUsed: this.learnCount };
    }

    return { learning: true, progress: this.learnCount / this.learnTarget };
  }

  /** @private */
  _comparePeriod() {
    const current = this.currentPeriod;
    const ref = this.reference;
    const n = this.periodLength;

    // Compute Pearson correlation coefficient
    const curStats = this._computeStats(current);
    let crossSum = 0;
    for (let i = 0; i < n; i++) {
      crossSum += (ref[i] - this.refMean) * (current[i] - curStats.mean);
    }
    const correlation = (this.refStddev > 0 && curStats.stddev > 0)
      ? crossSum / (n * this.refStddev * curStats.stddev)
      : 1.0;

    // Compute Normalized RMS Error
    let squaredErrorSum = 0;
    for (let i = 0; i < n; i++) {
      const diff = ref[i] - current[i];
      squaredErrorSum += diff * diff;
    }
    const rmse = Math.sqrt(squaredErrorSum / n);
    const range = Math.max(curStats.max - curStats.min, 1e-10);
    const nrmse = rmse / range;

    const anomaly = correlation < this.corrThreshold || nrmse > this.nrmseThreshold;

    return {
      anomaly,
      correlation,
      nrmse,
      periodIndex: this.periodsCompleted,
      amplitudeChange: (curStats.max - curStats.min) /
        (this._computeStats(this.reference).max - this._computeStats(this.reference).min + 1e-10) - 1.0,
      meanShift: curStats.mean - this.refMean,
    };
  }

  /** @private */
  _computeStats(arr) {
    let sum = 0, min = Infinity, max = -Infinity;
    const n = arr.length;
    for (let i = 0; i < n; i++) {
      sum += arr[i];
      if (arr[i] < min) min = arr[i];
      if (arr[i] > max) max = arr[i];
    }
    const mean = sum / n;
    let varianceSum = 0;
    for (let i = 0; i < n; i++) {
      varianceSum += (arr[i] - mean) ** 2;
    }
    return { mean, stddev: Math.sqrt(varianceSum / n), min, max };
  }

  /**
   * Update period length (e.g., when auto-detected period changes).
   */
  setPeriodLength(newLength) {
    this.periodLength = newLength;
    this.currentPeriod = new Float64Array(newLength);
    this.currentIndex = 0;
    // Reference is invalidated — need to re-learn
    this.reference = null;
  }

  /** Update reference from external source (e.g., user captures a period). */
  setReference(waveform) {
    this.reference = new Float64Array(waveform);
    this.periodLength = waveform.length;
    const stats = this._computeStats(this.reference);
    this.refMean = stats.mean;
    this.refStddev = stats.stddev;
  }
}
```

### Usage

```javascript
// Assume period detection has found period = 50 samples
const comparator = new WaveformComparator(50, 0.95, 0.15);

// Phase 1: Learn the baseline waveform (first 5 periods)
comparator.startLearning(5);

// Phase 2: Continuous monitoring (after learning completes)
function onNewSample(value) {
  const result = comparator.addSample(value);
  if (result === null) return; // Mid-period, nothing to report

  if (result.learned) {
    console.log('Baseline learned from', result.periodsUsed, 'periods');
  } else if (result.learning) {
    console.log('Learning:', (result.progress * 100).toFixed(0) + '%');
  } else if (result.anomaly) {
    console.log(`WAVEFORM DISTORTION: r=${result.correlation.toFixed(3)}, `
      + `NRMSE=${result.nrmse.toFixed(3)}`);
  }
}
```

### Characteristics

| Property | Value |
|----------|-------|
| Memory | ~2.4 KB per detector (3 x Float64Array of period length) |
| CPU per sample | O(1) accumulate, O(n) per period completion |
| Detection latency | One full period (at 50Hz with period=50, that is 1 second) |
| False positive rate | Low when correlation threshold is 0.90-0.95 |
| Best for | Waveform distortion, clipping, harmonic changes |
| Weakness | Requires known period length; not useful for aperiodic signals |

### Correlation Threshold Guide

| Threshold | Sensitivity | Use Case |
|-----------|-------------|----------|
| 0.99 | Very high — catches tiny shape changes | Clean signals, precision monitoring |
| 0.95 | High — good general-purpose | Standard sensor monitoring |
| 0.90 | Moderate — tolerates noise | Noisy IMU data |
| 0.80 | Low — only catches major distortion | Very noisy environments |

---

## Approach 4: EMA-Based Detection

### Concept

Exponential Moving Average (EMA) is the lightest approach. It tracks a smoothed version of the signal and flags when the raw value deviates too far from the EMA. Two EMAs with different time constants (fast and slow) can also detect when a signal is trending away from its usual behavior.

Key advantage: O(1) memory, O(1) computation, no buffers needed. This is the approach to use when you need anomaly detection on many channels simultaneously (e.g., 6 IMU axes + 4 PWM channels).

### Algorithm

**Single EMA with deviation check:**
1. `ema = alpha * value + (1 - alpha) * ema`
2. `deviation = |value - ema|`
3. Also track EMA of deviation: `ema_dev = alpha * deviation + (1 - alpha) * ema_dev`
4. Anomaly if `deviation > N * ema_dev`

**Dual EMA (fast/slow crossover):**
1. `ema_fast = alpha_fast * value + (1 - alpha_fast) * ema_fast`
2. `ema_slow = alpha_slow * value + (1 - alpha_slow) * ema_slow`
3. Anomaly if `|ema_fast - ema_slow| > threshold`

### JavaScript Implementation

```javascript
/**
 * EMA-based anomaly detector.
 * Extremely lightweight: 6 numbers of state per detector.
 * Suitable for running on every channel simultaneously.
 */
class EMADetector {
  /**
   * @param {number} alpha - Smoothing factor (0-1). Higher = more responsive.
   *                         Rule of thumb: alpha = 2 / (N + 1) where N is
   *                         the equivalent window size.
   * @param {number} deviationMultiplier - How many smoothed-deviations
   *                                       to trigger anomaly (like N-sigma).
   */
  constructor(alpha = 0.05, deviationMultiplier = 3.0) {
    this.alpha = alpha;
    this.multiplier = deviationMultiplier;

    this.ema = 0;           // Smoothed signal value
    this.emaDev = 0;        // Smoothed absolute deviation
    this.initialized = false;
    this.sampleCount = 0;
    this.warmupSamples = Math.ceil(2 / alpha); // ~2 time constants
  }

  /**
   * @param {number} value
   * @returns {{ isAnomaly: boolean, value: number, ema: number,
   *             deviation: number, threshold: number }}
   */
  addSample(value) {
    this.sampleCount++;

    if (!this.initialized) {
      this.ema = value;
      this.emaDev = 0;
      this.initialized = true;
      return { isAnomaly: false, value, ema: value, deviation: 0, threshold: 0 };
    }

    // Compute deviation before updating EMA
    const deviation = Math.abs(value - this.ema);

    // Update EMAs
    this.ema = this.alpha * value + (1 - this.alpha) * this.ema;
    this.emaDev = this.alpha * deviation + (1 - this.alpha) * this.emaDev;

    // Threshold (minimum floor prevents false alarms on constant signals)
    const threshold = Math.max(this.multiplier * this.emaDev, 1e-10);

    // Only flag after warmup period
    const isAnomaly = this.sampleCount > this.warmupSamples
                      && deviation > threshold;

    return {
      isAnomaly,
      value,
      ema: this.ema,
      deviation,
      threshold,
    };
  }

  /** Reset state. */
  reset() {
    this.ema = 0;
    this.emaDev = 0;
    this.initialized = false;
    this.sampleCount = 0;
  }
}

/**
 * Dual-EMA drift detector.
 * Uses fast and slow EMAs to detect when the signal is trending
 * away from its long-term average.
 */
class DualEMADetector {
  /**
   * @param {number} alphaFast - Fast EMA smoothing (e.g., 0.1 = ~20 samples)
   * @param {number} alphaSlow - Slow EMA smoothing (e.g., 0.01 = ~200 samples)
   * @param {number} divergenceThreshold - Max allowed fast-slow difference
   */
  constructor(alphaFast = 0.1, alphaSlow = 0.01, divergenceThreshold = 0.5) {
    this.alphaFast = alphaFast;
    this.alphaSlow = alphaSlow;
    this.threshold = divergenceThreshold;

    this.emaFast = 0;
    this.emaSlow = 0;
    this.initialized = false;
    this.sampleCount = 0;
    this.warmup = Math.ceil(2 / alphaSlow);
  }

  addSample(value) {
    this.sampleCount++;

    if (!this.initialized) {
      this.emaFast = value;
      this.emaSlow = value;
      this.initialized = true;
      return { isAnomaly: false, divergence: 0, emaFast: value, emaSlow: value };
    }

    this.emaFast = this.alphaFast * value + (1 - this.alphaFast) * this.emaFast;
    this.emaSlow = this.alphaSlow * value + (1 - this.alphaSlow) * this.emaSlow;

    const divergence = this.emaFast - this.emaSlow;
    const isAnomaly = this.sampleCount > this.warmup
                      && Math.abs(divergence) > this.threshold;

    return {
      isAnomaly,
      divergence,
      direction: divergence > 0 ? 'up' : 'down',
      emaFast: this.emaFast,
      emaSlow: this.emaSlow,
    };
  }

  reset() {
    this.emaFast = 0;
    this.emaSlow = 0;
    this.initialized = false;
    this.sampleCount = 0;
  }
}
```

### Usage

```javascript
// Lightweight: one detector per signal channel
const detectors = {
  ax: new EMADetector(0.05, 3.0),
  ay: new EMADetector(0.05, 3.0),
  az: new EMADetector(0.05, 3.0),
  gx: new EMADetector(0.05, 3.0),
  gy: new EMADetector(0.05, 3.0),
  gz: new EMADetector(0.05, 3.0),
};

// Process IMU data — negligible overhead
function onImuData(imuData) {
  const results = {
    ax: detectors.ax.addSample(imuData.accel.x),
    ay: detectors.ay.addSample(imuData.accel.y),
    az: detectors.az.addSample(imuData.accel.z),
    gx: detectors.gx.addSample(imuData.gyro.x),
    gy: detectors.gy.addSample(imuData.gyro.y),
    gz: detectors.gz.addSample(imuData.gyro.z),
  };

  for (const [channel, result] of Object.entries(results)) {
    if (result.isAnomaly) {
      highlightChannel(channel, result);
    }
  }
}
```

### Alpha Selection Guide

| Alpha | Equivalent window | Response time | Use case |
|-------|------------------|---------------|----------|
| 0.20 | ~10 samples | Very fast | Spike detection, fast signals |
| 0.10 | ~20 samples | Fast | General purpose |
| 0.05 | ~40 samples | Medium | Standard sensor monitoring |
| 0.02 | ~100 samples | Slow | Long-term trend tracking |
| 0.01 | ~200 samples | Very slow | Drift detection |

Formula: equivalent window N = (2/alpha) - 1

### Characteristics

| Property | Value |
|----------|-------|
| Memory | 48 bytes per detector (6 numbers) |
| CPU per sample | O(1) — two multiplications, two additions |
| Detection latency | Fast for spikes; ~2/alpha samples for warmup |
| False positive rate | Medium; adapts to noise level automatically |
| Best for | Lightweight monitoring of many channels; spike detection |
| Weakness | Cannot detect waveform shape changes; adapts to gradual drift |

---

## Approach 5: Peak/Trough Tracking

### Concept

Track the evolution of signal peaks (maxima) and troughs (minima) across periods. This directly detects amplitude drift, asymmetric changes, and DC offset shifts by monitoring how the extreme values change over time.

This is particularly useful for oscilloscope-style viewing: the user sees a repeating waveform, and peak/trough tracking tells them if the envelope is changing.

### Algorithm

1. Detect local maxima and minima using a simple three-point comparison
2. Record peak and trough values per period (or per rolling window)
3. Track EMA of peak values and trough values separately
4. Alert when peak-EMA or trough-EMA changes by more than a threshold

### JavaScript Implementation

```javascript
/**
 * Peak/Trough tracker with trend detection.
 * Monitors how signal extremes evolve over time.
 * Detects amplitude drift, asymmetry, and DC offset changes.
 */
class PeakTroughTracker {
  /**
   * @param {number} windowSize - Samples per analysis window (one period)
   * @param {number} historySize - How many peak/trough values to remember
   * @param {number} changeThreshold - Fractional change to trigger alert (0.05 = 5%)
   */
  constructor(windowSize = 100, historySize = 50, changeThreshold = 0.05) {
    this.windowSize = windowSize;
    this.historySize = historySize;
    this.changeThreshold = changeThreshold;

    // Current window accumulator
    this.windowBuffer = new Float64Array(windowSize);
    this.windowIndex = 0;

    // History of per-period statistics
    this.peakHistory = new Float64Array(historySize);
    this.troughHistory = new Float64Array(historySize);
    this.meanHistory = new Float64Array(historySize);
    this.historyHead = 0;
    this.historyCount = 0;

    // Baseline values (from first few periods)
    this.baselinePeak = null;
    this.baselineTrough = null;
    this.baselineMean = null;
    this.baselinePeriods = 5;
    this.periodsProcessed = 0;
  }

  /**
   * Add a sample. Returns analysis when a window completes.
   * @param {number} value
   * @returns {null | PeakTroughResult}
   */
  addSample(value) {
    this.windowBuffer[this.windowIndex] = value;
    this.windowIndex++;

    if (this.windowIndex < this.windowSize) {
      return null; // Window not yet full
    }

    // Window complete — analyze
    this.windowIndex = 0;
    return this._analyzeWindow();
  }

  /** @private */
  _analyzeWindow() {
    const buf = this.windowBuffer;
    const n = this.windowSize;

    // Find peak, trough, and mean
    let peak = -Infinity, trough = Infinity, sum = 0;
    let peakIndex = 0, troughIndex = 0;

    for (let i = 0; i < n; i++) {
      sum += buf[i];
      if (buf[i] > peak) { peak = buf[i]; peakIndex = i; }
      if (buf[i] < trough) { trough = buf[i]; troughIndex = i; }
    }
    const mean = sum / n;
    const amplitude = peak - trough;

    // Count local extrema (for waveform complexity tracking)
    let localMaxCount = 0, localMinCount = 0;
    for (let i = 1; i < n - 1; i++) {
      if (buf[i] > buf[i-1] && buf[i] > buf[i+1]) localMaxCount++;
      if (buf[i] < buf[i-1] && buf[i] < buf[i+1]) localMinCount++;
    }

    // Store in history
    this.peakHistory[this.historyHead] = peak;
    this.troughHistory[this.historyHead] = trough;
    this.meanHistory[this.historyHead] = mean;
    this.historyHead = (this.historyHead + 1) % this.historySize;
    if (this.historyCount < this.historySize) this.historyCount++;

    this.periodsProcessed++;

    // Establish baseline from first N periods
    if (this.periodsProcessed <= this.baselinePeriods) {
      if (this.periodsProcessed === this.baselinePeriods) {
        this.baselinePeak = this._historyMean(this.peakHistory);
        this.baselineTrough = this._historyMean(this.troughHistory);
        this.baselineMean = this._historyMean(this.meanHistory);
      }
      return {
        learning: true,
        progress: this.periodsProcessed / this.baselinePeriods,
        peak, trough, mean, amplitude,
      };
    }

    // Compute changes relative to baseline
    const baselineAmp = this.baselinePeak - this.baselineTrough;
    const peakChange = baselineAmp > 0 ? (peak - this.baselinePeak) / baselineAmp : 0;
    const troughChange = baselineAmp > 0 ? (trough - this.baselineTrough) / baselineAmp : 0;
    const meanChange = baselineAmp > 0 ? (mean - this.baselineMean) / baselineAmp : 0;
    const ampChange = baselineAmp > 0 ? (amplitude - baselineAmp) / baselineAmp : 0;

    // Compute trend (are peaks/troughs drifting steadily?)
    const peakTrend = this._computeTrend(this.peakHistory);
    const troughTrend = this._computeTrend(this.troughHistory);

    // Anomaly detection
    const anomalies = [];
    if (Math.abs(peakChange) > this.changeThreshold) {
      anomalies.push({ type: 'peak_shift', change: peakChange });
    }
    if (Math.abs(troughChange) > this.changeThreshold) {
      anomalies.push({ type: 'trough_shift', change: troughChange });
    }
    if (Math.abs(meanChange) > this.changeThreshold) {
      anomalies.push({ type: 'dc_offset', change: meanChange });
    }
    if (Math.abs(ampChange) > this.changeThreshold) {
      anomalies.push({ type: 'amplitude_change', change: ampChange });
    }
    if (localMaxCount !== 1 || localMinCount !== 1) {
      // Multiple peaks/troughs per period may indicate distortion
      anomalies.push({ type: 'complexity_change', maxCount: localMaxCount, minCount: localMinCount });
    }

    return {
      learning: false,
      anomaly: anomalies.length > 0,
      anomalies,
      peak, trough, mean, amplitude,
      peakChange, troughChange, meanChange, ampChange,
      peakTrend, troughTrend,
      localMaxCount, localMinCount,
      periodIndex: this.periodsProcessed,
    };
  }

  /** @private Compute mean of history buffer. */
  _historyMean(arr) {
    let sum = 0;
    const n = Math.min(this.historyCount, this.historySize);
    for (let i = 0; i < n; i++) sum += arr[i];
    return n > 0 ? sum / n : 0;
  }

  /** @private Compute simple linear trend (slope) of recent history. */
  _computeTrend(arr) {
    const n = Math.min(this.historyCount, this.historySize);
    if (n < 3) return 0;

    // Simple linear regression: slope = correlation * (sy / sx)
    let sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    for (let i = 0; i < n; i++) {
      // Read in chronological order
      const idx = (this.historyHead - n + i + this.historySize) % this.historySize;
      sumX += i;
      sumY += arr[idx];
      sumXY += i * arr[idx];
      sumX2 += i * i;
    }
    const denom = n * sumX2 - sumX * sumX;
    return denom !== 0 ? (n * sumXY - sumX * sumY) / denom : 0;
  }

  /** Get the peak history as an array (for sparkline display). */
  getPeakHistory() {
    return this._getOrderedHistory(this.peakHistory);
  }

  /** Get the trough history as an array (for sparkline display). */
  getTroughHistory() {
    return this._getOrderedHistory(this.troughHistory);
  }

  /** Get the mean history as an array (for sparkline display). */
  getMeanHistory() {
    return this._getOrderedHistory(this.meanHistory);
  }

  /** @private */
  _getOrderedHistory(arr) {
    const n = Math.min(this.historyCount, this.historySize);
    const result = new Array(n);
    for (let i = 0; i < n; i++) {
      result[i] = arr[(this.historyHead - n + i + this.historySize) % this.historySize];
    }
    return result;
  }

  /** Reset and optionally start re-learning baseline. */
  reset() {
    this.windowIndex = 0;
    this.historyHead = 0;
    this.historyCount = 0;
    this.periodsProcessed = 0;
    this.baselinePeak = null;
    this.baselineTrough = null;
    this.baselineMean = null;
  }
}
```

### Characteristics

| Property | Value |
|----------|-------|
| Memory | ~2.8 KB per detector (3 history arrays + 1 window buffer) |
| CPU per sample | O(1) accumulate, O(n) per period for analysis |
| Detection latency | One full period |
| False positive rate | Low — compares aggregate metrics, not individual samples |
| Best for | Amplitude drift, DC offset, asymmetric changes |
| Weakness | Requires known period length; one-period delay |

---

## Approach 6: Rise/Fall Time Monitoring

### Concept

Track the slope of the signal at key transition points (rising edges, falling edges). Changes in rise/fall time indicate waveform shape changes that other methods might miss: slower edges suggest component degradation, steeper edges suggest oscillation or ringing.

This is particularly valuable for PWM signals and step responses.

### Algorithm

1. Detect zero crossings (or threshold crossings) of the signal
2. At each crossing, measure the slope (dV/dt) using a few samples around the crossing
3. Track slope values over time using EMA
4. Alert when slope changes significantly

### JavaScript Implementation

```javascript
/**
 * Rise/Fall time monitor.
 * Tracks signal slope at transition points.
 * Detects changes in edge sharpness, ringing, degradation.
 */
class RiseFallMonitor {
  /**
   * @param {number} crossingLevel - Signal level that defines a "crossing"
   *                                 (default: auto-detect from signal midpoint)
   * @param {number} slopeWindow - Samples around crossing to measure slope
   * @param {number} changeThreshold - Fractional change to trigger alert
   */
  constructor(crossingLevel = null, slopeWindow = 5, changeThreshold = 0.20) {
    this.crossingLevel = crossingLevel;
    this.slopeWindow = slopeWindow;
    this.changeThreshold = changeThreshold;

    // Circular sample buffer for slope calculation
    this.bufferSize = slopeWindow * 2 + 1;
    this.buffer = new Float64Array(this.bufferSize);
    this.bufHead = 0;
    this.bufCount = 0;

    // Previous sample for crossing detection
    this.prevValue = null;
    this.sampleIndex = 0;

    // Auto-level detection
    this.emaLevel = 0;
    this.levelAlpha = 0.001; // Very slow tracking for midpoint
    this.levelInitialized = false;

    // Slope history
    this.riseSlopeEma = 0;
    this.fallSlopeEma = 0;
    this.slopeAlpha = 0.1; // Track slope changes
    this.riseCount = 0;
    this.fallCount = 0;

    // Baseline
    this.baselineRiseSlope = null;
    this.baselineFallSlope = null;
    this.baselineSamples = 10; // Crossings before baseline is established
  }

  /**
   * Add a sample.
   * @param {number} value
   * @param {number} dt - Time step between samples (seconds). Default 1/50.
   * @returns {null | { type: 'rise'|'fall', slope: number, change: number,
   *           anomaly: boolean }}
   */
  addSample(value, dt = 0.02) {
    this.sampleIndex++;

    // Store in buffer
    this.buffer[this.bufHead] = value;
    this.bufHead = (this.bufHead + 1) % this.bufferSize;
    if (this.bufCount < this.bufferSize) this.bufCount++;

    // Auto-detect crossing level from signal midpoint
    if (this.crossingLevel === null) {
      if (!this.levelInitialized) {
        this.emaLevel = value;
        this.levelInitialized = true;
      } else {
        this.emaLevel = this.levelAlpha * value + (1 - this.levelAlpha) * this.emaLevel;
      }
    }
    const level = this.crossingLevel !== null ? this.crossingLevel : this.emaLevel;

    // Detect crossing
    let result = null;
    if (this.prevValue !== null && this.bufCount >= this.bufferSize) {
      const crossedUp = this.prevValue < level && value >= level;
      const crossedDown = this.prevValue >= level && value < level;

      if (crossedUp || crossedDown) {
        const slope = this._measureSlope(dt);
        const type = crossedUp ? 'rise' : 'fall';

        if (type === 'rise') {
          this.riseCount++;
          if (this.riseCount === 1) {
            this.riseSlopeEma = Math.abs(slope);
          } else {
            this.riseSlopeEma = this.slopeAlpha * Math.abs(slope)
              + (1 - this.slopeAlpha) * this.riseSlopeEma;
          }
        } else {
          this.fallCount++;
          if (this.fallCount === 1) {
            this.fallSlopeEma = Math.abs(slope);
          } else {
            this.fallSlopeEma = this.slopeAlpha * Math.abs(slope)
              + (1 - this.slopeAlpha) * this.fallSlopeEma;
          }
        }

        // Establish baseline
        const totalCrossings = this.riseCount + this.fallCount;
        if (totalCrossings === this.baselineSamples) {
          this.baselineRiseSlope = this.riseSlopeEma;
          this.baselineFallSlope = this.fallSlopeEma;
        }

        // Check for anomaly
        let change = 0;
        let anomaly = false;
        if (this.baselineRiseSlope !== null) {
          const baseline = type === 'rise' ? this.baselineRiseSlope : this.baselineFallSlope;
          const current = type === 'rise' ? this.riseSlopeEma : this.fallSlopeEma;
          change = baseline > 0 ? (current - baseline) / baseline : 0;
          anomaly = Math.abs(change) > this.changeThreshold;
        }

        result = {
          type,
          slope: Math.abs(slope),
          slopeEma: type === 'rise' ? this.riseSlopeEma : this.fallSlopeEma,
          change,
          anomaly,
          crossingIndex: this.sampleIndex,
        };
      }
    }

    this.prevValue = value;
    return result;
  }

  /** @private Compute slope around the most recent crossing using linear regression. */
  _measureSlope(dt) {
    const n = this.bufferSize;
    let sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;

    for (let i = 0; i < n; i++) {
      const idx = (this.bufHead - n + i + this.bufferSize) % this.bufferSize;
      const x = i * dt;
      const y = this.buffer[idx];
      sumX += x;
      sumY += y;
      sumXY += x * y;
      sumX2 += x * x;
    }

    const denom = n * sumX2 - sumX * sumX;
    return denom !== 0 ? (n * sumXY - sumX * sumY) / denom : 0;
  }

  /** Reset state. */
  reset() {
    this.bufHead = 0;
    this.bufCount = 0;
    this.prevValue = null;
    this.sampleIndex = 0;
    this.riseSlopeEma = 0;
    this.fallSlopeEma = 0;
    this.riseCount = 0;
    this.fallCount = 0;
    this.baselineRiseSlope = null;
    this.baselineFallSlope = null;
  }
}
```

### Characteristics

| Property | Value |
|----------|-------|
| Memory | ~200 bytes per detector |
| CPU per sample | O(1) normally; O(slopeWindow) at crossings |
| Detection latency | At the next crossing event (typically half a period) |
| False positive rate | Low — only fires at crossings, smoothed by EMA |
| Best for | PWM quality, step response, edge degradation |
| Weakness | Meaningless for aperiodic or non-oscillating signals |

---

## Comparison Table

| Approach | Memory | CPU/Sample | Detect Latency | False Positives | Drift | Spikes | Shape | Multi-Channel |
|----------|--------|-----------|----------------|-----------------|-------|--------|-------|---------------|
| **Rolling Stats** | ~1.6 KB | O(1) | Immediate | Medium (tunable) | Poor | Excellent | Poor | Good |
| **CUSUM** | 48 B | O(1) | Proportional to shift | Very low | Excellent | Poor | Poor | Excellent |
| **Window Compare** | ~2.4 KB | O(n)/period | 1 period | Low | Good | Good | Excellent | Fair |
| **EMA** | 48 B | O(1) | ~2/alpha samples | Medium | Moderate | Good | Poor | Excellent |
| **Peak/Trough** | ~2.8 KB | O(n)/period | 1 period | Low | Excellent | Fair | Fair | Good |
| **Rise/Fall** | ~200 B | O(1) | Half period | Low | Fair | Poor | Good (edges) | Good |

### Recommended Combinations

**For IMU monitoring (accelerometer, gyroscope):**
1. EMA detector on each axis (lightweight, catches spikes)
2. CUSUM on each axis (catches drift)
3. Peak/Trough tracker per plot (catches amplitude changes)

**For PWM signals:**
1. Rolling Stats (catches duty cycle anomalies)
2. Rise/Fall monitor (catches edge quality changes)
3. Peak/Trough tracker (catches voltage level changes)

**For vibration analysis:**
1. Window Comparator (catches waveform shape changes)
2. Peak/Trough tracker (catches amplitude envelope changes)
3. CUSUM on peak values (catches gradual amplitude drift)

---

## Baseline Learning Phase

### Why a Baseline Is Needed

Most detection approaches need to know what "normal" looks like before they can identify "abnormal." This is the baseline learning phase.

### Design

```javascript
/**
 * Baseline learning manager.
 * Coordinates the learning phase across multiple detectors.
 */
class BaselineManager {
  /**
   * @param {number} learningSamples - Samples to collect before baselining
   * @param {number} learningPeriods - Periods to observe (for periodic signals)
   */
  constructor(learningSamples = 500, learningPeriods = 5) {
    this.targetSamples = learningSamples;
    this.targetPeriods = learningPeriods;

    this.samples = [];
    this.state = 'idle'; // 'idle' | 'learning' | 'ready'
    this.callbacks = [];
  }

  /** Start collecting baseline data. */
  startLearning() {
    this.samples = [];
    this.state = 'learning';
  }

  /**
   * Feed a sample during learning.
   * @returns {{ state: string, progress: number }}
   */
  addSample(value) {
    if (this.state !== 'learning') {
      return { state: this.state, progress: 1.0 };
    }

    this.samples.push(value);
    const progress = this.samples.length / this.targetSamples;

    if (this.samples.length >= this.targetSamples) {
      this.state = 'ready';
      this._notifyReady();
    }

    return { state: this.state, progress: Math.min(progress, 1.0) };
  }

  /** Get baseline statistics. */
  getBaseline() {
    if (this.state !== 'ready') return null;

    const n = this.samples.length;
    const mean = this.samples.reduce((a, b) => a + b, 0) / n;
    const variance = this.samples.reduce((s, x) => s + (x - mean) ** 2, 0) / (n - 1);
    const stddev = Math.sqrt(variance);
    const min = Math.min(...this.samples);
    const max = Math.max(...this.samples);

    return { mean, stddev, min, max, sampleCount: n };
  }

  /** Register callback for when baseline is ready. */
  onReady(callback) {
    this.callbacks.push(callback);
  }

  /** @private */
  _notifyReady() {
    const baseline = this.getBaseline();
    for (const cb of this.callbacks) {
      cb(baseline);
    }
  }
}
```

### Baseline Learning Modes

| Mode | Samples | Time at 50Hz | Use Case |
|------|---------|-------------|----------|
| Quick | 200 | 4 seconds | Impatient users, obvious signals |
| Standard | 500 | 10 seconds | Good balance |
| Thorough | 2000 | 40 seconds | Noisy signals, precision |
| Manual | User-triggered | User decides | Expert users |

### Recommended UX

```
┌──────────────────────────────────────────────────┐
│  Anomaly Detection: [▶ Start Learning]           │
│                                                  │
│  [After clicking Start Learning:]                │
│  Learning baseline... ████████░░ 80% (8s)       │
│                                                  │
│  [After learning completes:]                     │
│  Baseline: mean=1.002g, σ=0.015g  [Re-learn]   │
│  Monitoring: ● Active  Anomalies: 0             │
└──────────────────────────────────────────────────┘
```

The baseline should be per-channel and per-plot. When the user connects a new device or changes the signal source, they should re-learn.

### Auto-Baseline vs Manual

| Approach | Pro | Con |
|----------|-----|-----|
| Auto (start immediately) | No user action needed | May learn during an anomaly |
| Manual (button press) | User controls when "normal" is defined | Requires user awareness |
| Hybrid (auto + re-learn button) | Best of both worlds | Slightly more UI |

**Recommendation**: Use hybrid. Auto-start baseline learning on first data, but provide a "Re-learn baseline" button so the user can reset it when they know the signal is clean.

---

## Chart.js Anomaly Visualization

### Technique 1: Point Color Changes

Mark anomalous points with a different color on the existing line chart. This uses Chart.js's scriptable options feature.

```javascript
/**
 * Apply anomaly coloring to a Chart.js dataset.
 * Normal points are invisible (pointRadius: 0), anomaly points
 * are rendered as colored dots.
 */
function applyAnomalyVisualization(chart, datasetIndex, anomalyIndices) {
  const dataset = chart.data.datasets[datasetIndex];
  const n = dataset.data.length;

  // Scriptable pointRadius: show point only at anomalies
  dataset.pointRadius = (ctx) => {
    return anomalyIndices.has(ctx.dataIndex) ? 5 : 0;
  };

  // Scriptable pointBackgroundColor: red for anomalies
  dataset.pointBackgroundColor = (ctx) => {
    return anomalyIndices.has(ctx.dataIndex) ? '#ff4444' : dataset.borderColor;
  };

  // Scriptable pointBorderColor
  dataset.pointBorderColor = (ctx) => {
    return anomalyIndices.has(ctx.dataIndex) ? '#ff0000' : dataset.borderColor;
  };

  chart.update('none');
}
```

### Technique 2: Line Segment Coloring

Change the color of the line itself in anomalous regions using the `segment` property.

```javascript
/**
 * Color line segments based on anomaly state.
 * Green = normal, yellow = warning, red = anomaly.
 */
function createAnomalyDataset(label, color) {
  // Maintain anomaly state per sample index
  const anomalyState = []; // 'normal' | 'warning' | 'anomaly'

  return {
    label,
    data: [],
    borderColor: color,
    borderWidth: 1.5,
    pointRadius: 0,
    tension: 0.2,
    // Segment styling: color changes between consecutive points
    segment: {
      borderColor: (ctx) => {
        const state = anomalyState[ctx.p1DataIndex];
        if (state === 'anomaly') return '#ff4444';
        if (state === 'warning') return '#ffaa00';
        return undefined; // Use default color
      },
      borderWidth: (ctx) => {
        return anomalyState[ctx.p1DataIndex] === 'anomaly' ? 3 : undefined;
      },
    },
    // Expose anomalyState for external updates
    _anomalyState: anomalyState,
  };
}

// Usage in data update loop:
function updateWithAnomaly(chart, datasetIndex, value, isAnomaly) {
  const dataset = chart.data.datasets[datasetIndex];
  dataset.data.push(value);
  dataset._anomalyState.push(isAnomaly ? 'anomaly' : 'normal');

  // Trim old data
  if (dataset.data.length > MAX_DATA_POINTS) {
    dataset.data.shift();
    dataset._anomalyState.shift();
  }
}
```

### Technique 3: Annotation Overlays

Use chartjs-plugin-annotation to draw threshold bands, alarm markers, and reference lines.

```javascript
/**
 * Add dynamic anomaly annotations to a chart.
 * Requires: chartjs-plugin-annotation
 */
function setupAnomalyAnnotations(chart) {
  chart.options.plugins.annotation = {
    annotations: {}
  };

  return {
    /**
     * Show/update the baseline band (mean +/- N*sigma).
     */
    setBaselineBand(mean, sigma, nSigma = 3) {
      chart.options.plugins.annotation.annotations.baselineBand = {
        type: 'box',
        yMin: mean - nSigma * sigma,
        yMax: mean + nSigma * sigma,
        backgroundColor: 'rgba(0, 255, 0, 0.05)',
        borderColor: 'rgba(0, 255, 0, 0.2)',
        borderWidth: 1,
        borderDash: [4, 4],
        label: {
          content: `±${nSigma}σ`,
          enabled: true,
          position: 'start',
          color: '#888',
          font: { size: 10 },
        },
      };

      chart.options.plugins.annotation.annotations.meanLine = {
        type: 'line',
        yMin: mean,
        yMax: mean,
        borderColor: 'rgba(0, 255, 0, 0.4)',
        borderWidth: 1,
        borderDash: [2, 2],
        label: {
          content: `μ = ${mean.toFixed(3)}`,
          enabled: true,
          position: 'end',
          color: '#888',
          font: { size: 10 },
        },
      };

      chart.update('none');
    },

    /**
     * Add an anomaly marker at a specific point.
     */
    addAnomalyMarker(xValue, yValue, message) {
      const id = `anomaly_${Date.now()}`;
      chart.options.plugins.annotation.annotations[id] = {
        type: 'point',
        xValue,
        yValue,
        backgroundColor: 'rgba(255, 0, 0, 0.8)',
        radius: 6,
        borderColor: '#ff0000',
        borderWidth: 2,
      };
      chart.update('none');

      // Auto-remove after it scrolls off screen
      setTimeout(() => {
        delete chart.options.plugins.annotation.annotations[id];
      }, 30000);
    },

    /**
     * Clear all anomaly annotations.
     */
    clearAnomalies() {
      const annotations = chart.options.plugins.annotation.annotations;
      for (const key of Object.keys(annotations)) {
        if (key.startsWith('anomaly_')) {
          delete annotations[key];
        }
      }
      chart.update('none');
    },
  };
}
```

### Technique 4: Background Color Bands

Change the chart background color based on anomaly state.

```javascript
/**
 * Chart.js plugin that changes background color based on anomaly state.
 * Register once: Chart.register(anomalyBackgroundPlugin)
 */
const anomalyBackgroundPlugin = {
  id: 'anomalyBackground',
  beforeDraw(chart) {
    const state = chart.options.plugins.anomalyBackground;
    if (!state || !state.active) return;

    const { ctx, chartArea } = chart;
    const { top, bottom, left, right } = chartArea;

    let bgColor;
    switch (state.level) {
      case 'warning':
        bgColor = 'rgba(255, 170, 0, 0.05)';
        break;
      case 'anomaly':
        bgColor = 'rgba(255, 0, 0, 0.08)';
        break;
      default:
        return; // Normal — no background change
    }

    ctx.save();
    ctx.fillStyle = bgColor;
    ctx.fillRect(left, top, right - left, bottom - top);
    ctx.restore();
  }
};

// Usage:
Chart.register(anomalyBackgroundPlugin);

// Update anomaly state:
chart.options.plugins.anomalyBackground = {
  active: true,
  level: 'anomaly', // 'normal' | 'warning' | 'anomaly'
};
chart.update('none');
```

### Technique 5: Status Indicator Below Plot

A simple HTML element below each chart showing current anomaly status.

```javascript
/**
 * Create an anomaly status bar for a chart.
 * @param {HTMLElement} container - Element to append the status bar to
 */
function createAnomalyStatusBar(container) {
  const bar = document.createElement('div');
  bar.className = 'anomaly-status-bar';
  bar.innerHTML = `
    <span class="anomaly-indicator" style="color: #4caf50;">● Normal</span>
    <span class="anomaly-stats">
      Mean: <span class="stat-mean">--</span>
      Peak: <span class="stat-peak">--</span>
      σ: <span class="stat-sigma">--</span>
    </span>
    <span class="anomaly-count">Anomalies: <span class="count">0</span></span>
  `;
  container.appendChild(bar);

  return {
    update(state) {
      const indicator = bar.querySelector('.anomaly-indicator');
      if (state.anomaly) {
        indicator.style.color = '#ff4444';
        indicator.textContent = `● ANOMALY: ${state.message}`;
      } else if (state.warning) {
        indicator.style.color = '#ffaa00';
        indicator.textContent = `● Warning: ${state.message}`;
      } else {
        indicator.style.color = '#4caf50';
        indicator.textContent = '● Normal';
      }

      bar.querySelector('.stat-mean').textContent = state.mean?.toFixed(3) ?? '--';
      bar.querySelector('.stat-peak').textContent = state.peak?.toFixed(3) ?? '--';
      bar.querySelector('.stat-sigma').textContent = state.stddev?.toFixed(4) ?? '--';
      bar.querySelector('.count').textContent = state.anomalyCount ?? 0;
    },
  };
}
```

### CSS for Anomaly Status Bar

```css
.anomaly-status-bar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 4px 8px;
  background: #1a1a1a;
  border-top: 1px solid #333;
  font-size: 11px;
  font-family: monospace;
  color: #888;
}

.anomaly-indicator {
  font-weight: bold;
  min-width: 200px;
}

.anomaly-stats span {
  color: #aaa;
}

.anomaly-count .count {
  color: #ff4444;
  font-weight: bold;
}
```

### Visualization Recommendation Summary

| Technique | Visual Impact | Performance Cost | Recommended For |
|-----------|-------------|-----------------|-----------------|
| Point color changes | Subtle | Low | Individual spike marking |
| Segment coloring | Strong | Low | Sustained anomaly regions |
| Annotation overlays | Strong | Medium | Threshold bands, key events |
| Background color | Ambient | Very low | Overall status (alarm mode) |
| Status bar | Informational | None | Always-on statistics display |

**Recommended combination for fc_tool:**
1. **Always**: Status bar below each chart (shows stats, anomaly count)
2. **Always**: Background color plugin (ambient alarm indication)
3. **On anomaly**: Segment coloring (shows exactly where anomaly occurred)
4. **Optional**: Annotation band showing baseline +/- N*sigma region

---

## Anomaly Trend Sparklines

### Concept

Small mini-graphs (sparklines) below the main plot that show how derived metrics evolve over time. These give the user a "meta view" of signal health.

### Implementation

```javascript
/**
 * Sparkline renderer using a secondary Chart.js instance.
 * Renders a minimal line chart suitable for trend visualization.
 *
 * Memory: ~50 values per sparkline. At 1 update per period (~1 second),
 * 50 values = 50 seconds of trend history. Increase for longer history.
 */
class Sparkline {
  /**
   * @param {HTMLCanvasElement} canvas - Small canvas element (e.g., 200x30)
   * @param {string} color - Line color
   * @param {number} maxPoints - Maximum data points to display
   */
  constructor(canvas, color = '#4fc3f7', maxPoints = 60) {
    this.maxPoints = maxPoints;
    this.chart = new Chart(canvas, {
      type: 'line',
      data: {
        labels: [],
        datasets: [{
          data: [],
          borderColor: color,
          borderWidth: 1,
          pointRadius: 0,
          tension: 0.3,
          fill: {
            target: 'origin',
            above: color + '20',
            below: color + '20',
          },
        }],
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        animation: false,
        plugins: {
          legend: { display: false },
          tooltip: { enabled: false },
        },
        scales: {
          x: { display: false },
          y: {
            display: false,
            grace: '20%',
          },
        },
        elements: {
          line: { borderWidth: 1 },
        },
      },
    });
  }

  /** Add a data point and update the sparkline. */
  addPoint(value) {
    const dataset = this.chart.data.datasets[0];
    dataset.data.push(value);
    this.chart.data.labels.push('');

    if (dataset.data.length > this.maxPoints) {
      dataset.data.shift();
      this.chart.data.labels.shift();
    }

    this.chart.update('none');
  }

  /** Clear all data. */
  clear() {
    this.chart.data.datasets[0].data = [];
    this.chart.data.labels = [];
    this.chart.update('none');
  }

  /** Destroy the chart instance. */
  destroy() {
    this.chart.destroy();
  }
}

/**
 * Create a sparkline panel below a main chart.
 * Shows peak, trough, and mean trends as mini-graphs.
 */
function createSparklinePanel(parentContainer) {
  const panel = document.createElement('div');
  panel.className = 'sparkline-panel';
  panel.innerHTML = `
    <div class="sparkline-row">
      <span class="sparkline-label">Peak</span>
      <canvas class="sparkline-canvas" id="spark-peak"></canvas>
      <span class="sparkline-value" id="spark-peak-val">--</span>
    </div>
    <div class="sparkline-row">
      <span class="sparkline-label">Mean</span>
      <canvas class="sparkline-canvas" id="spark-mean"></canvas>
      <span class="sparkline-value" id="spark-mean-val">--</span>
    </div>
    <div class="sparkline-row">
      <span class="sparkline-label">Trough</span>
      <canvas class="sparkline-canvas" id="spark-trough"></canvas>
      <span class="sparkline-value" id="spark-trough-val">--</span>
    </div>
  `;
  parentContainer.appendChild(panel);

  const sparkPeak = new Sparkline(
    panel.querySelector('#spark-peak'), '#ef5350', 60
  );
  const sparkMean = new Sparkline(
    panel.querySelector('#spark-mean'), '#81c784', 60
  );
  const sparkTrough = new Sparkline(
    panel.querySelector('#spark-trough'), '#4fc3f7', 60
  );

  return {
    update(peakVal, meanVal, troughVal) {
      sparkPeak.addPoint(peakVal);
      sparkMean.addPoint(meanVal);
      sparkTrough.addPoint(troughVal);

      panel.querySelector('#spark-peak-val').textContent = peakVal.toFixed(3);
      panel.querySelector('#spark-mean-val').textContent = meanVal.toFixed(3);
      panel.querySelector('#spark-trough-val').textContent = troughVal.toFixed(3);
    },
    clear() {
      sparkPeak.clear();
      sparkMean.clear();
      sparkTrough.clear();
    },
    destroy() {
      sparkPeak.destroy();
      sparkMean.destroy();
      sparkTrough.destroy();
      panel.remove();
    },
  };
}
```

### Sparkline CSS

```css
.sparkline-panel {
  display: flex;
  flex-direction: column;
  gap: 2px;
  padding: 4px 8px;
  background: #1a1a1a;
  border-top: 1px solid #333;
}

.sparkline-row {
  display: flex;
  align-items: center;
  gap: 8px;
  height: 24px;
}

.sparkline-label {
  font-size: 10px;
  color: #666;
  width: 40px;
  text-align: right;
  font-family: monospace;
}

.sparkline-canvas {
  width: 150px;
  height: 20px;
}

.sparkline-value {
  font-size: 10px;
  color: #aaa;
  width: 60px;
  font-family: monospace;
}
```

### Sparkline Layout

```
┌──────────────────────────────────────────────────────────┐
│  Accelerometer (g)                        [Mode ▼] [+][-]│
│  ┌──────────────────────────────────────────────────────┐│
│  │                                                      ││
│  │    Main Chart (existing accel chart)                 ││
│  │                                                      ││
│  └──────────────────────────────────────────────────────┘│
│  ● Normal  Mean: 1.002  Peak: 1.045  σ: 0.015  Anom: 0  │  ← Status bar
│  Peak   ─────────────── 1.045                            │  ← Sparklines
│  Mean   ─────────────── 1.002                            │
│  Trough ─────────────── 0.961                            │
└──────────────────────────────────────────────────────────┘
```

---

## Integration Architecture

### How All Pieces Fit Together in fc_tool

```
Serial Data (50Hz)
       │
       ▼
  parseImuData()          ← Existing parser in main.js
       │
       ├──► updateCharts()    ← Existing Chart.js update
       │
       └──► AnomalyEngine    ← NEW: orchestrates all detectors
              │
              ├──► EMADetector (per axis)         ─── Spike detection
              ├──► CUSUMDetector (per axis)       ─── Drift detection
              ├──► PeakTroughTracker (per plot)   ─── Amplitude monitoring
              ├──► WaveformComparator (per plot)  ─── Shape monitoring (if periodic)
              └──► RiseFallMonitor (per plot)     ─── Edge quality (if periodic)
                     │
                     ▼
              AnomalyVisualizer
              ├──► Segment coloring on Chart.js
              ├──► Background color plugin
              ├──► Status bar updates
              └──► Sparkline updates
```

### Proposed AnomalyEngine Class

```javascript
/**
 * Orchestrates multiple anomaly detectors for a single signal channel.
 * This is the top-level class that fc_tool's main.js would instantiate.
 */
class AnomalyEngine {
  constructor(config = {}) {
    this.config = {
      emaAlpha: config.emaAlpha ?? 0.05,
      emaSigma: config.emaSigma ?? 3.0,
      cusumAllowance: config.cusumAllowance ?? 0.5,
      cusumThreshold: config.cusumThreshold ?? 4.0,
      rollingWindow: config.rollingWindow ?? 200,
      rollingSigma: config.rollingSigma ?? 3.0,
      ...config,
    };

    // Lightweight detectors (always active)
    this.ema = new EMADetector(this.config.emaAlpha, this.config.emaSigma);
    this.cusum = new CUSUMDetector(0, this.config.cusumAllowance, this.config.cusumThreshold);
    this.rolling = new RollingStatsDetector(this.config.rollingWindow, this.config.rollingSigma);

    // Period-based detectors (activated when period is known)
    this.peakTracker = null;
    this.waveformComp = null;
    this.riseFall = null;

    // State
    this.baselineManager = new BaselineManager(500);
    this.anomalyCount = 0;
    this.isActive = false;
    this.sampleCount = 0;
  }

  /** Start the anomaly detection engine. */
  start() {
    this.isActive = true;
    this.baselineManager.startLearning();
  }

  /** Stop the engine. */
  stop() {
    this.isActive = false;
  }

  /**
   * Enable period-based detectors when period is known.
   * Call this when pattern detection identifies the signal period.
   */
  setPeriod(periodLength) {
    this.peakTracker = new PeakTroughTracker(periodLength, 50, 0.05);
    this.waveformComp = new WaveformComparator(periodLength, 0.95, 0.15);
    this.riseFall = new RiseFallMonitor();

    this.waveformComp.startLearning(5);
  }

  /**
   * Process a new sample through all active detectors.
   * @param {number} value
   * @returns {AnomalyReport}
   */
  addSample(value) {
    if (!this.isActive) return null;
    this.sampleCount++;

    // Baseline learning
    const baseline = this.baselineManager.addSample(value);
    if (baseline.state === 'ready' && this.sampleCount === this.baselineManager.targetSamples) {
      const stats = this.baselineManager.getBaseline();
      this.cusum.calibrateFromBaseline(
        this.baselineManager.samples,
        0.5,
        4.0
      );
    }

    // Run lightweight detectors
    const emaResult = this.ema.addSample(value);
    const cusumResult = this.cusum.addSample(value);
    const rollingResult = this.rolling.addSample(value);

    // Run period-based detectors
    let peakResult = null, waveformResult = null, riseFallResult = null;
    if (this.peakTracker) peakResult = this.peakTracker.addSample(value);
    if (this.waveformComp) waveformResult = this.waveformComp.addSample(value);
    if (this.riseFall) riseFallResult = this.riseFall.addSample(value);

    // Aggregate anomaly verdict
    const anomalies = [];
    if (emaResult.isAnomaly) anomalies.push({ source: 'ema', detail: emaResult });
    if (cusumResult.alarm) anomalies.push({ source: 'cusum', detail: cusumResult });
    if (rollingResult.isAnomaly) anomalies.push({ source: 'rolling', detail: rollingResult });
    if (peakResult?.anomaly) anomalies.push({ source: 'peak', detail: peakResult });
    if (waveformResult?.anomaly) anomalies.push({ source: 'waveform', detail: waveformResult });
    if (riseFallResult?.anomaly) anomalies.push({ source: 'riseFall', detail: riseFallResult });

    if (anomalies.length > 0) this.anomalyCount++;

    return {
      sampleIndex: this.sampleCount,
      value,
      hasAnomaly: anomalies.length > 0,
      anomalies,
      stats: {
        mean: rollingResult.mean,
        stddev: rollingResult.stddev,
        ema: emaResult.ema,
      },
      peakTrough: peakResult,
      anomalyCount: this.anomalyCount,
      baselineState: baseline.state,
      baselineProgress: baseline.progress,
    };
  }

  /** Reset all detectors. */
  reset() {
    this.ema.reset();
    this.cusum.resetAccumulators();
    this.rolling.reset();
    if (this.peakTracker) this.peakTracker.reset();
    if (this.waveformComp) this.waveformComp = null;
    if (this.riseFall) this.riseFall.reset();
    this.anomalyCount = 0;
    this.sampleCount = 0;
    this.baselineManager = new BaselineManager(500);
  }
}
```

### Memory Budget Analysis

Running all detectors on 6 IMU channels for hours:

| Component | Per Channel | x6 Channels | Notes |
|-----------|-------------|------------|-------|
| EMADetector | 48 B | 288 B | 6 numbers |
| CUSUMDetector | 48 B | 288 B | 6 numbers |
| RollingStatsDetector | 1.6 KB | 9.6 KB | Float64Array(200) |
| PeakTroughTracker | 2.8 KB | 16.8 KB | 3 history arrays + window |
| WaveformComparator | 2.4 KB | 14.4 KB | Reference + current period |
| RiseFallMonitor | 200 B | 1.2 KB | Small buffer |
| **Total** | **~7.1 KB** | **~42.6 KB** | **Fixed — no growth** |

All implementations use fixed-size circular buffers. There is no memory growth over time. Running for hours, days, or weeks uses the same 42.6 KB across all channels.

For comparison, the existing Chart.js data arrays (6 axes x 100 Float64 values = 4.8 KB) are in the same ballpark.

### CPU Budget Analysis

At 50Hz on 6 channels:

| Detector | Operations per sample | Total per second |
|----------|---------------------|------------------|
| EMA | ~10 FLOPs | 3,000 |
| CUSUM | ~10 FLOPs | 3,000 |
| Rolling Stats | ~15 FLOPs | 4,500 |
| Peak/Trough | ~5 per sample, ~200 per period | ~1,900 |
| Waveform Comp | ~5 per sample, ~500 per period | ~2,000 |
| Rise/Fall | ~5 per sample, ~50 per crossing | ~1,600 |
| **Total** | | **~16,000 FLOPs/sec** |

This is negligible. A modern browser can handle hundreds of millions of FLOPs per second. The anomaly detection overhead is under 0.01% of available CPU, even at 100Hz.

---

## Recommendations for fc_tool

### Implementation Phases

**Phase 1 — Minimum Viable Anomaly Detection (low effort, high value):**
1. Add `EMADetector` to each IMU axis (6 detectors, 288 bytes total)
2. Add status bar below each chart showing mean, peak, stddev
3. Use segment coloring to highlight anomalous regions
4. No baseline learning needed — EMA is self-adapting

**Phase 2 — Drift Detection (medium effort):**
1. Add `CUSUMDetector` per axis with auto-calibration
2. Add baseline learning UI (progress bar, "Re-learn" button)
3. Add background color plugin for ambient alarm state
4. Add anomaly counter to status bar

**Phase 3 — Waveform Analysis (requires pattern detection first):**
1. After pattern/trigger mode is implemented (from signal-analysis-discussion.md)
2. Add `PeakTroughTracker` per plot
3. Add sparklines showing peak/trough/mean trends
4. Add `WaveformComparator` for shape monitoring
5. Add `RiseFallMonitor` for edge quality

**Phase 4 — Full Engine:**
1. Integrate `AnomalyEngine` as the orchestrator
2. Add per-plot anomaly configuration panel
3. Add anomaly history export (CSV)
4. Add annotation overlays for threshold bands

### Key Design Decisions

| Decision | Recommendation | Rationale |
|----------|---------------|-----------|
| Baseline mode | Hybrid (auto + manual re-learn) | Best UX balance |
| Default sigma | 3.0 | Standard; low false positive rate |
| Detection on/off | Per-plot toggle | Users may not want it on all plots |
| Anomaly visualization | Segment coloring + status bar | Low cost, high clarity |
| Sparklines | Optional panel | Nice-to-have, adds visual complexity |
| Web Worker | Not needed initially | CPU cost is negligible at 50Hz |

### Integration with Existing Code

The current `processSerialData()` -> `updateCharts()` flow in `main.js` can be extended minimally:

```javascript
// In main.js, after parseImuData():
const anomalyEngines = {
  ax: new AnomalyEngine(),
  ay: new AnomalyEngine(),
  // ... etc
};

function updateCharts(imuData) {
  // ... existing chart update code ...

  // NEW: Run anomaly detection
  if (anomalyDetectionEnabled) {
    const results = {
      ax: anomalyEngines.ax.addSample(imuData.accel.x),
      ay: anomalyEngines.ay.addSample(imuData.accel.y),
      az: anomalyEngines.az.addSample(imuData.accel.z),
    };

    // Update visualization
    for (const [channel, result] of Object.entries(results)) {
      if (result?.hasAnomaly) {
        updateAnomalyVisualization(channel, result);
      }
    }
  }
}
```

---

## References

### Algorithms

- Page, E.S. (1954). "Continuous Inspection Schemes." Biometrika, 41(1/2), 100-115. Original CUSUM paper.
- Welford, B.P. (1962). "Note on a Method for Calculating Corrected Sums of Squares and Products." Technometrics, 4(3), 419-420. Online variance algorithm.
- Roberts, S.W. (1959). "Control Chart Tests Based on Geometric Moving Averages." Technometrics, 1(3), 239-250. EWMA (exponential weighted moving average) for control charts.
- Montgomery, D.C. (2019). "Introduction to Statistical Quality Control." 8th Edition, Wiley. Comprehensive reference for CUSUM, EWMA, and Shewhart charts.

### Chart.js

- [Chart.js Segment Styling](https://www.chartjs.org/docs/latest/samples/line/segments.html) — Scriptable segment colors for line charts.
- [Chart.js Scriptable Options](https://www.chartjs.org/docs/latest/general/options.html#scriptable-options) — Dynamic per-point styling (colors, radius).
- [chartjs-plugin-annotation](https://www.chartjs.org/chartjs-plugin-annotation/latest/) — Overlay lines, boxes, and labels on charts.
- [Chart.js Custom Plugins](https://www.chartjs.org/docs/latest/developers/plugins.html) — Writing custom rendering plugins (e.g., background color).

### Related fc_tool Research

- [signal-analysis-discussion.md](../signal-analysis-discussion.md) — Pattern detection and anomaly overlay vision.
- [chartjs-oscilloscope-research.md](chartjs-oscilloscope-research.md) — Chart.js features for oscilloscope-style display.
- [multi-graph-plotter-research.md](multi-graph-plotter-research.md) — Multi-plot architecture.
- [serial-telemetry-protocol.md](../features/serial-telemetry-protocol.md) — Data format and rates.

---

*This research informs fc_tool's anomaly detection implementation. All code examples are designed for the existing vanilla JS + Chart.js architecture with no additional dependencies beyond chartjs-plugin-annotation.*
