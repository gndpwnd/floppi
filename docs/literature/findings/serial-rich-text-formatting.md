# Serial Rich Text Formatting — Research Findings

**Date**: 2026-02-10
**Context**: Investigating how to send formatted/colored text over serial connections from embedded firmware, and which serial monitors can render it.

---

## 1. ANSI Escape Codes / VT100

### Background

ANSI escape codes (formally ECMA-48 / ISO 6429) are in-band signaling sequences embedded in text streams to control text formatting, color, and cursor position on video text terminals. They originated with the DEC VT100 terminal (1978) and became the de facto standard via the ANSI X3.64 standard. Nearly all modern terminal emulators implement some subset of these codes.

### Escape Sequence Structure

All ANSI escape sequences begin with the **ESC** character (`0x1B`, octal `\033`, or `\e` in some languages), followed by a command character or a **CSI** (Control Sequence Introducer) sequence:

```
ESC [  <parameter> ; <parameter> m
```

The CSI is `ESC [` (bytes `0x1B 0x5B`). The `m` suffix denotes **SGR** (Select Graphic Rendition), which controls text appearance.

### SGR Parameters — Complete Reference

| Code | Effect | Reset Code |
|------|--------|------------|
| `0` | Reset all attributes | — |
| `1` | **Bold** (increased intensity) | `22` |
| `2` | **Dim** (decreased intensity / faint) | `22` |
| `3` | *Italic* | `23` |
| `4` | Underline | `24` |
| `5` | Slow blink (<150 per minute) | `25` |
| `6` | Rapid blink (>150 per minute) | `25` |
| `7` | Reverse video (swap FG/BG) | `27` |
| `8` | Hidden / conceal | `28` |
| `9` | ~~Strikethrough~~ (crossed-out) | `29` |
| `21` | Double underline | `24` |
| `53` | Overline | `55` |

### Foreground Colors (Standard 8)

| Code | Color |
|------|-------|
| `30` | Black |
| `31` | Red |
| `32` | Green |
| `33` | Yellow |
| `34` | Blue |
| `35` | Magenta |
| `36` | Cyan |
| `37` | White |
| `39` | Default foreground |

### Background Colors (Standard 8)

| Code | Color |
|------|-------|
| `40` | Black |
| `41` | Red |
| `42` | Green |
| `43` | Yellow |
| `44` | Blue |
| `45` | Magenta |
| `46` | Cyan |
| `47` | White |
| `49` | Default background |

### Bright / High-Intensity Colors

| Foreground | Background | Color |
|-----------|-----------|-------|
| `90` | `100` | Bright Black (Gray) |
| `91` | `101` | Bright Red |
| `92` | `102` | Bright Green |
| `93` | `103` | Bright Yellow |
| `94` | `104` | Bright Blue |
| `95` | `105` | Bright Magenta |
| `96` | `106` | Bright Cyan |
| `97` | `107` | Bright White |

### 256-Color Mode

```
ESC [ 38 ; 5 ; <n> m    — Set foreground to color <n> (0-255)
ESC [ 48 ; 5 ; <n> m    — Set background to color <n> (0-255)
```

Color ranges:
- `0–7`: Standard colors (same as 30–37)
- `8–15`: High-intensity colors (same as 90–97)
- `16–231`: 6x6x6 RGB color cube (216 colors). Formula: `16 + 36*r + 6*g + b` where r,g,b in 0-5
- `232–255`: Grayscale ramp (24 shades, dark to light)

### 24-bit True Color (RGB)

```
ESC [ 38 ; 2 ; <r> ; <g> ; <b> m    — Set foreground to RGB
ESC [ 48 ; 2 ; <r> ; <g> ; <b> m    — Set background to RGB
```

Where `r`, `g`, `b` are each 0–255.

### Cursor Control (Useful for live telemetry displays)

| Sequence | Effect |
|----------|--------|
| `ESC [ <n> A` | Cursor up n lines |
| `ESC [ <n> B` | Cursor down n lines |
| `ESC [ <n> C` | Cursor forward n columns |
| `ESC [ <n> D` | Cursor back n columns |
| `ESC [ <r> ; <c> H` | Move cursor to row r, column c |
| `ESC [ 2 J` | Clear entire screen |
| `ESC [ K` | Clear from cursor to end of line |
| `ESC [ 2 K` | Clear entire line |
| `ESC [ s` | Save cursor position |
| `ESC [ u` | Restore cursor position |

