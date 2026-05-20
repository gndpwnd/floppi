# BNO085 Quick Reference Guide

**Fast answers to common questions about calibration and usage.**

---

## 🚀 Quick Start (5 minutes)

### Upload Firmware
```bash
cd /home/devel/floppi/auto_orientation
platformio run -e arduino_mega -t upload
```

### Calibrate
```bash
python3 tools/bno_calibrate.py /dev/ttyACM1 --mode calibrate
```
Follow on-screen instructions. Move board in large figure-8 patterns for 5-10 minutes until you reach level 3.

### Monitor Orientation
```bash
python3 tools/bno_calibrate.py /dev/ttyACM1 --mode monitor
```
See real-time quaternions and Euler angles (roll/pitch/yaw).

---

## 🔧 Hardware Setup

```
Arduino Mega → BNO085
├─ SDA (Pin 20) → SDA (white/blue wire)
├─ SCL (Pin 21) → SCL (yellow/green wire)
├─ GND → GND (black wire)
└─ 3.3V → 3.3V (red wire)
```

**Important:** DI pin on BNO085 can be left floating (not connected to GND).

---

## 📊 What You Get

### Calibration Levels
```
Level 0 (░░░): Uncalibrated
Level 1 (█░░): Low calibration
Level 2 (██░): Medium ← Good enough for most uses, auto-saves
Level 3 (███): High ← Best accuracy, our goal
```

### Output Format (JSON)
```json
{
  "timestamp": 12345,
  "orientation": {
    "w": 0.707,
    "x": 0.000,
    "y": 0.000,
    "z": 0.707,
    "magnitude": 1.000000,
    "euler": {
      "roll_deg": 0.0,
      "pitch_deg": 0.0,
      "yaw_deg": 90.0
    },
    "calibration": {
      "system": 3,
      "accel": 3,
      "gyro": 3,
      "mag": 3
    }
  }
}
```

### Quaternion (w, x, y, z)
- Represents 3D rotation in a 4-number format
- Always normalized: `w² + x² + y² + z² = 1`
- Stable and unambiguous (no gimbal lock)

### Euler Angles (Roll, Pitch, Yaw)
- **Roll:** Rotation around X-axis (-180 to 180°)
- **Pitch:** Rotation around Y-axis (-90 to 90°)
- **Yaw:** Rotation around Z-axis (-180 to 180°)
  - Referenced to **magnetic north** (requires calibration!)

---

## ⚙️ Calibration Details

### Why Calibration Matters
The magnetometer can't tell "north" if there's magnetic interference:
- Power supplies
- Steel beams
- Nearby motors
- Your electronics

Calibration teaches the sensor how to subtract this interference.

### What We're Calibrating
- **Accelerometer:** Usually stays calibrated (gravity is always the same)
- **Gyroscope:** Usually stays calibrated (rotation is rotation)
- **Magnetometer:** NEEDS RECALIBRATION in new locations
  - Hard iron effects (permanent magnets, power supplies)
  - Soft iron effects (ferromagnetic materials nearby)

### Calibration Storage
- Saved to EEPROM (permanent memory on Arduino)
- Survives power cycles
- Survives code uploads
- Erased only by explicit clear command

### How Long Does It Take?
- Level 1: 30 seconds (light motion)
- Level 2: 1-2 minutes (moderate figure-8)
- Level 3: 5-10 minutes (aggressive figure-8, all axes)

