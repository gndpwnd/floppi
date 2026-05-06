# BNO085 Communication Modes: Implementation Status & Code Compatibility

**Date**: 2026-05-06  
**Status**: Research Complete - Code Alignment Needed  
**Current Mode**: UART-SHTP (Intended)  
**Hardware Conflict**: User reports I2C mode (pins 20/21) while code uses UART-SHTP (pins 18/19)

---

## Project Context

### Original Design (MDC Session)
- **Mode**: UART (non-RVC, full orientation)
- **Reasoning**: Full pitch/roll support for deployable orientation sensors
- **P1 Pin**: Required HIGH (5V) for UART mode
- **Code Basis**: Uses `begin_UART()` with Serial1

### Current Implementation (This Session)
- **Code Status**: ✅ Compiles successfully for Arduino Mega
- **Memory**: ✅ Only 10% Flash used (plenty of room)
- **Mode Selection**: ✅ UART-SHTP implemented
- **Initialization**: Uses `begin_UART(&Serial1, -1)`
- **Pin Assignment**: Hardcoded to Serial1 (pins 18/19)

### User Observation Conflict
- **User States**: "BNO085 currently working in I2C mode (SCL→pin 21, SDA→pin 20)"
- **Code Shows**: UART-only implementation (pins 18/19)
- **Implication**: Hardware and code may not match

---

## Critical Issues to Resolve

### Issue #1: Hardware/Code Mismatch

| Aspect | Code Says | User Says | Resolution |
|--------|-----------|-----------|------------|
| **Mode** | UART-SHTP | I2C | Verify actual hardware config |
| **Pins** | 18/19 (Serial1) | 20/21 (I2C) | Check physical connections |
| **Initialization** | `begin_UART()` | (implied I2C) | Update code to match hardware |
| **P1 Pin** | Assumed at 5V | Not mentioned | Verify P1 voltage |

**Action Required**: Clarify which mode is actually wired and working.

### Issue #2: P1 Pin Voltage (UART Mode)

From project documentation (MASTER_TASK_LIST.md):
- ✅ "P1 pin must be 5V for UART mode"
- ✅ "Check P1 pin voltage with multimeter"
- ⚠️ Suggests this was a known troubleshooting point

**Status**: 
- If using UART: P1 MUST be connected to 5V (critical!)
- If using I2C: P0 and P1 must be LOW/GND (default)

**Current Code**: No P1 pin control in code (hardware-only configuration)

---

## Code Analysis

### Current bno085.cpp Implementation

```cpp
// Initialization for Arduino Mega
#elif defined(__AVR_ATmega2560__)
  // Arduino Mega: Use Serial1 (hardware UART)
  Serial1.begin(115200);
  uart_stream = &Serial1;
  
  if (!imu_->begin_UART(uart_stream)) {
    // Initialization failed
    return false;
  }
  
  // Enable rotation vector (absolute orientation)
  if (!imu_->enableReport(SH2_ROTATION_VECTOR, 100000)) {
    return false;
  }
```

### What This Code Does

✅ **Correct for UART-SHTP mode**:
- Initializes Serial1 at 115200 bps (UART-SHTP standard)
- Uses `begin_UART()` method (correct for non-RVC mode)
- Enables rotation vector reports (quaternion output)
- Sets 100 ms (10 Hz) update rate

❌ **Issues with I2C Hardware**:
- Won't work if hardware is wired to I2C pins (20/21)
- No I2C initialization method available
- Would fail silently or print error message

⚠️ **Issues with P1 Pin Management**:
- Code doesn't control P1 pin state
- Assumes P1 is already at 5V (hardware configuration)
- No error detection if P1 not set correctly

### Adding I2C Support (If Needed)

If hardware is actually I2C, code needs modification:

```cpp
// Option 1: Compile-time selection
#ifdef USE_I2C_MODE
  #include <Wire.h>
  if (!imu_->begin_I2C(0x4A, &Wire)) {
    return false;
  }
#else  // UART mode (default)
  #include <HardwareSerial.h>
  Serial1.begin(115200);
  if (!imu_->begin_UART(&Serial1)) {
    return false;
  }
#endif

// Option 2: Runtime mode detection
if (detected_mode == MODE_I2C) {
  imu_->begin_I2C(0x4A, &Wire);
} else {
  Serial1.begin(115200);
  imu_->begin_UART(&Serial1);
}

// Option 3: P1 pin control (dynamic UART enable)
#define P1_GPIO_PIN XX  // Define which pin controls P1
digitalWrite(P1_GPIO_PIN, HIGH);  // Set UART mode
delay(100);  // Wait for mode switch
Serial1.begin(115200);
imu_->begin_UART(&Serial1);
```

