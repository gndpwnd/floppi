# 🔧 TROUBLESHOOTING GUIDE

Complete problem-solving guide for common flight controller issues.

---

## 📋 Troubleshooting Index

**Quick Navigation:**
- [💻 Upload/Compilation Issues](#-uploadcompilation-issues)
- [📡 Receiver Problems](#-receiver-problems)
- [🧭 IMU/Sensor Issues](#-imusensor-issues)
- [⚡ Motor/ESC Problems](#-motoresc-problems)
- [🛫 Flight Issues](#-flight-issues)
- [🔋 Power Problems](#-power-problems)
- [🐛 Code/Software Issues](#-codesoftware-issues)

---

## 💻 Upload/Compilation Issues

### Error: "Teensy not found" or "No Teensy found on USB"

**Symptoms:**
- Upload fails immediately
- Error message about USB device not found
- PlatformIO can't detect board

**Solutions:**

1. **Check USB cable:**
   - Use data cable (not charge-only)
   - Try different USB port
   - Try different cable

2. **Install Teensy drivers:**
   ```bash
   # Windows: Download and run Teensy Loader
   # Mac: Install via Homebrew
   brew install teensy-cli
   
   # Linux: Add udev rules
   sudo cp 99-teensy.rules /etc/udev/rules.d/
   sudo udevadm control --reload-rules
   ```

3. **Press reset button on Teensy:**
   - Small button on board
   - Press once, LED should blink
   - Try upload again immediately

4. **Check device manager (Windows):**
   - Should see "USB Serial Device" or "Teensy"
   - If "Unknown Device", reinstall drivers

---

### Error: "Compilation failed" with lots of errors

**Symptoms:**
- Code won't compile
- Multiple error messages
- Missing files or libraries

**Solutions:**

1. **Check PlatformIO installation:**
   ```bash
   pio --version
   # Should show version number
   ```

2. **Clean and rebuild:**
   ```bash
   pio run -e teensy40 -t clean
   pio run -e teensy40
   ```

3. **Check file structure:**
   ```
   flight_controller/
   ├── include/
   │   ├── config.h
   │   ├── pin_definitions.h
   │   └── calibration.h
   ├── lib/
   │   ├── RadioComm/
   │   └── Calibration/
   ├── src/
   │   └── main.cpp
   └── platformio.ini
   ```

4. **Verify platformio.ini:**
   - Correct board selected (`teensy40` or `teensy41`)
   - All required libraries listed
   - No syntax errors

---

### Error: "Multiple definitions" or "Undefined reference"

**Cause:** Include files (.h) have function implementations instead of just declarations.

**Solution:**
- Move function code from .h files to .cpp files
- Use header guards in all .h files:
  ```cpp
  #ifndef FILENAME_H
  #define FILENAME_H
  // declarations here
  #endif
  ```

---

## 📡 Receiver Problems

### Receiver LED not lighting up

**Cause:** No power to receiver

**Check:**

1. **Wiring:**
   - Red wire to VIN (or 5V)
   - Black wire to GND
   - Measure voltage: Should be 4.5V-5.5V

2. **Power source:**
   - USB connected?
   - BEC working? (if using battery power)

3. **Receiver itself:**
   - Try different receiver (if available)
   - Check for physical damage

---

### Receiver LED blinking (won't bind)

**Symptoms:**
- LED blinks continuously
- Won't bind to transmitter
- No response to TX controls

**Solutions:**

1. **Verify TX is in SBUS mode:**
   - Menu → System → RX Setup → SBUS
   - Power cycle TX after changing

2. **Binding procedure:**
   - TX OFF
   - Hold RX bind button
   - Power on RX (plug USB)
   - LED flashes rapidly
   - TX ON
   - Wait for solid LED

3. **Try different bind method:**
   - Some RX: tap bind button 5x quickly
   - Some RX: hold for 10+ seconds
   - Check RX manual for specific procedure

4. **Check antenna:**
   - Both antennas connected?
   - Not damaged or bent?

---

### Receiver bound but no signal in Serial Monitor

**Symptoms:**
- RX LED solid when TX on
- Channels show 1500 constantly
- No response to stick movement

**Solutions:**

1. **Check SBUS wiring:**
   - White wire to Pin 21 (RX5)
   - NOT Pin 20 (TX5)
   - Secure connection

2. **Verify protocol in code:**
   ```cpp
   // In config.h:
   #define USE_SBUS_RECEIVER  // Should be uncommented
   ```

3. **Check Serial port:**
   ```cpp
   // In radioComm.cpp:
   #define SBUS_SERIAL_PORT Serial5  // Should be Serial5 for Pin 21
   ```

4. **Test with different receiver** (if available)

---

### Channels stuck at 1000μs or 2000μs

**Cause:** Failsafe activated or bad binding

**Solutions:**

1. **Turn on transmitter** (obvious but often forgotten!)

2. **Check TX battery** (low battery = weak signal = failsafe)

3. **Move closer to RX** (test within 1-2 meters)

4. **Rebind RX to TX:**
   - Follow binding procedure again
   - Sometimes a fresh bind fixes issues

5. **Check failsafe settings in config.h:**
   ```cpp
   #define FAILSAFE_THROTTLE 1000  // Min throttle on signal loss
   ```

---

## 🧭 IMU/Sensor Issues

### IMU not detected / "MPU6050 connection failed"

**Symptoms:**
- Error message on startup
- LED blinks 3x rapidly, then stops
- Serial Monitor shows connection error

**Solutions:**

1. **Check I2C wiring:**
   ```
   MPU6050   →   Teensy
   VCC       →   3.3V (NOT 5V!)
   GND       →   GND
   SDA       →   Pin 18
   SCL       →   Pin 19
   ```

2. **Measure voltages:**
   - VCC pin: Should be 3.3V
   - GND pin: Should be 0V

3. **Test I2C communication:**
   - Use I2C scanner sketch
   - Should detect device at 0x68 or 0x69

4. **Try different I2C address:**
   - Connect AD0 pin to 3.3V (changes address to 0x69)
   - Update code if needed:
     ```cpp
     #define MPU6050_ADDRESS 0x69
     ```

5. **Check for shorts:**
   - Disconnect all wires
   - Measure resistance between VCC and GND
   - Should be >10kΩ (not shorted)

---

### IMU data very noisy or jumping around

**Symptoms:**
- GyroX/Y/Z values jump ±10-50°/s
- AccX/Y/Z unstable
- Roll/pitch/yaw oscillate wildly

**Solutions:**

1. **Check physical mounting:**
   - IMU secured firmly (no wobbling)
   - Mounted away from motors
   - Use foam tape for vibration dampening

2. **Check for EMI:**
   - Move IMU wires away from motor wires
   - Add ferrite beads on I2C wires
   - Twist SDA and SCL wires together

3. **Increase filtering:**
   ```cpp
   // In config.h:
   #define B_ACCEL 0.08  // Lower = more filtering
   #define B_GYRO 0.06
   ```

4. **Power supply:**
   - Add 100nF + 10μF caps at MPU6050 VCC/GND
   - Measure VCC with oscilloscope - should be clean
   - Check for voltage drops

---

### IMU values drift over time

**Symptoms:**
- Roll/pitch slowly changes even when still
- Gyro values not centered at 0
- Aircraft drifts in one direction

**Solutions:**

1. **Re-run calibration** (`./tools/calibrate.sh`, option 7, or type `i`):
   - Must be perfectly still
   - Must be perfectly level
   - Temperature stable (wait 5min after power-on)

2. **Check temperature:**
   - MPU6050 drifts with temperature changes
   - Let warm up for 5 minutes before calibrating
   - Recalibrate if flying in very different temps

3. **Increase Madgwick beta:**
   ```cpp
   #define MADGWICK_BETA 0.06  // Trusts accel more
   ```

4. **Check for magnetic interference:**
   - Steel table can affect results
   - Move away from speakers, magnets
   - Use non-ferrous screws if possible

---

### Roll and pitch are swapped or inverted

**Cause:** IMU mounted rotated or upside-down

**Recommended solution:** Use the automatic orientation detection command:

```bash
# Using calibrate.sh (option 9):
./tools/calibrate.sh /dev/ttyACM0 orientation

# Or type 'o' in the serial monitor
```

This auto-detects your IMU mounting and generates the correct axis transformation code. See [2_calibration_guide.md](2_calibration_guide.md) Part 3 for details.

**Manual solution** (if auto-detection isn't available): Apply axis transformation in `getIMUdata()`:

```cpp
// For 90° clockwise rotation:
float AccX_temp = AccY;
float AccY_temp = -AccX;
AccX = AccX_temp;
AccY = AccY_temp;
// (and same for GyroX/GyroY)
```

---

## ⚡ Motor/ESC Problems

### Motors won't arm

**Symptoms:**
- No "ARMED" message in Serial Monitor
- Motors don't spin at all
- Throttle up has no effect

**Solutions:**

1. **Check arming conditions:**
   - Throttle below 1050μs
   - CH5 switch LOW (<1500μs)
   - Watch Serial Monitor for status

2. **Verify arming code:**
   ```cpp
   // In main.cpp:
   void armedStatus() {
       bool throttle_low = (channel_3_pwm < 1050);
       bool throttle_cut_off = (channel_5_pwm < 1500);
       
       if (!armedFly && throttle_low && throttle_cut_off) {
           armedFly = true;
           Serial.println(F("*** ARMED ***"));
       }
   }
   ```

3. **Test receiver channels:**
   ```cpp
   printRadioData();
   ```
   - CH3 should go <1050 with throttle down
   - CH5 should go <1500 with switch low

4. **Check Serial Monitor:**
   - Look for arming messages
   - Look for error messages

---

### Motors spin but won't stop (throttle cut not working)

**Cause:** Throttle cut logic not activating

**Check:**

1. **CH5 switch:**
   - printRadioData() and verify CH5 changes
   - Should be >1500 for throttle cut

2. **Throttle cut code:**
   ```cpp
   void throttleCut() {
       if ((channel_5_pwm > 1500) || !armedFly) {
           // Motors to minimum
           m1_command_PWM = 1000;  // Or 125 for OneShot
           // ... etc
       }
   }
   ```

3. **Emergency stop:**
   - Unplug battery immediately if motors won't stop!
   - Fix code before reconnecting power

---

### One or more motors not spinning

**Symptoms:**
- 3 motors spin, 1 doesn't
- Motor spins manually but not from controller
- Specific motor always problematic

**Solutions:**

1. **Check wiring:**
   - ESC signal wire connected to correct pin
   - Check pin assignment in `pin_definitions.h`

2. **Swap ESC:**
   - Swap ESC with working motor
   - If problem follows ESC → bad ESC
   - If problem stays → wiring or code issue

3. **Test ESC separately:**
   - Connect ESC to servo tester or RX
   - Should spin when signal applied
   - If not → replace ESC

4. **Check motor:**
   - Spin motor by hand (should be smooth)
   - Check motor wires (not broken)
   - Measure motor resistance (should be 0.1-1Ω)

---

### Motors spinning in wrong direction

**Cause:** ESC or motor wiring phase sequence wrong

**Solution:**

**For BLHeli ESCs:**
- Swap any 2 of 3 motor wires
- Example: Swap blue and yellow wires

**For ESCs with programming:**
- Use BLHeli configurator
- Set motor direction to reversed

**⚠️ CRITICAL:** All motors must spin correct direction before attempting flight!

```
Standard Quad X:
    Front
     ↑
  1     2    ← CCW  CW
    \ /
    / \
  4     3    ← CW   CCW
```

---

### Motors beep continuously / won't initialize

**Symptoms:**
- Continuous beeping pattern
- Motors won't spin
- ESCs not arming

**Solutions:**

1. **ESC calibration needed:**
   ```
   a) Throttle full up
   b) Connect battery
   c) ESCs beep
   d) Throttle full down
   e) ESCs beep again - calibrated
   ```

2. **Check PWM signal:**
   - Minimum: 1000μs (or 125μs for OneShot)
   - Maximum: 2000μs (or 250μs for OneShot)
   - Some ESCs: Need 950-2000μs range

3. **Check PWM frequency:**
   ```cpp
   // In commandMotors():
   analogWriteFrequency(MOTOR_PIN_1, 250);  // 250Hz for standard PWM
   ```

4. **Power cycle:**
   - Disconnect battery
   - Wait 10 seconds
   - Reconnect
   - Try again

---

## 🛫 Flight Issues

### Aircraft flips immediately on takeoff

**Cause:** Motor direction wrong OR props wrong

**Solutions:**

1. **Verify motor directions** (props off!)
   ```
   Motor 1 (FL): CCW
   Motor 2 (FR): CW
   Motor 3 (BR): CCW
   Motor 4 (BL): CW
   ```

2. **Check propeller directions:**
   - CCW motors: CCW props (pusher)
   - CW motors: CW props (puller)
   - Props have markings (look for arrows)

3. **Test on ground:**
   - Arm with props OFF
   - Tilt aircraft right → Right motors should speed up
   - Tilt left → Left motors speed up
   - Tilt forward → Front motors speed up

---

### Aircraft oscillates rapidly (5-10Hz)

**Cause:** P gain too high

**Solution:**

```cpp
// In config.h:
#define KP_ROLL_RATE 0.15  // Decrease by 20-30%
#define KP_ROLL_RATE 0.10  // Try this

#define KP_PITCH_RATE 0.10  // Same for pitch
```

**Test incrementally** - decrease by 0.02 at a time until oscillation stops.

---

### Aircraft wobbles slowly (<1Hz)

**Cause:** I gain too high

**Solution:**

```cpp
// In config.h:
#define KI_ROLL_RATE 0.15  // Decrease by 50%
#define KI_ROLL_RATE 0.08  // Try this

#define KI_PITCH_RATE 0.08  // Same for pitch
```

---

### Aircraft drifts in one direction

**Possible causes:**

1. **IMU not calibrated:**
   - Re-run auto-calibration
   - Verify board was level

2. **I gain too low:**
   ```cpp
   #define KI_ROLL_RATE 0.20  // Increase I gain
   ```

3. **Wind:**
   - Normal in wind - pilot must correct
   - GPS/barometer needed for auto-position hold

4. **Center of gravity off:**
   - Battery moved?
   - Payload unbalanced?
   - Check aircraft balance

---

### Sluggish response / slow to react

**Cause:** P gain too low OR D gain too high

**Solutions:**

```cpp
// Increase P gain:
#define KP_ROLL_RATE 0.20  // Was 0.15

// Decrease D gain:
#define KD_ROLL_RATE 0.0002  // Was 0.0004
```

**Test after each change** - increase P until responsive, but not oscillating.

---

### Aircraft won't hold altitude

**Cause:** No barometer (altitude hold requires extra sensor)

**This is normal!** - Manual throttle control required.

**To add altitude hold:**
- Install BMP280/MS5611 barometer
- Implement altitude PID controller
- See advanced features documentation

---

## 🔋 Power Problems

### Teensy won't power on

**Check:**

1. **USB cable:**
   - Data cable (not charge-only)
   - Try different cable
   - Try different USB port

2. **Power LED:**
   - Should light up immediately
   - If not, check for shorts

3. **Measure voltages:**
   - 3.3V pin: Should be 3.3V
   - 5V pin: Should be 5V
   - If not, Teensy may be damaged

---

### System resets randomly / brownouts

**Symptoms:**
- Random resets during operation
- "Brownout detected" in Serial Monitor
- System freezes

**Causes & Solutions:**

1. **Insufficient power:**
   - Use 5V BEC rated for 3A+
   - Check BEC output voltage (should be 4.9V-5.1V)
   - Add large capacitor (470μF-1000μF) at Teensy VIN

2. **Voltage drop:**
   - Check wire gauge (18AWG minimum for power)
   - Check connections (crimp or solder, not twist)
   - Measure voltage at Teensy VIN under load

3. **EMI/noise:**
   - Add LC filter on power line
   - Ferrite beads on power wires
   - Separate grounds (star topology)

---

### Battery drains quickly

**Check:**

1. **Current draw:**
   - Measure with multimeter
   - Should be <200mA without motors
   - If >500mA, something is wrong

2. **Short circuits:**
   - Disconnect everything
   - Measure each subsystem separately
   - Find culprit

3. **Battery health:**
   - Check cell voltages (should be equal)
   - Charge/discharge test
   - Replace if old/damaged

---

## 🐛 Code/Software Issues

### Serial Monitor shows gibberish

**Cause:** Wrong baud rate

**Solution:**
- Set Serial Monitor to 115200 baud
- Verify code has `Serial.begin(115200);`

---

### Loop rate very slow (<1000Hz)

**Symptoms:**
- `printLoopRate()` shows >1000μs
- Sluggish control response
- Serial Monitor flooded with messages

**Solutions:**

1. **Disable debug prints:**
   - Comment out ALL printXXX() functions
   - Each print adds ~50-100μs

2. **Check for delays:**
   - Search code for `delay()`
   - Remove or minimize delays

3. **Optimize code:**
   - Use `float` instead of `double`
   - Pre-calculate constants
   - Minimize Serial.print() calls

---

### Code compiles but doesn't work as expected

**Debug steps:**

1. **Add debug prints strategically:**
   ```cpp
   Serial.println(F("Checkpoint 1"));
   ```

2. **Check variable values:**
   ```cpp
   Serial.print(F("variable = "));
   Serial.println(variable);
   ```

3. **Test subsystems individually:**
   - Test receiver first
   - Test IMU separately
   - Test motors one at a time

4. **Compare to working example:**
   - Use original dRehmFlight code as reference
   - Check function implementations
   - Verify order of operations

---

## 🆘 Emergency Procedures

### Aircraft won't disarm

**Immediate action:**
1. **Activate throttle cut** (CH5 switch HIGH)
2. If that fails: **Unplug battery**
3. Never try to grab spinning props!

---

### Aircraft flying away (runaway)

**Immediate action:**
1. **Throttle cut** (CH5 HIGH)
2. If no response: **Turn off transmitter** (triggers failsafe)
3. Let aircraft crash rather than chase it

---

### Crash recovery checklist

**After a crash:**
- [ ] Check for damaged props (replace if any damage)
- [ ] Check motor mounts (tighten screws)
- [ ] Check ESC/motor wires (no breaks)
- [ ] Re-calibrate IMU (may have shifted)
- [ ] Test arming on ground
- [ ] Test motors at low throttle (props off)
- [ ] Check frame for cracks
- [ ] Hover test before full flight

---

## 📞 Getting Help

### Information to Provide

When asking for help, include:

1. **Hardware:**
   - Teensy version (4.0 or 4.1)
   - IMU model (MPU6050, MPU9250)
   - Receiver model (FS-iA6B)
   - Aircraft type (quad, hex, plane)

2. **Software:**
   - Which branch/version of code
   - Recent changes made
   - Full error message (copy/paste)

3. **What you've tried:**
   - Steps already taken
   - Results of troubleshooting
   - When problem started

4. **Serial Monitor output:**
   ```
   [Copy and paste output]
   ```

### Resources

**GitHub Issues:**
- [GitHub repo](https://github.com/your-repo)
- Check existing issues first
- Use issue templates

**Forums:**
- dRehmFlight Google Group
- RCGroups Forums
- Reddit r/Multicopter

**Documentation:**
- [Quickstart Guide](0_quickstart.md)
- [Hardware Setup](1_hardware_setup.md)
- [Calibration Guide](2_calibration_guide.md)

---

## ✅ Preventive Maintenance

### Before Each Flight

- [ ] Check battery voltage (>11.1V for 3S)
- [ ] Check prop tightness
- [ ] Verify arming/disarming works
- [ ] Check for loose wires
- [ ] Quick visual inspection

### Weekly

- [ ] Recalibrate IMU if flying conditions changed
- [ ] Check for prop damage
- [ ] Tighten all screws
- [ ] Clean dust/debris
- [ ] Check solder joints

### Monthly

- [ ] Full recalibration (IMU + radio)
- [ ] Check motor bearings (smooth rotation?)
- [ ] Inspect ESCs for damage
- [ ] Check battery cell balance
- [ ] Update firmware if available

**Preventive maintenance = fewer mid-air failures!** ✅