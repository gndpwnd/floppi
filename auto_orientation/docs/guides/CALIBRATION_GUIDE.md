# BNO085 Magnetometer Calibration Guide

A complete guide to calibrating your BNO085 IMU sensor for accurate orientation tracking.

## Overview

The BNO085 contains a magnetometer that measures Earth's magnetic field. To give accurate heading (yaw) measurements, the magnetometer needs calibration to account for:
- Magnetic interference from nearby metals or electronics
- Device-specific magnetic distortions
- Environmental magnetic anomalies

**Good news**: Once calibrated, the calibration is automatically saved and persists across power cycles—no need to recalibrate every time you power on.

---

## Quick Start (2 Minutes)

For experienced users or second-time calibration:

### Step 1: Power On & Monitor
1. Upload firmware to your Arduino Mega
2. Open serial monitor to watch calibration progress:
   ```bash
   minicom -D /dev/ttyACM0 -b 115200
   ```
3. Look for the calibration status line in the output:
   ```
   ... | Cal: Low | ...
   ```

### Step 2: Perform Figure-8 Motion (30-60 seconds)
1. Hold the device naturally in your hand
2. Move it in a smooth figure-8 pattern (like drawing a sideways "8")
3. Rotate the device through different orientations while doing this
4. Watch the serial monitor for calibration status to progress

### Step 3: Verify Completion
Monitor displays:
```
Cal: Unreliable → Cal: Low → Cal: Medium → Cal: High
```

When status reaches **Cal: High**, calibration is complete and automatically saved.

---

## Detailed Calibration Process

### What is Magnetometer Calibration?

The BNO085's magnetometer measures magnetic field strength in three axes (X, Y, Z). Without calibration, these measurements include:
- **Hard iron distortion**: Permanent magnets and magnetic materials (phones, metal frames, electronics)
- **Soft iron distortion**: Ferrous metals that distort the field

Calibration mathematically removes these distortions so the sensor accurately detects Earth's magnetic field direction (true north).

### Calibration Status Explained

The BNO085 reports a calibration status from 0 to 3:

| Status | Name | Meaning | Action |
|--------|------|---------|--------|
| 0 | Unreliable | No motion detected yet | Continue figure-8 motion |
| 1 | Low | Some calibration data collected, minimal motion | Continue moving, be more vigorous |
| 2 | Medium | Good calibration data, approaching convergence | Almost done, keep moving |
| 3 | High | Fully calibrated, ready to use | Stop—calibration complete and saved |

**Note**: Status 0-2 during the first 10 seconds is normal (warming up).

### Optimal Figure-8 Motion

The figure-8 motion helps the magnetometer "see" the magnetic field from multiple angles:

```
        Front View         Side View        Top View
        
           /\                |              
          /  \               |              
         /    \            /   \            
        |      |          |     |           
         \    /            \   /            
          \  /               |              
           \/                |              

Start at bottom, move up and around like drawing a sideways "8"
Continuously rotate the device as you move it
```