---

## Compatibility Matrix

### What Works Now

| Configuration | Status | Notes |
|---------------|--------|-------|
| UART-SHTP (Pins 18/19) | ✅ Code Ready | Compiles, hardware must match |
| P1 pin at 5V | ⚠️ Assumed | Not controlled by code |
| Arduino Mega | ✅ Supported | Only platform currently tested |
| Quaternion output | ✅ Implemented | Via `SH2_ROTATION_VECTOR` |
| Calibration status | ✅ Implemented | Status reads 0-3 |

### What Doesn't Work Yet

| Configuration | Status | Notes |
|---------------|--------|-------|
| I2C mode (Pins 20/21) | ❌ Not Implemented | User reports using this |
| Dynamic mode switching | ❌ Not Implemented | Requires P1 GPIO control |
| Arduino Nano/Uno | ❌ Not Supported | No hardware UART available |
| Multiple BNO085 sensors | ❌ Limited | Library supports one sensor |
| RVC mode | ❌ Not Implemented | Different library required |

---

## Library Capability Check

### Adafruit_BNO08x Library Status

```cpp
// Current includes in bno085.cpp:
#include "Adafruit_BNO08x.h"

// Methods available in library:
bool begin_I2C(uint8_t addr = 0x4A, TwoWire *wire = &Wire);
bool begin_UART(Stream *serial, int reset_pin = -1);
bool begin_SPI(int8_t cs_pin, int8_t int_pin, 
               int8_t reset_pin, SPIClass *spi);

// Data reading (same for all modes):
bool getSensorEvent(sh2_SensorValue_t *value);

// Report configuration (same for all modes):
bool enableReport(uint16_t reportID, uint32_t period_us);

// Calibration (SH-2 protocol, available in all modes):
bool getFRS(uint16_t recordID, uint8_t *buffer, 
            uint16_t *len, uint32_t timeout);
bool setFRS(uint16_t recordID, uint8_t *buffer, 
            uint16_t len, uint32_t timeout);
```

**Conclusion**: Library fully supports both I2C and UART modes.

---

## Detailed Mode Implementation Comparison

### I2C Mode (If Hardware Uses This)

**Changes Needed**:
```cpp
// REMOVE this:
#elif defined(__AVR_ATmega2560__)
  Serial1.begin(115200);
  uart_stream = &Serial1;
  if (!imu_->begin_UART(uart_stream)) {

// REPLACE with this:
#elif defined(__AVR_ATmega2560__)
  if (!imu_->begin_I2C(0x4A, &Wire)) {
```

**Advantages of Switch**:
- ✅ Matches user's actual hardware
- ✅ Eliminates P1 pin configuration requirement
- ✅ Default mode (both P0 and P1 pulled LOW by default)
- ✅ Uses well-known Arduino Wire library

**Disadvantages**:
- ⚠️ Slightly lower throughput (~600 B/s vs 1300 B/s)
- ⚠️ Synchronous (blocking) communication
- ⚠️ No CRC error checking in standard I2C

### UART Mode (Current Code)

**What's Required**:
- ✅ Pins 18/19 on Arduino Mega (Serial1)
- ✅ 115200 baud rate (hardcoded in library)
- ✅ P1 pin set to 5V (hardware configuration)
- ✅ P0 pin set to GND (hardware configuration)

**Advantages**:
- ✅ Higher throughput
- ✅ Asynchronous (non-blocking)
- ✅ SHTP protocol with CRC error checking
- ✅ More robust for real-world deployment

**Current Issues**:
- ⚠️ Requires P1 pin configuration
- ⚠️ No code control of P1 state
- ⚠️ User reports hardware doesn't match

---

## Testing Strategy

### Step 1: Verify Actual Hardware Configuration

Before any code changes, determine what's actually wired:

```bash
# Check for I2C device:
i2cdetect -y 1    # If I2C available on Mega
# Look for 0x4A or 0x4B in output

# Check UART connection:
# Use logic analyzer on pins 18/19 for TX/RX activity
# Should see ~115200 bps serial data if UART mode

# Check P0/P1 jumpers:
# Visual inspection of Adafruit breakout board
# P1 should be soldered/closed if UART-SHTP mode
```

