# BNO085 I2C Implementation Guide

**Status**: Research Complete - Ready for Implementation  
**Date**: 2026-05-06  
**Author**: Claude (Research via Adafruit BNO08x Documentation)  
**Focus**: Proper I2C initialization for BNO085 on Arduino Mega using Adafruit BNO08x library

---

## Executive Summary

The current BNO085 driver implementation (`src/sensors/bno085.cpp`) is written for **UART mode** but the hardware is configured for **I2C mode** (pins 20/21 on Arduino Mega). This document provides the complete specification for proper I2C initialization, including:

- **Initialization checklist** for I2C setup
- **Code migration path** from UART to I2C
- **Absolute orientation/rotation vector** output configuration (same in I2C as UART)
- **Quaternion data format** verification
- **Common I2C issues** and solutions specific to BNO085

### Key Findings

1. **Adafruit `begin_I2C()` method** is the standard way to initialize on I2C
2. **Default I2C address is 0x4A** (configurable to 0x4B via DI pin)
3. **I2C clock speed should be 400 kHz** (standard) after initialization
4. **Absolute Orientation (SH2_ROTATION_VECTOR) works identically** in I2C and UART
5. **Quaternion format is scalar-first**: w (real), x (i), y (j), z (k)
6. **Clock stretching and pull-up resistor issues** are known BNO085 hardware quirks
7. **Wire library on Mega** has onboard pull-ups on pins 20/21

---

## Part 1: I2C Initialization Specification

### 1.1 Hardware Requirements

#### Arduino Mega I2C Pins
- **SDA (Serial Data)**: Pin 20
- **SCL (Serial Clock)**: Pin 21
- **Pull-ups**: Arduino Mega has onboard 47kΩ pull-up resistors built in

#### I2C Address Configuration

```
Default Address:  0x4A (1001010)
Alternative:      0x4B (1001011) - Set by pulling DI pin to VCC
```

Verify DI pin connection:
- DI = GND → Address is 0x4A
- DI = VCC → Address is 0x4B

#### Wiring Checklist

```
BNO085 Pin          Arduino Mega
=========================================
VCC (3.3V)      →  3.3V supply
GND             →  GND
SDA             →  Pin 20
SCL             →  Pin 21
DI              →  GND (for default 0x4A address)
PS0, PS1        →  GND (select I2C mode)
INT             →  Optional (not required for basic operation)
RST             →  Optional (can use -1 for no reset pin)
```

### 1.2 Wire Library Initialization

The Arduino Mega's Wire library automatically uses pins 20/21 when `Wire.begin()` is called with no parameters:

```cpp
// In setup()
void setup() {
  Serial.begin(115200);
  
  // Initialize I2C bus - automatically uses pins 20/21 on Mega
  Wire.begin();
  
  // Optional: Set I2C clock speed to 400 kHz
  // Must be called AFTER Wire.begin()
  Wire.setClock(400000L);  // 400 kHz is standard, max 1 MHz
  
  // Initialize BNO085 via I2C
  if (!bno085.begin()) {
    Serial.println("Failed to initialize BNO085");
    while (1) delay(10);
  }
}
```

**Important**: Wire.setClock() must be called AFTER Wire.begin() to take effect.

### 1.3 Adafruit BNO08x begin_I2C() Method

#### Method Signature

```cpp
bool begin_I2C(uint8_t i2c_addr = BNO08x_I2CADDR_DEFAULT,
               TwoWire *wire = &Wire, 
               int32_t sensor_id = 0);
```

#### Parameters

| Parameter | Default | Notes |
|-----------|---------|-------|
| `i2c_addr` | 0x4A | I2C address of sensor. Change to 0x4B if DI pin pulled high |
| `wire` | &Wire | Which Wire/I2C bus. Use &Wire for Mega's primary I2C |
| `sensor_id` | 0 | Internal sensor ID (for multiple sensors, not supported) |

#### Return Value

- `true` if initialization successful
- `false` if sensor not found at address or communication failed

#### Complete Implementation Example

