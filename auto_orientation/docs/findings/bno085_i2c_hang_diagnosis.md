# BNO085 I2C Initialization Hang Troubleshooting Guide

## Problem Statement

**Symptom**: Arduino Mega with BNO085 on I2C bus (pins 20/21) boots normally, prints initialization messages including "===", but then **hangs indefinitely** during sensor initialization. The code never progresses past the `imu.begin()` call.

**Example Output**:
```
=== Auto Orientation System ===
Initializing sensors...
Board: Initializing BNO085 IMU sensor...
[HANGS HERE - No response to serial]
```

---

## Root Cause Analysis

### Why BNO085 I2C Init Might Hang

#### 1. **Missing or Weak Pull-up Resistors**

**Technical Background**:
- I2C uses open-drain drivers; both SCL and SDA need pull-up resistors to VCC
- Arduino Mega has internal pull-ups (~47kΩ), often insufficient for reliable I2C
- BNO085 breakouts typically have 10kΩ pull-ups, but may be missing or damaged

**Why it causes hangs**:
- Without adequate pull-ups, SDA/SCL lines don't rise to VCC properly
- Master can drive lines LOW but slave can't release them efficiently
- Creates timing violations where the master waits indefinitely for ACK
- At higher clock speeds (400kHz), weak pull-ups worsen the effect

**Severity**: ⚠️ **MOST COMMON cause** of I2C initialization hangs

**Solutions**:
- Add external 4.7kΩ pull-up resistors (VCC to SCL, VCC to SDA) if missing
- Try lower I2C clock speed (100kHz) to reduce timing sensitivity
- Check for multiple pull-ups in series (can cascade issue)

---

#### 2. **I2C Clock Speed Too High**

**Problem**:
- Arduino default is often 400kHz, too fast for unreliable wiring or weak pull-ups
- BNO085 supports 100kHz to 1MHz, but implementation quality varies by breakout

**Why it causes hangs**:
- High clock speed requires precise voltage transitions and timing
- Wiring capacitance, weak pull-ups, or long lines exacerbate timing violations
- Slave may not complete a byte before master expects next bit
- Master waits forever for ACK on byte that never completes

**Solutions**:
1. Try 100kHz first (most reliable):
   ```cpp
   Wire.setClock(100000L);  // 100 kHz
   ```
2. If still hangs, try 50kHz:
   ```cpp
   Wire.setClock(50000L);  // 50 kHz
   ```

---

#### 3. **Device Not Present on I2C Bus**

**Symptoms**:
- No ACK from sensor at address 0x4A or 0x4B
- Slave is not responding to bus activity
- Master times out waiting for slave acknowledgment

**Why it causes hangs**:
- `imu_->begin_I2C(0x4A, &Wire, 0)` scans the bus for device at 0x4A
- If device doesn't respond, library waits indefinitely (no timeout in some versions)
- No error status returned, just hang

**Common causes**:
- Sensor wiring not connected or loose
- Wrong I2C address (check DI pin configuration)
- Sensor power supply issue (not enough current, brownout)
- Defective sensor

**Solutions**:
- Verify SCL/SDA connections with multimeter (continuity)
- Run I2C scanner to detect presence/address
- Check power supply stability

---

#### 4. **Wire Library Not Properly Initialized**

**Problem**:
- Some Arduino boards require specific initialization order
- Wire library may not be ready before I2C operations

**Why it causes hangs**:
- Calling I2C operations before `Wire.begin()` fails silently
- Some boards have issues with I2C initialization timing
- Internal state not properly set up

**Solutions**:
```cpp
// ALWAYS call Wire.begin() FIRST
Wire.begin();          // Initialize I2C master mode
delay(10);             // Small delay for stabilization
Wire.setClock(100000L); // Set clock AFTER begin()

// Now safe to communicate
if (!imu_->begin_I2C(0x4A, &Wire, 0)) {
  // Handle error
}
```

