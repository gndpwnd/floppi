📄 Updated demo_usage.md (with arming procedure added):
markdown
# Complete Setup & Usage Guide - Comment/Uncomment Debug Method

## 🎯 Philosophy: ONE main.cpp, NOT Multiple Test Files

You have **ONE complete main.cpp** with all functionality. You test different subsystems by **commenting/uncommenting debug print functions** in the loop. This is how the original dRehmFlight works!

---

## 📁 Files Provided

### Core Files (Use These)

1. **main_COMPLETE.cpp** → `src/main.cpp`
   - Complete flight controller
   - All functionality built-in
   - Comment/uncomment debug sections

2. **radioComm_CORRECTED.cpp** → `lib/RadioComm/radioComm.cpp`
   - All receiver protocols

3. **radioComm_CORRECTED.h** → `lib/RadioComm/radioComm.h`
   - Header file

### Guides (Read These)

4. **MPU6050_CALIBRATION_GUIDE.md** - IMU calibration using Serial Plotter
5. **RECEIVER_BINDING_GUIDE.md** - Bind FS-iA6B using Teensy USB power
6. **PID_TUNING_GUIDE.md** - Tune flight controller using transmitter
7. **COMPLETE_WIRING_GUIDE.md** - Physical wiring diagrams

---

## 🔧 HOW THE DEBUG SYSTEM WORKS

### The Debug Print Functions

In `main.cpp`, these functions exist (scroll to bottom):

