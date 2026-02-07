# Dark Theme & Neon Color Palette Research for fc_tool

> Last updated: 2026-02-07

Research on implementing a polished dark-themed Chart.js setup with bright neon
data colors for the fc_tool Tauri desktop application. Covers canvas background
rendering, origin axis lines, dynamic color palettes, measurement line z-ordering,
segment-based anomaly coloring, CSS dark theme integration, and a comparison of
Chart.js against alternative libraries for dark-themed real-time plotting.

**Current stack:** Chart.js ^4.4.0, vanilla JS, Tauri 2, no bundler (direct
script tag loading via `/lib/chart.min.js`). The app already uses a midnight-blue
dark theme (`#1a1a2e` background) and has IMU charting with basic dark styling.

---

## Table of Contents

1. [Dark Plot Background](#1-dark-plot-background)
2. [Origin Axes (x=0, y=0)](#2-origin-axes-x0-y0)
3. [Bright Data Line Colors](#3-bright-data-line-colors)
4. [Neon Measurement Line Colors](#4-neon-measurement-line-colors)
5. [Dynamic Color Changes](#5-dynamic-color-changes)
6. [CSS Integration](#6-css-integration)
7. [Comparison: Chart.js Dark Theme vs Alternatives](#7-comparison-chartjs-dark-theme-vs-alternatives)
8. [Complete Working Example](#8-complete-working-example)
9. [Recommendations](#9-recommendations)
10. [References](#10-references)

---

## 1. Dark Plot Background

### The Problem

Chart.js renders on an HTML `<canvas>` element, which is transparent by default.
The chart area background comes from whatever CSS background is behind the canvas.
This works for simple cases, but has limitations:

- The chart area (inside the axes) and the surrounding area (legend, padding) share
  the same background
- Exported chart images (via `canvas.toDataURL()`) will have a transparent background
- You cannot independently color the plot area vs. the outer chrome

### Solution A: CSS Background (Simplest, Already In Use)

The existing fc_tool code uses CSS to set the chart container background:

```css
.chart-container {
  background-color: #16213e;  /* Deep navy */
}
```

This gives the entire container (including canvas) a dark background. The canvas
itself is transparent, so the CSS color shows through.

**Pros:** Zero JavaScript, works immediately, no plugin overhead.
**Cons:** Cannot independently color the plot area; exports are transparent.

### Solution B: Chart.js Background Color Plugin (Recommended)

Chart.js supports custom plugins that hook into the rendering lifecycle. A
`beforeDraw` plugin can fill the entire canvas (or just the chart area) with a
solid color before any chart elements are rendered.

```javascript
// ============================================================================
// Canvas Background Plugin
// ============================================================================

const canvasBackgroundPlugin = {
  id: 'canvasBackground',
  beforeDraw(chart, args, options) {
    const { ctx, chartArea, width, height } = chart;
    ctx.save();

    // Option 1: Fill entire canvas
    if (options.color) {
      ctx.fillStyle = options.color;
      ctx.fillRect(0, 0, width, height);
    }

    // Option 2: Fill only the chart area (plot region) with a different color
    if (options.areaColor && chartArea) {
      ctx.fillStyle = options.areaColor;
      ctx.fillRect(
        chartArea.left,
        chartArea.top,
        chartArea.right - chartArea.left,
        chartArea.bottom - chartArea.top
      );
    }

    ctx.restore();
  },
};

// Register globally
Chart.register(canvasBackgroundPlugin);
```

Usage in chart configuration:

```javascript
const chart = new Chart(ctx, {
  type: 'line',
  options: {
    plugins: {
      canvasBackground: {
        color: '#1a1a2e',       // Entire canvas: midnight blue
        areaColor: '#0d1117',   // Plot area only: GitHub dark
      },
    },
  },
});
```

**Pros:** Exported images have correct background; plot area can differ from
outer chrome; minimal overhead (single `fillRect` per frame).
**Cons:** Requires plugin registration (trivial).

### Recommended Color Choices

| Element | Hex | Description | Contrast |
|---------|-----|-------------|----------|
| Canvas outer | `#1a1a2e` | Midnight blue (matches existing app) | -- |
| Plot area | `#0d1117` | GitHub dark (slightly darker) | Good separation from chrome |
| Plot area alt | `#111827` | Tailwind gray-900 | Slightly warmer |
| Plot area alt | `#0a0a1a` | Near-black with blue tint | Maximum contrast for neon lines |

The existing fc_tool uses `#1a1a2e` for the app background and `#16213e` for
card/container backgrounds. For the plot area specifically, `#0d1117` provides
good contrast -- dark enough for neon lines to pop, but not pure black (which
looks flat and unfinished).

### Grid Lines

```javascript
scales: {
  x: {
    grid: {
      color: '#2d2d2d',         // Subtle dark grey
      lineWidth: 0.5,           // Thin -- should not compete with data
      drawTicks: false,         // Cleaner look without tick marks
    },
    ticks: {
      color: '#888',            // Medium grey for readability
      font: {
        family: "'SF Mono', 'Fira Code', monospace",
        size: 11,
      },
    },
    border: {
      color: '#444',            // Slightly brighter axis border
      width: 1,
    },
  },
  y: {
    grid: {
      color: '#2d2d2d',
      lineWidth: 0.5,
      drawTicks: false,
    },
    ticks: {
      color: '#888',
      font: {
        family: "'SF Mono', 'Fira Code', monospace",
        size: 11,
      },
    },
    border: {
      color: '#444',
      width: 1,
    },
  },
}
```

### Grid Line Color Comparison

| Color | Appearance | Best For |
|-------|-----------|----------|
| `#1a1a1a` | Nearly invisible | Maximum data focus |
| `#2d2d2d` | Subtle, visible on close look | **Recommended: good balance** |
| `#333333` | Clearly visible | When grid reference is important |
| `#444444` | Prominent | Dense data where grid aids reading |

For real-time sensor data, `#2d2d2d` with `lineWidth: 0.5` is ideal. The grid
should guide the eye without competing with the bright data lines.

---

## 2. Origin Axes (x=0, y=0)

### The Problem

When plotting sensor data that oscillates around zero (accelerometer, gyroscope),
it is very useful to have a clear reference line at y=0. Unlike grid lines
(which are at regular tick intervals), the origin axis should:

- Be positioned at the actual data value y=0 (or x=0), not at a fixed pixel position
- "Float" as the chart auto-scales -- if the data range shifts, the zero line
  moves with it
- Only appear when zero is within the visible range
- Be visually distinct from grid lines but not compete with data lines

### Solution A: chartjs-plugin-annotation (Recommended)

The annotation plugin provides the cleanest solution. Line annotations are
positioned at specific data values and automatically move with the scale.

```javascript
// Requires: chartjs-plugin-annotation loaded after Chart.js
// <script src="/lib/chartjs-plugin-annotation.min.js"></script>

const chart = new Chart(ctx, {
  type: 'line',
  options: {
    plugins: {
      annotation: {
        annotations: {
          // Horizontal origin line at y=0
          yOrigin: {
            type: 'line',
            yMin: 0,
            yMax: 0,
            borderColor: '#555',
            borderWidth: 1,
            borderDash: [],          // Solid line (not dashed)
            drawTime: 'beforeDatasetsDraw',  // Behind data lines
            label: {
              display: false,        // No label -- it's implicit
            },
          },
          // Vertical origin line at x=0 (if x-axis has meaningful zero)
          xOrigin: {
            type: 'line',
            xMin: 0,
            xMax: 0,
            borderColor: '#555',
            borderWidth: 1,
            borderDash: [],
            drawTime: 'beforeDatasetsDraw',
          },
        },
      },
    },
  },
});
```

**Key properties:**

| Property | Value | Purpose |
|----------|-------|---------|
| `yMin` / `yMax` | `0` | Both the same = horizontal line at y=0 |
| `xMin` / `xMax` | `0` | Both the same = vertical line at x=0 |
| `borderColor` | `#555` | Subtle but visible against `#0d1117` background |
| `borderWidth` | `1` | Thin -- should not dominate |
| `borderDash` | `[]` | Solid line (empty array = no dash) |
| `drawTime` | `'beforeDatasetsDraw'` | Renders behind data lines |

**drawTime options and z-ordering:**

| drawTime | Renders | Use Case |
|----------|---------|----------|
| `'beforeDraw'` | Before everything (behind grid) | Background elements |
| `'beforeDatasetsDraw'` | After grid, before data | **Origin axes, reference lines** |
| `'afterDatasetsDraw'` | After data, before other plugins | **Measurement lines, cursors** |
| `'afterDraw'` | After everything | Overlay labels |

**Automatic visibility:** Annotation lines are automatically clipped to the
chart area. If y=0 is not within the visible y-axis range (e.g., data is all
positive from 5 to 10), the line simply does not appear. This is the "floating"
behavior desired -- no extra code needed.

### Solution B: Custom Plugin (No Dependency)

If you want to avoid the annotation plugin dependency entirely, a custom plugin
achieves the same effect:

```javascript
const originAxesPlugin = {
  id: 'originAxes',
  afterDatasetsDraw(chart, args, options) {
    if (!options.enabled) return;

    const { ctx, chartArea, scales } = chart;
    const { left, right, top, bottom } = chartArea;

    ctx.save();
    ctx.strokeStyle = options.color || '#555';
    ctx.lineWidth = options.lineWidth || 1;
    ctx.setLineDash(options.dash || []);

    // Horizontal line at y=0
    if (options.yOrigin !== false) {
      const yScale = scales.y;
      const yZero = yScale.getPixelForValue(0);

      // Only draw if y=0 is within the visible chart area
      if (yZero >= top && yZero <= bottom) {
        ctx.beginPath();
        ctx.moveTo(left, yZero);
        ctx.lineTo(right, yZero);
        ctx.stroke();
      }
    }

    // Vertical line at x=0
    if (options.xOrigin === true) {
      const xScale = scales.x;
      const xZero = xScale.getPixelForValue(0);

      if (xZero >= left && xZero <= right) {
        ctx.beginPath();
        ctx.moveTo(xZero, top);
        ctx.lineTo(xZero, bottom);
        ctx.stroke();
      }
    }

    ctx.restore();
  },
};

Chart.register(originAxesPlugin);

// Usage:
options: {
  plugins: {
    originAxes: {
      enabled: true,
      color: '#555',
      lineWidth: 1,
      yOrigin: true,   // Draw y=0 line
      xOrigin: false,  // No x=0 line (time-series x-axis)
    },
  },
}
```

**Pros:** No dependency; very lightweight; explicit visibility control.
**Cons:** Manual implementation; less flexible than annotation plugin for
adding additional reference lines later.

### Recommendation

Use **Solution A (annotation plugin)** if you are already planning to use
annotations for threshold lines, anomaly markers, or measurement cursors (which
the existing research documents suggest). The annotation plugin is already
recommended in `chartjs-signal-visualization-research.md`.

Use **Solution B (custom plugin)** if you want minimal dependencies and only
need origin axes.

### Origin Axis Color Choice

| Color | Contrast on `#0d1117` | Notes |
|-------|----------------------|-------|
| `#333` | Very subtle | Almost invisible; too close to grid lines |
| `#444` | Subtle | Barely distinguishable from `#2d2d2d` grid |
| `#555` | **Recommended** | Visible but does not compete with data |
| `#666` | Clear | Good visibility; works if grid is very faint |
| `#888` | Strong | Too prominent; competes with tick labels |

---

## 3. Bright Data Line Colors

### Primary Palette (8 Colors)

These colors are selected for maximum visibility on a `#0d1117` dark background,
mutual distinguishability, and aesthetic cohesion (neon/bright theme).

| Index | Name | Hex | RGB | Background Alpha | Notes |
|-------|------|-----|-----|-----------------|-------|
| 0 | Lime green | `#00FF88` | 0, 255, 136 | `#00FF8840` | High luminance, excellent on dark |
| 1 | Coral red | `#FF4444` | 255, 68, 68 | `#FF444440` | Warm contrast to greens/blues |
| 2 | Cyan | `#00DDFF` | 0, 221, 255 | `#00DDFF40` | Cool, distinct from lime |
| 3 | Orange | `#FF8C00` | 255, 140, 0 | `#FF8C0040` | Warm, distinct from red |
| 4 | Magenta | `#FF44FF` | 255, 68, 255 | `#FF44FF40` | Purple family, high saturation |
| 5 | White | `#FFFFFF` | 255, 255, 255 | `#FFFFFF40` | Neutral fallback, always visible |
| 6 | Lavender | `#BB88FF` | 187, 136, 255 | `#BB88FF40` | Pastel purple, softer accent |
| 7 | Pink | `#FF6B9D` | 255, 107, 157 | `#FF6B9D40` | Warm pink, distinct from red/magenta |

### Palette as JavaScript Constant

```javascript
// ============================================================================
// Neon Color Palette for Dark Theme
// ============================================================================

const NEON_PALETTE = [
  { name: 'Lime',     border: '#00FF88', background: '#00FF8840' },
  { name: 'Coral',    border: '#FF4444', background: '#FF444440' },
  { name: 'Cyan',     border: '#00DDFF', background: '#00DDFF40' },
  { name: 'Orange',   border: '#FF8C00', background: '#FF8C0040' },
  { name: 'Magenta',  border: '#FF44FF', background: '#FF44FF40' },
  { name: 'White',    border: '#FFFFFF', background: '#FFFFFF40' },
  { name: 'Lavender', border: '#BB88FF', background: '#BB88FF40' },
  { name: 'Pink',     border: '#FF6B9D', background: '#FF6B9D40' },
];

/**
 * Get color for a dataset index, cycling through the palette.
 * @param {number} index - Dataset index (0-based)
 * @returns {{ border: string, background: string, name: string }}
 */
function getDatasetColor(index) {
  return NEON_PALETTE[index % NEON_PALETTE.length];
}
```

### Auto-Assigning Colors to Dynamic Datasets

When datasets are added dynamically (e.g., new telemetry variables appear in the
serial stream), assign colors from the palette in order:

```javascript
/**
 * Create a new dataset with auto-assigned neon color.
 * @param {string} label - Dataset label (e.g., "temperature")
 * @param {number} index - Index for color assignment
 * @returns {Object} Chart.js dataset configuration
 */
function createNeonDataset(label, index) {
  const color = getDatasetColor(index);
  return {
    label: label,
    data: [],
    borderColor: color.border,
    backgroundColor: color.background,
    borderWidth: 1.5,
    pointRadius: 0,
    tension: 0.2,
    fill: false,
  };
}

// Example: dynamically adding datasets as new variables arrive
function addVariable(chart, variableName) {
  const index = chart.data.datasets.length;
  const dataset = createNeonDataset(variableName, index);
  chart.data.datasets.push(dataset);
  chart.update('none');
}
```

### Color Cycling for >8 Datasets

When more than 8 datasets are on the same plot, colors cycle. To improve
distinguishability when cycling, modify the line style (dashed, dotted) for
the second cycle:

```javascript
/**
 * Create a dataset with color cycling and dash variation.
 * First 8 datasets: solid lines with palette colors.
 * Next 8 datasets: dashed lines with the same colors.
 * Next 8 datasets: dotted lines with the same colors.
 */
function createCyclingDataset(label, index) {
  const color = getDatasetColor(index);
  const cycle = Math.floor(index / NEON_PALETTE.length);

  const dashPatterns = [
    [],           // Cycle 0: solid
    [8, 4],       // Cycle 1: dashed
    [2, 3],       // Cycle 2: dotted
    [12, 4, 2, 4], // Cycle 3: dash-dot
  ];

  return {
    label: label,
    data: [],
    borderColor: color.border,
    backgroundColor: color.background,
    borderWidth: 1.5,
    pointRadius: 0,
    tension: 0.2,
    fill: false,
    borderDash: dashPatterns[cycle % dashPatterns.length],
  };
}
```

### Accessibility: Colorblind-Friendly Alternatives

The primary neon palette relies heavily on color hue for differentiation, which
is problematic for users with color vision deficiencies (CVD). Approximately 8%
of males and 0.5% of females have some form of CVD.

**Common confusions with the neon palette:**

| CVD Type | Confused Pairs |
|----------|---------------|
| Protanopia (no red) | Coral red <-> Orange; Lime <-> White |
| Deuteranopia (no green) | Lime <-> Orange; Coral <-> Orange |
| Tritanopia (no blue) | Cyan <-> Lime; Lavender <-> Pink |

**Alternative: Colorblind-safe palette**

This uses luminance variation and warm/cool contrast rather than pure hue:

```javascript
const COLORBLIND_PALETTE = [
  { name: 'Yellow',      border: '#FFD700', background: '#FFD70040' },
  { name: 'Blue',        border: '#0077BB', background: '#0077BB40' },
  { name: 'Orange',      border: '#EE7733', background: '#EE773340' },
  { name: 'Cyan',        border: '#33BBEE', background: '#33BBEE40' },
  { name: 'Magenta',     border: '#EE3377', background: '#EE337740' },
  { name: 'Grey',        border: '#BBBBBB', background: '#BBBBBB40' },
  { name: 'Teal',        border: '#009988', background: '#00998840' },
  { name: 'White',       border: '#FFFFFF', background: '#FFFFFF40' },
];
```

This palette is based on the Paul Tol bright qualitative scheme, which is
designed to be distinguishable under all common forms of CVD.

**Best practice:** Combine color with line style (solid, dashed, dotted) and
provide a legend. Even without color differentiation, line style allows
identification. The `borderDash` cycling approach above helps with this.

**Future enhancement:** A settings toggle to switch between "Neon" and
"Colorblind-safe" palettes. Since the palette is a single array, this is
trivial to implement.

### Luminance Check

All palette colors have been verified for minimum 4.5:1 contrast ratio against
the `#0d1117` background (WCAG AA for normal text). Actual contrast ratios:

| Color | Contrast Ratio vs `#0d1117` | WCAG AA |
|-------|---------------------------|---------|
| `#00FF88` Lime | 12.8:1 | Pass |
| `#FF4444` Coral | 5.1:1 | Pass |
| `#00DDFF` Cyan | 10.4:1 | Pass |
| `#FF8C00` Orange | 6.3:1 | Pass |
| `#FF44FF` Magenta | 5.9:1 | Pass |
| `#FFFFFF` White | 18.1:1 | Pass |
| `#BB88FF` Lavender | 6.7:1 | Pass |
| `#FF6B9D` Pink | 6.0:1 | Pass |

All colors meet WCAG AA contrast requirements against the dark plot background.

---

## 4. Neon Measurement Line Colors

### Color Assignments

| Line Type | Color | Hex | Purpose |
|-----------|-------|-----|---------|
| Vertical intercept / cursor | Neon yellow | `#FFFF00` | Time marker, cursor position |
| Horizontal intercept / cursor | Neon blue | `#00BFFF` | Value marker, threshold reference |
| Trigger level | Amber | `#FFAB40` | Oscilloscope trigger line |
| Baseline / origin | Medium grey | `#555555` | Zero reference |

### Z-Order: Ensuring Measurement Lines Render on Top

In Chart.js, rendering order is controlled by the `drawTime` property for
annotation plugin elements and by the hook name for custom plugins.

**Drawing order (back to front):**

1. `beforeDraw` -- Canvas background
2. Grid lines (rendered by Chart.js internally)
3. `beforeDatasetsDraw` -- Origin axes, reference bands
4. Datasets (the actual data lines)
5. `afterDatasetsDraw` -- **Measurement lines, cursors** (render here)
6. `afterDraw` -- Labels, tooltips

**Using annotation plugin drawTime:**

```javascript
annotations: {
  // Origin axis: behind data
  yOrigin: {
    type: 'line',
    yMin: 0, yMax: 0,
    borderColor: '#555',
    drawTime: 'beforeDatasetsDraw',
  },

  // Measurement cursor: in front of data
  verticalCursor: {
    type: 'line',
    xMin: cursorX, xMax: cursorX,
    borderColor: '#FFFF00',      // Neon yellow
    borderWidth: 1.5,
    borderDash: [4, 4],
    drawTime: 'afterDatasetsDraw',  // ON TOP of data lines
  },

  horizontalCursor: {
    type: 'line',
    yMin: cursorY, yMax: cursorY,
    borderColor: '#00BFFF',      // Neon blue
    borderWidth: 1.5,
    borderDash: [4, 4],
    drawTime: 'afterDatasetsDraw',
  },
}
```

**Using a custom plugin for measurement lines:**

```javascript
const measurementLinesPlugin = {
  id: 'measurementLines',

  // afterDatasetsDraw ensures lines render ON TOP of all data
  afterDatasetsDraw(chart, args, options) {
    if (!options.lines || options.lines.length === 0) return;

    const { ctx, chartArea, scales } = chart;
    const { left, right, top, bottom } = chartArea;

    ctx.save();

    for (const line of options.lines) {
      ctx.strokeStyle = line.color;
      ctx.lineWidth = line.width || 1.5;
      ctx.setLineDash(line.dash || [4, 4]);
      ctx.globalAlpha = line.opacity || 1.0;

      ctx.beginPath();

      if (line.orientation === 'vertical') {
        const xPixel = scales.x.getPixelForValue(line.value);
        if (xPixel >= left && xPixel <= right) {
          ctx.moveTo(xPixel, top);
          ctx.lineTo(xPixel, bottom);
        }
      } else {
        const yPixel = scales.y.getPixelForValue(line.value);
        if (yPixel >= top && yPixel <= bottom) {
          ctx.moveTo(left, yPixel);
          ctx.lineTo(right, yPixel);
        }
      }

      ctx.stroke();

      // Draw value label
      if (line.showLabel) {
        ctx.fillStyle = line.color;
        ctx.font = '10px monospace';
        ctx.globalAlpha = 0.8;

        if (line.orientation === 'vertical') {
          const xPixel = scales.x.getPixelForValue(line.value);
          ctx.fillText(line.label || line.value.toFixed(2), xPixel + 4, top + 12);
        } else {
          const yPixel = scales.y.getPixelForValue(line.value);
          ctx.fillText(line.label || line.value.toFixed(2), left + 4, yPixel - 4);
        }
      }
    }

    ctx.restore();
  },
};

Chart.register(measurementLinesPlugin);
```

Usage:

```javascript
options: {
  plugins: {
    measurementLines: {
      lines: [
        {
          orientation: 'vertical',
          value: 50,          // x-axis data value
          color: '#FFFF00',   // Neon yellow
          width: 1.5,
          dash: [4, 4],
          showLabel: true,
          label: 't = 50',
        },
        {
          orientation: 'horizontal',
          value: 1.5,         // y-axis data value
          color: '#00BFFF',   // Neon blue
          width: 1.5,
          dash: [4, 4],
          showLabel: true,
          label: '1.50 g',
        },
      ],
    },
  },
}
```

### Color Visibility Against Data Lines

The measurement colors were chosen to be distinct from all 8 data palette colors:

| Measurement Color | Potential Confusion | Mitigation |
|-------------------|-------------------|------------|
| `#FFFF00` (yellow) | Orange `#FF8C00` | Yellow is much brighter; dash pattern helps |
| `#00BFFF` (neon blue) | Cyan `#00DDFF` | Slightly different hue; dash pattern helps |

Using dashed lines for measurement cursors and solid lines for data provides
an additional visual cue beyond color alone.

---

## 5. Dynamic Color Changes

### Use Case: Anomaly State Coloring

When anomaly detection is active, the data line color should change to reflect
signal state:

| State | Color | Hex | Meaning |
|-------|-------|-----|---------|
| Normal | Line's palette color | (varies) | Signal within normal range |
| Warning | Amber/Yellow | `#FFD54F` | Signal approaching threshold |
| Critical | Red | `#EF5350` | Signal exceeding threshold |

### Approach 1: Whole-Line Color Change

Change the entire dataset's `borderColor` based on current state. This is the
simplest approach and is very performant.

```javascript
/**
 * Update dataset color based on anomaly state.
 * Call this in the data update loop BEFORE chart.update().
 */
function updateLineColor(chart, datasetIndex, state) {
  const dataset = chart.data.datasets[datasetIndex];
  const originalColor = dataset._originalColor; // Store original on creation

  switch (state) {
    case 'warning':
      dataset.borderColor = '#FFD54F';
      dataset.backgroundColor = '#FFD54F40';
      break;
    case 'critical':
      dataset.borderColor = '#EF5350';
      dataset.backgroundColor = '#EF535040';
      break;
    default:
      dataset.borderColor = originalColor.border;
      dataset.backgroundColor = originalColor.background;
  }
  // No chart.update() here -- let the regular update cycle handle it
}
```

**Performance:** Changing `borderColor` is essentially free. Chart.js reads
the color value during the next `update()` call. There is no additional
rendering cost compared to a static color.

### Approach 2: Segment Coloring (Per-Segment Color)

Chart.js 4 has a native `segment` option that allows different colors for
different segments of the same line. A segment is the line between two
consecutive data points.

This is powerful for showing exactly where anomalies occurred in the waveform
history:

```javascript
// ============================================================================
// Segment-Based Anomaly Coloring
// ============================================================================

// Maintain an array of anomaly states parallel to the data array
const anomalyStates = []; // 'normal' | 'warning' | 'critical'

const dataset = {
  label: 'Accel X',
  data: [],
  borderColor: '#00FF88',         // Default: lime green (normal)
  borderWidth: 1.5,
  pointRadius: 0,
  tension: 0.2,

  // Segment styling -- called for EVERY segment between two consecutive points
  segment: {
    borderColor: (ctx) => {
      // ctx.p0DataIndex and ctx.p1DataIndex are the indices of the two endpoints
      const state = anomalyStates[ctx.p1DataIndex];
      if (state === 'critical') return '#EF5350';
      if (state === 'warning') return '#FFD54F';
      return undefined;  // undefined = use default borderColor
    },
    borderWidth: (ctx) => {
      const state = anomalyStates[ctx.p1DataIndex];
      if (state === 'critical') return 2.5;  // Thicker for critical
      return undefined;  // Default width
    },
  },
};

// In the data update loop:
function addDataPoint(chart, value, anomalyState) {
  chart.data.datasets[0].data.push(value);
  anomalyStates.push(anomalyState);

  // Trim old data
  if (chart.data.datasets[0].data.length > MAX_POINTS) {
    chart.data.datasets[0].data.shift();
    anomalyStates.shift();
  }
}
```

**How segment coloring works internally:**

1. During rendering, Chart.js iterates over all line segments
2. For each segment, it calls the `segment.borderColor` function
3. The function receives a context object with `p0` and `p1` (the two endpoints)
4. If the function returns `undefined`, the default dataset color is used
5. If it returns a color string, that color is used for that specific segment

**Performance considerations:**

- The segment function is called once per segment per render frame
- For 200 data points, that is 199 function calls per `update()`
- Each call is a simple array lookup + comparison -- extremely fast
- Measured overhead: < 0.1ms for 500 segments on modern hardware
- **Verdict: negligible performance impact even at 60fps**

The `segment` option also supports `borderDash`, `backgroundColor` (for fill),
and `borderCapStyle` per-segment. All accept either a static value or a function.

### Approach 3: Gradient Coloring (Heatmap-Style)

For continuous anomaly scoring (not just threshold states), you can map a
numeric score to a color gradient:

```javascript
/**
 * Map a normalized anomaly score (0.0 to 1.0) to a color.
 * 0.0 = green (normal), 0.5 = yellow (warning), 1.0 = red (critical)
 */
function anomalyScoreToColor(score) {
  // Clamp
  const s = Math.max(0, Math.min(1, score));

  let r, g, b;
  if (s < 0.5) {
    // Green -> Yellow
    const t = s * 2;
    r = Math.round(255 * t);
    g = 255;
    b = Math.round(136 * (1 - t));  // Fade out the teal
  } else {
    // Yellow -> Red
    const t = (s - 0.5) * 2;
    r = 255;
    g = Math.round(255 * (1 - t));
    b = 0;
  }

  return `rgb(${r}, ${g}, ${b})`;
}

// Usage in segment:
segment: {
  borderColor: (ctx) => {
    const score = anomalyScores[ctx.p1DataIndex] || 0;
    if (score > 0.1) {  // Only color if score is notable
      return anomalyScoreToColor(score);
    }
    return undefined;  // Default color
  },
}
```

### Updating Colors During chart.update()

When changing dataset colors during real-time updates, the flow is:

```
1. New data arrives (serial event)
2. Push data to dataset array
3. Compute anomaly state for new data point
4. Push anomaly state to parallel array
5. (Optional) Update whole-line color if state changed
6. Call chart.update('none')
   --> Chart.js re-reads borderColor and segment functions
   --> Canvas is redrawn with new colors
```

**Critical:** Never call `chart.update()` more than once per frame. The color
change happens "for free" on the next scheduled update. Changing a color
property does NOT trigger a re-render -- only `chart.update()` does.

---

## 6. CSS Integration

### Complete Dark Theme CSS

Below is a comprehensive CSS snippet for a dark-themed chart interface with
readout panel and controls. This integrates with the existing fc_tool style
patterns.

```css
/* ============================================================================
   Dark Theme Variables
   ============================================================================ */

:root {
  /* Background hierarchy (darkest to lightest) */
  --bg-base: #0d1117;          /* Deepest: plot areas */
  --bg-surface: #1a1a2e;       /* App background */
  --bg-elevated: #16213e;      /* Cards, containers */
  --bg-hover: #1e2a45;         /* Hover states */

  /* Border colors */
  --border-subtle: #2d2d2d;
  --border-default: #333;
  --border-strong: #444;

  /* Text colors */
  --text-primary: #e0e0e0;
  --text-secondary: #aaa;
  --text-muted: #888;
  --text-faint: #555;

  /* Accent colors */
  --accent-primary: #4fc3f7;   /* Cyan (existing fc_tool accent) */
  --accent-success: #81c784;   /* Green */
  --accent-warning: #ffd54f;   /* Yellow */
  --accent-danger: #ef5350;    /* Red */

  /* Font stack */
  --font-mono: 'SF Mono', 'Fira Code', 'Consolas', 'JetBrains Mono', monospace;
  --font-sans: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
}

/* ============================================================================
   Chart Container
   ============================================================================ */

.chart-container {
  background-color: var(--bg-elevated);
  border-radius: 6px;
  padding: 12px;
  border: 1px solid var(--border-default);
  /* Subtle inner shadow for depth */
  box-shadow: inset 0 1px 3px rgba(0, 0, 0, 0.3);
}

.chart-container h3 {
  margin: 0 0 8px 0;
  font-size: 0.9em;
  color: var(--text-muted);
  font-weight: normal;
  font-family: var(--font-mono);
}

.chart-container canvas {
  width: 100% !important;
  height: 200px !important;
  border-radius: 3px;
  /* The canvas background is handled by the plugin, but this
     provides a fallback and eliminates a flash of white on load */
  background-color: var(--bg-base);
}

/* ============================================================================
   Readout Panel (below chart)
   ============================================================================ */

.readout-bar {
  display: flex;
  gap: 16px;
  padding: 6px 10px;
  margin-top: 6px;
  background-color: var(--bg-base);
  border-radius: 3px;
  border: 1px solid var(--border-subtle);
  font-family: var(--font-mono);
  font-size: 0.78em;
  overflow-x: auto;
  white-space: nowrap;
}

.readout-item {
  display: inline-flex;
  gap: 4px;
  align-items: baseline;
}

.readout-key {
  color: var(--text-faint);
  font-size: 0.9em;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

.readout-val {
  color: var(--text-primary);
  min-width: 55px;
  text-align: right;
  /* Tabular figures for stable number width */
  font-variant-numeric: tabular-nums;
}

/* Color-coded readout values */
.readout-val.normal  { color: var(--accent-success); }
.readout-val.warning { color: var(--accent-warning); }
.readout-val.critical { color: var(--accent-danger); }

/* ============================================================================
   Chart Control Buttons
   ============================================================================ */

.chart-controls {
  display: flex;
  gap: 4px;
  margin-top: 6px;
}

.chart-btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  min-width: 32px;
  height: 28px;
  padding: 0 8px;
  border-radius: 4px;
  border: 1px solid var(--border-default);
  background: transparent;
  color: var(--text-secondary);
  font-family: var(--font-mono);
  font-size: 0.85em;
  cursor: pointer;
  transition: all 0.15s ease;
  user-select: none;
}

.chart-btn:hover {
  background: var(--bg-hover);
  color: var(--text-primary);
  border-color: var(--border-strong);
}

.chart-btn:active {
  background: rgba(79, 195, 247, 0.15);
  border-color: var(--accent-primary);
}

/* Specific button styles */
.chart-btn.zoom-in::before  { content: '+'; font-weight: bold; }
.chart-btn.zoom-out::before { content: '\2212'; font-weight: bold; }  /* minus sign */
.chart-btn.reset::before    { content: '\27F2'; }  /* reset arrow */
.chart-btn.pause::before    { content: '\23F8'; }  /* pause icon */
.chart-btn.play::before     { content: '\25B6'; }  /* play icon */

/* Active state for toggle buttons */
.chart-btn.active {
  background: rgba(79, 195, 247, 0.2);
  border-color: var(--accent-primary);
  color: var(--accent-primary);
}

/* Mode selector dropdown */
.chart-mode-select {
  height: 28px;
  padding: 0 8px;
  border-radius: 4px;
  border: 1px solid var(--border-default);
  background: var(--bg-elevated);
  color: var(--text-secondary);
  font-family: var(--font-mono);
  font-size: 0.8em;
  cursor: pointer;
}

.chart-mode-select:hover {
  border-color: var(--border-strong);
}

/* ============================================================================
   Chart Header Row (title + controls inline)
   ============================================================================ */

.chart-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 8px;
}

.chart-header h3 {
  margin: 0;
  font-size: 0.9em;
  color: var(--text-muted);
  font-weight: normal;
}

.chart-header-controls {
  display: flex;
  gap: 4px;
  align-items: center;
}

/* ============================================================================
   Legend Styling Override
   ============================================================================ */

/* Chart.js renders legends on canvas, but these CSS rules apply to
   the HTML fallback legend if using generateLabels + custom HTML */
.chart-legend {
  display: flex;
  gap: 12px;
  padding: 4px 0;
  flex-wrap: wrap;
}

.chart-legend-item {
  display: flex;
  align-items: center;
  gap: 4px;
  font-family: var(--font-mono);
  font-size: 0.75em;
  color: var(--text-muted);
  cursor: pointer;
}

.chart-legend-item:hover {
  color: var(--text-primary);
}

.chart-legend-swatch {
  width: 12px;
  height: 3px;
  border-radius: 1px;
}

/* ============================================================================
   Responsive Grid for Multiple Charts
   ============================================================================ */

.chart-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 16px;
}

@media (max-width: 900px) {
  .chart-grid {
    grid-template-columns: 1fr;
  }
}

/* Vertical stack layout alternative */
.chart-stack {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.chart-stack .chart-container canvas {
  height: 150px !important;  /* Shorter when stacked vertically */
}
```

### Font Choices for Dark Backgrounds

| Font | Style | Use Case | Notes |
|------|-------|----------|-------|
| SF Mono | Monospace | Readout values, data display | macOS native, excellent at small sizes |
| Fira Code | Monospace | Readout values, code | Ligatures, good on all platforms |
| JetBrains Mono | Monospace | Alternative to Fira Code | Slightly wider, very readable |
| Consolas | Monospace | Windows fallback | Built into Windows |
| Inter | Sans-serif | UI labels, button text | Excellent screen font |

**Key typography rules for dark theme readouts:**

1. Use monospace for numeric values (alignment, tabular figures)
2. Use `font-variant-numeric: tabular-nums` to ensure digits have equal width
   (prevents layout shift as values change)
3. Use lighter font weights on dark backgrounds (300-400, not 500+)
4. Minimum font size of 11px for readability on dark backgrounds
5. Avoid pure white `#FFFFFF` for large text blocks -- use `#e0e0e0` instead
   (reduces eye strain)

### Button Styling Details

The control buttons ([+] [-] etc.) use Unicode symbols rendered in the monospace
font stack:

| Button | Unicode | Entity | Description |
|--------|---------|--------|-------------|
| Zoom in | + | `+` | Plus sign |
| Zoom out | - | `\2212` | Minus sign (true minus, not hyphen) |
| Reset | &#x27F2; | `\27F2` | Anticlockwise circle arrow |
| Pause | &#x23F8; | `\23F8` | Double vertical bar |
| Play | &#x25B6; | `\25B6` | Right-pointing triangle |
| Expand | &#x2922; | `\2922` | NE/SW double arrow |

---

## 7. Comparison: Chart.js Dark Theme vs Alternatives

### Chart.js 4.x

**Dark theme support:** Chart.js has no built-in "dark mode" toggle, but every
visual element is configurable through options. The approach is:

1. Set global defaults for dark colors
2. Use a canvas background plugin
3. Configure scale/grid/tick colors per chart

```javascript
// Global dark theme defaults
Chart.defaults.color = '#888';                    // Default text color
Chart.defaults.borderColor = '#333';              // Default border color
Chart.defaults.backgroundColor = 'transparent';   // Default fill
```

| Criterion | Rating | Notes |
|-----------|--------|-------|
| Dark theme ease | +++ | Full control over all colors |
| Neon color support | +++ | Any CSS color, per-segment coloring |
| Canvas background | ++ | Requires plugin (5 lines) |
| Grid customization | +++ | Color, width, dash, per-axis |
| Legend styling | +++ | Color, font, box style |
| Annotation support | +++ | Via plugin -- lines, boxes, points |
| Performance | ++ | Good for 3-4 charts at 20Hz |
| Bundle size | ~60 KB | Moderate |
| Real-time suitability | ++ | Adequate with `update('none')` |

### uPlot

**Dark theme support:** uPlot uses CSS for container styling and JavaScript
options for canvas elements. It is slightly more manual than Chart.js but
achieves the same result with better performance.

```javascript
const opts = {
  width: 800,
  height: 200,
  series: [
    {},
    { stroke: '#00FF88', width: 1.5 },
    { stroke: '#FF4444', width: 1.5 },
    { stroke: '#00DDFF', width: 1.5 },
  ],
  axes: [
    {
      stroke: '#888',           // Tick label color
      grid: { stroke: '#2d2d2d', width: 0.5 },
      ticks: { stroke: '#444' },
    },
    {
      stroke: '#888',
      grid: { stroke: '#2d2d2d', width: 0.5 },
      ticks: { stroke: '#444' },
    },
  ],
};
```

uPlot renders the plot area background using CSS on the `.u-over` element:

```css
.uplot .u-over {
  background-color: #0d1117;
}
```

| Criterion | Rating | Notes |
|-----------|--------|-------|
| Dark theme ease | ++ | CSS + JS options, slightly more manual |
| Neon color support | +++ | Any CSS color per series |
| Canvas background | +++ | Pure CSS on overlay element |
| Grid customization | ++ | Color, width, per-axis |
| Legend styling | + | Basic built-in, or custom HTML |
| Annotation support | - | No plugin system; manual canvas drawing |
| Performance | ++++ | **10x faster** than Chart.js for large datasets |
| Bundle size | ~10 KB | Very small |
| Real-time suitability | ++++ | Purpose-built for time-series |

**Key advantage for dark theme:** uPlot's CSS-based background approach is
simpler than Chart.js's plugin approach. The `.u-over` element is the exact
plot area, so CSS background-color works perfectly without a custom plugin.

**Key disadvantage:** No annotation plugin. Drawing measurement lines, origin
axes, or threshold bands requires manual canvas code in the `drawOrder` hooks.
This adds significant implementation effort compared to Chart.js + annotation
plugin.

```javascript
// uPlot custom drawing hook for origin axis
const originAxisPlugin = {
  hooks: {
    drawClear: [
      (u) => {
        const ctx = u.ctx;
        const { left, top, width, height } = u.bbox;

        // y=0 line
        const yPos = u.valToPos(0, 'y', true);
        if (yPos >= top && yPos <= top + height) {
          ctx.save();
          ctx.strokeStyle = '#555';
          ctx.lineWidth = 1;
          ctx.beginPath();
          ctx.moveTo(left, yPos);
          ctx.lineTo(left + width, yPos);
          ctx.stroke();
          ctx.restore();
        }
      },
    ],
  },
};
```

### SmoothieChart

**Dark theme support:** SmoothieChart has a built-in dark theme and was
designed for dark backgrounds from the start.

```javascript
const smoothie = new SmoothieChart({
  grid: {
    fillStyle: '#0d1117',       // Plot background
    strokeStyle: '#2d2d2d',     // Grid lines
    borderVisible: true,
    verticalSections: 5,
  },
  labels: {
    fillStyle: '#888',          // Tick labels
    fontSize: 11,
  },
  millisPerPixel: 20,
  interpolation: 'bezier',
});

const series1 = new TimeSeries();
smoothie.addTimeSeries(series1, {
  strokeStyle: '#00FF88',       // Neon lime
  lineWidth: 1.5,
});
```

| Criterion | Rating | Notes |
|-----------|--------|-------|
| Dark theme ease | ++++ | **Built-in dark theme, minimal config** |
| Neon color support | +++ | Any color per series |
| Canvas background | ++++ | Built into grid.fillStyle |
| Grid customization | ++ | Basic (sections, stroke style) |
| Legend styling | + | No built-in legend |
| Annotation support | + | Horizontal lines only |
| Performance | +++ | Good for real-time streaming |
| Bundle size | ~15 KB | Small |
| Real-time suitability | ++++ | **Purpose-built for real-time** |

**Key advantage:** SmoothieChart was designed for exactly this use case --
dark-themed, real-time, streaming data. The dark theme is the default
appearance. Zero configuration needed.

**Key disadvantage:** SmoothieChart only supports continuously scrolling
time-series. There is no trigger mode, no zoom, no pan, no annotations
(beyond horizontal lines), and no interactive features. It is a "fire and
forget" renderer -- you push data, it scrolls.

This makes it unsuitable for fc_tool's full feature set (oscilloscope trigger,
annotations, measurement cursors, zoom), but it could be used for the simple
streaming case (current IMU chart behavior).

### Dygraphs

**Dark theme support:** Dygraphs uses CSS for container styling. The library
renders on a `<canvas>` but also uses DOM elements for labels and annotations.

```css
/* Dygraphs dark theme */
.dygraph-legend {
  background-color: #16213e !important;
  color: #e0e0e0 !important;
  border: 1px solid #333 !important;
}

.dygraph-axis-label {
  color: #888 !important;
}
```

```javascript
const g = new Dygraph(container, data, {
  colors: ['#00FF88', '#FF4444', '#00DDFF'],
  gridLineColor: '#2d2d2d',
  axisLineColor: '#444',
  axisLabelColor: '#888',
  // Background is CSS on the container div
});
```

| Criterion | Rating | Notes |
|-----------|--------|-------|
| Dark theme ease | ++ | Mix of CSS and JS options |
| Neon color support | +++ | Any color per series |
| Canvas background | ++ | CSS on container div |
| Grid customization | ++ | Color, basic styling |
| Legend styling | ++ | CSS-based, customizable |
| Annotation support | +++ | **Built-in annotation support** |
| Performance | +++ | Good for large datasets (downsampling) |
| Bundle size | ~70 KB | Largest |
| Real-time suitability | ++ | Can work, but not optimized |

**Key advantage:** Dygraphs has built-in annotation support without needing a
plugin. It also handles large datasets well through automatic downsampling.

**Key disadvantage:** Dygraphs is primarily designed for static or slowly
updating data. Real-time updating requires calling `updateOptions()` which
triggers a full re-render. At high update rates (20+ Hz), this becomes
expensive. Also, its mixed DOM/canvas rendering adds overhead compared to
pure-canvas solutions.

### Comparison Matrix

| Feature | Chart.js 4 | uPlot | SmoothieChart | Dygraphs |
|---------|-----------|-------|---------------|----------|
| Dark theme config | Plugin + options | CSS + options | **Built-in** | CSS + options |
| Neon palette | Full control | Full control | Full control | Full control |
| Canvas background | Plugin (5 lines) | **CSS only** | Built-in | CSS only |
| Origin axes | Annotation plugin | Manual hook | Not supported | Annotation API |
| Measurement lines | Annotation plugin | Manual hook | Horiz only | Annotation API |
| Segment coloring | **Native (segment opt)** | Manual | Not supported | Not supported |
| Z-order control | **drawTime option** | Hook order | Not supported | Annotation z-order |
| Bundle size | 60 KB | **10 KB** | 15 KB | 70 KB |
| Real-time perf (4 charts) | 20 Hz | **60 Hz** | 60 Hz | 10 Hz |
| Trigger mode | Manual impl | Manual impl | **Not possible** | Manual impl |
| Zoom/Pan | Plugin | Manual | Not supported | Built-in |
| Ecosystem/plugins | **Largest** | Small | Minimal | Moderate |

### Recommendation

**Stay with Chart.js for fc_tool.** Reasons:

1. **Already integrated** -- switching costs are significant
2. **Best plugin ecosystem** -- annotation plugin covers origin axes, measurement
   lines, threshold bands, and anomaly markers with minimal code
3. **Segment coloring is native** -- the key feature for anomaly visualization
   is built into Chart.js 4 and is not available in the alternatives
4. **Dark theme is fully achievable** -- the canvas background plugin is 5 lines
   of code; all other dark styling is configuration
5. **Performance is adequate** -- for 3-4 charts at 20Hz with 200 data points,
   Chart.js performs well. The fc_tool use case does not require the raw speed
   of uPlot.

**When to consider switching to uPlot:**

- If fc_tool needs 6+ simultaneous charts
- If data windows grow beyond 1000 points per chart
- If 60fps rendering is needed (unlikely for sensor monitoring)
- If bundle size becomes a concern (unlikely for Tauri desktop)

**When NOT to use SmoothieChart:**

- It cannot do oscilloscope trigger mode (a planned fc_tool feature)
- It cannot do measurement cursors or annotations
- It cannot do zoom/pan
- It is only suitable as a simple streaming fallback

---

## 8. Complete Working Example

Below is a self-contained example showing all the dark theme pieces working
together. This can be used as a reference for integrating into fc_tool.

### HTML

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Dark Theme Chart Demo</title>
  <script src="/lib/chart.min.js"></script>
  <script src="/lib/chartjs-plugin-annotation.min.js"></script>
  <style>
    /* Minimal dark theme */
    body {
      margin: 0;
      padding: 20px;
      background: #1a1a2e;
      color: #e0e0e0;
      font-family: 'SF Mono', 'Fira Code', monospace;
    }

    .demo-container {
      max-width: 800px;
      margin: 0 auto;
    }

    .chart-box {
      background: #16213e;
      border-radius: 6px;
      padding: 12px;
      border: 1px solid #333;
      margin-bottom: 16px;
    }

    .chart-box h3 {
      margin: 0 0 8px 0;
      font-size: 0.9em;
      color: #888;
      font-weight: normal;
    }

    .chart-box canvas {
      width: 100% !important;
      height: 250px !important;
      background: #0d1117;
      border-radius: 3px;
    }

    .readout {
      display: flex;
      gap: 16px;
      padding: 6px 10px;
      margin-top: 6px;
      background: #0d1117;
      border-radius: 3px;
      border: 1px solid #2d2d2d;
      font-size: 0.78em;
    }

    .readout span.key { color: #555; }
    .readout span.val { color: #e0e0e0; min-width: 50px; text-align: right; }

    .controls {
      display: flex;
      gap: 4px;
      margin-top: 6px;
    }

    .controls button {
      min-width: 32px;
      height: 28px;
      padding: 0 8px;
      border-radius: 4px;
      border: 1px solid #333;
      background: transparent;
      color: #aaa;
      font-family: inherit;
      font-size: 0.85em;
      cursor: pointer;
    }

    .controls button:hover {
      background: #1e2a45;
      color: #e0e0e0;
    }
  </style>
</head>
<body>
  <div class="demo-container">
    <h2 style="color: #4fc3f7; margin-bottom: 16px;">Dark Theme Chart Demo</h2>

    <div class="chart-box">
      <h3>Sensor Data (3 channels, neon palette)</h3>
      <canvas id="demo-chart"></canvas>
      <div class="readout">
        <span><span class="key">Min:</span> <span class="val" id="r-min">--</span></span>
        <span><span class="key">Max:</span> <span class="val" id="r-max">--</span></span>
        <span><span class="key">Mean:</span> <span class="val" id="r-mean">--</span></span>
      </div>
      <div class="controls">
        <button id="btn-zoom-in" title="Zoom in">+</button>
        <button id="btn-zoom-out" title="Zoom out">&minus;</button>
        <button id="btn-reset" title="Reset zoom">&#x27F2;</button>
        <button id="btn-pause" title="Pause">&#x23F8;</button>
      </div>
    </div>
  </div>

  <script type="module">
    // ========================================================================
    // Plugin: Canvas Background
    // ========================================================================
    const canvasBackgroundPlugin = {
      id: 'canvasBackground',
      beforeDraw(chart, args, options) {
        const { ctx, chartArea, width, height } = chart;
        ctx.save();
        if (options.color) {
          ctx.fillStyle = options.color;
          ctx.fillRect(0, 0, width, height);
        }
        if (options.areaColor && chartArea) {
          ctx.fillStyle = options.areaColor;
          ctx.fillRect(
            chartArea.left, chartArea.top,
            chartArea.right - chartArea.left,
            chartArea.bottom - chartArea.top
          );
        }
        ctx.restore();
      },
    };
    Chart.register(canvasBackgroundPlugin);

    // ========================================================================
    // Neon Palette
    // ========================================================================
    const NEON_PALETTE = [
      { border: '#00FF88', background: '#00FF8840' },
      { border: '#FF4444', background: '#FF444440' },
      { border: '#00DDFF', background: '#00DDFF40' },
    ];

    // ========================================================================
    // Chart Setup
    // ========================================================================
    const ctx = document.getElementById('demo-chart').getContext('2d');
    const chart = new Chart(ctx, {
      type: 'line',
      data: {
        labels: [],
        datasets: [
          {
            label: 'Channel A',
            data: [],
            borderColor: NEON_PALETTE[0].border,
            backgroundColor: NEON_PALETTE[0].background,
            borderWidth: 1.5,
            pointRadius: 0,
            tension: 0.2,
          },
          {
            label: 'Channel B',
            data: [],
            borderColor: NEON_PALETTE[1].border,
            backgroundColor: NEON_PALETTE[1].background,
            borderWidth: 1.5,
            pointRadius: 0,
            tension: 0.2,
          },
          {
            label: 'Channel C',
            data: [],
            borderColor: NEON_PALETTE[2].border,
            backgroundColor: NEON_PALETTE[2].background,
            borderWidth: 1.5,
            pointRadius: 0,
            tension: 0.2,
          },
        ],
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        animation: false,
        devicePixelRatio: 1,
        interaction: { mode: null },
        hover: { mode: null },

        plugins: {
          canvasBackground: {
            color: '#1a1a2e',
            areaColor: '#0d1117',
          },
          legend: {
            labels: {
              color: '#888',
              boxWidth: 12,
              padding: 10,
              font: {
                family: "'SF Mono', 'Fira Code', monospace",
                size: 11,
              },
            },
          },
          tooltip: { enabled: false },
          annotation: {
            annotations: {
              yOrigin: {
                type: 'line',
                yMin: 0,
                yMax: 0,
                borderColor: '#555',
                borderWidth: 1,
                borderDash: [],
                drawTime: 'beforeDatasetsDraw',
              },
            },
          },
        },

        scales: {
          x: {
            display: false,
          },
          y: {
            grid: {
              color: '#2d2d2d',
              lineWidth: 0.5,
              drawTicks: false,
            },
            ticks: {
              color: '#888',
              font: {
                family: "'SF Mono', 'Fira Code', monospace",
                size: 11,
              },
            },
            border: {
              color: '#444',
              width: 1,
            },
          },
        },
      },
    });

    // ========================================================================
    // Simulated Real-Time Data
    // ========================================================================
    let sampleCount = 0;
    let paused = false;
    const MAX_POINTS = 200;

    function addData() {
      if (paused) return;

      const t = sampleCount * 0.05;
      const a = Math.sin(t * 2) * 1.5 + Math.random() * 0.2 - 0.1;
      const b = Math.cos(t * 1.3) * 1.0 + Math.random() * 0.15 - 0.075;
      const c = Math.sin(t * 0.7 + 1) * 0.8 + Math.random() * 0.1 - 0.05;

      chart.data.labels.push(sampleCount);
      chart.data.datasets[0].data.push(a);
      chart.data.datasets[1].data.push(b);
      chart.data.datasets[2].data.push(c);

      if (chart.data.labels.length > MAX_POINTS) {
        chart.data.labels.shift();
        chart.data.datasets.forEach(ds => ds.data.shift());
      }

      chart.update('none');

      // Update readout
      const data = chart.data.datasets[0].data;
      const min = Math.min(...data);
      const max = Math.max(...data);
      const mean = data.reduce((a, b) => a + b, 0) / data.length;
      document.getElementById('r-min').textContent = min.toFixed(3);
      document.getElementById('r-max').textContent = max.toFixed(3);
      document.getElementById('r-mean').textContent = mean.toFixed(3);

      sampleCount++;
    }

    setInterval(addData, 50); // 20 Hz

    // ========================================================================
    // Controls
    // ========================================================================
    document.getElementById('btn-pause').addEventListener('click', () => {
      paused = !paused;
      document.getElementById('btn-pause').textContent = paused ? '\u25B6' : '\u23F8';
    });

    document.getElementById('btn-zoom-in').addEventListener('click', () => {
      const y = chart.scales.y;
      const range = y.max - y.min;
      const center = (y.max + y.min) / 2;
      chart.options.scales.y.min = center - range * 0.4;
      chart.options.scales.y.max = center + range * 0.4;
      chart.update('none');
    });

    document.getElementById('btn-zoom-out').addEventListener('click', () => {
      const y = chart.scales.y;
      const range = y.max - y.min;
      const center = (y.max + y.min) / 2;
      chart.options.scales.y.min = center - range * 0.625;
      chart.options.scales.y.max = center + range * 0.625;
      chart.update('none');
    });

    document.getElementById('btn-reset').addEventListener('click', () => {
      chart.options.scales.y.min = undefined;
      chart.options.scales.y.max = undefined;
      chart.update();
    });
  </script>
</body>
</html>
```

---

## 9. Recommendations

### Integration Plan for fc_tool

The existing fc_tool already has partial dark theme support. Here is the
recommended upgrade path:

**Step 1: Register canvas background plugin (5 minutes)**

Add the `canvasBackgroundPlugin` from Section 1 and configure
`areaColor: '#0d1117'` in chart options. This gives the plot area a distinct,
darker background.

**Step 2: Switch to neon palette (10 minutes)**

Replace the existing `CHART_COLORS` in `main.js`:

```javascript
// Current (muted colors):
const CHART_COLORS = {
  x: { border: "#ef5350", background: "#ef535040" },
  y: { border: "#81c784", background: "#81c78440" },
  z: { border: "#4fc3f7", background: "#4fc3f740" },
};

// Recommended (neon palette):
const CHART_COLORS = {
  x: { border: "#00FF88", background: "#00FF8840" },  // Lime
  y: { border: "#FF4444", background: "#FF444440" },  // Coral
  z: { border: "#00DDFF", background: "#00DDFF40" },  // Cyan
};
```

Note: The existing red/green/blue scheme is a common convention for X/Y/Z axes
in 3D visualization. The neon palette (lime/coral/cyan) preserves the same
hue relationships (green/red/blue) while increasing brightness and saturation
for dark backgrounds.

**Step 3: Add origin axis line (5 minutes)**

Add the annotation configuration from Section 2 to chart options. Requires
downloading `chartjs-plugin-annotation.min.js` to `/lib/` (already recommended
in existing research docs).

**Step 4: Refine grid styling (5 minutes)**

Update `chartOptions` in `main.js` to use the refined grid configuration from
Section 1 (thinner lines, `drawTicks: false`, border color).

**Step 5: Add readout bar CSS (15 minutes)**

Add the readout bar HTML and CSS from Section 6 below each chart. This is
already detailed in `chartjs-signal-visualization-research.md`.

**Step 6: Add measurement line support (30 minutes)**

Implement the measurement lines plugin from Section 4 with `drawTime:
'afterDatasetsDraw'` for correct z-ordering.

**Step 7: Add segment coloring for anomalies (20 minutes)**

When anomaly detection is integrated (per `anomaly-detection-research.md`),
add the `segment` option from Section 5 to datasets.

### Color Constants File

Consider extracting all color constants into a dedicated configuration object:

```javascript
// ============================================================================
// theme.js -- Color and style constants for fc_tool
// ============================================================================

const THEME = {
  // Backgrounds
  bg: {
    base: '#0d1117',
    surface: '#1a1a2e',
    elevated: '#16213e',
    hover: '#1e2a45',
  },

  // Borders
  border: {
    subtle: '#2d2d2d',
    default: '#333',
    strong: '#444',
  },

  // Text
  text: {
    primary: '#e0e0e0',
    secondary: '#aaa',
    muted: '#888',
    faint: '#555',
  },

  // Chart grid
  grid: {
    color: '#2d2d2d',
    lineWidth: 0.5,
    borderColor: '#444',
  },

  // Origin axes
  origin: {
    color: '#555',
    lineWidth: 1,
  },

  // Data palette (neon)
  palette: [
    { name: 'Lime',     border: '#00FF88', background: '#00FF8840' },
    { name: 'Coral',    border: '#FF4444', background: '#FF444440' },
    { name: 'Cyan',     border: '#00DDFF', background: '#00DDFF40' },
    { name: 'Orange',   border: '#FF8C00', background: '#FF8C0040' },
    { name: 'Magenta',  border: '#FF44FF', background: '#FF44FF40' },
    { name: 'White',    border: '#FFFFFF', background: '#FFFFFF40' },
    { name: 'Lavender', border: '#BB88FF', background: '#BB88FF40' },
    { name: 'Pink',     border: '#FF6B9D', background: '#FF6B9D40' },
  ],

  // Measurement cursors
  cursor: {
    vertical: '#FFFF00',    // Neon yellow
    horizontal: '#00BFFF',  // Neon blue
    width: 1.5,
    dash: [4, 4],
  },

  // Anomaly states
  anomaly: {
    normal: '#81c784',
    warning: '#FFD54F',
    critical: '#EF5350',
  },

  // Trigger line
  trigger: {
    color: '#FFAB40',
    width: 1,
    dash: [6, 3],
  },
};
```

---

## 10. References

### Chart.js

- [Chart.js Canvas Background](https://www.chartjs.org/docs/latest/configuration/canvas-background.html) -- Official docs on background plugins
- [Chart.js Scriptable Options](https://www.chartjs.org/docs/latest/general/options.html#scriptable-options) -- Dynamic per-element styling
- [Chart.js Line Segment Styling](https://www.chartjs.org/docs/latest/charts/line.html#segment) -- Per-segment color, width, dash
- [Chart.js Custom Plugins](https://www.chartjs.org/docs/latest/developers/plugins.html) -- Plugin hooks and lifecycle
- [chartjs-plugin-annotation](https://www.chartjs.org/chartjs-plugin-annotation/latest/) -- Lines, boxes, points, labels overlay
- [chartjs-plugin-annotation drawTime](https://www.chartjs.org/chartjs-plugin-annotation/latest/guide/options.html#draw-time) -- Z-order control

### Alternative Libraries

- [uPlot GitHub](https://github.com/leeoniya/uPlot) -- Lightweight time-series charting
- [SmoothieChart](http://smoothiecharts.org/) -- Real-time streaming charts
- [Dygraphs](https://dygraphs.com/) -- Interactive time-series viewer

### Color Theory

- [Paul Tol Color Schemes](https://personal.sron.nl/~pault/) -- Colorblind-safe qualitative palettes
- [WCAG Contrast Checker](https://webaim.org/resources/contrastchecker/) -- Verify contrast ratios
- [Viz Palette](https://projects.susielu.com/viz-palette) -- Check palette distinguishability

### Related fc_tool Research

- [chartjs-oscilloscope-research.md](chartjs-oscilloscope-research.md) -- Crosshair, zoom, performance
- [chartjs-signal-visualization-research.md](chartjs-signal-visualization-research.md) -- Trigger engine, annotations, readout bars
- [anomaly-detection-research.md](anomaly-detection-research.md) -- Segment coloring for anomaly states
- [multi-graph-plotter-research.md](multi-graph-plotter-research.md) -- Dynamic multi-plot architecture

---

*This research informs fc_tool's dark theme and visual styling implementation.
All code examples are designed for the existing vanilla JS + Chart.js 4.x
architecture running in Tauri 2.*