### C/C++ Usage Examples (Embedded)

```cpp
// Bold red text
Serial.print("\033[1;31mERROR:\033[0m Sensor failure");

// Green text
Serial.print("\033[32mOK\033[0m");

// Yellow bold warning
Serial.print("\033[1;33mWARNING:\033[0m Low battery");

// Cyan telemetry header
Serial.print("\033[36m=== TELEMETRY ===\033[0m");

// 256-color (orange-ish, color 208)
Serial.print("\033[38;5;208mOrange text\033[0m");

// Macros for embedded use
#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_DIM     "\033[2m"
#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_BLUE    "\033[34m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_BRED    "\033[1;31m"  // Bold red
#define ANSI_BGREEN  "\033[1;32m"  // Bold green

// Usage
Serial.println(ANSI_BRED "CRITICAL" ANSI_RESET " — Motor " ANSI_YELLOW "3" ANSI_RESET " stalled");
```

### What Serial Monitors Support ANSI Escape Codes

| Monitor | Bold | Dim | Italic | Underline | Strikethrough | 8-Color | 256-Color | True Color | Cursor Control | Notes |
|---------|------|-----|--------|-----------|---------------|---------|-----------|------------|----------------|-------|
| **PuTTY** | Yes | Yes | No | Yes | No | Yes | Partial | No | Yes | Full VT100+. Italic not rendered (treated as reverse in some configs). |
| **minicom** | Yes | Yes | No | Yes | No | Yes | No | No | Yes | Classic Unix. Relies on underlying terminal for rendering. `-c on` flag enables color. |
| **PlatformIO Monitor** | Yes | Yes | Yes | Yes | No | Yes | Yes | Yes | Yes | Built on pyserial-monitor; runs in your shell terminal so inherits its ANSI support. Full color support in modern terminals. |
| **Arduino IDE 2.x Serial Monitor** | **No** | **No** | **No** | **No** | **No** | **No** | **No** | **No** | **No** | **Does NOT support ANSI codes.** Renders escape sequences as raw garbage characters. Major limitation. |
| **Arduino IDE 1.x Serial Monitor** | **No** | **No** | **No** | **No** | **No** | **No** | **No** | **No** | **No** | Same as 2.x — no ANSI support. |
| **CoolTerm** | **No** | **No** | **No** | **No** | **No** | **No** | **No** | **No** | Partial | Plain-text focused. Some cursor control but no SGR formatting. |
| **Tera Term** | Yes | Yes | Partial | Yes | No | Yes | Yes | No | Yes | Excellent VT100/VT220/VT382 support. One of the best Windows serial terminals for ANSI. |
| **screen** (Unix) | Yes | Yes | Partial | Yes | No | Yes | Yes | Depends | Yes | Terminal multiplexer; ANSI support depends on outer terminal. `TERM=xterm-256color`. |
| **picocom** | Yes | Yes | Yes | Yes | Varies | Yes | Yes | Yes | Yes | Lightweight Unix serial tool. Passes through to host terminal — full ANSI if terminal supports it. |
| **RealTerm** | **No** | **No** | **No** | **No** | **No** | **No** | **No** | **No** | **No** | Binary/hex focused. No ANSI rendering. |
| **HTerm** | **No** | **No** | **No** | **No** | **No** | **No** | **No** | **No** | **No** | Hex terminal. No ANSI support. |
| **Serial Studio** | Partial | No | No | No | No | Partial | No | No | No | Focused on data visualization, limited text formatting. |
| **Tabby** (terminal) | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Modern electron-based terminal with serial plugin. Full ANSI. |
| **kitty** (with serial) | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | GPU-accelerated terminal. Full ANSI + extensions. Via picocom/minicom. |
| **Windows Terminal** (with serial) | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Microsoft's modern terminal. Full ANSI. Serial via WSL or tools. |
| **VS Code Terminal** (PlatformIO) | Yes | Yes | Yes | Yes | No | Yes | Yes | Yes | Yes | Inherits VS Code terminal's ANSI support. This is the most common setup for PlatformIO users. |

**Key finding**: The Arduino Serial Monitor (both 1.x and 2.x) does NOT support ANSI escape codes. Since many users use PlatformIO (which renders in a real terminal emulator), this is less of a problem for the Floppi project. However, any ANSI-formatted output will appear as garbage in the Arduino Serial Monitor.

