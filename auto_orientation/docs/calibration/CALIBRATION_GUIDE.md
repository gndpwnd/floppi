# BNO085 Calibration Guide

**Complete step-by-step guide for magnetometer calibration on the Auto Orientation system.**

- **Document Status**: v1.0 - Comprehensive user guide for first-time and recalibration  
- **Target Audience**: Developers/technical users with hardware assembled  
- **Tested On**: Arduino Mega 2560, BNO085 (Adafruit breakout), I2C @ 100kHz  
- **Time Required**: 5-10 minutes (2-3 minutes of active motion)

---

## Table of Contents

1. [What Is Calibration?](#what-is-calibration)
2. [Why Calibration Matters](#why-calibration-matters)
3. [Calibration Status Levels](#calibration-status-levels)
4. [Before You Start](#before-you-start)
5. [Step-by-Step Calibration Procedure](#step-by-step-calibration-procedure)
6. [Recognizing Completion](#recognizing-completion)
7. [Persistence and Auto-Loading](#persistence-and-auto-loading)
8. [Recalibration: When and Why](#recalibration-when-and-why)
9. [Troubleshooting](#troubleshooting)
10. [Common Mistakes](#common-mistakes)

---

## What Is Calibration?

The **BNO085** is a 9-axis IMU that measures:
- **Accelerometer**: Gravity and linear motion
- **Gyroscope**: Rotational motion
- **Magnetometer**: Earth's magnetic field

**Calibration** is the process of teaching the sensor to accurately measure Earth's magnetic field despite local interference. Without calibration, magnetic heading (yaw/compass direction) can be off by 30-90 degrees.

### The Problem It Solves

Nearby metal objects, electronics, and magnetic interference distort magnetometer readings:

```
Without Calibration:
  Actual direction: North (0°)
  Sensor reads: 45° (wrong!)
  Error: 45°

With Calibration:
  Actual direction: North (0°)
  Sensor reads: 2° (correct!)
  Error: ±2° (acceptable)
```

### Why Only Magnetometer?

The magnetometer requires external calibration because it measures **external magnetic fields** (Earth's field), unlike the accelerometer and gyroscope which measure internal forces. The BNO085 firmware automatically calibrates accel and gyro internally during normal operation—you only need to calibrate the magnetometer.

---

## Why Calibration Matters

### Impact on Heading Accuracy

| Calibration Status | Yaw Accuracy | Use Case | Example |
|---|---|---|---|
| **Uncalibrated (0)** | ±30-90° | Not usable | 45° off from true north |
| **Low (1)** | ±15-30° | Testing only | 15° off from true north |
| **Medium (2)** | ±5-10° | Field deployment | 8° off from true north |
| **High (3)** | ±2-3° | **Production ready** | 2° off from true north |

### Real-World Impact

**Drone Navigation**: If yaw is 45° off, a navigation algorithm will fly in the wrong direction.

**Camera Pointing**: If yaw calibration is poor, a camera gimbal will point 30° off target.

**Vehicle Tracking**: A self-driving vehicle needs ±2° heading accuracy; poor calibration causes drifting.

---

## Calibration Status Levels

The BNO085 reports four independent calibration metrics:

```
Calibration Status Structure:
{
  "system": 3,      ← Overall system calibration (composite)
  "accel": 3,       ← Accelerometer calibration
  "gyro": 3,        ← Gyroscope calibration
  "mag": 3          ← Magnetometer calibration (focus here)
}

Values: 0 = Uncalibrated, 1 = Low, 2 = Medium, 3 = High
```

### Focusing on Magnetometer (mag)

For this procedure, **focus on the "mag" field**:

- **mag = 0** (░░░): No calibration. Heading unreliable.
- **mag = 1** (█░░): Low calibration. Yaw may drift 20-30°.
- **mag = 2** (██░): Medium calibration. Yaw accurate to ~5-10°.
- **mag = 3** (███): **DONE!** Yaw accurate to ±2-3°.

### Why mag >= 2 Is Good Enough

| mag Level | System Field | Yaw Drift | Safe for | Notes |
|---|---|---|---|---|
| **0** | Too weak | > 30° | Nothing | Unusable |
| **1** | Weak | 15-30° | Testing | Not for deployment |
| **2** | Good | 5-10° | Field use | Acceptable for most applications |
| **3** | Excellent | 2-3° | Production | Best possible with consumer hardware |

**Recommendation**: Stop when **mag >= 2**. Reaching mag = 3 is nice but not always necessary.

---

## Before You Start

### Prerequisites Checklist

- [ ] **Hardware is wired correctly**
  - BNO085 connected to Arduino Serial1 (pins 18/19)
  - P1 pin is set HIGH (5V)
  - Power supply is stable (5V, 1A)
  - All connections tight (no loose wires)

- [ ] **Firmware is loaded**
  - Latest code uploaded via `platformio run --target upload`
  - Serial monitor shows output when powered

- [ ] **Monitoring tool is ready**
  ```bash
  python3 tools/simple_monitor.py /dev/ttyACM0
  ```
  (Replace `/dev/ttyACM0` with your COM port)

- [ ] **Time and space**
  - 5-10 minutes available
  - Quiet, clear space (indoors is fine)
  - Preferably away from large metal objects or electronics

### Finding Your Serial Port

**Linux/Mac**:
```bash
ls /dev/tty*
# Look for /dev/ttyACM0 or /dev/ttyUSB0
```

**Windows**:
- Open Device Manager → Ports (COM & LPT)
- Look for "Arduino Mega" or similar
- Note the COM port number (e.g., COM3)

---

## Step-by-Step Calibration Procedure

### Phase 1: Start Monitoring (30 seconds)

1. **Open a terminal** in your project directory

2. **Start the simple monitor**:
   ```bash
   python3 tools/simple_monitor.py /dev/ttyACM0
   ```
   
   On Windows, use:
   ```bash
   python3 tools/simple_monitor.py COM3
   ```

3. **Verify output** appears:
   ```
   Quaternion: w=0.707107 x=0.000000 y=0.000000 z=0.707107
   Calibration:
     System: 0 ░░░
     Accel:  0 ░░░
     Gyro:   0 ░░░
     Mag:    0 ░░░
   ```

4. **Note the baseline**:
   - All values should start at 0 (uncalibrated)
   - Wait 5-10 seconds for sensor warmup
   - You'll see "NOT CALIBRATED - move board now!"

---

### Phase 2: Perform Figure-8 Motion (1-2 minutes)

This is the critical step. The pattern helps the magnetometer "see" Earth's field from all angles.

#### Visual Guide

```
Imagine drawing a lazy "8" in 3D space:

From Front:           From Side:           From Top:
    /\                                    /~~~~~\
   /  \               Wave up/down        |  ◯   |
  /    \              while doing         \~~~~~/ 
 |      |             figure-8 motion
  \    /
   \  /
    \/

Simultaneously rotate on all three axes:
• ROLL:  Rotate around front-back axis (like rolling a ball)
• PITCH: Tilt up-down (nodding yes)
• YAW:   Rotate left-right (shaking head for no)
```

#### Detailed Steps

1. **Hold the device** naturally in your hand (no need to grip hard)

2. **Start slow**: First 10 seconds, move gently in a figure-8 pattern

3. **Increase vigor**: Seconds 10-30, move faster and more deliberately:
   - Large, flowing figure-8 motions
   - Rotate on all three axes simultaneously
   - Walk in circles while doing this (~1-2 meter diameter)

4. **Maintain motion**: Keep moving for **at least 60 seconds total**

5. **Don't stop early**: Even if calibration reaches "High" (mag = 3), keep moving for 30+ more seconds to ensure it's stable

#### Example Timeline

```
Time    Calibration Status   What You See         What You Should Do
----    -------------------  ---------            ----
0-5s    Mag: 0 ░░░          "NOT CALIBRATED"     Wait, sensor warmup
5-10s   Mag: 0-1 █░░        "move board now!"    Start figure-8 motion
10-20s  Mag: 1 █░░          "Calibrating..."     Keep moving smoothly
20-40s  Mag: 1-2 ██░        "Calibrating..."     Increase motion vigor
40-60s  Mag: 2 ██░          "Almost done..."     Continue for 20+ more sec
60-80s  Mag: 2-3 ███        "FULLY CALIBRATED"   Keep moving to confirm
80+s    Mag: 3 ███          "DONE!"              You can stop
```

#### Common Motion Patterns

**Pattern 1: Figure-8 Sweep** (Recommended)
- Hold device at arm's length
- Trace a figure-8 in the air (sideways, like infinity symbol)
- Rotate device on all axes as you trace
- Walk in a slow circle as you do this
- Duration: 60+ seconds

**Pattern 2: Rotating Sphere**
- Hold device in center of imaginary sphere
- Rotate it to trace all directions on sphere surface
- Pitch up-down, roll side-side, yaw left-right
- Walk around while doing this
- Duration: 60+ seconds

**Pattern 3: Gentle Wave**
- Wave device up-down while rotating it
- Move it side-to-side in smooth motion
- Rotate around all axes simultaneously
- This is slower but very thorough
- Duration: 90+ seconds

---

### Phase 3: Monitor Calibration Progress

As you perform the motion, watch the monitor output:

```
Current state example:
Quaternion: w=0.625 x=0.325 y=-0.214 z=0.681
Calibration:
  System: 1 █░░
  Accel:  2 ██░
  Gyro:   2 ██░
  Mag:    1 █░░   ← Focus on this line
⚠ Calibrating... (move board in figure-8)
```

**Mag values progression**:
- 0 → 1: You've started collecting data (good!)
- 1 → 2: Significant improvement (continue for full coverage)
- 2 → 3: Near perfect (very good calibration)

**Note**: The mag value might fluctuate slightly. This is normal. The sensor is refining its estimates.

---

### Phase 4: Confirm Completion

Stop moving when you see:

```
Calibration:
  System: 3 ███
  Accel:  3 ███
  Gyro:   3 ███
  Mag:    3 ███
✓✓✓ FULLY CALIBRATED! ✓✓✓
```

**Acceptable minimums**:
- **Must have**: mag >= 2
- **Recommended**: mag = 3
- **Total time active motion**: 60+ seconds

**Don't worry about**: Accel and Gyro. These auto-calibrate during normal use. Focus on Mag.

---

## Recognizing Completion

### Visual Indicators from Monitor

1. **All four metrics at level 3 (███)**:
   ```
   System: 3 ███
   Accel:  3 ███
   Gyro:   3 ███
   Mag:    3 ███
   ```

2. **Confirmation message**:
   ```
   ✓✓✓ FULLY CALIBRATED! ✓✓✓
   ```

3. **Quaternion values stabilize**:
   - If you keep the device still, quaternion stays constant
   - If you rotate, it changes smoothly (no jumps)

### Testing Yaw Stability

**Quick test after calibration**:

1. **Point north**: Use a compass app, point device north
2. **Check yaw value**: Should match compass (±2-3° acceptable)
3. **Rotate 90°**: Yaw should change by ~90°
4. **Compare**: Your measured yaw vs actual compass direction

**Good calibration looks like**:
```
Compass says:    North (0°)
Monitor shows:   Yaw = 358° (±2° is excellent!)

Compass says:    East (90°)
Monitor shows:   Yaw = 92° (±2° is excellent!)

Compass says:    South (180°)
Monitor shows:   Yaw = 178° (±2° is excellent!)
```

---

## Persistence and Auto-Loading

### How Persistence Works

When you reach **mag = 3** (or mag >= 2), the BNO085 automatically saves the calibration to its internal flash memory. This is **instantaneous**—no extra steps needed.

```
Timeline:
Calibration → Mag = 3 → [Automatic save to sensor NVM] → Done
```

### Next Power-On (Auto-Load)

When you power on the device again:

1. **Arduino boots**
2. **BNO085 checks internal memory**: "Is there valid calibration?"
3. **YES**: Applies saved calibration immediately (~100ms)
4. **RESULT**: You skip the 60-second calibration step!

```
Power Cycle 1 (First Time):
Power on → Calibrate manually (60s) → Save → Power off

Power Cycle 2 (Next Time):
Power on → [AUTO-LOAD saved calibration] → Ready immediately!
```

### Verification of Persistence

**Test 1: Same-day power cycle**
1. Calibrate fully (mag = 3)
2. Note the mag value: 3
3. Power off device completely (5 second wait)
4. Power on again
5. **Result**: mag should immediately show 2-3 (auto-loaded)

**Test 2: Extended deployment**
1. Calibrate at home (mag = 3)
2. Transport to field location
3. Power on at field location
4. **Result**: Calibration is there, ready to use
5. You can skip the 60-second calibration at field

### Calibration Lifespan

- **Duration**: **Indefinite**. Calibration persists until:
  - You perform a new calibration (overwrites old one)
  - EEPROM becomes corrupted (extremely rare)
  - Device is power-cycled improperly during save (very unlikely)

- **Frequency**: You typically recalibrate when:
  - Moving to a very different geographic location (different magnetic field characteristics)
  - Yaw values start behaving erratically
  - You add metal components near sensor

---

## Recalibration: When and Why

### When You DO Need to Recalibrate

1. **Moving to very different location** (e.g., different continent)
   - Earth's magnetic field varies by location
   - Magnetic declination and inclination change
   - Local anomalies differ
   - **Action**: Recalibrate at new location

2. **Yaw drifts or behaves oddly**
   - Before: Yaw stayed at 245° ± 2°
   - Now: Yaw jumps 245° → 280° → 310°
   - **Cause**: Possible EEPROM corruption or sensor drift
   - **Action**: Recalibrate to refresh calibration

3. **Adding metal objects nearby**
   - Mount a metal bracket on sensor
   - Add metal shielding
   - Install metal frame around device
   - **Action**: Recalibrate to account for new geometry

### When You DO NOT Need to Recalibrate

- **Every power cycle**: NO! Calibration auto-loads.
- **Moving to different room**: NO! Calibration is global (Earth's field).
- **Moving between indoors and outdoors**: NO! Magnetic field is same.
- **Time passing**: NO! Calibration doesn't degrade.
- **Temperature changes**: NO! BNO085 auto-compensates.

### How to Recalibrate

Simply repeat the [Step-by-Step Calibration Procedure](#step-by-step-calibration-procedure) above. The new calibration **overwrites the old one** in the sensor's memory.

---

## Troubleshooting

### "Calibration status shows all zeros, won't improve"

**Symptoms**:
```
Mag: 0 ░░░
⚠ Calibrating... (move board in figure-8)
```
After 30 seconds of vigorous motion, mag still 0.

**Likely Causes**:
1. **P1 pin not set to 5V** (most common)
2. BNO085 not powered correctly
3. UART connection loose or faulty
4. Firmware not uploaded

**Fixes**:
1. **Check P1 pin voltage** (use multimeter):
   ```
   P1 to GND should read: 5V
   If it reads 0V or 3.3V: Connection is wrong
   Fix: Connect P1 to 5V rail via resistor (10kΩ recommended)
   ```

2. **Power test**:
   - Disconnect BNO085 from Arduino
   - Connect multimeter to BNO085 VCC and GND
   - Verify 5V ± 0.2V
   - Reconnect if good

3. **Verify UART connection**:
   - TX (BNO085) → RX1 (Pin 19) - disconnect and reinsert
   - RX (BNO085) → TX1 (Pin 18) - disconnect and reinsert
   - All connections should be snug

4. **Check firmware upload**:
   ```bash
   platformio run --target upload
   # Should see "avrdude done" at end
   ```

---

### "Calibration status bounces between levels"

**Symptoms**:
```
Mag: 2 ██░
[a few seconds later]
Mag: 1 █░░
[then]
Mag: 2 ██░
```
Status keeps cycling instead of progressing smoothly.

**Likely Causes**:
1. Power supply is unstable or noisy
2. Magnetic interference in environment
3. Sensor circuit has loose connections

**Fixes**:

1. **Stabilize power supply**:
   - Use wall-powered 5V supply instead of USB
   - Add large capacitor (100-220μF) near Arduino 5V input
   - Check power cable isn't damaged

2. **Move to different location**:
   - Away from electronics (computers, routers, microwaves)
   - Away from large metal objects
   - Outdoors is often better (fewer interference sources)

3. **Check power delivery to BNO085**:
   ```
   With multimeter, measure BNO085 VCC:
   - Should be steady 5V
   - If it dips below 4.8V, power supply is weak
   - If it fluctuates, power supply is noisy
   ```

---

### "Calibration reaches mag = 2, then stops improving"

**Symptoms**:
```
[After 30 seconds]
Mag: 2 ██░
[After 60 seconds]
Mag: 2 ██░ (still same)
[After 120 seconds]
Mag: 2 ██░ (stuck!)
```

**Likely Causes**:
1. Not enough motion variety (moving in only one plane)
2. Magnetic interference in location (biases the sensor)
3. Need more vigorous motion

**Fixes**:

1. **Increase motion variety**:
   - Rotate on ALL three axes (roll, pitch, yaw) simultaneously
   - Walk in circles while moving figure-8
   - Don't just wave up-down; also rotate side-to-side

2. **Change location**:
   - Move away from large metal objects
   - Get away from electronics, power lines
   - Open outdoor space is ideal

3. **More vigorous motion**:
   - Larger figure-8 patterns (1-2 meter size)
   - Faster rotation on all axes
   - Continue for 90-120 seconds total

---

### "Monitor shows correct calibration, but yaw is still off"

**Symptoms**:
```
Calibration:
  Mag: 3 ███
✓✓✓ FULLY CALIBRATED! ✓✓✓

But when you point north, Yaw shows 45° (should be ~0°)
```

**Likely Causes**:
1. Magnetometer calibration is location-biased
2. Large metal object permanently nearby
3. Magnetic declination not accounted for (normal, expected)

**Fixes**:

1. **Understand magnetic declination**:
   - True North ≠ Magnetic North
   - In most locations, difference is 5-15°
   - This is **normal and expected**
   - Use compass app to verify your location's declination

2. **Recalibrate in open space**:
   - Move to location with no nearby metal
   - Repeat calibration procedure
   - This often improves yaw accuracy significantly

3. **Accept local bias**:
   - If calibration is stable (mag = 3), it's working
   - Small yaw offset (5-10°) is acceptable
   - Use compass app as ground truth, not the monitor

---

### "I don't see calibration data on the monitor"

**Symptoms**:
```
[Monitor output missing calibration section]
Quaternion: w=0.707 x=0 y=0 z=0.707
[No "Calibration:" section below]
```

**Likely Causes**:
1. Simple monitor script not displaying calibration
2. BNO085 not responding
3. Firmware not sending calibration status

**Fixes**:

1. **Use simple_monitor.py directly**:
   ```bash
   python3 tools/simple_monitor.py /dev/ttyACM0
   ```
   This script is designed to show calibration.

2. **Verify BNO085 is running**:
   ```bash
   platformio run --target upload
   # If upload fails, check hardware connections
   ```

3. **Check if JSON has calibration field**:
   ```bash
   # Raw output capture
   python3 tools/simple_monitor.py /dev/ttyACM0 2>&1 | head -20
   ```
   Look for "calibration" in the JSON.

---

## Common Mistakes

### Mistake 1: Stopping Motion Too Early

**What you do**: Move the device for 30 seconds, see calibration improving, then stop.

**What happens**: Calibration gets stuck at mag = 1 or mag = 2.

**Why**: The sensor hasn't seen enough magnetic field variation yet. It needs continuous motion from multiple angles.

**Fix**: **Always move for at least 60 seconds**, even if calibration seems to be progressing slowly. The last 30 seconds of motion often make the difference between mag = 2 and mag = 3.

---

### Mistake 2: Only Moving in One Direction

**What you do**: Wave device up-and-down, or only side-to-side.

**What happens**: Calibration improves but gets stuck; yaw values are still off after calibration.

**Why**: The sensor needs to experience the magnetic field from **all orientations**. Moving in only one plane (up-down, or side-side) doesn't provide enough data variety.

**Fix**:
- Rotate on **all three axes** simultaneously
- Roll (rotate around front-back axis)
- Pitch (tilt up-down)
- Yaw (rotate left-right)
- Walk in circles while doing this

---

### Mistake 3: Moving Too Fast or Jerky

**What you do**: Rapid, jerky motions with sudden direction changes.

**What happens**: Calibration status bounces or is unstable; sensor can't lock onto good calibration.

**Why**: Jerky motion confuses the gyroscope and accelerometer, making it harder for the sensor fusion algorithm to lock calibration.

**Fix**: **Move smoothly and continuously**, like tai chi or slow swimming motions. Think of it as gentle, flowing motion rather than rapid movements.

---

### Mistake 4: Not Waiting for Sensor Warmup

**What you do**: Immediately start calibration motion upon power-on.

**What happens**: Calibration seems stuck at mag = 0 for the first 10 seconds.

**Why**: The sensor needs 5-10 seconds to warm up and stabilize internal references before it can start collecting calibration data.

**Fix**: **Wait 10 seconds after power-on before starting motion**. Or just ignore the first 10 seconds of "NOT CALIBRATED" status—it's normal.

---

### Mistake 5: Staying in Location with Local Magnetic Interference

**What you do**: Calibrate right next to a computer, metal furniture, or electronics.

**What happens**: Calibration reaches mag = 2-3, but yaw values are biased or strange.

**Why**: Local magnetic interference from nearby metal/electronics skews the sensor's perception of Earth's magnetic field. The sensor adapts to this local bias, making yaw readings unreliable when you move away.

**Fix**:
- Move to a location away from large metal objects
- Move away from electronics, routers, microwaves
- Preferably outdoors in open space (least interference)
- Recalibrate at the location where you'll be using the system

---

### Mistake 6: Expecting Mag = 3 Every Time

**What you do**: Try over and over to reach mag = 3, get frustrated when it sticks at mag = 2.

**What happens**: You feel like calibration is failing, but it's actually working fine.

**Why**: mag = 2 is **fully acceptable** for most applications. Reaching mag = 3 requires perfect conditions and sustained motion. mag = 2 gives ±5-10° yaw accuracy, which is good enough for field deployment.

**Fix**: **Stop at mag >= 2**. This is production-ready. Mag = 3 is nice-to-have, not required.

```
mag = 2: Yaw accurate ±5-10° → GOOD for field use
mag = 3: Yaw accurate ±2-3°   → EXCELLENT, but not always achievable
```

---

## Summary Checklist

### Before Calibration
- [ ] All hardware connections verified
- [ ] Firmware uploaded and running
- [ ] Monitor tool ready
- [ ] Serial port identified
- [ ] 5-10 minutes available
- [ ] Clear space (indoors is fine)

### During Calibration
- [ ] Wait 10 seconds for sensor warmup
- [ ] Start monitor and watch calibration status
- [ ] Perform figure-8 motion for 60+ seconds
- [ ] Rotate on all three axes simultaneously
- [ ] Walk in circles while moving
- [ ] Move smoothly (not jerky)
- [ ] Continue until mag >= 2 (or mag = 3 if possible)

### After Calibration
- [ ] Verify final status shows mag >= 2
- [ ] Test yaw accuracy with compass app
- [ ] Power off and on to verify auto-load
- [ ] Document location (for future recalibration reference)

### Recalibration Decision
- Moving to new geographic location → Recalibrate
- Yaw drifting after working correctly → Recalibrate
- Adding metal objects nearby → Recalibrate
- Otherwise → No recalibration needed

---

## Reference: Serial Output Format

### Calibration Data in JSON

```json
{
  "timestamp_ms": 123456,
  "orientation": {
    "valid": true,
    "quaternion": {
      "w": 0.707,
      "x": 0.0,
      "y": 0.0,
      "z": 0.707
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

### Interpreting Calibration Values

```
"system": 3    → Overall system calibration = excellent
"accel": 3     → Accelerometer calibration = excellent (auto)
"gyro": 3      → Gyroscope calibration = excellent (auto)
"mag": 3       → Magnetometer calibration = excellent (manual)

All values: 0 = Uncalibrated, 1 = Low, 2 = Medium, 3 = High
```

### Monitor Output Visualization

```
Calibration: High ███

Explanation:
░░░ = Uncalibrated (no data)
█░░ = Low (some data)
██░ = Medium (good data, 2/3 full)
███ = High (excellent, fully calibrated)
```

---

## FAQ: Calibration Questions

**Q: Do I need to calibrate every time I power on?**  
A: No! Calibration is saved automatically. After reaching mag >= 2, the next power-on will auto-load the calibration in ~100ms.

**Q: Can I move to a different room after calibrating?**  
A: Yes. The calibration is for Earth's global magnetic field. Moving between rooms doesn't require recalibration.

**Q: Can I calibrate indoors?**  
A: Yes. Earth's magnetic field is the same everywhere (locally). Indoors is fine for calibration.

**Q: What if I calibrate, then move to a different country?**  
A: Consider recalibrating at the new location. Earth's magnetic field varies by location (declination, inclination). Recalibration ensures maximum accuracy.

**Q: How long does calibration last?**  
A: Indefinitely, until you recalibrate. There's no time limit or degradation.

**Q: Why is yaw still off by 30° even though mag = 3?**  
A: This is likely magnetic declination (true north vs magnetic north). Use a compass app for your location to understand the expected offset. ±5-10° is normal.

**Q: Can I calibrate outdoors?**  
A: Yes, and it's often better. Fewer interference sources (metal, electronics) outdoors.

**Q: What if I can't reach mag = 2?**  
A: Try a different location. Persistent magnetic interference (nearby electronics, metal) may be blocking good calibration. Move away from potential sources and try again.

---

**Last Updated**: 2026-05  
**Version**: 1.0  
**Difficulty Level**: Beginner to Intermediate  
**Tested Hardware**: Arduino Mega 2560 + BNO085 (Adafruit) + I2C @ 100kHz
