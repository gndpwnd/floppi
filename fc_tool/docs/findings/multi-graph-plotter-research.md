# Multi-Graph Serial Plotter Research

> Last updated: 2026-02-06

Research findings on serial plotter protocols and multi-graph solutions.

---

## Summary

We researched Arduino Serial Plotter, Teleplot, and other tools to design an enhanced plotting system that:
- Remains **backward-compatible** with Arduino Serial Plotter
- Supports **dynamic multi-graph** assignment
- Uses **sparse plot IDs** (only shows plots that are referenced)

---

## Arduino Serial Plotter Protocol

### Standard Format

Arduino Serial Plotter expects newline-terminated ASCII messages with values separated by space, tab, or comma.

**Unlabeled values:**
```
25.5 30.2 18.7
26.0 31.1 19.2
```

**Labeled values (preferred):**
```
Temperature:25.5 Humidity:30.2 Pressure:18.7
Temperature:26.0 Humidity:31.1 Pressure:19.2
```

### Characteristics

| Feature | Value |
|---------|-------|
| Terminator | `\n` (newline) |
| Separators | space, tab, comma |
| Label format | `Label:Value` |
| X-axis | 500-point rolling window |
| Y-axis | Auto-scaling |
| Multi-variable | All on same plot, different colors |

### Limitations

- **Single plot only** — all variables share one Y-axis
- No way to assign variables to separate plots
- Position-based if unlabeled (fragile)

---

## Teleplot Protocol

Teleplot is a modern telemetry viewer using UDP on port 47269.

### Format

```
varName:value|g
varName:timestamp:value|g
```

- `varName` — variable identifier
- `timestamp` — optional milliseconds since epoch
- `value` — numeric value
- `g` — gauge type indicator

### Characteristics

- **UDP-based** — requires network (WiFi/Ethernet)
- **Name-based assignment** — each variable auto-creates a plot
- **Web visualization** — browser-based
- **Timestamp support** — precise timing

### Limitation

Requires network connectivity, not pure UART serial.

---

## Other Tools Reviewed

| Tool | Transport | Multi-Graph | Notes |
|------|-----------|-------------|-------|
| SerialPlot | Serial | Yes | Qt-based, configurable |
| Better Serial Plotter | Serial | Configurable | Drop-in replacement |
| Serial Studio | Serial | JSON config | Flexible but complex |
| Tauno Serial Plotter | Serial | Limited | Embedded-focused |

---

## fc_tool Enhanced Protocol Design

### Goals

1. **Arduino backward-compatible** — plain `Serial.println()` works
2. **Dynamic plot assignment** — `variable@plotID:value`
3. **Sparse plot IDs** — only referenced IDs create plots
4. **Label support** — named variables for clarity

### Proposed Format

#### Basic (Arduino-compatible)

```
ax:1.23 ay:4.56 az:7.89 gx:0.12 gy:0.34 gz:0.56
```

All variables on **default plot** (plot 0).

#### Extended (Multi-graph)

```
temperature@1:25.5 humidity@2:65.2 pressure@2:1013.25
```

- `temperature` → Plot #1
- `humidity` → Plot #2
- `pressure` → Plot #2 (same plot as humidity)

#### Plot ID Rules

1. No `@N` suffix → default plot (plot 0 or first plot)
2. `@N` assigns variable to plot N
3. Only referenced plot IDs are displayed
4. Example: IDs `1, 3, 11` → creates 3 plots labeled "#1", "#3", "#11"
5. Plot IDs are integers (0-999 recommended)

#### Mixed Usage

```
ax:1.23 ay:4.56 az:7.89 temp@1:25.5 altitude@2:100
```

- `ax`, `ay`, `az` → Default plot (IMU data)
- `temp` → Plot #1
- `altitude` → Plot #2

### Firmware Implementation

**Simple (default plot):**
```cpp
// All variables on one plot
Serial.print("ax="); Serial.print(ax, 2);
Serial.print(" ay="); Serial.print(ay, 2);
Serial.print(" az="); Serial.println(az, 2);
```

**Multi-graph:**
```cpp
// Variables on separate plots
char buf[100];
sprintf(buf, "temp@1:%.1f humidity@1:%.1f altitude@2:%.1f",
        temperature, humidity, altitude);
Serial.println(buf);
```

