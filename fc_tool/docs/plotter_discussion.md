# Plotter Feature Discussion

> **Status**: IN PROGRESS — decisions being captured, research ongoing
> Last updated: 2026-02-07

This document captures plotter feature ideas and questions for discussion. See [findings/multi-graph-plotter-research.md](findings/multi-graph-plotter-research.md) for technical research.

---

## User Requirements Summary

### Core Features Requested

1. **Oscilloscope-like markers** — X and Y cursor pairs for measurement
2. **Pause/freeze mode** — Stop display while data continues buffering
3. **Mouse hover values** — Show intersection coordinates on hover
4. **Auto-scaling axes** — Handle vastly different ranges (x=1, y=10000)
5. **Dynamic scaling** — Expand from 0.1 to millions as data grows
6. **Button-based scaling controls** — [+] [-] ⟲ buttons on plots, font size on serial monitor
7. **Signal/pattern analysis mode** — Detect repetitive patterns, show N periods instead of continuous scroll
8. **Anomaly detection overlay** — Track max/min/critical points over time on signal plots

### UI Flow

1. Serial Monitor shown first (primary view)
2. "Show Plotter" button reveals charts
3. Plotter can be hidden again to focus on monitor
4. Separate "Clear" buttons for plots vs serial monitor

---

## Feature Details

### 1. Measurement Cursors (Oscilloscope-style)

**Concept**: Two vertical (X1, X2) and two horizontal (Y1, Y2) marker lines.

```
     Y2 ─────────────────────────────
        │                           │
        │      ~~~~~/\~~~~          │
        │     /      \              │
     Y1 ─────/────────\─────────────│
        │   /          \            │
        │  /            ~~~~        │
        ├──────────────────────────
       X1                          X2

   ΔX = X2 - X1 (time difference)
   ΔY = Y2 - Y1 (value difference)
```

**Decision: Both draggable AND input field controls.** Users can drag cursors on the plot or type exact values.

**Full design captured in [cursor-interaction-discussion.md](cursor-interaction-discussion.md).**

**Resolved questions:**

- [x] Q1: Should cursors be draggable or controlled via input fields? **→ BOTH**
- [x] Q2: Show delta values (ΔX, ΔY) in a panel or on the plot itself? **→ Separate readout panel below plot (expandable for future analytics)**
- [x] Q3: Should cursor pairs move together or independently? **→ Independently; if 2 placed, left-click picks up nearest one**
- [x] Q4: Color-code cursors (e.g., X=blue, Y=red)? **→ YES: Neon yellow for vertical (Y-intercepts), neon blue for horizontal (X-intercepts)**

---

### 2. Pause/Freeze Mode

**Concept**: Stop the display from scrolling while data continues arriving in a buffer.

**Decision:** When paused, data collection **stops by default**. A global toggle "Keep recording when paused" allows buffering during pause (affects all plots). Separate "Clear" buttons for plots vs serial monitor.

**Modes:**
- **Live**: Plot updates in real-time (current behavior)
- **Paused (default)**: Display frozen, **data collection stops**
- **Paused + Recording**: Display frozen, data buffered (opt-in toggle)
- **Resume**: Continue live (with or without buffered data depending on mode)

**Resolved questions:**

- [x] Q5: When resuming, show all buffered data at once or animate through it? **→ Stop collecting by default; "Keep recording when paused" toggle for buffering**
- [ ] Q6: Show a buffer indicator (e.g., "Paused - 150 samples buffered")? *(Still relevant when "Keep recording" is on)*
- [ ] Q7: Maximum buffer size before oldest data is dropped? *(Still relevant when "Keep recording" is on)*

---

### 3. Mouse Hover Values

**Concept**: Show crosshair at mouse position with X,Y coordinates.

```
        │
        │     ┌─────────────┐
        │     │ X: 1.234    │
   ─────┼─────│ Y: 567.89   │─────
        │     └─────────────┘
        │           │
        │           │
```

**Decision:** The "tooltip" IS the greyed-out crosshair lines (dotted vertical + horizontal following mouse). Plus a fixed readout panel below the plot showing exact coordinates. So effectively: both.

Additional: "Show data points" toggle — when ON, displays small circles at actual data points on the plotted line.

**Resolved questions:**

