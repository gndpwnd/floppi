/**
 * cursors.js — Measurement cursor system for Chart.js plots
 *
 * Provides draggable vertical (yellow) and horizontal (blue) measurement lines.
 * Each plot supports up to 2 vertical + 2 horizontal lines with delta readout.
 */

const CURSOR_COLORS = {
  vertical: '#FFFF00',   // Neon yellow
  horizontal: '#00BFFF', // Neon blue
};

const GRAB_THRESHOLD = 8; // px proximity to grab a line

/**
 * Create measurement state for a single plot.
 */
function createMeasurementState() {
  return {
    enabled: false,          // Trigger mode on/off
    activeAxis: 'vertical',  // 'vertical' or 'horizontal'
    verticalLines: [],       // [{value: xDataVal}] max 2
    horizontalLines: [],     // [{value: yDataVal}] max 2
    dragging: null,          // {axis, index} while dragging
  };
}

/**
 * Factory: creates a measurement plugin with per-chart state.
 * @param {object} state - measurement state from createMeasurementState()
 * @param {HTMLElement} deltaEl - element to show delta readout
 */
function createMeasurementPlugin(state, deltaEl) {
  return {
    id: 'measurement_' + Math.random().toString(36).slice(2, 8),

    afterDatasetsDraw(chart) {
      if (!state.enabled) return;
      const { ctx, chartArea: a, scales } = chart;
      if (!a) return;

      ctx.save();

      // Draw vertical lines (yellow)
      ctx.strokeStyle = CURSOR_COLORS.vertical;
      ctx.lineWidth = 1.5;
      for (const line of state.verticalLines) {
        const px = scales.x.getPixelForValue(line.value);
        if (px >= a.left && px <= a.right) {
          ctx.beginPath();
          ctx.moveTo(px, a.top);
          ctx.lineTo(px, a.bottom);
          ctx.stroke();
        }
      }

      // Draw horizontal lines (blue)
      ctx.strokeStyle = CURSOR_COLORS.horizontal;
      for (const line of state.horizontalLines) {
        const px = scales.y.getPixelForValue(line.value);
        if (px >= a.top && px <= a.bottom) {
          ctx.beginPath();
          ctx.moveTo(a.left, px);
          ctx.lineTo(a.right, px);
          ctx.stroke();
        }
      }

      ctx.restore();
    },
  };
}

function formatVal(v) {
  if (v === null || v === undefined || isNaN(v)) return '--';
  return Number.isInteger(v) ? v.toString() : v.toFixed(3);
}

function updateDeltaReadout(state, el) {
  if (!state.enabled) { el.textContent = ''; return; }
  const parts = [];

  if (state.verticalLines.length >= 1) {
    const v = state.verticalLines;
    if (v.length === 2) {
      const d = Math.abs(v[1].value - v[0].value);
      parts.push(`Y1=${formatVal(v[0].value)}  Y2=${formatVal(v[1].value)}  \u0394Y=${formatVal(d)}`);
    } else {
      parts.push(`Y1=${formatVal(v[0].value)}`);
    }
  }

  if (state.horizontalLines.length >= 1) {
    const h = state.horizontalLines;
    if (h.length === 2) {
      const d = Math.abs(h[1].value - h[0].value);
      parts.push(`X1=${formatVal(h[0].value)}  X2=${formatVal(h[1].value)}  \u0394X=${formatVal(d)}`);
    } else {
      parts.push(`X1=${formatVal(h[0].value)}`);
    }
  }

  el.textContent = parts.join('  |  ') || (state.enabled ? 'Click to place lines' : '');
}

/**
 * Attach native DOM events for measurement interaction.
 * @param {HTMLCanvasElement} canvas
 * @param {Chart} chart
 * @param {object} state - measurement state
 * @param {HTMLElement} deltaEl - delta readout element
 */
function attachMeasurementEvents(canvas, chart, state, deltaEl) {
  // Suppress browser context menu on the plot
  canvas.addEventListener('contextmenu', (e) => {
    if (state.enabled) e.preventDefault();
  });

  canvas.addEventListener('mousedown', (e) => {
    if (!state.enabled) return;
    const rect = canvas.getBoundingClientRect();
    const cssX = e.clientX - rect.left;
    const cssY = e.clientY - rect.top;
    const a = chart.chartArea;
    if (!a || cssX < a.left || cssX > a.right || cssY < a.top || cssY > a.bottom) return;

    // Right-click: toggle axis mode
    if (e.button === 2) {
      e.preventDefault();
      state.activeAxis = state.activeAxis === 'vertical' ? 'horizontal' : 'vertical';
      updateDeltaReadout(state, deltaEl);
      return;
    }

    // Left-click: place or grab a line
    if (e.button === 0) {
      const lines = state.activeAxis === 'vertical' ? state.verticalLines : state.horizontalLines;
      const scale = state.activeAxis === 'vertical' ? chart.scales.x : chart.scales.y;
      const mousePos = state.activeAxis === 'vertical' ? cssX : cssY;

      // Check if near an existing line (grab it)
      let nearestIdx = -1;
      let nearestDist = Infinity;
      for (let i = 0; i < lines.length; i++) {
        const linePx = scale.getPixelForValue(lines[i].value);
        const dist = Math.abs(linePx - mousePos);
        if (dist < GRAB_THRESHOLD && dist < nearestDist) {
          nearestIdx = i;
          nearestDist = dist;
        }
      }

      if (nearestIdx >= 0) {
        // Grab existing line for dragging
        state.dragging = { axis: state.activeAxis, index: nearestIdx };
        canvas.style.cursor = state.activeAxis === 'vertical' ? 'col-resize' : 'row-resize';
      } else if (lines.length < 2) {
        // Place new line
        const value = scale.getValueForPixel(mousePos);
        lines.push({ value });
        updateDeltaReadout(state, deltaEl);
        chart.update('none');
      } else {
        // Max lines placed — grab nearest to reposition
        state.dragging = { axis: state.activeAxis, index: nearestIdx >= 0 ? nearestIdx : 0 };
        canvas.style.cursor = state.activeAxis === 'vertical' ? 'col-resize' : 'row-resize';
      }
    }
  });

  canvas.addEventListener('mousemove', (e) => {
    if (!state.dragging) return;
    const rect = canvas.getBoundingClientRect();
    const cssPos = state.dragging.axis === 'vertical'
      ? e.clientX - rect.left
      : e.clientY - rect.top;
    const scale = state.dragging.axis === 'vertical' ? chart.scales.x : chart.scales.y;
    const lines = state.dragging.axis === 'vertical' ? state.verticalLines : state.horizontalLines;

    lines[state.dragging.index].value = scale.getValueForPixel(cssPos);
    updateDeltaReadout(state, deltaEl);
    chart.update('none');
  });

  const endDrag = () => {
    if (state.dragging) {
      state.dragging = null;
      canvas.style.cursor = '';
    }
  };
  canvas.addEventListener('mouseup', endDrag);
  canvas.addEventListener('mouseleave', endDrag);
}

export {
  createMeasurementState,
  createMeasurementPlugin,
  attachMeasurementEvents,
  updateDeltaReadout,
  CURSOR_COLORS,
};
