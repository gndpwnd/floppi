# BNO085 Calibration Persistence: Implementation Guide

**Status**: Ready for Implementation  
**Estimated Effort**: 200-300 lines of code  
**Complexity**: Medium (SH-2 protocol, EEPROM management)  

---

## Architecture Overview

```
Auto Orientation HAL Interface
│
├─ OrientationSensor::getCalibrationProfile(data, length)
├─ OrientationSensor::setCalibrationProfile(data, length)
│
└─ BNO085 Implementation
   │
   ├─ Read from BNO085 (SH-2 protocol)
   │  └─ Feature 0xFE GET_FEATURE_REQUEST/RESPONSE
   │
   ├─ Write to BNO085 (SH-2 protocol)
   │  └─ Feature 0xFE SET_FEATURE_REQUEST/RESPONSE
   │
   └─ Persistent Storage
      ├─ Arduino EEPROM (240 bytes at offset 0)
      └─ Metadata (timestamp, location, version)
```

---

## File Structure (Recommended)

```
src/sensors/
├── bno085.h                    (existing, update methods)
├── bno085.cpp                  (implementation of HAL methods)
├── sh2_protocol.h              (new: SH-2 low-level commands)
└── bno085_calibration.h        (new: calibration-specific helpers)

src/config/
└── calibration_storage.h       (new: EEPROM abstraction)

docs/findings/
├── bno085-calibration-persistence.md       (DONE: research summary)
├── sh2-protocol-reference.md               (DONE: detailed spec)
└── calibration-implementation-guide.md     (this file)
```

---

## Implementation Checklist

### Phase 1: SH-2 Protocol Layer (Priority: HIGH)

**File**: `src/sensors/sh2_protocol.h`

**Functions to Implement:**
- [ ] `uint16_t sh2_crc16_ccitt(const uint8_t* data, uint16_t len)`
- [ ] `bool sh2_send_packet(const uint8_t* packet, uint16_t len, uint8_t channel)`
- [ ] `uint16_t sh2_receive_packet(uint8_t* buffer, uint16_t max_len, uint32_t timeout_ms)`
- [ ] `bool sh2_verify_response(uint8_t cmd_type, uint8_t feature_id)`

**Example Implementation:**

