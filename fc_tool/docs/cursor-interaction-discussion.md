# Cursor & Interaction System — Discussion & Design

> **Status**: DECISIONS CAPTURED — ready for implementation research
> Last updated: 2026-02-07

This document details the plot cursor, crosshair, and measurement interaction system for fc_tool's enhanced plotter.

---

## Overview

The interaction system has two layers:

1. **Passive hover** — Visual crosshair follows the mouse for orientation
2. **Active measurement (Trigger Mode)** — Place intercept lines to measure deltas

Users toggle between passive and active modes. This is inspired by oscilloscope functionality but designed for a lightweight web tool.

---

## 1. Passive Hover Crosshair

### Behavior

When the mouse is over any plot:

- A **dotted vertical line** (Y-axis intercept) follows the mouse X position
- A **dotted horizontal line** (X-axis intercept) follows the mouse Y position
- Both lines are **light grey / greyed out** (non-intrusive)
- The **readout panel** below the plot shows current mouse X and Y coordinates
- Only the plot under the mouse shows the crosshair (other plots show nothing)

### Origin Axes

- **Solid line at y=0** — always visible if 0 is in the current view range
- **Solid line at x=0** — always visible if 0 is in the current view range
- Origin axes are thin, subtle, and positioned by autoscaling (they float to wherever 0 falls in the view)

### ASCII Mockup

```text
┌──────────────────────────────────────────────────┐
│ Plot #1                          [Axis: OFF] [⚙] │
│ ┌──────────────────────────────────────────────┐ │
│ │            ╎                                 │ │
│ │  ~~~/\~~~~ ╎ ~~~~                            │ │
│ │═══════════════════════════════ (y=0 solid)   │ │
│ │ /    \     ╎    \                            │ │
│ │╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ (hover Y, grey)      │ │
│ │            ╎ (hover X, grey)                 │ │
│ └──────────────────────────────────────────────┘ │
│ X: 1.234  Y: 567.89                             │
└──────────────────────────────────────────────────┘
```

---

## 2. Active Measurement — Trigger Mode

### Enabling

- **[Axis: OFF/ON]** toggle button on each plot header
- When OFF: only passive grey crosshair (no accidental clicks)
- When ON ("Trigger Mode"): enables placing measurement intercept lines

### Axis Selection (Right-Click Toggle)

Right-click on the plot toggles between two modes:

| Mode | Highlighted Line | Color | What It Measures |
|------|-----------------|-------|------------------|
| **Y-intercept mode** (default) | Vertical line follows mouse | **Neon Yellow** | X-axis values (time, samples, etc.) |
| **X-intercept mode** | Horizontal line follows mouse | **Neon Blue** | Y-axis values (sensor readings, etc.) |

**Terminology clarification:**
- "Y-axis intercept" = a **vertical line** that crosses the X-axis (like an asymptote) → Neon Yellow
- "X-axis intercept" = a **horizontal line** that crosses the Y-axis → Neon Blue

### Placing Intercept Lines

- **Left-click** places the highlighted line at that position
- Each plot supports up to **2 yellow vertical lines** (Y-intercepts) and **2 blue horizontal lines** (X-intercepts)
- Total: 4 measurement lines per plot (2 + 2)

### Re-positioning Placed Lines

- If 2 lines of the current type are already placed, left-click **picks up the nearest one** (it detaches and follows the mouse again)
- User can then left-click to place it at the new position

### Delta Display

The readout panel shows deltas between placed lines:

```text
│ Readout:                                         │
│ Mouse: X=1.234  Y=567.89                         │
│ Y1=0.500  Y2=1.750  ΔY=1.250                     │
│ X1=100.0  X2=350.5  ΔX=250.5                     │
```

Note: X-axis may not always be time — keep labels generic.

### Full Trigger Mode Mockup

```text
┌──────────────────────────────────────────────────┐
│ Plot #1              [Axis: ON ▾ Y-intercept] [⚙] │
│ ┌──────────────────────────────────────────────┐ │
│ │        ║Y1           ║Y2                     │ │
│ │  ~~~/\ ║ ~~~~       ║                        │ │
│ │═══════════════════════════════ (y=0 solid)   │ │
│ │ /    \ ║    \       ║                        │ │
│ │════════════════════════════ X1 (neon blue)   │ │
│ │        ║      ~~~~  ║                        │ │
│ │════════════════════════════ X2 (neon blue)   │ │
│ │        ║ (neon yellow Y1, Y2)                │ │
│ └──────────────────────────────────────────────┘ │
│ Mouse: X=1.234  Y=567.89                         │
│ Y1=0.500  Y2=1.750  ΔY=1.250 (neon yellow)       │
│ X1=100.0  X2=350.5  ΔX=250.5 (neon blue)         │
└──────────────────────────────────────────────────┘
```

---

## 3. Visual Style

### Plot Theme: Dark Background