---

#### 5. **Device in Undefined/Locked State**

**Problem**:
- BNO085 can enter undefined state if:
  - Power cycled while I2C transfer in progress
  - Previous sketch crashed during I2C communication
  - Sensor firmware bug or corruption

**Why it causes hangs**:
- Sensor's I2C state machine stuck expecting data
- Won't respond to master, but also won't reset
- Only solution is full power cycle

**Solutions**:
- Power cycle the entire system (including sensor)
- Add hardware reset capability:
  ```cpp
  // Define reset pin in pins.h
  #define BNO085_RESET_PIN 5
  
  // In code:
  pinMode(BNO085_RESET_PIN, OUTPUT);
  digitalWrite(BNO085_RESET_PIN, LOW);
  delay(100);
  digitalWrite(BNO085_RESET_PIN, HIGH);
  delay(650);  // Wait for sensor startup
  Wire.begin();
  ```
- Use watchdog timer to auto-reset if hung

---

#### 6. **Adafruit Library Bug or Address Mismatch**

**Potential issues**:
- Library version mismatch with specific Arduino board
- Bug in `begin_I2C()` implementation for your board variant
- Address hardcoded incorrectly

**How to check**:
- Use I2C scanner to verify actual address
- Try manually setting address at compile time
- Update library to latest version

---

## How to Verify I2C Device Presence

### Method 1: I2C Bus Scanner Sketch

Use this sketch to detect all devices on the I2C bus:

```cpp
/*
 * I2C Bus Scanner
 * Scans addresses 0x00-0x7F and reports which devices respond
 */

#include <Wire.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  
  Serial.println("\n=== I2C Bus Scanner ===");
  Serial.println("Scanning I2C addresses 0x00-0x7F...");
  delay(100);
  
  // Initialize I2C at different speeds to test
  scanBusAtSpeed(100000L, "100kHz");
}

void loop() {
  delay(10000);  // Scan every 10 seconds
}

void scanBusAtSpeed(uint32_t clock_speed, const char* speed_label) {
  Wire.begin();
  Wire.setClock(clock_speed);
  delay(100);
  
  Serial.print("\n--- Scanning at ");
  Serial.print(speed_label);
  Serial.println(" ---");
  
  int count = 0;
  for (uint8_t addr = 0x00; addr <= 0x7F; addr++) {
    // Send START condition and address
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission(true);
    
    if (error == 0) {  // ACK received
      Serial.print("Device found at 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      count++;
    } else if (error == 4) {
      Serial.print("Unknown error at 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
    }
  }
  
  Serial.print("Total devices found: ");
  Serial.println(count);
}
```

**Expected Output for BNO085**:
```
=== I2C Bus Scanner ===
Scanning I2C addresses 0x00-0x7F...

--- Scanning at 100kHz ---
Device found at 0x4a        [if DI pin tied to GND]
or
Device found at 0x4b        [if DI pin tied to VCC]
Total devices found: 1
```

**Troubleshooting the scanner**:
| Output | Interpretation | Next Step |
|--------|-----------------|-----------|
| No devices found | No I2C communication | Check wiring, power supply |
| Found at 0x4a or 0x4b | Device present | Try main sketch with that address |
| "Unknown error" at multiple addresses | Weak pull-ups or noise | Add 4.7kΩ pull-ups |

---

### Method 2: Minimal I2C Test Code

Test the BNO085 with minimal dependencies:

```cpp
/*
 * Minimal BNO085 I2C Test
 * Tests each initialization step with debug output
 */

#include <Wire.h>

#define BNO085_I2C_ADDR 0x4A  // Or 0x4B if DI pin to VCC
#define BNO085_CHIP_ID 0xA0   // Expected value for chip ID register

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  
  Serial.println("\n=== BNO085 Minimal I2C Test ===");
  
  // Step 1: Initialize I2C
  Serial.print("Step 1: Initializing Wire (I2C)... ");
  Wire.begin();
  delay(10);
  Serial.println("OK");
  
  // Step 2: Set clock speed
  Serial.print("Step 2: Setting I2C clock to 100kHz... ");
  Wire.setClock(100000L);
  delay(10);
  Serial.println("OK");
  
  // Step 3: Try to detect device
  Serial.print("Step 3: Scanning for BNO085 at 0x");
  Serial.print(BNO085_I2C_ADDR, HEX);
  Serial.print("... ");
  
  Wire.beginTransmission(BNO085_I2C_ADDR);
  uint8_t error = Wire.endTransmission(true);
  
  if (error == 0) {
    Serial.println("FOUND");
  } else {
    Serial.print("NOT FOUND (error code: ");
    Serial.print(error);
    Serial.println(")");
    Serial.println("ERROR: Device not responding. Check wiring and power.");
    while(1) { delay(100); }
  }
  
  // Step 4: Try to read chip ID
  Serial.print("Step 4: Reading chip ID register... ");
  uint8_t chip_id = readRegister(BNO085_I2C_ADDR, 0x00);  // Chip ID at reg 0x00
  Serial.print("0x");
  Serial.println(chip_id, HEX);
  
  if (chip_id != 0xA0) {
    Serial.println("WARNING: Unexpected chip ID. Expected 0xA0");
  } else {
    Serial.println("SUCCESS: Chip ID matches BNO085");
  }
  
  Serial.println("\n=== All tests passed ===");
  Serial.println("Ready to initialize Adafruit library");
}

void loop() {
  delay(1000);
}

uint8_t readRegister(uint8_t device_addr, uint8_t reg_addr) {
  Wire.beginTransmission(device_addr);
  Wire.write(reg_addr);
  Wire.endTransmission(false);  // Don't release bus
  
  Wire.requestFrom(device_addr, 1, true);  // Request 1 byte
  
  if (Wire.available()) {
    return Wire.read();
  }
  return 0xFF;  // Error value
}
```

---

## Hardware Verification Checklist

### Electrical Checks

| Check | Tool | Expected | Action if Failed |
|-------|------|----------|------------------|
| **VCC to GND continuity** | Multimeter (Ohms) | 0Ω (short) | Power supply not connected |
| **VCC to sensor VCC** | Multimeter (Ohms) | <1Ω | Check wire/connection |
| **GND to sensor GND** | Multimeter (Ohms) | <1Ω | Check ground return |
| **VCC to sensor (voltage)** | Multimeter (DC V) | 3.3V ±5% | Power supply failing |
| **SCL continuity** | Multimeter (Ohms) | <5Ω to pin 21 | Broken wire |
| **SDA continuity** | Multimeter (Ohms) | <5Ω to pin 20 | Broken wire |
| **Pull-up resistors present** | Visual inspection | 4.7kΩ on SCL+SDA | Add if missing |
| **Pull-up resistor values** | Multimeter (Ohms, unpowered) | 4.7kΩ ±10% | Replace if wrong |

### I2C Bus Voltage Checks

**With power ON**:
- Idle SCL voltage: 3.0V - 3.3V (should be HIGH)
- Idle SDA voltage: 3.0V - 3.3V (should be HIGH)
- When pulled LOW: 0V - 0.2V

**If voltages wrong**:
- SCL/SDA stuck LOW → Short circuit or device holding bus
- SCL/SDA stuck HIGH → Pull-ups missing or device not pulling
- SCL/SDA oscillating → Capacitance issue or noise

---

## Debug Steps: Progressive Validation

Follow these steps in order to isolate the problem:

### **Debug Level 1: Basic Setup**

