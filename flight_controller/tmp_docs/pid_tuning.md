# PID Controller Tuning Guide - Using Your Transmitter

## 🎯 Overview

This guide shows you how to tune your flight controller's PID gains to get stable, responsive flight. You'll use the Serial Plotter to visualize the response and adjust gains in `config.h`.

---

## 📚 Understanding PID Control

### What is PID?

**PID** = Proportional + Integral + Derivative

```
           ┌─────────────────────────────┐
User moves │                             │
stick   ───┤  Desired     Controller     ├──→ Motor
           │  Angle/Rate  (PID)          │    Commands
Sensor  ───┤                             │
reading    └─────────────────────────────┘
```

### The Three Terms

**P (Proportional):**
- Responds to current error
- Higher P = stronger correction
- Too high = oscillation
- Too low = slow response

**I (Integral):**
- Responds to accumulated error over time
- Eliminates steady-state error (drift)
- Too high = overshoot and slow oscillation
- Too low = doesn't hold position

**D (Derivative):**
- Responds to rate of change of error
- Dampens oscillations
- Too high = twitchy, noisy
- Too low = overshoot

### Rate vs Angle Control

**Rate Mode (Acro):**
- Stick controls rotation speed
- No self-leveling
- For advanced pilots

**Angle Mode (Stabilize):**
- Stick controls tilt angle
- Self-levels when released
- For beginners

---

## 🎛️ STEP 1: Debug Output Setup

### Enable PID Debug Printing

In `main.cpp`, uncomment these in the `loop()` function:

```cpp
void loop() {
    // Core flight controller ...
    
    // ========== DEBUG OUTPUT ==========
    // Uncomment for PID tuning:
    printRollPitchYaw();   // Shows attitude
    printPIDoutput();      // Shows PID response
    //printGyroData();     // Optional: shows raw gyro
}
```

### Open Serial Plotter

```bash
# Upload code
pio run -e teensy40 -t upload

# Open plotter
pio device monitor
# OR in Arduino IDE: Tools → Serial Plotter
```

### What You'll See

```
roll:0.0 pitch:0.0 yaw:0.0 roll_PID:0.0 pitch_PID:0.0 yaw_PID:0.0

Graphs show:
- roll/pitch/yaw: Current attitude angles
- roll_PID/pitch_PID/yaw_PID: Controller output
```

---

## 🛠️ STEP 2: Initial PID Values

### Default Values (in config.h)

**Rate Controller (Acro Mode):**
```cpp
// Roll/Pitch Rate PID
#define KP_ROLL_RATE   0.15
#define KI_ROLL_RATE   0.15
#define KD_ROLL_RATE   0.0004

#define KP_PITCH_RATE  0.15
#define KI_PITCH_RATE  0.15
#define KD_PITCH_RATE  0.0004

// Yaw Rate PID
#define KP_YAW_RATE    0.30
#define KI_YAW_RATE    0.05
#define KD_YAW_RATE    0.00015
```

**Angle Controller (Stabilize Mode):**
```cpp
// Roll/Pitch Angle PID
#define KP_ROLL_ANGLE  0.20
#define KI_ROLL_ANGLE  0.00
#define KD_ROLL_ANGLE  0.05

#define KP_PITCH_ANGLE 0.20
#define KI_PITCH_ANGLE 0.00
#define KD_PITCH_ANGLE 0.05

// Yaw still uses rate control
```

**Start with these as baseline!**

---

## 🧪 STEP 3: Ground Testing (No Props)

### Safety First

⚠️ **REMOVE PROPELLERS** ⚠️
- ALL tuning done without props
- Secure aircraft so it can't fly
- Keep fingers away from motors

### Test Setup

1. **Secure aircraft:**
   - Zip-tie to test stand
   - OR weight it down
   - Must be able to tilt freely

2. **Power on:**
   - Connect battery
   - USB for serial monitor
   - Transmitter ON

3. **Arm motors:**
   - Throttle minimum
   - Wait for arming sequence

### Baseline Test

1. **Throttle up slowly to 30%**

2. **Move roll stick gently:**
   - Watch Serial Plotter
   - Aircraft should try to roll
   - Note: oscillation? sluggish? overshoots?

