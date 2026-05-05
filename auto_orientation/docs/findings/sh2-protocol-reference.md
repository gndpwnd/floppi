# SH-2 Protocol Reference: BNO085 Calibration Commands

**Status**: Reference Documentation  
**Last Updated**: 2026-05-05  
**For**: BNO085 calibration save/restore implementation

---

## SH-2 (Sensor Hub 2) Protocol Overview

The BNO085 communicates via the **SH-2 protocol**, which is a packet-based protocol over I2C or UART. All commands follow a standardized frame structure.

### Packet Structure

```
┌─────────────────────────────────────┐
│  Byte 0-1: Packet Header            │
│  Header[0]: Length (payload + 1)    │
│  Header[1]: Channel ID              │
├─────────────────────────────────────┤
│  Byte 2: Sequence Number            │
├─────────────────────────────────────┤
│  Byte 3: Reserved                   │
├─────────────────────────────────────┤
│  Byte 4+: Payload                   │
├─────────────────────────────────────┤
│  Last 2 Bytes: CRC-16 (Big Endian)  │
└─────────────────────────────────────┘
```

**Constants:**
- **Channel IDs**: 0=Control, 1=Execute, 2=Data, 3=Reserved
- **Max Payload**: 255 bytes
- **CRC-16**: CCITT polynomial (0x1021), init 0x14B0

### Command Types

| ID  | Name | Channel | Direction | Purpose |
|-----|------|---------|-----------|---------|
| 0xF1 | ADVERTISE | 0 | Host←Device | Device info on startup |
| 0xF2 | GET_FEATURE | 0 | Host→Device | Read sensor feature config |
| 0xF3 | SET_FEATURE | 0 | Host→Device | Write sensor feature config |
| 0xF4 | FRS_WRITE_DATA | 1 | Host→Device | Write raw sensor data |
| 0xF5 | FRS_READ_DATA | 1 | Host←Device | Read raw sensor data (response) |
| 0xF6 | COMMAND_REQUEST | 0 | Host→Device | Execute command (DFU, reset, etc.) |
| 0xF7 | COMMAND_RESPONSE | 0 | Host←Device | Command result |
| 0xF8 | EXEC_ENABLE | 0 | Host→Device | Enable DFU mode |
| 0xF9 | MODE_INDICATION | 0 | Host←Device | Mode change notification |
| 0xFA | GET_FEATURE_RESPONSE | 0 | Host←Device | Feature config response |
| 0xFB | ERROR | 0 | Host←Device | Error notification |

---

## Calibration Data Access: Feature 0xFE

The BNO085 calibration profile is accessed via **Feature 0xFE** (0xFE = Calibration Profile).

### GET_FEATURE_REQUEST (Read Calibration)

**Request Format:**
```
Length:    0x05 (5 bytes)
Channel:   0x00 (Control)
Sequence:  0x01-0xFF (auto-increment)
Reserved:  0x00
Cmd Type:  0xF2 (GET_FEATURE)
Feature:   0xFE (Calibration Profile)
Flags:     0x00 (usually)
CRC:       (calculated)
```

**Example Hex:**
```
05 00 01 00 F2 FE 00 [CRC-16]
```

**Response Format:**
```
Length:    Variable (e.g., 0x4F for 79-byte payload)
Channel:   0x00 (Control)
Sequence:  Echo of request sequence
Reserved:  0x00
Cmd Type:  0xFA (GET_FEATURE_RESPONSE)
Feature:   0xFE (Calibration Profile)
Flags:     Response flags
Calibration Data: 70+ bytes
CRC:       (calculated)
```

**Response Calibration Data Structure** (typical, firmware-dependent):

```
Offset  Bytes  Field                          Notes
──────────────────────────────────────────────────
0-2     3      Accel X Offset                 Int16 + padding
3-5     3      Accel Y Offset                 Signed, mg units
6-8     3      Accel Z Offset
9-11    3      Accel X Scale                  1.0 = 0x4000
12-14   3      Accel Y Scale
15-17   3      Accel Z Scale
18-20   3      Gyro X Offset                  Rad/sec
21-23   3      Gyro Y Offset
24-26   3      Gyro Z Offset
27-29   3      Mag X Hard-Iron Offset         uT
30-32   3      Mag Y Hard-Iron Offset
33-35   3      Mag Z Hard-Iron Offset
36-53   18     Mag Soft-Iron Scale Matrix     9 x Int16 (3x3)
54+     ?      Reserved / padding             Depends on FW version
```

