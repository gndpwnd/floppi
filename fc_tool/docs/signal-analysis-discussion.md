# Signal / Pattern Analysis Mode — Discussion & Research

> **Status**: RESEARCH IN PROGRESS
> Last updated: 2026-02-07

This document explores the signal/pattern analysis features proposed for fc_tool's enhanced plotter. These features transform fc_tool from a basic serial monitor into a lightweight signal analysis tool.

---

## Vision

fc_tool should be a **creative foundation** for developers working with embedded systems. Not a full signal analysis suite (like MATLAB or LabVIEW), but significantly more capable than Arduino's Serial Plotter. Users should be able to:

1. Monitor continuous data streams (existing)
2. Inspect repetitive signals by viewing N periods (new — **Period Mode**)
3. Measure values and deltas on any data (new — **Trigger Mode**, see [cursor-interaction-discussion.md](cursor-interaction-discussion.md))
4. Detect when a signal changes unexpectedly (anomaly detection)
5. Use the tool creatively for many embedded use cases
6. Stream data into more powerful tools (MATLAB, etc.) when needed — fc_tool is the lightweight foundation

---

## IMPORTANT: Trigger Mode vs Period Mode

These are **separate, independent features** (user decision 2026-02-07):

| Feature | Trigger Mode | Period Mode |
| ------- | ----------- | ----------- |
| **Purpose** | Measure values/deltas on any data | Analyze periodic/repeating signals |
| **Works on** | Any plot, any data, any time | Repetitive data only (vibration, PWM, etc.) |
| **What it does** | Place neon yellow/blue intercept lines, see ΔX/ΔY | Detect period, show N repetitions "standing still" |
| **Requires periodic data?** | **No** | **Yes** |
| **Per-plot?** | Yes | Yes |
| **Can be used together?** | Yes — both active on same plot | Yes |
| **Period count** | N/A | Free number input, default 1, greyed out when disabled |

**Trigger Mode** is documented in [cursor-interaction-discussion.md](cursor-interaction-discussion.md).
**Period Mode** is documented below.

Users may also want trigger mode on non-periodic data — it's purely a measurement tool.

---

## Feature 1: Period Mode (Pattern Detection)

### Concept

When data is repetitive (e.g., a sine wave, PWM signal, vibration), instead of scrolling thousands of identical periods, detect the period length and display just 1 to N periods. The signal appears "standing still" on the plot.

### How Oscilloscopes Do It

Real oscilloscopes use a **trigger** mechanism:
- Set a voltage threshold (trigger level)
- Wait for signal to cross threshold in a specific direction (rising/falling edge)
- Display one screen width of data starting from trigger point
- Repeat for each trigger event → signal appears "standing still"

### Approaches for Software Implementation

| Approach | Description | Complexity | Accuracy |
|----------|-------------|------------|----------|
| Zero-crossing detection | Count crossings of mean value | Low | Good for simple waves |
| Autocorrelation | Correlate signal with shifted copy | Medium | Excellent for noisy signals |
| FFT peak detection | Find dominant frequency via FFT | Medium | Best for frequency identification |
| Threshold trigger | User-set trigger level (like oscilloscope) | Low | Most intuitive for EE users |
| Peak-to-peak detection | Find recurring peaks | Low | Good for clean signals |

### Questions

- [x] Q21: What should the pattern mode be called? **→ "Period Mode" for periodic data; "Trigger Mode" is separate (measurement intercepts)**
- [x] Q22: Should period count be user-configurable (1, 2, 5, N) or fixed presets? **→ Free number input, default 1, greyed out when Period Mode disabled**
- [ ] Q25: Should pattern detection work automatically or require a manual "detect period" button?
- [ ] Q-NEW-1: Should there be a manual trigger level control (like an oscilloscope)?
- [ ] Q-NEW-2: Support for rising edge, falling edge, or both trigger types?
- [ ] Q-NEW-3: Should the detected frequency/period be displayed numerically?

---

## Feature 2: Anomaly Detection Overlay

### Concept

While viewing a signal in pattern mode (standing still), track statistical properties over time. When the signal shape changes (drift, shift, spike), highlight it so the user notices faster than they could by watching raw data.

### What to Track

| Metric | Description | Use Case |
|--------|-------------|----------|
| Max value | Peak amplitude per period | Amplitude drift detection |
| Min value | Trough per period | Asymmetric signal changes |
| Mean value | DC offset per period | Bias drift |
| RMS value | Signal energy per period | Power changes |
| Period length | Time between triggers | Frequency drift |
| Rise/fall time | Slope at zero crossings | Waveform shape changes |
| Critical points | Local maxima/minima count | Waveform distortion |

### Visualization Ideas

1. **Overlay markers** — Small arrows or dots on the plot showing where max/min occurred
2. **Trend mini-graph** — Small sparkline below the main plot showing how max/min evolve over time
3. **Color shift** — Plot line color changes when anomaly detected (e.g., green → yellow → red)
4. **Status bar** — Text showing "Max: 1.05 (↑0.02 from baseline)" below the plot
5. **Alert threshold** — User sets acceptable range, visual alert when exceeded

### Questions

