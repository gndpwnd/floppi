# OLED Display Options for Flight Controller

> Research date: 2026-02-06

## Overview

Small OLED displays are useful for showing calibration values, WiFi credentials (SSID, IP, MAC), and flight status without requiring a serial connection.

---

## DSD Tech 0.91" OLED (Primary Recommendation)

### Specifications
- **Size**: 0.91 inches (128x32 pixels)
- **Driver chip**: SSD1306
- **Interface**: I2C (4 pins: VCC, GND, SDA, SCL)
- **Voltage**: 3.3V - 5V compatible
- **I2C Address**: 0x3C (typical)
- **Price**: ~$8-12 on Amazon

### Reference Implementation

From `~/Desktop/SwarmLoc/GPS_module/GPS_OLED_091/GPS_OLED_091.ino`:

```cpp
#include <Wire.h>
#include <U8g2lib.h>

// OLED Display setup - software I2C
U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA);

void setup() {
  u8g2.begin();
}

void updateDisplay() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  u8g2.drawStr(0, 10, "Line 1");
  u8g2.drawStr(0, 20, "Line 2");
  u8g2.drawStr(0, 30, "Line 3");

  u8g2.sendBuffer();
}
```

### Wiring (ESP32 Example)
```
DSD Tech OLED  →  ESP32
--------------------------
GND            →  GND
VCC            →  3.3V
SDA            →  GPIO 21 (or GPIO 23 on Feather)
SCL            →  GPIO 22
```

### Wiring (Teensy 4.0)
```
DSD Tech OLED  →  Teensy 4.0
-----------------------------
GND            →  GND
VCC            →  3.3V
SDA            →  Pin 18 (SDA)
SCL            →  Pin 19 (SCL)
```

### Library: U8g2

**Why U8g2 over Adafruit_SSD1306:**
- More fonts built-in
- Better memory efficiency
- Works with many display types
- Cleaner API

**PlatformIO lib_deps:**
```ini
lib_deps =
    olikraus/U8g2@^2.35.0
```

### Key Patterns from Reference Code

1. **Loading bar for startup:**
```cpp
void showLoadingBar(int percentage) {
  int barWidth = 100;
  int barHeight = 6;
  int filledWidth = map(percentage, 0, 100, 0, barWidth);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "Initializing...");

  u8g2.drawFrame(0, 22, barWidth, barHeight);  // Outline
  u8g2.drawBox(0, 22, filledWidth, barHeight);  // Filled portion
  u8g2.sendBuffer();
}
```

2. **Dynamic text with values:**
```cpp
u8g2.setCursor(85, 20);
u8g2.print(percentage);
u8g2.print("%");
```

3. **Display refresh pattern:**
```cpp
// Only update when data changes (not every loop)
if (dataUpdated) {
  dataUpdated = false;
  updateDisplay();
}
```

---

## Alternative Displays

### 0.96" OLED (128x64) - SSD1306
- **Pros**: More vertical space (4 lines vs 3), same driver
- **Cons**: Slightly larger, slightly more power
- **Price**: ~$6-10 on Amazon
- **Recommendation**: Good for more detailed status display

### 1.3" OLED (128x64) - SH1106
- **Pros**: Larger, easier to read
- **Cons**: Different driver (SH1106), more board space
- **Price**: ~$8-12 on Amazon
- **Note**: U8g2 supports SH1106 with different constructor

### Comparison Table

| Display | Resolution | Driver | I2C Addr | Best For |
|---------|------------|--------|----------|----------|
| 0.91" | 128x32 | SSD1306 | 0x3C | Minimal status, compact |
| 0.96" | 128x64 | SSD1306 | 0x3C | More info, still compact |
| 1.3" | 128x64 | SH1106 | 0x3C | Readability at distance |

---

## I2C vs SPI Considerations

### I2C (Recommended for FC)
- **Pros**: Only 2 data pins (SDA, SCL), simpler wiring
- **Cons**: Slower (~400kHz typical)
- **Use when**: Pin count matters, display updates are infrequent

### SPI
- **Pros**: Faster refresh (~10MHz), smoother animations
- **Cons**: More pins (CS, DC, RST, MOSI, SCK)
- **Use when**: Fast animations needed, pins available

**For flight controller calibration display:** I2C is sufficient. Updates are infrequent (1-10Hz).

---

## Power Consumption

| Display | Active | Sleep |
|---------|--------|-------|
| 0.91" SSD1306 | ~20mA | <10uA |
| 0.96" SSD1306 | ~25mA | <10uA |
| 1.3" SH1106 | ~30mA | <10uA |

All are low enough for battery-powered operation.

---

## Flight Controller Integration

### Proposed Use Cases

1. **Calibration mode**: Show current step, progress, values
2. **Startup**: Show firmware version, initialization status
3. **WiFi mode (ESP32)**: Show SSID, IP address, MAC
4. **Live mode**: Show armed status, battery (if sensor added)

### Example: Calibration Status Display
```cpp
void showCalibrationStatus(const char* step, int progress) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  u8g2.drawStr(0, 10, "CALIBRATION");
  u8g2.drawStr(0, 20, step);

  // Progress bar
  u8g2.drawFrame(0, 24, 100, 6);
  u8g2.drawBox(0, 24, progress, 6);

  u8g2.sendBuffer();
}
```

### Example: WiFi Info Display (ESP32)
```cpp
void showWiFiInfo(const char* ssid, const char* ip, const char* mac) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tf);  // Smaller font for more info

  u8g2.drawStr(0, 7, "SSID:");
  u8g2.drawStr(30, 7, ssid);

  u8g2.drawStr(0, 16, "IP:");
  u8g2.drawStr(30, 16, ip);

  u8g2.drawStr(0, 25, "MAC:");
  u8g2.drawStr(30, 25, mac);

  u8g2.sendBuffer();
}
```

---

## Recommended Purchase

**Primary choice**: DSD Tech 0.91" I2C OLED
- Available on Amazon
- Well-documented
- Working reference code exists
- Compact form factor

**Amazon search**: "DSD Tech 0.91 OLED I2C" or "SSD1306 128x32 OLED"

---

## Implementation Priority

1. **Phase 1**: Basic display support in calibration mode (show step/progress)
2. **Phase 2**: WiFi info display for ESP32 variant
3. **Phase 3**: Live status display (armed, battery, etc.)

Display support is optional and should be behind a build flag:
```cpp
#ifdef USE_OLED_DISPLAY
  #include "display.h"
  updateDisplay();
#endif
```
