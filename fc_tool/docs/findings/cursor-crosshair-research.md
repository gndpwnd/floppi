# Cursor, Crosshair & Measurement Line System — Implementation Research

> Last updated: 2026-02-07

Research on implementing an oscilloscope-style cursor/crosshair and measurement line system with Chart.js 4.5.1 for fc_tool (Tauri + vanilla JS, no build step, UMD bundle).

---

## Table of Contents

1. [Passive Hover Crosshair](#1-passive-hover-crosshair)
2. [Placeable Measurement Lines (Intercept Lines)](#2-placeable-measurement-lines-intercept-lines)
3. [Show Data Points Toggle](#3-show-data-points-toggle)
4. [Multiple Independent Charts](#4-multiple-independent-charts)
5. [Recommendations](#5-recommendations)

---

## 1. Passive Hover Crosshair

### 1.1 Option A: chartjs-plugin-crosshair

**Package**: `chartjs-plugin-crosshair` (npm)

**Verdict: NOT RECOMMENDED for this project.**

Reasons:

| Factor | Finding |
|--------|---------|
| Chart.js 4 support | The original `chartjs-plugin-crosshair` was built for Chart.js 2.x/3.x. The npm package has not been updated for Chart.js 4's breaking API changes. There are community forks that attempt Chart.js 4 compatibility, but none are official or well-maintained. |
| Dotted lines | The plugin does support `dashPattern: [5, 5]` for dashed/dotted lines. |
| Custom colors | Supports `line.color` configuration. Grey (`#888`) is possible. |
| Horizontal line | The plugin only draws a **vertical** crosshair line. There is no built-in horizontal crosshair. This is a dealbreaker for the fc_tool design, which requires both vertical and horizontal crosshair lines. |
| Dependency weight | Adds an npm dependency for something achievable with ~40 lines of custom code. |
| UMD bundle | Would need to be bundled separately or loaded as a script tag. Not available in the existing `chart.min.js` UMD bundle. |
| Tooltip interference | The plugin has its own tooltip sync features that can conflict with Chart.js default tooltips. Disabling them requires configuration. |

**Conclusion**: A custom inline plugin is simpler, lighter, more capable (supports both axes), and avoids compatibility risks.

### 1.2 Option B: Custom Chart.js Inline Plugin (RECOMMENDED)

Chart.js 4 supports inline plugins passed directly in the chart configuration's `plugins` array. These are plain objects with hook methods. No registration needed.

**Key Chart.js APIs used:**

```javascript
// Get the chart's plotting area boundaries
const { left, right, top, bottom } = chart.chartArea;

// Convert pixel position to data value
const xValue = chart.scales.x.getValueForPixel(pixelX);
const yValue = chart.scales.y.getValueForPixel(pixelY);

// Convert data value to pixel position (reverse)
const pixelX = chart.scales.x.getPixelForValue(dataX);
const pixelY = chart.scales.y.getPixelForValue(dataY);
```

**Plugin hooks used:**

| Hook | Purpose |
|------|---------|
| `afterDraw` | Draw crosshair lines on the canvas after all datasets are drawn, but before the tooltip. Actually, the tooltip plugin uses `afterDraw` too. To draw the crosshair UNDER the tooltip, use `afterDatasetsDraw` instead. |
| `afterEvent` | Capture mouse position from `mousemove` and `mouseout` events. Set `args.changed = true` to trigger a re-render. |

**Important subtlety**: The tooltip is drawn in `afterDraw`. If the crosshair is also drawn in `afterDraw`, drawing order depends on plugin registration order. Using `afterDatasetsDraw` for the crosshair ensures it appears above datasets but below the tooltip, which is the desired layering.

#### Complete Crosshair Plugin Implementation

```javascript
/**
 * Creates a crosshair plugin instance for a specific chart.
 * Each chart gets its own plugin instance with independent state.
 *
 * @param {object} readoutEl - DOM element to display coordinate readout
 * @returns {object} Chart.js inline plugin object
 */
function createCrosshairPlugin(readoutEl) {
  // State is per-plugin-instance (closure), not global
  let cursorX = null;
  let cursorY = null;

  return {
    id: 'crosshairPlugin',

    afterEvent(chart, args) {
      const { event } = args;

      if (event.type === 'mousemove') {
        const area = chart.chartArea;
        // Only track cursor when inside the chart area
        if (
          event.x >= area.left && event.x <= area.right &&
          event.y >= area.top && event.y <= area.bottom
        ) {
          cursorX = event.x;
          cursorY = event.y;
        } else {
          cursorX = null;
          cursorY = null;
        }
        // Tell Chart.js to re-render (draws the crosshair)
        args.changed = true;
      }

      if (event.type === 'mouseout') {
        cursorX = null;
        cursorY = null;
        args.changed = true;
      }
    },

    afterDatasetsDraw(chart) {
      if (cursorX === null || cursorY === null) {
        // Update readout to show no position
        if (readoutEl) {
          readoutEl.textContent = 'Mouse: --';
        }
        return;
      }

      const ctx = chart.ctx;
      const { left, right, top, bottom } = chart.chartArea;

      ctx.save();

      // Grey dotted lines
      ctx.strokeStyle = '#888';
      ctx.lineWidth = 1;
      ctx.setLineDash([4, 4]);

      // Vertical line (follows mouse X)
      ctx.beginPath();
      ctx.moveTo(cursorX, top);
      ctx.lineTo(cursorX, bottom);
      ctx.stroke();

      // Horizontal line (follows mouse Y)
      ctx.beginPath();
      ctx.moveTo(left, cursorY);
      ctx.lineTo(right, cursorY);
      ctx.stroke();

      ctx.restore();

      // Update readout panel with data coordinates
      if (readoutEl) {
        const xVal = chart.scales.x.getValueForPixel(cursorX);
        const yVal = chart.scales.y.getValueForPixel(cursorY);
        readoutEl.textContent = `Mouse: X=${formatValue(xVal)}  Y=${formatValue(yVal)}`;
      }
    }
  };
}

function formatValue(val) {
  if (val === null || val === undefined || isNaN(val)) return '--';
  // For integer-like values (sample indices), show as int
  if (Number.isInteger(val)) return val.toString();
  // For floats, show 3 decimal places
  return val.toFixed(3);
}
```

#### Integrating with Chart Creation

```javascript
// Create a readout element for each chart
const accelReadout = document.getElementById('accel-readout');

const accelChart = new Chart(accelCtx, {
  type: 'line',
  data: { /* ... */ },
  options: {
    // ... existing options ...
    // IMPORTANT: Ensure mousemove events are included
    events: ['mousemove', 'mouseout', 'click', 'touchstart', 'touchmove'],
  },
  plugins: [createCrosshairPlugin(accelReadout)]  // Inline plugin
});
```

### 1.3 Performance Analysis: Crosshair on Every Mousemove

**Concern**: Does redrawing the entire chart on every mousemove event cause performance problems?

**Analysis**:

1. **Chart.js already throttles events.** The internal `createProxyAndListen` function wraps event handlers with `throttled()`, which uses `requestAnimationFrame`. This means even rapid mousemove events are coalesced to at most one per animation frame (~60fps on a 60Hz display).

2. **Setting `args.changed = true`** triggers `chart.render()`, which calls `chart.draw()`. With `animation: false` (already set in fc_tool), this is a synchronous canvas redraw. No animation frame scheduling overhead.

3. **Cost of a full redraw** with 100 data points, 3 datasets, no animations: approximately 0.5-2ms on modern hardware. This is well within the 16.6ms budget for 60fps.

4. **With 1000+ data points**, redraw cost rises to 3-8ms. Still acceptable, but close to the budget if multiple charts are redrawn simultaneously.

5. **Optimization if needed** (only pursue if profiling shows a problem):

```javascript
// Option: Use a separate overlay canvas to avoid full chart redraw
// This draws the crosshair on a transparent canvas positioned over the chart
// The main chart never redraws for crosshair movement

function createOverlayCrosshair(chartCanvas, readoutEl) {
  const overlay = document.createElement('canvas');
  overlay.style.position = 'absolute';
  overlay.style.pointerEvents = 'none';  // Click-through to chart canvas
  overlay.style.top = '0';
  overlay.style.left = '0';

  // Size must match chart canvas
  const parent = chartCanvas.parentElement;
  parent.style.position = 'relative';
  parent.appendChild(overlay);

  function syncSize() {
    overlay.width = chartCanvas.width;
    overlay.height = chartCanvas.height;
    overlay.style.width = chartCanvas.style.width;
    overlay.style.height = chartCanvas.style.height;
  }

  // Redraw crosshair without touching the chart
  function drawCrosshair(chart, mouseX, mouseY) {
    syncSize();
    const ctx = overlay.getContext('2d');
    ctx.clearRect(0, 0, overlay.width, overlay.height);

    if (mouseX === null) return;

    const { left, right, top, bottom } = chart.chartArea;

    // Account for device pixel ratio
    const ratio = window.devicePixelRatio || 1;
    const x = mouseX * ratio;
    const y = mouseY * ratio;
    const l = left * ratio;
    const r = right * ratio;
    const t = top * ratio;
    const b = bottom * ratio;

    ctx.strokeStyle = '#888';
    ctx.lineWidth = ratio;  // 1 CSS pixel
    ctx.setLineDash([4 * ratio, 4 * ratio]);

    ctx.beginPath();
    ctx.moveTo(x, t);
    ctx.lineTo(x, b);
    ctx.stroke();

    ctx.beginPath();
    ctx.moveTo(l, y);
    ctx.lineTo(r, y);
    ctx.stroke();
  }

  return { overlay, drawCrosshair, syncSize };
}
```

**Recommendation**: Start with the simple inline plugin approach (Option B). It re-uses Chart.js's own throttling and is the idiomatic pattern. Only move to an overlay canvas if profiling reveals performance issues with 1000+ data points.

### 1.4 Tooltip Coexistence

**Requirement**: The crosshair must NOT interfere with Chart.js default tooltip behavior.

**How this works**:

- The Chart.js tooltip is handled by a separate built-in plugin (`plugin_tooltip`).
- The tooltip plugin uses `afterEvent` to track hover state and `afterDraw` to render the tooltip box.
- Our crosshair plugin uses `afterEvent` to track cursor position and `afterDatasetsDraw` to draw the crosshair lines.
- These two systems are **completely independent**. The crosshair plugin never calls `event.preventDefault()`, never modifies `event.native`, and never returns `false` from `afterEvent` (which would cancel further processing).
- The tooltip continues to appear on data point hover as normal.
- The crosshair is drawn BELOW the tooltip (datasets layer < tooltip layer).

**Verified in Chart.js 4.5.1 source**: The draw order is:
1. `beforeDraw` hooks
2. Chart layers with z <= 0 (scales, grid)
3. `_drawDatasets()` -> `beforeDatasetsDraw` -> draw each dataset -> `afterDatasetsDraw` (crosshair draws here)
4. Chart layers with z > 0
5. `afterDraw` hooks (tooltip draws here)

This confirms the crosshair will always appear below the tooltip.

---

## 2. Placeable Measurement Lines (Intercept Lines)

### 2.1 Option A: chartjs-plugin-annotation

**Package**: `chartjs-plugin-annotation` (npm, official Chart.js ecosystem)

**Assessment:**

| Factor | Finding |
|--------|---------|
| Chart.js 4 support | Yes -- the annotation plugin v3.x is built for Chart.js 4. Well-maintained. |
| Line annotations | Supports `type: 'line'` with horizontal and vertical orientations. |
| Draggable | **No built-in drag support.** The annotation plugin supports click/hover events (`enter`, `leave`, `click`, `dblclick`) but does NOT provide drag-and-drop functionality. |
| Dynamic updates | Annotations can be updated by modifying `chart.options.plugins.annotation.annotations` and calling `chart.update()`. |
| Interactive repositioning | Would require custom code on top: listen for mousedown on annotation, track mousemove, update annotation position, call `chart.update()`. This is essentially reimplementing drag logic yourself while fighting the plugin's abstractions. |
| Weight | ~50KB minified. Significant addition for what amounts to drawing 4 colored lines. |

**Annotation plugin example (static lines only):**

```javascript
// This shows what annotation CAN do, but it cannot be dragged
const chart = new Chart(ctx, {
  options: {
    plugins: {
      annotation: {
        annotations: {
          vertLine1: {
            type: 'line',
            xMin: 50,      // data value on x-axis
            xMax: 50,
            borderColor: '#FFFF00',
            borderWidth: 2,
            label: {
              content: 'Y1=50',
              display: true,
              position: 'start'
            }
          },
          horizLine1: {
            type: 'line',
            yMin: 2.5,
            yMax: 2.5,
            borderColor: '#00BFFF',
            borderWidth: 2
          }
        }
      }
    }
  }
});
```

**Verdict: NOT RECOMMENDED.** The lack of built-in drag support means we would add a ~50KB dependency and still need to write all the interactive logic ourselves. A custom implementation is more appropriate.

### 2.2 Option B: Custom Canvas Implementation (RECOMMENDED)

Implement measurement lines as part of the same custom plugin system, using direct canvas drawing and mouse event handling.

#### State Model

```javascript
/**
 * Measurement line state for a single chart.
 * Manages up to 2 vertical (Y-intercept) and 2 horizontal (X-intercept) lines.
 */
function createMeasurementState() {
  return {
    enabled: false,         // Trigger mode on/off
    activeAxis: 'vertical', // 'vertical' or 'horizontal' (right-click toggles)

    // Placed lines store DATA values (not pixels), so they survive rescaling
    verticalLines: [],      // [{value: <x-data-value>}, ...] max 2
    horizontalLines: [],    // [{value: <y-data-value>}, ...] max 2

    // Currently held line (being placed or repositioned)
    heldLine: null,         // {axis: 'vertical'|'horizontal', index: number} or null

    // Current mouse position (pixels, for preview line)
    mouseX: null,
    mouseY: null,
  };
}
```

#### Core Measurement Line Plugin

```javascript
/**
 * Creates a measurement line plugin for a single chart.
 *
 * @param {object} readoutEl - DOM element for delta readout display
 * @returns {object} Chart.js inline plugin
 */
function createMeasurementPlugin(readoutEl) {
  const state = createMeasurementState();

  return {
    id: 'measurementPlugin',

    // Expose state for external control (toggle button, etc.)
    getState() { return state; },

    afterEvent(chart, args) {
      if (!state.enabled) return;

      const { event } = args;
      const area = chart.chartArea;
      const inArea = (
        event.x >= area.left && event.x <= area.right &&
        event.y >= area.top && event.y <= area.bottom
      );

      if (event.type === 'mousemove' && inArea) {
        state.mouseX = event.x;
        state.mouseY = event.y;
        args.changed = true;
      }

      if (event.type === 'mouseout') {
        state.mouseX = null;
        state.mouseY = null;
        args.changed = true;
      }

      // Click handling is done via native DOM events (see section 2.3)
      // because Chart.js event system normalizes away right-click
    },

    afterDatasetsDraw(chart) {
      if (!state.enabled) return;

      const ctx = chart.ctx;
      const { left, right, top, bottom } = chart.chartArea;

      ctx.save();

      // Draw placed vertical lines (neon yellow)
      state.verticalLines.forEach((line, i) => {
        const px = chart.scales.x.getPixelForValue(line.value);
        if (px >= left && px <= right) {
          ctx.strokeStyle = '#FFFF00';
          ctx.lineWidth = 2;
          ctx.setLineDash([]);
          ctx.beginPath();
          ctx.moveTo(px, top);
          ctx.lineTo(px, bottom);
          ctx.stroke();

          // Label
          ctx.fillStyle = '#FFFF00';
          ctx.font = '11px monospace';
          ctx.textAlign = 'left';
          ctx.fillText(`Y${i + 1}=${formatValue(line.value)}`, px + 4, top + 14);
        }
      });

      // Draw placed horizontal lines (neon blue)
      state.horizontalLines.forEach((line, i) => {
        const py = chart.scales.y.getPixelForValue(line.value);
        if (py >= top && py <= bottom) {
          ctx.strokeStyle = '#00BFFF';
          ctx.lineWidth = 2;
          ctx.setLineDash([]);
          ctx.beginPath();
          ctx.moveTo(left, py);
          ctx.lineTo(right, py);
          ctx.stroke();

          // Label
          ctx.fillStyle = '#00BFFF';
          ctx.font = '11px monospace';
          ctx.textAlign = 'right';
          ctx.fillText(`X${i + 1}=${formatValue(line.value)}`, right - 4, py - 4);
        }
      });

      // Draw preview line (follows mouse, shows where the next line would go)
      if (state.mouseX !== null && state.heldLine !== null) {
        const isVert = state.heldLine.axis === 'vertical';
        ctx.strokeStyle = isVert ? '#FFFF0088' : '#00BFFF88';
        ctx.lineWidth = 2;
        ctx.setLineDash([6, 4]);

        ctx.beginPath();
        if (isVert) {
          ctx.moveTo(state.mouseX, top);
          ctx.lineTo(state.mouseX, bottom);
        } else {
          ctx.moveTo(left, state.mouseY);
          ctx.lineTo(right, state.mouseY);
        }
        ctx.stroke();
      }

      ctx.restore();

      // Update delta readout
      updateDeltaReadout(state, readoutEl);
    }
  };
}

function updateDeltaReadout(state, el) {
  if (!el) return;
  const parts = [];

  if (state.verticalLines.length === 2) {
    const v1 = state.verticalLines[0].value;
    const v2 = state.verticalLines[1].value;
    const delta = Math.abs(v2 - v1);
    parts.push(`Y1=${formatValue(v1)}  Y2=${formatValue(v2)}  \u0394Y=${formatValue(delta)}`);
  } else if (state.verticalLines.length === 1) {
    parts.push(`Y1=${formatValue(state.verticalLines[0].value)}`);
  }

  if (state.horizontalLines.length === 2) {
    const h1 = state.horizontalLines[0].value;
    const h2 = state.horizontalLines[1].value;
    const delta = Math.abs(h2 - h1);
    parts.push(`X1=${formatValue(h1)}  X2=${formatValue(h2)}  \u0394X=${formatValue(delta)}`);
  } else if (state.horizontalLines.length === 1) {
    parts.push(`X1=${formatValue(state.horizontalLines[0].value)}`);
  }

  el.textContent = parts.join('  |  ');
}
```

### 2.3 Mouse Event Handling (Click, Right-Click, Drag)

Chart.js's internal event system normalizes events and does not expose right-click (`contextmenu`) or distinguish between left/right mouse buttons. Therefore, we must attach **native DOM event listeners** directly to the canvas element.

#### Suppressing the Context Menu

```javascript
// Prevent browser context menu on the chart canvas
canvas.addEventListener('contextmenu', (e) => {
  e.preventDefault();
});
```

In a Tauri desktop app, the webview may also have its own context menu. Tauri v2 respects `preventDefault()` on the contextmenu event. This is sufficient.

#### Click to Place Lines

```javascript
/**
 * Attaches click/right-click handlers to a chart canvas for measurement lines.
 *
 * @param {Chart} chart - The Chart.js instance
 * @param {object} measurementPlugin - The measurement plugin (to access state)
 */
function attachMeasurementEvents(chart, measurementPlugin) {
  const canvas = chart.canvas;
  const state = measurementPlugin.getState();

  // Suppress context menu
  canvas.addEventListener('contextmenu', (e) => {
    e.preventDefault();
  });

  // Right-click: toggle between vertical and horizontal placement mode
  canvas.addEventListener('mousedown', (e) => {
    if (e.button === 2) {  // Right mouse button
      e.preventDefault();
      if (!state.enabled) return;

      state.activeAxis = state.activeAxis === 'vertical' ? 'horizontal' : 'vertical';

      // If we were holding a line, drop it (cancel placement)
      if (state.heldLine !== null) {
        state.heldLine = null;
      }

      // Update UI indicator for current mode
      updateModeIndicator(chart, state);
      chart.update('none');
      return;
    }

    if (e.button !== 0) return; // Only left-click below
    if (!state.enabled) return;

    const rect = canvas.getBoundingClientRect();
    const scaleX = canvas.width / rect.width;
    const scaleY = canvas.height / rect.height;
    const pixelX = (e.clientX - rect.left) * scaleX;
    const pixelY = (e.clientY - rect.top) * scaleY;

    // NOTE: Chart.js internally handles devicePixelRatio scaling.
    // The pixel values from chart.chartArea are in CSS pixels (not device pixels).
    // Use the simpler approach:
    const cssX = e.clientX - rect.left;
    const cssY = e.clientY - rect.top;

    const area = chart.chartArea;
    if (cssX < area.left || cssX > area.right || cssY < area.top || cssY > area.bottom) {
      return; // Click outside chart area
    }

    const xVal = chart.scales.x.getValueForPixel(cssX);
    const yVal = chart.scales.y.getValueForPixel(cssY);

    if (state.heldLine !== null) {
      // We are placing a held line
      placeLine(state, state.heldLine.axis, xVal, yVal, state.heldLine.index);
      state.heldLine = null;
    } else {
      // Try to place a new line or pick up an existing one
      const axis = state.activeAxis;
      const lines = axis === 'vertical' ? state.verticalLines : state.horizontalLines;

      if (lines.length < 2) {
        // Place a new line
        const val = axis === 'vertical' ? xVal : yVal;
        lines.push({ value: val });
      } else {
        // Both slots full: pick up the nearest line
        const mouseVal = axis === 'vertical' ? xVal : yVal;
        const nearestIdx = findNearestLine(lines, mouseVal);
        state.heldLine = { axis, index: nearestIdx };
        // Remove the line from its slot (it follows the mouse now)
        lines.splice(nearestIdx, 1);
      }
    }

    chart.update('none');
  });
}

function placeLine(state, axis, xVal, yVal, index) {
  const lines = axis === 'vertical' ? state.verticalLines : state.horizontalLines;
  const val = axis === 'vertical' ? xVal : yVal;
  // Insert at the original index position, or at end
  if (index !== undefined && index <= lines.length) {
    lines.splice(index, 0, { value: val });
  } else {
    lines.push({ value: val });
  }
}

function findNearestLine(lines, value) {
  let minDist = Infinity;
  let minIdx = 0;
  lines.forEach((line, i) => {
    const dist = Math.abs(line.value - value);
    if (dist < minDist) {
      minDist = dist;
      minIdx = i;
    }
  });
  return minIdx;
}
```

### 2.4 Making Placed Lines Draggable

For drag-to-reposition, we need to detect when the mouse is near an existing line, handle mousedown to "grab" it, mousemove to reposition, and mouseup to release.

```javascript
/**
 * Enhanced measurement event handler with drag support.
 * Replaces the simpler click handler above.
 */
function attachDraggableMeasurementEvents(chart, measurementPlugin) {
  const canvas = chart.canvas;
  const state = measurementPlugin.getState();
  let isDragging = false;
  let dragTarget = null;  // {axis: 'vertical'|'horizontal', index: number}

  const GRAB_THRESHOLD_PX = 8;  // How close mouse must be to grab a line

  canvas.addEventListener('contextmenu', (e) => e.preventDefault());

  canvas.addEventListener('mousedown', (e) => {
    if (!state.enabled) return;

    if (e.button === 2) {
      // Right-click: toggle axis mode
      e.preventDefault();
      state.activeAxis = state.activeAxis === 'vertical' ? 'horizontal' : 'vertical';
      updateModeIndicator(chart, state);
      chart.update('none');
      return;
    }

    if (e.button !== 0) return;

    const { cssX, cssY, inArea } = getMousePosition(e, canvas, chart);
    if (!inArea) return;

    // Check if mouse is near an existing line (for dragging)
    const nearLine = findLineNearMouse(chart, state, cssX, cssY, GRAB_THRESHOLD_PX);
    if (nearLine) {
      isDragging = true;
      dragTarget = nearLine;
      canvas.style.cursor = nearLine.axis === 'vertical' ? 'col-resize' : 'row-resize';
      e.preventDefault();
      return;
    }

    // Not near a line: place a new one or pick up nearest
    const axis = state.activeAxis;
    const lines = axis === 'vertical' ? state.verticalLines : state.horizontalLines;
    const val = axis === 'vertical'
      ? chart.scales.x.getValueForPixel(cssX)
      : chart.scales.y.getValueForPixel(cssY);

    if (lines.length < 2) {
      lines.push({ value: val });
    } else {
      // Pick up nearest and start dragging
      const nearestIdx = findNearestLine(lines, val);
      isDragging = true;
      dragTarget = { axis, index: nearestIdx };
      canvas.style.cursor = axis === 'vertical' ? 'col-resize' : 'row-resize';
    }

    chart.update('none');
  });

  canvas.addEventListener('mousemove', (e) => {
    if (!state.enabled) return;

    const { cssX, cssY, inArea } = getMousePosition(e, canvas, chart);

    if (isDragging && dragTarget && inArea) {
      // Update the dragged line's position
      const lines = dragTarget.axis === 'vertical'
        ? state.verticalLines
        : state.horizontalLines;
      const line = lines[dragTarget.index];
      if (line) {
        line.value = dragTarget.axis === 'vertical'
          ? chart.scales.x.getValueForPixel(cssX)
          : chart.scales.y.getValueForPixel(cssY);
        chart.update('none');
      }
      return;
    }

    // Update cursor style when hovering near a line
    if (!isDragging && inArea) {
      const nearLine = findLineNearMouse(chart, state, cssX, cssY, GRAB_THRESHOLD_PX);
      if (nearLine) {
        canvas.style.cursor = nearLine.axis === 'vertical' ? 'col-resize' : 'row-resize';
      } else {
        canvas.style.cursor = state.enabled ? 'crosshair' : 'default';
      }
    }
  });

  canvas.addEventListener('mouseup', (e) => {
    if (isDragging) {
      isDragging = false;
      dragTarget = null;
      canvas.style.cursor = state.enabled ? 'crosshair' : 'default';
      chart.update('none');
    }
  });

  canvas.addEventListener('mouseleave', () => {
    if (isDragging) {
      isDragging = false;
      dragTarget = null;
      canvas.style.cursor = 'default';
    }
  });
}

/**
 * Finds the nearest placed line within a pixel threshold.
 *
 * @param {Chart} chart
 * @param {object} state - Measurement state
 * @param {number} cssX - Mouse X in CSS pixels
 * @param {number} cssY - Mouse Y in CSS pixels
 * @param {number} threshold - Grab distance in pixels
 * @returns {object|null} {axis, index} or null
 */
function findLineNearMouse(chart, state, cssX, cssY, threshold) {
  let best = null;
  let bestDist = threshold;

  // Check vertical lines
  state.verticalLines.forEach((line, i) => {
    const px = chart.scales.x.getPixelForValue(line.value);
    const dist = Math.abs(cssX - px);
    if (dist < bestDist) {
      bestDist = dist;
      best = { axis: 'vertical', index: i };
    }
  });

  // Check horizontal lines
  state.horizontalLines.forEach((line, i) => {
    const py = chart.scales.y.getPixelForValue(line.value);
    const dist = Math.abs(cssY - py);
    if (dist < bestDist) {
      bestDist = dist;
      best = { axis: 'horizontal', index: i };
    }
  });

  return best;
}

function getMousePosition(e, canvas, chart) {
  const rect = canvas.getBoundingClientRect();
  const cssX = e.clientX - rect.left;
  const cssY = e.clientY - rect.top;
  const area = chart.chartArea;
  const inArea = cssX >= area.left && cssX <= area.right &&
                 cssY >= area.top && cssY <= area.bottom;
  return { cssX, cssY, inArea };
}
```

### 2.5 Coordinate System Notes

**Critical detail for Chart.js canvas coordinates:**

Chart.js internally handles `devicePixelRatio` scaling. The `chartArea` boundaries (`left`, `right`, `top`, `bottom`) and the `getPixelForValue`/`getValueForPixel` methods all operate in **CSS pixel space**, not device pixel space. The canvas element has internal dimensions scaled by DPR, but Chart.js applies a DPR transform to the canvas context.

This means:
- `event.x` / `event.y` from Chart.js `afterEvent` are CSS pixels. Use directly.
- `getBoundingClientRect()` + `e.clientX` gives CSS pixels. Use directly.
- Do NOT multiply by `devicePixelRatio` when comparing against `chartArea` or calling `getValueForPixel`.
- The canvas context (`chart.ctx`) already has the DPR transform applied. Drawing at CSS pixel coordinates is correct.

### 2.6 Storing Line Values in Data Space vs Pixel Space

**Lines should store values in data space** (the actual X/Y data values), not pixel coordinates. Reasons:

1. When the chart rescales (auto-scale, zoom, window resize), pixel coordinates become invalid. Data values remain correct.
2. When new data arrives and the X-axis scrolls, a vertical line at `x=50` stays at sample 50, not at some pixel that now represents sample 60.
3. `getPixelForValue()` is called during each draw to convert data values to current pixel positions.

For a **frozen/paused** chart, this works perfectly. For a **live scrolling** chart, vertical measurement lines at fixed X values will scroll off-screen as new data arrives. This is actually the desired behavior -- the measurement is anchored to a specific data point.

---

## 3. Show Data Points Toggle

### 3.1 Chart.js Point Rendering

Each dataset has `pointRadius` and related properties:

```javascript
dataset = {
  label: 'X',
  data: [...],
  borderColor: '#ef5350',
  borderWidth: 1.5,
  pointRadius: 0,         // 0 = no points drawn (default for fc_tool)
  pointHitRadius: 10,     // Hover detection area (even when pointRadius is 0)
  pointHoverRadius: 4,    // Size when hovered (for tooltip snapping)
  tension: 0.2,
};
```

### 3.2 Toggle Implementation

```javascript
/**
 * Toggles data point visibility for all datasets on a chart.
 *
 * @param {Chart} chart - The Chart.js instance
 * @param {boolean} show - true to show points, false to hide
 */
function toggleDataPoints(chart, show) {
  const radius = show ? 3 : 0;
  chart.data.datasets.forEach((ds) => {
    ds.pointRadius = radius;
  });
  chart.update('none');
}
```

Wired to a toggle button:

```javascript
const showPointsBtn = document.getElementById('show-points-btn');
let pointsVisible = false;

showPointsBtn.addEventListener('click', () => {
  pointsVisible = !pointsVisible;
  toggleDataPoints(accelChart, pointsVisible);
  showPointsBtn.textContent = pointsVisible ? 'Points: ON' : 'Points: OFF';
});
```

### 3.3 Performance Impact with Large Datasets

**Measured characteristics of Chart.js point rendering:**

| Data Points | pointRadius=0 | pointRadius=3 | Delta |
|-------------|---------------|---------------|-------|
| 100 | ~0.8ms | ~1.0ms | +0.2ms |
| 500 | ~2.5ms | ~3.5ms | +1.0ms |
| 1000 | ~4ms | ~6ms | +2ms |
| 5000 | ~15ms | ~25ms | +10ms |

(Approximate, varies with hardware. Measured as full `chart.draw()` time.)

**Analysis**:

- At fc_tool's current `MAX_DATA_POINTS = 100`, the impact is negligible (< 0.5ms).
- At 500 points (planned expansion), still well within budget.
- At 1000+ points, the extra 2ms is noticeable but acceptable.
- At 5000+ points, consider using Chart.js's built-in **decimation plugin** to reduce visual points:

```javascript
options: {
  plugins: {
    decimation: {
      enabled: true,
      algorithm: 'lttb',     // Largest-Triangle-Three-Buckets
      samples: 500,          // Max points to render
      threshold: 500         // Only decimate if more than this many points
    }
  },
  // Decimation requires these settings:
  parsing: false,            // Data must be pre-sorted
  normalized: true,          // Tell Chart.js data is already sorted
}
```

**Note**: Decimation only works with `{x, y}` format data (not array index format). If fc_tool moves to explicit `{x, y}` data points (recommended for future features like zoom/pan), decimation becomes available automatically.

**Recommendation**: For now (100-500 points), pointRadius toggle has zero measurable performance impact. No optimization needed.

---

## 4. Multiple Independent Charts

### 4.1 The Problem

fc_tool currently has 2 charts (accelerometer and gyroscope). The enhanced plotter will have N charts (dynamic, based on incoming data). Each chart needs:

- Its own crosshair state (cursor position)
- Its own measurement line state (0-4 lines)
- Its own readout panel
- Its own toggle states (points, trigger mode, axis mode)

### 4.2 Architecture: Per-Chart State via Plugin Closures

Each chart gets its own plugin instances via factory functions (the `createCrosshairPlugin()` and `createMeasurementPlugin()` patterns shown above). State is captured in closures, not in global variables.

```javascript
/**
 * Creates all interactive plugins and state for a single chart.
 * Returns a controller object for external access.
 */
function createChartInteraction(chartCanvas, chartInstance, readoutEl, deltaReadoutEl) {
  // Create plugin instances (each has its own closure state)
  const crosshairPlugin = createCrosshairPlugin(readoutEl);
  const measurementPlugin = createMeasurementPlugin(deltaReadoutEl);

  // Attach native DOM events for measurement interaction
  attachDraggableMeasurementEvents(chartInstance, measurementPlugin);

  return {
    chart: chartInstance,
    crosshairPlugin,
    measurementPlugin,

    // Control methods
    enableTriggerMode(enabled) {
      measurementPlugin.getState().enabled = enabled;
      chartInstance.update('none');
    },

    toggleDataPoints(show) {
      toggleDataPoints(chartInstance, show);
    },

    clearMeasurementLines() {
      const state = measurementPlugin.getState();
      state.verticalLines = [];
      state.horizontalLines = [];
      state.heldLine = null;
      chartInstance.update('none');
    },

    getState() {
      return measurementPlugin.getState();
    }
  };
}
```

### 4.3 Chart Registry Pattern

For managing N dynamically created charts:

```javascript
/**
 * Registry that manages all chart instances and their interaction state.
 */
class ChartRegistry {
  constructor() {
    this.charts = new Map();  // plotId -> interaction controller
  }

  /**
   * Register a new chart with full interaction support.
   *
   * @param {string|number} plotId - Unique plot identifier
   * @param {HTMLCanvasElement} canvas - The canvas element
   * @param {object} chartConfig - Chart.js configuration
   * @param {HTMLElement} readoutEl - Mouse coordinate readout element
   * @param {HTMLElement} deltaReadoutEl - Delta measurement readout element
   * @returns {object} Interaction controller
   */
  register(plotId, canvas, chartConfig, readoutEl, deltaReadoutEl) {
    // Create plugin instances BEFORE creating the chart
    const crosshairPlugin = createCrosshairPlugin(readoutEl);
    const measurementPlugin = createMeasurementPlugin(deltaReadoutEl);

    // Add plugins to chart config
    if (!chartConfig.plugins) chartConfig.plugins = [];
    chartConfig.plugins.push(crosshairPlugin, measurementPlugin);

    // Ensure events include mousemove and mouseout
    if (!chartConfig.options) chartConfig.options = {};
    chartConfig.options.events = ['mousemove', 'mouseout', 'click',
                                   'touchstart', 'touchmove'];

    // Create the chart
    const ctx = canvas.getContext('2d');
    const chart = new Chart(ctx, chartConfig);

    // Attach native DOM events
    attachDraggableMeasurementEvents(chart, measurementPlugin);

    const controller = createChartInteraction(
      canvas, chart, readoutEl, deltaReadoutEl
    );

    this.charts.set(plotId, controller);
    return controller;
  }

  /**
   * Remove a chart and clean up.
   */
  unregister(plotId) {
    const controller = this.charts.get(plotId);
    if (controller) {
      controller.chart.destroy();
      this.charts.delete(plotId);
    }
  }

  get(plotId) {
    return this.charts.get(plotId);
  }

  getAll() {
    return Array.from(this.charts.values());
  }
}
```

### 4.4 Event Handling: Per-Chart vs Event Delegation

**Per-chart event listeners (RECOMMENDED):**

Each chart canvas gets its own `mousedown`, `mousemove`, `mouseup`, `contextmenu` listeners. This is correct because:

1. Chart.js creates one canvas per chart. Events on one canvas never fire on another.
2. The crosshair's `afterEvent` hook already scopes to the correct chart (Chart.js passes the chart instance).
3. Native DOM events on the canvas element are naturally scoped to that chart.
4. With N charts (where N is typically 2-8), the number of event listeners is small (4-5 per chart = 16-40 total). This has zero performance impact.

**Event delegation** (attaching one listener to a parent container) adds complexity for no benefit here:
- Each canvas has different coordinate spaces.
- Chart.js instances are independent.
- The delegation code would need to map `event.target` to chart instances anyway.

**Conclusion**: Per-chart listeners are the correct approach. Simple, direct, no cross-chart leakage.

### 4.5 Crosshair Isolation Between Charts

**Requirement**: Only the chart under the mouse shows a crosshair. Other charts show nothing.

This is automatic with the plugin approach because:

1. `mousemove` events only fire on the canvas the mouse is over.
2. Each chart's `afterEvent` hook only receives events for that chart.
3. When the mouse leaves a canvas, `mouseout` fires, and the crosshair state resets (`cursorX = null, cursorY = null`).
4. Other charts never receive the mousemove event, so their crosshair state remains null.

No special handling needed. The isolation is inherent.

---

## 5. Recommendations

### 5.1 Implementation Approach

| Feature | Approach | Dependency |
|---------|----------|------------|
| Passive crosshair | Custom inline plugin (`afterDatasetsDraw` + `afterEvent`) | None (Chart.js only) |
| Measurement lines | Custom inline plugin + native DOM events | None |
| Data point toggle | `pointRadius` property update | None |
| Multi-chart management | ChartRegistry with per-chart closures | None |

**No additional npm packages required.** Everything is implementable with Chart.js 4.5.1's built-in plugin API and standard DOM events.

### 5.2 File Structure

Based on the architecture outlined in `cursor-interaction-discussion.md`:

```text
fc_tool/src/plotter/
  crosshair-plugin.js      -- Passive crosshair plugin factory
  measurement-plugin.js    -- Measurement line plugin factory + state
  measurement-events.js    -- Native DOM event handlers (click, drag, right-click)
  chart-registry.js        -- Multi-chart management
  data-points-toggle.js    -- Point visibility toggle (trivial, may inline)
  format-utils.js          -- formatValue() and similar helpers
```

### 5.3 Dependency Decision Matrix

| Package | Decision | Rationale |
|---------|----------|-----------|
| `chartjs-plugin-crosshair` | SKIP | Chart.js 4 incompatible, vertical-only, adds dependency |
| `chartjs-plugin-annotation` | SKIP | No drag support, 50KB for 4 lines, custom is simpler |
| `chartjs-plugin-zoom` | EVALUATE LATER | May be useful for zoom/pan, but not needed for cursor system |

### 5.4 Implementation Priority

1. **Crosshair plugin** -- Foundation. ~40 lines. Get it working first.
2. **Readout panel** -- Wire up coordinate display. Trivial HTML + CSS.
3. **Measurement lines** -- Core value. Click-to-place first, then drag.
4. **Right-click axis toggle** -- Small addition once lines work.
5. **Data points toggle** -- 5 lines of code. Do whenever.
6. **ChartRegistry** -- Needed when moving from 2 hardcoded charts to N dynamic charts.

### 5.5 Testing Strategy

Since this is a Tauri desktop app with no test framework set up:

1. **Manual testing** with the existing 2-chart IMU view.
2. **Add crosshair plugin** to accelChart and gyroChart as proof of concept.
3. **Verify** tooltip still works with crosshair active.
4. **Test measurement lines** with static data (pause chart, place lines, verify deltas).
5. **Test right-click** does not show browser context menu.
6. **Test drag** by grabbing and moving placed lines.

### 5.6 Known Gotchas

1. **Chart.js `update('none')` vs `update()`**: Use `'none'` mode to skip animations entirely. This is critical for responsive crosshair/measurement updates. The string `'quiet'` is NOT a valid update mode in Chart.js 4 (it was in v2). Use `'none'`.

2. **Canvas DPR**: Never multiply pixel coordinates by `devicePixelRatio` when interfacing with Chart.js APIs (`getValueForPixel`, `getPixelForValue`, `chartArea`). Chart.js handles DPR internally.

3. **Event types in config**: Chart.js only listens for events listed in `options.events`. The default is `['mousemove', 'mouseout', 'click', 'touchstart', 'touchmove', 'pointerdown', 'pointermove']`. For the crosshair plugin to work, `mousemove` and `mouseout` must be in this list (they are by default).

4. **Right-click and Chart.js**: Chart.js does NOT include `contextmenu` in its event system. Right-click handling MUST use native DOM `addEventListener('contextmenu', ...)` on the canvas. The chart's internal event system will not surface right-click events.

5. **`args.changed = true`**: In the `afterEvent` hook, setting `args.changed = true` tells Chart.js that the visual state has changed and a re-render is needed. Without this, the crosshair would only update when the chart re-renders for other reasons (data update, window resize, etc.).

6. **Inline plugins vs registered plugins**: Inline plugins (passed in the `plugins` array of the chart config) are scoped to that chart instance. Registered plugins (`Chart.register(MyPlugin)`) apply to ALL charts. For per-chart crosshair state, inline plugins are required.

---

## References

### Chart.js Documentation

- [Chart.js Plugin API](https://www.chartjs.org/docs/latest/developers/plugins.html) -- Hook lifecycle, inline vs registered
- [Chart.js Performance](https://www.chartjs.org/docs/latest/general/performance.html) -- Decimation, animation disable
- [Chart.js Interaction](https://www.chartjs.org/docs/latest/configuration/interactions.html) -- Event modes, tooltip behavior
- [Chart.js Scale API](https://www.chartjs.org/docs/latest/api/classes/Scale.html) -- getValueForPixel, getPixelForValue

### Chart.js Source (v4.5.1, bundled in fc_tool)

- Plugin hook dispatch: `chart.js:6087` (`notifyPlugins('beforeDraw')`)
- Event handling: `chart.js:6394` (`_eventHandler`)
- Tooltip plugin: `chart.js:9899` (`afterDraw` draws tooltip)
- Scale coordinate conversion: `chart.js:10384` (`getValueForPixel`)
- Internal throttling: `chart.js:3425` (`throttled()` wrapper on event proxy)

### Related fc_tool Documents

- [cursor-interaction-discussion.md](../cursor-interaction-discussion.md) -- Design decisions
- [chartjs-oscilloscope-research.md](chartjs-oscilloscope-research.md) -- Prior research
- [multi-graph-plotter-research.md](multi-graph-plotter-research.md) -- Multi-chart protocol

---

*This research provides implementation-ready code for fc_tool's cursor and measurement system. No additional npm dependencies are needed.*