### Step 2: Validate Code Against Hardware

```cpp
// Add diagnostic code to identify mode:
void diagnose_bno085_mode() {
  Serial.println("Attempting I2C initialization...");
  if (imu->begin_I2C(0x4A, &Wire)) {
    Serial.println("✓ I2C mode detected!");
    detected_mode = MODE_I2C;
    return;
  }
  Serial.println("✗ I2C failed, trying UART...");
  
  Serial1.begin(115200);
  delay(100);
  if (imu->begin_UART(&Serial1)) {
    Serial.println("✓ UART mode detected!");
    detected_mode = MODE_UART;
    return;
  }
  Serial.println("✗ Both modes failed! Check wiring.");
}
```

### Step 3: Update Code to Match Hardware

- If I2C detected: Switch code to use `begin_I2C()`
- If UART detected: Verify P1 pin voltage and keep current code
- If neither works: Diagnose wiring and mode selection pins

### Step 4: Validate Quaternion Data Quality

After initialization succeeds:

```cpp
// Check quaternion magnitude
float magnitude = sqrt(w*w + x*x + y*y + z*z);
if (magnitude < 0.99 || magnitude > 1.01) {
  Serial.println("WARNING: Quaternion magnitude out of range!");
  Serial.println("Data may be invalid or corrupted");
}

// Check calibration progression
if (previous_status < current_status) {
  Serial.println("Good: Calibration improving");
}

// Check update frequency
if (time_since_last_read > 200) {  // Should be ~100ms
  Serial.println("WARNING: Data updates too slow");
}
```

---

## Documentation Updates Needed

### For Hardware Setup

- [ ] Create definitive hardware configuration guide
- [ ] Include photos of actual wiring
- [ ] Document P0/P1 jumper states
- [ ] Verify with multimeter screenshots
- [ ] Add troubleshooting flowchart

### For Code

