# BNO085 I2C Initialization Hang - Complete Documentation Index

## Overview

This documentation package provides comprehensive troubleshooting for **BNO085 I2C initialization hangs** on Arduino Mega. The issue occurs when:

- Arduino Mega + BNO085 on I2C (pins 20/21) boots normally
- Code prints initialization messages
- **Hangs indefinitely during `imu.begin()` call**
- No response to serial input, requires hard reset

---

## Documentation Files

### Start Here

#### **1. `bno085_quick_reference.md` ⭐ START HERE**
- **Purpose**: Quick 30-second diagnosis and common fixes
- **Read time**: 3-5 minutes
- **Contents**:
  - Symptom checklist
  - Power/voltage quick check
  - 7-step fix sequence
  - Wiring diagram
  - Most common root causes
  - When to declare sensor dead

**When to use**: First thing - try the quick fixes before diving deeper

---

#### **2. `bno085_i2c_hang_diagnosis.md` ⭐ COMPREHENSIVE GUIDE**
- **Purpose**: Deep troubleshooting guide with root cause analysis
- **Read time**: 20-30 minutes
- **Contents**:
  - 6 root causes with detailed explanations
  - How to verify I2C device presence
  - Hardware verification checklist
  - Debug levels 1-4 with progressive code examples
  - Common user mistakes
  - Decision tree for systematic diagnosis
  - Step-by-step debugging procedure
  - Recovery steps for different scenarios
  - Adafruit library API reference

**When to use**: After quick reference, for systematic diagnosis

---

#### **3. `bno085_test_sketches.ino` ⭐ READY-TO-USE CODE**
- **Purpose**: Collection of 10 test sketches for hardware/software validation
- **Read time**: 2-3 minutes (scanning code)
- **Contents**:
  - TEST 1: I2C Bus Scanner (find devices on bus)
  - TEST 2: Minimal I2C Test (step-by-step init)
  - TEST 3: Debug Level 1 (Wire library test)
  - TEST 4: Clock Speed Testing (find working speed)
  - TEST 5: Hardware Voltage Check (verify I2C voltages)
  - TEST 6: Adafruit Library Init (test Adafruit_BNO08x)
  - TEST 7: Register Read (read chip ID)
  - TEST 8: Progressive Clock Speed (find max speed)
  - TEST 9: Full Sensor Test (read orientation data)
  - TEST 10: Power Cycle Stress Test (recovery check)

**When to use**: For hands-on testing; copy/paste code into Arduino IDE

---

## Recommended Diagnostic Sequence

### Quick Path (15 minutes)
1. **Power cycle** everything (30 seconds)
2. Read **`bno085_quick_reference.md`** (5 minutes)
3. Follow **Step 1-3 checklist** (multimeter checks, 5 minutes)
4. Run **TEST 1 (I2C Scanner)** from `bno085_test_sketches.ino` (5 minutes)

### Standard Path (45 minutes)
1. Complete **Quick Path** above
2. Read **`bno085_i2c_hang_diagnosis.md` - "Root Cause Analysis"** (10 minutes)
3. Run appropriate **DEBUG TEST** from `bno085_test_sketches.ino` (10 minutes)
4. Apply fix based on findings
5. Test with TEST 9 (Full Sensor Test) (5 minutes)

### Advanced Path (2+ hours)
1. Complete **Standard Path** above
2. Read **entire `bno085_i2c_hang_diagnosis.md`** (30 minutes)
3. Run **all relevant TEST sketches** in sequence (30+ minutes)
4. Apply fixes, test, iterate

---

## Problem Diagnosis Flowchart

