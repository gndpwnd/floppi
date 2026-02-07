# Plotter Feature Discussion

> **Status**: IMPORTANT DISCUSSION — needs user input before implementation
> Last updated: 2026-02-06

This document captures plotter feature ideas and questions for discussion. See [findings/multi-graph-plotter-research.md](findings/multi-graph-plotter-research.md) for technical research.

---

## User Requirements Summary

### Core Features Requested

1. **Oscilloscope-like markers** — X and Y cursor pairs for measurement
2. **Pause/freeze mode** — Stop display while data continues buffering
3. **Mouse hover values** — Show intersection coordinates on hover
4. **Auto-scaling axes** — Handle vastly different ranges (x=1, y=10000)
5. **Dynamic scaling** — Expand from 0.1 to millions as data grows
6. **Zoom with Ctrl+scroll** — Regular scroll for text selection, Ctrl+scroll for zoom

### UI Flow

1. Serial Monitor shown first (primary view)
2. "Show Plotter" button reveals charts
3. Plotter can be hidden again to focus on monitor

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

**Questions:**

- [ ] Q1: Should cursors be draggable or controlled via input fields?
- [ ] Q2: Show delta values (ΔX, ΔY) in a panel or on the plot itself?
- [ ] Q3: Should cursor pairs move together or independently?
- [ ] Q4: Color-code cursors (e.g., X=blue, Y=red)?

---

### 2. Pause/Freeze Mode

**Concept**: Stop the display from scrolling while data continues arriving in a buffer.

**Modes:**
- **Live**: Plot updates in real-time (current behavior)
- **Paused**: Display frozen, data buffered
- **Resume**: Flush buffer and continue live

**Questions:**

- [ ] Q5: When resuming, show all buffered data at once or animate through it?
- [ ] Q6: Show a buffer indicator (e.g., "Paused - 150 samples buffered")?
- [ ] Q7: Maximum buffer size before oldest data is dropped?

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

**Implementation options:**
- Tooltip following mouse
- Fixed readout panel (always visible)
- Both (tooltip + panel)

**Questions:**

- [ ] Q8: Tooltip, fixed panel, or both?
- [ ] Q9: Show interpolated values between data points?
- [ ] Q10: Snap to nearest data point or show exact mouse position?

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

**User preference**: Button-based zoom (not scroll-based)
- Regular scroll = text selection scrolling (unchanged)
- Corner buttons: [+] and [-] for zoom in/out

**Proposed UI:**
```
┌─────────────────────────────────────────────┐
│ Plot #1                           [+] [-] ⟲ │
│ ┌─────────────────────────────────────────┐ │
│ │                                         │ │
│ │         ~~~~/\~~~~                      │ │
│ │        /    \                           │ │
│ └─────────────────────────────────────────┘ │
└─────────────────────────────────────────────┘

[+] = Zoom in
[-] = Zoom out
⟲  = Reset zoom (fit all)
```

**Additional options:**
- Keyboard shortcuts: `+`/`-` keys when plot focused
- Zoom percentage indicator (e.g., "150%")
- Pinch-to-zoom on touch devices (future)

**Questions:**

- [ ] Q15: Zoom X-axis only, Y-axis only, or both together?
- [ ] Q16: Show zoom percentage indicator?
- [ ] Q17: Add "fit all" reset button (⟲)?

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