```cpp
#include <Wire.h>
#include <Adafruit_BNO08x.h>

Adafruit_BNO08x bno08x;

bool initializeBNO085_I2C() {
  // Initialize I2C bus on Mega (pins 20/21)
  Wire.begin();
  Wire.setClock(400000L);  // 400 kHz
  
  // Initialize BNO085 via I2C
  // Address 0x4A is default (DI pin to GND)
  if (!bno08x.begin_I2C(0x4A, &Wire, 0)) {
    Serial.println("ERROR: BNO085 not found at 0x4A");
    return false;
  }
  
  Serial.println("BNO085 initialized on I2C at 0x4A");
  return true;
}
```

---

## Part 2: Migration from UART to I2C

### 2.1 Current UART Implementation Issues

The current `bno085.cpp` uses:
```cpp
Serial1.begin(115200);
imu_->begin_UART(&Serial1);  // ← UART mode
```

But the hardware is wired for I2C (pins 20/21), not UART (pins 18/19).

### 2.2 Required Changes

#### Change 1: Include Wire library

```cpp
// Add to top of bno085.cpp
#include <Wire.h>
```

#### Change 2: Replace begin() method

**Original (UART):**
```cpp
bool BNO085::begin() {
  imu_ = new Adafruit_BNO08x();
  if (!imu_) return false;
  
  HardwareSerial *uart_stream = nullptr;
  
  #if defined(__AVR_ATmega2560__)
    Serial1.begin(115200);
    uart_stream = &Serial1;
  #endif
  
  if (!imu_->begin_UART(uart_stream)) {
    delete imu_;
    return false;
  }
  // ... rest
}
```

**Replacement (I2C):**
```cpp
bool BNO085::begin() {
  imu_ = new Adafruit_BNO08x();
  if (!imu_) return false;
  
  // Initialize I2C bus (Mega uses pins 20/21 automatically)
  Wire.begin();
  Wire.setClock(400000L);  // 400 kHz standard speed
  
  // Initialize BNO085 via I2C
  // Default address 0x4A (DI pin to GND)
  if (!imu_->begin_I2C(0x4A, &Wire, 0)) {
    delete imu_;
    imu_ = nullptr;
    initialized_ = false;
    return false;
  }
  
  // Enable the rotation vector (absolute orientation) report
  // SH2_ROTATION_VECTOR = 0x05
  // Period: 100ms (10 Hz, or 100000 microseconds)
  if (!imu_->enableReport(SH2_ROTATION_VECTOR, 100000)) {
    delete imu_;
    imu_ = nullptr;
    initialized_ = false;
    return false;
  }
  
  initialized_ = true;
  last_read_ms_ = millis();
  
  return true;
}
```

#### Change 3: Update pins.h

No longer needed for UART, but keep for reference:

```cpp
#ifndef PINS_H
#define PINS_H

// ============================================================================
// I2C Pins for BNO085 (Arduino Mega)
// ============================================================================
// Arduino Mega uses hardwired pins for I2C:
// SDA (Serial Data)   = Pin 20
// SCL (Serial Clock)  = Pin 21
// Pull-ups: 47kΩ onboard

// BNO085 I2C Address (set by DI pin)
#define BNO085_I2C_ADDRESS 0x4A  // Default (DI to GND)
// #define BNO085_I2C_ADDRESS 0x4B  // Alternative (DI to VCC)

// I2C Clock Speed
#define BNO085_I2C_SPEED 400000L  // 400 kHz (standard)

// ... other pins ...
#endif
```

### 2.3 Code Comparison: UART vs I2C

| Aspect | UART Mode | I2C Mode |
|--------|-----------|----------|
| Arduino Pins | 18 (RX), 19 (TX) | 20 (SDA), 21 (SCL) |
| Baud Rate | 115200 bps | Not applicable |
| Method | `begin_UART(stream)` | `begin_I2C(addr, wire)` |
| Initialization | Serial1.begin(115200) | Wire.begin(); Wire.setClock(400kHz) |
| Report Config | `enableReport()` | `enableReport()` (identical) |
| Data Reading | `getSensorEvent()` | `getSensorEvent()` (identical) |

---

## Part 3: Absolute Orientation Output Configuration

### 3.1 Rotation Vector (Absolute Orientation)

The BNO085 provides three orientation modes:

