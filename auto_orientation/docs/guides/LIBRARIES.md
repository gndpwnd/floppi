# External Libraries for auto_orientation

This document lists all non-Arduino.h dependencies that must be cloned locally for field deployment (no cloud IDE dependency).

## Required Libraries (v1.0)

### 1. Adafruit BNO08x Library
**Purpose**: BNO085 absolute orientation sensor  
**Repository**: https://github.com/adafruit/Adafruit_BNO08x_Arduino  
**Status**: Required for BNO085 support  
**Clone to**: `lib/Adafruit_BNO08x_Arduino/`

```bash
cd lib && git clone https://github.com/adafruit/Adafruit_BNO08x_Arduino.git
```

**Key Files**:
- `Adafruit_BNO08x.h` — Main sensor class
- `Adafruit_BNO08x.cpp` — Implementation

**Calibration API** (TO BE INVESTIGATED):
- Need to investigate: Does this library expose calibration save/restore methods?
- May need to access lower-level I2C/UART functions

---

### 2. Ublox NEO-M9N NMEA Parser (if needed)
**Purpose**: GPS module NMEA sentence parsing  
**Options**:
- **Option A**: Implement NMEA parsing in-house (lightweight, no dependency)
- **Option B**: Use external NMEA library (e.g., https://github.com/RandyMcMillan/nmea)

**Status**: TBD (research phase will determine)  
**Clone to**: `lib/nmea/` (if external library chosen)

---

### 3. ArduinoJSON (Optional, for output formatting)
**Purpose**: JSON output formatting  
**Repository**: https://github.com/bblanchon/ArduinoJson  
**Status**: Optional (can use simpler delimited output initially)  
**Clone to**: `lib/ArduinoJson/`

```bash
cd lib && git clone https://github.com/bblanchon/ArduinoJson.git
```

---

## Optional Libraries (Future)

### SD Card Support (v1.1+)
- **SD.h** — Usually built-in; may need SdFat for advanced features
- https://github.com/greiman/SdFat

### Custom Sensor Fusion (v1.1+, if needed)
- **Madgwick Filter** — http://x-io.co.uk/open-source-imu-and-ahrs-algorithms/
- Only if MPU 6050 support needed without BNO085

---

## Cloning Strategy for Field Deployment

```bash
# From project root:
mkdir -p lib
cd lib

# Core required
git clone https://github.com/adafruit/Adafruit_BNO08x_Arduino.git

# Optional (only if needed)
# git clone https://github.com/bblanchon/ArduinoJson.git
# git clone https://github.com/RandyMcMillan/nmea.git
```

**PlatformIO will auto-discover libraries in `lib/`** and include them in builds.

---

## Built-in Arduino Libraries Used

These come with Arduino.h and don't need cloning:
- `Wire.h` — I2C communication (for BNO085)
- `Serial.h` — USB/UART output
- `EEPROM.h` — (if using local storage fallback)

---

## Library Verification Checklist

- [ ] Adafruit BNO08x cloned to lib/
- [ ] Verify PlatformIO builds successfully with library
- [ ] Test BNO085 initialization and quaternion output
- [ ] Document any API differences or quirks in findings/

---

## Notes

- Keep local library clones in `.gitignore` with exception for .git metadata
- Use specific commit hashes for reproducibility if needed
- Document any library version-specific issues in findings/