### Graceful Degradation Strategy

```cpp
// Option 1: Compile-time toggle
#ifdef USE_ANSI_COLORS
  #define CLR_RED    "\033[31m"
  #define CLR_GREEN  "\033[32m"
  #define CLR_RESET  "\033[0m"
#else
  #define CLR_RED    ""
  #define CLR_GREEN  ""
  #define CLR_RESET  ""
#endif

// Option 2: Runtime detection (send query, check response)
// ESC [ 6 n  — Device Status Report (cursor position query)
// If the terminal responds, it likely supports ANSI.
// Not practical for most embedded scenarios.
```

---

## 2. Markdown-over-Serial

### Does Anyone Do This?

**No established standard or widely-used project exists for rendering Markdown in a serial terminal.** This is a gap in the ecosystem. The fundamental problem is that serial terminals are character-cell displays — they can render bold/italic/color via ANSI codes, but they cannot render:

- Headers with different font sizes (no font scaling in terminals)
- Images
- Tables with borders (though ASCII table drawing characters work)
- Links (clickable — though some modern terminals like iTerm2, kitty, and Windows Terminal support OSC 8 hyperlinks)

### Closest Approaches Found

1. **Rich (Python library by Will McGugan)** — `pip install rich`. Renders Markdown to terminal output with ANSI codes. Can render headings (bold + color), code blocks (syntax highlighted), lists, emphasis (bold/italic), and horizontal rules. This is the closest thing to "Markdown in a terminal" but it's a Python library for the receiving end, not an embedded sender protocol.

2. **Glow (Charm.sh)** — CLI Markdown renderer written in Go. `glow README.md` renders Markdown beautifully in terminal with ANSI codes. Again, a consumer-side tool.

3. **mdcat** — Terminal Markdown renderer. Supports iTerm2 and kitty image protocols for inline images. Terminal-side rendering.

4. **bat** — A `cat` clone with syntax highlighting. Can render Markdown with ANSI formatting.

5. **ESP-IDF Log System** — Espressif's logging framework (`ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE`) uses ANSI color codes to colorize log levels. This is the closest "embedded firmware sending ANSI-colored output" precedent:
   - `ESP_LOGE` (error): Red
   - `ESP_LOGW` (warning): Yellow
   - `ESP_LOGI` (info): Green
   - `ESP_LOGD` (debug): No color
   - `ESP_LOGV` (verbose): No color

6. **Zephyr RTOS shell** — Uses ANSI codes for colored prompts and command output.

### A Practical "Markdown-to-ANSI" Mapping for Embedded

If you wanted to send "Markdown-like" text from firmware and have it render nicely in ANSI terminals, here's a mapping that could be applied:

| Markdown | ANSI Equivalent |
|----------|----------------|
| `# Heading` | `\033[1;37m` (Bold bright white) + `\033[0m` |
| `## Subheading` | `\033[1;36m` (Bold cyan) + `\033[0m` |
| `**bold**` | `\033[1m` + `\033[22m` |
| `*italic*` | `\033[3m` + `\033[23m` |
| `` `code` `` | `\033[7m` (reverse video) + `\033[27m` |
| `---` (horizontal rule) | Print `─` repeated across terminal width |
| `> blockquote` | `\033[2;3m` (dim italic) + `\033[0m` |

**However**, this approach has no real precedent in the embedded world. Nobody is doing this in production firmware. The ESP-IDF approach (simple color-coded log levels) is the practical standard.

---

## 3. Custom Serial Protocols for Rich Output

### Existing Approaches

#### 3a. ESP-IDF Log Coloring (Most Relevant)

The most widely-deployed "rich serial output" in embedded is ESP-IDF's colored logging:

```c
// ESP-IDF internally wraps log output with ANSI codes:
// Error:   \033[0;31m  (red)
// Warning: \033[0;33m  (yellow)
// Info:    \033[0;32m  (green)
// Debug:   (no color)
// Verbose: (no color)
// All reset with \033[0m

// Controlled by menuconfig: Component config → Log output → Color
// CONFIG_LOG_COLORS=y
```

#### 3b. Arduino Libraries for ANSI Output

Several Arduino libraries wrap ANSI escape codes:

- **AnsiStream** — Arduino library providing `<<` style output with ANSI formatting methods.
- **BasicTerm** — Arduino library for VT100 terminal control (cursor movement, colors, clear screen). GitHub: `nottwo/BasicTerm`.
- **SerialColor** — Minimal Arduino library for colored serial output.
- **Terminal** by Rob Tillaart — Arduino library wrapping common VT100/ANSI sequences.