3. **Return stick to center:**
   - Watch how fast it settles
   - Any oscillation after settling?

4. **Repeat for pitch**

---

## 📊 STEP 4: Tuning Process

### The Golden Rule

**Tune in this order:**
1. Set I=0, D=0
2. Increase P until oscillation
3. Reduce P by 25%
4. Add D to dampen
5. Add I last

### Step-by-Step Tuning

#### Phase 1: P Gain Only

1. **Set I and D to zero:**
   ```cpp
   // In config.h:
   #define KI_ROLL_RATE 0.0
   #define KD_ROLL_RATE 0.0
   ```

2. **Start with low P:**
   ```cpp
   #define KP_ROLL_RATE 0.05
   ```

3. **Upload and test:**
   ```bash
   pio run -e teensy40 -t upload
   ```

4. **Give roll input:**
   - Move stick, watch plotter
   - Response too slow? Increase P
   - Oscillating? Decrease P

5. **Increase P gradually:**
   ```
   Try: 0.05 → 0.08 → 0.10 → 0.12 → 0.15 → 0.18 → 0.20
   
   Keep increasing until you see oscillation
   Then reduce by 25%
   ```

6. **Find oscillation point:**
   ```
   Example:
   P=0.15: Smooth response ✓
   P=0.20: Slight oscillation
   P=0.25: Strong oscillation ✗
   
   Use P=0.15 (75% of 0.20)
   ```

#### Phase 2: Add D Gain

1. **With P set, start adding D:**
   ```cpp
   #define KP_ROLL_RATE 0.15  // From Phase 1
   #define KD_ROLL_RATE 0.0001
   ```

2. **Upload and test**

3. **Increase D gradually:**
   ```
   Try: 0.0001 → 0.0002 → 0.0003 → 0.0004 → 0.0005
   
   D should reduce overshoot
   Too much D = twitchy, noisy response
   ```

4. **Find sweet spot:**
   ```
   Good D:
   - Smooth movement
   - Little to no overshoot
   - Fast settling time
   
   Too much D:
   - Twitchy, jittery
   - High-frequency noise
   - Motors sound raspy
   ```

#### Phase 3: Add I Gain

1. **With P and D set, add I:**
   ```cpp
   #define KP_ROLL_RATE 0.15  // From Phase 1
   #define KD_ROLL_RATE 0.0004  // From Phase 2
   #define KI_ROLL_RATE 0.05
   ```

2. **Upload and test**

3. **Increase I gradually:**
   ```
   Try: 0.05 → 0.10 → 0.15 → 0.20
   
   I eliminates drift
   Too much I = slow oscillation
   ```

4. **Test I by holding attitude:**
   - Tilt aircraft and hold
   - Should maintain angle without drift
   - Release → should return to center

5. **Signs of too much I:**
   - Slow wobbling (0.5-1 Hz)
   - Overshoots target and bounces back
   - Takes long time to settle

---

## 📈 Reading Serial Plotter

### Perfect Response

```
Roll stick right then center:

roll angle:     ___/‾‾‾‾\___  (follows input, settles quickly)
roll_PID:       ___/‾‾‾‾\___  (smooth curve, no overshoot)
```

**Characteristics:**
- ✓ Fast response (<100ms)
- ✓ No overshoot
- ✓ Settles within 200ms
- ✓ No oscillation

### Under-tuned (P too low)

```
roll angle:     ___/‾‾‾‾‾‾‾‾\___  (slow, sluggish)
roll_PID:       ___/‾‾‾‾‾‾‾‾\___  (weak response)
```

**Fix:** Increase P gain

### Over-tuned (P too high)

```
roll angle:     __/\/\/\/\__  (oscillating)
roll_PID:       __/\/\/\/\__  (unstable)
```

**Fix:** Decrease P gain

### No Damping (D too low)

```
roll angle:     __/‾‾\__/‾\__/\_  (overshoots, bounces)
roll_PID:       __/‾‾\__/‾\__/\_  (ringing)
```

**Fix:** Increase D gain

### Too Much Damping (D too high)

```
roll angle:     __/\/\/\/\/\__  (jittery, noisy)
roll_PID:       __/\/\/\/\/\__  (twitchy)
```

