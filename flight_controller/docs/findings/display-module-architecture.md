# Display Module Architecture

> Research date: 2026-02-07

## Overview

This document describes the architecture for a display abstraction layer in the flight controller firmware. The goal is to support multiple OLED display types across Teensy and ESP32 platforms, with a clean separation between "what to display" and "how to display it."

**Key insight**: The flight control core should not know or care about display hardware. It outputs data to a common interface. The display module handles rendering for whatever hardware is connected.

---

## 1. Design Philosophy

### The Problem

Different boards and use cases need different display behavior:

| Platform | Calibration Mode | Live Mode |
|----------|-----------------|-----------|
| **Teensy** | Show calibration progress + values on OLED | Status indicators (future) |
| **ESP32** | Show calibration progress + values on OLED | Show MAC, SSID, IP + status on OLED |

But the flight control code (Core 0 on ESP32, main loop on Teensy) shouldn't contain display logic. It should just produce data.

### The Solution: Producer-Consumer Pattern

```
┌─────────────────────┐     ┌──────────────────┐     ┌─────────────────────┐
│  Flight Control     │     │  Display Data     │     │  Display Module     │
│  (Core 0 / main)    │────▶│  (shared struct)  │────▶│  (Core 1 / timed)   │
│                     │     │                   │     │                     │
│  - IMU readings     │     │  - attitude       │     │  - U8g2 rendering   │
│  - PID loop         │     │  - motor values   │     │  - Layout logic     │
│  - Motor output     │     │  - cal progress   │     │  - Screen selection │
│  - Calibration      │     │  - system state   │     │  - I2C communication│
└─────────────────────┘     └──────────────────┘     └─────────────────────┘
```

The flight control code writes to a shared data structure. The display module reads from it and renders. On ESP32, these run on different cores. On Teensy, the display update is rate-limited in the main loop.

---

## 2. Display Hardware Support

### Target Displays

| Size | Controller | Resolution | I2C Address | Use Case |
|------|-----------|------------|-------------|----------|
| 0.91" | SSD1306 | 128x32 | 0x3C | Compact, 3 lines (DSD TECH) |
| 0.96" | SSD1306 | 128x64 | 0x3C | Standard, 6 lines |
| 1.3" | SH1106 | 128x64 | 0x3C | Larger, same resolution as 0.96" |

All use I2C. No address conflict with MPU6050 IMU (0x68).

### Library Choice: U8g2

**U8g2** is the recommended library because:
- Single library supports all three display controllers (SSD1306, SH1106)
- Drawing API is identical across all display types — only the constructor changes
- Already used in the reference code (GPS_OLED_091.ino)
- Good memory efficiency (~512B RAM for 128x32, ~1KB for 128x64)
- Works with PlatformIO on both Teensy and ESP32
- Supports both hardware and software I2C

```ini
# platformio.ini
lib_deps = olikraus/U8g2@^2.35.0
```

### Compile-Time Display Selection

The display type is selected via build flags. U8g2 makes this clean because only the constructor changes:

```cpp
#if defined(DISPLAY_SSD1306_128X32)
    U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
    #define DISPLAY_WIDTH  128
    #define DISPLAY_HEIGHT 32
    #define DISPLAY_LINES  3

#elif defined(DISPLAY_SSD1306_128X64)
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
    #define DISPLAY_WIDTH  128
    #define DISPLAY_HEIGHT 64
    #define DISPLAY_LINES  6

#elif defined(DISPLAY_SH1106_128X64)
    U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
    #define DISPLAY_WIDTH  128
    #define DISPLAY_HEIGHT 64
    #define DISPLAY_LINES  6
#endif
```

Build flag in platformio.ini:
```ini
build_flags = -D USE_OLED_DISPLAY -D DISPLAY_SSD1306_128X32
```

---

## 3. I2C Bus Strategy

### The Problem: Bus Contention

The IMU (MPU6050) and OLED both use I2C. On ESP32 dual-core, if Core 0 reads IMU while Core 1 writes to OLED on the **same** I2C bus, data corruption occurs. The Wire library is NOT thread-safe.

### Solution Options

| Approach | Pros | Cons | Recommended For |
|----------|------|------|-----------------|
| **Software I2C for OLED** | Zero contention, simple | Slightly slower (fine at 10Hz) | Both platforms |
| **Separate hardware I2C (Wire1)** | Fast, zero contention | Uses extra pins | ESP32 (has 2 I2C peripherals) |
| **Shared bus + mutex** | Uses fewer pins | Flight loop blocks during display update (~2ms) | Not recommended |

### Recommended: Software I2C on Dedicated Pins

Use hardware I2C (Wire) exclusively for the IMU. Use software I2C on separate GPIO pins for the OLED. This completely eliminates bus contention.

