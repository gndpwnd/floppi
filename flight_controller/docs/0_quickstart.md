# 🚀 QUICKSTART GUIDE - 60 Minutes to First Flight

**Target:** Get your flight controller working and flying in under 60 minutes.

---

## ⚡ Prerequisites Checklist

Before starting, make sure you have:

- [ ] **Hardware:**
  - Teensy 4.0 or 4.1
  - MPU6050 (GY-521 breakout board)
  - FlySky FS-iA6B receiver
  - FlySky transmitter (FS-i6, FS-i6X, or compatible)
  - USB cable for Teensy
  - 4 jumper wires (dupont style)

- [ ] **Software:**
  - PlatformIO installed (VS Code extension)
  - USB drivers for Teensy (Teensy Loader)

- [ ] **Knowledge:**
  - Basic soldering (if pins not pre-installed)
  - How to use Serial Monitor

---

## 🔧 Part 1: Hardware Setup (20 minutes)

### Step 1: Wire the MPU6050

```
MPU6050 GY-521    →    Teensy 4.0
─────────────────────────────────
VCC (3.3V or 5V)  →    3.3V
GND               →    GND  
SDA               →    Pin 18
SCL               →    Pin 19
```

**✅ Verification:** Power on Teensy via USB. MPU6050 power LED should light up.

---

### Step 2: Wire the FS-iA6B Receiver

**First, set transmitter to SBUS mode:**
1. Power ON transmitter
2. Menu → System → RX Setup → Serial Mode → **SBUS**
3. Save and power cycle transmitter

**Then wire receiver:**

```
FS-iA6B SBUS Port    →    Teensy 4.0
──────────────────────────────────
GND (Black wire)     →    GND
+5V (Red wire)       →    VIN
SBUS (White wire)    →    Pin 21 (RX5)
```

**✅ Verification:** Power on Teensy via USB. Receiver LED should blink (waiting for bind).

---

### Step 3: Bind Receiver to Transmitter

1. **Transmitter OFF**
2. **Press and HOLD bind button** on receiver (small button on top)
3. **While holding, plug in USB** to Teensy (powers receiver)
4. **Receiver LED flashes rapidly** (2-3x per second)
5. **Turn transmitter ON**
6. **Wait 3-5 seconds** - LED becomes solid
7. **Release bind button**

**✅ Verification:** Move transmitter sticks. Receiver LED should dim/brighten slightly.

📖 **Detailed wiring:** See [HARDWARE_SETUP.md](./HARDWARE_SETUP.md)

---

## 💻 Part 2: Software Upload (10 minutes)

### Step 4: Clone and Open Project

```bash
cd ~/floppi/flight_controller
code .  # Opens VS Code
```

### Step 5: Configure for Your Hardware

Open `include/config.h` and verify these are uncommented:

```cpp
#define USE_MPU6050          // ✓ Your IMU
#define USE_SBUS_RECEIVER    // ✓ Your receiver
#define USE_ANGLE_CONTROLLER // ✓ Beginner-friendly mode
```

### Step 6: Upload Code

```bash
# In VS Code terminal or external terminal:
pio run -e teensy40 -t upload

# Should see:
# SUCCESS: Uploaded to Teensy 4.0
```

**✅ Verification:** Teensy LED blinks 3 times rapidly, then 1Hz blinking.

---

## 🎛️ Part 3: Quick Calibration (15 minutes)

### Step 7: Test Receiver Communication

```bash
pio device monitor
```

**In `src/main.cpp`, uncomment:**
```cpp
printRadioData();  // Around line 180
```

**Re-upload and check Serial Monitor:**
```
CH1:1500 CH2:1500 CH3:1000 CH4:1500 CH5:1000 CH6:1000
```

**✅ Move sticks:** Numbers should change (1000-2000 range).

---

### Step 8: Auto-Calibrate MPU6050

**Method A: On Startup (Recommended)**

1. Place aircraft **flat and level** on desk
2. **Hold CH6 switch HIGH** (top position on 3-pos switch)
3. Power on Teensy via USB
4. **Wait 5 seconds** - calibration runs automatically
5. **Copy printed values** from Serial Monitor

**Method B: In-Flight Trigger**

1. Aircraft flat and level
2. **Disarm** (CH5 low)
3. **Throttle minimum**
4. **Hold CH6 MID** (middle position) for 3 seconds
5. Calibration runs
6. Copy values

**Output example:**
```
=== CALIBRATION RESULTS ===
IMU_ACC_ERROR_X 0.012345
IMU_ACC_ERROR_Y -0.008765
IMU_ACC_ERROR_Z 0.023456
IMU_GYRO_ERROR_X 0.456789
IMU_GYRO_ERROR_Y -0.234567
IMU_GYRO_ERROR_Z 0.123456
```

**Paste these into `include/config.h`:**
```cpp
#define IMU_ACC_ERROR_X 0.012345
#define IMU_ACC_ERROR_Y -0.008765
// ... etc
```

**Re-upload code.**

📖 **Manual calibration:** See [CALIBRATION_GUIDE.md](./CALIBRATION_GUIDE.md)

---

### Step 9: Verify IMU Data

**In `src/main.cpp`, uncomment:**
```cpp
printGyroData();
printAccelData();
printRollPitchYaw();
```

**Re-upload and check Serial Monitor:**

```
GyroX:0.05 GyroY:-0.12 GyroZ:0.03     ← Should be near 0 when still
AccX:0.01 AccY:0.00 AccZ:1.01         ← AccZ ≈ 1.0 when level
roll:0.0 pitch:0.0 yaw:0.0            ← Angles near 0 when level
```

