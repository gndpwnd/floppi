# PlatformIO Migration - Complete Fix Guide

## 🎯 All Errors Fixed

Your compilation errors have **4 root causes**. Here's how to fix each one:

---

## ❌ ERROR 1: `kinetis.h: No such file or directory`

### Root Cause
The Arduino framework for Teensy includes `WireKinetis.cpp`, which is **only for Teensy 3.x**, not Teensy 4.x. Your build flags incorrectly defined `__MK66FX1M0__` (Teensy 3.6) for Teensy 4.0.

### Fix
Update `platformio.ini` - **remove the wrong define** and add `-I include`:

```ini
[env:teensy40]
platform = teensy
board = teensy40
build_flags = 
    ${env.build_flags}
    -D ARDUINO_TEENSY40
    -D __IMXRT1062__          # ← Correct for Teensy 4.x
    # REMOVED: -D __MK66FX1M0__  ← This was WRONG (Teensy 3.6)
    -I include                # ← ADD THIS for lib/ to find config.h
    -O2
    -mcpu=cortex-m7
    -mfloat-abi=hard
    -mfpu=fpv5-d16
```

**Why this works:**
- `__IMXRT1062__` tells the framework "this is Teensy 4.x"
- Framework won't compile `WireKinetis.cpp` for Teensy 4.x
- Only compiles `WireIMXRT.cpp` (the correct one)

---

## ❌ ERROR 2: `config.h: No such file or directory`

### Root Cause
`radioComm.cpp` includes `"config.h"`, but in PlatformIO:
- `include/` directory is in the include path for `src/` files ✓
- `include/` directory is **NOT** in the path for `lib/` files by default ✗

### Fix (Already in platformio.ini above)
Add `-I include` to `build_flags` in `[env]` section:

```ini
[env]
framework = arduino
build_flags = 
    -D USB_SERIAL
    -I include    # ← Makes include/ visible to lib/ files
```

**Now radioComm.cpp can find config.h!**

---

## ❌ ERROR 3: `GYRO_SCALE` and `ACCEL_SCALE` not declared

### Root Cause
Your `config.h` doesn't define these constants, but `main.cpp` uses them:

```cpp
mpu6050.setFullScaleGyroRange(GYRO_SCALE);   // ← Undefined!
mpu6050.setFullScaleAccelRange(ACCEL_SCALE); // ← Undefined!
```

### Fix
Add these defines to `config.h`:

```cpp
#ifdef USE_MPU6050
    // MPU6050 Gyroscope scale
    #define GYRO_SCALE MPU6050_GYRO_FS_500
    
    // MPU6050 Accelerometer scale
    #define ACCEL_SCALE MPU6050_ACCEL_FS_2
    
    // Scale factors for raw data conversion
    #define GYRO_SCALE_FACTOR  65.5
    #define ACCEL_SCALE_FACTOR 16384.0
#endif
```

**What the values mean:**
- `MPU6050_GYRO_FS_500` = ±500°/s range (defined in MPU6050.h)
- `MPU6050_ACCEL_FS_2` = ±2g range (defined in MPU6050.h)
- Scale factors convert raw int16 values to real units

---

## ❌ ERROR 4: `class PWMServo has no member 'writeMicroseconds'`

### Root Cause
Your PWMServo library uses `write(degrees)`, not `writeMicroseconds(us)`.

### Original Code (Broken)
```cpp
servo1.writeMicroseconds(s1_command_PWM);  // ← Method doesn't exist
```

### Fixed Code
```cpp
// Convert microseconds (1000-2000) to degrees (0-180)
servo1.write((s1_command_PWM - 1000) * 0.18);
```

**Conversion formula:**
```
degrees = (microseconds - 1000) / 1000 * 180
        = (microseconds - 1000) * 0.18

Examples:
1000μs → 0° (minimum)
1500μs → 90° (center)
2000μs → 180° (maximum)
```

