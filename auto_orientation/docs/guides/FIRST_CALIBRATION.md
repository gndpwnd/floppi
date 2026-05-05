# First Calibration: Getting Started Guide

A user-friendly walkthrough for first-time calibration of the Auto Orientation system.

---

## Before You Start

**Prerequisites:**
- [ ] Hardware is wired and tested (see [Quick Start](QUICK_START_GETTING_STARTED.md))
- [ ] Firmware is uploaded and running
- [ ] Monitor is displaying orientation data
- [ ] You have 5-10 minutes of uninterrupted time
- [ ] You're in a quiet area (indoors is fine for initial test)

---

## What is Calibration?

The BNO085 IMU contains a magnetometer that measures Earth's magnetic field. However, nearby metal objects, electronics, and magnetic interference can distort these measurements.

**Calibration** teaches the sensor to recognize and ignore these distortions, so it can accurately detect true north (magnetic north).

**Good news**: Once calibrated, the calibration is automatically saved and **persists across power cycles**. You won't need to recalibrate every time you power on!

---

## Expected Calibration Time

| Stage | Time | What's Happening |
|-------|------|-----------------|
| Power-on | 0-10 sec | Sensor warming up, no action needed |
| Initial Motion | 10-20 sec | Sensor collects first calibration data |
| Mid-calibration | 20-40 sec | Calibration improving, keep moving |
| Fine-tuning | 40-60 sec | Nearly complete, move more vigorously |
| **Completion** | **~60 sec** | Calibration saved, ready to use |

**Total time**: 1-2 minutes from cold start to fully calibrated.

---

## The Calibration Process

### Step 1: Start the Monitor (30 seconds)

Open the serial monitor to watch the calibration status in real-time:

```bash
python3 tools/real_time_monitor.py /dev/ttyACM0
```

You should see output like:
```
Orientation:
  Roll:  0.0°
  Pitch: 0.0°
  Yaw:   0.0°

Status: Initializing...
Calibration: Unreliable ░░░
```

### Step 2: Perform Figure-8 Motion (60 seconds)

This is the key step. The figure-8 motion helps the magnetometer "see" Earth's magnetic field from many angles.

**How to do it:**

1. Hold the device naturally in your hand (no need to grip tightly)

2. Move it in a smooth **figure-8 pattern** (like drawing a sideways "8" in the air)

3. **Simultaneously rotate** the device on all three axes:
   - **Roll**: Rotate around the front-to-back axis (like rolling an apple)
   - **Pitch**: Tilt up and down (like nodding yes)
   - **Yaw**: Rotate left and right (like shaking head for "no")

4. Keep moving for **30-60 seconds** continuously

5. Move in a ~1-2 meter circle while doing this (walk around a bit)

**Visual Guide:**

```
Front View:          Side View:           Top View:
    /\                                    
   /  \               Move in a           /~~~\
  /    \              circle while        |   |
 |      |             doing figure-8      \___/
  \    /
   \  /              Keep device rotating
    \/               on all axes at once
```

### Step 3: Watch Calibration Progress

Monitor the status line as you move:

```
Calibration: Unreliable ░░░  → Low █░░  → Medium ██░  → High ███
```

**Calibration Status Explained:**

| Status | Symbol | Meaning | Action |
|--------|--------|---------|--------|
| **Unreliable** | ░░░ | No motion detected | Start moving the device |
| **Low** | █░░ | Some data collected, but not enough | Keep moving, be more vigorous |
| **Medium** | ██░ | Good data, almost there | Continue moving, almost done |
| **High** | ███ | **Fully calibrated!** | You can stop—it's saved |

**Timeline example:**
```
0-10 sec:  "Unreliable" (sensor warming up)
10-20 sec: "Low" (started collecting data)
20-40 sec: "Medium" (getting better)
40-60 sec: "High" ← STOP HERE, you're done!
```

### Step 4: Verify Completion

When you see:
```
Calibration: High ███
```

