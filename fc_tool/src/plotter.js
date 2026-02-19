/**
 * plotter.js — Dynamic multi-graph serial plotter for fc_tool
 *
 * Protocol: name@plotId:value (backward compatible with name:value and plain CSV)
 * Creates Chart.js instances dynamically as new plot IDs arrive in the data stream.
 */

import {
  createMeasurementState,
  createMeasurementPlugin,
  attachMeasurementEvents,
  updateDeltaReadout,
} from './cursors.js';
import { ThresholdTrigger } from './period-detector.js';

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
// Shared inline styles for JS-generated controls (dark theme)
// ============================================================================

const CTRL_STYLE = 'background:#2a2a3e;color:#e0e0e0;border:1px solid #444;' +
  'border-radius:3px;padding:1px 4px;font-size:11px;font-family:inherit;';

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
    this.showPoints = false;
    this.keepRecording = false;
    this._buffer = [];       // buffered lines while paused with keepRecording
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
    if (!this.enabled) return;

    // When paused: buffer if keepRecording, otherwise drop
    if (this.paused) {
      if (this.keepRecording) {
        this._buffer.push(line);
        // Cap buffer to prevent unbounded memory growth
        if (this._buffer.length > this.maxDataPoints * 2) {
          this._buffer.splice(0, this._buffer.length - this.maxDataPoints * 2);
        }
      }
      return;
    }

    this._ingestLine(line);
  }

  /** Internal: parse and route a single line to charts. */
  _ingestLine(line) {
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

      // Frozen mode: silently drop data
      if (plot.mode === 'frozen') continue;

      plot.sampleCount++;

      // Track recent values for auto-level (first variable on this plot)
      const firstValue = pts[0].value;
      plot.recentValues.push(firstValue);
      if (plot.recentValues.length > 500) {
        plot.recentValues.splice(0, plot.recentValues.length - 500);
      }

      // Period/Single mode: route through trigger
      if ((plot.mode === 'period' || plot.mode === 'single') && plot.trigger) {
        const result = plot.trigger.addSample(firstValue);
        // Still update stats for all variables
        for (const pt of pts) {
          if (!plot.stats.has(pt.name)) {
            plot.stats.set(pt.name, { min: pt.value, max: pt.value, sum: pt.value, count: 1 });
          } else {
            const s = plot.stats.get(pt.name);
            if (pt.value < s.min) s.min = pt.value;
            if (pt.value > s.max) s.max = pt.value;
            s.sum += pt.value;
            s.count++;
          }
        }
        if (result) {
          this._displayTriggeredPeriods(plot, result, pts);
        }
        this._updateStatsBar(plot);
        continue;
      }

      // Continuous mode: existing behavior
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

      this._applyXScale(plot);
      plot.chart.update('none');
      this._updateStatsBar(plot);
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

    const closeBtn = document.createElement('button');
    closeBtn.type = 'button';
    closeBtn.className = 'plot-close-btn';
    closeBtn.textContent = '\u00D7';
    closeBtn.title = 'Close this plot (will reappear if data continues)';
    header.appendChild(closeBtn);

    const triggerBtn = document.createElement('button');
    triggerBtn.type = 'button';
    triggerBtn.className = 'plot-trigger-btn';
    triggerBtn.textContent = 'Trigger';
    triggerBtn.title = 'Toggle measurement cursors (right-click plot to switch axis)';
    header.appendChild(triggerBtn);

    const zoomControls = document.createElement('div');
    zoomControls.className = 'plot-zoom';
    zoomControls.innerHTML =
      '<span class="zoom-label">Y</span>' +
      '<button type="button" data-zoom="in" title="Zoom in Y">+</button>' +
      '<button type="button" data-zoom="out" title="Zoom out Y">\u2212</button>' +
      '<button type="button" data-zoom="auto" title="Auto-fit Y">A</button>' +
      '<button type="button" data-zoom="up" title="Pan up">\u25B2</button>' +
      '<button type="button" data-zoom="down" title="Pan down">\u25BC</button>';
    header.appendChild(zoomControls);

    const xZoomControls = document.createElement('div');
    xZoomControls.className = 'plot-zoom';
    xZoomControls.innerHTML =
      '<span class="zoom-label">X</span>' +
      '<button type="button" data-xzoom="in" title="Zoom in X (fewer samples)">+</button>' +
      '<button type="button" data-xzoom="out" title="Zoom out X (more samples)">\u2212</button>' +
      '<button type="button" data-xzoom="auto" title="Auto-fit X">A</button>' +
      '<button type="button" data-xzoom="left" title="Pan left">\u25C0</button>' +
      '<button type="button" data-xzoom="right" title="Pan right">\u25B6</button>';
    header.appendChild(xZoomControls);

    // Mode selector
    const modeSelect = document.createElement('select');
    modeSelect.className = 'plot-mode-select';
    modeSelect.title = 'Plot display mode';
    modeSelect.innerHTML =
      '<option value="continuous">Continuous</option>' +
      '<option value="period">Period (N)</option>' +
      '<option value="single">Single Period</option>' +
      '<option value="frozen">Frozen</option>';
    modeSelect.style.cssText = CTRL_STYLE + 'cursor:pointer;';
    header.appendChild(modeSelect);

    // Period count input (visible only in period mode)
    const periodInput = document.createElement('input');
    periodInput.type = 'number';
    periodInput.className = 'plot-period-input';
    periodInput.min = '1';
    periodInput.max = '20';
    periodInput.value = '1';
    periodInput.title = 'Number of periods to display';
    periodInput.style.cssText = 'display:none;width:35px;text-align:center;' + CTRL_STYLE;
    header.appendChild(periodInput);

    // Trigger controls (visible when mode=period/single)
    const triggerControlsEl = document.createElement('div');
    triggerControlsEl.className = 'plot-trigger-controls';
    triggerControlsEl.style.cssText = 'display:none;align-items:center;gap:4px;';
    triggerControlsEl.innerHTML =
      '<label style="color:#888;font-size:10px;">Lvl:</label>' +
      '<input type="number" class="trigger-level-input" step="0.1" value="0" ' +
        'style="width:55px;text-align:center;' + CTRL_STYLE + '">' +
      '<select class="trigger-edge-select" style="' + CTRL_STYLE + 'cursor:pointer;">' +
        '<option value="rising">Rising</option>' +
        '<option value="falling">Falling</option>' +
      '</select>' +
      '<button type="button" class="trigger-auto-btn" title="Auto-detect trigger level" ' +
        'style="' + CTRL_STYLE + 'cursor:pointer;padding:1px 6px;">Auto</button>';
    header.appendChild(triggerControlsEl);

    // Period/frequency readout (shown in period/single modes)
    const periodReadout = document.createElement('span');
    periodReadout.className = 'plot-period-readout';
    periodReadout.style.cssText = 'display:none;color:#888;font-size:10px;font-family:monospace;';
    header.appendChild(periodReadout);

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

    const deltaReadout = document.createElement('div');
    deltaReadout.className = 'plot-delta-readout';
    wrapper.appendChild(deltaReadout);

    const statsBar = document.createElement('div');
    statsBar.className = 'plot-stats';
    wrapper.appendChild(statsBar);

    this.container.appendChild(wrapper);

    const crosshairPlugin = createCrosshairPlugin(readoutText);
    const measureState = createMeasurementState();
    const measurePlugin = createMeasurementPlugin(measureState, deltaReadout);

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
      plugins: [zeroLinePlugin, crosshairPlugin, measurePlugin],
    });

    attachMeasurementEvents(canvas, chart, measureState, deltaReadout);

    const plotState = {
      chart,
      wrapper,
      canvas,
      readoutText,
      titleEl,
      statsBar,
      deltaReadout,
      measureState,
      plotId,
      datasets: new Map(), // name -> dataset index
      stats: new Map(),    // name -> { min, max, sum, count }
      colorIdx: 0,
      sampleCount: 0,
      yAuto: true,
      xAuto: true,
      xZoom: 1.0,  // fraction of maxDataPoints visible (1.0 = all)
      xPan: 0,     // samples offset from right edge
      // Mode state
      mode: 'continuous', // 'continuous' | 'period' | 'single' | 'frozen'
      periodCount: 1,
      trigger: null,      // ThresholdTrigger instance (created when entering period/single mode)
      // Mode UI references
      modeSelect,
      periodInput,
      triggerControlsEl,
      periodReadout,
      // Recent values buffer for auto-level detection
      recentValues: [],
    };

    // Trigger (measurement cursor) toggle
    triggerBtn.addEventListener('click', () => {
      measureState.enabled = !measureState.enabled;
      triggerBtn.classList.toggle('active', measureState.enabled);
      if (!measureState.enabled) {
        measureState.verticalLines.length = 0;
        measureState.horizontalLines.length = 0;
        measureState.dragging = null;
      }
      updateDeltaReadout(measureState, deltaReadout);
      chart.update('none');
    });

    // Y-axis zoom/pan button handler
    zoomControls.addEventListener('click', (e) => {
      const btn = e.target.closest('[data-zoom]');
      if (!btn) return;
      const action = btn.dataset.zoom;
      const yScale = chart.options.scales.y;

      if (action === 'auto') {
        yScale.min = undefined;
        yScale.max = undefined;
        plotState.yAuto = true;
      } else if (action === 'in' || action === 'out') {
        const currentMin = chart.scales.y.min;
        const currentMax = chart.scales.y.max;
        const center = (currentMin + currentMax) / 2;
        const halfRange = (currentMax - currentMin) / 2;
        const factor = action === 'in' ? 0.5 : 2;
        yScale.min = center - halfRange * factor;
        yScale.max = center + halfRange * factor;
        plotState.yAuto = false;
      } else if (action === 'up' || action === 'down') {
        const currentMin = chart.scales.y.min;
        const currentMax = chart.scales.y.max;
        const range = currentMax - currentMin;
        const step = range * 0.25;
        const shift = action === 'up' ? step : -step;
        yScale.min = currentMin + shift;
        yScale.max = currentMax + shift;
        plotState.yAuto = false;
      }
      chart.update('none');
    });

    // X-axis zoom/pan button handler
    xZoomControls.addEventListener('click', (e) => {
      const btn = e.target.closest('[data-xzoom]');
      if (!btn) return;
      const action = btn.dataset.xzoom;

      if (action === 'auto') {
        plotState.xAuto = true;
        plotState.xZoom = 1.0;
        plotState.xPan = 0;
        chart.options.scales.x.min = undefined;
        chart.options.scales.x.max = undefined;
      } else if (action === 'in') {
        plotState.xZoom = Math.max(0.05, plotState.xZoom * 0.5);
        plotState.xAuto = false;
      } else if (action === 'out') {
        plotState.xZoom = Math.min(1.0, plotState.xZoom * 2);
        if (plotState.xZoom >= 1.0) {
          plotState.xAuto = true;
          plotState.xPan = 0;
          chart.options.scales.x.min = undefined;
          chart.options.scales.x.max = undefined;
        }
      } else if (action === 'left' || action === 'right') {
        if (plotState.xAuto) return; // no pan when auto
        const visibleCount = Math.max(10, Math.round(this.maxDataPoints * plotState.xZoom));
        const step = Math.max(1, Math.round(visibleCount / 4));
        if (action === 'left') {
          plotState.xPan = Math.min(this.maxDataPoints - visibleCount, plotState.xPan + step);
        } else {
          plotState.xPan = Math.max(0, plotState.xPan - step);
        }
      }
      this._applyXScale(plotState);
      chart.update('none');
    });

    // Mode selector handler
    modeSelect.addEventListener('change', () => {
      const newMode = modeSelect.value;
      const isPeriodic = newMode === 'period' || newMode === 'single';

      plotState.mode = newMode;
      periodInput.style.display = newMode === 'period' ? '' : 'none';
      triggerControlsEl.style.display = isPeriodic ? 'flex' : 'none';
      periodReadout.style.display = isPeriodic ? '' : 'none';

      if (newMode === 'single') plotState.periodCount = 1;

      if (isPeriodic && !plotState.trigger) {
        // Create trigger, auto-level from recent data
        plotState.trigger = new ThresholdTrigger({
          periodsNeeded: newMode === 'single' ? 1 : plotState.periodCount,
        });
        if (plotState.recentValues.length > 10) {
          plotState.trigger.autoLevel(plotState.recentValues);
          const lvlInput = triggerControlsEl.querySelector('.trigger-level-input');
          if (lvlInput) lvlInput.value = plotState.trigger.level.toFixed(2);
        }
        periodReadout.textContent = 'Waiting for trigger...';
      } else if (!isPeriodic) {
        plotState.trigger = null;
        periodReadout.textContent = '';
      }
    });

    periodInput.addEventListener('change', () => {
      plotState.periodCount = Math.max(1, Math.min(20, parseInt(periodInput.value, 10) || 1));
      if (plotState.trigger) {
        plotState.trigger.periodsNeeded = plotState.periodCount;
        plotState.trigger.completedPeriods = [];
      }
    });

    // Trigger level input
    triggerControlsEl.querySelector('.trigger-level-input')
      ?.addEventListener('change', (e) => {
        if (plotState.trigger) {
          plotState.trigger.level = parseFloat(e.target.value) || 0;
          plotState.trigger.reset();
          periodReadout.textContent = 'Waiting for trigger...';
        }
      });

    // Trigger edge selector
    triggerControlsEl.querySelector('.trigger-edge-select')
      ?.addEventListener('change', (e) => {
        if (plotState.trigger) {
          plotState.trigger.edge = e.target.value;
          plotState.trigger.reset();
          periodReadout.textContent = 'Waiting for trigger...';
        }
      });

    // Auto-level button
    triggerControlsEl.querySelector('.trigger-auto-btn')
      ?.addEventListener('click', () => {
        if (plotState.trigger && plotState.recentValues.length > 10) {
          plotState.trigger.autoLevel(plotState.recentValues);
          const lvlInput = triggerControlsEl.querySelector('.trigger-level-input');
          if (lvlInput) lvlInput.value = plotState.trigger.level.toFixed(2);
          plotState.trigger.reset();
          periodReadout.textContent = 'Waiting for trigger...';
        }
      });

    this.plots.set(plotId, plotState);

    closeBtn.addEventListener('click', () => {
      this.removePlot(plotId);
    });
  }

  /** Apply X-axis min/max based on zoom/pan state. */
  _applyXScale(plot) {
    if (plot.xAuto) {
      plot.chart.options.scales.x.min = undefined;
      plot.chart.options.scales.x.max = undefined;
      return;
    }
    const labels = plot.chart.data.labels;
    if (labels.length === 0) return;
    const visibleCount = Math.max(10, Math.round(this.maxDataPoints * plot.xZoom));
    const lastIdx = labels.length - 1;
    const endIdx = Math.max(0, lastIdx - plot.xPan);
    const startIdx = Math.max(0, endIdx - visibleCount);
    plot.chart.options.scales.x.min = labels[startIdx];
    plot.chart.options.scales.x.max = labels[endIdx];
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
        pointRadius: this.showPoints ? 2.5 : 0,
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

    // Update running statistics
    if (!plot.stats.has(name)) {
      plot.stats.set(name, { min: value, max: value, sum: value, count: 1 });
    } else {
      const s = plot.stats.get(name);
      if (value < s.min) s.min = value;
      if (value > s.max) s.max = value;
      s.sum += value;
      s.count++;
    }
  }

  /** Render stats bar for a plot (called after chart update). */
  _updateStatsBar(plot) {
    if (plot.stats.size === 0) return;
    const parts = [];
    for (const [name, s] of plot.stats) {
      const mean = s.sum / s.count;
      parts.push(
        `<span class="stat-label">${name}:</span> ` +
        `<span class="stat-value">min ${s.min.toFixed(2)} max ${s.max.toFixed(2)} avg ${mean.toFixed(2)}</span>`
      );
    }
    plot.statsBar.innerHTML = parts.join(' &nbsp; ');
  }

  /**
   * Display triggered periods on a plot (replaces chart data with aligned periods).
   * Called when ThresholdTrigger has collected enough periods.
   */
  _displayTriggeredPeriods(plot, triggerResult, pts) {
    const { chart, datasets, periodReadout } = plot;
    const { periods, frequencyHz, periodMs, periodSamples } = triggerResult;

    // Concatenate all period arrays into one flat data array
    const flatData = [];
    for (const period of periods) {
      for (const v of period) flatData.push(v);
    }

    // Ensure the first variable's dataset exists
    const firstName = pts[0].name;
    if (!datasets.has(firstName)) {
      const color = NEON_PALETTE[plot.colorIdx % NEON_PALETTE.length];
      plot.colorIdx++;
      chart.data.datasets.push({
        label: firstName,
        data: [],
        borderColor: color.border,
        backgroundColor: color.bg,
        borderWidth: 1.5,
        pointRadius: this.showPoints ? 2.5 : 0,
        pointHitRadius: 10,
        tension: 0.2,
        fill: false,
      });
      datasets.set(firstName, chart.data.datasets.length - 1);
      const names = [...datasets.keys()].join(', ');
      plot.titleEl.textContent = `Plot ${plot.plotId}: ${names}`;
    }

    // Replace chart data with aligned period data
    chart.data.labels = flatData.map((_, i) => i);
    const dsIdx = datasets.get(firstName);
    chart.data.datasets[dsIdx].data = flatData;

    // Clear other datasets to match label length (fill with null)
    for (let i = 0; i < chart.data.datasets.length; i++) {
      if (i !== dsIdx) {
        chart.data.datasets[i].data = new Array(flatData.length).fill(null);
      }
    }

    // Reset X scale for period view (show all data, no scrolling)
    chart.options.scales.x.min = undefined;
    chart.options.scales.x.max = undefined;

    chart.update('none');

    // Update readout with detected frequency/period
    if (frequencyHz > 0) {
      periodReadout.textContent =
        `${frequencyHz.toFixed(1)} Hz | ${periodMs.toFixed(1)} ms | ${periodSamples} smp`;
      periodReadout.style.display = '';
    } else {
      periodReadout.textContent = 'Waiting for trigger...';
    }
  }

  /** Clear all plots and destroy chart instances. */
  clearAll() {
    for (const [, plot] of this.plots) {
      plot.chart.destroy();
      plot.wrapper.remove();
    }
    this.plots.clear();
    this._buffer.length = 0;
  }

  /** Remove a single plot by ID. Auto-recreates if data continues. */
  removePlot(plotId) {
    const plot = this.plots.get(plotId);
    if (!plot) return;
    plot.chart.destroy();
    plot.wrapper.remove();
    this.plots.delete(plotId);
  }

  /** Reset running statistics for all plots. */
  resetStats() {
    for (const [, plot] of this.plots) {
      plot.stats.clear();
      plot.statsBar.innerHTML = '';
    }
  }

  /** Toggle pause. On unpause, flushes buffered data if keepRecording was on. Returns new paused state. */
  togglePause() {
    this.paused = !this.paused;
    if (!this.paused && this._buffer.length > 0) {
      // Flush buffered lines
      for (const line of this._buffer) {
        this._ingestLine(line);
      }
      this._buffer.length = 0;
    }
    return this.paused;
  }

  /** Toggle data point visibility on all plots. */
  setShowPoints(show) {
    this.showPoints = show;
    const radius = show ? 2.5 : 0;
    for (const [, plot] of this.plots) {
      for (const ds of plot.chart.data.datasets) {
        ds.pointRadius = radius;
      }
      plot.chart.update('none');
    }
  }

  destroy() {
    this.clearAll();
  }
}

export { PlotterManager, parseLine, THEME, NEON_PALETTE };