### What If Calibration Gets Stuck?
**Level stays at 0 or 1:**
- Move the board BIGGER and SLOWER
- Make sure all 3 axes rotate (don't just rock it)
- Keep going for at least 2-3 minutes continuously
- Check that board moves in all directions

**Level 2 won't go to 3:**
- Level 2 is "good enough" for most applications
- Level 3 is difficult (requires perfect motion)
- If you need it, try: slow large circles, then figure-8, repeat

**Lost calibration after moving:**
- Magnetic environment changed significantly
- Just re-calibrate in the new location (usually faster the 2nd time)

---

## 🧭 Understanding Absolute Orientation

### What "Absolute" Means
Not just "which way is up?" but "which way exactly?"

**References:**
- **Up/Down:** Gravity (accelerometer) - never changes
- **North/South:** Magnetic north (magnetometer) - changes with location
- **East/West:** Perpendicular (derived)

### Why It Works
1. Accelerometer measures gravity → Know which way is DOWN
2. This gives you PITCH (forward/back tilt) and ROLL (side tilt)
3. Magnetometer measures magnetic field → Know which way is NORTH
4. This gives you YAW (which direction facing)
5. Gyroscope smooths everything out as you move

### What Breaks It
- **Without calibration:** Magnetometer can't find north → yaw is wrong by 30-90°
- **Moving too fast:** Accelerometer can't separate gravity from motion
- **Near magnets:** Local magnetic field overpowers Earth's field

---

## 📝 Typical Workflow

### First Time Setup
```
1. Upload firmware
2. Connect via USB
3. Run calibration tool
4. Move board in figure-8 for 5-10 minutes
5. See "✓✓✓ Calibration saved to EEPROM"
6. Unplug and replug Arduino
7. See "✓ Calibration restored successfully"
8. DONE - ready to use
```

### Subsequent Uses
```
1. Connect via USB
2. Calibration auto-loads (no motion needed)
3. System ready immediately
4. Run monitor tool to see data
```

### After Moving to New Location
```
Option A: Same environment → no recalibration needed
Option B: Very different (different building, outdoors vs indoors)
         → System may warn you
         → Quick recalibration (5-10 min) gets it right
```

---

## 🔍 Debugging

### Check Initialization
```bash
# Firmware compiles?
platformio run -e arduino_mega

# Firmware uploads?
platformio run -e arduino_mega -t upload

# BNO085 responds?
timeout 8 python3 -c "
import serial
s = serial.Serial('/dev/ttyACM1', 115200, timeout=1)
import time; time.sleep(2)
for _ in range(20):
    if s.in_waiting:
        print(s.readline().decode())
s.close()
"
```

### Check I2C Connection
```bash
# BNO085 on I2C bus?
# (Need to upload an I2C scanner sketch)
# Look for address 0x4A or 0x4B
```

### Check Calibration
```bash
# Look for these messages:
# "✓ Calibration restored successfully" → data loaded from EEPROM
# "✓✓✓ Calibration saved to EEPROM" → new calibration saved
# "ℹ No saved calibration" → first boot, need to calibrate
```

---

## 🎓 Understanding the Algorithm

See: [../theory/BNO085_ALGORITHM_AND_REPLICATION.md](../theory/BNO085_ALGORITHM_AND_REPLICATION.md)

**Quick version:**
- BNO085 has a small computer inside running CEVA's MotionEngine firmware
- Reads 3 sensors at 10 kHz, fuses them at 200 Hz internally
- You get the quaternion output at your requested rate (10 Hz default)
- Calibration makes the magnetometer readings reliable

---

## 💡 Pro Tips

1. **Always move in figure-8:** Covers all rotation axes at once
2. **Big slow movements:** Better than fast jerky ones
3. **Keep moving until auto-save:** Don't stop at level 2, go for level 3
4. **Check EEPROM message:** "Calibration saved" = success
5. **Power cycle to verify:** Unplug, wait 10s, plug back in
   - Should see "Calibration restored" immediately
6. **Monitor yaw:** If yaw drifts, calibration might be weak
7. **Check saturation:** Quaternion magnitude should always be ≈1.0

---

## 🚀 Next Steps

1. **Get BNO085 to production:** ✅ Done (you have it)
2. **Research MPU6050 replication:** See algorithm document
3. **Implement Madgwick filter:** Start here
4. **Compare outputs:** Run both simultaneously
5. **Document findings:** Real numbers on accuracy vs cost

---

**Questions?** See READY_TO_USE.md or BNO085_ALGORITHM_AND_REPLICATION.md
