# Chart.js Signal Visualization Research for fc_tool

Research findings for implementing oscilloscope-style signal visualization in the
fc_tool Tauri desktop application. The app monitors embedded flight controller
systems (IMU, PWM, sensors) over serial using Chart.js 4.x.

**Current stack:** Chart.js ^4.4.0, vanilla JS, Tauri 2, no bundler (direct
script tag loading via `/lib/chart.min.js`).

---

## Table of Contents

1. [Oscilloscope Trigger Display](#1-oscilloscope-trigger-display)
2. [Per-Plot Mode Switching](#2-per-plot-mode-switching)
3. [Overlay Annotations for Anomaly Markers](#3-overlay-annotations-for-anomaly-markers)
4. [Sparkline Mini-Graphs](#4-sparkline-mini-graphs)
5. [Color-Coded Signal State](#5-color-coded-signal-state)
6. [Frequency/Period Readout Bar](#6-frequencyperiod-readout-bar)
7. [Performance with Multiple Feature-Rich Plots](#7-performance-with-multiple-feature-rich-plots)
8. [Library Comparison](#8-library-comparison)
9. [Plugin Compatibility Notes](#9-plugin-compatibility-notes)
10. [Recommended Architecture](#10-recommended-architecture)

---

## 1. Oscilloscope Trigger Display

### The Problem

A real oscilloscope shows a signal "standing still" by triggering on a repeating
edge and displaying exactly N complete periods from that trigger point. Data
arrives continuously over serial, but the chart must only show **aligned,
stable** waveform views.

### Algorithm: Rising-Edge Trigger with Ring Buffer

The core idea: accumulate incoming samples into a ring buffer, detect trigger
points (rising-edge zero crossings), and when a trigger fires, extract exactly N
periods of data starting from the trigger point and replace the chart data
wholesale.

```javascript
// ============================================================================
// Oscilloscope Trigger Engine
// ============================================================================

class TriggerEngine {
  constructor(options = {}) {
    this.bufferSize = options.bufferSize || 4096;
    this.buffer = new Float64Array(this.bufferSize);
    this.writePos = 0;
    this.sampleCount = 0;

    // Trigger settings
    this.triggerLevel = options.triggerLevel || 0.0;
    this.triggerEdge = options.triggerEdge || 'rising'; // 'rising' | 'falling'
    this.triggerChannel = options.triggerChannel || 0;
    this.periodsToShow = options.periodsToShow || 2;
    this.displayPoints = options.displayPoints || 500;

    // Hysteresis to prevent false triggers on noise
    this.hysteresis = options.hysteresis || 0.05;

    // State
    this.armed = true;
    this.lastSample = null;
    this.lastTriggerIndex = null;
    this.detectedPeriod = null; // in samples
  }

  // Feed a new sample into the buffer. Returns display data when triggered.
  addSample(value) {
    // Write to ring buffer
    this.buffer[this.writePos % this.bufferSize] = value;
    this.writePos++;
    this.sampleCount++;

    const triggered = this._checkTrigger(value);

    if (triggered && this.lastTriggerIndex !== null) {
      // We have two trigger points => we know the period
      const currentIndex = this.writePos - 1;
      const period = currentIndex - this.lastTriggerIndex;
      this.detectedPeriod = period;

      // Calculate how many samples we need for N periods
      const samplesToShow = Math.min(
        period * this.periodsToShow,
        this.bufferSize - period // safety margin
      );

      // Extract display data from the trigger point
      const startIndex = currentIndex - samplesToShow;
      if (startIndex >= 0 && startIndex < this.writePos) {
        const displayData = this._extractFromBuffer(startIndex, samplesToShow);
        this.lastTriggerIndex = currentIndex;
        return displayData;
      }
    }

    if (triggered) {
      this.lastTriggerIndex = this.writePos - 1;
    }

    this.lastSample = value;
    return null; // No display update
  }

  _checkTrigger(currentValue) {
    if (this.lastSample === null) {
      this.lastSample = currentValue;
      return false;
    }

    const prev = this.lastSample;
    const level = this.triggerLevel;
    const hyst = this.hysteresis;

    let triggered = false;
    if (this.triggerEdge === 'rising') {
      // Previous sample below (level - hysteresis), current at or above level
      triggered = prev < (level - hyst) && currentValue >= level;
    } else {
      // Falling edge
      triggered = prev > (level + hyst) && currentValue <= level;
    }

    this.lastSample = currentValue;
    return triggered;
  }

  _extractFromBuffer(startIndex, count) {
    const data = new Array(count);
    for (let i = 0; i < count; i++) {
      data[i] = this.buffer[(startIndex + i) % this.bufferSize];
    }
    return data;
  }

  // Get detected frequency (requires known sample rate)
  getFrequency(sampleRateHz) {
    if (this.detectedPeriod && this.detectedPeriod > 0) {
      return sampleRateHz / this.detectedPeriod;
    }
    return null;
  }

  // Get detected period in seconds
  getPeriod(sampleRateHz) {
    if (this.detectedPeriod && this.detectedPeriod > 0) {
      return this.detectedPeriod / sampleRateHz;
    }
    return null;
  }

  reset() {
    this.writePos = 0;
    this.sampleCount = 0;
    this.lastSample = null;
    this.lastTriggerIndex = null;
    this.detectedPeriod = null;
    this.armed = true;
  }
}
```

### Integration with Chart.js

The key insight: in trigger mode, you **do not** push/shift data continuously.
Instead, you **replace** the entire dataset when the trigger fires.

```javascript
// ============================================================================
// Triggered Chart Update
// ============================================================================

const trigger = new TriggerEngine({
  triggerLevel: 0.0,
  triggerEdge: 'rising',
  periodsToShow: 2,
  bufferSize: 8192,
});

// Called for each incoming sample (from serial data parsing)
function onSample(value) {
  const displayData = trigger.addSample(value);

  if (displayData !== null) {
    // Trigger fired -- replace chart data wholesale
    const labels = displayData.map((_, i) => i);

    accelChart.data.labels = labels;
    accelChart.data.datasets[0].data = displayData;

    // Use 'none' mode to skip animation (critical for real-time)
    accelChart.update('none');
  }
}
```

### Multi-Channel Trigger

For IMU data with 3 axes (X, Y, Z), you typically trigger on one channel and
display all three aligned to that trigger:

```javascript
// ============================================================================
// Multi-Channel Triggered Display
// ============================================================================

class MultiChannelTrigger {
  constructor(channelCount, options = {}) {
    this.channelCount = channelCount;
    this.buffers = [];
    for (let i = 0; i < channelCount; i++) {
      this.buffers.push(new Float64Array(options.bufferSize || 4096));
    }
    this.trigger = new TriggerEngine(options);
    this.writePos = 0;
    this.bufferSize = options.bufferSize || 4096;
  }

  // values: array of channel values [x, y, z]
  // triggerChannelIndex: which channel to trigger on (default 0)
  addSamples(values, triggerChannelIndex = 0) {
    // Store all channels
    for (let ch = 0; ch < this.channelCount; ch++) {
      this.buffers[ch][this.writePos % this.bufferSize] = values[ch];
    }
    this.writePos++;

    // Trigger only on the selected channel
    const displayData = this.trigger.addSample(values[triggerChannelIndex]);

    if (displayData !== null) {
      // Extract aligned data from all channels
      const period = this.trigger.detectedPeriod;
      const samplesToShow = period * this.trigger.periodsToShow;
      const startIndex = this.writePos - 1 - samplesToShow;

      const result = [];
      for (let ch = 0; ch < this.channelCount; ch++) {
        const chData = new Array(samplesToShow);
        for (let i = 0; i < samplesToShow; i++) {
          chData[i] = this.buffers[ch][(startIndex + i) % this.bufferSize];
        }
        result.push(chData);
      }
      return result; // [xData, yData, zData]
    }
    return null;
  }
}

// Usage with the existing chart:
const imuTrigger = new MultiChannelTrigger(3, {
  triggerLevel: 0.0,
  triggerEdge: 'rising',
  periodsToShow: 2,
  bufferSize: 8192,
});

function updateAccelTriggered(accelX, accelY, accelZ) {
  const result = imuTrigger.addSamples([accelX, accelY, accelZ], 0);

  if (result) {
    const [xData, yData, zData] = result;
    const labels = xData.map((_, i) => i);

    accelChart.data.labels = labels;
    accelChart.data.datasets[0].data = xData;
    accelChart.data.datasets[1].data = yData;
    accelChart.data.datasets[2].data = zData;
    accelChart.update('none');
  }
}
```

### Adjustable Trigger Controls (HTML)

```html
<div class="trigger-controls">
  <label>Trigger Level:
    <input type="range" id="trigger-level" min="-2" max="2" step="0.01" value="0">
    <span id="trigger-level-value">0.00</span>
  </label>
  <label>Edge:
    <select id="trigger-edge">
      <option value="rising">Rising</option>
      <option value="falling">Falling</option>
    </select>
  </label>
  <label>Periods:
    <select id="trigger-periods">
      <option value="1">1</option>
      <option value="2" selected>2</option>
      <option value="3">3</option>
      <option value="5">5</option>
    </select>
  </label>
  <label>Trigger Ch:
    <select id="trigger-channel">
      <option value="0">X</option>
      <option value="1">Y</option>
      <option value="2">Z</option>
    </select>
  </label>
</div>
```

### Trigger Level Visualization

Draw a horizontal line on the chart showing the trigger level using a custom
Chart.js plugin (no annotation plugin needed for this simple case):

```javascript
const triggerLinePlugin = {
  id: 'triggerLine',
  afterDraw(chart, args, options) {
    if (!options.enabled) return;
    const { ctx } = chart;
    const yScale = chart.scales.y;
    const yPixel = yScale.getPixelForValue(options.level);
    const { left, right } = chart.chartArea;

    ctx.save();
    ctx.beginPath();
    ctx.moveTo(left, yPixel);
    ctx.lineTo(right, yPixel);
    ctx.strokeStyle = options.color || '#ffab40';
    ctx.lineWidth = 1;
    ctx.setLineDash([6, 3]);
    ctx.stroke();

    // Small "T" marker on the left edge
    ctx.fillStyle = options.color || '#ffab40';
    ctx.font = '10px monospace';
    ctx.fillText('T', left - 12, yPixel + 3);

    ctx.restore();
  }
};

// Register the plugin
Chart.register(triggerLinePlugin);

// Use in chart options:
const chartOptions = {
  // ... existing options ...
  plugins: {
    triggerLine: {
      enabled: true,
      level: 0.0,
      color: '#ffab40',
    },
  },
};
```

---

## 2. Per-Plot Mode Switching

### Architecture: Plot Controller Class

Each plot (accel, gyro, PWM, etc.) gets its own controller that manages mode
state, data buffers, and the Chart.js instance.

```javascript
// ============================================================================
// Plot Modes
// ============================================================================

const PlotMode = {
  CONTINUOUS: 'continuous',  // Scrolling window (current behavior)
  TRIGGERED:  'triggered',   // Oscilloscope trigger mode
  SINGLE:     'single',      // Single triggered capture, then freeze
  FROZEN:     'frozen',      // Paused, no updates
};

// ============================================================================
// Plot Controller
// ============================================================================

class PlotController {
  constructor(canvasId, options = {}) {
    this.mode = PlotMode.CONTINUOUS;
    this.channelCount = options.channelCount || 3;
    this.maxPoints = options.maxPoints || 100;
    this.sampleRate = options.sampleRate || 100; // Hz, estimated

    // Continuous mode buffer (current behavior)
    this.continuousLabels = [];
    this.continuousData = Array.from({ length: this.channelCount }, () => []);
    this.sampleCounter = 0;

    // Trigger engine for triggered/single modes
    this.trigger = new MultiChannelTrigger(this.channelCount, {
      triggerLevel: options.triggerLevel || 0.0,
      triggerEdge: options.triggerEdge || 'rising',
      periodsToShow: options.periodsToShow || 2,
      bufferSize: options.bufferSize || 8192,
    });

    // Single-shot state
    this.singleCaptured = false;

    // Chart.js instance
    this.chart = this._createChart(canvasId, options);
  }

  _createChart(canvasId, options) {
    const ctx = document.getElementById(canvasId).getContext('2d');
    const colors = options.colors || [
      { border: '#ef5350', background: '#ef535040' },
      { border: '#81c784', background: '#81c78440' },
      { border: '#4fc3f7', background: '#4fc3f740' },
    ];
    const labels = options.channelLabels || ['X', 'Y', 'Z'];

    return new Chart(ctx, {
      type: 'line',
      data: {
        labels: [],
        datasets: colors.slice(0, this.channelCount).map((c, i) => ({
          label: labels[i] || `Ch${i}`,
          data: [],
          borderColor: c.border,
          backgroundColor: c.background,
          borderWidth: 1.5,
          pointRadius: 0,
          tension: 0.2,
        })),
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        animation: false,
        scales: {
          x: { display: false },
          y: {
            grid: { color: '#333' },
            ticks: { color: '#888' },
          },
        },
        plugins: {
          legend: {
            labels: { color: '#888', boxWidth: 12, padding: 8 },
          },
          triggerLine: {
            enabled: false,
            level: 0.0,
            color: '#ffab40',
          },
        },
      },
    });
  }

  setMode(mode) {
    this.mode = mode;

    // Update trigger line visibility
    this.chart.options.plugins.triggerLine.enabled =
      (mode === PlotMode.TRIGGERED || mode === PlotMode.SINGLE);

    if (mode === PlotMode.SINGLE) {
      this.singleCaptured = false;
    }

    if (mode === PlotMode.TRIGGERED || mode === PlotMode.SINGLE) {
      this.trigger.trigger.reset();
    }

    this.chart.update('none');
  }

  setTriggerLevel(level) {
    this.trigger.trigger.triggerLevel = level;
    this.chart.options.plugins.triggerLine.level = level;
    if (this.mode === PlotMode.FROZEN || this.mode === PlotMode.SINGLE) {
      this.chart.update('none'); // Redraw to show new level
    }
  }

  // Main entry point: feed channel values [v0, v1, v2, ...]
  addData(values) {
    if (this.mode === PlotMode.FROZEN) return;
    if (this.mode === PlotMode.SINGLE && this.singleCaptured) return;

    if (this.mode === PlotMode.CONTINUOUS) {
      this._addContinuous(values);
    } else if (this.mode === PlotMode.TRIGGERED || this.mode === PlotMode.SINGLE) {
      this._addTriggered(values);
    }
  }

  _addContinuous(values) {
    this.continuousLabels.push(this.sampleCounter++);
    for (let ch = 0; ch < this.channelCount; ch++) {
      this.continuousData[ch].push(values[ch]);
    }

    // Trim
    if (this.continuousLabels.length > this.maxPoints) {
      this.continuousLabels.shift();
      for (let ch = 0; ch < this.channelCount; ch++) {
        this.continuousData[ch].shift();
      }
    }

    this.chart.data.labels = [...this.continuousLabels];
    for (let ch = 0; ch < this.channelCount; ch++) {
      this.chart.data.datasets[ch].data = [...this.continuousData[ch]];
    }
    this.chart.update('none');
  }

  _addTriggered(values) {
    const triggerCh = this.trigger.trigger.triggerChannel;
    const result = this.trigger.addSamples(values, triggerCh);

    if (result) {
      const labels = result[0].map((_, i) => i);
      this.chart.data.labels = labels;
      for (let ch = 0; ch < this.channelCount; ch++) {
        this.chart.data.datasets[ch].data = result[ch];
      }
      this.chart.update('none');

      if (this.mode === PlotMode.SINGLE) {
        this.singleCaptured = true;
      }
    }
  }

  // Get stats for the readout bar
  getStats() {
    const freq = this.trigger.trigger.getFrequency(this.sampleRate);
    const period = this.trigger.trigger.getPeriod(this.sampleRate);

    // Calculate min/max/RMS from current display data
    const displayData = this.chart.data.datasets[0].data;
    if (!displayData || displayData.length === 0) {
      return { freq: null, period: null, min: null, max: null, rms: null };
    }

    let min = Infinity, max = -Infinity, sumSq = 0;
    for (let i = 0; i < displayData.length; i++) {
      const v = displayData[i];
      if (v < min) min = v;
      if (v > max) max = v;
      sumSq += v * v;
    }
    const rms = Math.sqrt(sumSq / displayData.length);

    return { freq, period, min, max, rms };
  }

  destroy() {
    this.chart.destroy();
  }
}
```

### Mode Selector UI

```html
<div class="plot-mode-bar">
  <select class="plot-mode-select" data-chart="accel">
    <option value="continuous">Continuous</option>
    <option value="triggered">Triggered (N periods)</option>
    <option value="single">Single Capture</option>
    <option value="frozen">Frozen</option>
  </select>
  <button class="rearm-btn" data-chart="accel" title="Re-arm single trigger">
    Re-arm
  </button>
</div>
```

```javascript
document.querySelectorAll('.plot-mode-select').forEach(select => {
  select.addEventListener('change', (e) => {
    const chartId = e.target.dataset.chart;
    const controller = plotControllers[chartId]; // Map of PlotController instances
    controller.setMode(e.target.value);
  });
});

document.querySelectorAll('.rearm-btn').forEach(btn => {
  btn.addEventListener('click', (e) => {
    const chartId = e.target.dataset.chart;
    plotControllers[chartId].setMode(PlotMode.SINGLE);
  });
});
```

---

## 3. Overlay Annotations for Anomaly Markers

### Plugin: chartjs-plugin-annotation

- **Latest version:** 3.1.0 (compatible with Chart.js >= 4.0.0)
- **npm:** `chartjs-plugin-annotation`
- **CDN load (no bundler):**
  ```html
  <script src="https://cdn.jsdelivr.net/npm/chartjs-plugin-annotation@3.1.0/dist/chartjs-plugin-annotation.min.js"></script>
  ```
  Or download to `/lib/chartjs-plugin-annotation.min.js` for offline Tauri use.
- **Registration:** Auto-registers when loaded via script tag, OR manual:
  ```javascript
  Chart.register(window['chartjs-plugin-annotation']);
  ```

### Annotation Types Available

| Type      | Use Case                         |
|-----------|----------------------------------|
| `line`    | Baseline reference, thresholds   |
| `box`     | Anomaly highlight regions        |
| `point`   | Max/min markers, critical points |
| `label`   | Text annotations                 |
| `polygon` | Custom shaped markers            |
| `ellipse` | Region highlights                |

### Working Examples

#### A. Max/Min Markers (Point Annotations)

```javascript
// Dynamically find min/max and create point annotations
function computeMinMaxAnnotations(chart, datasetIndex = 0) {
  const data = chart.data.datasets[datasetIndex].data;
  if (!data || data.length === 0) return {};

  let minVal = Infinity, maxVal = -Infinity;
  let minIdx = 0, maxIdx = 0;

  for (let i = 0; i < data.length; i++) {
    if (data[i] < minVal) { minVal = data[i]; minIdx = i; }
    if (data[i] > maxVal) { maxVal = data[i]; maxIdx = i; }
  }

  return {
    maxPoint: {
      type: 'point',
      xValue: maxIdx,
      yValue: maxVal,
      radius: 5,
      backgroundColor: '#ff6b6b',
      borderColor: '#ff6b6b',
      borderWidth: 2,
      // Label on the point
      label: {
        display: true,
        content: `Max: ${maxVal.toFixed(2)}`,
        position: 'start',
        color: '#ff6b6b',
        font: { size: 10, family: 'monospace' },
        backgroundColor: 'rgba(0,0,0,0.7)',
        padding: 3,
      },
    },
    minPoint: {
      type: 'point',
      xValue: minIdx,
      yValue: minVal,
      radius: 5,
      backgroundColor: '#4ecdc4',
      borderColor: '#4ecdc4',
      borderWidth: 2,
      label: {
        display: true,
        content: `Min: ${minVal.toFixed(2)}`,
        position: 'end',
        color: '#4ecdc4',
        font: { size: 10, family: 'monospace' },
        backgroundColor: 'rgba(0,0,0,0.7)',
        padding: 3,
      },
    },
  };
}
```

#### B. Baseline Reference Lines

```javascript
// Horizontal reference line at y=0 (or any baseline)
const baselineAnnotation = {
  baseline: {
    type: 'line',
    yMin: 0,
    yMax: 0,
    borderColor: 'rgba(255, 255, 255, 0.3)',
    borderWidth: 1,
    borderDash: [4, 4],
    label: {
      display: true,
      content: 'Baseline',
      position: 'start',
      color: 'rgba(255, 255, 255, 0.5)',
      font: { size: 9, family: 'monospace' },
      backgroundColor: 'transparent',
    },
  },
};

// Threshold lines for +-1g on accelerometer
const thresholdAnnotations = {
  upperThreshold: {
    type: 'line',
    yMin: 1.0,
    yMax: 1.0,
    borderColor: 'rgba(255, 171, 64, 0.5)',
    borderWidth: 1,
    borderDash: [3, 3],
  },
  lowerThreshold: {
    type: 'line',
    yMin: -1.0,
    yMax: -1.0,
    borderColor: 'rgba(255, 171, 64, 0.5)',
    borderWidth: 1,
    borderDash: [3, 3],
  },
};
```

#### C. Anomaly Highlight Regions (Colored Bands)

```javascript
// Highlight a region where anomaly was detected
function createAnomalyRegion(startIdx, endIdx, severity = 'warning') {
  const colors = {
    warning: 'rgba(255, 193, 7, 0.15)',   // Yellow band
    critical: 'rgba(244, 67, 54, 0.20)',   // Red band
    info: 'rgba(33, 150, 243, 0.10)',      // Blue band
  };
  const borderColors = {
    warning: 'rgba(255, 193, 7, 0.5)',
    critical: 'rgba(244, 67, 54, 0.5)',
    info: 'rgba(33, 150, 243, 0.3)',
  };

  return {
    type: 'box',
    xMin: startIdx,
    xMax: endIdx,
    backgroundColor: colors[severity] || colors.warning,
    borderColor: borderColors[severity] || borderColors.warning,
    borderWidth: 1,
    label: {
      display: true,
      content: severity.toUpperCase(),
      position: { x: 'center', y: 'start' },
      color: borderColors[severity],
      font: { size: 9, family: 'monospace', weight: 'bold' },
      backgroundColor: 'rgba(0,0,0,0.5)',
      padding: 2,
    },
  };
}
```

#### D. Critical Point Markers

```javascript
// Vertical line at a specific sample index
function createCriticalPointMarker(index, label, color = '#ff6b6b') {
  return {
    type: 'line',
    xMin: index,
    xMax: index,
    borderColor: color,
    borderWidth: 2,
    borderDash: [2, 2],
    label: {
      display: true,
      content: label,
      position: 'start',
      rotation: -90,
      color: color,
      font: { size: 9, family: 'monospace' },
      backgroundColor: 'rgba(0,0,0,0.7)',
      padding: 2,
    },
  };
}
```

#### E. Putting It All Together: Dynamic Annotation Updates

The critical pattern for real-time: update the annotation config object
**in-place**, then call `chart.update('none')`.

```javascript
function updateAnnotations(chart, displayData) {
  const annotations = {};

  // Always show baseline
  annotations.baseline = baselineAnnotation.baseline;

  // Compute and show min/max
  const minMax = computeMinMaxAnnotations(chart, 0);
  Object.assign(annotations, minMax);

  // Detect and show anomaly regions
  const anomalies = detectAnomalies(displayData); // your detection logic
  anomalies.forEach((a, i) => {
    annotations[`anomaly_${i}`] = createAnomalyRegion(
      a.startIdx, a.endIdx, a.severity
    );
  });

  // Apply: replace the entire annotations object
  chart.options.plugins.annotation.annotations = annotations;
  // Note: chart.update('none') should be called ONCE after all data + annotation
  // changes are applied, not here separately
}

// In your data update loop:
function onNewDisplayData(chart, data) {
  chart.data.labels = data.labels;
  chart.data.datasets[0].data = data.values;
  updateAnnotations(chart, data.values);
  chart.update('none'); // Single update call
}
```

### Performance Note on Annotations

Each annotation is rendered per frame. Keep the total count under ~20 per chart
for smooth 60fps updates. Remove old annotations rather than accumulating them.
Point annotations are cheaper than box annotations. Line annotations are
cheapest.

---

## 4. Sparkline Mini-Graphs

### Option A: Chart.js Minimal Configuration (Recommended)

Chart.js can render efficient sparklines by stripping all visual chrome. Since
the app already includes Chart.js, this avoids adding another dependency.

```javascript
// ============================================================================
// Sparkline Factory
// ============================================================================

function createSparkline(canvasId, color = '#81c784', maxPoints = 60) {
  const ctx = document.getElementById(canvasId).getContext('2d');

  return new Chart(ctx, {
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
          above: color + '20', // Very faint fill
        },
      }],
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      animation: false,
      // Strip all chrome for sparkline appearance
      scales: {
        x: { display: false },
        y: { display: false },
      },
      plugins: {
        legend: { display: false },
        tooltip: { enabled: false },
      },
      layout: {
        padding: 0,
      },
      elements: {
        line: {
          borderCapStyle: 'round',
        },
      },
    },
  });
}

// Update sparkline with new stat value
function updateSparkline(sparkline, value, maxPoints = 60) {
  sparkline.data.labels.push('');
  sparkline.data.datasets[0].data.push(value);

  if (sparkline.data.labels.length > maxPoints) {
    sparkline.data.labels.shift();
    sparkline.data.datasets[0].data.shift();
  }

  sparkline.update('none');
}
```

#### Sparkline HTML/CSS

```html
<div class="sparkline-row">
  <div class="sparkline-item">
    <span class="sparkline-label">Max</span>
    <canvas id="sparkline-max" class="sparkline-canvas"></canvas>
    <span class="sparkline-value" id="sparkline-max-val">--</span>
  </div>
  <div class="sparkline-item">
    <span class="sparkline-label">Min</span>
    <canvas id="sparkline-min" class="sparkline-canvas"></canvas>
    <span class="sparkline-value" id="sparkline-min-val">--</span>
  </div>
  <div class="sparkline-item">
    <span class="sparkline-label">Mean</span>
    <canvas id="sparkline-mean" class="sparkline-canvas"></canvas>
    <span class="sparkline-value" id="sparkline-mean-val">--</span>
  </div>
  <div class="sparkline-item">
    <span class="sparkline-label">RMS</span>
    <canvas id="sparkline-rms" class="sparkline-canvas"></canvas>
    <span class="sparkline-value" id="sparkline-rms-val">--</span>
  </div>
</div>
```

```css
.sparkline-row {
  display: flex;
  gap: 12px;
  margin-top: 8px;
}

.sparkline-item {
  display: flex;
  align-items: center;
  gap: 6px;
  flex: 1;
  background-color: #16213e;
  border-radius: 3px;
  padding: 4px 8px;
  border: 1px solid #333;
}

.sparkline-label {
  color: #666;
  font-size: 0.75em;
  min-width: 30px;
}

.sparkline-canvas {
  width: 80px !important;
  height: 24px !important;
}

.sparkline-value {
  color: #e0e0e0;
  font-size: 0.8em;
  min-width: 50px;
  text-align: right;
  font-family: 'SF Mono', 'Fira Code', monospace;
}
```

### Option B: Canvas-Only Sparklines (Lighter Weight)

If you want to avoid Chart.js overhead for tiny sparklines, a direct canvas
approach is ~50 lines and extremely fast:

```javascript
// ============================================================================
// Lightweight Canvas Sparkline (no Chart.js dependency)
// ============================================================================

class CanvasSparkline {
  constructor(canvasId, options = {}) {
    this.canvas = document.getElementById(canvasId);
    this.ctx = this.canvas.getContext('2d');
    this.color = options.color || '#81c784';
    this.fillColor = options.fillColor || (this.color + '20');
    this.maxPoints = options.maxPoints || 60;
    this.data = [];
  }

  addValue(value) {
    this.data.push(value);
    if (this.data.length > this.maxPoints) {
      this.data.shift();
    }
    this._draw();
  }

  _draw() {
    const { ctx, canvas, data, color, fillColor } = this;
    const w = canvas.width;
    const h = canvas.height;

    ctx.clearRect(0, 0, w, h);

    if (data.length < 2) return;

    const min = Math.min(...data);
    const max = Math.max(...data);
    const range = max - min || 1;
    const stepX = w / (data.length - 1);

    // Draw fill
    ctx.beginPath();
    ctx.moveTo(0, h);
    for (let i = 0; i < data.length; i++) {
      const x = i * stepX;
      const y = h - ((data[i] - min) / range) * (h - 2) - 1;
      ctx.lineTo(x, y);
    }
    ctx.lineTo((data.length - 1) * stepX, h);
    ctx.fillStyle = fillColor;
    ctx.fill();

    // Draw line
    ctx.beginPath();
    for (let i = 0; i < data.length; i++) {
      const x = i * stepX;
      const y = h - ((data[i] - min) / range) * (h - 2) - 1;
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.strokeStyle = color;
    ctx.lineWidth = 1;
    ctx.stroke();
  }
}
```

### Option C: Dedicated Sparkline Libraries

| Library       | Size   | Notes                                      |
|---------------|--------|--------------------------------------------|
| Peity         | ~2 KB  | jQuery dependency -- not suitable           |
| sparkline.js  | ~4 KB  | SVG-based, no dependencies, good           |
| Sparkline     | ~1 KB  | Canvas-based, minimal                      |

**Recommendation:** Use **Option A** (Chart.js sparklines) for consistency with
the rest of the app, or **Option B** (canvas-only) if you need maximum
performance with many sparklines. Avoid adding external sparkline libraries
since Chart.js or raw canvas covers the need.

---

## 5. Color-Coded Signal State

### Approach: Segment-Based Coloring (Chart.js 4 Native)

Chart.js 4 has a powerful `segment` style option that changes the line color per
segment based on a function. This is the best approach -- no plugins needed.

```javascript
// ============================================================================
// Signal Health Thresholds
// ============================================================================

const SignalHealth = {
  NORMAL: 'normal',
  WARNING: 'warning',
  CRITICAL: 'critical',
};

// Define thresholds per signal type
const healthThresholds = {
  accel: {
    warningMin: -1.5,
    warningMax: 1.5,
    criticalMin: -2.0,
    criticalMax: 2.0,
  },
  gyro: {
    warningMin: -250,
    warningMax: 250,
    criticalMin: -500,
    criticalMax: 500,
  },
};

const healthColors = {
  normal:   '#81c784', // Green
  warning:  '#ffd54f', // Yellow
  critical: '#ef5350', // Red
};

// ============================================================================
// Segment Style Function
// ============================================================================

// This function is called by Chart.js for EVERY line segment.
// p0 and p1 are the parsed data values at segment endpoints.
function getSegmentColor(ctx, thresholds) {
  const value = ctx.p1.parsed.y; // Use the endpoint value

  if (value < thresholds.criticalMin || value > thresholds.criticalMax) {
    return healthColors.critical;
  }
  if (value < thresholds.warningMin || value > thresholds.warningMax) {
    return healthColors.warning;
  }
  return healthColors.normal;
}

// ============================================================================
// Dataset Configuration with Segment Coloring
// ============================================================================

const accelDatasetWithHealth = {
  label: 'X',
  data: [],
  borderWidth: 1.5,
  pointRadius: 0,
  tension: 0.2,
  // Segment-based coloring -- the key feature
  segment: {
    borderColor: (ctx) => getSegmentColor(ctx, healthThresholds.accel),
  },
  // Default color (used for legend)
  borderColor: healthColors.normal,
};
```

### Alternative: Whole-Dataset Color Change

If you want the **entire line** to change color based on current state (not
per-segment), update the dataset color directly:

```javascript
function updateDatasetHealthColor(chart, datasetIndex, currentValue, thresholds) {
  let color;
  if (currentValue < thresholds.criticalMin || currentValue > thresholds.criticalMax) {
    color = healthColors.critical;
  } else if (currentValue < thresholds.warningMin || currentValue > thresholds.warningMax) {
    color = healthColors.warning;
  } else {
    color = healthColors.normal;
  }

  const dataset = chart.data.datasets[datasetIndex];
  if (dataset.borderColor !== color) {
    dataset.borderColor = color;
    dataset.backgroundColor = color + '40';
    // No need to call chart.update() separately -- it will be called
    // in the regular data update cycle
  }
}
```

### Combining Both Approaches

Use segment coloring for the main plot (shows exactly where anomalies occurred
in the waveform) and whole-dataset coloring for sparklines (simpler, lighter).

### Background Color Flash on Critical Events

For dramatic visual feedback, briefly flash the chart container background:

```javascript
function flashChartContainer(containerId, severity) {
  const el = document.getElementById(containerId);
  const flashColors = {
    warning: '#3d3000',
    critical: '#3d0000',
  };

  el.style.backgroundColor = flashColors[severity] || '#16213e';
  setTimeout(() => {
    el.style.backgroundColor = '#16213e';
  }, 200);
}
```

---

## 6. Frequency/Period Readout Bar

### Recommendation: Plain HTML Elements Synced with Chart Updates

Using HTML elements below each chart is simpler, more flexible, and more
performant than Chart.js subtitle plugins. HTML text rendering is free (no
canvas redraws). You can use CSS for styling, and the DOM update is trivial.

A Chart.js subtitle plugin would require a canvas redraw to update text, which
is wasteful.

### Implementation

```html
<div class="chart-container" id="accel-container">
  <h3>Accelerometer (g)</h3>
  <canvas id="accel-chart"></canvas>
  <!-- Readout bar -->
  <div class="readout-bar" id="accel-readout">
    <span class="readout-item">
      <span class="readout-key">Period:</span>
      <span class="readout-val" id="accel-period">--</span>
    </span>
    <span class="readout-item">
      <span class="readout-key">Freq:</span>
      <span class="readout-val" id="accel-freq">--</span>
    </span>
    <span class="readout-item">
      <span class="readout-key">Min:</span>
      <span class="readout-val" id="accel-min">--</span>
    </span>
    <span class="readout-item">
      <span class="readout-key">Max:</span>
      <span class="readout-val" id="accel-max">--</span>
    </span>
    <span class="readout-item">
      <span class="readout-key">RMS:</span>
      <span class="readout-val" id="accel-rms">--</span>
    </span>
  </div>
</div>
```

```css
.readout-bar {
  display: flex;
  gap: 12px;
  padding: 6px 10px;
  margin-top: 6px;
  background-color: #0d1117;
  border-radius: 3px;
  border: 1px solid #2a2a3a;
  font-family: 'SF Mono', 'Fira Code', monospace;
  font-size: 0.78em;
  overflow-x: auto;
  white-space: nowrap;
}

.readout-item {
  display: inline-flex;
  gap: 4px;
}

.readout-key {
  color: #666;
}

.readout-val {
  color: #e0e0e0;
  min-width: 55px;
}

/* Color-code the frequency readout based on signal quality */
.readout-val.normal { color: #81c784; }
.readout-val.warning { color: #ffd54f; }
.readout-val.critical { color: #ef5350; }
```

### Readout Update Logic

```javascript
// ============================================================================
// Readout Bar Updater
// ============================================================================

class ReadoutBar {
  constructor(prefix) {
    // prefix: 'accel', 'gyro', etc.
    this.els = {
      period: document.getElementById(`${prefix}-period`),
      freq:   document.getElementById(`${prefix}-freq`),
      min:    document.getElementById(`${prefix}-min`),
      max:    document.getElementById(`${prefix}-max`),
      rms:    document.getElementById(`${prefix}-rms`),
    };

    // Throttle updates to 4Hz (every 250ms) to avoid DOM thrash
    this._lastUpdate = 0;
    this._updateInterval = 250; // ms
  }

  update(stats) {
    const now = performance.now();
    if (now - this._lastUpdate < this._updateInterval) return;
    this._lastUpdate = now;

    if (stats.period !== null) {
      const periodMs = stats.period * 1000;
      this.els.period.textContent = `${periodMs.toFixed(1)}ms`;
    } else {
      this.els.period.textContent = '--';
    }

    if (stats.freq !== null) {
      this.els.freq.textContent = `${stats.freq.toFixed(1)}Hz`;
    } else {
      this.els.freq.textContent = '--';
    }

    if (stats.min !== null) {
      this.els.min.textContent = stats.min.toFixed(3);
    }

    if (stats.max !== null) {
      this.els.max.textContent = stats.max.toFixed(3);
    }

    if (stats.rms !== null) {
      this.els.rms.textContent = stats.rms.toFixed(3);
    }
  }
}

// Usage in the update loop:
const accelReadout = new ReadoutBar('accel');

// After chart update:
const stats = accelPlotController.getStats();
accelReadout.update(stats);
```

### Alternative: Chart.js Custom Plugin for In-Canvas Readout

If you want the readout rendered **inside** the chart canvas (e.g., as a
semi-transparent bar at the bottom), this is possible but adds rendering cost:

```javascript
const readoutPlugin = {
  id: 'readoutBar',
  afterDraw(chart, args, options) {
    if (!options.enabled || !options.stats) return;

    const { ctx, chartArea } = chart;
    const { left, right, bottom } = chartArea;
    const stats = options.stats;

    // Draw background bar
    const barHeight = 18;
    ctx.save();
    ctx.fillStyle = 'rgba(0, 0, 0, 0.7)';
    ctx.fillRect(left, bottom - barHeight, right - left, barHeight);

    // Draw text
    ctx.fillStyle = '#aaa';
    ctx.font = '10px monospace';
    ctx.textBaseline = 'middle';

    const y = bottom - barHeight / 2;
    const parts = [];
    if (stats.period) parts.push(`Period: ${(stats.period*1000).toFixed(1)}ms`);
    if (stats.freq) parts.push(`Freq: ${stats.freq.toFixed(1)}Hz`);
    if (stats.min !== null) parts.push(`Min: ${stats.min.toFixed(2)}`);
    if (stats.max !== null) parts.push(`Max: ${stats.max.toFixed(2)}`);
    if (stats.rms !== null) parts.push(`RMS: ${stats.rms.toFixed(2)}`);

    ctx.fillText(parts.join('  |  '), left + 8, y);
    ctx.restore();
  }
};

Chart.register(readoutPlugin);
```

**Recommendation:** Use the HTML approach. It is simpler, more accessible,
easier to style, and does not cost a canvas redraw. The in-canvas plugin is
only worthwhile if you need the readout to appear in exported chart images.

---

## 7. Performance with Multiple Feature-Rich Plots

### Baseline Performance Characteristics

Chart.js is a Canvas 2D-based library. Each `chart.update()` triggers a full
canvas redraw: clear, compute layouts, draw grid, draw datasets, draw
annotations, draw legends. Performance depends on:

| Factor                    | Impact   | Budget Guidance                      |
|---------------------------|----------|--------------------------------------|
| Number of data points     | High     | < 500 points per dataset             |
| Number of datasets        | Medium   | < 6 per chart                        |
| Number of annotations     | Medium   | < 20 per chart                       |
| Update frequency          | Critical | 10-30Hz is practical, 60Hz is hard   |
| Number of chart instances | High     | 3-4 is fine, 8+ gets expensive       |
| Canvas resolution         | High     | Avoid 4K canvas if not needed        |
| Animation                 | Critical | MUST be disabled for real-time       |

### Practical Benchmarks (Estimated)

For a Tauri desktop app on a modern machine (2020+ hardware):

| Configuration                              | Update Rate | CPU Usage |
|--------------------------------------------|-------------|-----------|
| 2 charts, 100 pts, no annotations          | 60 Hz       | ~5%       |
| 2 charts, 200 pts, 5 annotations each      | 30 Hz       | ~8%       |
| 4 charts, 200 pts, 10 annotations each     | 20 Hz       | ~15%     |
| 4 charts, 500 pts, 20 annotations, segments| 10 Hz       | ~20%     |
| 6 charts, 500 pts, full features           | 5 Hz        | ~25%     |

### Critical Performance Tips

#### 1. Always Use `chart.update('none')`

Already in the existing code. The `'none'` mode flag disables animations,
which is the single biggest performance win.

```javascript
chart.update('none'); // No animation, immediate redraw
```

#### 2. Batch Updates -- Single `update()` Call per Frame

Never call `chart.update()` more than once per animation frame. If you receive
multiple serial data samples between frames, buffer them and update once.

```javascript
// ============================================================================
// Batched Update Scheduler
// ============================================================================

class ChartUpdateScheduler {
  constructor() {
    this.pendingCharts = new Set();
    this.rafId = null;
  }

  scheduleUpdate(chart) {
    this.pendingCharts.add(chart);

    if (this.rafId === null) {
      this.rafId = requestAnimationFrame(() => {
        for (const c of this.pendingCharts) {
          c.update('none');
        }
        this.pendingCharts.clear();
        this.rafId = null;
      });
    }
  }
}

const scheduler = new ChartUpdateScheduler();

// Instead of: chart.update('none')
// Use: scheduler.scheduleUpdate(chart)
```

#### 3. Reduce Points with Decimation

Chart.js 4 has a built-in decimation plugin for large datasets:

```javascript
const chartOptions = {
  // ...
  plugins: {
    decimation: {
      enabled: true,
      algorithm: 'lttb', // Largest Triangle Three Buckets
      samples: 200,      // Reduce to ~200 visible points
    },
  },
  // Decimation requires indexed x-axis
  parsing: false, // For best decimation performance
};
```

However, the built-in decimation only works with `{x, y}` point format and
`indexAxis: 'x'`. For the simpler array format currently used, implement manual
decimation:

```javascript
// Manual LTTB-style decimation for array data
function decimateData(data, targetPoints) {
  if (data.length <= targetPoints) return data;

  const result = [data[0]]; // Always keep first point
  const bucketSize = (data.length - 2) / (targetPoints - 2);

  let prevIndex = 0;
  for (let i = 1; i < targetPoints - 1; i++) {
    const bucketStart = Math.floor((i - 1) * bucketSize) + 1;
    const bucketEnd = Math.min(Math.floor(i * bucketSize) + 1, data.length);

    // Find the point in this bucket that creates the largest triangle
    // with the previous selected point and the average of the next bucket
    const nextBucketStart = Math.floor(i * bucketSize) + 1;
    const nextBucketEnd = Math.min(
      Math.floor((i + 1) * bucketSize) + 1, data.length
    );

    let nextAvg = 0;
    for (let j = nextBucketStart; j < nextBucketEnd; j++) {
      nextAvg += data[j];
    }
    nextAvg /= (nextBucketEnd - nextBucketStart);

    let maxArea = -1;
    let bestIndex = bucketStart;
    for (let j = bucketStart; j < bucketEnd; j++) {
      const area = Math.abs(
        (prevIndex - (nextBucketStart + nextBucketEnd) / 2) * (data[j] - data[prevIndex]) -
        (prevIndex - j) * (nextAvg - data[prevIndex])
      );
      if (area > maxArea) {
        maxArea = area;
        bestIndex = j;
      }
    }

    result.push(data[bestIndex]);
    prevIndex = bestIndex;
  }

  result.push(data[data.length - 1]); // Always keep last point
  return result;
}
```

#### 4. Disable Hidden Overhead

```javascript
const performanceChartOptions = {
  responsive: true,
  maintainAspectRatio: false,
  animation: false,
  // Disable hover interactions (significant CPU savings)
  interaction: {
    mode: null,
  },
  hover: {
    mode: null,
  },
  // Minimize tooltip overhead
  plugins: {
    tooltip: { enabled: false },
    legend: {
      // Keep legend but make it static
      labels: { color: '#888', boxWidth: 12, padding: 8 },
    },
  },
  // Minimize point rendering
  elements: {
    point: { radius: 0, hoverRadius: 0, hitRadius: 0 },
    line: { borderWidth: 1.5 },
  },
  // Disable parsing if using pre-formatted data
  parsing: false,
};
```

#### 5. Throttle Chart Updates Independently from Data Collection

Always collect data at full rate, but throttle chart rendering:

```javascript
class ThrottledPlot {
  constructor(plotController, targetFps = 20) {
    this.controller = plotController;
    this.interval = 1000 / targetFps;
    this.lastRender = 0;
    this.pendingData = [];
  }

  addData(values) {
    // Always store data (for trigger engine, stats, etc.)
    this.controller.addDataToBuffer(values);
    this.pendingData.push(values);

    const now = performance.now();
    if (now - this.lastRender >= this.interval) {
      // Process all pending data, render once
      this.controller.renderLatest();
      this.pendingData = [];
      this.lastRender = now;
    }
  }
}
```

#### 6. Use OffscreenCanvas for Background Rendering (Advanced)

For extreme cases with 6+ charts, render charts in a Web Worker using
OffscreenCanvas. Chart.js 4 does not natively support this, but you can use
uPlot (see section 8) which does. Alternatively, render to an offscreen canvas
and blit to the visible one:

```javascript
// This is more relevant for uPlot than Chart.js.
// Chart.js does not officially support OffscreenCanvas.
// See section 8 for uPlot as an alternative.
```

#### 7. Canvas Size Matters

On HiDPI/Retina displays, the canvas pixel count quadruples. Force lower DPI
for real-time charts:

```javascript
// Before creating the chart, set devicePixelRatio override
Chart.defaults.devicePixelRatio = 1; // Force 1x, not 2x
```

Or per-chart:

```javascript
new Chart(ctx, {
  // ...
  options: {
    devicePixelRatio: 1,
    // ...
  },
});
```

This halves the pixel count on Retina and roughly doubles rendering speed.

### Performance Summary

For the fc_tool use case (3-4 charts with annotations, trigger, color coding):

- Target **20 Hz** chart update rate (sufficient for visual perception)
- Collect serial data at full rate into ring buffers
- Use `requestAnimationFrame` scheduler for batched updates
- Disable animations, tooltips, hover
- Keep annotations under 15 per chart
- Use segment coloring sparingly (prefer whole-dataset color change if possible)
- Set `devicePixelRatio: 1`
- Use 200-300 display points per chart (decimate if buffer is larger)

---

## 8. Library Comparison

### Chart.js 4

| Aspect              | Rating   | Notes                                          |
|---------------------|----------|-------------------------------------------------|
| Ease of use         | +++      | Already in use, rich ecosystem                  |
| Real-time perf      | ++       | Canvas 2D, good up to ~4 charts at 20Hz        |
| Oscilloscope style  | +        | Manual trigger implementation needed            |
| Annotations         | +++      | Excellent plugin-annotation support             |
| Bundle size         | ~60 KB   | Minified+gzipped                                |
| Dark theme          | ++       | Fully configurable                              |
| Community           | +++      | Largest charting community                      |
| Sparklines          | ++       | Good with stripped config                       |

**Verdict:** Good general choice. You already have it. Adding trigger logic and
annotations on top is feasible and the code examples above show how. Performance
is adequate for 3-4 plots at 20Hz with moderate annotations.

### uPlot

| Aspect              | Rating   | Notes                                          |
|---------------------|----------|-------------------------------------------------|
| Ease of use         | ++       | More complex API, less documentation            |
| Real-time perf      | ++++     | ~10x faster than Chart.js for large datasets    |
| Oscilloscope style  | ++       | Better suited due to raw performance            |
| Annotations         | +        | No built-in plugin; manual canvas drawing       |
| Bundle size         | ~10 KB   | Extremely small                                 |
| Dark theme          | ++       | CSS-based theming                               |
| Community           | +        | Smaller but focused                             |
| Sparklines          | +++      | Lightweight enough for inline use               |

**Key advantage:** uPlot can render 100,000+ data points at 60fps. It uses a
columnar data format and is optimized for time-series. If you need to scale
beyond 4 plots or show much longer time windows, uPlot is the natural upgrade.

**Key disadvantage:** No annotation plugin. You would need to draw overlays
manually on the canvas or use a secondary overlay div. The API is less
intuitive than Chart.js.

```javascript
// uPlot minimal example for comparison
const opts = {
  width: 600,
  height: 200,
  series: [
    {},
    { stroke: '#ef5350', width: 1.5, label: 'X' },
    { stroke: '#81c784', width: 1.5, label: 'Y' },
    { stroke: '#4fc3f7', width: 1.5, label: 'Z' },
  ],
  scales: {
    x: { time: false },
  },
  axes: [
    { show: false },
    { stroke: '#888', grid: { stroke: '#333' } },
  ],
};

// Data format: [xValues, y1Values, y2Values, y3Values]
const data = [
  [0, 1, 2, 3, 4],      // x
  [0.1, 0.5, 0.3, 0.8, 0.2],  // series 1
  [0.2, 0.4, 0.6, 0.3, 0.7],  // series 2
  [0.3, 0.1, 0.4, 0.5, 0.9],  // series 3
];

const plot = new uPlot(opts, data, document.getElementById('chart-container'));

// Update: replace data and call setData
plot.setData(newData);
```

**CDN:** `https://cdn.jsdelivr.net/npm/uplot/dist/uPlot.iife.min.js`
plus `https://cdn.jsdelivr.net/npm/uplot/dist/uPlot.min.css`

### SmoothieChart

| Aspect              | Rating   | Notes                                          |
|---------------------|----------|-------------------------------------------------|
| Ease of use         | +++      | Purpose-built for real-time streaming           |
| Real-time perf      | +++      | Very efficient for streaming                    |
| Oscilloscope style  | +        | Designed for scrolling, not triggered display   |
| Annotations         | +        | Horizontal lines only (built-in)                |
| Bundle size         | ~15 KB   | Small                                           |
| Dark theme          | ++       | Built-in dark theme                             |
| Community           | +        | Niche but stable                                |
| Sparklines          | +        | Overkill for sparklines                         |

**Key advantage:** Zero-config real-time scrolling charts. You push data and it
scrolls automatically with smooth interpolation.

**Key disadvantage:** **No oscilloscope trigger mode.** It only does
continuously scrolling time-series. No way to "freeze" on a trigger point and
show aligned periods. Also, very limited annotation support (only horizontal
lines). Not suitable for the full feature set described.

```javascript
// SmoothieChart example
const smoothie = new SmoothieChart({
  grid: { fillStyle: '#16213e', strokeStyle: '#333' },
  labels: { fillStyle: '#888' },
});

const series = new TimeSeries();
smoothie.addTimeSeries(series, {
  strokeStyle: '#ef5350',
  lineWidth: 1.5,
});
smoothie.streamTo(document.getElementById('chart-canvas'), 1000);

// Push data at any rate
setInterval(() => {
  series.append(Date.now(), Math.random());
}, 100);
```

### Flot

| Aspect              | Rating   | Notes                                          |
|---------------------|----------|-------------------------------------------------|
| Ease of use         | ++       | Older API, jQuery dependency                    |
| Real-time perf      | +        | Slower than Chart.js for real-time              |
| Oscilloscope style  | +        | No built-in support                             |
| Annotations         | ++       | Plugin-based markings                           |
| Bundle size         | ~30 KB   | Plus jQuery dependency                          |
| Dark theme          | +        | Manual styling                                  |
| Community           | -        | Effectively unmaintained since ~2014            |

**Verdict:** Not recommended. jQuery dependency, unmaintained, slower than
alternatives. No advantage over Chart.js or uPlot.

### Dygraphs

Worth mentioning as a dark horse option:

| Aspect              | Rating   | Notes                                          |
|---------------------|----------|-------------------------------------------------|
| Ease of use         | ++       | Time-series focused API                         |
| Real-time perf      | +++      | Handles 100K+ points via downsampling           |
| Oscilloscope style  | +        | No trigger support, but fast enough to replot   |
| Annotations         | ++       | Built-in annotation support                     |
| Bundle size         | ~70 KB   | Larger                                          |
| Community           | ++       | Google-backed, well-maintained                  |

### Recommendation Matrix

| Use Case                    | Best Choice                       |
|-----------------------------|-----------------------------------|
| Current fc_tool (3-4 plots) | **Chart.js** (already integrated) |
| Need 6+ plots, 60fps       | **uPlot**                         |
| Simple scrolling only       | SmoothieChart                     |
| Huge datasets (100K+ pts)   | uPlot or Dygraphs                 |
| Full annotation support     | **Chart.js + annotation plugin**  |
| Minimum bundle size         | uPlot (~10KB)                     |

**Final recommendation for fc_tool:** Stay with Chart.js for now. The trigger
engine, mode switching, annotations, and color coding all integrate well.
If you hit performance walls with 4+ charts, consider migrating to uPlot --
but you will need to reimplement annotations manually.

---

## 9. Plugin Compatibility Notes

### chartjs-plugin-annotation

- **Version 3.x** is compatible with **Chart.js >= 4.0.0**
- npm: `chartjs-plugin-annotation@^3.1.0`
- For no-bundler usage, download the UMD build and include as a script tag
  **after** Chart.js:
  ```html
  <script src="/lib/chart.min.js"></script>
  <script src="/lib/chartjs-plugin-annotation.min.js"></script>
  ```
- Auto-registers when loaded via script tag (no manual `Chart.register()` call
  needed, though it does not hurt to call it explicitly)
- Annotation types: `line`, `box`, `point`, `label`, `polygon`, `ellipse`
- Dynamic updates: modify `chart.options.plugins.annotation.annotations` then
  call `chart.update('none')`

### chartjs-plugin-streaming

- **Version 2.x** was compatible with **Chart.js 3.x**
- As of early 2025, there is **no official Chart.js 4.x compatible release**
- The repository at `nagix/chartjs-plugin-streaming` has not been updated for
  Chart.js 4
- **Recommendation: Do NOT use this plugin.** Instead, implement manual data
  management (push/shift arrays) as the existing fc_tool code already does.
  The manual approach is actually simpler and gives more control for the
  oscilloscope trigger mode.

### chartjs-plugin-zoom

- **Version 2.x** is compatible with Chart.js 4
- Useful for pan/zoom on frozen or captured waveforms
- npm: `chartjs-plugin-zoom@^2.0.1`
- Depends on `hammerjs` for touch/mouse gestures
- Consider adding this for inspecting captured single-trigger waveforms

---

## 10. Recommended Architecture

### File Structure

```
fc_tool/src/
  lib/
    chart.min.js                        # Chart.js 4.x (existing)
    chartjs-plugin-annotation.min.js    # Annotation plugin (add)
  main.js                               # App entry, serial, events
  plotter.js                            # New: PlotController, TriggerEngine
  sparklines.js                         # New: Sparkline manager
  styles.css                            # Existing + new styles
  index.html                            # Existing + new UI elements
```

### Data Flow

```
Serial Port (Tauri backend)
  |
  v
Tauri Event: "serial-data"
  |
  v
processSerialData(data)
  |-- parse IMU/PWM/sensor lines
  |
  v
PlotController.addData(values)
  |-- Mode: CONTINUOUS -> push/shift, schedule render
  |-- Mode: TRIGGERED  -> feed TriggerEngine, render on trigger
  |-- Mode: SINGLE     -> same as triggered, freeze after first
  |-- Mode: FROZEN     -> discard (but still collect for stats)
  |
  v
ChartUpdateScheduler
  |-- requestAnimationFrame
  |-- batch all pending chart.update('none') calls
  |
  v
ReadoutBar.update(stats)
  |-- throttled DOM updates (4Hz)
  |
  v
SparklineManager.addStats(stats)
  |-- update sparkline charts (throttled to 2Hz)
```

### Integration with Existing Code

The existing `main.js` code uses a straightforward push/shift pattern for
Chart.js. The recommended migration path:

1. **Extract charting into `plotter.js`** -- move all chart creation and update
   logic into `PlotController` classes
2. **Replace `updateCharts()` calls** with `plotController.addData(values)`
3. **Add mode selector UI** to each chart container
4. **Add readout bars** as HTML below each chart
5. **Add annotation plugin** as a script tag and configure annotations
6. **Add sparklines** below the main chart grid

The trigger engine, mode switching, and readout bar are all pure JavaScript with
no additional dependencies. The only new dependency is `chartjs-plugin-annotation`
for the overlay markers.

### Minimal First Step

If you want to add one feature at a time, the recommended order is:

1. **Readout bar** (HTML only, no dependencies, immediate value)
2. **Color-coded segments** (Chart.js native, no dependencies)
3. **Annotation plugin + baseline/threshold lines** (one new dependency)
4. **Trigger engine** (pure JS, biggest architectural change)
5. **Mode switching UI** (wraps trigger engine + existing continuous mode)
6. **Sparklines** (nice-to-have, not critical)

---

## Appendix: Complete Integrated Example

Below is a condensed but complete example showing how the PlotController
integrates with the existing fc_tool pattern:

```javascript
// ============================================================================
// plotter.js -- Drop-in replacement for charting in main.js
// ============================================================================

// Import trigger engine (or define inline)
// class TriggerEngine { ... }
// class MultiChannelTrigger { ... }

const PlotMode = {
  CONTINUOUS: 'continuous',
  TRIGGERED:  'triggered',
  SINGLE:     'single',
  FROZEN:     'frozen',
};

class PlotController {
  constructor(canvasId, readoutPrefix, options = {}) {
    this.mode = PlotMode.CONTINUOUS;
    this.channelCount = options.channelCount || 3;
    this.maxPoints = options.maxPoints || 200;
    this.sampleRate = options.sampleRate || 100;

    // Buffers
    this.labels = [];
    this.channelData = Array.from({ length: this.channelCount }, () => []);
    this.sampleCounter = 0;

    // Trigger
    this.trigger = new MultiChannelTrigger(this.channelCount, {
      triggerLevel: 0.0,
      triggerEdge: 'rising',
      periodsToShow: 2,
      bufferSize: 8192,
    });
    this.singleCaptured = false;

    // Chart
    this.chart = this._initChart(canvasId, options);

    // Readout
    this.readout = readoutPrefix ? new ReadoutBar(readoutPrefix) : null;

    // Health thresholds
    this.thresholds = options.thresholds || null;
  }

  _initChart(canvasId, opts) {
    const ctx = document.getElementById(canvasId).getContext('2d');
    const colors = opts.colors || [
      { border: '#ef5350', bg: '#ef535040' },
      { border: '#81c784', bg: '#81c78440' },
      { border: '#4fc3f7', bg: '#4fc3f740' },
    ];
    const names = opts.channelLabels || ['X', 'Y', 'Z'];

    const datasets = [];
    for (let i = 0; i < this.channelCount; i++) {
      const ds = {
        label: names[i],
        data: [],
        borderColor: colors[i].border,
        backgroundColor: colors[i].bg,
        borderWidth: 1.5,
        pointRadius: 0,
        tension: 0.2,
      };

      // Add segment coloring if thresholds provided
      if (this.thresholds) {
        const thresh = this.thresholds;
        ds.segment = {
          borderColor: (ctx) => {
            const v = ctx.p1.parsed.y;
            if (v < thresh.criticalMin || v > thresh.criticalMax) return '#ef5350';
            if (v < thresh.warningMin || v > thresh.warningMax) return '#ffd54f';
            return colors[i].border; // normal: use channel color
          },
        };
      }

      datasets.push(ds);
    }

    return new Chart(ctx, {
      type: 'line',
      data: { labels: [], datasets },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        animation: false,
        devicePixelRatio: 1,
        interaction: { mode: null },
        hover: { mode: null },
        scales: {
          x: { display: false },
          y: { grid: { color: '#333' }, ticks: { color: '#888' } },
        },
        plugins: {
          legend: { labels: { color: '#888', boxWidth: 12, padding: 8 } },
          tooltip: { enabled: false },
          triggerLine: { enabled: false, level: 0.0, color: '#ffab40' },
          annotation: { annotations: {} },
        },
      },
    });
  }

  addData(values) {
    if (this.mode === PlotMode.FROZEN) return;
    if (this.mode === PlotMode.SINGLE && this.singleCaptured) return;

    if (this.mode === PlotMode.CONTINUOUS) {
      this._continuousUpdate(values);
    } else {
      this._triggeredUpdate(values);
    }

    // Update readout (throttled internally)
    if (this.readout) {
      this.readout.update(this.getStats());
    }
  }

  _continuousUpdate(values) {
    this.labels.push(this.sampleCounter++);
    for (let ch = 0; ch < this.channelCount; ch++) {
      this.channelData[ch].push(values[ch]);
    }

    while (this.labels.length > this.maxPoints) {
      this.labels.shift();
      for (let ch = 0; ch < this.channelCount; ch++) {
        this.channelData[ch].shift();
      }
    }

    this.chart.data.labels = this.labels;
    for (let ch = 0; ch < this.channelCount; ch++) {
      this.chart.data.datasets[ch].data = this.channelData[ch];
    }

    // Update annotations
    this._updateAnnotations();

    scheduler.scheduleUpdate(this.chart);
  }

  _triggeredUpdate(values) {
    const result = this.trigger.addSamples(values, 0);
    if (result) {
      this.chart.data.labels = result[0].map((_, i) => i);
      for (let ch = 0; ch < this.channelCount; ch++) {
        this.chart.data.datasets[ch].data = result[ch];
      }
      this._updateAnnotations();
      scheduler.scheduleUpdate(this.chart);

      if (this.mode === PlotMode.SINGLE) {
        this.singleCaptured = true;
      }
    }
  }

  _updateAnnotations() {
    const data = this.chart.data.datasets[0].data;
    if (!data || data.length === 0) return;

    const annotations = {};

    // Baseline at y=0
    annotations.baseline = {
      type: 'line',
      yMin: 0, yMax: 0,
      borderColor: 'rgba(255,255,255,0.2)',
      borderWidth: 1,
      borderDash: [4, 4],
    };

    // Min/Max markers (only if we have data)
    let min = Infinity, max = -Infinity, minI = 0, maxI = 0;
    for (let i = 0; i < data.length; i++) {
      if (data[i] < min) { min = data[i]; minI = i; }
      if (data[i] > max) { max = data[i]; maxI = i; }
    }

    annotations.maxPt = {
      type: 'point',
      xValue: maxI, yValue: max,
      radius: 4,
      backgroundColor: '#ff6b6b',
      borderColor: '#ff6b6b',
    };

    annotations.minPt = {
      type: 'point',
      xValue: minI, yValue: min,
      radius: 4,
      backgroundColor: '#4ecdc4',
      borderColor: '#4ecdc4',
    };

    this.chart.options.plugins.annotation.annotations = annotations;
  }

  getStats() {
    const data = this.chart.data.datasets[0].data;
    if (!data || data.length === 0) {
      return { freq: null, period: null, min: null, max: null, rms: null };
    }

    let min = Infinity, max = -Infinity, sumSq = 0;
    for (const v of data) {
      if (v < min) min = v;
      if (v > max) max = v;
      sumSq += v * v;
    }

    return {
      freq: this.trigger.trigger.getFrequency(this.sampleRate),
      period: this.trigger.trigger.getPeriod(this.sampleRate),
      min,
      max,
      rms: Math.sqrt(sumSq / data.length),
    };
  }

  setMode(mode) {
    this.mode = mode;
    this.chart.options.plugins.triggerLine.enabled =
      (mode === PlotMode.TRIGGERED || mode === PlotMode.SINGLE);
    if (mode === PlotMode.SINGLE) this.singleCaptured = false;
    if (mode !== PlotMode.CONTINUOUS) this.trigger.trigger.reset();
    this.chart.update('none');
  }

  setTriggerLevel(level) {
    this.trigger.trigger.triggerLevel = level;
    this.chart.options.plugins.triggerLine.level = level;
    this.chart.update('none');
  }
}

// Global update scheduler
const scheduler = new ChartUpdateScheduler();
```

### Usage in main.js

```javascript
// Replace existing chart initialization with:
const accelPlot = new PlotController('accel-chart', 'accel', {
  channelCount: 3,
  channelLabels: ['X', 'Y', 'Z'],
  maxPoints: 200,
  sampleRate: 100,
  thresholds: {
    warningMin: -1.5, warningMax: 1.5,
    criticalMin: -2.0, criticalMax: 2.0,
  },
});

const gyroPlot = new PlotController('gyro-chart', 'gyro', {
  channelCount: 3,
  channelLabels: ['X', 'Y', 'Z'],
  maxPoints: 200,
  sampleRate: 100,
  thresholds: {
    warningMin: -300, warningMax: 300,
    criticalMin: -500, criticalMax: 500,
  },
});

// Replace existing updateCharts() with:
function updateCharts(imuData) {
  accelPlot.addData([imuData.accel.x, imuData.accel.y, imuData.accel.z]);
  gyroPlot.addData([imuData.gyro.x, imuData.gyro.y, imuData.gyro.z]);
}
```