| Report Type | Uses | Best For |
|------------|------|----------|
| **Absolute Rotation Vector (0x05)** | Accel + Gyro + Mag | Highest accuracy, magnetic reference |
| Geomagnetic Vector (0x04) | Accel + Mag | Lower power, no gyro drift |
| Game Rotation Vector (0x08) | Accel + Gyro only | Gaming/AR, smooth without mag |

**We use: SH2_ROTATION_VECTOR (0x05) for absolute orientation.**

### 3.2 Enabling Rotation Vector Output

Same in both UART and I2C modes:

```cpp
// Enable absolute orientation (rotation vector)
imu_->enableReport(SH2_ROTATION_VECTOR, 100000);
// Parameters:
//   - SH2_ROTATION_VECTOR = report ID 0x05
//   - 100000 microseconds = 100 ms period = 10 Hz update rate
```

### 3.3 Update Rates and Timing

The period parameter in `enableReport()` sets the update frequency:

```cpp
// Common update rates:
imu_->enableReport(SH2_ROTATION_VECTOR, 10000);   // 10 ms   = 100 Hz
imu_->enableReport(SH2_ROTATION_VECTOR, 50000);   // 50 ms   = 20 Hz
imu_->enableReport(SH2_ROTATION_VECTOR, 100000);  // 100 ms  = 10 Hz (current)
imu_->enableReport(SH2_ROTATION_VECTOR, 1000000); // 1000 ms = 1 Hz
```

**Typical I2C bandwidth** at 400 kHz:
- Quaternion report ≈ 20 bytes payload
- 10 Hz updates = 200 bytes/second
- Well within I2C capacity (theoretically 50+ kBytes/sec at 400 kHz)

### 3.4 Quaternion Data Format

#### Quaternion Representation

The BNO085 uses **scalar-first** quaternion format (w, x, y, z):

```
w (real/scalar) = rotation amount
x (i)           = rotation axis x component
y (j)           = rotation axis y component
z (k)           = rotation axis z component
```

#### Data Mapping in Adafruit Library

From `sh2_RotationVector_t` structure:

```cpp
struct sh2_RotationVector_t {
  float real;   // w component (scalar)
  float i;      // x component
  float j;      // y component
  float k;      // z component
  uint8_t accuracy;  // Estimated accuracy in radians
};
```

#### Current Implementation (bno085.cpp:165-171)

This is **correct**:

```cpp
orientation_.w = sensor_value.un.rotationVector.real;
orientation_.x = sensor_value.un.rotationVector.i;
orientation_.y = sensor_value.un.rotationVector.j;
orientation_.z = sensor_value.un.rotationVector.k;
```

#### Quaternion Normalization

The BNO085 outputs normalized quaternions where:
```
w² + x² + y² + z² ≈ 1.0
```

Current magnitude check is good practice:

```cpp
float magnitude = sqrt(w*w + x*x + y*y + z*z);
if (magnitude < 0.99f || magnitude > 1.01f) {
  // Warning: quaternion not normalized
}
```

### 3.5 Calibration Status in I2C Mode

Same as UART mode - the `sensor_value.status` field provides overall calibration level:

```cpp
// From bno085.cpp:174-182
orientation_.cal_status = sensor_value.status;  // 0=unreliable, 1=low, 2=medium, 3=high

// Per-axis calibration not available in rotation vector report
orientation_.cal_accel = sensor_value.status;
orientation_.cal_gyro = sensor_value.status;
orientation_.cal_mag = sensor_value.status;
```

#### Calibration Status Meanings

| Value | Label | Meaning |
|-------|-------|---------|
| 0 | Unreliable | No valid calibration data yet |
| 1 | Low | Initial calibration in progress |
| 2 | Medium | Acceptable for most applications |
| 3 | High | Optimal calibration |

**Note:** BNO085 self-calibrates automatically. Best results achieved with:
- Device movement in all axes during first 30 seconds
- Stable environment (minimal magnetic interference)
- No rapid temperature changes

---

## Part 4: Sensor Reading and Data Verification

### 4.1 Reading Data in I2C Mode

Identical to UART mode - no changes needed:

```cpp
sh2_SensorValue_t sensor_value;
if (!imu_->getSensorEvent(&sensor_value)) {
  // No new data available
  return false;
}

// Verify it's a rotation vector report
if (sensor_value.sensorId != SH2_ROTATION_VECTOR) {
  // Different report type received
  return false;
}

// Extract quaternion
orientation_.w = sensor_value.un.rotationVector.real;
orientation_.x = sensor_value.un.rotationVector.i;
orientation_.y = sensor_value.un.rotationVector.j;
orientation_.z = sensor_value.un.rotationVector.k;
```

### 4.2 Checking for New Data

Use `hasNewData()` flag to avoid processing stale data:

```cpp
if (sensor.hasNewData()) {
  OrientationData quat = sensor.getOrientation();
  // Process quaternion...
}
```

### 4.3 Typical Update Behavior

At 10 Hz (100 ms period):
- `getSensorEvent()` returns true approximately every 100 ms
- Between updates, returns false
- Data is available immediately after reset
- No explicit polling interval needed - library handles timing

### 4.4 Accuracy and Stability Expectations

| Metric | Value | Notes |
|--------|-------|-------|
| Orientation Accuracy | ±2-3° | After calibration; depends on environment |
| Update Latency | < 10 ms | At 10 Hz reporting (depends on I2C speed) |
| Drift Over 1 Hour | ~0.2° | Gyroscope drift; mag-referenced prevents large drift |
| Temperature Drift | ±1°/10°C | Typical for MEMS sensors |

Best-case accuracy requires:
- At least "Medium" (level 2) calibration
- Stable environment (no rapid mag field changes)
- I2C bus free of noise/interference

---

## Part 5: Common I2C Issues and Solutions

### 5.1 Clock Stretching Problem (Known BNO085 Hardware Issue)

#### The Problem

The BNO085 violates I2C protocol timing during certain clock-stretching cycles:
- SDA-to-SCL setup time is occasionally on the edge of spec
- Some microcontrollers (especially older/slower ones) may timeout
- Error manifests as "I2C address not found" or intermittent read failures

#### Solution 1: Unequal Pull-up Resistors (Recommended)

The Arduino Mega already has 47kΩ onboard pull-ups. To fix clock stretching:

**Option A**: Add external 4.7kΩ resistor to SCL line only:

```
3.3V ─────[4.7kΩ]───┬─── SCL (pin 21)
                     │
                   BNO085 SCL
```

This creates a weaker external pull-up on SCL, speeding up rise time and reducing timing violations.

**Option B**: Add 2-3kΩ resistor to SDA line only (alternative):

```
3.3V ─────[2.7kΩ]───┬─── SDA (pin 20)
                     │
                   BNO085 SDA
```

This accelerates SDA rise time relative to SCL, increasing setup time margin.

#### Solution 2: Software Workaround (If Hardware Mods Not Feasible)

Reduce I2C clock speed:

```cpp
Wire.setClock(100000L);  // Reduced from 400 kHz to 100 kHz
```

Lower speed gives more time for I2C transactions, avoiding clock-stretching timeouts.

### 5.2 I2C Address Not Found

If initialization fails with "I2C address not found":

**Check 1: Verify Wiring**
- SDA pin 20 connected to BNO085 SDA
- SCL pin 21 connected to BNO085 SCL
- VCC (3.3V) connected
- GND connected
- Continuity check with multimeter

**Check 2: Verify Address Setting**
```cpp
// If DI pin is to GND (default), address is 0x4A
if (!bno08x.begin_I2C(0x4A)) { ... }

// If DI pin is to VCC, address is 0x4B
if (!bno08x.begin_I2C(0x4B)) { ... }

// Scan bus to find actual address:
for (uint8_t i = 0; i < 127; i++) {
  Wire.beginTransmission(i);
  if (Wire.endTransmission() == 0) {
    Serial.print("Found device at 0x");
    Serial.println(i, HEX);
  }
}
```

**Check 3: Verify Power and Reset**
- VCC should be stable 3.3V (check with multimeter)
- If RST pin is connected, ensure it's not held low
- Try power cycle: disconnect VCC for 2 seconds, reconnect

**Check 4: Try Reduced Speed**
```cpp
Wire.setClock(100000L);  // Slow down if clock-stretching is the issue
```

### 5.3 Intermittent or Corrupted Data

