# Serial Protocol Lexicon

> Last updated: 2026-02-18
> Status: Definitive reference for fc_tool serial protocol

---

## Overview

fc_tool accepts **plain ASCII text over USB serial** — the same output produced by Arduino's `Serial.print()` and `Serial.println()`. No special library required. No binary protocol. No handshake.

The protocol is **data-only**: firmware sends values, fc_tool visualizes them. There are no control commands, mode strings, or configuration directives in the serial stream. All visualization settings (colors, modes, zoom) are controlled in the fc_tool GUI.

### Design Principles

- **Zero firmware complexity** — works with raw `Serial.print()` calls
- **Backwards compatible** — anything that plots in Arduino IDE Serial Plotter also plots in fc_tool
- **Progressive enhancement** — simple formats work, richer formats unlock more features
- **Data-only protocol** — no control commands mixed with telemetry

---

## Connection Parameters

| Parameter | Value | Notes |
|-----------|-------|-------|
| Baud rate | 115200 (default) | 9600–921600 supported |
| Data bits | 8 | Standard |
| Parity | None | Standard |
| Stop bits | 1 | Standard |
| Flow control | None | No RTS/CTS or XON/XOFF |
| Line terminator | `\n` | Matches `Serial.println()` |

These match Arduino's `Serial.begin(115200)` defaults.

---

## Plotter Protocol

fc_tool's plotter parser (`plotter.js`) uses this regex:

```
/([\w.]+)(?:@(\d+))?[=:]([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)/g
```

### Format 1: Named with Plot Assignment (fc_tool extension)

```
name@plotId:value
```

Assigns a variable to a specific plot. This is the **most powerful format** — it enables multi-graph visualization where related variables share a chart.

| Component | Required | Description |
|-----------|----------|-------------|
| `name` | Yes | Variable name (`[\w.]+` — letters, digits, underscore, dot) |
| `@plotId` | No | Plot assignment (integer). Omit for default plot 0 |
| `:` or `=` | Yes | Separator (colon or equals) |
| `value` | Yes | Numeric value (integer, float, scientific notation) |

**Examples:**
```
ax@0:1.02 ay@0:-0.03 az@0:9.81
temp@1:25.3 humidity@1:62.0
altitude@2:152.7
motor1@3:1500 motor2@3:1520 motor3@3:1480 motor4@3:1510
```

**Arduino code:**
```cpp
// IMU data on plot 0, temperature on plot 1
Serial.print("ax@0:"); Serial.print(accel_x, 2);
Serial.print(" ay@0:"); Serial.print(accel_y, 2);
Serial.print(" az@0:"); Serial.print(accel_z, 2);
Serial.print(" temp@1:"); Serial.println(temperature, 1);
```