| Element | Style | Color |
|---------|-------|-------|
| Plot background | Solid dark | `#1a1a2e` or `#0d1117` |
| Grid lines | Thin, subtle | `#333` or `#2d2d2d` |
| Origin axes (x=0, y=0) | Solid, thin | `#555` or `#666` |
| Passive crosshair | Dotted | `#888` (grey) |
| Y-intercept lines (vertical) | Solid | `#FFFF00` (neon yellow) |
| X-intercept lines (horizontal) | Solid | `#00BFFF` (neon blue) |
| Data line colors | Solid, bright | See palette below |
| Data point markers | Small circles | Same as line color (toggled) |

### Data Line Color Palette

Multiple functions on the same plot get distinct colors. Not all neon — a mix of bright and readable:

| Index | Color | Hex | Name |
|-------|-------|-----|------|
| 1 | Bright green | `#00FF88` | Lime |
| 2 | Bright red | `#FF4444` | Coral |
| 3 | Bright cyan | `#00DDFF` | Cyan |
| 4 | Orange | `#FF8C00` | Orange |
| 5 | Magenta | `#FF44FF` | Magenta |
| 6 | White | `#FFFFFF` | White |
| 7 | Light purple | `#BB88FF` | Lavender |
| 8 | Bright pink | `#FF6B9D` | Pink |

Colors cycle if more than 8 datasets.

### "Show Data Points" Toggle

- OFF (default): Plot shows smooth lines only
- ON: Small circles at each actual data point on the line
- Per-plot toggle button

---

## 4. Readout Panel

### Concept

Each plot has a dedicated readout panel below it. This panel is future-expandable for more analytics.

### Current Readout Contents

```text
│ Mouse: X=1.234  Y=567.89                         │
│ Y1=0.500  Y2=1.750  ΔY=1.250                     │  ← only when lines placed
│ X1=100.0  X2=350.5  ΔX=250.5                     │  ← only when lines placed
```

### Future Expandability

The readout panel can grow to include:
- Signal statistics (min, max, mean, RMS) — from period mode
- Frequency/period detection results
- Anomaly alerts
- Custom user metrics

### Architecture Note

**Dedicated modules/files for plot analytics.** The readout and analytics should be modular so features can be added without bloating the core plotting code. Suggested file structure:

```text
fc_tool/src/
  plotter/
    chart-manager.js      — Chart.js instance management
    cursor-system.js      — Crosshair, intercept lines, hover
    readout-panel.js      — Readout display and formatting
    period-detector.js    — Pattern/period detection
    anomaly-tracker.js    — Anomaly detection overlay
    trigger-mode.js       — Trigger mode state management
    color-palette.js      — Dark theme colors
```

---

## 5. Trigger Mode vs Period Mode (Clarification)

These are **separate, independent features**:

| Feature | Trigger Mode | Period Mode |
|---------|-------------|-------------|
| Purpose | Measure values on any data | Analyze periodic/repeating signals |
| When to use | Any plot, any time | Repetitive data (vibration, PWM, etc.) |
| What it does | Place intercept lines, see deltas | Detect period, show N repetitions |
| Requires periodic data? | No | Yes |
| Per-plot? | Yes | Yes |
| Can be used together? | Yes | Yes |

**Trigger Mode** = measurement tool (neon yellow/blue intercept lines)
**Period Mode** = display mode (show N periods of detected pattern)

Both can be active on the same plot simultaneously.

---

## 6. Installation & Dependencies

### Philosophy

fc_tool should either:
- Come as a **single executable** with no runtime dependencies, OR
- Include **comprehensive install scripts** (bash/bat) that, given sudo/admin, set up everything needed

### Current Approach

Tauri produces a single binary that bundles the web frontend. No Node.js or browser install needed at runtime. Build dependencies are handled by `dev_setup/` scripts.

### For Signal Analysis Libraries

If JS signal processing libraries (FFT, autocorrelation) are needed, they should be:
1. **Bundled at build time** (npm install during build, bundled into the app)
2. **No runtime downloads** — works offline
3. Pure JavaScript (no native modules that complicate cross-platform builds)

---

## Open Questions

### Cursor Behavior

- [ ] Q-C1: Should right-click context menu be suppressed on plots (to use right-click for mode toggle)?
- [ ] Q-C2: Should there be keyboard shortcuts for switching Y/X intercept mode (e.g., V for vertical, H for horizontal)?
- [ ] Q-C3: Should intercept lines have labels on them (e.g., "Y1=0.500" shown next to the line)?
- [ ] Q-C4: Should the readout panel be collapsible/hideable?

### Visual Style

- [ ] Q-V1: Exact dark background color preference? (GitHub dark `#0d1117` vs midnight blue `#1a1a2e`)
- [ ] Q-V2: Grid line density — should it be configurable or fixed?
- [ ] Q-V3: Should the color palette be user-customizable (future)?

---

## References

- [plotter_discussion.md](plotter_discussion.md) — Main plotter feature discussion
- [signal-analysis-discussion.md](signal-analysis-discussion.md) — Period mode and anomaly detection
- [findings/chartjs-oscilloscope-research.md](findings/chartjs-oscilloscope-research.md) — Chart.js plugin research

---

*This document captures the complete cursor/interaction design. Update as implementation progresses.*