```cpp
void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(100); }
  
  Serial.println("\n=== DEBUG LEVEL 1: Basic Setup ===");
  
  // Test 1: Serial working?
  Serial.println("Test 1: Serial communication OK");
  delay(500);
  
  // Test 2: Wire library initialization
  Serial.println("Test 2: Calling Wire.begin()...");
  Wire.begin();
  Serial.println("Test 2: Wire.begin() completed");
  delay(500);
  
  // Test 3: Clock setting
  Serial.println("Test 3: Setting I2C clock to 100kHz...");
  Wire.setClock(100000L);
  Serial.println("Test 3: Clock set");
  delay(500);
  
  Serial.println("DEBUG LEVEL 1: PASSED");
  Serial.println("Proceed to Level 2");
}

void loop() { delay(1000); }
```

**If this hangs**: Problem is with Wire library initialization or board-specific I2C issues.
- Try different board variant in Arduino IDE
- Check for hardware I2C bus conflicts
- Consider alternate I2C library (if available)

---

### **Debug Level 2: Device Presence**

```cpp
void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(100); }
  
  Serial.println("\n=== DEBUG LEVEL 2: Device Presence ===");
  
  Wire.begin();
  Wire.setClock(100000L);
  delay(100);
  
  // Try to find device
  Serial.println("Attempting I2C transaction with BNO085...");
  Serial.print("Checking address 0x4A (DI=GND)... ");
  
  Wire.beginTransmission(0x4A);
  uint8_t error = Wire.endTransmission(true);
  
  if (error == 0) {
    Serial.println("FOUND - Device is responding");
  } else {
    Serial.print("NOT FOUND (error ");
    Serial.print(error);
    Serial.println(")");
    
    Serial.print("Checking address 0x4B (DI=VCC)... ");
    Wire.beginTransmission(0x4B);
    error = Wire.endTransmission(true);
    
    if (error == 0) {
      Serial.println("FOUND at 0x4B");
    } else {
      Serial.println("NOT FOUND");
      Serial.println("ERROR: No response from BNO085");
      Serial.println("Check: Wiring, Power Supply, DI pin configuration");
      while(1) { delay(1000); }
    }
  }
  
  Serial.println("DEBUG LEVEL 2: PASSED");
}

void loop() { delay(1000); }
```

**If this hangs**: Problem is likely Wire library or board-specific I2C issue
- See "Debug Level 1: Basic Setup" solutions
- Could also be no pull-ups (try adding external ones)

**If device not found**: See "Hardware Verification Checklist"

---

### **Debug Level 3: Library Initialization**

```cpp
#include <Wire.h>
#include "Adafruit_BNO08x.h"

Adafruit_BNO08x imu;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(100); }
  
  Serial.println("\n=== DEBUG LEVEL 3: Library Init ===");
  
  // Create IMU object
  Serial.println("Creating Adafruit_BNO08x object...");
  // Object created above as global
  Serial.println("Object created");
  
  // Initialize Wire
  Serial.println("Calling Wire.begin()...");
  Wire.begin();
  Wire.setClock(100000L);
  delay(100);
  Serial.println("Wire initialized");
  
  // Call begin_I2C with debug output
  Serial.println("Calling imu.begin_I2C(0x4A, &Wire, 0)...");
  Serial.println("  [If this hangs, problem is in Adafruit library]");
  
  bool success = imu.begin_I2C(0x4A, &Wire, 0);
  
  Serial.print("imu.begin_I2C() returned: ");
  Serial.println(success);
  
  if (!success) {
    Serial.println("ERROR: begin_I2C failed");
  } else {
    Serial.println("SUCCESS: Sensor initialized");
    Serial.println("Sensor product IDs:");
    Serial.print("  SW version: ");
    Serial.println(imu.prodIds.swVer);
  }
}

void loop() { delay(1000); }
```

**If this hangs at begin_I2C**: Problem is likely:
1. Weak pull-ups (add 4.7kΩ external resistors)
2. Device not present (verify with Level 2)
3. Sensor locked in bad state (power cycle)
4. Wrong address (verify with Level 2)

---

### **Debug Level 4: Clock Speed Testing**

If Level 3 hangs, try different clock speeds:

```cpp
#include <Wire.h>
#include "Adafruit_BNO08x.h"

void testClockSpeed(uint32_t speed_hz, const char* label) {
  Serial.print("\nTesting at ");
  Serial.println(label);
  
  Serial.println("Calling Wire.setClock()...");
  Wire.setClock(speed_hz);
  delay(100);
  Serial.println("Clock set");
  
  Serial.println("Scanning for device...");
  Wire.beginTransmission(0x4A);
  uint8_t error = Wire.endTransmission(true);
  
  if (error == 0) {
    Serial.println("Device found");
    
    // Try Adafruit initialization
    Adafruit_BNO08x imu;
    Wire.begin();
    Wire.setClock(speed_hz);
    delay(100);
    
    Serial.println("Calling begin_I2C...");
    bool success = imu.begin_I2C(0x4A, &Wire, 0);
    Serial.print("Result: ");
    Serial.println(success ? "SUCCESS" : "FAILED");
  } else {
    Serial.println("Device not found at this speed");
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(100); }
  
  Serial.println("\n=== DEBUG LEVEL 4: Clock Speed Testing ===");
  
  // Test different speeds from slowest to fastest
  testClockSpeed(50000L,   "50 kHz");
  testClockSpeed(100000L,  "100 kHz");
  testClockSpeed(200000L,  "200 kHz");
  testClockSpeed(400000L,  "400 kHz");
  
  Serial.println("\n=== Testing complete ===");
}

void loop() { delay(1000); }
```

**Results interpretation**:
- Works at 50kHz only → Weak pull-ups, long wires, or bus noise
- Works at 100kHz+ → Normal I2C bus, probably other issue
- Hangs at all speeds → Device not present or Wire library issue

---

## Common User Mistakes

### Mistake 1: Using Wrong Pins

**Problem**: User connects BNO085 to wrong pins, code still tries I2C on 20/21

**Arduino Mega I2C pins**:
- SDA (data): **Pin 20**
- SCL (clock): **Pin 21**

**Check your wiring**:
```
BNO085 SDA → Arduino Mega Pin 20
BNO085 SCL → Arduino Mega Pin 21
BNO085 GND → Arduino GND
BNO085 VCC → Arduino 3.3V (with pull-up resistors!)
```

**Fix**: Verify connections with multimeter or visual inspection

---

### Mistake 2: DI Pin Floating or Not Configured

**Problem**: DI (device select) pin not tied to voltage, causing unpredictable I2C address

**DI Pin Function**:
- DI to GND → Address 0x4A (standard)
- DI to VCC → Address 0x4B
- DI floating → Address unpredictable (causes hang)

**Fix**:
```
// On BNO085 breakout:
If not using DI pin, tie it to GND with resistor:
BNO085 DI → GND (via 10kΩ resistor, or direct)
```

**In code**:
```cpp
// Always try both addresses to be safe
bool init_success = false;
if (!imu.begin_I2C(0x4A, &Wire, 0)) {
  Serial.println("Not at 0x4A, trying 0x4B...");
  init_success = imu.begin_I2C(0x4B, &Wire, 0);
} else {
  init_success = true;
}

if (!init_success) {
  Serial.println("ERROR: Could not find BNO085 at either address");
}
```

---

### Mistake 3: Firmware Loaded for UART but Hardware Wired I2C

**Problem**: Code expects UART communication, but sensor wired for I2C (or vice versa)

**This project uses I2C**, not UART. Verify in pins.h:

```cpp
// pins.h should NOT define UART pins for I2C configuration
// For I2C, only define:
//   - Nothing (I2C uses fixed pins 20/21 on Mega)
//   - No BNO085_RX_PIN or BNO085_TX_PIN
```

**Check code in main.cpp**:
```cpp
// Correct for I2C:
Wire.begin();
imu.begin_I2C(0x4A, &Wire, 0);

// Wrong for I2C (UART code):
Serial1.begin(115200);
imu.begin_UART(&Serial1, 0);
```