- [x] Q8: Tooltip, fixed panel, or both? **→ BOTH — grey crosshair lines on plot + readout panel below**
- [x] Q9: Show interpolated values between data points? **→ Show exact mouse position; "Show data points" toggle for actual point markers**
- [x] Q10: Snap to nearest data point or show exact mouse position? **→ Exact mouse position (no snapping)**

---

### 4. Axis Auto-Scaling

**Concept**: Automatically adjust axis ranges based on incoming data.

**Challenges:**
- Very different X/Y ranges (x=1, y=10000)
- Exponential growth (0.1 → 1,000,000)
- Negative values
- Outliers skewing the scale

**Scaling strategies:**
| Strategy | Pros | Cons |
|----------|------|------|
| Linear | Simple, intuitive | Bad for exponential data |
| Logarithmic | Handles large ranges | Can't show zero/negative |
| Adaptive | Best of both | Complex implementation |

**Questions:**

- [ ] Q11: Start with linear and add log toggle, or auto-detect?
- [ ] Q12: How to handle negative values with log scale?
- [ ] Q13: Add manual min/max override controls?
- [ ] Q14: Per-plot scaling or global scaling for all plots?

---

### 5. Zoom Controls

**Concept**: Zoom in/out on the plot to inspect details.

**Renamed: "Scaling Controls"** — zoom is really scaling for plots; "zoom" on serial monitor means font size.

**Decision:** Both axes together by default, with independent X and Y controls available. Auto-fit ON by default (can be toggled off). "Auto fit" button to snap back to fitting data.

**For plots — Scaling Controls:**

- Button-based [+] [-] for both axes together
- Separate X-axis and Y-axis scale controls
- "Auto fit" button (⟲) — fits data to view
- Auto-fit ON by default, toggleable (useful for paused inspection)

**For serial monitor — Font Size:**