```cpp
// Debug print functions (DO NOT affect flight controller)
void printRadioData();      // Print receiver channels
void printDesiredState();   // Print desired roll/pitch/yaw
void printGyroData();       // Print gyro X/Y/Z
void printAccelData();      // Print accel X/Y/Z
void printMagData();        // Print mag X/Y/Z (MPU9250 only)
void printRollPitchYaw();   // Print attitude angles
void printPIDoutput();      // Print PID controller outputs
void printMotorCommands();  // Print motor PWM commands
void printServoCommands();  // Print servo PWM commands
void printLoopRate();       // Print loop timing
The Debug Section in loop()
cpp
void loop() {
    // ========== CORE FLIGHT CONTROLLER (ALWAYS RUNS) ==========
    getCommands();
    failSafe();
    getIMUdata();
    Madgwick6DOF(...);
    getDesState();
    controlRATE();  // or controlANGLE()
    controlMixer();
    scaleCommands();
    throttleCut();
    commandMotors();
    
    // ========== DEBUG OUTPUT (UNCOMMENT WHAT YOU WANT) ==========
    // ⚠️ WARNING: Debug prints WILL reduce loop rate!
    //   - No debug:     ~2000Hz (500µs loop time)  
    //   - 1-2 prints:   ~1800Hz (555µs loop time)
    //   - 3+ prints:    ~1500Hz (666µs loop time)
    //   - All prints:   ~1000Hz (1000µs loop time)
    // 
    // For TUNING: Some debug is OK
    // For FLIGHT: Comment out ALL debug for max performance
    
    //printRadioData();
    //printDesiredState();
    //printGyroData();
    //printAccelData();
    //printMagData();
    //printRollPitchYaw();
    //printPIDoutput();
    //printMotorCommands();
    //printServoCommands();
    //printLoopRate();
    
    // ========== STATUS LED ==========
    if (current_time - print_counter > 500000) {
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
    
    loopRate(LOOP_FREQUENCY_HZ);
}
Key Point: The flight controller code ALWAYS runs. Debug prints are optional!

🛡️ Arming Procedure
IMPORTANT: Before motors will spin, you MUST arm the flight controller!
The vehicle will arm when BOTH conditions are met:

✓ Throttle cut switch (CH5) is OFF (< 1500µs)

✓ Throttle stick is LOW (< 1050µs)

How to Arm (Props OFF!)
text
1. Power on vehicle (props OFF!)
2. Turn on transmitter
3. Verify throttle cut switch is OFF
4. Lower throttle stick to minimum
5. Watch serial monitor for "*** ARMED ***"
6. LED will blink 3 times rapidly
How to Disarm
text
1. Activate throttle cut switch (CH5 > 1500)
2. Watch serial monitor for "*** DISARMED ***"
3. Motors stop immediately
Safety Features
⚠️ Motors will NOT spin until armed

⚠️ Arming resets all PID integrators

⚠️ Throttle cut overrides all commands (instant shutdown)

⚠️ Failsafe defaults to disarmed state

🧪 Testing Scenarios
Scenario 0: Test Arming System (FIRST!)
Goal: Verify arming/disarming works before testing anything else

Setup: Props OFF! Motors can spin freely

Uncomment:

cpp
printRadioData();      // Watch CH3 (throttle) and CH5 (throttle cut)
//printMotorCommands(); // Optional: Watch motor commands
Test Sequence:

Throttle low, CH5 low → Should see "*** ARMED ***"

Move throttle up → motor commands should increase

Activate CH5 → Should see "*** DISARMED ***", motors go to minimum

Deactivate CH5, throttle low → Should re-arm

Scenario 1: Test Receiver ONLY
Goal: Verify receiver is working, channels are mapped correctly

Uncomment:

cpp
printRadioData();  // ← Uncomment this line ONLY
Upload & Monitor:

bash
pio run -e teensy40 -t upload
pio device monitor
Expected Output:

text
CH1:1500 CH2:1500 CH3:1000 CH4:1500 CH5:1000 CH6:1000
CH1:1678 CH2:1489 CH3:1234 CH4:1512 CH5:1000 CH6:1000
...
Move transmitter sticks → channel values change

Scenario 2: Calibrate IMU
Goal: Verify IMU data is clean, calibrate offsets

Uncomment:

cpp
printGyroData();   // ← Uncomment
printAccelData();  // ← Uncomment
//printRollPitchYaw();  // Keep commented
Upload & Open Serial Plotter:

bash
pio run -e teensy40 -t upload
# Tools → Serial Plotter
Watch graphs:

GyroX/Y/Z should be near 0 (stationary)

AccX/Y should be near 0 (stationary)

AccZ should be near 1.0 (gravity)

Rotate board:

Values should change smoothly

No spikes or sudden jumps

See: MPU6050_CALIBRATION_GUIDE.md for full details

Scenario 3: Verify Attitude Estimation
Goal: Check Madgwick filter is working, attitude is accurate

Uncomment:

cpp
printRollPitchYaw();  // ← Uncomment this line ONLY
Upload & Open Serial Plotter:

Test:

Board level → roll/pitch should be near 0

Tilt 45° right → roll should show ~45°

Tilt 45° forward → pitch should show ~45°

Return to level → should return to 0

Scenario 4: Test Full IMU Pipeline
Goal: Verify raw IMU → filtered IMU → attitude

Uncomment:

cpp
printGyroData();      // ← Uncomment
printAccelData();     // ← Uncomment
printRollPitchYaw();  // ← Uncomment
Watch Serial Plotter:

All 9 graphs (Gyro XYZ, Accel XYZ, Roll/Pitch/Yaw)

Rotate board → all should respond smoothly

No lag, no spikes

Scenario 5: PID Tuning (Ground Test)
Goal: Tune PID gains, verify control response

Setup:

Remove props

Secure aircraft (can't fly)

Motors can spin freely

Arm the system first!

Uncomment:

cpp
printRollPitchYaw();  // ← Uncomment
printPIDoutput();     // ← Uncomment
//printMotorCommands();  // Optional
Upload & Open Serial Plotter:

Test:

Arm motors (throttle low, CH5 off)

Throttle up to 30%

Move roll stick → watch graphs

Adjust gains in config.h based on response

See: PID_TUNING_GUIDE.md for full tuning process

Scenario 6: Combined Receiver + IMU Test
Goal: Verify receiver inputs affect PID outputs correctly

Uncomment:

cpp
printRadioData();     // ← Uncomment (channels)
printRollPitchYaw();  // ← Uncomment (attitude)
printPIDoutput();     // ← Uncomment (controller)
Test:

Arm the system

Move sticks → radio channels change

PID outputs should respond to stick inputs

Verify correct direction (right stick right = positive roll command)

Scenario 7: Motor Output Test
Goal: Verify motor commands are correct

Setup:

Remove props

Motors can spin

Arm the system first!

Uncomment:

cpp
printMotorCommands();  // ← Uncomment
//printServoCommands(); // If using servos
Test:

Arm motors

Throttle up slowly

Watch motor commands increase

All motors should spin

Move sticks → motors change speeds appropriately

Activate throttle cut → motors stop immediately

Scenario 8: Loop Rate Verification
Goal: Verify flight controller is running at 2kHz (500μs loop time)

Uncomment:

cpp
printLoopRate();  // ← Uncomment this line ONLY
Expected Output:

text
dt(μs):498
dt(μs):502
dt(μs):500
dt(μs):497
Should be:

Around 500μs (2000Hz)

Consistent (not jumping around)

If too high (>600μs), loop is slow

🎯 Complete Testing Sequence
Recommended Order
Test Arming System:

cpp
printRadioData();
//printMotorCommands();
Goal: Verify arming/disarming works before anything else

Test Receiver:

cpp
printRadioData();
Goal: Verify receiver works, channels mapped

Calibrate IMU:

cpp
printGyroData();
printAccelData();
Goal: Clean IMU data, calibrate offsets

Verify Attitude:

cpp
printRollPitchYaw();
Goal: Attitude estimation works

Test Integration:

cpp
printRadioData();
printRollPitchYaw();
Goal: Receiver + IMU working together

Tune PID (No Props, Armed):

cpp
printRollPitchYaw();
printPIDoutput();
Goal: Get stable PID response

Test Motors (No Props, Armed):

cpp
printMotorCommands();
Goal: Verify motor outputs

Final Check:

cpp
// Comment out ALL debug prints
Goal: Run full flight controller, verify loop rate

First Flight:

cpp
// Keep ALL commented
// Flight controller runs at full speed
📊 Serial Monitor vs Serial Plotter
Use Serial Monitor For:
Text-based output

Testing single function

Checking specific values

Debugging issues

Arming status messages

Example:

cpp
printRadioData();  // Text list of channels
Output:

text
CH1:1500 CH2:1500 CH3:1000 ...
*** ARMED ***
*** DISARMED ***
Use Serial Plotter For:
Visualizing data over time

Seeing trends and patterns

Tuning filters

PID tuning

Spotting oscillations

Example:

cpp
printGyroData();
printAccelData();
printRollPitchYaw();
Output: Graphs of all 9 values updating in real-time

🔄 Workflow Example
Day 1: Hardware Setup
Wire FS-iA6B to Teensy

Wire MPU6050 to Teensy

Bind receiver (see RECEIVER_BINDING_GUIDE.md)

Test arming:

cpp
printRadioData();
Day 2: IMU Calibration
Calibrate IMU (runs automatically in setup)

Verify clean data:

cpp
printGyroData();
printAccelData();
Tune filters if needed (see MPU6050_CALIBRATION_GUIDE.md)

Verify attitude:

cpp
printRollPitchYaw();
Day 3: Ground Testing
Remove props, secure aircraft

Test arming/disarming repeatedly

Test integration:

cpp
printRadioData();
printRollPitchYaw();
Test motor outputs (armed):

cpp
printMotorCommands();
Verify no issues

Day 4: PID Tuning
Still no props!

Enable PID debug:

cpp
printRollPitchYaw();
printPIDoutput();
Arm system, test at low throttle

Tune gains (see PID_TUNING_GUIDE.md)

Test at different throttle levels

Day 5: First Flight
Install props (correct direction!)

Comment out ALL debug:

cpp
// All commented
Final ground test (verify arming)

Hover test (1-2 inches)

If stable → gradual altitude increase

💡 Pro Tips
Tip 1: One Debug Function at a Time (Serial Monitor)
When debugging specific issue:

cpp
// Uncomment ONLY what you need
printRadioData();  // ← Just this
//printGyroData();
//printRollPitchYaw();
Cleaner output = easier to read

Tip 2: Multiple Functions for Plotter
Serial Plotter can handle many graphs:

cpp
// Uncomment multiple for visualization
printGyroData();      // 3 graphs
printAccelData();     // 3 graphs
printRollPitchYaw();  // 3 graphs
// Total: 9 graphs in plotter
Tip 3: Comment Out for Flight
During actual flight:

cpp
// Comment out ALL debug prints!
//printRadioData();
//printGyroData();
//etc...
Maximizes loop rate for best performance

Tip 4: Keep Notes
text
Testing Log:
Date: ________

Test 0: Arming System
- Conditions: throttle low, CH5 low = ARMED ✓
- Conditions: CH5 high = DISARMED ✓
- Safety: Instant motor stop when disarmed ✓

Test 1: Receiver
- Uncommented: printRadioData()
- Result: All channels working ✓

Test 2: IMU
- Uncommented: printGyroData(), printAccelData()
- Result: Noisy, increased filter to B_ACCEL=0.08 ✓

Test 3: Attitude
- Uncommented: printRollPitchYaw()
- Result: Accurate within 2° ✓

... etc
Tip 5: Use #define for Debug Sections
Advanced: Create debug flags in config.h:

cpp
// In config.h:
//#define DEBUG_RECEIVER
//#define DEBUG_IMU
//#define DEBUG_PID

// In main.cpp loop():
#ifdef DEBUG_RECEIVER
    printRadioData();
#endif

#ifdef DEBUG_IMU
    printGyroData();
    printAccelData();
    printRollPitchYaw();
#endif

#ifdef DEBUG_PID
    printRollPitchYaw();
    printPIDoutput();
#endif
Then just uncomment ONE #define at a time in config.h!

⚠️ Important Reminders
Debug Prints Affect Loop Rate
text
No debug prints:  ~2000Hz (500μs)
One print:        ~1800Hz (555μs)
Three prints:     ~1500Hz (666μs)
All prints:       ~1000Hz (1000μs)
For tuning: Some debug OK
For flight: Comment out all debug

Print Timing
Debug functions have built-in rate limiting:

cpp
void printGyroData() {
    if (current_time - print_counter > 10000) {  // ← Only prints every 10ms
        print_counter = current_time;
        Serial.print(F("GyroX:")); Serial.print(GyroX);
        // ...
    }
}
This prevents flooding serial buffer

Arming is REQUIRED
Motors will NOT spin unless:

✓ Throttle cut switch (CH5) is OFF (< 1500)

✓ Throttle stick is LOW (< 1050)

✓ Serial monitor shows "*** ARMED ***"

Test arming FIRST before any motor tests!

🎓 Summary
The Comment/Uncomment Method
Advantages:
✓ One file, not multiple versions
✓ All functionality always present
✓ Just uncomment what you want to test
✓ No risk of using wrong version
✓ Easy to switch between tests
✓ Exactly like original dRehmFlight

How to Use:

Open main.cpp

Find debug section in loop()

Uncomment functions you want

Upload

Open Serial Monitor or Plotter

Test your subsystem

Comment out when done

Simple!

📚 Reference Chart
What to Test	Uncomment	Tool	Notes
Arming System	printRadioData()	Serial Monitor	FIRST TEST! Verify CH3, CH5
Receiver working	printRadioData()	Serial Monitor	
IMU calibration	printGyroData() printAccelData()	Serial Plotter	
Attitude accuracy	printRollPitchYaw()	Serial Plotter	
PID tuning	printRollPitchYaw() printPIDoutput()	Serial Plotter	Arm first!
Motor outputs	printMotorCommands()	Serial Monitor	Arm first!
Loop timing	printLoopRate()	Serial Monitor	
Everything	ALL functions	Serial Plotter	Debug only!
Flight	NONE (all commented)	None	Max performance
You now have a complete, professional flight controller development workflow! 🚁

Next Steps:

Wire hardware (see COMPLETE_WIRING_GUIDE.md)

Bind receiver (see RECEIVER_BINDING_GUIDE.md)

Test arming system FIRST

Calibrate IMU (see MPU6050_CALIBRATION_GUIDE.md)

Tune PID (see PID_TUNING_GUIDE.md)

FLY! ✈️