Example with BasicTerm:
```cpp
#include <BasicTerm.h>
BasicTerm term(&Serial);

void setup() {
    Serial.begin(115200);
    term.init();
    term.cls();  // Clear screen
    term.set_color(BT_RED, BT_BLACK);
    term.print("ERROR: ");
    term.set_color(BT_WHITE, BT_BLACK);
    term.println("Sensor timeout");
}
```

#### 3c. CMSIS-DAP / SWO (ARM Cortex-M)

ARM's Serial Wire Output (SWO) protocol supports ITM (Instrumentation Trace Macrocell) stimulus ports. Debug viewers like Ozone, STM32CubeIDE, and Segger RTT Viewer can render colored/formatted output, but this uses a separate debug channel, not UART serial.

#### 3d. Segger RTT (Real Time Transfer)

Segger RTT uses shared memory for debug I/O (no UART needed). The RTT Viewer supports ANSI escape codes for colored output. Commonly used with J-Link debuggers. Supports terminal IDs (channels) for separating different log sources.

```c
// Segger RTT color macros
#define RTT_CTRL_TEXT_RED     "\x1B[31m"
#define RTT_CTRL_TEXT_GREEN   "\x1B[32m"
#define RTT_CTRL_RESET        "\x1B[0m"
SEGGER_RTT_printf(0, RTT_CTRL_TEXT_RED "Error: %s\n" RTT_CTRL_RESET, msg);
```

#### 3e. Tagged Protocols (Non-ANSI)

Some projects use parseable tags rather than ANSI codes, intended for custom host-side software:

- **Telemetry/MSP Protocol** (MultiWii Serial Protocol, used by Betaflight/iNav) — Binary protocol with typed messages. Not text-based. The host (Betaflight Configurator) renders the UI.
- **MAVLink** — Binary protocol for drone telemetry (PX4, ArduPilot). Host software (QGroundControl, Mission Planner) renders formatted displays.
- **Custom JSON logging** — Some embedded projects send `{"level":"error","msg":"...","ts":1234}` and let the host parse/colorize.
- **Firmata** — Protocol for communicating with microcontrollers from host software. Binary, not text-formatting focused.

**Key insight**: The embedded world overwhelmingly uses either (a) raw ANSI codes in text streams, or (b) binary protocols with host-side rendering. There is no widely-adopted "BBCode for serial" or "HTML tags over serial" standard. ANSI escape codes ARE the standard for in-band text formatting.

---

## 4. Terminal Emulator Feature Support Survey

### Comprehensive Feature Matrix

| Feature | xterm | VTE (GNOME Terminal, Tilix) | kitty | Alacritty | Windows Terminal | iTerm2 | PuTTY | Tera Term | macOS Terminal.app | Konsole |
|---------|-------|-----------------------------|-------|-----------|-----------------|--------|-------|-----------|-------------------|---------|
| Bold (`1`) | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Dim (`2`) | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Italic (`3`) | Yes | Yes | Yes | Yes | Yes | Yes | No* | Partial | Yes | Yes |
| Underline (`4`) | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Blink (`5`) | Yes | No** | No | No | No | Yes | Yes | Yes | No | Yes |
| Reverse (`7`) | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Hidden (`8`) | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Strikethrough (`9`) | Yes | Yes | Yes | Yes | Yes | Yes | No | No | Yes | Yes |
| Double underline (`21`) | Partial | Yes | Yes | No | Yes | Yes | No | No | No | Yes |
| Overline (`53`) | No | Yes | Yes | No | Yes | No | No | No | No | Yes |
| 8 colors (30-37) | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Bright colors (90-97) | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| 256 colors | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Yes | Yes | Yes |
| True color (24-bit) | Yes | Yes | Yes | Yes | Yes | Yes | No | No | Yes*** | Yes |
| Curly underline | No | Yes | Yes | No | Yes | Yes | No | No | No | Yes |
| Colored underline | No | Yes | Yes | No | Yes | Yes | No | No | No | Yes |
| Hyperlinks (OSC 8) | No | Yes | Yes | No | Yes | Yes | No | No | No | Yes |

*PuTTY maps italic to reverse video by default. Can be changed in settings but font support is limited.
**VTE-based terminals sometimes interpret blink as bold instead.
***macOS Terminal.app gained true color support in macOS Monterey (12.0+).