- [ ] Q23: How prominent should anomaly markers be? Subtle overlay or bold alerts?
- [ ] Q24: Should anomaly tracking show a separate mini-graph of critical point trends over time?
- [ ] Q-NEW-4: Should baseline be auto-detected or user-configurable?
- [ ] Q-NEW-5: Alert sound/notification when anomaly exceeds threshold?
- [ ] Q-NEW-6: Should anomaly history be exportable (for post-analysis)?

---

## Feature 3: Per-Plot Mode Selection

### Concept

Different plots serve different purposes in the same session. Some need continuous scrolling history (altitude, temperature), others need pattern view (vibration, PWM). Users should be able to set the mode per individual plot.

### Modes Per Plot

| Mode | Behavior | Best For |
|------|----------|----------|
| Continuous (scrolling) | Default. Data scrolls left, newest on right | Long-term trends |
| Continuous (expanding) | X-axis grows with data | Full capture analysis |
| Pattern (N periods) | Detect period, show N repetitions | Repetitive signals |
| Single period | Show exactly one detected period | Frequency/shape analysis |
| Frozen | Static snapshot, no updates | Inspection |

### UI Concept

Each plot header has a mode dropdown:

```text
┌──────────────────────────────────────────────────────┐
│ Plot #1  [Mode: Continuous ▼]  [Auto-fit] [+] [-]   │
│ Plot #2  [Mode: Pattern (2) ▼] [Auto-fit] [+] [-]   │
│ Plot #3  [Mode: Single period ▼] [Auto-fit] [+] [-]  │
└──────────────────────────────────────────────────────┘
```

### Questions

- [ ] Q-NEW-7: Should mode selection persist across sessions (saved to preferences)?
- [ ] Q-NEW-8: Default mode for new plots: always Continuous, or auto-detect?

---

## Technical Considerations

### Performance

- Period detection adds CPU overhead per plot
- Autocorrelation on 1000 samples ≈ O(n²) or O(n log n) with FFT
- Should run in a Web Worker to avoid blocking UI
- Detection can run at lower frequency than data updates (e.g., every 100ms)

### Libraries (researched — see findings)

- **fft.js** (4KB) — Recommended FFT library, radix-4 Cooley-Tukey, maintained
- **pitchy** (8KB) — McLeod Pitch Method with clarity metric, worth investigating
- **fili** (15KB) — IIR/FIR digital filters, optional
- **DSP.js** — ABANDONED (2013), do not use
- **ml-autocorrelation** — Does not exist as standalone package, implement via FFT trick
- **chartjs-plugin-annotation** v3.x — For overlay markers, compatible with Chart.js 4
- Total dependency footprint: ~6-20KB

See [findings/signal-period-detection-research.md](findings/signal-period-detection-research.md) for full library comparison and code.

### Data Flow

```text
Serial Data → Parser → Per-Plot Buffer → [Pattern Detector] → Chart Renderer
                                              ↓
                                     [Anomaly Tracker] → Overlay / Alerts
```

---

## Comparison with Existing Tools

| Feature | Arduino Plotter | PulseView | fc_tool (proposed) |
|---------|----------------|-----------|-------------------|
| Continuous plot | Yes | Yes | Yes |
| Trigger/pattern | No | Yes (logic) | Yes (analog signals) |
| Anomaly detection | No | Protocol decode | Yes (statistical) |
| Multi-graph | No | Yes | Yes |
| Per-plot modes | No | Per-channel | Yes |
| Lightweight | Yes | Medium | Yes (web-based) |

---

## Implementation Priority

Suggested order (to be discussed):

1. **Pattern mode with zero-crossing** — Simplest period detection
2. **Trigger level control** — Gives users manual control
3. **Anomaly overlay (max/min tracking)** — Most useful metric
4. **FFT-based period detection** — Better accuracy
5. **Anomaly mini-graph** — Nice-to-have visualization
6. **Exportable anomaly history** — Future

---

## References (Research Complete)

### Research Documents

- [signal-period-detection-research.md](findings/signal-period-detection-research.md) — Zero-crossing, autocorrelation, FFT, threshold trigger, peak-to-peak detection. Full JS implementations and library comparison.
- [anomaly-detection-research.md](findings/anomaly-detection-research.md) — EMA, CUSUM, rolling stats, waveform comparison, peak/trough tracking. AnomalyEngine orchestrator. Chart.js visualization techniques.
- [chartjs-signal-visualization-research.md](findings/chartjs-signal-visualization-research.md) — TriggerEngine, PlotController, per-plot modes, annotations, sparklines, readout bars. Library comparison (Chart.js vs uPlot vs SmoothieChart).
- [cursor-crosshair-research.md](findings/cursor-crosshair-research.md) — Custom crosshair plugin, measurement lines (drag/place), multi-chart state management. No extra npm deps needed.
- [dark-theme-neon-research.md](findings/dark-theme-neon-research.md) — Dark theme setup, neon palette (8 colors), origin axes, segment coloring for anomalies, library comparison (stay with Chart.js).

### Related Discussion Documents

- [cursor-interaction-discussion.md](cursor-interaction-discussion.md) — Trigger mode cursor system, visual style, readout panel
- [plotter_discussion.md](plotter_discussion.md) — Main plotter features, scaling controls, pause mode

---

*This document is a working discussion. All research findings are now complete (7/7 documents in findings/).*