**Good technique**:
- Hold device comfortably (don't grip too tight)
- Move smoothly and continuously for 30-60 seconds
- Rotate the device on all axes while moving (roll, pitch, yaw)
- Move in a ~1 meter circle while doing figure-8 pattern
- Don't rush—smooth motion is better than fast motion

**Poor technique** (avoid these):
- Leaving device still or barely moving
- Moving in a straight line instead of figure-8
- Keeping device orientation fixed while moving
- Jerky or erratic movements

### Expected Time Frame

| Scenario | Time | Notes |
|----------|------|-------|
| Clean environment (home/office) | 30-60 seconds | Normal case |
| High magnetic interference (industrial area, near machinery) | 60-120 seconds | May need more motion |
| Optimal conditions (outdoors, away from metal) | 20-30 seconds | Fastest case |

---

## Auto-Save Feature

### How It Works

1. **Detection**: When calibration status reaches 3 (High), the system automatically triggers a save
2. **Saving**: Calibration data is written to Arduino EEPROM (permanent storage)
3. **Timing**: Save completes in < 1 second (you won't notice it)
4. **Verification**: System confirms save success in firmware logs

### What's Saved?

The saved calibration includes:
- Magnetic field coefficients for X, Y, Z axes
- Calibration quality metrics
- Device-specific distortion models
- Checksum for corruption detection

### After Saving

- Device is ready to use immediately
- Calibration persists across power cycles
- No warm-up or re-calibration needed on next power-on
- Sensor starts with full accuracy from startup

---

## When to Re-Calibrate

You should re-calibrate in these situations:

### 1. Moving to a New Location
**Why**: Magnetic declination changes with geographic location. A device calibrated in New York will be off when used in London.

**When**: After moving more than ~500 km
**How**: Follow quick-start procedure again

### 2. Unexpected Calibration Drop
**Symptom**: Calibration was High (3), suddenly drops to Low (1)

**Causes**:
- Electromagnetic interference (EM) from nearby device
- Large metal object placed near sensor
- Electrical equipment turned on nearby

**Solution**:
1. Move away from suspected interference source
2. Wait a few seconds
3. If status doesn't recover, perform quick re-calibration

### 3. Significant Orientation Drift
**Symptom**: Yaw (heading) reading drifts or jumps randomly

**Diagnosis**:
- Rotate sensor in a horizontal circle on a table
- Yaw should smoothly change 0° → 360° → 0°
- If it jumps or drifts erratically, re-calibrate

**Solution**: Follow detailed calibration process again

### 4. Enclosure/Housing Changes
**Why**: Adding metal case or mounting hardware changes magnetic environment

**Solution**: Re-calibrate after adding permanent hardware

---

## Validation - How to Verify Good Calibration

After reaching status "High", verify calibration quality with these tests:

### Test 1: Horizontal Rotation Test
**Purpose**: Verify yaw (heading) is accurate

**Procedure**:
1. Place device on a flat surface (table or floor)
2. Rotate it 90° clockwise → watch for heading change
3. Rotate another 90° → watch for another ~90° heading change
4. Complete 360° rotation, watch heading go from 0° → 360°

**Pass Criteria**:
- Heading changes smoothly without jumps
- Each 90° rotation produces approximately 90° heading change
- No sudden reversals or spikes
- Returns close to starting heading after 360° rotation

**Fail Criteria**:
- Heading jumps or reverses
- Changes are inconsistent (first 90° rotation = 85°, second = 95°)
- Significant drift during rotation

### Test 2: Vertical Rotation Test (Pitch/Roll)
**Purpose**: Verify accelerometer/gyro still work with magnetometer

**Procedure**:
1. Hold device level, note the roll/pitch values
2. Tilt forward 45° → pitch should change to ~45°
3. Tilt backward 45° → pitch should change to ~-45°
4. Tilt right 45° → roll should change to ~45°
5. Tilt left 45° → roll should change to ~-45°

**Pass Criteria**:
- Values change smoothly and proportionally
- No sudden jumps or reversals
- Values are stable when holding still

### Test 3: Stationary Accuracy
**Purpose**: Check for drift when device is stationary

**Procedure**:
1. Place device on table, note heading
2. Wait 30 seconds, check heading again
3. Repeat 2-3 times over several minutes

**Pass Criteria**:
- Heading stays within ±2° of initial value
- No slow drift or gradual change
- Random variations less than 1°

### Test 4: Check Calibration Status Display
**Purpose**: Verify system recognizes good calibration

**Procedure**:
1. During calibration, watch calibration status
2. Should be: Unreliable → Low → Medium → High
3. Once High (3), leave device stationary for 10 seconds
4. Check that status remains High

**Pass Criteria**:
- Reaches status 3 (High)
- Status remains at 3 when stationary
- No dropping back to 2 or 1

---

## Troubleshooting

### Calibration Status Stuck at 0 (Unreliable)

**Symptom**: Device is powered on, but calibration status never progresses past 0

**Causes**:
- Device not moving at all
- Motion is too slight or slow
- Device overheating during startup

**Solutions**:
1. **Ensure motion**: Device must move through space, not just rotate
2. **Increase vigor**: Make larger, faster figure-8 motions
3. **Check device**: Feel if device is warm—if too hot, let cool for 1-2 minutes
4. **Reset if needed**: Power cycle device and try again

**Expected**: Status 0 is normal for first 2-5 seconds during warm-up

---

### Calibration Status Stuck at 1 (Low)

**Symptom**: Calibration reaches Low (1) but won't progress to Medium (2) after 30+ seconds

**Causes**:
- Insufficient motion variety (too much linear motion, not enough rotation)
- Slow or gentle movements
- Strong magnetic interference preventing convergence
- Magnetic materials in immediate surroundings

**Solutions** (in order):
1. **Perform more vigorous motion**: Make larger, faster figure-8 patterns
2. **Add rotation**: Roll, pitch, and yaw the device while moving
3. **Change location**: Move to a cleaner magnetic environment
   - Away from computers, phones, machinery
   - Outdoors is often better than indoors
   - Avoid large metal structures nearby
4. **Check surroundings**: Look for hidden magnetic sources:
   - Refrigerators, microwaves, speakers
   - Electrical panels, metal shelving
   - Motors, transformers, power supplies
5. **Clear EEPROM**: If device has old bad calibration saved:
   ```cpp
   // In firmware, uncomment this in setup():
   // clearCalibrationFromEEPROM();  // Force fresh calibration
   ```

**Time frame**: Usually progresses from Low to Medium within 30-60 seconds of continuous figure-8 motion

---

### Calibration Reaches Medium (2) But Won't Reach High (3)

**Symptom**: Gets to status 2 and stays there despite continued motion

**Causes**:
- Magnetic interference is near the threshold
- Motion pattern is becoming repetitive (sensor needs new angles)
- Device proximity to metal is affecting convergence

**Solutions**:
1. **Vary motion pattern**: Instead of same figure-8, try:
   - Figure-8 with different orientations
   - Circular motions at different heights
   - Slow spirals upward/downward
2. **Continue longer**: Sometimes takes 60-90 seconds instead of 30-60
3. **Move away from metal**: Check for nearby metal objects:
   - Take device 2+ meters away from electronics
   - Go outdoors if possible
   - Avoid being near desks with multiple devices
4. **Try location change**: Move to a different room or area

**Note**: Medium calibration (status 2) is still usable but less accurate than High (3)

---

### Calibration Drops After Being High (3)

**Symptom**: Reached status High, then suddenly dropped to Low/Medium

**Causes** (in order of likelihood):
1. **Electromagnetic interference** (most common)
   - Device powered on nearby (laptop, phone, drill, etc.)
   - Electrical equipment started running
   - High-power device plugged in
2. **Magnetic material moved near sensor**
   - Large speaker brought close
   - Metal object placed on table
   - Device moved into metal cabinet
3. **Sensor overheating** (less likely)
   - Extended motion in warm room
   - Thermal spike from intensive processing

**Solutions**:
1. **Identify interference source**: Look around for recently powered devices
   - Unplug nearby electronics
   - Move device 2+ meters away
   - Check if interference source can be turned off
2. **Wait for recovery**: Often recovers automatically when source is removed
   - Wait 30-60 seconds
   - Device may re-calibrate autonomously
3. **Manual re-calibration**: If status doesn't recover:
   - Move to clean magnetic environment
   - Perform quick figure-8 motion for 30-60 seconds
   - Watch status progress back to High

**Note**: Once recovered or re-calibrated, calibration usually stays stable

---

### Yaw Reading is Erratic or Jumps

**Symptom**: Heading (yaw) value jumps around or drifts randomly, even with High calibration

**Possible Causes**:
1. **Magnetometer saturation** (most likely)
   - Temporary strong interference
   - Device in high-magnetic environment
2. **Magnetometer not actually at status 3** (check display)
3. **Firmware filtering issue** (less likely)

**Diagnosis**:
1. Check calibration status display—is it really showing "High" (3)?
2. Perform horizontal rotation test (see Validation section)
3. Note if jumps correlate with nearby devices turning on/off

**Solutions**:
1. **Check environment**: Identify and remove interference sources
2. **Test in cleaner location**: Move to outdoor area or quiet room
3. **Verify calibration**: Re-run quick-start calibration procedure
4. **Report if persists**: If problem continues after clean environment test

---

### Device Never Initializes (Solid Red LED or No Output)

**Note**: This is a hardware issue, not calibration. See QUICK_TEST.md

**Likely causes**:
- BNO085 not connected or powered
- Serial connection issue
- Firmware not uploaded

**Next steps**: Follow hardware testing section in QUICK_TEST.md

---

## Multi-Device Notes

If you're using multiple BNO085 sensors:

### Each Device Needs Individual Calibration
- Magnetic distortions are device-specific
- Calibration can't be transferred between devices
- Mount/enclosure changes calibration even on same device

### Calibration Procedure for Multiple Devices
1. **Device A**: Upload firmware, calibrate, let save complete
2. **Device B**: Upload firmware, calibrate, let save complete
3. **Device N**: Repeat...

Each device saves its own calibration independently.

### Future Enhancement: Save/Load Calibration Files
In a future version, you'll be able to:
- Export calibration from a device to a file
- Import saved calibration to another device in same location
- Version control calibration data
- Maintain calibration history

For now, calibration data is stored locally in each device's EEPROM.

---

## Understanding Calibration in Context

### Why Heading Matters

The BNO085 measures three orientations:
- **Roll**: Rotation left/right (around forward axis)
- **Pitch**: Rotation forward/backward (around right axis)
- **Yaw/Heading**: Rotation around vertical axis (direction you're facing)

Roll and pitch can be measured by accelerometer (gravity) and gyroscope (rotation rate). **Heading requires magnetometer calibration** because:
- Accelerometer can't distinguish north from south
- Gyroscope drifts over time
- Magnetometer is the only long-term reference for heading

### Calibration Quality Metrics

| Quality Level | Status | Heading Accuracy | Use Case |
|---------------|--------|------------------|----------|
| Unreliable | 0 | ±45° or worse | Not usable |
| Low | 1 | ±20-30° | Testing only |
| Medium | 2 | ±5-10° | Basic applications |
| High | 3 | ±1-2° | Navigation, robotics |

---

## Step-by-Step Calibration (Full Procedure)

Complete walkthrough for first-time or detailed calibration:

### Preparation (1 minute)
- [ ] Ensure BNO085 is mounted and powered
- [ ] Open serial monitor to watch progress
- [ ] Clear workspace of loose metal objects (phones, watches, keys)
- [ ] Identify target location (preferably magnetic-clean environment)

### Motion Phase (30-90 seconds)
- [ ] Perform figure-8 motion as described above
- [ ] Watch status progress: 0 → 1 → 2 → 3
- [ ] Continue motion until reaching status 3 (High)
- [ ] Once High, stop moving and hold device stationary

### Save Phase (1 second)
- [ ] Observe firmware message confirming calibration saved
- [ ] Device is now ready to use
- [ ] Calibration persists across power cycles

### Validation Phase (2 minutes)
- [ ] Run validation tests from "Validation" section above
- [ ] Perform horizontal rotation test
- [ ] Check yaw readings are consistent
- [ ] Note any issues for troubleshooting

### Documentation
- [ ] Record date and location of calibration
- [ ] Note any magnetic interference observed
- [ ] Keep calibration test results for reference

---

## FAQ

**Q: Do I need to calibrate every time I power on?**  
A: No! Calibration is saved automatically and persists across power cycles. You only need to recalibrate if you move locations or notice accuracy issues.

**Q: How often should I calibrate?**  
A: Typically once per location. If you move to a new geographic area, recalibrate. If you move the device within the same building, no recalibration needed.

**Q: What if the device is in a case or enclosure?**  
A: Calibrate the device in its final mounted/enclosure configuration. Magnetic properties change with housing, so calibrate how you'll actually use it.

**Q: Can I save/load calibration to a file?**  
A: Not yet, but this is planned for a future version. Currently, calibration is stored in each device's EEPROM.

**Q: Is recalibration harmful or a waste of time?**  
A: Not at all. Recalibration is quick and harmless. If you suspect accuracy issues, recalibrating takes only a minute.

**Q: My device won't go above Medium (2) calibration. Is it broken?**  
A: Not necessarily. Medium calibration is still useful—it gives ±5-10° heading accuracy. But first try recalibrating in a cleaner magnetic environment (outdoors or away from electronics).

**Q: The serial monitor shows gibberish instead of data.**  
A: Check the baud rate. BNO085 uses 115200 baud. If monitor shows garbage, try: `minicom -D /dev/ttyACM0 -b 115200`

**Q: How long does calibration data stay valid?**  
A: Indefinitely, unless the device's magnetic environment changes significantly. Temperature variations and aging have minimal effect on magnetometer calibration.

**Q: Can I use uncalibrated data?**  
A: Yes, but heading readings will be inaccurate (±20-45° off). Roll and pitch are mostly unaffected since they don't require magnetometer calibration.

---

## Technical Reference

For developers integrating this system:

### Key Files
- `src/sensors/bno085_calibration.h` - Calibration data structures
- `src/config/calibration_storage.h` - EEPROM save/restore API
- `src/sensors/bno085.cpp` - BNO085 driver with calibration integration

### EEPROM Layout
```
Offset  | Bytes | Purpose
--------|-------|------------------------------------------
0x00    | 1     | Valid marker (0xCA = valid, 0xFF = empty)
0x01    | 1     | Data length (1-252 bytes)
0x02    | 1     | Version (0x01)
0x03    | 1     | CRC8 checksum
0x04    | 252   | Calibration payload
```

### Serial Commands (when implemented)
- `CAL_STATUS` - Current calibration status
- `CAL_SAVE` - Manually save calibration
- `CAL_RESTORE` - Restore from EEPROM
- `CAL_CLEAR` - Erase saved calibration (force fresh)

### Integration Example
See `src/sensors/bno085_integration_example.h` for code examples.

---

## Support & Resources

### In This Documentation
- **QUICK_TEST.md** - Hardware testing and initial validation
- **IMPLEMENTATION_NOTES.md** - Technical implementation details
- **CALIBRATION_README.md** - Framework and persistence implementation

### External Resources
- **BNO085 Datasheet**: https://www.adafruit.com/product/4754
- **Adafruit BNO08x Library**: https://github.com/adafruit/Adafruit_BNO08x_Arduino
- **SH-2 Protocol Guide**: Included in BNO085 datasheet appendix

### Troubleshooting Workflow
1. Check calibration status (is it really at 3?)
2. Run validation tests (horizontal rotation test)
3. Identify magnetic interference sources
4. Recalibrate in cleaner environment
5. Document results for reference

---

**Last Updated**: 2026-05-05  
**Status**: User-Ready  
**Audience**: End users, integrators, developers  
**Next Steps**: Hardware testing and validation