#### Symptoms
- Occasional `getSensorEvent()` returning garbage quaternion values
- Sporadic initialization failures
- Data dropouts or noise

#### Cause Analysis

Most likely issues in order:
1. **Clock stretching timeout** → See 5.1
2. **I2C bus noise** → EMI from power circuits, motor controllers, etc.
3. **Inadequate pull-ups** → Use resistors in 2-10kΩ range
4. **Voltage sag** → 3.3V supply dropping under transient load

#### Solutions

**For clock stretching**:
```cpp
// Add external resistor (see 5.1) OR reduce clock:
Wire.setClock(200000L);  // Try intermediate speed
```

**For noise**:
- Keep I2C wiring short and separate from high-current paths
- Use twisted-pair or shielded cable for long runs
- Add 100nF capacitor between VCC and GND near BNO085
- Check for ground loops

**For power**:
- Verify 3.3V supply rated for at least 50 mA
- Add 10µF capacitor on VCC near sensor
- Check voltage under load with oscilloscope

### 5.4 Multiple Devices on Same I2C Bus

If other I2C devices are present (e.g., EEPROM, RTC):

**Limitation**: Adafruit BNO08x library only supports **one BNO085 per I2C bus**. Cannot have multiple BNO085s on same bus.

**Safe coexistence** with other I2C devices:
- Different I2C addresses for other devices
- Same pull-ups work for all devices
- Clock speed should be compatible with slowest device
- Ensure no address conflicts:

```cpp
// Scan to find all devices
for (uint8_t addr = 0x01; addr < 127; addr++) {
  Wire.beginTransmission(addr);
  byte error = Wire.endTransmission();
  if (error == 0) {
    Serial.print("Device found: 0x");
    Serial.println(addr, HEX);
  }
}
```

---

## Part 6: Implementation Checklist

### Pre-Implementation Hardware Verification

- [ ] Verify BNO085 breakboard has **I2C mode selected**
  - Check PS0 and PS1 pins are tied to GND
  - UART mode uses PS1=HIGH or PS0=HIGH
  - I2C mode uses PS0=GND, PS1=GND

- [ ] Check **DI pin** connection
  - [ ] DI tied to GND for address 0x4A (default) 
  - [ ] OR DI tied to VCC for address 0x4B (alternative)

- [ ] Verify **wiring** on Arduino Mega
  - [ ] BNO085 VCC → Mega 3.3V
  - [ ] BNO085 GND → Mega GND
  - [ ] BNO085 SDA → Mega pin 20
  - [ ] BNO085 SCL → Mega pin 21

- [ ] Check **power supply**
  - [ ] 3.3V supply capable of 50+ mA
  - [ ] Voltage stable under load
  - [ ] Decoupling capacitor (10µF) on VCC

- [ ] Consider **pull-up resistor mod** (optional but recommended)
  - [ ] Add 4.7kΩ to SCL OR 2.7kΩ to SDA if clock-stretching issues occur

### Code Implementation Steps

- [ ] Add `#include <Wire.h>` to bno085.cpp

- [ ] Replace `begin()` method with I2C version
  - [ ] Remove Serial1.begin() and begin_UART() calls
  - [ ] Add Wire.begin()
  - [ ] Add Wire.setClock(400000L)
  - [ ] Call imu_->begin_I2C(0x4A, &Wire, 0)

- [ ] Update pins.h for reference
  - [ ] Add BNO085_I2C_ADDRESS constant
  - [ ] Add BNO085_I2C_SPEED constant
  - [ ] Remove/comment UART pin definitions

- [ ] Test initialization
  - [ ] Compile without errors
  - [ ] Upload to Mega
  - [ ] Check Serial output for "BNO085 initialized"

- [ ] Test data reading
  - [ ] Verify quaternion values update every 100 ms
  - [ ] Check calibration status reaches level 2 (Medium) after 30 sec
  - [ ] Rotate device and verify quaternion changes

- [ ] Stability testing
  - [ ] Run for > 5 minutes continuously
  - [ ] Check for dropped packets or noise in data
  - [ ] If issues occur, implement pull-up resistor mod

### Verification After Implementation