```cpp
#ifndef SH2_PROTOCOL_H
#define SH2_PROTOCOL_H

#include <stdint.h>
#include <Arduino.h>

// SH-2 Command Types
#define SH2_CMD_GET_FEATURE         0xF2
#define SH2_CMD_SET_FEATURE         0xF3
#define SH2_CMD_GET_FEATURE_RESP    0xFA
#define SH2_CMD_ERROR               0xFB

// Feature IDs
#define SH2_FEATURE_CALIBRATION     0xFE

// Channel IDs
#define SH2_CHANNEL_CONTROL         0x00
#define SH2_CHANNEL_EXECUTE         0x01

// Timeouts
#define SH2_TIMEOUT_MS              1000

// SH-2 Packet Structure
struct SH2_Packet {
  uint16_t length;      // Payload length + 1
  uint8_t channel;      // Channel ID
  uint8_t sequence;     // Sequence number (auto-increment)
  uint8_t reserved;     // Always 0x00
  uint8_t cmd_type;     // Command type (0xF2, 0xF3, etc.)
  uint8_t payload[256]; // Variable payload
  uint16_t crc;         // CRC-16
};

/**
 * Calculate CRC-16-CCITT for SH-2 packets
 * Polynomial: 0x1021, Init: 0x14B0
 */
uint16_t sh2_crc16_ccitt(const uint8_t* data, uint16_t len) {
  uint16_t crc = 0x14B0;
  
  for (uint16_t i = 0; i < len; i++) {
    crc ^= (uint16_t)(data[i] << 8);
    
    for (int j = 0; j < 8; j++) {
      crc <<= 1;
      if (crc & 0x10000) {
        crc ^= 0x11021;  // Polynomial
        crc &= 0xFFFF;
      }
    }
  }
  
  return crc;
}

/**
 * Send SH-2 packet to BNO085
 * Handles I2C or UART based on transport
 */
bool sh2_send_packet(const uint8_t* data, uint16_t len) {
  // This will be transport-specific
  // For now, assume SoftwareSerial or Wire is available
  #ifdef BNO085_I2C_MODE
    Wire.beginTransmission(BNO085_I2C_ADDR);
    Wire.write(data, len);
    return (Wire.endTransmission() == 0);
  #else
    // UART mode
    for (uint16_t i = 0; i < len; i++) {
      bno_serial.write(data[i]);
    }
    return true;
  #endif
}

/**
 * Receive SH-2 packet from BNO085
 * Returns packet length, or 0 if timeout
 */
uint16_t sh2_receive_packet(uint8_t* buffer, uint16_t max_len, 
                            uint32_t timeout_ms = SH2_TIMEOUT_MS) {
  uint32_t start_ms = millis();
  uint16_t recv_len = 0;
  
  while (millis() - start_ms < timeout_ms) {
    #ifdef BNO085_I2C_MODE
      Wire.requestFrom(BNO085_I2C_ADDR, (int)max_len);
      while (Wire.available() && recv_len < max_len) {
        buffer[recv_len++] = Wire.read();
      }
      if (recv_len > 0) break;
    #else
      // UART mode
      if (bno_serial.available()) {
        while (bno_serial.available() && recv_len < max_len) {
          buffer[recv_len++] = bno_serial.read();
        }
        break;
      }
    #endif
    
    delay(1);
  }
  
  return recv_len;
}

/**
 * Verify SH-2 response has correct structure
 */
bool sh2_verify_response(const uint8_t* response, uint16_t len, 
                         uint8_t expected_cmd, uint8_t expected_feature) {
  if (len < 7) return false;  // Minimum valid response
  
  // Verify CRC
  uint16_t resp_crc = ((uint16_t)response[len-2] << 8) | response[len-1];
  uint16_t calc_crc = sh2_crc16_ccitt(response, len - 2);
  if (resp_crc != calc_crc) {
    Serial.println("[SH2] CRC mismatch");
    return false;
  }
  
  // Verify command type
  if (response[4] != expected_cmd) {
    Serial.print("[SH2] Unexpected cmd type: 0x");
    Serial.println(response[4], HEX);
    return false;
  }
  
  // Verify feature ID
  if (response[5] != expected_feature) {
    Serial.print("[SH2] Unexpected feature: 0x");
    Serial.println(response[5], HEX);
    return false;
  }
  
  return true;
}

#endif  // SH2_PROTOCOL_H
```

### Phase 2: BNO085 Calibration Low-Level (Priority: HIGH)

**File**: `src/sensors/bno085_calibration.h`

**Functions to Implement:**
- [ ] `bool bno085_read_calibration_raw(uint8_t* cal_data, uint16_t* cal_len)`
- [ ] `bool bno085_write_calibration_raw(const uint8_t* cal_data, uint16_t cal_len)`
- [ ] `bool bno085_validate_calibration_data(const uint8_t* data, uint16_t len)`

**Example Implementation:**