- [ ] Add I2C mode option (#ifdef flag)
- [ ] Add mode detection/diagnostic code
- [ ] Document P1 pin configuration requirement
- [ ] Add error messages for initialization failures
- [ ] Update comments with pin assignments

### For Deployment

- [ ] Create mode selection checklist
- [ ] Document mode switching procedure
- [ ] Add quick-start guide per mode
- [ ] Include common pitfalls and solutions

---

## Recommended Next Steps

### Immediate (This Session)

1. **Verify Actual Hardware**
   - Examine BNO085 breakout board P0/P1 solder pads
   - Use multimeter to check P1 voltage
   - Visually inspect wiring to Arduino pins
   - Take photos for documentation

2. **Clarify Current Status**
   - Is code actually running on Mega?
   - Does `begin_UART()` return true?
   - Are quaternions being received?
   - If not, try I2C mode

3. **Update Code If Needed**
   - Add #ifdef flags for mode selection
   - Implement fallback mode detection
   - Add comprehensive error messages

### Short Term (Next Session)

4. **Test Both Modes**
   - Compile code for UART-SHTP mode
   - Compile code for I2C mode
   - Test each on hardware
   - Verify quaternion data quality

5. **Create Universal Initialization**
   - Implement compile-time mode selection
   - Add diagnostic code for mode detection
   - Create quick reference for mode switching

6. **Documentation**
   - Update HARDWARE_SETUP.md with verified wiring
   - Add P1 pin voltage verification steps
   - Include mode troubleshooting guide

### Long Term

7. **Production Readiness**
   - Add multiple sensor support (if needed)
   - Implement GPIO control of P1 pin (dynamic mode switching)
   - Add sensor health monitoring
   - Create deployment checklist

---

## Code Examples for Both Modes

### Universal Header File (Recommended)

```cpp
// pins.h - Updated with mode selection

#ifndef PINS_H
#define PINS_H

// ============================================================================
// BNO085 Communication Mode Selection
// ============================================================================
// Uncomment one of the following:
#define BNO085_USE_UART_SHTP    // UART mode (pins 18/19, P1=5V)
// #define BNO085_USE_I2C        // I2C mode (pins 20/21, P0/P1 LOW)

// ============================================================================
// UART-SHTP Configuration (if selected above)
// ============================================================================
#ifdef BNO085_USE_UART_SHTP
  #define BNO085_SERIAL Serial1
  #define BNO085_BAUD 115200
  #define BNO085_RX_PIN 19
  #define BNO085_TX_PIN 18
  // NOTE: P1 pin must be tied to 5V on breakout board!
#endif

// ============================================================================
// I2C Configuration (if selected above)
// ============================================================================
#ifdef BNO085_USE_I2C
  #define BNO085_I2C_ADDR 0x4A  // or 0x4B if DI pin HIGH
  #define BNO085_SDA_PIN 20
  #define BNO085_SCL_PIN 21
  // NOTE: P0 and P1 must be LOW/GND on breakout board!
#endif

// ============================================================================
// Other Pins
// ============================================================================
#define LED_PIN 13
#define SERIAL_OUTPUT_BAUD 115200

#endif  // PINS_H
```

### Universal Initialization

```cpp
// bno085.cpp - Updated initialization

bool BNO085::begin() {
  imu_ = new Adafruit_BNO08x();
  if (!imu_) {
    return false;
  }

#ifdef BNO085_USE_UART_SHTP
  Serial.println("BNO085: Initializing in UART-SHTP mode...");
  Serial.print("  RX: Pin ");
  Serial.print(BNO085_RX_PIN);
  Serial.print(", TX: Pin ");
  Serial.print(BNO085_TX_PIN);
  Serial.println();
  Serial.println("  ⚠️  WARNING: P1 pin must be 5V!");
  
  BNO085_SERIAL.begin(BNO085_BAUD);
  delay(100);
  
  if (!imu_->begin_UART(&BNO085_SERIAL)) {
    Serial.println("  ✗ UART initialization failed!");
    Serial.println("  CHECK: P1 pin voltage, RX/TX wiring");
    delete imu_;
    imu_ = nullptr;
    return false;
  }
  Serial.println("  ✓ UART initialized");

#elif defined(BNO085_USE_I2C)
  Serial.println("BNO085: Initializing in I2C mode...");
  Serial.print("  SDA: Pin ");
  Serial.print(BNO085_SDA_PIN);
  Serial.print(", SCL: Pin ");
  Serial.print(BNO085_SCL_PIN);
  Serial.println();
  Serial.print("  I2C Address: 0x");
  Serial.println(BNO085_I2C_ADDR, HEX);
  
  if (!imu_->begin_I2C(BNO085_I2C_ADDR, &Wire)) {
    Serial.println("  ✗ I2C initialization failed!");
    Serial.println("  CHECK: SDA/SCL wiring, pull-up resistors");
    delete imu_;
    imu_ = nullptr;
    return false;
  }
  Serial.println("  ✓ I2C initialized");

#else
  #error "Must define BNO085_USE_UART_SHTP or BNO085_USE_I2C"
#endif

  // Enable absolute orientation (works in both modes)
  if (!imu_->enableReport(SH2_ROTATION_VECTOR, 100000)) {
    Serial.println("  ✗ Failed to enable rotation vector report");
    delete imu_;
    imu_ = nullptr;
    return false;
  }
  
  initialized_ = true;
  Serial.println("  ✓ Rotation vector reports enabled (10 Hz)");
  return true;
}
```

---

## Summary Matrix

### Current Project Status

```
┌──────────────────────────────────────────────┐
│ BNO085 Implementation Status (2026-05-06)    │
├──────────────────────────────────────────────┤
│ Design Intent: UART-SHTP                     │
│ Code Implementation: UART-SHTP               │
│ User Hardware Report: I2C                     │
│ Status: MISMATCH DETECTED ⚠️                 │
├──────────────────────────────────────────────┤
│ CODE READINESS:                              │
│ ✅ Compiles successfully                     │
│ ✅ Memory efficient                          │
│ ✅ Quaternion output working                 │
│ ❌ Doesn't match user hardware               │
│ ❌ No I2C fallback option                    │
│ ⚠️  P1 pin not controlled by code            │
├──────────────────────────────────────────────┤
│ ACTION REQUIRED:                             │
│ 1. Verify actual hardware wiring             │
│ 2. Add I2C support option                    │
│ 3. Update documentation                      │
│ 4. Test both modes                           │
└──────────────────────────────────────────────┘
```

---

**Document Version**: 1.0  
**Status**: Ready for Implementation  
**Related Documents**:
- `bno085_communication_modes.md` - Comprehensive technical analysis
- `bno085_communication_modes_quick_reference.md` - Quick lookup reference
- `bno085_pin_diagrams.md` - Visual wiring guides
- `../src/sensors/bno085.cpp` - Current implementation