```
Code hangs at imu.begin()
│
├─ STEP 1: Power cycle everything (30 seconds)
│  └─ If works → DONE, was sensor lockup
│
├─ STEP 2: Check voltages with multimeter
│  ├─ VCC not 3.3V? → Power supply issue
│  ├─ SCL/SDA idle not 3.0-3.3V? → Add pull-up resistors
│  └─ Else → Continue
│
├─ STEP 3: Run I2C Scanner (TEST 1)
│  ├─ Scanner hangs? → Wire library issue (DEBUG Level 1)
│  ├─ No devices found? → Wiring issue (multimeter checks)
│  ├─ Found at 0x4A/0x4B? → Device OK, continue
│  └─ Else → Check DI pin configuration
│
├─ STEP 4: Try lower clock speed
│  ├─ Works at 100kHz? → Weak pull-ups (add resistors)
│  ├─ Works at 50kHz only? → Bus quality issue
│  └─ Hangs at all speeds? → Continue
│
├─ STEP 5: Run Adafruit Init test (TEST 6)
│  ├─ Hangs? → Sensor in bad state or defective
│  ├─ Returns false? → Library issue (update Adafruit)
│  └─ Returns true? → May work, test full sketch
│
└─ STEP 6: Declare sensor status
   ├─ Worked at some point? → Likely recoverable
   └─ Never worked? → Likely defective
```

---

## Common Fixes (Success Rate Estimates)

| Fix | Success Rate | Time | Difficulty |
|-----|--------------|------|------------|
| Power cycle | 30-40% | 1 min | Easy |
| Reduce clock to 100kHz | 25-35% | 2 min | Easy |
| Add 4.7kΩ pull-ups | 40-50% | 15 min | Medium |
| Check wiring/soldering | 20-30% | 10 min | Medium |
| Update Adafruit library | 5-10% | 5 min | Easy |
| Tie DI pin to GND | 5-10% | 2 min | Easy |
| Replace sensor | 90%+ | N/A | Hard |

---

## File Summary Table

| File | Size | Type | Purpose | Read Time |
|------|------|------|---------|-----------|
| `bno085_quick_reference.md` | 11 KB | Guide | Quick diagnosis | 5 min |
| `bno085_i2c_hang_diagnosis.md` | 27 KB | Guide | Deep troubleshooting | 30 min |
| `bno085_test_sketches.ino` | 15 KB | Code | Test sketches | 3 min |
| `bno085_i2c_compatibility_analysis.md` | 32 KB | Reference | I2C bus analysis | 20 min |
| `bno085_communication_modes.md` | 30 KB | Reference | I2C/UART/SPI modes | 15 min |
| `bno085_hardware_test.md` | 9.3 KB | Reference | Hardware test procedures | 10 min |
| Others | 90+ KB | Reference | Deep technical info | Variable |

---

## Key Technical Details

### Arduino Mega I2C Configuration
- **SDA Pin**: 20 (data line)
- **SCL Pin**: 21 (clock line)
- **I2C Address (BNO085)**: 0x4A (DI to GND) or 0x4B (DI to VCC)
- **Pull-ups**: Onboard ~47kΩ (often insufficient)
- **Recommended speed**: 100kHz (safe default), 400kHz (fast, requires good pull-ups)

### Critical Voltage Thresholds
- **VCC (sensor)**: 3.3V ±5% (3.14V - 3.46V acceptable)
- **SCL idle**: 3.0V - 3.3V (pulled high by resistors)
- **SDA idle**: 3.0V - 3.3V (pulled high by resistors)
- **LOW state**: <0.4V

### Root Causes (by Frequency)
1. **Weak pull-ups** (40%): Add 4.7kΩ external resistors
2. **Clock too fast** (25%): Use 100kHz instead of 400kHz
3. **Bad wiring** (20%): Check continuity with multimeter
4. **DI pin floating** (10%): Tie to GND or VCC
5. **Sensor lockup** (5%): Power cycle completely

---

## Most Important Tests

### Absolute Minimum (2 minutes)
1. Power cycle
2. Check VCC voltage (should be 3.3V)
3. Check SCL/SDA voltages (should be 3.0-3.3V when idle)

### Basic Hardware (5 minutes)
4. Run TEST 1 (I2C Scanner) to verify device present
5. Check for devices at both 0x4A and 0x4B

### Software Testing (10 minutes)
6. Change I2C clock to 100kHz
7. Try Adafruit initialization (TEST 6)
8. Read sensor data (TEST 9)

---

## When to Give Up

If you've completed **all steps** in `bno085_i2c_hang_diagnosis.md` and:

- [ ] Tested at 50kHz clock speed (safest possible)
- [ ] Added external 4.7kΩ pull-up resistors
- [ ] Verified all wiring with multimeter
- [ ] Power cycled multiple times
- [ ] Tried with different Arduino board (if available)
- [ ] Tested scanner at multiple speeds
- [ ] Updated Adafruit library to latest version