---

### Mistake 4: Pin 20/21 Used by Other I2C Devices

**Problem**: Another shield or device on the same I2C bus, address collision

**Symptoms**:
- Scanner finds extra devices at other addresses
- Hangs when initializing BNO085
- Works alone, fails with other devices

**Solution**:
1. Run I2C scanner to find ALL devices
2. Check address conflicts (each device must have unique address)
3. If conflict exists, remap other device to different address or pins
4. If using software I2C, implement on different pins

---

### Mistake 5: Missing Pull-up Resistors

**Problem**: Adafruit BNO085 breakout may have pull-ups on 3.3V side, but Arduino Mega has weak internal pull-ups

**Why it's an issue**:
- Weak pull-ups (47kΩ typical on Arduino) + BNO085 pull-ups create slow rise times
- At 400kHz, rise time becomes critical
- Data gets corrupted or master waits forever for slave response

**Solution**:
1. Add external 4.7kΩ pull-ups to VCC (3.3V):
   ```
   SCL ──┤ 4.7kΩ ├─ VCC (3.3V)
   SDA ──┤ 4.7kΩ ├─ VCC (3.3V)
   ```

2. Use lower clock speed if pull-ups missing:
   ```cpp
   Wire.setClock(100000L);  // Instead of 400000L
   ```

---

## Decision Tree for Troubleshooting

```
START: Code hangs during imu.begin() initialization
│
├─ Run I2C Scanner (see above)
│  │
│  ├─ Scanner HANGS
│  │  └─ Problem: Wire.begin() or I2C subsystem issue
│  │     Fix: See "Debug Level 1" solutions
│  │
│  ├─ Scanner finds device at 0x4A or 0x4B
│  │  └─ Device IS PRESENT on bus
│  │     Next: Go to "Library Initialization Hang"
│  │
│  └─ Scanner finds NO DEVICES
│     └─ Problem: Hardware not connected
│        Checks:
│        - Multimeter: VCC=3.3V, GND=0V
│        - Continuity: SCL line has <5Ω to pin 21
│        - Continuity: SDA line has <5Ω to pin 20
│        - Visual: No cold solder joints
│
├─ Library Initialization Hang (begin_I2C times out)
│  │
│  ├─ Device present at correct address
│  │  └─ Likely causes (in order):
│  │     1. Weak pull-ups → Add 4.7kΩ external resistors
│  │     2. Clock speed too high → Try Wire.setClock(100000L)
│  │     3. Sensor in bad state → Power cycle
│  │     4. Library bug → Update Adafruit library
│  │
│  ├─ Try reducing clock speed
│  │  │
│  │  ├─ Works at 100kHz
│  │  │  └─ Fix: Add pull-up resistors, use 100kHz mode
│  │  │
│  │  └─ Hangs at all speeds
│  │     └─ Fix: Power cycle, check wiring
│
└─ Initialization completes but code hangs in loop()
   └─ Different problem (not initialization hang)
      Check: getSensorEvent() call, enable report call, etc.
```

---

## Step-by-Step Debugging Procedure

### Scenario: Code hangs after printing "==="

**Step 1**: Power off everything
```
Turn off Arduino Mega
Unplug BNO085 sensor
Wait 30 seconds (capacitors discharge)
```

**Step 2**: Inspect hardware
```
- Check all solder joints (especially on pins 20, 21, VCC, GND)
- Check for cold solder joints (dull/grainy vs shiny)
- Verify pull-up resistors present (visual inspection)
  - Look for 2 resistors near SDA/SCL lines
  - Check value with multimeter if readable
- Check VCC connection has bypass capacitor (usually 100nF near sensor)
```