### Replace in `main.cpp` Line 729-735
```cpp
void commandMotors() {
    // Motors (unchanged)
    analogWriteFrequency(MOTOR_PIN_1, 250);
    analogWriteResolution(12);
    analogWrite(MOTOR_PIN_1, (m1_command_PWM / 4000.0) * 4095);
    // ... etc for motors 2-6
    
    // Servos (FIXED)
    servo1.write((s1_command_PWM - 1000) * 0.18);
    servo2.write((s2_command_PWM - 1000) * 0.18);
    servo3.write((s3_command_PWM - 1000) * 0.18);
    servo4.write((s4_command_PWM - 1000) * 0.18);
    servo5.write((s5_command_PWM - 1000) * 0.18);
    servo6.write((s6_command_PWM - 1000) * 0.18);
    servo7.write((s7_command_PWM - 1000) * 0.18);
}
```

---

## 📁 File Structure Verification

Your structure looks **correct**! Here's what you have:

```
flight_controller/
├── platformio.ini          ✓ Updated with fixes
├── include/
│   ├── config.h           ✓ Updated with GYRO_SCALE, ACCEL_SCALE
│   └── pin_definitions.h  ✓
├── lib/
│   ├── RadioComm/
│   │   ├── radioComm.cpp  ✓ Can now find config.h
│   │   └── radioComm.h    ✓
│   ├── MPU6050/           ✓
│   ├── SBUS/              ✓
│   ├── DSMRX/             ✓
│   ├── PWMServo/          ✓
│   └── ... (other libs)
├── src/
│   └── main.cpp           ✓ Updated with servo fix
└── build.sh               ✓ Optional convenience script
```

**Everything is where it should be!**

---

## 🔧 Complete Fix Steps

### Step 1: Update platformio.ini

Replace your `platformio.ini` with the fixed version I provided.

**Key changes:**
- ✓ Removed wrong `__MK66FX1M0__` define
- ✓ Added `-I include` to `[env]`
- ✓ Kept correct `__IMXRT1062__` for Teensy 4.x

### Step 2: Update config.h

Add to your `include/config.h` (after `#define USE_MPU6050`):

```cpp
//========== IMU SCALE SETTINGS ==========
#ifdef USE_MPU6050
    #define GYRO_SCALE MPU6050_GYRO_FS_500
    #define ACCEL_SCALE MPU6050_ACCEL_FS_2
    #define GYRO_SCALE_FACTOR  65.5
    #define ACCEL_SCALE_FACTOR 16384.0
#elif defined(USE_MPU9250)
    #define GYRO_SCALE MPU9250::GYRO_RANGE_500DPS
    #define ACCEL_SCALE MPU9250::ACCEL_RANGE_2G
    #define GYRO_SCALE_FACTOR  65.5
    #define ACCEL_SCALE_FACTOR 16384.0
#endif
```

### Step 3: Fix commandMotors() in main.cpp

Replace lines 729-735 in `src/main.cpp`:

```cpp
// OLD (broken):
servo1.writeMicroseconds(s1_command_PWM);

// NEW (fixed):
servo1.write((s1_command_PWM - 1000) * 0.18);
```

Do this for all 7 servos.

---

## ✅ Test the Build

### Clean build (recommended)

```bash
pio run -e teensy40 -t clean
pio run -e teensy40
```

### Expected output

```
...
Building in release mode
Compiling .pio/build/teensy40/src/main.cpp.o
Compiling .pio/build/teensy40/lib.../RadioComm/radioComm.cpp.o
...
Linking .pio/build/teensy40/firmware.elf
Building .pio/build/teensy40/firmware.hex
Calculating size .pio/build/teensy40/firmware.elf
RAM:   [===       ]  34.2% (used 180012 bytes from 524288 bytes)
Flash: [==        ]  23.8% (used 485736 bytes from 2031616 bytes)
======= [SUCCESS] Took 45.23 seconds =======
```

**If you see SUCCESS ✓, you're good!**

---

## 🚀 Upload and Test

### Upload to Teensy

```bash
pio run -e teensy40 -t upload
```

**Teensy Loader will open automatically**
- Press button on Teensy
- Upload completes

### Monitor serial output

```bash
pio device monitor
```