### Practical "Safe Subset" for Maximum Compatibility

Based on the matrix above, the following features are supported by essentially every terminal emulator that supports ANSI at all:

- **Bold** (`\033[1m`) — Universal
- **Dim** (`\033[2m`) — Nearly universal
- **Underline** (`\033[4m`) — Universal
- **Reverse video** (`\033[7m`) — Universal
- **8 standard colors** (30-37, 40-47) — Universal
- **16 colors with bright** (90-97, 100-107) — Universal
- **Reset** (`\033[0m`) — Universal

Features to use cautiously:
- **Italic** (`\033[3m`) — Not supported in PuTTY (major Windows serial terminal)
- **Strikethrough** (`\033[9m`) — Not supported in PuTTY or Tera Term
- **256-color** — Works almost everywhere except some older PuTTY versions
- **True color** — Not in PuTTY, Tera Term, or some older terminals

Features to avoid:
- **Blink** — Disabled in most modern terminals (considered annoying)
- **Overline, curly underline** — Limited support
- **Double underline** — Inconsistent

### The PlatformIO Factor

Since Floppi targets PlatformIO users, the serial monitor runs inside VS Code's integrated terminal (or the user's system terminal). VS Code's terminal supports: bold, dim, italic, underline, strikethrough, 8-color, 256-color, true color, and cursor control. This means PlatformIO users get full ANSI support.

---

## 5. Performance Considerations

### Byte Overhead Analysis

ANSI escape sequences add bytes to the serial stream. Here's the overhead for common sequences:

| Sequence | Bytes | Example |
|----------|-------|---------|
| Reset `\033[0m` | 4 | Always needed after colored text |
| Single attribute `\033[1m` | 4 | Bold |
| Single FG color `\033[31m` | 5 | Red foreground |
| Combined `\033[1;31m` | 7 | Bold red |
| 256-color `\033[38;5;208m` | 11 | Foreground color 208 |
| True color `\033[38;2;255;128;0m` | 19 | RGB foreground |
| Clear line `\033[2K` | 4 | |
| Cursor home `\033[H` | 3 | |
| Cursor position `\033[12;40H` | 9 | Row 12, col 40 |

### Bandwidth Budget at 115200 Baud

```
115200 baud = 115200 bits/sec
With 8N1 framing: 10 bits per byte (1 start + 8 data + 1 stop)
Effective throughput: 11,520 bytes/sec = 11.25 KB/sec
```

#### Scenario 1: Color-Coded Log Lines

A typical colorized log line:
```
\033[1;33mWARN\033[0m Roll: 12.34° Pitch: -5.67° Yaw: 89.12° Alt: 2.3m
```

- ANSI overhead: 7 (color) + 4 (reset) = **11 bytes**
- Payload: ~55 bytes
- Total: ~66 bytes
- Overhead percentage: **16.7%**
- Lines per second at 115200: ~174 lines/sec

#### Scenario 2: Multi-Color Telemetry Line

```
\033[36mRoll:\033[1;32m 12.34\033[0m \033[36mPitch:\033[1;31m-5.67\033[0m \033[36mYaw:\033[33m 89.1\033[0m
```

- ANSI overhead: ~42 bytes (6 color codes + 3 resets)
- Payload: ~35 bytes
- Total: ~77 bytes
- Overhead percentage: **54.5%**
- Lines per second at 115200: ~149 lines/sec

#### Scenario 3: Live Dashboard with Cursor Control

A refreshing dashboard (clear + reposition + colorized values):
```
\033[H\033[2J                          (clear screen, cursor home = 7 bytes)
\033[1;36m=== FLOPPI TELEMETRY ===\033[0m\n   (header)
\033[3;1H\033[36mRoll: \033[1;32m 12.34°\033[0m     (per-line updates)
```

- Full refresh: ~300-500 bytes for a 10-line dashboard
- At 115200: ~23-38 refreshes/sec (plenty for 10-20 Hz telemetry)
- Optimization: Only update changed values using cursor positioning (reduces to ~50-100 bytes per update)

#### Scenario 4: Minimal — Log Level Color Only

```
\033[31mE\033[0m Sensor timeout  (error in red)
\033[33mW\033[0m Battery 3.2V    (warning in yellow)
\033[32mI\033[0m Motors armed     (info in green)
```