**Or with a helper function (no library needed):**
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
  plotVar("ax", 0, accel_x);
  plotVar("ay", 0, accel_y);
  plotVar("az", 0, accel_z);
  plotVar("temp", 1, temperature);
  Serial.println();  // end the line
}
```

### Format 2: Named without Plot Assignment (Arduino compatible)

```
name:value
name=value
```

Variables go to **default plot 0**. The colon format is compatible with Arduino IDE Serial Plotter (v1.8.10+).

**Examples:**
```
temp:25.3 humidity:62.0 pressure:1013.2
CH1:1500 CH2:1500 CH3:1000 CH4:1500 CH5:1000 CH6:1000
```

**Arduino code:**
```cpp
Serial.print("temp:"); Serial.print(temperature, 1);
Serial.print(" humidity:"); Serial.print(humidity, 1);
Serial.print(" pressure:"); Serial.println(pressure, 1);
```

### Format 3: Plain Numbers (universal fallback)

```
25.3 62.0 1013.2
25.3,62.0,1013.2
25.3\t62.0\t1013.2
```

Space, comma, or tab separated. Auto-named `ch0`, `ch1`, `ch2`, etc. All go to plot 0. Compatible with every serial plotter tool.

**Arduino code:**
```cpp
Serial.print(val1); Serial.print(",");
Serial.print(val2); Serial.print(",");
Serial.println(val3);
```

### Format Mixing

Formats can be mixed on a single line:

```
ax:1.02 ay:4.56 az:9.81 temp@1:25.3
```

- `ax`, `ay`, `az` → plot 0 (no `@` specified)
- `temp` → plot 1

### Values Accepted

| Type | Examples | Notes |
|------|----------|-------|
| Integer | `1500`, `-42`, `+100` | |
| Float | `25.3`, `-0.01`, `.5` | Leading zero optional |
| Scientific | `1.23e-4`, `9.81E+0` | Upper or lower case E |

### What the Plotter Ignores

Lines that don't match any format are passed through to the serial monitor as text but are **not plotted**. This means debug messages, status strings, and ANSI-formatted output coexist safely with plotter data.

```
READY                          ← text, not plotted
ax@0:1.02 ay@0:-0.03           ← plotted
[INFO] Calibrating gyro...     ← text, not plotted
gx@0:0.5 gy@0:-0.3 gz@0:0.1   ← plotted
```

---

## Dashboard Protocol

The Live Dashboard shows key=value pairs updating in place (not scrolling). Uses `=` separator only — this avoids conflict with the plotter's `:` format.

**Regex:** `/([\w.]+)=(\S+)/g`

**Format:**
```
key=value
```

Any non-whitespace string is accepted as a value (not just numbers). Multiple pairs per line.

**Examples:**
```
mode=ANGLE armed=false battery=11.4V
loopHz=500 cpuLoad=42% freeRAM=128K
```

**Arduino code:**
```cpp
Serial.print("mode="); Serial.print(flightMode);
Serial.print(" armed="); Serial.print(armed ? "true" : "false");
Serial.print(" battery="); Serial.print(voltage, 1); Serial.print("V");
Serial.println();
```

### Dashboard vs Plotter Overlap

The `=` sign triggers **both** dashboard and plotter parsing:
- `temp=25.3` → Dashboard shows `temp: 25.3` AND plotter adds to plot 0
- `temp:25.3` → Plotter only (`:` doesn't trigger dashboard)
- `mode=ANGLE` → Dashboard only (non-numeric value ignored by plotter)

**Recommendation:** Use `=` for status/dashboard data, `:` for plotter-only data.

---

## ANSI Escape Codes

fc_tool renders ANSI SGR (Select Graphic Rendition) escape sequences as styled text. Toggle via the "ANSI" checkbox in the toolbar.

### Supported Codes

| Code | Effect | Reset Code |
|------|--------|------------|
| `\033[0m` | Reset all | — |
| `\033[1m` | **Bold** | `\033[22m` |
| `\033[2m` | Dim | `\033[22m` |
| `\033[4m` | Underline | `\033[24m` |
| `\033[30m`–`\033[37m` | Standard colors (8) | `\033[39m` |
| `\033[90m`–`\033[97m` | Bright colors (8) | `\033[39m` |

### Color Map

| Code | Color | Code | Bright Color |
|------|-------|------|--------------|
| 30 | Black | 90 | Bright Black (gray) |
| 31 | Red | 91 | Bright Red |
| 32 | Green | 92 | Bright Green |
| 33 | Yellow | 93 | Bright Yellow |
| 34 | Blue | 94 | Bright Blue |
| 35 | Magenta | 95 | Bright Magenta |
| 36 | Cyan | 96 | Bright Cyan |
| 37 | White | 97 | Bright White |

### Compound Codes

Multiple codes can be combined with semicolons:

```cpp
Serial.print("\033[1;31m");  // Bold red
Serial.print("ERROR: ");
Serial.print("\033[0m");     // Reset
Serial.println("Motor overheat detected");
```

### Arduino ANSI Macros (no library needed)

```cpp
#define ANSI_RESET  "\033[0m"
#define ANSI_BOLD   "\033[1m"
#define ANSI_RED    "\033[31m"
#define ANSI_GREEN  "\033[32m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_CYAN   "\033[36m"

Serial.println(ANSI_BOLD ANSI_GREEN "System OK" ANSI_RESET);
Serial.println(ANSI_BOLD ANSI_RED "FAULT: Sensor timeout" ANSI_RESET);
```

### Performance Note

At 115200 baud, ANSI sequences add ~4–8 bytes overhead per styled segment. At 50 Hz telemetry rate with modest coloring, this is negligible. fc_tool's parser has zero overhead when no `\033[` sequences are present (early-exit check).

---

## Plotter Behavior

### Dynamic Plot Creation

Plots are created on demand when new plot IDs appear in the data stream. No pre-registration needed.

- First line with `@3` → plot 3 created
- First line with `temp:25.3` → plot 0 created (if it doesn't exist)
- Plot headers auto-update: `Plot 0: ax, ay, az` (from variable names in data)

### Hard Limits

| Limit | Value | What Happens |
|-------|-------|--------------|
| Max simultaneous plots | 10 | Console warning, additional plots ignored |
| Max data points per plot | 200 (default) | Oldest points trimmed. Adjustable 50–5000 in GUI |
| Max plot ID | No limit | Sparse IDs OK: 0, 5, 99 → 3 plots |

### Plot Lifecycle

| Action | Result |
|--------|--------|
| New plot ID in data | Plot created automatically |
| Close button (x) on plot | Plot destroyed. **Auto-recreates** if data continues |
| "Clear" button / Ctrl+Shift+C | All plots destroyed. Auto-recreate on new data |
| Data stops for a plot | Plot stays visible with last data (no timeout) |

### Statistics

Each plot shows running min/max/avg per variable below the chart. Stats reset when a plot is recreated (after close/clear).

---

## Arduino IDE Compatibility

fc_tool's protocol is a **superset** of Arduino IDE Serial Plotter's format.

### What Works in Both

| Format | Arduino IDE | fc_tool |
|--------|:-----------:|:-------:|
| `name:value` | Yes (v1.8.10+) | Yes |
| Plain CSV `25.3,60.0` | Yes | Yes |
| Space-separated `25.3 60.0` | Yes | Yes |
| Tab-separated `25.3\t60.0` | Yes | Yes |

### fc_tool Extensions (not in Arduino IDE)

| Format | fc_tool | Notes |
|--------|:-------:|-------|
| `name@plotId:value` | Yes | Multi-graph routing. Arduino IDE shows `name@0` as label |
| `name=value` | Yes | Dashboard + plotter. Arduino IDE does not parse `=` |
| ANSI escape codes | Yes | Arduino IDE strips or ignores them |
| Dashboard key=value grid | Yes | fc_tool-specific feature |

### Arduino IDE Quirks to Know

- **Header format** (`label:\tlabel2:\n` then value-only lines) — fc_tool does NOT support this. The inline `label:value` format is recommended anyway since the header format is buggy in Arduino IDE 2.x.
- **No space after colon** — `temp:25.3` works. `temp: 25.3` breaks Arduino IDE (space is a separator). fc_tool handles both.
- **Separator** — Arduino IDE accepts space, tab, and comma between fields. fc_tool uses regex matching so any whitespace between key:value pairs works.

### Firmware That Works Everywhere

This output format works in Arduino IDE Serial Plotter, fc_tool, and any other `label:value` plotter:

```cpp
void loop() {
  Serial.print("ax:"); Serial.print(accel_x, 2);
  Serial.print(" ay:"); Serial.print(accel_y, 2);
  Serial.print(" az:"); Serial.println(accel_z, 2);
  delay(20);  // 50 Hz
}
```

To also use fc_tool's multi-graph feature (while remaining readable in Arduino IDE):

```cpp
void loop() {
  // These plot on separate graphs in fc_tool,
  // all on one graph in Arduino IDE (labels shown as "ax@0" etc.)
  Serial.print("ax@0:"); Serial.print(accel_x, 2);
  Serial.print(" ay@0:"); Serial.print(accel_y, 2);
  Serial.print(" az@0:"); Serial.print(accel_z, 2);
  Serial.print(" temp@1:"); Serial.println(temperature, 1);
  delay(20);
}
```

---

## Comparison with Other Tools

| Feature | Arduino IDE Plotter | fc_tool | Teleplot (VS Code) |
|---------|:-------------------:|:-------:|:------------------:|
| `name:value` | Yes | Yes | No (needs `>` prefix) |
| `name@plotId:value` | No | Yes | No |
| Plain CSV | Yes | Yes | Yes |
| Dashboard mode | No | Yes | No |
| ANSI colors | No | Yes | No |
| Multi-graph | No (one graph) | Yes (10 graphs) | Yes |
| Timestamps | Arrival time | Arrival time | Explicit optional |
| XY plots | No | No | Yes |
| Max variables | ~500 | 10 plots x 8 colors | Unlimited |
| Requires prefix | No | No | Yes (`>`) |
| Requires library | No | No | No (but has one) |

---

## Timing and Throughput

| Baud Rate | Max msg/s (60-char lines) | Recommended Use |
|-----------|--------------------------|-----------------|
| 9600 | ~16 | Slow debug only |
| 115200 | ~192 | Standard telemetry (50–100 Hz) |
| 460800 | ~768 | High-rate logging |
| 921600 | ~1536 | Maximum throughput |

**Recommended:** 115200 baud at 50 Hz. This is the sweet spot for flight controller telemetry — fast enough for real-time visualization, slow enough that serial never becomes a bottleneck.

### Avoiding Buffer Overflow

```cpp
// BAD: no delay, floods serial buffer
void loop() {
  Serial.println(analogRead(A0));
}

// GOOD: controlled rate
void loop() {
  Serial.println(analogRead(A0));
  delay(20);  // 50 Hz
}

// BEST: non-blocking rate control
unsigned long lastPrint = 0;
void loop() {
  if (millis() - lastPrint >= 20) {
    lastPrint = millis();
    Serial.print("sensor:"); Serial.println(analogRead(A0));
  }
  // ... other work continues
}
```

---

## Quick Reference Card

### Plotter Formats (all work with Serial.print)

```
name@plotId:value     →  named variable on specific plot
name:value            →  named variable on plot 0 (Arduino compatible)
name=value            →  named variable on plot 0 + dashboard
25.3,60.0,1013.2      →  auto-named ch0/ch1/ch2 on plot 0
```

### Dashboard Format

```text
key=value             →  any non-whitespace value (numbers, strings, booleans)
```

### ANSI Colors

```text
\033[1;31mERROR\033[0m   →  bold red "ERROR" then reset
```

### Complete Arduino Example

```cpp
#define ANSI_BOLD  "\033[1m"
#define ANSI_GREEN "\033[32m"
#define ANSI_RESET "\033[0m"

void setup() {
  Serial.begin(115200);
  Serial.println(ANSI_BOLD ANSI_GREEN "floppi FC ready" ANSI_RESET);
}

unsigned long lastTelemetry = 0;
void loop() {
  if (millis() - lastTelemetry >= 20) {  // 50 Hz
    lastTelemetry = millis();

    // Plotter data (multi-graph)
    Serial.print("ax@0:"); Serial.print(accelX, 2);
    Serial.print(" ay@0:"); Serial.print(accelY, 2);
    Serial.print(" az@0:"); Serial.print(accelZ, 2);
    Serial.print(" gx@1:"); Serial.print(gyroX, 2);
    Serial.print(" gy@1:"); Serial.print(gyroY, 2);
    Serial.print(" gz@1:"); Serial.print(gyroZ, 2);

    // Dashboard data (= separator)
    Serial.print(" loopHz="); Serial.print(loopRate);
    Serial.print(" armed="); Serial.print(armed ? "true" : "false");
    Serial.println();
  }
}
```

---

## Sources

- [Arduino Serial Plotter Protocol (Official)](https://github.com/arduino/Arduino/blob/master/build/shared/ArduinoSerialPlotterProtocol.md)
- [Arduino IDE 2.x Serial Plotter Tutorial](https://docs.arduino.cc/software/ide-v2/tutorials/ide-v2-serial-plotter/)
- [Arduino Serial.print() Reference](https://www.arduino.cc/reference/en/language/functions/communication/serial/print/)
- [Teleplot Protocol](https://github.com/nesnes/teleplot)
- [serialport-rs (Rust serial library)](https://docs.rs/serialport/latest/serialport/)
- [Teensy USB Serial](https://www.pjrc.com/teensy/td_serial.html)

---

*This document is the definitive reference for fc_tool's serial protocol. The protocol is data-only and stable — changes should be backwards compatible.*