**Important Notes:**
- Exact byte layout varies by BNO085 firmware version
- Some fields use Q-format fixed-point (e.g., scale = value / 0x4000)
- Always extract the full response and validate size

### SET_FEATURE_REQUEST (Write Calibration)

**Request Format:**
```
Length:    0x05 + CalData_Length
Channel:   0x00 (Control)
Sequence:  0x01-0xFF (auto-increment)
Reserved:  0x00
Cmd Type:  0xF3 (SET_FEATURE)
Feature:   0xFE (Calibration Profile)
Flags:     0x00
Calibration Data: (exact bytes from GET_FEATURE response)
CRC:       (calculated)
```

**Response Format:**
```
Length:    0x02
Channel:   0x00
Sequence:  Echo of request
Reserved:  0x00
Cmd Type:  0xFA (GET_FEATURE_RESPONSE)
Feature:   0xFE
Status:    0x00 = Success, non-zero = Error
CRC:       (calculated)
```

---

## CRC-16 Calculation

The BNO085 uses **CRC-16-CCITT** (polynomial 0x1021, initial value 0x14B0).

**Pseudo-code:**
```cpp
uint16_t crc16_ccitt(const uint8_t* data, uint16_t len) {
  uint16_t crc = 0x14B0;
  for (uint16_t i = 0; i < len; i++) {
    crc ^= (data[i] << 8);
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
```

**Verification:**
- CRC covers all bytes from Header[0] to last byte before CRC
- CRC is big-endian (high byte first)
- Always verify on receive; always calculate on send

---

## Implementation Example (Pseudo-code)

### Reading Calibration from BNO085

```cpp
bool read_calibration_from_bno(uint8_t* cal_data, uint16_t* cal_len) {
  // Prepare GET_FEATURE_REQUEST
  uint8_t request[7] = {
    0x05,       // Length (5 payload bytes)
    0x00,       // Channel (Control)
    0x01,       // Sequence
    0x00,       // Reserved
    0xF2,       // GET_FEATURE
    0xFE,       // Calibration Profile feature
    0x00        // Flags
  };
  
  // Calculate and append CRC-16
  uint16_t crc = crc16_ccitt(request, 7);
  request[7] = (crc >> 8) & 0xFF;   // High byte
  request[8] = crc & 0xFF;          // Low byte
  
  // Send request
  if (!send_to_bno(request, 9)) {
    return false;
  }
  
  // Receive response
  uint8_t response[256];
  uint16_t resp_len = receive_from_bno(response, 256);
  
  if (resp_len < 7) {
    return false;  // Minimum packet size
  }
  
  // Verify CRC
  uint16_t resp_crc = ((uint16_t)response[resp_len-2] << 8) 
                    | response[resp_len-1];
  uint16_t calc_crc = crc16_ccitt(response, resp_len - 2);
  
  if (resp_crc != calc_crc) {
    return false;  // CRC mismatch
  }
  
  // Verify it's a GET_FEATURE_RESPONSE
  if (response[4] != 0xFA || response[5] != 0xFE) {
    return false;
  }
  
  // Extract calibration data
  // Calibration data starts at byte 7 (after header, cmd, feature, flags)
  uint16_t cal_start = 7;
  uint16_t cal_size = resp_len - 2 - cal_start;  // Subtract 2 for CRC
  
  memcpy(cal_data, &response[cal_start], cal_size);
  *cal_len = cal_size;
  
  return true;
}
```

### Writing Calibration to BNO085

```cpp
bool write_calibration_to_bno(const uint8_t* cal_data, uint16_t cal_len) {
  // Prepare SET_FEATURE_REQUEST
  uint16_t req_len = 7 + cal_len + 2;  // Header + cal + CRC
  uint8_t request[256];
  
  request[0] = (7 + cal_len) & 0xFF;  // Length
  request[1] = 0x00;                  // Channel (Control)
  request[2] = 0x02;                  // Sequence (different from read)
  request[3] = 0x00;                  // Reserved
  request[4] = 0xF3;                  // SET_FEATURE
  request[5] = 0xFE;                  // Calibration Profile feature
  request[6] = 0x00;                  // Flags
  
  // Append calibration data
  memcpy(&request[7], cal_data, cal_len);
  
  // Calculate and append CRC
  uint16_t crc = crc16_ccitt(request, 7 + cal_len);
  request[7 + cal_len] = (crc >> 8) & 0xFF;
  request[7 + cal_len + 1] = crc & 0xFF;
  
  // Send request
  if (!send_to_bno(request, req_len)) {
    return false;
  }
  
  // Receive ACK response
  uint8_t response[256];
  uint16_t resp_len = receive_from_bno(response, 256);
  
  if (resp_len < 7) {
    return false;
  }
  
  // Verify response is success
  if (response[4] != 0xFA || response[5] != 0xFE) {
    return false;
  }
  
  // Check status byte (should be 0 for success)
  if (response[6] != 0x00) {
    return false;  // Command failed
  }
  
  return true;
}
```