**Teensy pin assignment:**
```cpp
// Hardware I2C for IMU
#define IMU_SDA_PIN 18
#define IMU_SCL_PIN 19

// Software I2C for OLED (pick available pins)
#define OLED_SW_SDA 16
#define OLED_SW_SCL 17
```

**ESP32 pin assignment:**
```cpp
// Hardware I2C for IMU
#define IMU_SDA_PIN 21
#define IMU_SCL_PIN 22

// Software I2C for OLED
#define OLED_SW_SDA 15
#define OLED_SW_SCL 4
```

**Constructor (same for both platforms):**
```cpp
U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(
    U8G2_R0,
    /* clock= */ OLED_SW_SCL,
    /* data= */  OLED_SW_SDA,
    /* reset= */ U8X8_PIN_NONE
);
```

---

## 4. Shared Data Structure

The flight control code and display module communicate through a shared struct. This is the "contract" between them.

```cpp
// display_data.h
#pragma once
#include <stdint.h>

typedef struct {
    // System state
    bool armed;
    bool calibration_in_progress;
    uint8_t calibration_mode;       // CALIB_NONE, CALIB_ACCEL_GYRO, etc.
    uint8_t calibration_step;       // Current step number
    uint8_t calibration_total_steps;
    int calibration_samples_done;
    int calibration_samples_total;

    // Attitude (from Madgwick filter)
    float roll;
    float pitch;
    float yaw;

    // IMU raw (for calibration display)
    float accX, accY, accZ;
    float gyroX, gyroY, gyroZ;

    // Calibration results
    float accErrorX, accErrorY, accErrorZ;
    float gyroErrorX, gyroErrorY, gyroErrorZ;

    // Motor outputs (scaled 0-1)
    float m1, m2, m3, m4;

    // Loop performance
    uint32_t loopRate_hz;

    // ESP32 network info (populated by Core 1 wifi task)
    char mac_address[18];    // "AA:BB:CC:DD:EE:FF"
    char ssid[33];           // WiFi SSID
    char ip_address[16];     // "192.168.4.1"
    bool wifi_connected;
    int8_t wifi_rssi;

    // Timestamp
    uint32_t timestamp_us;
} DisplayData_t;
```

---

## 5. Platform-Specific Integration

### 5a. Teensy (Single Core)

On Teensy, the display update runs in the main loop, rate-limited to avoid impacting the flight control timing.

```
setup()
├── setupIMU()          // Wire.begin() for IMU
├── setupDisplay()      // Software I2C for OLED
├── radioSetup()
├── setupMotors()
└── armMotors()

loop() @ 1000Hz
├── Flight control (every iteration)
│   ├── getIMUdata()
│   ├── Madgwick filter
│   ├── PID loop
│   └── commandMotors()
│
├── Update display data struct (every iteration, cheap)
│   └── Copy attitude, motors, cal status to DisplayData_t
│
└── Display render (every 100ms = 10Hz)
    └── if (current_time - display_counter > 100000)
        └── renderDisplay(&displayData)
```

**Timing budget**: At 1kHz loop, each iteration has 1000µs. Flight control uses ~500µs. A full 128x32 OLED buffer transfer via software I2C takes ~2-4ms. Since display only updates at 10Hz, the budget impact is 2-4ms every 100ms = 2-4% CPU overhead. Acceptable for calibration mode. For live mode, display can be reduced to 2-5Hz.

### 5b. ESP32 (Dual Core)

On ESP32, the display runs on Core 1 via a FreeRTOS task, completely decoupled from flight control on Core 0.

```
Core 0: Flight Control Task (priority 3)
├── getIMUdata() @ 1kHz
├── Madgwick filter
├── PID loop
├── commandMotors()
└── xQueueOverwrite(displayQueue, &data)  // Non-blocking push

Core 1: Display + WiFi Task (priority 1)
├── WiFi STA initialization (connect to existing network)
└── Loop:
    ├── xQueueReceive(displayQueue, &data)  // Block until new data
    ├── populateNetworkData(&data)          // Add WiFi status
    ├── renderDisplay(&data)                // OLED update
    └── handleWiFi()                        // Reconnection management
```

**Queue-based data transfer** (recommended over mutex):
```cpp
QueueHandle_t displayQueue;

// In setup:
displayQueue = xQueueCreate(1, sizeof(DisplayData_t));

// Core 0 writes (non-blocking, always overwrites with latest):
xQueueOverwrite(displayQueue, &data);

// Core 1 reads (blocking, waits for data):
xQueueReceive(displayQueue, &data, pdMS_TO_TICKS(100));
```

