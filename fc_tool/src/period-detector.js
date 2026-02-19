/**
 * period-detector.js — Threshold trigger for period mode
 *
 * Detects periodic signals by threshold crossing (like an oscilloscope trigger).
 * When signal crosses the trigger level, a new period starts. Completed periods
 * are returned for display.
 *
 * Based on research: fc_tool/docs/findings/signal-period-detection-research.md
 */

/**
 * Threshold-based period trigger.
 *
 * Segments a continuous data stream into periods based on edge crossings.
 * Each time the signal crosses the trigger level in the configured direction,
 * a new period boundary is detected. Completed periods are accumulated and
 * returned when the requested count (N) is reached.
 */
class ThresholdTrigger {
  /**
   * @param {object} options
   * @param {number} options.level - Trigger threshold value (default 0)
   * @param {'rising'|'falling'} options.edge - Trigger edge direction (default 'rising')
   * @param {number} options.hysteresis - Noise margin to prevent re-triggering (default 0)
   * @param {number} options.periodsNeeded - How many periods to collect before emitting (default 1)
   * @param {number} options.sampleRate - Sample rate in Hz for frequency calc (default 50)
   * @param {number} options.minPeriodSamples - Minimum samples per period to avoid noise triggers (default 5)
   */
  constructor(options = {}) {
    this.level = options.level ?? 0;
    this.edge = options.edge ?? 'rising';
    this.hysteresis = options.hysteresis ?? 0;
    this.periodsNeeded = options.periodsNeeded ?? 1;
    this.sampleRate = options.sampleRate ?? 50;
    this.minPeriodSamples = options.minPeriodSamples ?? 5;

    // State
    this.prevValue = null;
    this.armed = true;
    this.sampleIndex = 0;

    // Current period buffer (samples since last trigger)
    this.currentPeriod = [];

    // Completed periods waiting to be consumed
    this.completedPeriods = [];

    // Detected timing
    this.lastTriggerIndex = 0;
    this.detectedPeriodSamples = 0;
    this.detectedFrequencyHz = 0;
    this.detectedPeriodMs = 0;
    this.triggerCount = 0;
  }

  /**
   * Feed a sample value. Returns an object when enough periods are ready.
   * @param {number} value
   * @returns {null|{periods: number[][], periodSamples: number, frequencyHz: number, periodMs: number}}
   */
  addSample(value) {
    this.sampleIndex++;
    this.currentPeriod.push(value);

    let triggered = false;

    if (this.prevValue !== null) {
      const crossed = this.edge === 'rising'
        ? (this.prevValue < this.level && value >= this.level)
        : (this.prevValue >= this.level && value < this.level);

      if (crossed && this.armed && this.currentPeriod.length >= this.minPeriodSamples) {
        triggered = true;

        // Apply hysteresis: disarm until signal moves away from level
        if (this.hysteresis > 0) {
          this.armed = false;
        }

        // Calculate period timing
        if (this.triggerCount > 0) {
          this.detectedPeriodSamples = this.sampleIndex - this.lastTriggerIndex;
          this.detectedFrequencyHz = this.sampleRate / this.detectedPeriodSamples;
          this.detectedPeriodMs = (this.detectedPeriodSamples / this.sampleRate) * 1000;
        }
        this.lastTriggerIndex = this.sampleIndex;
        this.triggerCount++;

        // Save completed period (exclude the current trigger sample — it starts the next period)
        if (this.currentPeriod.length > 1) {
          this.completedPeriods.push(this.currentPeriod.slice(0, -1));
        }

        // Start new period from the trigger sample
        this.currentPeriod = [value];

        // Trim old completed periods (keep only what's needed + 1 buffer)
        while (this.completedPeriods.length > this.periodsNeeded + 1) {
          this.completedPeriods.shift();
        }
      }
    }

    // Re-arm hysteresis when signal moves away from trigger level
    if (!this.armed && this.hysteresis > 0) {
      const distFromLevel = Math.abs(value - this.level);
      if (distFromLevel >= this.hysteresis) {
        this.armed = true;
      }
    }

    this.prevValue = value;

    // Return completed periods when we have enough
    if (triggered && this.completedPeriods.length >= this.periodsNeeded) {
      const periods = this.completedPeriods.slice(-this.periodsNeeded);
      return {
        periods,
        periodSamples: this.detectedPeriodSamples,
        frequencyHz: this.detectedFrequencyHz,
        periodMs: this.detectedPeriodMs,
        triggerCount: this.triggerCount,
      };
    }

    return null;
  }

  /**
   * Auto-detect trigger level from an array of recent data.
   * Sets level to the midpoint of the data range.
   * Also sets hysteresis to 5% of the range.
   * @param {number[]} data - Recent sample values
   */
  autoLevel(data) {
    if (!data || data.length < 2) return;
    let min = Infinity, max = -Infinity;
    for (const v of data) {
      if (v < min) min = v;
      if (v > max) max = v;
    }
    const range = max - min;
    this.level = min + range / 2;
    this.hysteresis = range * 0.05;
  }

  /** Reset all state. Call when switching modes or signals. */
  reset() {
    this.prevValue = null;
    this.armed = true;
    this.sampleIndex = 0;
    this.currentPeriod = [];
    this.completedPeriods = [];
    this.lastTriggerIndex = 0;
    this.detectedPeriodSamples = 0;
    this.detectedFrequencyHz = 0;
    this.detectedPeriodMs = 0;
    this.triggerCount = 0;
  }
}

export { ThresholdTrigger };
