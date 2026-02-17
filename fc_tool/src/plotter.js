/**
 * plotter.js — Dynamic multi-graph serial plotter for fc_tool
 *
 * Protocol: name@plotId:value (backward compatible with name:value and plain CSV)
 * Creates Chart.js instances dynamically as new plot IDs arrive in the data stream.
 */

// ============================================================================
// Theme
// ============================================================================

const THEME = {
  bg: { base: '#1a1a2e', plot: '#0d1117' },
  grid: { color: '#2d2d2d', lineWidth: 0.5, borderColor: '#444' },
  text: { primary: '#e0e0e0', muted: '#888' },
};

const NEON_PALETTE = [
  { border: '#00FF88', bg: '#00FF8840' }, // Lime
  { border: '#FF4444', bg: '#FF444440' }, // Coral
  { border: '#00DDFF', bg: '#00DDFF40' }, // Cyan
  { border: '#FF8C00', bg: '#FF8C0040' }, // Orange
  { border: '#FF44FF', bg: '#FF44FF40' }, // Magenta
  { border: '#FFFFFF', bg: '#FFFFFF40' }, // White
  { border: '#BB88FF', bg: '#BB88FF40' }, // Lavender
  { border: '#FF6B9D', bg: '#FF6B9D40' }, // Pink
];

// ============================================================================
// Chart.js Plugins
// ============================================================================

/** Fills canvas and plot area with dark background. */
const canvasBackgroundPlugin = {
  id: 'canvasBackground',
  beforeDraw(chart) {
    const { ctx, chartArea, width, height } = chart;
    ctx.save();
    ctx.fillStyle = THEME.bg.base;
    ctx.fillRect(0, 0, width, height);
    if (chartArea) {
      ctx.fillStyle = THEME.bg.plot;
      ctx.fillRect(
        chartArea.left, chartArea.top,
        chartArea.right - chartArea.left,
        chartArea.bottom - chartArea.top,
      );
    }
    ctx.restore();
  },
};

/** Draws a subtle solid line at y=0 when zero is within the visible range. */
const zeroLinePlugin = {
  id: 'zeroLine',
  afterDatasetsDraw(chart) {
    const yScale = chart.scales.y;
    if (!yScale || yScale.min > 0 || yScale.max < 0) return;
    const yPixel = yScale.getPixelForValue(0);
    const { ctx, chartArea: a } = chart;
    ctx.save();
    ctx.beginPath();
    ctx.strokeStyle = '#555';
    ctx.lineWidth = 1;
    ctx.moveTo(a.left, yPixel);
    ctx.lineTo(a.right, yPixel);
    ctx.stroke();
    ctx.restore();
  },
};

/** Factory: creates a crosshair inline plugin with per-chart state. */
function createCrosshairPlugin(readoutEl) {
  let cursorX = null;
  let cursorY = null;

  return {
    id: 'crosshair_' + Math.random().toString(36).slice(2, 8),

    afterEvent(chart, args) {
      const { event } = args;
      if (event.type === 'mousemove') {
        const a = chart.chartArea;
        if (a && event.x >= a.left && event.x <= a.right &&
            event.y >= a.top && event.y <= a.bottom) {
          cursorX = event.x;
          cursorY = event.y;
          args.changed = true;
          if (readoutEl) {
            const yVal = chart.scales.y.getValueForPixel(cursorY);
            const xVal = chart.scales.x.getValueForPixel(cursorX);
            readoutEl.textContent =
              `x: ${Math.round(xVal)}  y: ${yVal.toFixed(4)}`;
          }
        }
      } else if (event.type === 'mouseout') {
        cursorX = null;
        cursorY = null;
        args.changed = true;
        if (readoutEl) readoutEl.textContent = '\u2014';
      }
    },

    afterDatasetsDraw(chart) {
      if (cursorX === null || cursorY === null) return;
      const { ctx, chartArea: a } = chart;
      ctx.save();
      ctx.setLineDash([4, 4]);
      ctx.lineWidth = 0.8;
      ctx.strokeStyle = '#666';

      ctx.beginPath();
      ctx.moveTo(cursorX, a.top);
      ctx.lineTo(cursorX, a.bottom);
      ctx.stroke();

      ctx.beginPath();
      ctx.moveTo(a.left, cursorY);
      ctx.lineTo(a.right, cursorY);
      ctx.stroke();

      ctx.restore();
    },
  };
}

