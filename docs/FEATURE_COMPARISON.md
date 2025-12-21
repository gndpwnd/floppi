# Flight Controller Feature Comparison

## dRehmFlight-master vs Refactored PlatformIO Version

This document compares the original dRehmFlight BETA 1.3 with the refactored PlatformIO version to identify any missing features or differences.

---

## Summary

| Category | Original dRehmFlight | Your Version | Status |
|----------|---------------------|--------------|--------|
| Core Flight Control | ✅ | ✅ | Equivalent |
| IMU Support | MPU6050/MPU9250 | MPU6050/MPU9250 | ✅ |
| Receiver Support | PWM/PPM/SBUS/DSM | PWM/PPM/SBUS/DSM | ✅ |
| Control Modes | ANGLE/ANGLE2/RATE | ANGLE/RATE | ⚠️ Missing ANGLE2 |
| Auto-Calibration | Manual only | CH6 switch triggered | ✅ Improved |
| Modular Code | Single file | Split files | ✅ Improved |
| ESC Calibration | calibrateESCs() | Not implemented | ❌ Missing |
| Magnetometer Cal | calibrateMagnetometer() | Not implemented | ❌ Missing |
| 9DOF Madgwick | Full implementation | Simplified stub | ⚠️ Partial |
| Float Faders | floatFaderLinear/Sine | Not implemented | ❌ Missing |
| Loop Blink | loopBlink() | LED status in main loop | ✅ Alternative |

---

## Detailed Feature Analysis

### ✅ Features Present in Both

1. **Madgwick 6DOF Filter** - Attitude estimation without magnetometer
2. **PID Controllers** - Rate and Angle mode
3. **Motor Mixing** - Quad X configuration
4. **Failsafe System** - Radio signal loss protection
5. **Throttle Cut** - Safety switch on CH5
6. **Debug Print Functions** - All major print functions
7. **OneShot125 Protocol** - Motor output
8. **Standard PWM** - Servo output
9. **Low-pass Filtering** - Accel/Gyro/Mag data
10. **Arming Logic** - Throttle low + switch check

### ⚠️ Partial/Different Implementations

#### 1. controlANGLE2() - Cascaded PID Controller
**Original (lines 983-1067):**
- Cascaded angle+rate controller
- Outer loop: angle error → desired rate
- Inner loop: rate error → control output
- Has B_loop_roll/pitch damping parameters

**Your Version:**
- Only has controlANGLE() and controlRATE()
- Missing cascaded control option

**Impact:** Medium - ANGLE2 provides better performance for experienced pilots but requires more tuning. Can be added later if needed.

---

#### 2. 9DOF Madgwick Filter (Magnetometer)
**Original (lines 702-820):**
- Full magnetometer fusion
- Computes Earth's magnetic field reference
- Provides absolute yaw heading

**Your Version (line 828-831):**
```cpp
void Madgwick(float gx, float gy, float gz, float ax, float ay, float az, float mx, float my, float mz, float invSampleFreq) {
    // 9DOF Madgwick filter (with magnetometer) - implement if using MPU9250
    Madgwick6DOF(gx, gy, gz, ax, ay, az, invSampleFreq);  // Simplified for now
}
```

**Impact:** Low if using MPU6050 (no magnetometer). High if using MPU9250 - will lose heading hold capability.

---

#### 3. Pitch Angle Sign in Madgwick Output
**Original (line 818):**
```cpp
pitch_IMU = -asin(constrain(-2.0f * (q1*q3 - q0*q2),-0.999999,0.999999))*57.29577951;
```

**Your Version (line 824):**
```cpp
pitch_IMU = asin(-2.0f * (q1 * q3 - q0 * q2)) * 57.2957795;
```

**Impact:** Low - Sign may need adjustment based on aircraft orientation. The constrain() prevents NaN from asin(), your version may need this added.

---

### ❌ Missing Features

#### 1. calibrateESCs() Function
**Original (lines 1329-1380):**
- Allows standard ESC calibration with radio
- Throttle passthrough mode
- Loops until power cycle

**Status:** Not implemented in your version

**Impact:** Medium - You'll need to calibrate ESCs manually or with manufacturer tools.

---

#### 2. calibrateMagnetometer() Function
**Original:** Referenced but not shown in provided code

**Status:** Not implemented

**Impact:** Low if using MPU6050, High for MPU9250.

---

#### 3. floatFaderLinear() and floatFaderLinear2() Functions
**Original (lines 1382-1399+):**
```cpp
float floatFaderLinear(float param, float param_min, float param_max, float fadeTime, int state, int loopFreq)
```
- Smoothly transitions parameters between states
- Useful for flight mode transitions
- Can fade PID gains, control limits, etc.

**Status:** Not implemented

**Impact:** Low for basic flight. Useful for VTOL or multi-mode aircraft.

---

#### 4. loopBlink() Function
**Original:**
- Separate blink function with timing logic

**Your Version:**
- LED status integrated directly in main loop (lines 408-424)
- Different patterns: fast=calibrating, solid=armed, slow=normal

**Status:** ✅ Equivalent functionality, different implementation

---

### ✅ Improvements in Your Version

1. **Modular Code Structure**
   - Separate header files (config.h, pin_definitions.h)
   - Cleaner organization
   - Easier to maintain

2. **Auto-Calibration via CH6 Switch**
   - Runtime calibration without recompiling
   - Multiple calibration modes
   - Safety checks (not armed, throttle low)
   - Visual feedback via LED

3. **Configurable Everything**
   - All tunable parameters in config.h
   - Easy to switch between modes
   - Clear documentation

4. **PlatformIO Build System**
   - Proper library management
   - Multi-platform support
   - Modern toolchain

5. **Better Serial Output**
   - Boot messages
   - Status information
   - Calibration instructions

---

## Recommendations

### High Priority (Should Add)

1. **Add constrain() to Madgwick pitch calculation:**
```cpp
pitch_IMU = asin(constrain(-2.0f * (q1 * q3 - q0 * q2), -0.999999f, 0.999999f)) * 57.2957795;
```
This prevents potential NaN values.

### Medium Priority (Nice to Have)

2. **Add ESC Calibration Function:**
Port `calibrateESCs()` from original for convenience.

3. **Add controlANGLE2() Cascaded Controller:**
For advanced pilots who want better performance.

### Low Priority (Optional)

4. **Full 9DOF Madgwick:**
Only needed if using MPU9250 with magnetometer.

5. **Float Fader Functions:**
Only needed for VTOL or mode-switching aircraft.

---

## Channel Mapping Difference

**Original:**
- channel_1 = throttle
- channel_2 = aileron (roll)
- channel_3 = elevator (pitch)
- channel_4 = rudder (yaw)

**Your Version:**
- Using config.h channel mapping
- Flexible assignment via defines

**Note:** Make sure your THROTTLE_CHANNEL, ROLL_CHANNEL, etc. match your radio setup.

---

## Functional Equivalence Checklist

For basic quadcopter flight, your version has everything needed:

- [x] IMU reading and filtering
- [x] Attitude estimation (Madgwick 6DOF)
- [x] Radio command processing
- [x] PID control (angle and rate modes)
- [x] Motor mixing (quad X)
- [x] PWM output to ESCs
- [x] Servo output
- [x] Failsafe protection
- [x] Arming/disarming logic
- [x] Throttle cut safety
- [x] Debug output
- [x] Calibration routines

**Conclusion:** Your flight controller is functionally complete for basic quadcopter operation. The missing features (ANGLE2, ESC cal, mag cal, float faders) are advanced features not required for initial testing.
