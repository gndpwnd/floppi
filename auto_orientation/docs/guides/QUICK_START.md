# Quick Start Guide

This guide consolidates all quick-start documentation for the Auto Orientation system. Choose your path below.

---

## Getting Started: 5-Minute Setup

Get your Auto Orientation system up and running in just 5 minutes.

### Prerequisites Checklist

Before you begin, make sure you have:

- **Hardware**
  - [ ] Arduino Mega 2560
  - [ ] BNO085 9-axis IMU sensor
  - [ ] Ublox NEO-M9N GPS receiver with antenna
  - [ ] USB cables (USB-B for Arduino and GPS)
  - [ ] Power supply (5V, 1A minimum)

- **Software**
  - [ ] Python 3.7+ installed
  - [ ] PlatformIO CLI installed (`pip install platformio`)
  - [ ] pyserial package (`pip install pyserial`)
  - [ ] USB drivers for Arduino (usually automatic on Mac/Linux)

- **Wiring Complete**
  - [ ] BNO085 connected to Arduino Serial1 (pins 18/19)
  - [ ] BNO085 P1 pin set HIGH to 5V
  - [ ] NEO-M9N GPS connected via USB
  - [ ] All ground connections verified

- **Time & Environment**
  - [ ] 10-15 minutes total (including first calibration)
  - [ ] Quiet space for calibration (indoors is fine for initial test)
  - [ ] Outdoor location with clear sky view for GPS verification

### 3-Step Installation

#### Step 1: Hardware Setup (2 minutes)

If you haven't already wired your hardware:

1. **Mount the Arduino Mega** on your workspace or enclosure
2. **Connect BNO085 to Serial1**:
   - TX (BNO085) → RX1 (Pin 19, Arduino)
   - RX (BNO085) → TX1 (Pin 18, Arduino)
   - VCC → 5V rail
   - GND → GND rail
   - P1 → 5V (via resistor or GPIO pin set HIGH)

3. **Connect GPS via USB**:
   - USB-B cable from NEO-M9N to Arduino USB port
   - Antenna attached to NEO-M9N connector

4. **Power the system**:
   - Connect 5V power supply or USB to Arduino
   - Check that power LED lights up

**Need help?** See [Hardware Setup Guide](HARDWARE_SETUP.md) for detailed diagrams.

#### Step 2: Build & Flash (2 minutes)

```bash
cd /path/to/auto_orientation

# Build the firmware
platformio run

# Flash to your Arduino
platformio run --target upload

# You should see:
# "Uploading .pio/build/arduino_mega/firmware.hex"
# "avrdude done"
```

**Using a different board?** Edit `platformio.ini`:
```ini
[platformio]
default_envs = arduino_nano  # or: arduino_mega, teensy31, esp32dev
```

#### Step 3: Verify Output (1 minute)

Start monitoring the serial output:

```bash
python3 tools/real_time_monitor.py /dev/ttyACM0
```

On Windows, replace `/dev/ttyACM0` with `COM3` (or whatever port your Arduino uses).

**You should see**:
```
Auto Orientation Monitor
========================

Orientation:
  Roll:  12.3°
  Pitch: -4.5°
  Yaw:   247.8°

Position:
  Lat: 37.4419°
  Lon: -122.1430°
  Alt: 150.5m
  
Status: Calibrating...
```