// ============================================================================
// Protocol Parser
// ============================================================================

const NAMED_RE = /([\w.]+)(?:@(\d+))?[=:]([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)/g;

/**
 * Parse a serial line into data points.
 * Supports: name@plotId:value, name:value, name=value, plain CSV/space-separated.
 * @param {string} line
 * @returns {Array<{name: string, plotId: number, value: number}>|null}
 */
function parseLine(line) {
  const results = [];
  NAMED_RE.lastIndex = 0;
  let match;
  while ((match = NAMED_RE.exec(line)) !== null) {
    results.push({
      name: match[1],
      plotId: match[2] !== undefined ? parseInt(match[2], 10) : 0,
      value: parseFloat(match[3]),
    });
  }
  if (results.length > 0) return results;

  // Fallback: plain numbers
  const nums = line.trim().split(/[\s,\t]+/).map(Number).filter(n => !isNaN(n));
  if (nums.length > 0) {
    return nums.map((v, i) => ({ name: `ch${i}`, plotId: 0, value: v }));
  }
  return null;
}

// ============================================================================
// PlotterManager
// ============================================================================

class PlotterManager {
  /**
   * @param {HTMLElement} containerEl - DOM element where charts are appended
   */
  constructor(containerEl) {
    this.container = containerEl;
    this.plots = new Map(); // plotId -> plot state
    this.maxDataPoints = 200;
    this.maxPlots = 10;
    this.paused = false;
    this.enabled = false;
    this._registered = false;
  }

  _ensureRegistered() {
    if (!this._registered) {
      Chart.register(canvasBackgroundPlugin);
      this._registered = true;
    }
  }

  /**
   * Process a raw serial line. Parses and routes to charts.
   * @param {string} line
   */
  processLine(line) {
    if (!this.enabled || this.paused) return;
    const points = parseLine(line);
    if (!points) return;

    const byPlot = new Map();
    for (const pt of points) {
      if (!byPlot.has(pt.plotId)) byPlot.set(pt.plotId, []);
      byPlot.get(pt.plotId).push(pt);
    }

    for (const [plotId, pts] of byPlot) {
      if (!this.plots.has(plotId)) {
        if (this.plots.size >= this.maxPlots) {
          console.warn(`PlotterManager: max plots (${this.maxPlots}) reached, ignoring plot ${plotId}`);
          continue;
        }
        this._createPlot(plotId);
      }
      const plot = this.plots.get(plotId);
      plot.sampleCount++;

      for (const pt of pts) {
        this._addDataPoint(plot, pt.name, pt.value);
      }

      // Trim to max
      const labels = plot.chart.data.labels;
      if (labels.length > this.maxDataPoints) {
        const excess = labels.length - this.maxDataPoints;
        labels.splice(0, excess);
        for (const ds of plot.chart.data.datasets) {
          ds.data.splice(0, excess);
        }
      }

      plot.chart.update('none');
    }
  }

  _createPlot(plotId) {
    this._ensureRegistered();

    const wrapper = document.createElement('div');
    wrapper.className = 'plot-wrapper';

    const header = document.createElement('div');
    header.className = 'plot-header';
    const titleEl = document.createElement('span');
    titleEl.className = 'plot-title';
    titleEl.textContent = `Plot ${plotId}`;
    header.appendChild(titleEl);

    const zoomControls = document.createElement('div');
    zoomControls.className = 'plot-zoom';
    zoomControls.innerHTML =
      '<button type="button" data-zoom="in" title="Zoom in Y">+</button>' +
      '<button type="button" data-zoom="out" title="Zoom out Y">\u2212</button>' +
      '<button type="button" data-zoom="auto" title="Auto-fit Y">A</button>';
    header.appendChild(zoomControls);

    wrapper.appendChild(header);

    const canvas = document.createElement('canvas');
    canvas.className = 'plot-canvas';
    wrapper.appendChild(canvas);

    const readout = document.createElement('div');
    readout.className = 'plot-readout';
    const readoutText = document.createElement('span');
    readoutText.className = 'readout-text';
    readoutText.textContent = '\u2014';
    readout.appendChild(readoutText);
    wrapper.appendChild(readout);

    this.container.appendChild(wrapper);

    const crosshairPlugin = createCrosshairPlugin(readoutText);

    const chart = new Chart(canvas.getContext('2d'), {
      type: 'line',
      data: { labels: [], datasets: [] },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        animation: false,
        devicePixelRatio: 1,
        interaction: { mode: null },
        hover: { mode: null },
        events: ['mousemove', 'mouseout', 'click'],
        plugins: {
          legend: {
            labels: {
              color: THEME.text.muted,
              boxWidth: 12,
              padding: 8,
              font: { family: "'SF Mono', 'Fira Code', monospace", size: 11 },
            },
          },
          tooltip: { enabled: false },
        },
        scales: {
          x: { display: false },
          y: {
            grid: {
              color: THEME.grid.color,
              lineWidth: THEME.grid.lineWidth,
              drawTicks: false,
            },
            ticks: {
              color: THEME.text.muted,
              font: { family: "'SF Mono', 'Fira Code', monospace", size: 11 },
              maxTicksLimit: 6,
            },
            border: { color: THEME.grid.borderColor, width: 1 },
          },
        },
      },
      plugins: [zeroLinePlugin, crosshairPlugin],
    });

    const plotState = {
      chart,
      wrapper,
      canvas,
      readoutText,
      titleEl,
      plotId,
      datasets: new Map(), // name -> dataset index
      colorIdx: 0,
      sampleCount: 0,
      yAuto: true,
    };

    // Zoom button handler
    zoomControls.addEventListener('click', (e) => {
      const btn = e.target.closest('[data-zoom]');
      if (!btn) return;
      const action = btn.dataset.zoom;
      const yScale = chart.options.scales.y;

      if (action === 'auto') {
        yScale.min = undefined;
        yScale.max = undefined;
        plotState.yAuto = true;
      } else {
        const currentMin = chart.scales.y.min;
        const currentMax = chart.scales.y.max;
        const center = (currentMin + currentMax) / 2;
        const halfRange = (currentMax - currentMin) / 2;
        const factor = action === 'in' ? 0.5 : 2;
        yScale.min = center - halfRange * factor;
        yScale.max = center + halfRange * factor;
        plotState.yAuto = false;
      }
      chart.update('none');
    });

    this.plots.set(plotId, plotState);
  }

  _addDataPoint(plot, name, value) {
    const { chart, datasets } = plot;

    if (!datasets.has(name)) {
      const color = NEON_PALETTE[plot.colorIdx % NEON_PALETTE.length];
      plot.colorIdx++;
      chart.data.datasets.push({
        label: name,
        data: new Array(chart.data.labels.length).fill(null),
        borderColor: color.border,
        backgroundColor: color.bg,
        borderWidth: 1.5,
        pointRadius: 0,
        pointHitRadius: 10,
        tension: 0.2,
        fill: false,
      });
      datasets.set(name, chart.data.datasets.length - 1);
      // Update plot header with variable names
      const names = [...datasets.keys()].join(', ');
      plot.titleEl.textContent = `Plot ${plot.plotId}: ${names}`;
    }

    const labels = chart.data.labels;
    const currentSample = plot.sampleCount;

    if (labels.length === 0 || labels[labels.length - 1] !== currentSample) {
      labels.push(currentSample);
      for (const ds of chart.data.datasets) {
        if (ds.data.length < labels.length) {
          ds.data.push(null);
        }
      }
    }

    chart.data.datasets[datasets.get(name)].data[labels.length - 1] = value;
  }

  /** Clear all plots and destroy chart instances. */
  clearAll() {
    for (const [, plot] of this.plots) {
      plot.chart.destroy();
      plot.wrapper.remove();
    }
    this.plots.clear();
  }

  /** Toggle pause. Returns new paused state. */
  togglePause() {
    this.paused = !this.paused;
    return this.paused;
  }

  destroy() {
    this.clearAll();
  }
}

export { PlotterManager, parseLine, THEME, NEON_PALETTE };