- ANSI overhead: 9 bytes per line (color + reset)
- This is negligible. At 115200 baud, you could send hundreds of these per second.

### Performance Verdict

**ANSI escape codes are completely practical at 115200 baud for flight controller telemetry.**

- At 50 Hz telemetry update rate: You have 230 bytes per update. A single colorized telemetry line fits easily.
- At 10 Hz (typical display refresh): You have 1,152 bytes per update. A full colorized dashboard fits.
- The serial port is already a bottleneck for high-rate telemetry — ANSI codes add 10-50% overhead depending on how many colors are used, but this is well within budget for human-readable output.
- **Important**: ANSI overhead is zero for binary telemetry protocols. If you need maximum throughput, send binary data and colorize on the host side. For human-readable debug output, the ANSI overhead is negligible.

### Comparison: ANSI vs. Raw Text vs. Binary

| Approach | Bytes per telemetry update | Updates/sec at 115200 | Human-readable? | Color support? |
|----------|---------------------------|----------------------|-----------------|----------------|
| Raw ASCII (no color) | ~60 bytes | ~192 | Yes | No |
| ANSI colored (minimal) | ~70 bytes | ~164 | Yes | Yes |
| ANSI colored (heavy) | ~100 bytes | ~115 | Yes | Yes |
| ANSI dashboard (cursor) | ~150 bytes (incremental) | ~76 | Yes | Yes |
| Binary (MSP-style) | ~20-30 bytes | ~384-576 | No | N/A |

---

## 6. Recommendations for Floppi

### Recommended Approach

1. **Use ANSI escape codes** — They are the universal standard. No custom protocol needed.
2. **Stick to the safe subset**: Bold (`1`), Dim (`2`), Underline (`4`), 8 standard colors (30-37), bright colors (90-97), and Reset (`0`).
3. **Compile-time toggle** via `#ifdef USE_ANSI_COLORS` in `config.h`. Default ON for ESP32 builds. Users with Arduino Serial Monitor can disable it.
4. **Define color macros** in a header file for consistent usage across the codebase.
5. **Use log-level coloring** as the primary pattern (matches ESP-IDF convention, minimal overhead).
6. **Avoid italic and strikethrough** — poor PuTTY/Tera Term support.
7. **Avoid 256-color and true color** — unnecessary for embedded debug output and not universally supported.
8. **Consider cursor control** for a live telemetry dashboard mode (optional, higher complexity).

### Suggested Color Scheme

```cpp
// Severity colors (matches ESP-IDF and common conventions)
#define LOG_ERROR   "\033[1;31m"   // Bold red
#define LOG_WARN    "\033[33m"     // Yellow
#define LOG_INFO    "\033[32m"     // Green
#define LOG_DEBUG   "\033[36m"     // Cyan
#define LOG_VERBOSE "\033[2m"      // Dim

// Semantic colors for telemetry
#define CLR_LABEL   "\033[36m"     // Cyan for parameter names
#define CLR_VALUE   "\033[1;37m"   // Bold white for values
#define CLR_OK      "\033[32m"     // Green for nominal
#define CLR_ALERT   "\033[1;33m"   // Bold yellow for attention
#define CLR_CRIT    "\033[1;31m"   // Bold red for critical
#define CLR_HEADER  "\033[1;36m"   // Bold cyan for headers
#define CLR_RESET   "\033[0m"      // Reset all
```

### Byte Budget Summary

At 115200 baud with 50 Hz telemetry:
- **Available**: 230 bytes per cycle
- **Typical colored line**: 70-100 bytes
- **Verdict**: Fits comfortably. Even at 100 Hz, a single colored status line (70 bytes) uses only 60% of available bandwidth.

---

## References

- ECMA-48 Standard: https://www.ecma-international.org/publications-and-standards/standards/ecma-48/
- Wikipedia — ANSI escape code: https://en.wikipedia.org/wiki/ANSI_escape_code
- XTerm Control Sequences: https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
- ANSI Escape Sequences Gist (fnky): https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797
- ESP-IDF Logging: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html
- PlatformIO Device Monitor: https://docs.platformio.org/en/latest/core/userguide/device/cmd_monitor.html
- Rich (Python): https://github.com/Textualize/rich
- BasicTerm (Arduino): https://github.com/nottwo/BasicTerm
- Segger RTT: https://wiki.segger.com/RTT
- Terminal color support survey: https://github.com/termstandard/colors