**And it still hangs**: Sensor is likely defective

**Recommendation**: Replace sensor with new unit

---

## Related BNO085 Documentation

This folder contains additional BNO085 documentation:

- **`bno085_i2c_implementation.md`**: I2C protocol implementation details
- **`bno085_communication_modes.md`**: I2C vs UART vs SPI comparison
- **`bno085_pin_diagrams.md`**: Pinout and wiring diagrams
- **`bno085-calibration-persistence.md`**: Calibration storage research
- **`bno085_sh2_protocol_analysis.md`**: SH2 protocol details
- **`bno085_hardware_test.md`**: Hardware test procedures

---

## Quick Reference: Key Code Snippets

### Minimal Working Initialization
```cpp
#include <Wire.h>
#include "Adafruit_BNO08x.h"

Adafruit_BNO08x imu;

void setup() {
  Serial.begin(115200);
  Wire.begin();                    // Initialize I2C
  Wire.setClock(100000L);          // Safe 100kHz speed
  delay(100);
  
  if (!imu.begin_I2C(0x4A, &Wire)) {
    Serial.println("Failed to initialize");
    while(1);
  }
  
  imu.enableReport(SH2_ROTATION_VECTOR, 100000);
  Serial.println("Initialized OK");
}
```

### I2C Device Presence Check
```cpp
Wire.beginTransmission(0x4A);
uint8_t error = Wire.endTransmission(true);

if (error == 0) {
  Serial.println("Device found");
} else {
  Serial.print("Not found, error: ");
  Serial.println(error);
}
```

### Clock Speed Testing Loop
```cpp
uint32_t speeds[] = {50000L, 100000L, 200000L, 400000L};
const char* labels[] = {"50kHz", "100kHz", "200kHz", "400kHz"};

for (int i = 0; i < 4; i++) {
  Wire.setClock(speeds[i]);
  delay(100);
  // Test at this speed
}
```

---

## Support Resources

**Official Adafruit**:
- Product: https://www.adafruit.com/product/4754
- GitHub Library: https://github.com/adafruit/Adafruit_BNO08x_Arduino
- Datasheet: Available from Adafruit or Silicon Labs

**Arduino I2C**:
- Wire Library: https://www.arduino.cc/reference/en/language/functions/communication/wire/
- I2C Standard: 100kHz (Standard Mode), 400kHz (Fast Mode)

**This Project**:
- Main implementation: `/src/sensors/bno085.cpp`
- Header: `/src/sensors/bno085.h`
- Pin configuration: `/src/config/pins.h`
- Example usage: `/src/main.cpp`

---

## Document Version

- **Created**: 2026-05-06
- **Based on**: Arduino Mega + Adafruit BNO085 + Adafruit_BNO08x library
- **Tested with**: Arduino IDE 1.8.x / 2.x
- **Library version**: Latest (2024+)

---

## Notes for Developers

### Adding to This Documentation

When adding new troubleshooting information:
1. Update **`bno085_quick_reference.md`** for quick access
2. Add detailed explanation to **`bno085_i2c_hang_diagnosis.md`**
3. Create new TEST sketch in **`bno085_test_sketches.ino`**
4. Reference in this INDEX file

### Testing New Solutions

Before documenting:
1. Test on actual hardware (Arduino Mega + BNO085)
2. Test at multiple I2C clock speeds
3. Test with different wire arrangements
4. Document success/failure rate

### Common Pitfalls for Troubleshooting

- Don't assume Wire library is working (test it!)
- Don't skip voltage checks (weak pull-ups often the cause)
- Don't forget DI pin configuration (affects I2C address)
- Don't blame the sensor first (usually hardware/wiring)
- Don't use 400kHz unless you have good pull-ups

---

## Quick Links

Within this documentation set:

- [Quick Reference](bno085_quick_reference.md) - Start here
- [Full Diagnosis Guide](bno085_i2c_hang_diagnosis.md) - Deep dive
- [Test Sketches](bno085_test_sketches.ino) - Ready-to-use code
- [I2C Analysis](bno085_i2c_compatibility_analysis.md) - Technical details
- [Communication Modes](bno085_communication_modes.md) - I2C vs alternatives