Why queue over mutex:
- `xQueueOverwrite` on a depth-1 queue is atomic — no priority inversion risk
- Core 0 never blocks waiting for the display
- Display always gets the latest data (no stale queue buildup)
- Cleaner separation of concerns

---

## 6. Display Screens

### 6a. Calibration Mode Screens (Both Platforms)

The calibration mode display is the same across Teensy and ESP32. The flight control core runs calibration on a single core and outputs progress to the display data struct.

**Screen: Calibration Progress**
```
┌────────────────────────┐
│ CALIBRATING            │   Line 1: Mode name
│ Step 2/6: Nose Up      │   Line 2: Current step (128x32: omit on small display)
│ ████████░░░░░░░ 53%    │   Line 3: Progress bar
└────────────────────────┘
```

**Screen: Calibration Complete**
```
┌────────────────────────┐
│ CALIBRATION DONE       │
│ AccErr: 0.02 -0.01 0.98│
│ GyrErr: 1.2  -0.8  0.3 │
└────────────────────────┘
```

### 6b. ESP32 Live Mode Screens

When in live mode on ESP32, Core 1 populates the network info fields and the display shows them.

**Screen: Network Info (Idle/Disarmed)**
```
┌────────────────────────┐
│ IP:192.168.1.42        │   Line 1: IP address (or "WiFi: Starting...")
│ MyNetwork              │   Line 2: Connected SSID
│ MAC:AA:BB:CC:DD:EE:FF  │   Line 3: MAC address
└────────────────────────┘
```

**Screen: Flight Status (Armed)**
```
┌────────────────────────┐
│ ARMED   R:+5.2 P:-3.1  │   Line 1: Status + attitude
│ M1:45% M2:52% M3:48%   │   Line 2: Motor outputs
│ M4:50% Yaw:+12.3       │   Line 3: Motor 4 + yaw
└────────────────────────┘
```

### 6c. Teensy Live Mode

Minimal for now — just armed/disarmed status. Future expansion possible.

**Screen: Ready**
```
┌────────────────────────┐
│ FLOPPI READY           │
│ Loop: 1000Hz           │
│ Telem: Serial          │
└────────────────────────┘
```

### Layout Adaptation for 128x64

On 128x64 displays (6 lines), the same information is shown with more detail and spacing:

```
┌────────────────────────┐
│ FLOPPI FC              │   Line 1: Title
│                        │   Line 2: Spacer
│ IP:  192.168.1.42      │   Line 3: IP
│ Net: MyNetwork         │   Line 4: Connected SSID
│ MAC: AA:BB:CC:DD:EE:FF │   Line 5: MAC
│ RSSI: -45 dBm          │   Line 6: Signal strength
└────────────────────────┘
```

---

## 7. Reference Code Analysis

### GPS_OLED_091.ino (Existing Reference)

The GPS tracker code in `/home/devel/Desktop/SwarmLoc/GPS_module/GPS_OLED_091/` demonstrates the exact patterns we need:

| Pattern | How It's Used | Apply To FC |
|---------|--------------|-------------|
| U8g2 library | `U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C` | Same library, same pattern |
| Software I2C | `SW_I2C(U8G2_R0, SCL, SDA)` | Use for OLED to avoid IMU bus conflict |
| Buffered rendering | `clearBuffer()` → draw → `sendBuffer()` | Same pattern |
| Rate-limited updates | 2-second timer check before update | Use 100ms (10Hz) |
| Conditional display | Different content based on GPS fix status | Different content based on mode |
| Progress bar | `drawFrame()` + `drawBox()` pattern | Use for calibration progress |
| Font | `u8g2_font_6x10_tf` (6x10 monospace) | Same font for status text |

### GravityProbe ESP32 WiFi Code (Existing Reference)

Code in `~/GravityProbe/esp32_*/` demonstrates WiFi patterns:

| Pattern | Source File | Apply To FC |
|---------|------------|-------------|
| WPA2-Enterprise (PEAP) | `esp32_enterprise_wpa3_eap.ino` | For eduroam/university WiFi |
| Credentials in header | `wifi_credentials.h` (gitignored) | Same pattern for FC |
| IP on OLED | `esp32_ewpa2_iic_091.ino` → `displayIPAddress()` | Reuse this pattern |
| Web server (port 80) | `esp32_ent_wpa2_peap_web80.ino` | Expand for status page |
| Reconnection w/ timeout | 30s timeout → `ESP.restart()` | Adapt for FC |
| U8g2 + WiFi together | `esp32_ewpa2_iic_091.ino` | Proves the combo works |

---

## 8. Calibration Mode: Single-Core Simplification

### The User's Question

> "Would it be easier to just make calibration mode all the same across all boards running on one core, and that core outputs information to some place?"

**Answer: Yes.** This is the recommended approach.

### Rationale

