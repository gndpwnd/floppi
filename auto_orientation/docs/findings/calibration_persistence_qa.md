# BNO085 Calibration Persistence: Key Questions & Answers

## Q1: Does the sensor work if powered on in a different orientation than calibration?

### Answer: **YES, absolutely!**

**Why:**
- The BNO085 calibration stores **magnetic offsets and gyro biases**, not orientation state
- The magnetometer calibration accounts for magnetic disturbances in your LOCAL environment (hard-iron/soft-iron effects)
- Once calibrated, these offsets work regardless of current orientation
- The sensor uses **real-time sensor fusion** to determine orientation from gravity + magnetic field

**How it works:**
1. **Gravity vector (always down)**: Accelerometer reads this regardless of orientation → tells pitch & roll
2. **Magnetic north (always same direction)**: Magnetometer reads this → tells yaw
3. **Gyroscope**: Helps smooth the estimates

The calibration ensures the magnetometer reading is accurate in YOUR environment, and that's independent of which way the device is pointing.

### Example:
- Calibrate in office with device lying flat
- Turn off, rotate device 180°, place on shelf upside down
- Turn on → still works perfectly!
- The stored calibration (offsets) applies to the new orientation

### What DOES change with orientation:
- Quaternion values (w, x, y, z) change because orientation changed
- Calibration STATUS (0-3) stays the same
- This is correct behavior!

---

## Q2: Does calibration survive when you upload a new sketch?

### Answer: **YES! EEPROM is independent of firmware**

**Why:**
- Arduino EEPROM is **non-volatile memory separate from program flash**
- Uploading new code only overwrites the program flash (first 250KB on Mega)
- EEPROM remains untouched (stored at bytes 0x0000-0x0FFF on Mega)
- Calibration data stored in EEPROM bytes 0x00-0xFF survives uploads

### Analogy:
- **Flash = Application installed on hard drive** (gets replaced with new version)
- **EEPROM = User's saved documents** (persist across app updates)

### Process on upload:
1. `platformio run -t upload` → erases and rewrites flash with new sketch
2. EEPROM unchanged → calibration still there
3. New sketch boots → BNO085 driver checks EEPROM → loads saved calibration
4. No re-calibration needed!

### Important caveat:
- **Only if the new sketch uses the same calibration storage location!**
- Location is defined as: `CAL_EEPROM_BASE 0x00` (first 256 bytes)
- If you move the storage location or change the format version, calibration won't load

---

## Q3: What exactly is stored in the calibration data?

**Calibration contains:**

1. **Accelerometer offsets** (3 floats) - Account for non-zero reading at rest
2. **Gyroscope biases** (3 floats) - Account for drift when stationary
3. **Magnetometer calibration** (hard-iron + soft-iron effects):
   - Hard-iron: Magnetic sources near the device (power supplies, motors, metal)
   - Soft-iron: Magnetic distortion from materials (steel, ferromagnetic objects)
   - Stored as: 3D offset vector + 3×3 scale matrix (~36-72 bytes total)
4. **System metadata** - Status flags, version info

**NOT stored:**
- Current orientation (this is **real-time**, not calibration)
- Timestamp or location
- Previous motion history

---

## Q4: How long does calibration persist?

**In EEPROM:**
- Should last **100+ years** (typical EEPROM spec: 100,000 write cycles)
- Survives:
  - ✅ Power cycles (unplugging, turning off)
  - ✅ Firmware updates (code uploads)
  - ✅ Orientation changes
  - ✅ Time passing
  - ✅ Moving to a different room (but may need re-calibration if magnetic environment changes drastically)

**When calibration becomes invalid:**
- ❌ Moving to a location with very different magnetic field (different continent, industrial area, etc.)
- ❌ Placing near new magnetic sources (magnet, power supply, motor)
- ❌ Physical damage to sensors (extremely rare)

---

## Q5: Can I manually trigger re-calibration if needed?

**Yes!** The `clearCalibrationFromEEPROM()` function exists:

```cpp
// Force re-calibration next boot
clearCalibrationFromEEPROM();
```

This clears the saved calibration so the sensor will require fresh calibration motion on next startup.

**Scenarios for re-calibration:**
- Moving to a new location with different magnetic environment
- Sensor near new magnetic sources (electromagnet, subwoofer, etc.)
- Suspecting calibration got corrupted

---

## Q6: What if the EEPROM gets corrupted?

**Safety built in:**

1. **Validity marker** (0xCA = calibration valid, 0xFF = empty)
2. **CRC8 checksum** - detects single-bit corruption
3. **Version field** - handles firmware compatibility

**If corruption detected:**
- `restoreFromEEPROM()` returns false
- Code skips restoration
- Sensor boots uncalibrated
- You re-calibrate (no data loss, just requires motion again)

---

## Implementation Summary

| Aspect | Behavior |
|--------|----------|
| **Survives power cycles** | ✅ Yes (EEPROM is non-volatile) |
| **Survives code uploads** | ✅ Yes (uploads only touch flash, not EEPROM) |
| **Works in different orientation** | ✅ Yes (calibration is direction-independent) |
| **Required storage** | 256 bytes EEPROM (Mega has 4096, so no conflict) |
| **Typical lifespan** | 100+ years in EEPROM |
| **Auto-save on level 2+** | ✅ Yes (triggers after ~2-5 minutes motion) |
| **Manual clear possible** | ✅ Yes (call `clearCalibrationFromEEPROM()`) |

---

## What This Means For Your Workflow

1. **First boot**: No calibration stored
   - Device boots → checks EEPROM → finds nothing
   - Serial says "No saved calibration"
   - Do 1-2 min figure-8 motion
   - When level 2 reached → auto-saved to EEPROM

2. **Subsequent boots**: Calibration restored
   - Device boots → checks EEPROM → finds calibration
   - Serial says "Calibration restored successfully!"
   - Sensor ready to use immediately
   - No motion needed

3. **Code changes**: Calibration persists
   - Edit source, upload new sketch
   - EEPROM untouched
   - Calibration auto-loads
   - Sensor ready immediately

4. **Moving to new location**: Usually works
   - Magnetic environment similar → works
   - Drastically different (opposite hemisphere, industrial facility) → may need re-calibration
   - Can force clear with button/menu if needed

---

## Technical Notes

**EEPROM Layout Used:**
```
Offset  Size    Content
0x00    1 byte  Validity marker (0xCA = valid)
0x01    1 byte  Data length (1-252)
0x02    1 byte  Format version (0x01)
0x03    1 byte  CRC8 checksum
0x04    252     Calibration payload
---
Total: 256 bytes (Mega has 4096, so this is 6% usage)
```

**Timing Notes:**
- Reading calibration from sensor: ~100-500ms
- Writing to EEPROM: ~3.3ms per byte = ~850ms for full block
- Total auto-save time: ~1-2 seconds (blocks main loop)
- Throttled to every 5 seconds to avoid excessive saves

---

## Recommendation

✅ **Calibrate once, forget about it!**

Your workflow should be:
1. First power-on: Do 1-2 minutes of figure-8 motion
2. Wait for "Calibration level 2 saved" message
3. Power cycle, move, upload new code, etc. → calibration persists
4. Only re-calibrate if you move to a very different location or suspect corruption

This is why the persistent storage is so important—otherwise you'd have to recalibrate every single time!