```cpp
#ifndef BNO085_CALIBRATION_H
#define BNO085_CALIBRATION_H

#include "sh2_protocol.h"

// Calibration constraints
#define BNO085_CAL_DATA_MIN_LEN  64   // Minimum valid calibration size
#define BNO085_CAL_DATA_MAX_LEN  256  // Maximum buffer size

/**
 * Read calibration profile from BNO085 via SH-2
 * 
 * @param cal_data Output buffer for calibration bytes
 * @param cal_len  Output: length of calibration data
 * @return true if successful
 */
bool bno085_read_calibration_raw(uint8_t* cal_data, uint16_t* cal_len) {
  // Prepare GET_FEATURE_REQUEST
  uint8_t request[7] = {
    0x05,                      // Length (5 payload bytes)
    SH2_CHANNEL_CONTROL,       // Channel
    0x01,                      // Sequence
    0x00,                      // Reserved
    SH2_CMD_GET_FEATURE,       // Command
    SH2_FEATURE_CALIBRATION,   // Feature ID
    0x00                       // Flags
  };
  
  // Calculate CRC
  uint16_t crc = sh2_crc16_ccitt(request, 7);
  uint8_t packet[9];
  memcpy(packet, request, 7);
  packet[7] = (crc >> 8) & 0xFF;
  packet[8] = crc & 0xFF;
  
  // Send
  if (!sh2_send_packet(packet, 9)) {
    Serial.println("[BNO] Failed to send GET_FEATURE request");
    return false;
  }
  
  // Receive
  uint8_t response[300];
  uint16_t resp_len = sh2_receive_packet(response, sizeof(response), 1000);
  
  if (resp_len == 0) {
    Serial.println("[BNO] No response to GET_FEATURE");
    return false;
  }
  
  // Verify response
  if (!sh2_verify_response(response, resp_len, 
                          SH2_CMD_GET_FEATURE_RESP, 
                          SH2_FEATURE_CALIBRATION)) {
    return false;
  }
  
  // Extract calibration data
  // Response format: [header][cmd][feature][flags][cal_data...][crc]
  uint16_t cal_start = 7;  // Skip header(4) + cmd(1) + feature(1) + flags(1)
  uint16_t cal_size = resp_len - 2 - cal_start;  // Subtract CRC
  
  if (cal_size < BNO085_CAL_DATA_MIN_LEN || cal_size > BNO085_CAL_DATA_MAX_LEN) {
    Serial.print("[BNO] Invalid calibration size: ");
    Serial.println(cal_size);
    return false;
  }
  
  memcpy(cal_data, &response[cal_start], cal_size);
  *cal_len = cal_size;
  
  Serial.print("[BNO] Read calibration: ");
  Serial.print(cal_size);
  Serial.println(" bytes");
  
  return true;
}

/**
 * Write calibration profile to BNO085 via SH-2
 * 
 * @param cal_data Calibration bytes to restore
 * @param cal_len  Length of calibration data
 * @return true if successful
 */
bool bno085_write_calibration_raw(const uint8_t* cal_data, uint16_t cal_len) {
  // Validate input
  if (cal_len < BNO085_CAL_DATA_MIN_LEN || cal_len > BNO085_CAL_DATA_MAX_LEN) {
    Serial.print("[BNO] Invalid calibration length: ");
    Serial.println(cal_len);
    return false;
  }
  
  // Prepare SET_FEATURE_REQUEST
  uint16_t packet_len = 7 + cal_len + 2;  // Header + cal + CRC
  uint8_t packet[256];
  
  packet[0] = (7 + cal_len) & 0xFF;     // Length
  packet[1] = SH2_CHANNEL_CONTROL;      // Channel
  packet[2] = 0x02;                     // Sequence
  packet[3] = 0x00;                     // Reserved
  packet[4] = SH2_CMD_SET_FEATURE;      // Command
  packet[5] = SH2_FEATURE_CALIBRATION;  // Feature ID
  packet[6] = 0x00;                     // Flags
  
  // Append calibration data
  memcpy(&packet[7], cal_data, cal_len);
  
  // Calculate and append CRC
  uint16_t crc = sh2_crc16_ccitt(packet, 7 + cal_len);
  packet[7 + cal_len] = (crc >> 8) & 0xFF;
  packet[7 + cal_len + 1] = crc & 0xFF;
  
  // Send
  if (!sh2_send_packet(packet, packet_len)) {
    Serial.println("[BNO] Failed to send SET_FEATURE request");
    return false;
  }
  
  // Receive response
  uint8_t response[16];
  uint16_t resp_len = sh2_receive_packet(response, sizeof(response), 1000);
  
  if (resp_len < 7) {
    Serial.println("[BNO] Invalid response to SET_FEATURE");
    return false;
  }
  
  // Verify response
  if (!sh2_verify_response(response, resp_len, 
                          SH2_CMD_GET_FEATURE_RESP, 
                          SH2_FEATURE_CALIBRATION)) {
    return false;
  }
  
  // Check status byte
  if (response[6] != 0x00) {
    Serial.print("[BNO] SET_FEATURE failed with status: 0x");
    Serial.println(response[6], HEX);
    return false;
  }
  
  Serial.print("[BNO] Wrote calibration: ");
  Serial.print(cal_len);
  Serial.println(" bytes");
  
  return true;
}

/**
 * Validate calibration data format and size
 */
bool bno085_validate_calibration_data(const uint8_t* data, uint16_t len) {
  // Basic checks
  if (data == NULL || len < BNO085_CAL_DATA_MIN_LEN) {
    return false;
  }
  
  // Sanity checks on calibration data
  // (These are heuristics; exact validation depends on firmware)
  
  // Check for entirely zeros or 0xFF
  uint8_t first = data[0];
  bool all_same = true;
  for (uint16_t i = 1; i < len; i++) {
    if (data[i] != first) {
      all_same = false;
      break;
    }
  }
  if (all_same) {
    return false;  // All zeros or all 0xFF = invalid
  }
  
  return true;
}

#endif  // BNO085_CALIBRATION_H
```