**Fix:** Decrease D gain

### I Term Issues (I too high)

```
roll angle:     __/‾‾‾‾\___/‾\_/  (slow wobble)
roll_PID:       __/‾‾‾‾‾‾‾‾‾‾‾‾  (builds up slowly)
```

**Fix:** Decrease I gain

---

## 🎮 STEP 5: Tuning from Transmitter

### Using Knobs/Switches (Advanced)

Some transmitters let you adjust gains in flight!

**Setup in transmitter:**
1. Assign knob to CH6
2. Map CH6 output: 1000-2000μs → 0.0-1.0 range
3. Read CH6 in code
4. Scale to P gain range

**Example code modification:**
```cpp
void loop() {
    // Read CH6 for live P gain adjustment
    float p_gain_multiplier = (channel_6_pwm - 1000.0) / 1000.0;
    p_gain_multiplier = constrain(p_gain_multiplier, 0.5, 2.0);
    
    // Apply multiplier to base P gain
    float active_kp = KP_ROLL_RATE * p_gain_multiplier;
    
    // Use active_kp in PID calculation
    roll_PID = active_kp * error_roll + Ki * integral + Kd * derivative;
}
```

**Benefits:**
- Real-time tuning
- No need to re-upload code
- Can feel difference immediately

**Cautions:**
- Test on ground first
- Small adjustments only
- Have default failsafe gains

---

## 🚁 STEP 6: First Flight Test

### Pre-flight Checklist

- [ ] Props removed for ground test
- [ ] Good PID response on ground
- [ ] No oscillation at 30-50% throttle
- [ ] Transmitter failsafe set
- [ ] Battery charged
- [ ] Clear, open area
- [ ] No wind

### Test Flight Procedure

1. **Install props (correct direction!)**

2. **Final ground test:**
   - Arm motors
   - Throttle 30%
   - Check no oscillation
   - Disarm

3. **First hover:**
   - Start with 40% throttle
   - Lift just off ground (1-2 inches)
   - Hold for 5 seconds
   - Watch for oscillation
   - Land

4. **Increase altitude gradually:**
   - 6 inches → 1 foot → 2 feet
   - Each time: hover 10 seconds
   - Look for oscillation
   - Listen to motors (raspy = over-tuned)

5. **Test control response:**
   - Small roll left/right
   - Small pitch forward/back
   - Yaw left/right
   - Each movement: smooth? overshoot?

---

## 🔧 STEP 7: Fine Tuning

### Roll/Pitch Tuning

**Too sluggish:**
```cpp
#define KP_ROLL_RATE 0.15  // Increase by 0.02-0.05
#define KP_ROLL_RATE 0.18
```

**Oscillating:**
```cpp
#define KP_ROLL_RATE 0.20  // Decrease by 20-30%
#define KP_ROLL_RATE 0.15
```

**Overshooting:**
```cpp
#define KD_ROLL_RATE 0.0004  // Increase by 0.0001
#define KD_ROLL_RATE 0.0005
```

**Drifting:**
```cpp
#define KI_ROLL_RATE 0.05  // Increase by 0.05
#define KI_ROLL_RATE 0.10
```

### Yaw Tuning

Yaw typically needs different gains:

```cpp
// Yaw usually needs:
// - Higher P than roll/pitch
// - Lower I than roll/pitch
// - Much lower D than roll/pitch

#define KP_YAW_RATE 0.30  // 2x roll P
#define KI_YAW_RATE 0.05  // 1/3 roll I
#define KD_YAW_RATE 0.00015  // 1/3 roll D
```

### Angle Mode Tuning

If using angle mode (self-leveling):

```cpp
// Start conservative:
#define KP_ROLL_ANGLE 0.15
#define KI_ROLL_ANGLE 0.00  // Usually not needed
#define KD_ROLL_ANGLE 0.03

// Increase P until responsive
// Add D if overshooting
```

---

## 🎯 Tuning Goals

### Ideal Response

| Metric | Target | How to Test |
|--------|--------|-------------|
| **Response time** | <100ms | Move stick, measure delay |
| **Settling time** | <200ms | Release stick, time to stable |
| **Overshoot** | <5% | Check plotter for overshoot |
| **Steady-state error** | <2° | Hold attitude, check drift |
| **Oscillation** | None | Listen to motors, watch plotter |

