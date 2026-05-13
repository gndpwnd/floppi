# Frequently Asked Questions (FAQs)

Common questions from users getting started with Auto Orientation.

---

## General Questions

### Q: What is Auto Orientation?

**A:** Auto Orientation is a sensor toolkit that automatically measures orientation (pitch, roll, yaw) and position (latitude, longitude, altitude) in real-time. It combines:
- **BNO085 9-axis IMU** for accurate orientation (pitch/roll/yaw)
- **Ublox NEO-M9N GPS** for position and altitude
- **Arduino Mega 2560** as the controller
- **Python monitoring tools** for real-time display and data logging

Think of it as a "black box" flight data recorder for orientation and position—automatically logging where you are and which direction you're facing.

---

### Q: What can I use this for?

**A:** Common applications:
- **Drone/UAV flight testing** — measure orientation during flight
- **Mobile robotics** — track robot heading and position
- **Surveying & mapping** — precise orientation for camera pointing
- **Vehicle tracking** — heading and position logging
- **Sports/fitness** — measure body orientation or training motion
- **Research** — orientation and position data collection
- **Calibration testing** — verify IMU calibration quality

---

### Q: Do I need both sensors (IMU + GPS)?

**A:** No, you can use just the IMU:
- **IMU only**: Orientation (pitch/roll/yaw) works without GPS
- **GPS optional**: Useful for accurate position, but not required for orientation
- **Standalone**: Works indoors (where GPS doesn't) for orientation measurement

If you only need heading/orientation, the BNO085 alone is sufficient.

---

### Q: What Arduino boards are supported?

**A:** Officially tested and supported:
- **Arduino Mega 2560** — Primary target, fully tested
- **Arduino Nano** — Also supported (smaller, lower power)
- **Arduino Uno** — Supported (limited memory)
- **Teensy 3.1+** — Supported (powerful alternative)
- **ESP32** — Supported (WiFi-enabled variant)

See `platformio.ini` for configuration options.

---

## Accuracy & Performance

### Q: How accurate is orientation (pitch/roll/yaw)?

**A:** With proper calibration:
- **Pitch & Roll**: ±2-3° typical, ±1° under ideal conditions
- **Yaw (Heading)**: ±5° typical, ±2-3° with good calibration
- **Requires**: Complete magnetometer calibration ("High" status)
- **Environmental**: Accuracy degrades near large metal objects or RF interference

**Comparison to compass:**
- Compass: ±15-20° typical
- Auto Orientation: ±2-5° with calibration
- Our system is 5-10x more accurate

---

### Q: How accurate is GPS position?

**A:** 
- **Typical accuracy**: 1-3 meters (1-sigma)
- **Excellent conditions**: <1 meter (with DGPS/RTK, extra cost)
- **Degraded conditions**: 5-10 meters (cloudy sky, urban canyon)
- **Altitude**: ±5-10 meters (less accurate than horizontal)

**Accuracy metrics:**
```
CEP (Circular Error Probable): 68% confidence radius
- CEP 2m = 68% of fixes within 2m circle
- CEP 5m = degraded signal, move to clearer location
```

---

### Q: How do I improve accuracy?

**A:**

**For Orientation (Yaw/Heading):**
1. Complete full magnetometer calibration (reach "High" status)
2. Minimize metal objects nearby (phones, watches, metal frames)
3. Stay away from power lines and RF sources
4. Recalibrate if moving to location with different magnetic anomalies

**For Position (GPS):**
1. Ensure antenna has clear sky view (5+ degrees above horizon)
2. Move to outdoor location with open sky
3. Wait longer for initial acquisition (60+ seconds first time)
4. Use base station/RTK for submeter accuracy (advanced)

**For IMU Data Quality:**
1. Avoid sudden vibrations or shocks
2. Keep device temperature stable
3. Allow 30 seconds warmup after power-on for thermal stabilization

---

### Q: What's the update rate (how fast is data updated)?

**A:** 
- **Orientation**: ~100 Hz (100 measurements per second)
- **GPS position**: 1-10 Hz (configurable, default 1 Hz)
- **Combined output**: 100 Hz (IMU updates fast, position updates slowly)

Data arrives in JSON format at 115200 baud serial:
```
Frame 1: {..., yaw: 245.1, lat: 37.4419, ...}
Frame 2: {..., yaw: 245.2, lat: 37.4419, ...}
Frame 3: {..., yaw: 245.3, lat: 37.4420, ...}
         ← Position updates every ~1 second (slower)
         ← Orientation updates every ~10ms (faster)
```

---

## Calibration Questions

### Q: How long does calibration take?

**A:** 
- **First time**: 1-2 minutes (including initial sensor warmup)
- **Typically**: 30-60 seconds of active motion needed
- **Fastest**: 30 seconds (vigorous figure-8 motion)
- **Typical**: 45-60 seconds (comfortable, smooth motion)

Timeline:
```
0-10 sec:  Sensor warmup (watch for "Unreliable" status)
10-40 sec: Active calibration (performing figure-8 motion)
40-60 sec: Fine-tuning (continue moving until "High" status)
```

---

### Q: What happens after I calibrate? Do I lose it?

**A:** No! Calibration is **permanently saved**:
- **Saved in**: BNO085's internal flash memory
- **Persists**: Across unlimited power cycles
- **Timing**: Automatic, no action needed on reboot
- **Benefit**: Saves 30+ seconds per boot cycle

You calibrate once per location (approximately), then it's permanent.

---

### Q: When do I need to recalibrate?

**A:** Recalibrate when:
- ✓ Moving to new geographic location (different magnetic field)
- ✓ Yaw starts drifting or behaving erratically
- ✓ After adding large metal components near sensor

**Do NOT recalibrate:**
- ✗ Every time you power on (it's saved!)
- ✗ Every time you move between rooms (indoors is stable)
- ✗ Between test runs (unless position changed significantly)
- ✗ Due to seasonal changes (Earth's field is stable)

---

### Q: Can I calibrate indoors?

**A:** Yes, absolutely:
- Calibration uses Earth's magnetic field (same everywhere locally)
- You can calibrate in a room, outdoors, or anywhere
- Indoors is actually convenient (climate controlled, stable)
- No GPS lock required for calibration

Avoid calibrating:
- In areas with lots of metal/electronics
- Near microwave ovens (active RF interferes)
- Inside vehicles with metal frames
- Directly next to WiFi routers

---

### Q: Why is my calibration stuck at "Medium"?

**A:** Calibration not progressing means insufficient motion data.

**Fixes**:
1. **Move more vigorously**:
   - Larger figure-8 motions (bigger arm movements)
   - Rotate more dramatically on all axes
   - Walk in bigger circles (2-3 meters)

2. **Try different location**:
   - Move to different room or outdoors
   - Avoid areas with metal objects nearby
   - Get away from electronics

3. **Check power stability**:
   - Unstable power can confuse sensor calibration
   - Use multimeter to verify 5V is steady
   - Try different power supply

---

### Q: My calibration shows "High" but yaw still seems off. Why?

**A:** "High" status means calibration complete, but yaw accuracy depends on:

1. **Environmental magnetic interference**:
   - Large metal objects nearby can offset yaw by 10-30°
   - This is *expected*, not a calibration failure
   - Try moving to different location to verify

2. **Reference compass comparison**:
   - Your phone compass might be inaccurate (±10°)
   - Magnetic north vs true north (varies by location)
   - Try checking against GPS bearing if available

3. **Sensor orientation**:
   - Make sure device orientation is consistent
   - Small tilts can change apparent yaw
   - Level device when taking measurements

**Test**:
```
Before calibration: Yaw = 0° → 45° → 200° (jumps around)
After calibration:  Yaw = 247° (stable, small variations <2°)
                    ↑ This is correct behavior!
```

---

## Hardware & Power Questions

### Q: Can I move the sensors around?

**A:** 
- **BNO085 (IMU)**: Don't move once calibrated (affects mounting offset)
- **GPS antenna**: Can move, but ensure clear sky view
- **Best practice**: Mount sensors rigidly to same frame

If you move the IMU after calibration:
- Orientation is relative to new mounting
- You may want to recalibrate if changed significantly
- Small tilts (<5°) generally don't require recalibration

---

### Q: How long do the batteries last?

**A:** System current draw: ~300 mA total

**Battery life examples**:
```
Battery Type          Capacity      Estimated Life
─────────────────────────────────────────────────
USB power bank        5000 mAh      ~16-17 hours
USB power bank        10000 mAh     ~33 hours
Alkaline AA (2×)      2000 mAh      ~6-7 hours
Rechargeable Li-Ion   5000 mAh      ~16-17 hours
Wall outlet (USB)     Unlimited     Unlimited
```

**Power consumption by component**:
- BNO085: 50 mA
- NEO-M9N GPS: 150 mA
- Arduino: 100 mA
- **Total: 300 mA**

---

### Q: Can I use a lower voltage (3.3V)?

**A:** Not recommended, but possible:

- **BNO085**: Works on 3.3V but needs 5V for UART mode
- **GPS**: Designed for 5V operation
- **Arduino**: Runs on 3.3V but all development assumed 5V
- **Recommendation**: Stick with 5V for compatibility

If you must use 3.3V:
- Switch BNO085 to I2C mode (not UART)
- Use 3.3V regulator for GPS
- Verify logic levels match your platform

---

### Q: What if the power supply is unstable?

**A:** Symptoms of power issues:
- Arduino resets unexpectedly
- Data stream drops or corrupts
- Calibration behaves oddly
- GPS signal drops frequently

**Fixes**:
1. Upgrade power supply to higher capacity (1-2A)
2. Add capacitor (100-470μF) near Arduino 5V input
3. Use quality USB cable (short, thick)
4. Check for loose connections creating resistance
5. Measure 5V rail with multimeter—should be 4.8-5.2V under load

---

### Q: Can I use the system wirelessly (Bluetooth/WiFi)?

**A:** Currently not supported, but possible:

**Options**:
1. **Bluetooth module** (HC-05, ~$5):
   - Add to Serial1 (after BNO085)
   - Requires software modification
   - Range: ~10 meters indoors

2. **WiFi variant** (ESP32, ~$15):
   - Use ESP32 instead of Arduino Mega
   - Includes WiFi and Bluetooth
   - Lower power than USB solution

3. **USB to serial wireless bridge**:
   - Bluetooth USB dongle on host computer
   - Works with existing system

**Recommendation**: For now, stick with wired USB connection (more reliable, easier to debug).

---

## GPS Questions

### Q: How long until GPS lock?

**A:** Depends on acquisition type:

| Scenario | Time | Notes |
|----------|------|-------|
| **First fix ever (cold start)** | 60-120 sec | No almanac/ephemeris data |
| **After power off (warm start)** | 10-60 sec | Has previous almanac data |
| **Repeat locks at same location** | 5-15 sec | Fastest acquisition |
| **Degraded signal (cloudy)** | 120+ sec | Needs more satellites |

**Factors that affect lock time**:
- Antenna placement (higher = faster)
- Sky view quality (open = faster)
- Weather (clear > cloudy > heavy rain)
- Time since last power-on (fresh = slower)

---

### Q: GPS says "No Fix" or keeps searching. Why?

**A:** GPS unable to acquire satellites.

**Quick fixes** (try in order):

1. **Move outdoors**:
   - GPS doesn't work indoors (roof/walls block signals)
   - Move to open area (parking lot, rooftop, field)

2. **Check antenna**:
   - Is antenna physically connected (screwed on)?
   - Is antenna pointing up (vertical)?
   - Try 1-2 meter relocation

3. **Wait longer**:
   - Minimum 30 seconds on first acquisition
   - Try 60+ seconds if location is new

4. **Verify power**:
   - Check 5V power to GPS module
   - GPS is power-hungry (150 mA)
   - Unstable power = no fix

5. **Clear obstruction**:
   - No dense foliage directly overhead
   - No buildings blocking sky view
   - Need 5+ degrees above horizon to all directions

6. **Check cable**:
   - USB cable properly seated
   - No visible damage to cable
   - Try different USB cable

---

### Q: GPS signal is poor (high CEP). How do I improve it?

**A:** CEP (Circular Error Probable) shows accuracy.

| CEP Value | Signal Quality | Action |
|-----------|----------------|--------|
| <1 m | Excellent | All good, use confidently |
| 1-3 m | Good | Normal operation, acceptable |
| 3-5 m | Fair | Degraded, try moving |
| 5-10 m | Poor | Bad signal, definitely move |
| >10 m | Very Poor | No usable signal |

**Improve signal quality**:

1. **Move to clear location**:
   - Open field > parking lot > rooftop > street > indoors
   - Worst: dense forest, urban canyon, underground

2. **Higher antenna placement**:
   - Each additional meter of height = better signal
   - Rooftop > window > ground level

3. **Antenna orientation**:
   - Point antenna upward (vertical is best)
   - Avoid pointing horizontally

4. **Averaging multiple fixes**:
   ```python
   # Average 10 consecutive GPS fixes
   positions = [data['lat'], data['lon'], data['alt']]
   averaged = sum(positions) / len(positions)
   # Improves accuracy by ~3x
   ```

---

## Troubleshooting Questions

### Q: I see "BNO085 FAILED" on startup. What's wrong?

**A:** IMU sensor not responding. Check in this order:

**Most common issue (80% of cases):**
1. **P1 pin not set to 5V**:
   - P1 must be HIGH (connected to 5V)
   - P1 = GND means IMU mode, not UART
   - Verify with multimeter: should read 5V

**Other causes**:
2. Check UART wiring:
   - TX (BNO085) → RX1 (Pin 19, Arduino)
   - RX (BNO085) → TX1 (Pin 18, Arduino)
   - Verify connections are secure

3. Check power:
   - 5V supply stable (use multimeter)
   - No shorts between VCC and GND
   - GND connections secure

4. Check firmware:
   ```bash
   platformio run --target clean
   platformio run
   platformio run --target upload
   ```

---

### Q: Serial monitor shows garbled characters. Why?

**A:** Baud rate mismatch or signal corruption.

**Baud rate (most common)**:
```bash
# Default is 115200
python3 tools/real_time_monitor.py /dev/ttyACM0

# Try 9600 if above doesn't work
python3 tools/real_time_monitor.py /dev/ttyACM0 --baud 9600
```

**Signal corruption**:
- Check USB cable quality
- Verify connections are tight
- Try shorter cable
- Add ferrite clamp to USB cable
- Check for RF interference

---

### Q: Monitor says "High error count". What's happening?

**A:** Receiving corrupted serial packets.

**Causes**:
1. USB cable quality (check length/shielding)
2. Baud rate mismatch (verify platformio.ini)
3. Power supply noise (unstable voltage)
4. RF interference (move away from electronics)

**Fixes**:
```bash
# Check current port
ls /dev/tty*

# Try lower baud rate
python3 tools/real_time_monitor.py /dev/ttyACM0 --baud 9600

# Try without USB hub
# (direct connection to computer USB port)

# Replace USB cable
```

---

### Q: Data logging file is empty or incomplete. Why?

**A:** File not written or disk full.

**Checks**:
```bash
# 1. Verify disk space
df -h

# 2. Check file exists and has data
ls -lh field_data.jsonl
wc -l field_data.jsonl  # Line count

# 3. Verify file format
head field_data.jsonl
tail field_data.jsonl

# 4. Check file permissions
file field_data.jsonl
```

**Fixes**:
1. Ensure disk has space (at least 100 MB)
2. Create log directory explicitly:
   ```bash
   mkdir -p ~/logs
   python3 tools/real_time_monitor.py /dev/ttyACM0 --log ~/logs/data.jsonl
   ```
3. Check for permission errors
4. Try absolute path instead of relative

---

## Sensor Integration Questions

### Q: Can I add another sensor?

**A:** Yes, but requires code modification.

**Steps**:
1. Add sensor library to `platformio.ini`:
   ```ini
   lib_deps =
       Adafruit Unified Sensor
       Adafruit BME280  # Example: temperature sensor
   ```

2. Add sensor initialization in `src/main.cpp`:
   ```cpp
   Adafruit_BME280 bme;  // Temperature/pressure sensor
   
   void setup() {
       if (!bme.begin(0x77)) {
           Serial.println("BME280 FAILED");
       }
   }
   ```

3. Read sensor data and output to JSON:
   ```cpp
   float temp = bme.readTemperature();
   // Add to output JSON: "temp": temp
   ```

See [Adding New Sensors Guide](guides/ADDING_NEW_SENSORS.md) for detailed instructions.

---

### Q: Can I use a different IMU (MPU6050, ICM-20689)?

**A:** Current system designed for BNO085, but possible:

**Challenges**:
- BNO085 outputs quaternions (rotation-ready)
- Other IMUs output raw acceleration/rotation rates
- Need to fuse data (more complex)
- No magnetometer = no absolute heading (unless external compass)

**Options**:
1. **Add external compass** (magnetometer):
   - Use QMC5883L or HMC5883L (~$5)
   - Fuse with 6-axis IMU for 9-axis data
   - Similar to BNO085 but more work

2. **Accept relative orientation**:
   - Use MPU6050 alone for roll/pitch only
   - No absolute heading (yaw drifts)
   - Useful for tilt measurement

3. **Use BNO085** (recommended):
   - All-in-one solution
   - Requires no fusion algorithms
   - Well-tested and supported

---

### Q: What about IMU drifts when GPS is unavailable?

**A:** 

**IMU (BNO085)**:
- Uses gyroscope, accelerometer, magnetometer
- Magnetometer provides absolute reference
- **Yaw doesn't drift** (locked to magnetic field)
- Drift is minimal without external reference

**GPS**:
- Not needed for orientation accuracy
- Only needed for position (latitude/longitude)
- Works perfectly indoors without GPS

**No GPS scenario**:
- Orientation (pitch/roll/yaw): Works perfectly
- Position (lat/lon): Not available
- Duration: Works indefinitely indoors

---

## Data & Analysis Questions

### Q: What format is the data in?

**A:** JSON lines format (one JSON per line):

```json
{"timestamp": 1234567890.123, "roll": 2.3, "pitch": -5.4, "yaw": 247.8, "lat": 37.4419, "lon": -122.1430, "alt": 150.5, "cep": 2.3}
{"timestamp": 1234567890.124, "roll": 2.4, "pitch": -5.3, "yaw": 247.9, "lat": 37.4419, "lon": -122.1430, "alt": 150.5, "cep": 2.3}
```

**Fields**:
- `timestamp`: Unix timestamp (seconds since 1970-01-01)
- `roll`: Roll angle in degrees (-180 to +180)
- `pitch`: Pitch angle in degrees (-90 to +90)
- `yaw`: Yaw/heading angle in degrees (0 to 360)
- `lat`: Latitude in decimal degrees
- `lon`: Longitude in decimal degrees
- `alt`: Altitude in meters
- `cep`: GPS accuracy (Circular Error Probable) in meters

---

### Q: How do I export data to CSV or Excel?

**A:** Convert JSONL to CSV:

```bash
python3 << 'EOF'
import json
import csv

# Read JSONL
with open('sensor_data.jsonl', 'r') as f:
    data = [json.loads(line) for line in f]

# Write CSV
with open('sensor_data.csv', 'w', newline='') as f:
    writer = csv.DictWriter(f, fieldnames=data[0].keys())
    writer.writeheader()
    writer.writerows(data)

print(f"Exported {len(data)} rows to CSV")
EOF
```

Now open `sensor_data.csv` in Excel/Sheets.

---

### Q: How do I plot the data?

**A:** Use Python matplotlib:

```python
import json
import matplotlib.pyplot as plt

# Read data
with open('sensor_data.jsonl', 'r') as f:
    data = [json.loads(line) for line in f]

# Extract values
yaw = [d['yaw'] for d in data]
time = [d['timestamp'] for d in data]

# Plot
plt.figure(figsize=(12, 4))
plt.plot(time, yaw)
plt.xlabel('Time (seconds)')
plt.ylabel('Yaw (degrees)')
plt.title('Orientation Over Time')
plt.grid(True)
plt.savefig('yaw_plot.png')
```

---

## Quick Links

- **[Quick Start Guide](guides/QUICK_START_GETTING_STARTED.md)** — Get started in 5 minutes
- **[First Calibration Guide](guides/FIRST_CALIBRATION.md)** — Calibrate your sensor
- **[Real-Time Monitoring Guide](guides/MONITORING_REAL_TIME_DATA.md)** — Monitor and log data
- **[Field Deployment Guide](guides/FIELD_DEPLOYMENT.md)** — Deploy in the field
- **[Hardware Setup Guide](guides/HARDWARE_SETUP.md)** — Wiring and connections
- **[Calibration Guide](guides/CALIBRATION_GUIDE.md)** — Detailed calibration reference
- **[Adding New Sensors](guides/ADDING_NEW_SENSORS.md)** — Extend with new sensors

---

## Still Have Questions?

1. **Check the guides**: Most answers are in the [Quick Start](guides/QUICK_START_GETTING_STARTED.md)
2. **Search this FAQ**: Use Ctrl+F to search for keywords
3. **Check [Architecture](ARCHITECTURE.md)**: System design and diagrams
4. **Review examples**: Check `tools/EXAMPLES.md` for usage patterns
5. **Read findings**: See `docs/findings/` for research notes

---

**Last Updated**: 2025-05  
**Total Questions**: 50+  
**Most Common Questions**: Covered in Quick Start Guide