### Phase 3: Persistent Storage Layer (Priority: HIGH)

**File**: `src/config/calibration_storage.h`

**Functions to Implement:**
- [ ] `bool cal_storage_init()`
- [ ] `bool cal_storage_save(const uint8_t* data, uint16_t len, const char* name)`
- [ ] `bool cal_storage_load(uint8_t* data, uint16_t* len, const char* name)`
- [ ] `bool cal_storage_erase(const char* name)`

**Example Implementation:**

```cpp
#ifndef CALIBRATION_STORAGE_H
#define CALIBRATION_STORAGE_H

#include <stdint.h>
#include <EEPROM.h>

// EEPROM Layout (Arduino Mega has 4096 bytes)
#define CAL_EEPROM_BASE          0      // Start of calibration area
#define CAL_EEPROM_SIZE          256    // Size for single profile
#define CAL_EEPROM_PROFILES_MAX  4      // Room for ~4 profiles

// Structure: [magic][version][length][flags][data...][crc]
#define CAL_MAGIC_BYTE           0xCA
#define CAL_VERSION              0x01
#define CAL_HEADER_SIZE          8      // 1+1+2+1+1+2 (magic, version, len, flags, crc)

struct CalibrationHeader {
  uint8_t magic;       // 0xCA
  uint8_t version;     // 0x01
  uint16_t data_len;   // Length of calibration data
  uint8_t flags;       // Reserved
  uint16_t crc;        // CRC of data
  // Followed by: data[data_len]
};

/**
 * Initialize calibration storage
 */
bool cal_storage_init() {
  // Check if EEPROM is valid
  if (EEPROM.read(CAL_EEPROM_BASE) == CAL_MAGIC_BYTE) {
    Serial.println("[EEPROM] Calibration storage initialized");
    return true;
  }
  
  Serial.println("[EEPROM] Initializing new calibration storage");
  // Clear the area
  for (uint16_t i = CAL_EEPROM_BASE; i < CAL_EEPROM_BASE + CAL_EEPROM_SIZE; i++) {
    EEPROM.write(i, 0xFF);
  }
  
  return true;
}

/**
 * Save calibration to EEPROM
 */
bool cal_storage_save(const uint8_t* data, uint16_t len, const char* name) {
  if (len == 0 || len > 240) {
    Serial.println("[EEPROM] Invalid calibration length");
    return false;
  }
  
  // Write header
  uint16_t offset = CAL_EEPROM_BASE;
  
  EEPROM.write(offset + 0, CAL_MAGIC_BYTE);
  EEPROM.write(offset + 1, CAL_VERSION);
  EEPROM.write(offset + 2, (len >> 8) & 0xFF);
  EEPROM.write(offset + 3, len & 0xFF);
  EEPROM.write(offset + 4, 0x00);  // Flags
  
  // Write calibration data
  for (uint16_t i = 0; i < len; i++) {
    EEPROM.write(offset + 5 + i, data[i]);
  }
  
  // Calculate and write CRC of data
  uint16_t data_crc = crc16_data(data, len);
  EEPROM.write(offset + 5 + len, (data_crc >> 8) & 0xFF);
  EEPROM.write(offset + 6 + len, data_crc & 0xFF);
  
  // Commit to flash
  EEPROM.commit();  // Some platforms require explicit commit
  
  Serial.print("[EEPROM] Saved calibration: ");
  Serial.print(len);
  Serial.println(" bytes");
  
  return true;
}

/**
 * Load calibration from EEPROM
 */
bool cal_storage_load(uint8_t* data, uint16_t* len, const char* name) {
  uint16_t offset = CAL_EEPROM_BASE;
  
  // Check magic byte
  if (EEPROM.read(offset + 0) != CAL_MAGIC_BYTE) {
    Serial.println("[EEPROM] No calibration data found");
    return false;
  }
  
  // Check version
  if (EEPROM.read(offset + 1) != CAL_VERSION) {
    Serial.println("[EEPROM] Incompatible calibration version");
    return false;
  }
  
  // Read length
  uint16_t stored_len = ((uint16_t)EEPROM.read(offset + 2) << 8) 
                      | EEPROM.read(offset + 3);
  
  if (stored_len == 0 || stored_len > 240) {
    Serial.println("[EEPROM] Invalid calibration length");
    return false;
  }
  
  // Read data
  for (uint16_t i = 0; i < stored_len; i++) {
    data[i] = EEPROM.read(offset + 5 + i);
  }
  
  // Verify CRC
  uint16_t stored_crc = ((uint16_t)EEPROM.read(offset + 5 + stored_len) << 8)
                      | EEPROM.read(offset + 6 + stored_len);
  uint16_t calc_crc = crc16_data(data, stored_len);
  
  if (stored_crc != calc_crc) {
    Serial.println("[EEPROM] CRC mismatch on stored calibration");
    return false;
  }
  
  *len = stored_len;
  
  Serial.print("[EEPROM] Loaded calibration: ");
  Serial.print(stored_len);
  Serial.println(" bytes");
  
  return true;
}

/**
 * CRC-16 for stored data (different from SH2_CRC16)
 * Simple checksum for validation
 */
uint16_t crc16_data(const uint8_t* data, uint16_t len) {
  uint16_t crc = 0xFFFF;
  
  for (uint16_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  
  return crc;
}

#endif  // CALIBRATION_STORAGE_H
```