**Step 3**: Verify wiring with multimeter (power off)
```
Set multimeter to Ohms (Ω)
Measure SCL line: Pin 21 to BNO085 SCL → Should be <5Ω (near 0)
Measure SDA line: Pin 20 to BNO085 SDA → Should be <5Ω (near 0)
Measure VCC line: 3.3V to BNO085 VCC → Should be <1Ω
Measure GND line: GND to BNO085 GND → Should be <1Ω
```

**Step 4**: Power on and check voltages
```
Set multimeter to DC Volts (V)
Measure VCC: Should read 3.3V ± 0.2V (no load)
Measure SCL (idle): Should read 3.0V - 3.3V (HIGH state)
Measure SDA (idle): Should read 3.0V - 3.3V (HIGH state)

If both SCL and SDA read 0V: Short circuit or device holding bus
If both read >3.3V: Open circuit or missing pull-ups
If readings oscillate: Timing issue or noise
```

**Step 5**: Upload I2C Scanner sketch
```cpp
// See "I2C Bus Scanner Sketch" section above
// Run it and observe output
```

**Depending on scanner output**:
- **Hangs**: Go to Debug Level 1
- **Finds device at 0x4A**: Device OK, go to Step 6
- **Finds device at 0x4B**: Update address in code to 0x4B
- **No devices found**: Go to hardware checks

**Step 6**: Upload Debug Level 3 sketch
```cpp
// See "Debug Level 3: Library Initialization" section above
// Observe which line causes the hang
```

**Step 7**: If begin_I2C hangs:
```
1. Reduce clock speed:
   Change: Wire.setClock(400000L);
   To:     Wire.setClock(100000L);
   
2. Upload and test
3. If still hangs, add pull-up resistors:
   SCL ──┤ 4.7kΩ ├─ VCC (3.3V)
   SDA ──┤ 4.7kΩ ├─ VCC (3.3V)
   
4. Power cycle and test again
```

**Step 8**: If begin_I2C still hangs:
```
1. Power cycle entire system (unplug everything, wait 30 seconds)
2. Test with clock speed 50kHz:
   Wire.setClock(50000L);
   
3. If works at 50kHz: Bus quality issue (pull-ups, noise, wiring)
4. If still hangs: Sensor may be defective, try different unit
```

---

## Reference: Adafruit BNO08x Library API

### Key Functions Used in This Project

#### `begin_I2C()`
```cpp
bool begin_I2C(uint8_t i2c_addr = BNO08x_I2CADDR_DEFAULT,
               TwoWire *wire = &Wire,
               int32_t sensor_id = 0);
```

**Parameters**:
- `i2c_addr`: I2C address (0x4A or 0x4B)
- `wire`: Wire object reference (default &Wire for Arduino Mega pins 20/21)
- `sensor_id`: Sensor ID for logging (not critical)

**Returns**: `true` if initialization successful, `false` if failed

**Timeout behavior**: Library may hang indefinitely if device not responding (version-dependent)

---

#### `enableReport()`
```cpp
bool enableReport(sh2_SensorId_t sensor, uint32_t interval_us = 10000);
```

**Parameters**:
- `sensor`: Report type (SH2_ROTATION_VECTOR = 0x05 for orientation)
- `interval_us`: Update period in microseconds

**Common values**:
- 10000 (10ms, 100Hz)
- 50000 (50ms, 20Hz)
- 100000 (100ms, 10Hz) - Recommended for most applications

**Returns**: `true` if report enabled, `false` if failed

---

#### `getSensorEvent()`
```cpp
bool getSensorEvent(sh2_SensorValue_t *event);
```

**Parameters**:
- `event`: Pointer to sensor value structure to fill

**Returns**: `true` if new data available, `false` if no new data