Calibration mode does NOT need dual-core processing:
- Motors are not spinning (no real-time control needed)
- The calibration routine is sequential (collect N samples → compute offsets)
- Display updates during calibration are non-critical timing
- The same calibration code should produce identical results on Teensy and ESP32

### Architecture for Calibration Mode

```
Single Core (both Teensy and ESP32):
├── Run calibration routine
│   ├── Collect IMU samples
│   ├── Compute offsets
│   └── Update DisplayData_t with progress
│
├── Render display (rate-limited, in same loop)
│   └── Show progress bar, step info, results
│
└── Output results
    ├── Serial (for fc_tool / copy-paste to config.h)
    ├── OLED (for field calibration without laptop)
    └── Web server (ESP32 only, Core 1 serves cached results)
```

On ESP32 in calibration mode, Core 1 can still run a simple web server that serves the last calibration results. But the calibration itself runs entirely on Core 0 with inline display updates, same as Teensy.

### The "Display Sink" Concept

The flight control / calibration code writes to `DisplayData_t` without knowing what will read it. Different "sinks" consume this data:

| Sink | Platform | Mode | Description |
|------|----------|------|-------------|
| OLED (direct) | Teensy | Calibration | Main loop renders to OLED via SW I2C |
| OLED (Core 1 task) | ESP32 | Live | FreeRTOS task on Core 1 renders to OLED |
| Web server | ESP32 | Both | HTTP endpoint serves DisplayData as JSON |
| Serial telemetry | Both | Both | Existing `printIMUTelemetry()` / `printFullTelemetry()` |

This is the "output to some place" concept. The flight code just fills the struct. The platform decides what to do with it.

---

## 9. Implementation Phases

### Phase 1: Calibration Display (Teensy + ESP32)
- Add U8g2 library dependency
- Create `display.h` / `display.cpp` with compile-time display selection
- Define `DisplayData_t` shared struct
- Add OLED pin definitions to both pin definition headers
- Rate-limited display update in calibration loop
- Screens: startup, calibration progress, calibration results
- Build flag: `-D USE_OLED_DISPLAY -D DISPLAY_SSD1306_128X32`

### Phase 2: ESP32 Network Info Display

- Display MAC, SSID, IP, RSSI on OLED after WiFi STA connection
- Core 1 populates network fields in DisplayData_t via `populateNetworkData()`
- Screen: network info (idle/disarmed)
- Shows connection status while connecting to infrastructure WiFi

### Phase 3: ESP32 Web Status Server
- ESPAsyncWebServer on Core 1
- Serve DisplayData_t as JSON endpoint
- mDNS for `http://floppi.local` discovery
- WebSocket for real-time telemetry streaming to fc_tool

### Phase 4: Live Flight Display
- Armed/disarmed status screen
- Attitude + motor output display
- Screen rotation (cycle through screens with a button or auto-rotate)

---

## 10. File Structure (Proposed)

```
flight_controller/
├── include/
│   ├── display.h           # Display abstraction + compile-time selection
│   ├── display_data.h      # DisplayData_t struct definition
│   └── display_screens.h   # Screen rendering function declarations
├── src/
│   ├── display.cpp         # Display init, render dispatch, rate limiting
│   └── display_screens.cpp # Individual screen layouts (calibration, network, flight)
└── platformio.ini          # Build flags: -D USE_OLED_DISPLAY -D DISPLAY_SSD1306_128X32
```

---

## 11. Key Decisions Summary

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Library | U8g2 | Supports all target displays, already in reference code |
| I2C strategy | Software I2C for OLED | Eliminates bus contention with IMU |
| Data transfer | Queue (ESP32) / Direct (Teensy) | Non-blocking, always latest data |
| Calibration arch | Single core, same on all platforms | Simpler, consistent results |
| Display selection | Compile-time build flags | No runtime overhead, clean code |
| Update rate | 10Hz (100ms interval) | Smooth enough for display, low CPU cost |
| Build gating | `#ifdef USE_OLED_DISPLAY` | Optional, compiles out cleanly |

---

## References

- [U8g2 Library](https://github.com/olikraus/u8g2) — Universal display library
- [U8g2 Constructor Reference](https://github.com/olikraus/u8g2/wiki/u8g2setupcpp)
- [ESP32 FreeRTOS Queues](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos_idf.html)
- GPS_OLED_091.ino — Existing reference implementation
- GravityProbe esp32_ewpa2_iic_091 — Existing WiFi + OLED reference
- [esp32-wifi-connectivity.md](esp32-wifi-connectivity.md) — WiFi research findings
- [esp32-dual-core-research.md](esp32-dual-core-research.md) — Dual-core architecture research
- [oled-display-options.md](oled-display-options.md) — Initial display research