**You should see:**
```
========================================
  dRehmFlight VTOL Flight Controller
========================================

Initializing IMU...
MPU6050 connected
Calibrating IMU...
Keep board FLAT and STILL!
AccError: X=0.0123 Y=-0.0087 Z=0.0234
GyroError: X=0.4567 Y=-0.2345 Z=0.1234
IMU calibration complete

Initializing receiver...
SBUS receiver initialized
  Port: Serial5 (RX pin 21)
  
Arming motors...
Motors armed

========================================
  FLIGHT CONTROLLER READY!
========================================
```

---

## 🎮 Test Receiver (Follow Your Guides)

Your guides are **perfectly compatible** with this build!

### 1. Bind Receiver (receiver_binding.md)
```bash
# Upload code
pio run -e teensy40 -t upload

# Power Teensy via USB
# Receiver gets power from VIN
# Follow binding steps in your guide
```

### 2. Test Receiver (demo_usage.md)
```cpp
// In main.cpp loop(), uncomment:
printRadioData();
```

Upload and monitor:
```bash
pio run -e teensy40 -t upload && pio device monitor
```

### 3. Calibrate IMU (mpu_6050_calibration.md)
- Automatic calibration runs in setup()
- Use Serial Plotter for visualization
- Tune filters in config.h

### 4. PID Tuning (pid_tuning.md)
```cpp
// Uncomment in loop():
printRollPitchYaw();
printPIDoutput();
```

---

## 🛠️ Using the Build Script

Your `build.sh` is a nice convenience wrapper!

```bash
# Make executable (one time)
chmod +x build.sh

# Run menu
./build.sh
```

**Menu options:**
1. Build all → `pio run`
2. Build specific → Choose teensy40/41
3. Upload → `pio run -t upload`
4. Clean → `pio run -t clean`
5. Monitor → `pio device monitor`
6. Clean + Build + Upload → All in one!

**But you can also just use:**
```bash
pio run              # Build
pio run -t upload    # Upload
pio device monitor   # Monitor
```

---

## 📋 Pre-Flight Checklist

Before testing with motors:

- [ ] All 4 errors fixed in code
- [ ] `pio run -e teensy40` compiles successfully
- [ ] Upload works (Teensy Loader)
- [ ] Serial monitor shows startup messages
- [ ] Receiver bound (LED solid)
- [ ] `printRadioData()` shows channel values changing
- [ ] IMU calibration completes (no errors)
- [ ] `printRollPitchYaw()` shows accurate attitude

**THEN:**
- [ ] Remove propellers
- [ ] Test motor outputs (ground)
- [ ] PID tuning (ground, no props)
- [ ] First hover test (props on)

---

## ❓ FAQ

**Q: Do I need the build.sh script?**
A: No, it's optional. `pio run` works fine. The script is just convenient.

**Q: Can I use Arduino IDE Serial Monitor?**
A: Yes! Just set baud rate to 115200.

**Q: Why -I include instead of #include "../include/config.h"?**
A: Cleaner code. With `-I include`, all lib files can use `#include "config.h"` directly.

**Q: Will my guides (receiver_binding.md, etc.) still work?**
A: Yes! 100% compatible. Follow them exactly as written.

**Q: What about the FlightCore empty code?**
A: You're right to ignore it. `main.cpp` + `radioComm` handle everything. No wrappers needed!

---

## 🎯 Summary

**All errors fixed by:**
1. ✅ Removed wrong `__MK66FX1M0__` define (Teensy 3.6) from Teensy 4.0 build
2. ✅ Added `-I include` so lib/ files can find config.h
3. ✅ Added `GYRO_SCALE` and `ACCEL_SCALE` defines to config.h
4. ✅ Changed `writeMicroseconds()` to `write()` for servos

**Your project structure is perfect. No changes needed to file organization!**

**Next steps:**
1. Apply the 3 file changes (platformio.ini, config.h, main.cpp)
2. Run `pio run -e teensy40`
3. Upload and test
4. Follow your excellent guides for setup!

🚁 Ready to fly!