- [+] [-] buttons control font size only
- No axis scaling (it's text)

**Proposed UI:**

```text
┌──────────────────────────────────────────────────────────┐
│ Plot #1          [Auto-fit: ON] [X±] [Y±] [+] [-] ⟲    │
│ ┌──────────────────────────────────────────────────────┐ │
│ │                                                      │ │
│ │         ~~~~/\~~~~                                   │ │
│ │        /    \                                        │ │
│ └──────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────┘

[+]  = Scale in (both axes)
[-]  = Scale out (both axes)
[X±] = X-axis scale controls
[Y±] = Y-axis scale controls
⟲    = Reset to auto-fit
[Auto-fit: ON/OFF] = Toggle auto-fitting to data
```

**Resolved questions:**

- [x] Q15: Zoom X-axis only, Y-axis only, or both together? **→ Both together by default, with independent X/Y controls**
- [ ] Q16: Show zoom percentage indicator?
- [x] Q17: Add "fit all" reset button (⟲)? **→ YES, plus auto-fit toggle (ON by default)**

---

### 6. Time Window Modes

**Concept**: Control how the X-axis (time) behaves.

**Modes:**
| Mode | Behavior | Use Case |
|------|----------|----------|
| Scrolling | Fixed window, oldest data falls off | Real-time monitoring |
| Expanding | X-axis grows with data | Analyzing full capture |
| Fixed | Static window, data wraps | Frequency analysis |

**Questions:**

- [ ] Q18: Default mode? (Scrolling seems natural for serial)
- [ ] Q19: Toggle between modes, or mode selector?
- [ ] Q20: Configurable window size (samples or seconds)?

---

### 7. Signal / Pattern Analysis Mode (NEW)

> **IMPORTANT**: This is a major feature idea — see [signal-analysis-discussion.md](signal-analysis-discussion.md) for detailed research and discussion.

**Concept**: Instead of always scrolling continuous data, detect repetitive patterns and show just 1 to N periods. Like an oscilloscope's trigger mode — show the signal "in place" rather than scrolling.

**Use cases:**

- IMU vibration analysis (periodic signals)
- PWM duty cycle monitoring
- Heartbeat/pulse signals
- Any repetitive waveform where you care about shape, not history

**Key ideas from user:**

1. **Pattern/period detection** — Automatically detect when data is repetitive, show N periods instead of continuous scroll
2. **Per-plot toggle** — Some plots need long history (e.g., altitude over time), others are signals (e.g., vibration). User selects "Pattern mode" per individual plot
3. **Anomaly detection** — While showing signal "in place," track max/min/critical points over time. If the signal shape changes, the user sees it via overlay markers (shift detection)
4. **Creative foundation** — Not a full signal analysis suite, but robust enough for users to use creatively for basic signal work

**Proposed UI:**

```text
┌──────────────────────────────────────────────────────────┐
│ Plot #1  [Continuous ▼]  [Periods: 2]  [Anomaly: ON]    │
│ ┌──────────────────────────────────────────────────────┐ │
│ │     /\      /\                                       │ │
│ │    /  \    /  \        ▲ max: 1.05 (shifted +0.02)  │ │
│ │   /    \  /    \                                     │ │
│ │  /      \/      \                                    │ │
│ └──────────────────────────────────────────────────────┘ │
│ Period: 20ms | Freq: 50Hz | Min: -0.98 | Max: 1.05      │
└──────────────────────────────────────────────────────────┘

Mode dropdown: [Continuous] / [Period Mode (N)] / [Single period]
Trigger Mode is separate — see [cursor-interaction-discussion.md](cursor-interaction-discussion.md)
```

**Decisions & questions:**

- [x] Q21: What should the pattern mode be called? **→ "Period Mode" for periodic data; "Trigger Mode" is separate (measurement intercepts on any data)**
- [x] Q22: Should period count be user-configurable (1, 2, 5, N) or fixed presets? **→ Free number input, default 1, greyed out when Period Mode not enabled**
- [ ] Q23: How prominent should anomaly markers be? Subtle overlay or bold alerts?
- [ ] Q24: Should anomaly tracking show a separate mini-graph of critical point trends over time?
- [ ] Q25: Should pattern detection work automatically or require a manual "detect period" button?

**Full design:** See [signal-analysis-discussion.md](signal-analysis-discussion.md) and [cursor-interaction-discussion.md](cursor-interaction-discussion.md).

---

### 8. Separate Clear Buttons (NEW)

**Decision:** Separate "Clear" buttons for plots and serial monitor.

```text
┌────────────────────────────────────────────────┐
│ Serial Monitor            [Clear Monitor] [⚙]  │
│ ┌────────────────────────────────────────────┐ │
│ │ > ax=0.02 ay=-0.01 az=1.01 ...            │ │
│ └────────────────────────────────────────────┘ │
├────────────────────────────────────────────────┤
│ Plots  [Pause] [Keep Recording: OFF]           │
│        [Clear All Plots]                       │
│ ┌────────────────────────────────────────────┐ │
│ │ Plot #1 ...                                │ │
│ └────────────────────────────────────────────┘ │
└────────────────────────────────────────────────┘
```

No open questions — straightforward implementation.

---

## Technical Research Summary

### Recommended Libraries/Plugins

| Feature | Solution | Notes |
|---------|----------|-------|
| Crosshair cursor | chartjs-plugin-crosshair | Real-time value display |
| Static markers | chartjs-plugin-annotation | Threshold lines |
| Pause/buffer | Custom implementation | Decouple data from render |
| Log scale | Chart.js built-in | `type: 'logarithmic'` |
| Zoom | chartjs-plugin-zoom | Ctrl+wheel support |

### Performance Considerations

1. **Disable animations** during real-time updates
2. **Use 'quiet' updates** to skip expensive redraws
3. **Data decimation** for large datasets (LTTB algorithm)
4. **Buffer size limits** to prevent memory bloat
5. **RequestAnimationFrame** for smooth 60 FPS

See [findings/chartjs-oscilloscope-research.md](findings/chartjs-oscilloscope-research.md) for detailed implementation notes.

---

## Priority Discussion

Please rank these features (1=highest priority):

| Feature | Priority (1-5) | Notes |
|---------|----------------|-------|
| Pause/freeze mode | | |
| Mouse hover values | | |
| Measurement cursors | | |
| Ctrl+scroll zoom | | |
| Auto-scaling | | |
| Log scale toggle | | |
| Time window modes | | |

---

## Open Questions Summary

### UX Questions
- Q1-Q4: Cursor behavior and presentation
- Q5-Q7: Pause/buffer handling
- Q8-Q10: Hover tooltip design

### Technical Questions
- Q11-Q14: Axis scaling strategy
- Q15-Q17: Zoom behavior
- Q18-Q20: Time window modes

---

## Next Steps

1. **Discuss**: Review questions above and provide preferences
2. **Prototype**: Implement basic pause + hover first (most useful)
3. **Iterate**: Add cursors and zoom based on usage
4. **Document**: Update protocol if firmware needs changes

---

*This document is a working discussion. Update with decisions as they're made.*
