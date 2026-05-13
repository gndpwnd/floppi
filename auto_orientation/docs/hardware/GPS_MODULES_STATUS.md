# GPS Modules Status & Documentation

**Status:** ✅ **VERIFIED WORKING - READY TO PACK**

---

## Hardware Overview

### Modules
1. **NEO-M9N** (u-blox)
   - Port: `/dev/ttyACM0`
   - Status: ✅ Locked with 12+ satellites
   - HDOP: 0.75m (excellent)
   - Output: NMEA sentences (GPRMC, GPGGA, etc.)

2. **M8T** (u-blox)
   - Port: `/dev/ttyACM2`
   - Status: ✅ Locked with 12+ satellites
   - HDOP: 0.87m (excellent)
   - Output: NMEA sentences (GPRMC, GPGGA, etc.)

### Connection Type
**USB-based** - These are standalone GPS receivers that connect directly to your computer via USB, NOT to the Arduino.
- They appear as `/dev/ttyACM*` devices
- They output NMEA over serial at 115200 baud
- Arduino is NOT involved in GPS data collection
- GPS is completely independent from BNO085

---

## Current Implementation Status

### ✅ What Works
- Both modules detect satellites and lock
- Both output valid NMEA sentences
- Position accuracy: ±0.75-0.87m (excellent)
- Can read data directly from serial port

### ❌ What's NOT Implemented
- GPS code in Arduino firmware (not needed - USB devices)
- Integration with orientation system
- Merged JSON output with GPS + orientation
- Real-time position display on Arduino

### ⏳ What's Planned (Future)
- Integrate GPS position into output JSON
- Combine with orientation data at output layer
- Create unified sensor data stream

---

## Code & Documentation

### Code Files
**Main firmware:**
- `src/sensors/sensor_base.h` - Contains `PositionData` struct definition (lines 44-57)
  - latitude, longitude, altitude
  - accuracy_m, num_satellites, fix_quality
  - timestamp_ms

**Python tools:**
- `tools/test_nmea_parser.py` - Test script to read NMEA sentences from GPS

### Documentation Files

**Current (Useful):**
- `docs/findings/gps_lock_troubleshooting.md` - How to get satellites to lock
- `docs/findings/gps-accuracy-improvement.md` - Antenna positioning for better signal
- `docs/findings/session_status_gps_and_bno085_working.md` - Status from previous session

**Archived (Historical):**
- `docs/implementation/neo_m9n_driver_implementation.md` - Old notes on driver
- `docs/todo/gps_checklist.md` - Checklist from development

---

## How to Use GPS Modules

### Read Data Directly
```bash
# Check what's on the ports
lsusb | grep u-blox

# Read raw NMEA from NEO-M9N
timeout 5 cat /dev/ttyACM0 | head -20

# Read raw NMEA from M8T
timeout 5 cat /dev/ttyACM2 | head -20
```

### Parse NMEA Data
```bash
python3 tools/test_nmea_parser.py /dev/ttyACM0
```

### Monitor Continuously
```bash
# Watch GPS data from NEO-M9N
screen /dev/ttyACM0 115200
# Press CTRL+A then D to detach
```

---

## NMEA Sentence Format

**Example GPRMC (Recommended Minimum):**
```
$GNRMC,031721.60,A,6139.38898,N,14917.91248,W,0.055,,070526,,,D,V*06
         |       | |          |  |          | |                  | 
      time     status lat    N  lon        W speed             mode
```

**Example GNGGA (Global Positioning System Fix Data):**
```
$GNGGA,031721.60,6139.38898,N,14917.91248,W,2,12,1.30,205.3,M,10.5,M,,0000*51
         |       |          |  |          | | || |   |     |  |
      time     lat        N  lon        W q ns hdop alt unit geoid
```

**Fields:**
- `q`: Fix quality (2=DGPS, 1=GPS, 0=no fix)
- `ns`: Number of satellites
- `hdop`: Horizontal dilution of precision (lower is better)
- `alt`: Altitude above sea level
- `geoid`: Height of geoid above WGS84 ellipsoid

---

## Antenna Positioning (Critical for Lock)

**Current setup:** Outdoor location with clear sky view

**If not locking:**
1. Move antenna outdoors (away from buildings/metal)
2. Point antenna toward open sky
3. Wait 30-60 seconds for initial fix
4. Check HDOP (should be <2.0 for good signal)

**Current performance:**
- HDOP: 0.75-0.87m (⭐ excellent)
- Satellites: 12+ (⭐ excellent)
- Fix: Stable (⭐ excellent)

---

## When You Pack Them Up

### Before Storage
1. ✅ Verify both modules lock (done)
2. ✅ Note the performance specs (HDOP, satellite count) - done
3. ✅ Document any quirks or special setup - none found

### When You Use Them Again
1. Plug in via USB
2. Wait 30-60 seconds for initial lock
3. Read from `/dev/ttyACM0` or `/dev/ttyACM2`
4. Parse NMEA sentences as needed

### Integration (Future)
When ready to integrate with BNO085:
1. Read GPS position in parallel thread
2. Combine with orientation data
3. Output unified JSON with both streams

---

## Summary

**GPS modules are:**
- ✅ Hardware-verified working
- ✅ Positioned correctly (good HDOP)
- ✅ Ready to pack
- ✅ Documented
- ⏳ Not integrated with firmware (planned for future)

**You can:**
- Read them directly from any language that does serial I/O
- Run them in parallel with BNO085 (independent streams)
- Combine the data at application layer

**No additional work needed** before packing them away.

---

## Related Documentation

- **BNO085 absolute orientation:** docs/BNO085_ALGORITHM_AND_REPLICATION.md
- **Full system architecture:** docs/ARCHITECTURE.md
- **Sensor base classes:** src/sensors/sensor_base.h
- **Quick reference:** docs/BNO085_QUICK_REFERENCE.md