**Congratulations!** Your magnetometer is fully calibrated. The sensor automatically saved the calibration to its internal flash memory.

---

## What "Done" Looks Like

**Perfect calibration indicators:**

1. **Status shows "High" (███)**:
   ```
   Calibration: High ███
   ```

2. **Yaw angle is stable**:
   - Before calibration: Yaw value might drift or jump randomly
   - After calibration: Yaw stays steady even when you're still
   
   ```
   Before: Yaw jumps from 0° → 45° → 200° (unstable)
   After:  Yaw stays at 245° ± 2° (stable)
   ```

3. **Orientation values are smooth**:
   - Roll, pitch, and yaw change smoothly as you move
   - No sudden large jumps
   - Values return to expected values when returning to same position

4. **Data continues to output normally**:
   - No error messages
   - GPS data updates (if outdoors with clear sky)
   - Orientation updates at ~100 Hz (rapid, smooth)

---

## Common Mistakes to Avoid

### Mistake 1: Stopping Motion Too Early

**What happens**: Calibration gets stuck at "Low" or "Medium"

**Why**: The sensor hasn't seen the magnetic field from enough angles yet

**Fix**: Keep moving for the full 60 seconds, even if the status seems to be progressing slowly

**How to know**: Monitor the status line. If it stays at the same level for >10 seconds, you need to move more vigorously.

---

### Mistake 2: Only Moving Up and Down (or in one direction)

**What happens**: Calibration improves but gets stuck before reaching "High"

**Why**: The sensor needs to experience the field from multiple orientations

**Fix**: 
- Rotate the device on ALL three axes simultaneously
- Don't just move up/down—also rotate around the axes
- Walk in a circle while doing figure-8

**Bad technique**: Standing still, only rotating around vertical axis
**Good technique**: Walking + figure-8 motion + rotating on all axes

---

### Mistake 3: Moving Too Fast or Jerkily

**What happens**: Calibration is inconsistent, status jumps between levels

**Why**: Jerky motion confuses the sensor's motion detectors

**Fix**:
- Move smoothly and continuously
- Avoid sudden jerks or fast direction changes
- Think of it like tai chi—slow, flowing motion

**Bad technique**: Rapid, jerky movements
**Good technique**: Smooth, flowing figure-8 motion

---

### Mistake 4: Not Waiting for Initial Warmup

**What happens**: You think calibration failed because status is "Unreliable"

**Why**: The sensor needs 5-10 seconds to warm up and calibrate internal references

**Fix**: Wait at least 10 seconds before starting motion, or just ignore the initial "Unreliable" status

**Timeline**:
- 0-10 sec: Sensor warmup (you can ignore this)
- 10+ sec: Start your figure-8 motion

---

### Mistake 5: Staying in One Location with Lots of Metal Nearby

**What happens**: Calibration improves but seems biased (yaw values odd)

**Why**: Localized magnetic interference (nearby electronics, metal furniture)

**Fix**:
- Move to a different location if possible
- Preferably away from large metal objects
- Indoors away from computers or appliances is fine
- Outdoors in open space is best

---

## If Calibration Gets Stuck

**Symptom**: Status shows "Low" or "Medium" for >30 seconds, won't progress

**Solutions in order of effort:**

1. **Move more vigorously**:
   - Larger figure-8 motions
   - Walk in bigger circles (2-3 meters)
   - Rotate more dramatically

2. **Change location**:
   - Move to a different room
   - Go outside (away from buildings)
   - Avoid areas with lots of metal or electronics

3. **Try again from scratch**:
   - Power off the device completely (5-10 second wait)
   - Power back on
   - Start fresh calibration motion immediately

4. **Check for interference**:
   - Are there microwave ovens, wireless routers, or cell towers nearby?
   - Are you wearing electronic devices (smartwatch, phone)?
   - Try removing potential interference sources

5. **Check power supply**:
   - Is the device powered at stable 5V?
   - Unstable power can cause calibration issues
   - Try different power supply if available

---

## Recalibration: When to Do It Again

**You typically need to recalibrate when:**