---

## I2C vs UART Communication

### I2C (Preferred for Adafruit breakout)

**Address**: 0x4A (P0=GND, P1=GND) or 0x4B (P0=3V3, P1=GND)

**I2C Packet Format:**
```
[START] [ADDR+W] [ACK] [HEADER_H] [HEADER_L] [PAYLOAD...] [CRC_H] [CRC_L] [STOP]
[START] [ADDR+R] [ACK] [HEADER_H] [HEADER_L] [PAYLOAD...] [CRC_H] [CRC_L] [NACK] [STOP]
```

**Header Format (I2C):**
- Byte 0 (HEADER_H): 0x00 (always for I2C)
- Byte 1 (HEADER_L): Length (payload + 1 for sequence byte)

### UART

**Speed**: 115200 baud (default)  
**Format**: 8N1

**Same packet structure as above**, no additional framing needed.

---

## Adafruit Library Integration

The Adafruit library **does not expose these commands directly**, but you can:

1. **Access I2C directly**: Use `Wire.begin()`, `Wire.write()`, `Wire.read()`
2. **Access UART directly**: Use `Serial` or `SoftwareSerial`
3. **Wrap low-level calls** inside higher-level functions

**Example using Arduino Wire library:**

```cpp
#include <Wire.h>

#define BNO085_I2C_ADDR 0x4A

bool read_cal_feature_i2c(uint8_t* cal_data, uint16_t* cal_len) {
  // Build request packet
  uint8_t request[] = {0x05, 0x00, 0x01, 0x00, 0xF2, 0xFE, 0x00};
  uint16_t crc = crc16_ccitt(request, 7);
  
  // Send via I2C
  Wire.beginTransmission(BNO085_I2C_ADDR);
  Wire.write(request, 7);
  Wire.write((crc >> 8) & 0xFF);
  Wire.write(crc & 0xFF);
  Wire.endTransmission();
  
  // Receive response
  delay(10);  // Wait for response
  uint8_t response[256];
  Wire.requestFrom(BNO085_I2C_ADDR, 256);
  
  int resp_len = 0;
  while (Wire.available()) {
    response[resp_len++] = Wire.read();
  }
  
  // Verify and extract (as shown in pseudo-code above)
  return verify_and_extract_calibration(response, resp_len, cal_data, cal_len);
}
```

---

## Debugging Checklist

- [ ] CRC calculation matches BNO085 expected value
- [ ] Packet length field is correct (payload bytes, not including header/length bytes)
- [ ] Feature ID is 0xFE (calibration profile)
- [ ] Sequence number increments on each request
- [ ] Response command type is 0xFA (GET_FEATURE_RESPONSE)
- [ ] Calibration data size matches firmware version
- [ ] Timeouts handled (I2C/UART may have latency)
- [ ] I2C address is correct (0x4A typical, verify with pin config)
- [ ] Baud rate is 115200 for UART
- [ ] BNO085 is not in bootloader mode (check ADVERTISE packet at startup)

---

## References

- Bosch BNO085 Datasheet: https://www.bosch-sensortec.com/bst/pdf/datasheets/BST-BNO085-DS000.pdf
- SH-2 Protocol Specification: Available from Bosch (request from support)
- Adafruit BNO085 Hookup Guide: https://learn.adafruit.com/bno085-absolute-orientation-sensor-with-calibration/
- Adafruit Library Source: https://github.com/adafruit/Adafruit_BNO08x_Arduino/blob/master/Adafruit_BNO08x.cpp

---

## Future Work

1. Create reusable `SH2Protocol` class in auto_orientation/src/sensors/
2. Add unit tests with mock BNO085 responses
3. Document full calibration lifecycle in calibration guide
4. Add SD card support for multi-location calibration profiles
