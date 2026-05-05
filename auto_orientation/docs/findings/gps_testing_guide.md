# GPS Testing Quick Guide

## Prerequisites

### 1. User Permissions (One-time Setup)
```bash
# Add your user to dialout group for serial port access
sudo usermod -aG dialout devel

# Apply changes (either log out/in or use newgrp)
newgrp dialout
id  # verify dialout group now appears
```

### 2. Verify Hardware Connection
```bash
# List available serial devices
ls -la /dev/ttyACM*

# Expected: /dev/ttyACM0 and/or /dev/ttyACM1 (NEO-M9N on /dev/ttyACM1)
```

---

## Testing Workflow

### Step 1: Capture Raw NMEA Output
```bash
cd /home/devel/floppi/auto_orientation

# Capture 60 seconds of NMEA data from GPS
python3 tools/serial_monitor.py /dev/ttyACM1 --baud 115200 --wait 60 \
  --output test_real_gps.txt

# Expected output:
# - Connected: /dev/ttyACM1 @ 115200 baud
# - Real-time NMEA sentences displayed
# - File saved: test_real_gps.txt
```

### Step 2: Parse NMEA Output
```bash
# Parse the captured data (use test file or sample data)
python3 tools/test_nmea_parser.py test_real_gps.txt -o test_gps_parsed.csv -v

# Output shows:
# - Total sentences parsed
# - GPGGA fix statistics (lat/lon/altitude/HDOP)
# - GPRMC navigation data (speed/course)
# - All parsed fixes with details
```

### Step 3: Analyze Results
```bash
# View CSV results in spreadsheet or command line
cat test_gps_parsed.csv | column -t -s,

# Or open in Excel/LibreOffice for visualization
```

---

## Expected Results

### GPS Lock Time
- **Cold Start**: 30-60 seconds (first fix)
- **Warm Start**: 5-10 seconds (fresh ephemeris)
- **Hot Start**: < 1 second (cached ephemeris)

### Position Accuracy
- **Nominal**: ±1 meter (95% CEP)
- **HDOP < 2.0**: Excellent geometry
- **HDOP 2.0-5.0**: Good geometry
- **HDOP > 5.0**: Poor geometry (< 4 satellites or bad visibility)

### Typical NMEA Output
```
GPRMC: Recommended Minimum Navigation Information (1 Hz)
  - Position, speed, course, date
  - Status field shows A (Active) when valid

GPGGA: Global Positioning System Fix Data (1 Hz)
  - Position, altitude, fix quality, accuracy
  - Fix quality 1 = GPS, 2 = DGPS, 4 = RTK
  - HDOP indicates horizontal accuracy

GPGSV: Satellites in View (5 Hz or less common)
  - Lists visible satellites and signal strength
  - Can be parsed but not required for basic navigation
```

---

## Troubleshooting

### Issue: Device Not Found
```
Error: [Errno 13] Permission denied: /dev/ttyACM1
```
**Solution**: User not in dialout group. Run:
```bash
sudo usermod -aG dialout $USER
newgrp dialout
```

### Issue: Port Busy
```
Error: Port busy! Try: python3 serial_monitor.py /dev/ttyACM1 --release
```
**Solution**: Another process is using the port. Release it:
```bash
python3 tools/serial_monitor.py /dev/ttyACM1 --release
```

### Issue: No GPS Lock (No Position Data)
```
Status = V (Void/Invalid) or Fix Quality = 0
```
**Causes**:
- Device just powered on (needs 30-60 seconds)
- Indoors or poor sky visibility
- Insufficient satellites (need ≥ 4)
- Antenna not connected properly

**Solution**:
- Move to open sky (away from buildings/trees)
- Wait 2-3 minutes for warm fix
- Check antenna connection
- Verify signal strength with GPGSV sentences

### Issue: Poor HDOP (HDOP > 5.0)
```
Horizontal Dilution of Precision > 5.0 meters
```
**Causes**:
- Limited satellite visibility
- Satellites clustered in one direction
- Reflections/multipath interference