Or using a helper:
```cpp
void plotVar(const char* name, int plotId, float value) {
  Serial.print(name);
  Serial.print("@");
  Serial.print(plotId);
  Serial.print(":");
  Serial.print(value, 2);
  Serial.print(" ");
}

void loop() {
  plotVar("temp", 1, temperature);
  plotVar("humidity", 1, humidity);
  plotVar("altitude", 2, altitude);
  Serial.println();
}
```

### Parser Implementation (JavaScript)

```javascript
// Parse line: "temp@1:25.5 humidity@2:65.2 pressure@2:1013.25"
function parsePlotLine(line) {
  const entries = [];
  const parts = line.trim().split(/\s+/);

  for (const part of parts) {
    // Match: name@plotId:value or name:value
    const match = part.match(/^(\w+)(?:@(\d+))?[=:]([-\d.]+)$/);
    if (match) {
      entries.push({
        name: match[1],
        plotId: match[2] ? parseInt(match[2]) : 0,  // default plot 0
        value: parseFloat(match[3])
      });
    }
  }

  return entries;
}

// Example usage:
parsePlotLine("temp@1:25.5 humidity@2:65.2 ax:1.23")
// Returns:
// [
//   { name: "temp", plotId: 1, value: 25.5 },
//   { name: "humidity", plotId: 2, value: 65.2 },
//   { name: "ax", plotId: 0, value: 1.23 }
// ]
```

---

## UI Design

### Default View

- **Serial Monitor** shown first (terminal)
- **"Show Plotter"** button to reveal plots

### Plotter View

- Plots created dynamically as data arrives
- Each plot labeled with its ID: "#1", "#2", etc.
- Variables listed in legend with colors
- Plots arranged vertically or in grid

### Plot Management

```
┌─────────────────────────────────────────────────────┐
│ Serial Monitor                         [Show Plotter]│
├─────────────────────────────────────────────────────┤
│ > temp@1:25.5 humidity@1:65 altitude@2:100          │
│ > temp@1:25.6 humidity@1:64 altitude@2:102          │
│ > temp@1:25.4 humidity@1:66 altitude@2:105          │
└─────────────────────────────────────────────────────┘

[After clicking "Show Plotter"]

┌─────────────────────────────────────────────────────┐
│ Plot #1 — temp (red), humidity (blue)               │
│ ┌─────────────────────────────────────────────────┐ │
│ │ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~     │ │
│ │ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~     │ │
│ └─────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────┤
│ Plot #2 — altitude (green)                          │
│ ┌─────────────────────────────────────────────────┐ │
│ │                    ~~~~~~~~~~~~~~~~~~~~~~~~     │ │
│ │ ~~~~~~~~~~~~~~~~~~~                             │ │
│ └─────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────┘
```

---

## Comparison: fc_tool vs Arduino Serial Plotter

| Feature | Arduino Plotter | fc_tool Enhanced |
|---------|-----------------|------------------|
| Multiple variables | Yes (same plot) | Yes |
| Multiple plots | No | Yes (dynamic) |
| Variable naming | `Label:Value` | `Label:Value` |
| Plot assignment | N/A | `Label@N:Value` |
| Sparse IDs | N/A | Yes (1,3,11 → 3 plots) |
| Backward compatible | — | Yes |

---

## References

### Arduino Serial Plotter

- [Arduino Serial Plotter Protocol](https://github.com/arduino/Arduino/blob/master/build/shared/ArduinoSerialPlotterProtocol.md) — Official specification
- [Arduino Serial Plotter Tutorial](https://docs.arduino.cc/software/ide-v2/tutorials/ide-v2-serial-plotter) — Official documentation
- [Multiple Values Tutorial](https://www.norwegiancreations.com/2016/01/tutorial-multiple-values-in-the-arduino-ide-serial-plotter/) — Norwegian Creations

### Teleplot

- [Teleplot GitHub](https://github.com/nesnes/teleplot) — Source code
- [Teleplot.fr](https://teleplot.fr/) — Official site

### Other Tools

- [SerialPlot](https://github.com/hyOzd/serialplot) — Qt-based plotter
- [Better Serial Plotter](https://hackaday.io/project/181686-better-serial-plotter) — Enhanced plotter
- [Serial Studio](https://github.com/Serial-Studio/Serial-Studio) — JSON-configurable

---

*This research informs fc_tool's enhanced plotting implementation.*