If you see this output, **congratulations!** Your system is working. If not, see [Troubleshooting](#troubleshooting-quick-reference) below.

---

## Troubleshooting Quick Reference

### "USB device not found" or "Connection refused"

**Problem**: Arduino doesn't appear as `/dev/ttyACM0` or `COM3`

**Fixes**:
1. Check USB cable is connected to Arduino (USB-B port)
2. Verify USB cable is data-capable (not power-only)
3. Find correct port:
   ```bash
   # Linux/Mac
   ls /dev/tty*
   
   # Windows
   # Check Device Manager → Ports (COM & LPT)
   ```
4. Update command with correct port:
   ```bash
   python3 tools/real_time_monitor.py /dev/ttyUSB0  # or COM3, etc.
   ```

### "BNO085 FAILED" message

**Problem**: IMU sensor not responding

**Fixes**:
1. **Check P1 pin** (most common issue!):
   - P1 must be connected to 5V, NOT GND
   - Verify with multimeter: should read 5V when powered
   
2. Check UART connections:
   - TX (BNO085) → RX1 (Pin 19)
   - RX (BNO085) → TX1 (Pin 18)
   - Power connections (VCC and GND)

3. Verify 5V supply is stable:
   - Should read 4.8V-5.2V under load
   - Try upgraded power supply if reading lower

4. Recompile and re-upload:
   ```bash
   platformio run --target clean
   platformio run
   platformio run --target upload
   ```

### "No GPS data" or "Waiting for fix..."

**Problem**: GPS module not getting satellite signals

**Fixes**:
1. **Move to clear outdoor location**:
   - GPS needs clear sky view (5+ degrees above horizon)
   - Avoid buildings, trees, bridges overhead
   
2. Wait for initial acquisition:
   - First GPS lock can take 30-60 seconds (cold start)
   - Subsequent locks are faster (warm start)

3. Check antenna:
   - Verify antenna is screwed firmly to NEO-M9N connector
   - Keep antenna vertical or nearly vertical
   - Try moving antenna to different location

4. Verify GPS module is powered:
   - Check USB cable is firmly seated
   - Monitor shows "GPS Connected" status

### "Calibration stuck at 'Low' or 'Medium'"

**Problem**: IMU calibration not progressing

**Fixes**:
1. Perform figure-8 motion:
   - Hold device and move in smooth figure-8 pattern
   - Rotate device on all axes (pitch, roll, yaw)
   - Continue for 30-60 seconds

2. Move in a larger area:
   - Walk in a ~1 meter circle while doing figure-8
   - Avoid staying in one spot

3. Check for magnetic interference:
   - Move away from electronics, power lines
   - Remove from bags/containers with metal components

### "Garbled or corrupted serial data"

**Problem**: Serial output shows unreadable characters

**Fixes**:
1. Verify baud rate (should be 115200):
   ```bash
   python3 tools/real_time_monitor.py /dev/ttyACM0 --baud 115200
   ```

2. Check for loose wires:
   - UART connections can be sensitive to interference
   - Try using a shorter USB cable or level shifter

3. Power supply noise:
   - Add 100μF capacitor near Arduino 5V input
   - Use higher-quality power supply

---

## JSON Output Format (Formatters Quick Start)

The system outputs sensor data in JSON format (recommended) or delimited format.

### Installation

The formatters are located in `src/output/` and are automatically included in the Arduino project.

### Basic Usage

#### SensorOutputManager (Recommended)

```cpp
#include "output/sensor_output_manager.h"

SensorOutputManager output_manager;

void setup() {
  Serial.begin(115200);
  output_manager.begin(OutputFormat::JSON);
  output_manager.setFrequencyHz(10.0f);    // 10 Hz output
  output_manager.setGpsFreshnessTimeoutMs(5000);  // 5s GPS timeout
}

void loop() {
  // Read sensors
  if (imu.read() && imu.hasNewData()) {
    output_manager.update(imu.getOrientation());
  }
  if (gps.read() && gps.hasNewData()) {
    output_manager.update(gps.getPosition());
  }
  
  // Output when ready
  if (output_manager.shouldOutput()) {
    char buffer[512];
    uint16_t len = output_manager.getFormattedOutput(buffer, sizeof(buffer));
    if (len > 0) {
      Serial.println(buffer);
    }
  }
}
```

### Output Formats

#### JSON (Primary Format - Recommended)
```json
{"timestamp":123456,"orientation":{"w":0.7070,"x":0.0000,"y":0.0000,"z":0.7070,"magnitude":1.0000,"calibration":{"system":3,"accel":3,"gyro":3,"mag":2}},"position":{"latitude":37.7740,"longitude":-122.4176,"altitude":12.50,"accuracy_m":5.20,"satellites":12,"fix_quality":1}}
```

**Advantages:**
- Standard format, easy to parse with JSON libraries
- Human-readable with labeled fields
- Includes calibration status for each axis
- Handles missing GPS data gracefully

#### DELIMITED (Simple Alternative)
```
123456|0.7070|0.0000|0.0000|0.7070|3|37.7740|-122.4176|12.50|5.20|12
```

**Advantages:**
- Compact (60-80 bytes vs 300+ for JSON)
- Simple parsing with string.split('|')
- Fields: timestamp|w|x|y|z|cal|lat|lon|alt|accuracy|sats

### Frequency Limiting

Control output bandwidth via the output manager:

```cpp
output_manager.setFrequencyHz(10.0f);   // 10 Hz = 100 ms between outputs
output_manager.setFrequencyHz(1.0f);    // 1 Hz = 1000 ms between outputs
output_manager.setFrequencyHz(5.0f);    // 5 Hz = 200 ms between outputs
```

### Decimal Precision

```cpp
JSONFormatter json(3);    // 3 decimal places
JSONFormatter json(5);    // 5 decimal places
```

---

## NEO-M9N GPS Driver Quick Start

### Installation

The NEO-M9N driver is ready to use. No additional dependencies needed beyond Arduino.h.

### Configuration

Edit `src/config/pins.h` if needed:

```cpp
#define GPS_BAUD_RATE 115200  // Standard for NEO-M9N
```

### Basic Usage

```cpp
#include "sensors/neo_m9n.h"

NEOM9N gps;

void setup() {
  Serial.begin(115200);
  
  if (!gps.begin()) {
    Serial.println("GPS init failed!");
    while(1);
  }
  Serial.println("GPS ready!");
}

void loop() {
  if (gps.read()) {
    if (gps.hasNewData()) {
      const PositionData& pos = gps.getPosition();
      
      Serial.print("Lat: "); Serial.println(pos.latitude, 6);
      Serial.print("Lon: "); Serial.println(pos.longitude, 6);
      Serial.print("Alt: "); Serial.print(pos.altitude, 1); Serial.println("m");
      Serial.print("Acc: "); Serial.print(pos.accuracy_m, 1); Serial.println("m");
      Serial.print("Sats: "); Serial.println(pos.num_satellites);
    }
  }
  
  Serial.println(gps.getStatusString());  // "GPS: 8 sats, GPS fix, HDOP 0.9m"
  delay(1000);
}
```

### Hardware Connection

#### Arduino Mega

```
NEO-M9N GPS    Arduino Mega
   RX     →     TX3 (pin 18)
   TX     →     RX3 (pin 19)
   GND    →     GND
   5V     →     5V (if not USB powered)
```

### Position Data Structure

```cpp
struct PositionData {
  double latitude;         // decimal degrees (-90 to +90)
  double longitude;        // decimal degrees (-180 to +180)
  float altitude;          // meters above WGS84 ellipsoid
  float accuracy_m;        // estimated accuracy (HDOP * 5)
  uint8_t num_satellites;  // satellites in fix
  uint8_t fix_quality;     // 0=invalid, 1=GPS, 2=DGPS, 4=RTK
  uint32_t timestamp_ms;   // system time of fix
};
```

### Fix Quality Codes

```
0: Invalid        → No fix
1: GPS Fix        → Standard 2D/3D fix (±1m)
2: DGPS Fix       → Differential GPS (±0.5m)
3: PPS Fix        → Precise Positioning
4: RTK            → Real-Time Kinematic (±2cm!)
5: RTK Float      → RTK convergence in progress
6: Dead Reckoning → Estimated position
```

### Understanding HDOP

**HDOP** (Horizontal Dilution of Precision) indicates satellite geometry:

```
HDOP < 1.0  →  Excellent (±2-5m accuracy possible)
HDOP 1-2    →  Very Good (±5-10m)
HDOP 2-5    →  Good      (±10-25m)
HDOP 5-10   →  Moderate  (±25-50m)
HDOP > 10   →  Poor      (avoid relying on this)
```

---

## What's Next?

1. **First Calibration**: See [First Calibration Guide](FIRST_CALIBRATION.md)
   - How to calibrate your magnetometer
   - What "fully calibrated" looks like
   - Common mistakes to avoid

2. **Monitor Real-Time Data**: See [Real-Time Monitoring Guide](MONITORING_REAL_TIME_DATA.md)
   - How to interpret the display
   - Logging data for analysis
   - Recording performance metrics

3. **Deploy to Field**: See [Field Deployment Guide](FIELD_DEPLOYMENT.md)
   - Packing checklist
   - Location selection
   - Power management for extended operation

4. **Questions?** See [FAQs](../FAQS.md)
   - Common questions answered
   - Accuracy specifications
   - Adding more sensors

---

## Quick Reference: Common Commands

```bash
# Build only (don't upload)
platformio run

# Upload to board
platformio run --target upload

# Clean build
platformio run --target clean

# Monitor with auto baud detection
python3 tools/real_time_monitor.py /dev/ttyACM0

# Monitor with logging
python3 tools/real_time_monitor.py /dev/ttyACM0 --log data.jsonl

# Monitor without color (for simple terminals)
python3 tools/real_time_monitor.py /dev/ttyACM0 --no-color
```

---

**Last Updated**: 2025-05  
**Difficulty**: Beginner  
**Time Required**: 5-10 minutes