**Solution**:
- Move to better sky visibility
- Wait for satellite geometry to improve (15-30 min)
- Reduce reflective surfaces nearby

---

## Sample NMEA Sentences Explained

### GPGGA Example
```
$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*42
        ^^^^^^  ^^^^^^^^^    ^^^^^^^^^        ^^    ^^^^        ^^^^
        time    latitude    longitude    fix sats HDOP  altitude

- Time: 12:35:19 UTC
- Position: 48°07'02.28"N, 11°31'00.00"E (Munich area)
- Fix Quality: 1 = GPS Fix (good)
- Satellites: 8 (solid constellation)
- HDOP: 0.9m (excellent precision)
- Altitude: 545.4m above sea level
- Geoid: 46.9m (WGS84 ellipsoid offset)
```

### GPRMC Example
```
$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*7B
        ^^^^^^  ^ ^^^^^^^^^    ^^^^^^^^^  ^^^^^  ^^^^^  ^^^^^^
        time   status position           speed course date

- Time: 12:35:19 UTC
- Status: A = Active (valid fix)
- Position: 48°07'02.28"N, 11°31'00.00"E
- Speed: 22.4 knots (~41.5 km/h)
- Course: 084.4° (East-Southeast)
- Date: 23 Mar 1994 (example data)
- Magnetic Variation: 3.1° West
```

---

## Advanced Testing

### Multi-Sample Accuracy (Stationary Test)
```bash
# Capture 5 minutes of stationary fixes
python3 tools/serial_monitor.py /dev/ttyACM1 --wait 300 \
  --output stationary_test.txt

# Parse and analyze position variation
python3 tools/test_nmea_parser.py stationary_test.txt -o results.csv -v

# Examine CSV for:
# - Latitude range: should be < 0.0001° (~10 meters)
# - Longitude range: should be < 0.0001° (~10 meters)
# - Average position for truth reference
# - HDOP trend (should stabilize < 2.0)
```

### Speed/Course Validation (Moving Test)
```bash
# Capture data while moving at constant speed
python3 tools/serial_monitor.py /dev/ttyACM1 --wait 120 \
  --output moving_test.txt

# Parse and check:
# - Speed consistency (should vary < ±10%)
# - Course stability (should vary < ±5°)
# - Position drift in direction of motion
```

### Cold Start Characterization
```bash
# Power cycle GPS and time to first fix
python3 tools/serial_monitor.py /dev/ttyACM1 --wait-for "Fix=GPS" \
  --timeout 120

# Note: Requires custom parser for this check
# Alternative: monitor for transition from Fix=0 to Fix=1
```

---

## Files Reference

| File | Purpose |
|------|---------|
| `tools/serial_monitor.py` | Serial capture utility (supports TTY + USB CDC) |
| `tools/test_nmea_parser.py` | NMEA parser with CSV export |
| `tools/gps_test_output.txt` | Sample NMEA data (20 sentences, Munich coords) |
| `tools/gps_parsed_data.csv` | Parsed sample data (10 fixes + 10 nav records) |
| `docs/findings/gps_nmea_verification.md` | Complete NMEA specification and test results |
| `src/sensors/neo_m9n.h` | C++ sensor class interface for integration |

---

## Next Steps for Integration

1. **Capture Real Data**: Run serial_monitor.py with actual NEO-M9N
2. **Validate Parser**: Compare parsed output with NMEA spec
3. **Implement C++ Version**: Translate parser logic to neo_m9n.cpp
4. **Test Sensor Fusion**: Combine GPS + BNO085 orientation data
5. **Add Accuracy Analysis**: Implement multi-sample averaging per gps-accuracy-improvement.md

---

## Quick Command Reference

```bash
# Everything in one command
python3 tools/serial_monitor.py /dev/ttyACM1 --wait 60 --output test.txt && \
python3 tools/test_nmea_parser.py test.txt -o results.csv -v

# Or step by step
python3 tools/serial_monitor.py /dev/ttyACM1 --wait 30 -o test.txt
python3 tools/test_nmea_parser.py test.txt -o results.csv -v
cat results.csv | column -t -s,
```

---

**Ready to test!** 🛰️
