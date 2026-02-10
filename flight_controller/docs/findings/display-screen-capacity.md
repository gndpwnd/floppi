# Display Screen Capacity Analysis

> Date: 2026-02-09

## Supported Displays

| Display | Controller | Resolution | Size | Config Flag |
|---------|-----------|-----------|------|-------------|
| DSD TECH 0.91" | SSD1306 | 128x32 | Small | `DISPLAY_SSD1306_128X32` |
| Generic 0.96" | SSD1306 | 128x64 | Medium | `DISPLAY_SSD1306_128X64` |
| HiLetGo 1.3" | SH1106 | 128x64 | Large | `DISPLAY_SH1106_128X64` |

All use I2C interface. Software I2C on dedicated pins (separate from IMU bus).

## Text Capacity

Using `u8g2_font_6x10_tf` (6px wide, 10px tall, fixed-width):

| Display | Chars/Line | Lines | Total Chars | Usable Area |
|---------|-----------|-------|-------------|-------------|
| 128x32 | 21 | 3 | 63 | Very limited |
| 128x64 | 21 | 6 | 126 | Comfortable |

## What Needs to Be Displayed

| Info Category | Content | Chars Needed | Priority |
|---------------|---------|-------------|----------|
| Status | "ARMED" / "DISARMED" / "READY" | 8-10 | Critical |
| Attitude | R:+12.3 P:-5.2 Y:+180.0 | 26 (2 lines) | High |
| Motors | M:45 52 48 51 | 15 | High (armed) |
| Loop timing | Loop: 500us | 12 | Medium |
| WiFi IP | IP:192.168.1.100 | 18 | Medium |
| WiFi SSID | MyNetwork | up to 21 | Low |
| MAC address | MAC:AA:BB:CC:DD:EE:FF | 21 | Low |
| RSSI | RSSI: -65 dBm | 15 | Low |
| Calibration | Mode + instruction | 2 lines | Context |

## 128x64 Displays (6 lines) — No Rotation Needed

All info fits simultaneously:

**Idle screen:**
```
FLOPPI READY
Loop: 500us
R:+12.3 P:-5.2
Y:+180.0
IP:192.168.1.100
MAC:AA:BB:CC:DD:EE:FF
```

**Armed screen:**
```
** ARMED **
R:+12.3 P:-5.2
M:45 52 48 51
Y:+180.0
IP:192.168.1.100
RSSI: -65 dBm
```

**Network screen:**
```
IP:192.168.1.100
MyNetwork
MAC:AA:BB:CC:DD:EE:FF
RSSI: -65 dBm
DISARMED
```

## 128x32 Displays (3 lines) — Screen Rotation Required

Only 3 lines available. Cannot show all info at once. Solution: rotate between screens every 2 seconds.

**Screen rotation order (idle/disarmed):**

| Screen | Line 1 | Line 2 | Line 3 |
|--------|--------|--------|--------|
| 0 - Status | FLOPPI READY | Loop: 500us | R:+12.3 P:-5.2 |
| 1 - Attitude | R:+12.3 P:-5.2 | Y:+180.0 | Loop: 500us |
| 2 - Network* | IP:192.168.1.100 | MyNetwork | RSSI: -65 dBm |

*Network screen only shown when WiFi connected.

**Screen rotation order (armed):**

| Screen | Line 1 | Line 2 | Line 3 |
|--------|--------|--------|--------|
| 0 - Flight | ** ARMED ** | R:+12.3 P:-5.2 | M:45 52 48 51 |
| 1 - Detail | ** ARMED ** | Y:+180.0 | Loop: 500us |

**No rotation for:**
- Calibration screen (always shows calibration mode info)
- Startup screen (always shows init message)

## Implementation

- `DISPLAY_LINES` macro determines behavior at compile time
- When `DISPLAY_LINES >= 6`: show everything, no rotation
- When `DISPLAY_LINES < 6`: rotate screens every 2 seconds using `millis()` timer
- Screen counter resets on state change (idle↔armed↔calibrating)
- Rotation is purely visual — no flight loop impact (runs on Core 1 or at 10Hz on Teensy)