**✅ Tilt aircraft 45°:** Roll/pitch should show ~45°.

---

## 🛠️ Part 4: Ground Testing (10 minutes)

### Step 10: Test Arming System

**⚠️ PROPS OFF! ⚠️**

**Arming conditions:**
1. Throttle stick LOW (<1050μs)
2. CH5 switch LOW (throttle cut OFF)

**Test sequence:**
1. Comment out all `print` functions in `main.cpp`
2. Re-upload
3. Open Serial Monitor
4. Lower throttle stick
5. CH5 switch to LOW position
6. Watch Serial Monitor: **"*** ARMED ***"**
7. LED blinks 3x rapidly
8. Raise throttle slightly → motors should try to spin (no props!)
9. CH5 switch to HIGH → **"*** DISARMED ***"** → motors stop

**✅ Verify:** Instant motor stop when disarmed.

---

### Step 11: Motor Direction Test

**⚠️ STILL NO PROPS! ⚠️**

**Standard quadcopter X configuration:**
```
    Front
     ↑
  1     2    ← CCW  CW
    \ /
    / \
  4     3    ← CW   CCW
```

**Test:**
1. Arm system
2. Slowly raise throttle to 30%
3. **Watch motor rotation** (no props, just feel with finger on shaft)
4. **Match diagram above** - swap wires if wrong

**✅ All 4 motors spinning correct direction.**

---

## 🎯 Part 5: First Flight (5 minutes)

### Step 12: Pre-Flight Checklist

- [ ] All calibrations complete
- [ ] Arming/disarming tested
- [ ] Motor directions correct
- [ ] **Props installed** (correct direction!)
- [ ] Battery charged
- [ ] Clear, open area (10m x 10m minimum)
- [ ] No wind (<5 mph)
- [ ] Spotter/safety observer

---

### Step 13: Hover Test

1. **Place aircraft on ground** in open area
2. **Arm system** (throttle low + CH5 low)
3. **Slowly raise throttle** to 40-50%
4. **Aircraft lifts 15cm off ground**
5. **Hold for 5 seconds** - watch for oscillations
6. **Gently lower throttle** - land softly
7. **Disarm** (CH5 high)

**✅ Success criteria:**
- Stable hover (no wild oscillations)
- Responds to stick inputs
- Lands gently

---

### Step 14: First Flight

**If hover test passed:**

1. Repeat hover at 30cm altitude
2. Try gentle **roll left/right** (small inputs!)
3. Try gentle **pitch forward/back**
4. Try gentle **yaw left/right**
5. Fly around small area (3m x 3m)
6. Land when battery low (~30% remaining)

**🎉 Congratulations! You're flying!**

---

## 📊 Expected Performance

**With default PID gains:**
- Hover: Stable, minor drift acceptable
- Response: Moderate speed
- Oscillations: None or very minor

**If oscillations occur:** See PID tuning section below.

---

## 🔧 Quick PID Tuning (if needed)

**Symptoms → Solutions:**

| Problem | Solution |
|---------|----------|
| Sluggish response | Increase `KP_ROLL_RATE` and `KP_PITCH_RATE` by 0.02 |
| Fast oscillation (5-10Hz) | Decrease `KP` by 20% |
| Slow wobble (<1Hz) | Decrease `KI` by 50% |
| Overshoots and bounces | Increase `KD` by 0.0001 |

**Edit in `include/config.h`, re-upload, test again.**

📖 **Full PID tuning:** See [PID_TUNING_GUIDE.md](./PID_TUNING_GUIDE.md)

---

## ❓ Quick Troubleshooting

### Receiver not responding
- Check SBUS wiring (Pin 21)
- Verify transmitter is in SBUS mode
- Re-bind receiver

### IMU data noisy/drifting
- Re-run auto-calibration (board must be perfectly still)
- Check MPU6050 is secure (no vibration)
- Verify 3.3V power supply is stable

### Motors won't arm
- Check throttle is below 1050μs
- Check CH5 is low (<1500μs)
- Check Serial Monitor for error messages

### Aircraft flips on takeoff
- Check motor directions match diagram
- Check propeller directions match motors
- Re-check PID gains (may be too high)

📖 **Full troubleshooting:** See [TROUBLESHOOTING.md](./TROUBLESHOOTING.md)

---

## 🎓 Next Steps

After successful first flight:

1. **Tune PID gains** for better performance
2. **Test different flight modes** (rate vs angle)
3. **Add failsafe testing**
4. **Optimize filter coefficients**
5. **Advanced maneuvers**

**📚 Recommended reading order:**
1. [HARDWARE_SETUP.md](./HARDWARE_SETUP.md) - Detailed wiring
2. [CALIBRATION_GUIDE.md](./CALIBRATION_GUIDE.md) - Advanced calibration
3. [PID_TUNING_GUIDE.md](./PID_TUNING_GUIDE.md) - Performance tuning
4. [TROUBLESHOOTING.md](./TROUBLESHOOTING.md) - Problem solving

---

## ⚠️ Safety Reminders

- ✅ **ALWAYS remove props** for ground testing
- ✅ **NEVER fly indoors** (first flight)
- ✅ **ALWAYS have spotter** watching aircraft
- ✅ **ALWAYS test arming/disarming** before flight
- ✅ **ALWAYS check battery voltage** before flight
- ✅ **NEVER fly near people/animals**

**Happy flying! 🚁**