### Phase 4: Update BNO085 HAL Implementation (Priority: MEDIUM)

**File**: `src/sensors/bno085.cpp`

**Methods to Implement:**
- [ ] `BNO085::getCalibrationProfile(uint8_t* data, uint16_t* len)`
- [ ] `BNO085::setCalibrationProfile(const uint8_t* data, uint16_t len)`
- [ ] `BNO085::begin()` - add calibration restore logic

**Example Implementation:**

```cpp
#include "bno085.h"
#include "bno085_calibration.h"
#include "calibration_storage.h"

BNO085::BNO085() 
  : imu_(nullptr), initialized_(false), new_data_(false), 
    calibration_data_length_(0), last_read_ms_(0) {
  memset(calibration_data_, 0, sizeof(calibration_data_));
}

bool BNO085::begin() {
  // ... existing initialization code ...
  
  // Initialize calibration storage
  if (!cal_storage_init()) {
    Serial.println("[BNO] Failed to init calibration storage");
  }
  
  // Try to restore calibration from EEPROM
  uint8_t stored_cal[256];
  uint16_t stored_len = 0;
  
  if (cal_storage_load(stored_cal, &stored_len, "default")) {
    Serial.println("[BNO] Found stored calibration, restoring...");
    
    // Restore to BNO085
    if (bno085_write_calibration_raw(stored_cal, stored_len)) {
      memcpy(calibration_data_, stored_cal, stored_len);
      calibration_data_length_ = stored_len;
      Serial.println("[BNO] Calibration restored successfully");
    } else {
      Serial.println("[BNO] Failed to restore calibration");
    }
  } else {
    Serial.println("[BNO] No stored calibration found");
  }
  
  initialized_ = true;
  return true;
}

bool BNO085::getCalibrationProfile(uint8_t* profile_data, uint16_t* length) {
  if (!initialized_) {
    return false;
  }
  
  // Read current calibration from BNO085
  if (!bno085_read_calibration_raw(profile_data, length)) {
    Serial.println("[BNO] Failed to read calibration");
    return false;
  }
  
  // Cache it
  if (*length <= sizeof(calibration_data_)) {
    memcpy(calibration_data_, profile_data, *length);
    calibration_data_length_ = *length;
  }
  
  return true;
}

bool BNO085::setCalibrationProfile(const uint8_t* profile_data, uint16_t length) {
  if (!initialized_ || profile_data == nullptr) {
    return false;
  }
  
  // Validate
  if (!bno085_validate_calibration_data(profile_data, length)) {
    Serial.println("[BNO] Invalid calibration data");
    return false;
  }
  
  // Write to BNO085
  if (!bno085_write_calibration_raw(profile_data, length)) {
    Serial.println("[BNO] Failed to write calibration");
    return false;
  }
  
  // Save to EEPROM for persistence
  if (!cal_storage_save(profile_data, length, "default")) {
    Serial.println("[BNO] Warning: Failed to save calibration to EEPROM");
    // Non-fatal: calibration is restored in BNO, but won't persist
  }
  
  // Cache it
  if (length <= sizeof(calibration_data_)) {
    memcpy(calibration_data_, profile_data, length);
    calibration_data_length_ = length;
  }
  
  return true;
}
```