- [ ] `begin()` returns true during startup
- [ ] `read()` returns true regularly (10+ times per second at 10 Hz)
- [ ] `hasNewData()` is true periodically (~every 100 ms)
- [ ] Quaternion magnitude is 0.99-1.01 (normalized)
- [ ] Calibration status reaches 2 (Medium) or 3 (High)
- [ ] No Serial errors or dropped data
- [ ] Quaternion changes when device is rotated

---

## Part 7: Reference Documentation

### Adafruit BNO08x Library

- **GitHub**: https://github.com/adafruit/Adafruit_BNO08x
- **Examples**: https://github.com/adafruit/Adafruit_BNO08x/tree/master/examples
- **API Reference**: https://adafruit.github.io/Adafruit_BNO08x/html/

### BNO085 Datasheets

- **Bosch BNO080/085 Datasheet**: Full technical specification
  - I2C/UART/SPI protocols
  - FRS (Feature Record Store) for calibration
  - Register maps and timing specifications

### Arduino Mega Pinout

- **I2C Pins**: 20 (SDA), 21 (SCL)
- **Power**: 5V and 3.3V outputs
- **Note**: Onboard pull-ups ~47kΩ on I2C pins

### Quaternion Conventions

- **Standard Format**: w, x, y, z (scalar first)
- **BNO085 Output**: real, i, j, k (equivalent)
- **Normalization**: |q| ≈ 1.0
- **Representation**: Unit quaternion for 3D rotation

---

## Part 8: Code Snippets and Examples

### Minimal I2C Initialization

```cpp
#include <Wire.h>
#include <Adafruit_BNO08x.h>

Adafruit_BNO08x bno08x;

void setup() {
  Serial.begin(115200);
  
  Wire.begin();
  Wire.setClock(400000L);
  
  if (!bno08x.begin_I2C(0x4A)) {
    Serial.println("Failed to initialize BNO085");
    while (1) delay(100);
  }
  
  Serial.println("BNO085 OK on I2C");
  bno08x.enableReport(SH2_ROTATION_VECTOR, 100000);
}

void loop() {
  sh2_SensorValue_t sensorValue;
  
  if (bno08x.getSensorEvent(&sensorValue)) {
    if (sensorValue.sensorId == SH2_ROTATION_VECTOR) {
      float w = sensorValue.un.rotationVector.real;
      float x = sensorValue.un.rotationVector.i;
      float y = sensorValue.un.rotationVector.j;
      float z = sensorValue.un.rotationVector.k;
      
      Serial.print("Q: ");
      Serial.print(w); Serial.print(", ");
      Serial.print(x); Serial.print(", ");
      Serial.print(y); Serial.print(", ");
      Serial.println(z);
    }
  }
}
```

### I2C Address Discovery

```cpp
void scanI2C() {
  Serial.println("Scanning I2C bus...");
  
  for (uint8_t address = 0x01; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print("Found device at 0x");
      Serial.println(address, HEX);
    }
  }
  
  Serial.println("Scan complete");
}
```

### Safe Clock Stretching Workaround

```cpp
// If experiencing clock stretching issues:
void setup() {
  Wire.begin();
  Wire.setClock(200000L);  // 200 kHz instead of 400 kHz
  
  // Optional: even slower if still having issues
  // Wire.setClock(100000L);  // 100 kHz (slowest standard)
  
  if (!bno08x.begin_I2C(0x4A)) {
    Serial.println("Failed to initialize BNO085");
    // Try alternate approach...
  }
}
```

---

## Summary

The Adafruit BNO08x library provides straightforward I2C initialization via `begin_I2C()`. The current implementation's rotation vector output code is correct and doesn't need changes - only the initialization path needs to switch from UART to I2C.

Key points:
- **Wire.begin()** initializes I2C on pins 20/21
- **Wire.setClock(400000L)** sets standard speed (call after Wire.begin())
- **begin_I2C(0x4A)** initializes sensor at default address
- **enableReport(SH2_ROTATION_VECTOR, 100000)** same as UART mode
- **getSensorEvent()** and quaternion reading unchanged
- **Clock stretching** is a known BNO085 quirk; resistor mods help if needed

---

## Revision History

| Date | Author | Changes |
|------|--------|---------|
| 2026-05-06 | Claude | Initial research complete - full I2C specification documented |