- You move to a completely different geographic location (different magnetic anomalies)
- The device has been stationary in a location with local magnetic interference for extended periods
- Yaw values start behaving erratically after working correctly
- You add significant metal components near the sensor

**You do NOT need to recalibrate:**
- Every time you power on (calibration is saved!)
- Every time you move to a new room
- Every time you go from indoors to outdoors
- Between test runs

---

## Next Steps

**After successful calibration:**

1. **Test orientation accuracy**:
   - Hold device in a known orientation
   - Watch yaw angle (should match compass direction)
   - Rotate 90° and verify yaw changes by ~90°

2. **Try GPS lock** (if outdoors):
   - Ensure antenna has clear sky view
   - Wait 30-60 seconds for initial lock
   - Verify latitude/longitude appear in monitor

3. **Log data for analysis**:
   ```bash
   python3 tools/real_time_monitor.py /dev/ttyACM0 --log my_data.jsonl
   ```
   See [Real-Time Monitoring Guide](MONITORING_REAL_TIME_DATA.md)

4. **Deploy to field**:
   - See [Field Deployment Guide](FIELD_DEPLOYMENT.md)
   - Plan location with clear sky (for GPS)
   - Prepare power and storage

---

## Quick Reference: Status Codes

```
Calibration Status Display Legend:

███  = High (fully calibrated, ready to use)
██░  = Medium (good, keep moving for final tuning)
█░░  = Low (starting to improve, keep going)
░░░  = Unreliable (not enough motion data yet)
```

**Yaw stability test:**
```
Before calibration:
  Frame 1: Yaw = 45°
  Frame 2: Yaw = 123°  (big jump!)
  Frame 3: Yaw = 201°  (unstable)

After calibration:
  Frame 1: Yaw = 245°
  Frame 2: Yaw = 244°  (stable)
  Frame 3: Yaw = 245°  (predictable)
```

---

## Troubleshooting Calibration Issues

### "I don't see calibration status on the monitor"

**Possible causes:**
- Monitor not displaying calibration line
- Sensor not responding properly

**Fixes:**
```bash
# Try with verbose output
python3 tools/real_time_monitor.py /dev/ttyACM0 --debug

# Check that BNO085 is working
python3 tools/test_monitor.py
```

### "Calibration status keeps cycling between High and Low"

**Cause**: Unstable power supply or loose connections

**Fixes:**
- Verify all UART connections are tight
- Check 5V power supply stability (use multimeter)
- Try different power supply
- Add capacitor (100-220μF) near Arduino 5V input

### "After calibration, yaw still drifts significantly"

**Cause**: Local magnetic interference or calibration incomplete

**Fixes:**
- Move to different location
- Recalibrate with more vigorous motion
- Ensure you reached "High" status completely
- Check for metal objects on or near device

---

## Frequently Asked Questions (First Calibration)

**Q: Do I need to calibrate every time I power on?**
A: No! Once you reach "High" calibration status, it's automatically saved. Next power-on, you're ready to use.

**Q: Can I move to a different location after calibration?**
A: Yes, the calibration persists. You can move indoors, outdoors, or to a different room without recalibration.

**Q: How long does calibration last?**
A: Indefinitely. The magnetometer calibration is stored in the BNO085's flash memory and doesn't degrade.

**Q: Why does yaw matter?**
A: Yaw is the heading/direction angle. Without calibration, it can be off by 30-90 degrees and drift. With calibration, it's accurate to within a few degrees.

**Q: Can I calibrate indoors?**
A: Yes! Indoors is fine for initial testing. Earth's magnetic field is the same everywhere (locally consistent).

**Q: What if I calibrate in one location and use in another?**
A: Calibration is global (applies to Earth's magnetic field everywhere). Local magnetic anomalies might make accuracy slightly worse in some locations, but recalibration is rarely needed.

---

**Last Updated**: 2025-05  
**Difficulty**: Beginner  
**Time Required**: 5-10 minutes  
**Skill Level**: First-time user