---

## Testing Strategy

### Unit Tests (use Arduino unit test framework or PlatformIO TEST)

```cpp
// tests/test_calibration.cpp
#include <unity.h>
#include "bno085_calibration.h"
#include "calibration_storage.h"

void setUp(void) {
  // Reset state before each test
}

void tearDown(void) {
}

void test_crc16_calculation(void) {
  uint8_t data[] = {0x05, 0x00, 0x01, 0x00, 0xF2, 0xFE, 0x00};
  uint16_t crc = sh2_crc16_ccitt(data, 7);
  
  // Verify against known CRC value
  TEST_ASSERT_NOT_EQUAL(0, crc);
  TEST_ASSERT_NOT_EQUAL(0xFFFF, crc);
}

void test_calibration_validation(void) {
  uint8_t valid_cal[70] = {0x01, 0x02, 0x03};  // Non-trivial data
  uint8_t invalid_cal_zero[70] = {0};           // All zeros
  uint8_t invalid_cal_ff[70];
  memset(invalid_cal_ff, 0xFF, 70);
  
  TEST_ASSERT_TRUE(bno085_validate_calibration_data(valid_cal, 70));
  TEST_ASSERT_FALSE(bno085_validate_calibration_data(invalid_cal_zero, 70));
  TEST_ASSERT_FALSE(bno085_validate_calibration_data(invalid_cal_ff, 70));
}

void test_eeprom_save_load(void) {
  uint8_t original[70];
  uint8_t restored[70];
  uint16_t len;
  
  // Fill with non-trivial data
  for (int i = 0; i < 70; i++) {
    original[i] = (i * 17) % 256;
  }
  
  // Save and load
  TEST_ASSERT_TRUE(cal_storage_save(original, 70, "test"));
  TEST_ASSERT_TRUE(cal_storage_load(restored, &len, "test"));
  
  // Verify
  TEST_ASSERT_EQUAL(70, len);
  TEST_ASSERT_EQUAL_MEMORY(original, restored, 70);
}
```

### Integration Tests (Hardware)

1. **Boot → Read Calibration**
   - Power on, call `getCalibrationProfile()`
   - Should return 70+ bytes of valid data