**Fills in event fields**:
- `event->sensorId`: Report type (verify it's SH2_ROTATION_VECTOR)
- `event->un.rotationVector`: Quaternion data (real, i, j, k)
- `event->status`: Calibration level (0-3)

---

## Recovery Steps for Different Scenarios

### Scenario A: Sensor Powered but Not Responding

**Signs**: Scanner hangs or no devices found, VCC=3.3V

**Recovery**:
1. Check DI pin (should be tied to GND or VCC, not floating)
2. Power cycle sensor (unplug VCC for 5 seconds)
3. Add hardware reset:
   ```cpp
   #define BNO085_RESET_PIN 5
   pinMode(BNO085_RESET_PIN, OUTPUT);
   digitalWrite(BNO085_RESET_PIN, LOW);
   delay(100);
   digitalWrite(BNO085_RESET_PIN, HIGH);
   delay(650);
   ```
4. Re-scan bus

---

### Scenario B: Weak I2C Bus (Scanner hangs at high speeds)

**Signs**: Works at 50kHz, fails at 100kHz+

**Recovery**:
1. Check for multiple pull-up resistors (should be only 1 set)
2. Add external 4.7kΩ pull-ups if missing
3. Shorten I2C wires if possible (>1 meter problematic)
4. Remove other I2C devices to test
5. Check for capacitive coupling (shield wires, keep SCL/SDA apart from high-current paths)

---

### Scenario C: Wire Library Not Working

**Signs**: Debug Level 1 sketch hangs at Wire.begin()

**Recovery**:
1. Check Arduino IDE board selection matches actual board
2. Try different I2C frequency
3. Reset Arduino by uploading blink sketch, then your code
4. Check for custom Wire library conflicts
5. Update Arduino IDE and board definitions

---

## Testing Checklist

Before shipping or deploying code:

- [ ] I2C scanner finds BNO085 at expected address
- [ ] Code initializes at 100kHz without hanging
- [ ] Code initializes at 400kHz without hanging
- [ ] Sensor readings are reasonable (quaternion magnitude ≈ 1.0)
- [ ] Calibration status reaches "Medium" or "High" after warm-up
- [ ] Power cycling doesn't cause hang
- [ ] Other I2C devices don't interfere with BNO085
- [ ] Code handles sensor reset gracefully (wasReset() check)
- [ ] Pull-up resistors measured (4.7kΩ ± 10%)
- [ ] VCC voltage stable (3.3V ± 5%)

---

## Additional Resources

**Adafruit BNO085 Breakout**:
- Product Page: https://www.adafruit.com/product/4754
- Datasheet: BNO085 Absolute Orientation IMU
- Adafruit Library: https://github.com/adafruit/Adafruit_BNO08x_Arduino

**Arduino I2C Troubleshooting**:
- Arduino Wire Library Reference: https://www.arduino.cc/reference/en/language/functions/communication/wire/
- I2C Specification: 100kHz (standard), 400kHz (fast mode)
- Pull-up Sizing: 4.7kΩ typical for 3.3V buses

**Common I2C Issues**:
- Pull-up resistor calculation: R = VCC / (10 × I_sink) for 400kHz
- For 3.3V @ 400kHz: 3.3V / (10 × 3mA) ≈ 110Ω minimum, 4.7kΩ typical
- Bus capacitance: <400pF typical for short runs, <100pF for 400kHz

---

## Summary Decision Table

| Symptom | Likely Cause | Test | Fix |
|---------|--------------|------|-----|
| Hangs at begin_I2C, scanner finds device | Weak pull-ups or high clock | Try 100kHz | Add 4.7kΩ pull-ups, reduce clock |
| Hangs at Wire.begin(), scanner hangs | Wire library issue | Try different board config | Reset Arduino, update IDE |
| Scanner finds no devices, VCC=3.3V | Device not responding | Check DI pin, power cycle | Tie DI pin, power cycle sensor |
| Scanner finds no devices, VCC=0V | No power | Check power supply | Verify 3.3V supply |
| Code works at 50kHz only | Bus quality issue | Try different clock speeds | Check wiring, add pull-ups |
| Address found at 0x4B not 0x4A | DI pin not tied to GND | Check DI pin connection | Tie DI to GND, update code |