### Performance Indicators

**Good tune:**
- ✓ Smooth, predictable response
- ✓ Quick corrections
- ✓ Holds position without drift
- ✓ Quiet motors (not raspy)
- ✓ Stable in hover
- ✓ Comfortable to fly

**Poor tune:**
- ✗ Sluggish or twitchy
- ✗ Oscillation or bouncing
- ✗ Drifts off position
- ✗ Motors sound raspy/grinding
- ✗ Unstable hover
- ✗ Hard to control

---

## 📊 Example Tuning Session

### Scenario: Quadcopter X Configuration

**Starting values:**
```cpp
#define KP_ROLL_RATE 0.10
#define KI_ROLL_RATE 0.00
#define KD_ROLL_RATE 0.00
```

**Test 1: Ground test, 30% throttle**
- Response: Too sluggish, takes 500ms to respond
- Action: Increase P to 0.15

**Test 2: Ground test, 30% throttle**
- Response: Better, but still slow (300ms)
- Action: Increase P to 0.20

**Test 3: Ground test, 30% throttle**
- Response: Fast, but oscillates at 2Hz
- Action: Reduce P to 0.15 (75% of 0.20)

**Test 4: Ground test, 30% throttle**
- Response: Fast, no oscillation, but overshoots
- Action: Add D = 0.0003

**Test 5: Ground test, 30% throttle**
- Response: Fast, less overshoot, good
- Action: Test at 50% throttle

**Test 6: Ground test, 50% throttle**
- Response: Slight drift over 10 seconds
- Action: Add I = 0.10

**Test 7: Ground test, 50% throttle**
- Response: No drift, but slow wobble
- Action: Reduce I to 0.08

**Final values:**
```cpp
#define KP_ROLL_RATE 0.15
#define KI_ROLL_RATE 0.08
#define KD_ROLL_RATE 0.0003
```

**Flight test:** Stable, responsive, no issues! ✓

---

## 🛡️ Safety Tips

### Always

- ✓ Remove props for initial tuning
- ✓ Secure aircraft for ground tests
- ✓ Start with conservative gains
- ✓ Increase gains slowly
- ✓ Test after every change
- ✓ Have someone watching while you tune
- ✓ Use failsafe on transmitter

### Never

- ✗ Tune with props on (ground testing)
- ✗ Make large gain changes (>50%)
- ✗ Test in high wind
- ✗ Fly without ground testing first
- ✗ Ignore oscillations ("it's fine")

---

## ❓ FAQ

**Q: How long does tuning take?**
A: 30 minutes to 2 hours depending on starting point. Be patient!

**Q: Do I need to tune all axes?**
A: Yes, but roll and pitch usually use same gains. Yaw is different.

**Q: My quad oscillates in flight but not on ground?**
A: Props create vibration. Try increasing D slightly or improving vibration isolation.

**Q: Can I copy someone else's gains?**
A: Good starting point, but every build is different. You'll need to fine-tune.

**Q: What if I can't get rid of oscillation?**
A: Could be mechanical (loose screws, bent prop, imbalanced motors). Check hardware first.

**Q: Should I use rate or angle mode?**
A: Beginners: Angle mode (easier). Advanced: Rate mode (more responsive).

**Q: How often do I need to retune?**
A: Only when you change hardware (props, motors, weight). Otherwise stable.

---

## 📝 Tuning Log Template

Keep notes during tuning!

```
Date: _________
Aircraft: _________
Total weight: _________

Roll Axis:
P=_____ I=_____ D=_____
Notes: ___________________

Pitch Axis:
P=_____ I=_____ D=_____
Notes: ___________________

Yaw Axis:
P=_____ I=_____ D=_____
Notes: ___________________

Test Results:
[ ] No oscillation at hover
[ ] Responsive to inputs
[ ] Settles quickly
[ ] No drift
[ ] Good in wind

Issues: ___________________
Next steps: ___________________
```

---

**Remember: Good PID tuning = safe, fun flying!**

Next: After tuning, test in different flight modes and conditions!