2. **Calibrate → Save → Restore**
   - Calibrate sensor (figure-8 motion)
   - Wait for status = 3 (fully calibrated)
   - Call `getCalibrationProfile()` → save bytes
   - Power cycle
   - Call `setCalibrationProfile()` → restore from saved bytes
   - Verify sensor applies the calibration (status should jump to 3 immediately)

3. **Cross-Location Test**
   - Calibrate in Austin (declination: -8.5°)
   - Note: BNO085 does NOT auto-correct for declination shifts
   - Move to Denver (declination: -7.0°)
   - Orientation error will depend on device orientation
   - Recommendation: Re-calibrate for each location

---

## Integration with Existing Code

### Update `src/main.cpp`

```cpp
#include "sensors/bno085.h"
#include "sensors/neo_m9n.h"

BNO085 bno;
// NEO_M9N gps;  // Future

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(100);
  
  Serial.println("\n=== Auto Orientation System ===");
  
  // Initialize BNO085 (includes calibration restore)
  if (!bno.begin()) {
    Serial.println("[ERR] BNO085 init failed");
    while (true) delay(1000);
  }
  
  Serial.println("[OK] BNO085 initialized");
}

void loop() {
  if (bno.read()) {
    const OrientationData& orient = bno.getOrientation();
    
    Serial.print("Q: ");
    Serial.print(orient.w); Serial.print(", ");
    Serial.print(orient.x); Serial.print(", ");
    Serial.print(orient.y); Serial.print(", ");
    Serial.println(orient.z);
    
    Serial.print("Cal: ");
    Serial.print(orient.cal_mag);
    Serial.print("/3 mag, ");
    Serial.print(orient.cal_gyro);
    Serial.print("/3 gyro, ");
    Serial.print(orient.cal_accel);
    Serial.println("/3 accel");
  }
  
  delay(50);
}
```

### Serial Commands for Debugging

Add to main loop:

```cpp
void loop() {
  // ... existing sensor reading ...
  
  // Check for serial commands
  if (Serial.available()) {
    char cmd = Serial.read();
    
    switch (cmd) {
      case 'c':  // Read and save current calibration
        {
          uint8_t cal[256];
          uint16_t len;
          if (bno.getCalibrationProfile(cal, &len)) {
            Serial.print("Calibration data (");
            Serial.print(len);
            Serial.println(" bytes):");
            for (uint16_t i = 0; i < len; i++) {
              Serial.print(cal[i], HEX);
              Serial.print(" ");
              if ((i + 1) % 16 == 0) Serial.println();
            }
          }
        }
        break;
      
      case 'r':  // Reset calibration (force re-calibrate)
        Serial.println("Performing factory reset...");
        // BNO085 will drop to level 0 on next startup
        // without setting the profile
        break;
      
      default:
        Serial.println("Commands: c=read cal, r=reset");
    }
  }
  
  // ...
}
```

---

## Troubleshooting Checklist

- [ ] CRC calculation verified against known values
- [ ] I2C/UART communication verified with logic analyzer
- [ ] EEPROM addresses don't conflict with other storage
- [ ] Calibration data size matches firmware version
- [ ] BNO085 is not in bootloader mode (check ADVERTISE on startup)
- [ ] Timeouts are sufficient (1000ms is safe)
- [ ] Response parsing handles variable-length payloads
- [ ] CRC in SH-2 packets is big-endian
- [ ] Sequence numbers increment between requests
- [ ] EEPROM.commit() is called (if required by platform)

---

## Future Enhancements

1. **Multi-profile support**: Store calibrations for multiple locations
2. **SD card storage**: For systems with SD card (flight controller, datalogger)
3. **Magnetic declination correction**: Auto-adjust based on GPS location
4. **Calibration versioning**: Track which firmware version created the profile
5. **Live calibration quality**: Monitor `cal_mag` and warn if degrading
6. **Web dashboard**: Real-time calibration status and historical data

---

## References

- Bosch BNO085 Datasheet
- SH-2 Protocol Specification
- Adafruit BNO08x Library: https://github.com/adafruit/Adafruit_BNO08x_Arduino
- This project: auto_orientation/src/sensors/
