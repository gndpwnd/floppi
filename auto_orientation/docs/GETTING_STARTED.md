# Getting Started: End-to-End Setup Guide

**Complete walkthrough from hardware assembly through first GPS lock and calibration verification.**

- **Document Status**: v1.0 - Comprehensive end-to-end guide  
- **Target Audience**: Developers/technical users with no prior experience  
- **Time Required**: 30-45 minutes total (includes 5-10 min calibration)  
- **Tested On**: Arduino Mega 2560, BNO085 (Adafruit), NEO-M9N GPS, Windows/Mac/Linux

---

## Table of Contents

1. [Part 1: Hardware Setup](#part-1-hardware-setup)
2. [Part 2: Software Installation](#part-2-software-installation)
3. [Part 3: First Boot and Diagnostics](#part-3-first-boot-and-diagnostics)
4. [Part 4: Calibration Procedure](#part-4-calibration-procedure)
5. [Part 5: Verification and Testing](#part-5-verification-and-testing)
6. [Part 6: GPS Integration Check](#part-6-gps-integration-check)
7. [Next Steps](#next-steps)
8. [Troubleshooting](#troubleshooting)

---

## Part 1: Hardware Setup

### Bill of Materials

| Item | Part Number | Qty | Notes |
|---|---|---|---|
| Arduino Mega 2560 | A000067 | 1 | Main controller |
| BNO085 9-axis IMU | Adafruit 4754 | 1 | Orientation sensor (UART mode) |
| u-blox NEO-M9N GPS | u-blox or Ublox | 1 | Position sensor with antenna |
| USB-B to USB-A Cable | Standard | 2 | One for Arduino, one for GPS |
| 5V Power Supply | Generic | 1 | 1A minimum (USB charger OK) |
| Resistor | 10kΩ | 1 | For BNO085 P1 pullup (optional) |
| Capacitor | 100-220μF | 1 | For power supply decoupling (optional) |
| Breadboard | Standard | 1 | For connecting BNO085 (optional) |
| Male/Female Jumper Wires | Standard | 8-10 | For breadboard connections |

### Assembly: BNO085 UART Connection

The BNO085 breakout board communicates with Arduino via UART (serial port). P1 pin must be set HIGH to enable UART mode.

#### Wiring Diagram

```
Arduino Mega 2560                BNO085 Breakout
====================              ===============

Pin 18 (TX1)        ─────────────► RX
Pin 19 (RX1)        ◄───────────── TX
5V Rail             ─────────────► VCC
GND Rail            ◄───────────── GND
5V Rail             ─────────────► P1 (via 10kΩ resistor)

Optional (for stability):
GND Rail            ─────────────► GND
5V Rail             ◄───[100μF]──► GND (decoupling capacitor)
```

#### Step-by-Step Assembly

1. **Prepare breadboard** (if using):
   ```
   Place Arduino Mega to the right
   Place BNO085 breakout to the left
   Leave space in middle for wires
   ```

2. **Connect serial communications**:
   ```
   Wire 1: Arduino Pin 18 (TX1) ──► BNO085 RX pin
   Wire 2: Arduino Pin 19 (RX1) ◄── BNO085 TX pin
   ```

3. **Connect power**:
   ```
   Wire 3: Arduino 5V ──► BNO085 VCC
   Wire 4: Arduino GND ◄── BNO085 GND
   ```

4. **Connect P1 mode pin** (critical for UART):
   ```
   Wire 5: Arduino 5V ──[10kΩ resistor]──► BNO085 P1 pin
   
   OR (alternative):
   Wire 5: Arduino Pin X (set HIGH in setup()) ──► BNO085 P1 pin
   ```

5. **Add decoupling capacitor** (optional but recommended):
   ```
   Capacitor positive ──► Arduino 5V
   Capacitor negative ──► Arduino GND
   (Place near BNO085 power pins)
   ```

6. **Verify connections**:
   - [ ] TX1/RX1 wires connected (not reversed!)
   - [ ] 5V and GND connected to BNO085
   - [ ] P1 pin is HIGH (5V)
   - [ ] All connections are tight (no loose wires)
   - [ ] No shorts between adjacent pins

#### Wiring Verification

```bash
# Use multimeter to verify (before powering on):
- Arduino 5V pin to BNO085 VCC: should show ~5V
- Arduino GND pin to BNO085 GND: should show ~0V (same reference)
- Arduino 5V pin to BNO085 P1: should show ~5V (or close, depends on resistor)
```

### Assembly: GPS (NEO-M9N) Connection

The NEO-M9N GPS receiver connects via USB to the Arduino Mega's USB port.

#### USB Connection Steps

1. **Locate USB ports on Arduino Mega**:
   ```
   USB-B port (Programming) ← Left side of board
   USB 2.0 port (Host) ← Right side of board (not used in standard setup)
   ```

2. **Connect USB-B cable from GPS to Arduino**:
   ```
   USB-B cable: One end to NEO-M9N
                Other end to Arduino Mega's USB-B port
   ```

3. **Attach antenna to GPS**:
   ```
   NEO-M9N has antenna connector (SMA or similar)
   Attach antenna (provided with u-blox kit)
   Position antenna pointing upward (for clear sky view)
   ```

4. **Position antenna for clear sky**:
   ```
   GPS needs clear view to satellites
   Ideal: Mounted on outside of enclosure with clear sky view
   Acceptable: Next to window indoors (weaker signal but works)
   Poor: Inside building with no window view (may not lock)
   ```

### Power Connection

#### Option 1: USB Power (Simplest for testing)

```
USB-A to USB-B cable
USB Power Adapter (5V, 1A)
    ↓
USB-B cable to Arduino Mega USB port
    ↓
Arduino: 5V and GND automatically powered
```

**Pros**: Single cable, no additional equipment  
**Cons**: Limited current (USB 500mA max)

#### Option 2: Dedicated 5V Supply (Better for deployment)

```
5V Power Supply (1A+)
    ↓
[GND] ──► Arduino GND pin
[+5V] ──► Arduino 5V pin (via barrel jack or direct pin)
    ↓
BNO085 and GPS both powered from Arduino 5V rail
```

**Pros**: More current available, more stable  
**Cons**: Requires separate power supply

### Initial Hardware Test

Before uploading firmware, verify hardware is working:

1. **Power on the system**:
   ```
   Connect 5V power (USB or dedicated supply)
   Red LED on Arduino should light up
   ```

2. **Listen for USB enumeration**:
   ```
   Computer should recognize:
   - Arduino Mega (usually /dev/ttyACM0 on Linux)
   - NEO-M9N GPS (if USB cable connected)
   ```

3. **Verify with device detection**:
   ```bash
   # Linux/Mac:
   ls /dev/tty*
   # Look for /dev/ttyACM0, /dev/ttyACM1, /dev/ttyUSB*
   
   # Windows:
   # Open Device Manager → Ports (COM & LPT)
   # Look for "Arduino" or "U-Blox"
   ```

---

## Part 2: Software Installation

### Prerequisites

Before starting, ensure you have:

```bash
# 1. Python 3.7+ installed
python3 --version
# Output: Python 3.X.X (should be 3.7+)

# 2. Git installed (for cloning repository)
git --version
# Output: git version X.X.X

# 3. USB drivers for Arduino (usually automatic)
# Windows: May need driver from arduino.cc
# Mac/Linux: Built-in
```

### Step 1: Clone the Repository

```bash
cd ~
git clone https://github.com/YOUR_ORG/auto_orientation.git
cd auto_orientation
```

### Step 2: Install PlatformIO

**Option A: PlatformIO CLI (Recommended)**

```bash
# Install via pip
pip install platformio

# Verify installation
platformio --version
# Output: PlatformIO Core X.X.X
```

**Option B: PlatformIO IDE (VSCode extension)**

```
1. Install VSCode: https://code.visualstudio.com/
2. Install PlatformIO IDE extension (search in VSCode extensions)
3. Reload VSCode
4. PlatformIO should be ready to use
```

### Step 3: Install Python Serial Library

```bash
pip install pyserial

# Verify
python3 -c "import serial; print(serial.__version__)"
```

### Step 4: Configure for Your Board

Edit `platformio.ini`:

```ini
[platformio]
default_envs = arduino_mega    # or: arduino_nano, teensy31, esp32dev

[env:arduino_mega]
platform = atmelavr
board = megaatmega2560
framework = arduino
lib_deps =
    Adafruit Unified Sensor
    Adafruit BusIO
```

If using different board, update `board` parameter:

```
Arduino Nano:  board = nanoatmega328
Arduino Uno:   board = uno
Teensy 3.1:    board = teensy31
ESP32:         board = esp32dev
```

### Step 5: Build the Firmware

```bash
cd /path/to/auto_orientation

# Build for your configured board
platformio run

# Output should end with:
# ============ [SUCCESS] ============
```

If build fails, check:
- [ ] PlatformIO installed correctly (`platformio --version`)
- [ ] Board specified in `platformio.ini`
- [ ] All dependencies listed in `lib_deps` are available

### Step 6: Upload to Arduino

```bash
# Connect Arduino via USB first!

# Upload firmware
platformio run --target upload

# You should see:
# Uploading .pio/build/arduino_mega/firmware.hex
# avrdude done
# ============ [SUCCESS] ============
```

**Troubleshooting upload failures:**

```bash
# If upload fails with "could not open port":
1. Check USB cable is connected to correct port
2. Find correct port:
   ls /dev/tty*
3. Specify port manually:
   platformio run --target upload --upload-port /dev/ttyACM0
   
# On Windows, use COM port:
   platformio run --target upload --upload-port COM3
```

---

## Part 3: First Boot and Diagnostics

### Boot Sequence

When you power on the Arduino for the first time:

1. **Arduino boots** (~1 second)
2. **BNO085 initializes** (~2 seconds)
3. **GPS searches for satellites** (~30-60 seconds initially)
4. **System ready** outputs diagnostic messages

### Monitoring Serial Output

#### Using simple_monitor.py

```bash
# Start monitoring
python3 tools/simple_monitor.py /dev/ttyACM0

# Expected output:
# ============================================================
# Connected to /dev/ttyACM0 at 115200 baud
# Press CTRL+C to exit
# ============================================================
```

#### Expected First Boot Output

```
[HH:MM:SS.mmm] JSON #1 PARSED OK
  Quaternion: w=0.707107 x=0.000000 y=0.000000 z=0.707107
  Calibration:
    System: 0 ░░░
    Accel:  0 ░░░
    Gyro:   0 ░░░
    Mag:    0 ░░░
  ✗ NOT CALIBRATED - move board now!

[HH:MM:SS.mmm] JSON #2 PARSED OK
  Quaternion: w=0.707107 x=0.000000 y=0.000000 z=0.707107
  Calibration:
    System: 0 ░░░
    Accel:  0 ░░░
    Gyro:   0 ░░░
    Mag:    0 ░░░
  ✗ NOT CALIBRATED - move board now!
```

### Interpreting Boot Diagnostics

**Good signs** ✓
```
- JSON output arriving at ~10 Hz (one line per 100ms)
- Quaternion values present (w, x, y, z)
- Calibration status shows (even if all zeros)
- No error messages in monitor
```

**Warning signs** ⚠
```
- Monitor shows no output → Check USB cable and port
- JSON parsing errors → Firmware may be corrupted
- Calibration values always at max (3) → Sensor may be hung
- Garbled characters → Serial port speed mismatch (check platformio.ini)
```

### First Boot Verification Checklist

- [ ] Monitor is running and shows JSON output
- [ ] Output is arriving at ~10 Hz (rapid updates)
- [ ] Quaternion values are reasonable (w, x, y, z all between -1 and 1)
- [ ] Calibration status is visible (even if all zeros is fine)
- [ ] No error messages or crashes

If any items are failing, see [Troubleshooting](#troubleshooting) at end of document.

---

## Part 4: Calibration Procedure

The BNO085 requires calibration of its magnetometer for accurate yaw (heading) measurement.

### Why Calibrate

```
Without calibration: Yaw (heading) can be off by 30-90°
With calibration:    Yaw (heading) accurate to ±2-5°
```

Calibration saves automatically and persists across power cycles.

### Pre-Calibration Checklist

- [ ] Monitor is running and showing JSON output
- [ ] You have 5-10 minutes available
- [ ] You're in a space away from large metal objects
- [ ] You're indoors (GPS doesn't matter for IMU calibration)

### Calibration Steps

#### Step 1: Start the Monitor (already done)

You should see:
```
Calibration:
  System: 0 ░░░
  Accel:  0 ░░░
  Gyro:   0 ░░░
  Mag:    0 ░░░
```

#### Step 2: Wait for Sensor Warmup (10 seconds)

Do nothing, let sensor initialize.

```
Timeline:
0s:  Calibration showing all zeros (normal)
5s:  Still warming up (normal)
10s: Ready to start calibration motion
```

#### Step 3: Perform Figure-8 Motion (60+ seconds)

This is the key step. The motion pattern helps the magnetometer "see" Earth's field from all angles.

**Motion Pattern**:
```
Imagine drawing a lazy figure-8 in 3D space while rotating the device:

    /\          Simultaneously:
   /  \         - Rotate on all axes (roll, pitch, yaw)
  /    \        - Walk in circles (~1-2 meter diameter)
 |      |       - Move smoothly, not jerky
  \    /        - Continue for 60+ seconds
   \  /
    \/
```

**Detailed Technique**:
1. Hold device naturally in your hand
2. Move in a figure-8 pattern (sideways, like infinity symbol)
3. Rotate the device on all three axes:
   - Roll: Rotate around front-back axis (like rolling a ball)
   - Pitch: Tilt up-down (nodding yes)
   - Yaw: Rotate left-right (shaking head for no)
4. Walk slowly in circles while doing this
5. Keep moving for **at least 60 seconds** (full motion)

**Watch the monitor** as you move:

```
Time     Calibration Status            What You See
----     ──────────────────            ────────────
0-10s    Mag: 0 ░░░                    "NOT CALIBRATED"
10-20s   Mag: 0-1 █░░                  "move board now!"
20-40s   Mag: 1 █░░                    "Calibrating..."
40-60s   Mag: 1-2 ██░                  "Calibrating..."
60-80s   Mag: 2-3 ███                  "Almost done!"
80+s     Mag: 3 ███                    "FULLY CALIBRATED!"
```

#### Step 4: Confirm Completion

Stop moving when you see:

```
Calibration:
  System: 3 ███
  Accel:  3 ███
  Gyro:   3 ███
  Mag:    3 ███
✓✓✓ FULLY CALIBRATED! ✓✓✓
```

**Acceptable minimum**: Mag >= 2 (you can stop there if time-limited)  
**Ideal**: All values = 3 (full calibration)

### Post-Calibration Verification

1. **Keep device still**: Quaternion values should not change
   ```
   Quaternion: w=0.725 x=0.023 y=-0.045 z=0.687
   Quaternion: w=0.725 x=0.023 y=-0.045 z=0.687  ✓ Same
   ```

2. **Rotate slowly**: Quaternion should change smoothly
   ```
   [Rotate device 90° clockwise]
   Quaternion: w=0.500 x=0.000 y=0.000 z=0.866  ✓ Smooth change
   ```

3. **Test with compass**: Use compass app to verify yaw accuracy
   ```
   Compass app says:  North (0°)
   Monitor shows:     Yaw ≈ 358-2° (±2° is excellent!)
   
   Rotate 90°:
   Compass app says:  East (90°)
   Monitor shows:     Yaw ≈ 88-92° (±2° is excellent!)
   ```

---

## Part 5: Verification and Testing

### Test 1: Orientation Accuracy

**Objective**: Verify pitch, roll, yaw are measured correctly

**Procedure**:

1. Place device on flat table
2. Note yaw value from monitor
3. Use compass app to verify it matches magnetic north
4. Rotate device 90° clockwise
5. Verify yaw changed by approximately 90°

**Expected Result**:
```
Starting position:
  Compass: North (0°)
  Monitor: Yaw ≈ 358°

After 90° rotation:
  Compass: East (90°)
  Monitor: Yaw ≈ 88°

Difference: ≈ 90° (exact match or within ±2°)
```

### Test 2: Data Output Rate

**Objective**: Verify data is arriving at expected rate (10 Hz for IMU)

**Procedure**:

```bash
# Count lines received in 10 seconds
python3 -c "
import serial
import time

ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
time.sleep(1)

start = time.time()
count = 0
while time.time() - start < 10:
    if ser.readline():
        count += 1

print(f'Lines received in 10 seconds: {count}')
print(f'Expected rate: 100 (10 Hz * 10 sec)')
print(f'Actual rate: {count / 10} Hz')

ser.close()
"

# Expected output:
# Lines received in 10 seconds: ~100
# Expected rate: 100
# Actual rate: 10.0 Hz
```

**Expected Result**: ~10 lines per second (±10% variance acceptable)

### Test 3: Persistence Across Power Cycles

**Objective**: Verify calibration is saved and auto-loads on restart

**Procedure**:

1. Note current calibration status
   ```
   Monitor shows: Mag: 3 ███
   ```

2. Power off device (unplug USB)
3. Wait 5 seconds
4. Power on again
5. Watch monitor output

**Expected Result**:

```
Before power cycle:
  Mag: 3 ███

Immediately after power-on:
  Mag: 3 ███ (should be loaded immediately, or within 1-2 seconds)

Timeline:
Power off → [5 second wait] → Power on → [100ms] → Mag: 3 shows up
```

**Result**: Calibration is persisted ✓

### Test 4: Multiple Boot Cycles

**Objective**: Verify stability over multiple power cycles

**Procedure**:

```bash
for i in {1..5}; do
  echo "Cycle $i:"
  echo "  Power off now, wait 5 seconds, power on..."
  sleep 5
  echo "  Powered on, checking calibration..."
  python3 tools/simple_monitor.py /dev/ttyACM0 --lines 5 --timeout 5
  echo ""
done
```

**Expected Result**: Calibration mag level stays consistent (all 5 cycles should show mag = 3)

---

## Part 6: GPS Integration Check

### GPS Acquisition Timeline

GPS signal acquisition takes time:

```
Cold start (first time, no satellite history):
  Time: 30-120 seconds
  Reason: Receiver must download ephemeris data from satellites

Warm start (satellite history available):
  Time: 1-30 seconds
  Reason: Previous position and satellite data known

Hot start (recent position, current almanac):
  Time: < 5 seconds
  Reason: All data cached from previous acquisition
```

### Prerequisites for GPS Lock

1. **Clear sky view**: Antenna must see at least 4 satellites
   ```
   Outdoor location best: Clear sky above
   Window sill acceptable: Some blockage tolerated
   Inside building: May not work
   ```

2. **Antenna orientation**: Should point upward
   ```
   Ideal: Mounted on roof/outside with clear view
   OK: Antenna positioned toward window
   Poor: Antenna facing building (blocked by metal/RF)
   ```

3. **Time**: Allow 30-60 seconds for cold start

### GPS Verification Procedure

#### Step 1: Move to Outdoor Location

```
Find location with:
- Clear sky view above (no roof/trees blocking)
- Away from large buildings or metal structures
- Open area preferred (park, parking lot, field)
```

#### Step 2: Position Antenna for Sky View

```
Antenna should point upward at roughly 45° angle.

Good:       Bad:
  ^         ←  (pointing sideways)
  |
  ↗         ↓  (pointing down)
```

#### Step 3: Monitor GPS Output

```bash
python3 tools/simple_monitor.py /dev/ttyACM0

# Watch for GPS data in output:
Position:
  Lat: 37.4419°
  Lon: -122.1430°
  Alt: 150.5m
  Num Satellites: 12
  Fix Quality: 1
```

#### Step 4: Verify Fix Quality

```
Fix Quality Meanings:
  0 = No fix (no position yet)
  1 = GPS fix (good)
  2 = DGPS fix (better, with ground station)
  3 = PPS fix (best, precise positioning)

Expected: Quality 1-2 for regular GPS
```

#### Step 5: Record Coordinates

```
Once you see valid latitude/longitude, record them:

Your location:
  Latitude: _____________
  Longitude: ____________
  Altitude: _____________

Use web tool to verify reasonableness:
  1. Open Google Maps
  2. Paste coordinates (Lat, Lon)
  3. Verify location matches your actual position

Example:
  System shows: Lat 37.4419, Lon -122.1430
  Maps shows: San Jose, CA ✓ (matches your location)
```

### GPS Acquisition Troubleshooting

**GPS not acquiring fix after 2 minutes**:

1. Check antenna connection (snug)
2. Move to location with clearer sky view
3. Move antenna higher (roof vs ground)
4. Try warm-up: Power on, leave outside for 5 minutes
5. Check that NEO-M9N USB cable is connected

**Fix quality is 0 but numbers keep appearing**:

```
This means: Receiver is working but can't lock to satellites yet
Action: Wait longer, move to better location
Wait: 30-120 seconds is normal for cold start
```

---

## Part 7: End-to-End Verification Test

Run all components together:

### Full System Test Script

```bash
#!/bin/bash
# Run this in your project directory

echo "=== AUTO ORIENTATION FULL SYSTEM TEST ==="
echo ""

# Test 1: Serial connectivity
echo "[1/5] Testing serial connectivity..."
python3 -c "
import serial
import time
ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
time.sleep(0.5)
ser.write(b'')
print('  ✓ Serial port open')
ser.close()
"

# Test 2: JSON parsing
echo "[2/5] Testing JSON parsing..."
python3 -c "
import serial
import json
import time
ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
time.sleep(0.5)
for i in range(5):
    line = ser.readline().decode('utf-8', errors='ignore').strip()
    if line.startswith('{'):
        data = json.loads(line)
        print(f'  ✓ JSON #{i+1} parsed OK')
        break
ser.close()
"

# Test 3: Calibration status
echo "[3/5] Checking calibration status..."
python3 tools/simple_monitor.py /dev/ttyACM0 --lines 3 --timeout 5

# Test 4: Data rate
echo "[4/5] Measuring data rate..."
python3 -c "
import serial
import time
ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
start = time.time()
count = 0
while time.time() - start < 3:
    if ser.readline():
        count += 1
rate = count / (time.time() - start)
print(f'  Data rate: {rate:.1f} Hz (expected ~10 Hz)')
ser.close()
"

# Test 5: GPS status
echo "[5/5] Checking GPS status (move outdoors first!)..."
echo "  Position:"
python3 tools/simple_monitor.py /dev/ttyACM0 --lines 10 --timeout 5 | grep -A 5 "Position:"

echo ""
echo "=== TEST COMPLETE ==="
```

### Full System Test Expected Output

```
=== AUTO ORIENTATION FULL SYSTEM TEST ===

[1/5] Testing serial connectivity...
  ✓ Serial port open

[2/5] Testing JSON parsing...
  ✓ JSON #1 parsed OK

[3/5] Checking calibration status...
[HH:MM:SS.mmm] JSON #1 PARSED OK
  Calibration:
    System: 3 ███
    Mag:    3 ███
  ✓✓✓ FULLY CALIBRATED! ✓✓✓

[4/5] Measuring data rate...
  Data rate: 10.0 Hz (expected ~10 Hz)

[5/5] Checking GPS status...
  Position:
    Lat: 37.4419°
    Lon: -122.1430°
    Num Satellites: 12

=== TEST COMPLETE ===
```

---

## Next Steps

### Immediate Next Steps

1. **Explore output data**:
   ```bash
   python3 tools/simple_monitor.py /dev/ttyACM0 --log my_data.jsonl
   ```
   This logs data to a file for analysis.

2. **Read detailed documentation**:
   - **Calibration Guide**: `docs/CALIBRATION_GUIDE.md` (detailed calibration help)
   - **Absolute Orientation Theory**: `docs/ABSOLUTE_ORIENTATION_EXPLAINED.md` (understand the physics)

3. **Experiment with data**:
   - Rotate device and watch quaternion values change
   - Compare yaw to compass app
   - Move around and observe GPS position updates

### Integration with Your Project

1. **Using raw JSON data**:
   ```bash
   # Stream JSON directly
   python3 tools/simple_monitor.py /dev/ttyACM0
   
   # Parse in your code
   import json
   data = json.loads(line)
   orientation = data['orientation']
   yaw = atan2(2*(w*z + x*y), 1 - 2*(y^2 + z^2))  # Convert to degrees
   ```

2. **Using CSV format** (if preferred):
   - Edit `src/main.cpp` to use `CSVFormatter` instead of `JSONFormatter`
   - Rebuild: `platformio run`
   - Output will be comma-separated instead of JSON

3. **Custom data processing**:
   - See `tools/EXAMPLES.md` for Python examples
   - See `docs/reference/API_REFERENCE.md` for detailed API

### Advanced Topics

- **Adding new sensors**: See `docs/guides/ADDING_NEW_SENSORS.md`
- **Field deployment**: See `docs/guides/FIELD_DEPLOYMENT.md`
- **Real-time monitoring dashboard**: See `tools/MONITOR_README.md`
- **Data logging and analysis**: See `docs/guides/MONITORING_REAL_TIME_DATA.md`

### Troubleshooting Resources

- **Calibration issues**: See `docs/CALIBRATION_GUIDE.md#troubleshooting`
- **GPS acquisition**: See `docs/findings/gps_lock_troubleshooting.md`
- **Hardware issues**: See `docs/guides/HARDWARE_SETUP.md`
- **General FAQ**: See `docs/FAQS.md`

---

## Troubleshooting

### "Serial port not found" or "Permission denied"

**Problem**: Monitor script can't open serial port

**Fixes**:

```bash
# Linux: Check what ports are available
ls /dev/tty*
# Look for /dev/ttyACM0, /dev/ttyACM1, /dev/ttyUSB0, etc.

# Linux: Add user to dialout group
sudo usermod -a -G dialout $USER
# Then logout and log back in

# Mac: Ports should work automatically
# Try COM3, COM4, etc.

# Windows: Check Device Manager → Ports (COM & LPT)
# Note the COM port number and use it:
python3 tools/simple_monitor.py COM3
```

---

### "Upload failed: could not open port"

**Problem**: PlatformIO can't upload firmware to Arduino

**Fixes**:

```bash
# 1. Verify USB cable is connected
# 2. Check Arduino appears in device list
ls /dev/tty*  # Linux/Mac
# or Device Manager (Windows)

# 3. Try upload with explicit port
platformio run --target upload --upload-port /dev/ttyACM0

# 4. Check driver is installed
# Windows: May need Arduino drivers from arduino.cc

# 5. Try different USB port on computer
# USB 2.0 ports work more reliably than USB 3.0
```

---

### "JSON parsing error" or "Malformed JSON"

**Problem**: Monitor script can't parse JSON from device

**Fixes**:

```bash
# 1. Verify firmware uploaded correctly
platformio run --target upload

# 2. Verify serial speed (should be 115200)
# Check platformio.ini: Serial1.begin(115200)

# 3. Try raw serial monitoring
# (Don't use simple_monitor.py, just view raw output)
cat /dev/ttyACM0  # Linux/Mac
# or use Arduino IDE serial monitor

# 4. Rebuild firmware
platformio clean
platformio run --target upload
```

---

### "BNO085 FAILED" or no orientation data

**Problem**: IMU sensor not responding

**Fixes**:

1. **Check P1 pin voltage** (most common issue!):
   ```bash
   Use multimeter: P1 to GND should read ~5V
   If reading ~0V: P1 not connected to 5V
   If reading ~3.3V: P1 connected incorrectly
   ```

2. **Check UART connections**:
   ```
   Verify:
   - TX (BNO085) → RX1 (Arduino pin 19)
   - RX (BNO085) → TX1 (Arduino pin 18)
   - NOT reversed
   ```

3. **Check power**:
   ```
   BNO085 VCC should read: ~5V ± 0.2V
   If lower: Power supply too weak
   If unstable: Add capacitor for stability
   ```

4. **Reseat all connections**:
   ```
   Disconnect and reconnect all wires
   Ensure tight fit in breadboard or connectors
   ```

---

### "GPS not acquiring fix"

**Problem**: GPS coordinates not appearing after 2+ minutes

**Fixes**:

1. **Move outdoors**:
   ```
   GPS needs clear sky view
   Move away from buildings and trees
   Position antenna pointing upward
   ```

2. **Check antenna connection**:
   ```
   Verify antenna is connected to NEO-M9N
   Try a different antenna if available
   Check connector is tight
   ```

3. **Wait longer for cold start**:
   ```
   First GPS lock can take 30-120 seconds
   Leave device outside for 5+ minutes
   ```

4. **Verify USB connection**:
   ```
   GPS is connected via USB, not UART
   Check USB cable is fully inserted
   Try different USB port on computer
   ```

---

### "Calibration won't improve past mag = 2"

**Problem**: Calibration status stuck at medium (██░)

**Fixes**:

1. **Increase motion vigor**:
   ```
   Move more dramatically
   Larger figure-8 patterns
   Faster rotation on all axes
   Continue for 120+ seconds
   ```

2. **Change location**:
   ```
   Move away from electronics (computers, routers)
   Move away from large metal objects
   Try outdoors in open space
   ```

3. **Accept mag = 2**:
   ```
   mag = 2 is fully acceptable for most applications
   Provides ±5-10° yaw accuracy
   Reaching mag = 3 requires near-perfect conditions
   mag = 2 is good enough for field deployment
   ```

---

### "Orientation values are constantly changing" (even when device is still)

**Problem**: Quaternion values fluctuate wildly

**Fixes**:

1. **Check for vibration**:
   ```
   Place device on stable surface
   Remove any shaking or movement
   Vibration causes gyro noise
   ```

2. **Check power supply**:
   ```
   Unstable power causes sensor noise
   Use wall-powered 5V supply instead of USB
   Add capacitor (100-220μF) for smoothing
   Check power cable for damage
   ```

3. **Verify serial speed** (less likely):
   ```
   Mismatch between platformio.ini and actual baud rate
   Causes data corruption and noise
   Verify: Serial1.begin(115200)
   ```

---

## Summary Checklist

### Hardware Assembly
- [ ] BNO085 UART wired (pins 18/19)
- [ ] BNO085 P1 pin connected to 5V
- [ ] BNO085 power connected (5V, GND)
- [ ] GPS USB cable connected
- [ ] GPS antenna attached
- [ ] Power supply connected
- [ ] All connections verified and tight

### Software Installation
- [ ] Python 3.7+ installed
- [ ] PlatformIO CLI installed
- [ ] PySerial installed
- [ ] Repository cloned
- [ ] platformio.ini configured for board
- [ ] Firmware built (`platformio run`)
- [ ] Firmware uploaded (`platformio run --target upload`)

### First Boot
- [ ] Serial monitor running
- [ ] JSON output arriving at ~10 Hz
- [ ] Quaternion values visible
- [ ] Calibration status visible

### Calibration
- [ ] Calibration procedure completed
- [ ] Mag status reached >= 2 (or = 3)
- [ ] Monitor shows "FULLY CALIBRATED"
- [ ] Yaw verified with compass app

### Verification
- [ ] Orientation accuracy tested
- [ ] Data rate verified (~10 Hz)
- [ ] Power cycle persistence tested
- [ ] GPS lock acquired (outdoors)

### Ready for Deployment
- [ ] All items above completed
- [ ] System tested for 5+ minutes
- [ ] Data logging working
- [ ] Ready for field deployment

---

**Last Updated**: 2026-05  
**Version**: 1.0  
**Difficulty Level**: Beginner to Intermediate  
**Tested On**: Arduino Mega 2560, BNO085 (Adafruit), NEO-M9N GPS  
**Supported Platforms**: Windows, macOS, Linux
