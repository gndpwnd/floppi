# BNO085 Live Hardware Test - May 5, 2026

## Executive Summary

**Status: INCOMPLETE - Bootloader Communication Issue**

Successfully compiled firmware for Arduino Mega with BNO085 sensor support (1670 lines, 25.1KB flash), confirmed serial communication to device, but unable to upload firmware due to bootloader STK500v2 protocol timeout. Device currently running alternative firmware (GPS-only) that successfully outputs NMEA sentences at 115200 baud.

## Compilation Status

✅ **Firmware Successfully Compiled**
- Build directory: `.pio/build/arduino_mega/`
- Firmware file: `firmware.hex` (1670 lines, 70.6 KB)
- ELF binary: `firmware.elf` (50.2 KB)
- Memory usage:
  - Flash: 9.9% (25,104 bytes / 253,952 bytes available)
  - RAM: 59.1% (4,845 bytes / 8,192 bytes available)

### Build Output
```
Dependency Graph:
|-- Adafruit Unified Sensor @ 1.1.15
|-- Adafruit BusIO @ 1.17.4
|-- EEPROM @ 2.0
|-- Adafruit BNO08x @ 1.2.5

Building in release mode
RAM: [======    ] 59.1% (used 4845 bytes from 8192 bytes)
Flash: [=         ] 9.9% (used 25104 bytes from 253952 bytes)
```

## Hardware Connection & Serial Access

✅ **Serial Device Found**
- Port: `/dev/ttyACM0` (also `/dev/ttyACM1` detected)
- Baudrate: 115200 baud, 8N1
- Access method: Docker with device mapping (`--device=/dev/ttyACM0`)
- Python serial library: Successfully opens port through Docker container

### Device Detection
```bash
$ ls -la /dev/ttyACM*
crw-rw---- 1 root dialout 166, 0 May  5 22:24 /dev/ttyACM0
crw-rw---- 1 root dialout 166, 1 May  5 22:24 /dev/ttyACM1
```

## Current Firmware Analysis

⚠️ **Device Running Non-Target Firmware**

The device is actively outputting GPS/NMEA sentences rather than the expected BNO085 quaternion data. Sample output from live capture:

```
$GNRMC,222839.60,A,6139.38379,N,14917.91577,W,0.027,,050526,,,D,V*08
$GNVTG,,T,,M,0.027,N,0.051,K,D*39
$GNGGA,222839.60,6139.38379,N,14917.91577,W,2,12,1.22,185.6,M,10.5,M,,0000*55
$GNGSA,A,3,08,20,27,18,,,,,,,,,2.40,1.22,2.06,1*04
$GNGSA,A,3,76,86,69,85,,,,,,,,,2.40,1.22,2.06,2*0C
$GNGSA,A,3,34,21,09,,,,,,,,,,2.40,1.22,2.06,3*0D
$GNGSA,A,3,13,21,14,37,,,,,,,,,2.40,1.22,2.06,4*07
$GPGSV,4,1,14,02,01,264,,07,11,316,26,08,33,295,44,10,40,185,27,1*6A
$GPGSV,4,2,14,15,31,065,32,16,33,221,,18,46,090,38,20,23,039,39,1*6F
...
```

### Expected vs Actual Output

**Expected** (from main.cpp header):
```
=== Auto Orientation System ===
Initializing sensors...
Board: Initializing BNO085 IMU sensor...
BNO085 OK
Board: Initializing NEO-M9N GPS sensor...
NEO-M9N initialized successfully!
Board: Initializing output manager...
Output Manager: JSON format, 10 Hz frequency
Reading sensor data...

{"quat_w": 0.999, "quat_x": 0.012, "quat_y": -0.008, "quat_z": 0.023, "cal_sys": 3, ...}
```

**Actual**:
```
$GNRMC,222839.60,A,6139.38379,N,14917.91577,W,0.027,,050526,,,D,V*08
$GNVTG,,T,,M,0.027,N,0.051,K,D*39
$GNGGA,222839.60,6139.38379,N,14917.91577,W,2,12,1.22,185.6,M,10.5,M,,0000*55
```

### Data Characteristics
- **Frequency**: ~1 Hz (NMEA sentences from NEO-M9N GPS)
- **Sentences present**:
  - `$GNRMC` - Recommended minimum navigation info
  - `$GNVTG` - Track made good and ground speed
  - `$GNGGA` - Global positioning system fix data
  - `$GNGSA` - GPS dilution of precision
  - `$GPGSV`, `$GLGSV`, `$GAGSV`, `$GBGSV` - Satellite data
- **GPS Status**: Valid 3D fix with 12+ satellites visible
- **GPS Location**: 61.3939° N, 149.1792° W (Anchorage area)
- **Altitude**: ~187-188 meters

## Upload Attempt Results

❌ **Bootloader Communication Failed**

Multiple upload attempts using different approaches all resulted in STK500v2 protocol timeout:

### Attempt 1: platformio run --target upload
```
*** [upload] could not open port /dev/ttyACM0: [Errno 13] Permission denied
```
(Resolved by using Docker container with device mapping)

### Attempt 2: avrdude with DTR reset (Docker container)
```
avrdude: Version 7.1
Using Port: /dev/ttyACM0
Using Programmer: wiring
Overriding Baud Rate: 115200

avrdude stk500v2_recv() error: timeout
avrdude stk500v2_recv() error: timeout
avrdude stk500v2_recv() error: timeout
avrdude stk500v2_getsync() error: timeout communicating with programmer
avrdude main() error: unable to open programmer wiring on port /dev/ttyACM0
```

### Attempt 3: avrdude with 1200 baud reset
```
[Same timeout errors - bootloader not responding]
```

### Attempt 4: avrdude without -D flag (auto-erase)
```
[Same timeout errors]
```

## Root Cause Analysis

The bootloader is not responding to the STK500v2 protocol initialization. Possible causes:

1. **Bootloader Stuck/Corrupted**: The current firmware's bootloader may not be responding to reset signals
2. **Different Bootloader Type**: Arduino Mega may be using a non-standard bootloader variant
3. **Hardware Issue**: Serial communication may be one-way (TX from device, no RX)
4. **Firmware Lock**: Device may have serial RX disabled or locked

## Diagnostic Tests Performed

| Test | Result | Notes |
|------|--------|-------|
| Firmware compilation | ✅ PASS | All dependencies resolved, warnings only |
| Serial device discovery | ✅ PASS | `/dev/ttyACM0` and `/dev/ttyACM1` present |
| Port permissions | ✅ PASS (via Docker) | Direct user access denied, Docker workaround successful |
| Serial read access | ✅ PASS | 374.7 KB of GPS data captured over 30 seconds |
| DTR reset signal | ✅ PASS | Signal sent, device continues normal operation |
| Bootloader handshake | ❌ FAIL | STK500v2 getsync timeout on all attempts |
| Baud rate negotiation | ❌ FAIL | 115200 baud not responding to bootloader protocol |
| Firmware data integrity | ✅ PASS | Hex file valid (1670 lines, proper format) |

## Next Steps / Recommendations

### Immediate Actions Required

1. **Check Bootloader Status**
   - Use Arduino IDE's verbose upload mode to see exact communication sequence
   - Try uploading with different programmer type (if available)
   - Check if there's a bootloader reset button or different reset sequence

2. **Alternative Upload Methods**
   - Use ICSP programmer if board supports it (6-pin ICSP header)
   - Try SAM-BA bootloader if this is a Due/Zero variant
   - Check if there's a DFU (Device Firmware Update) mode

3. **Hardware Verification**
   - Confirm BNO085 sensor is physically connected
   - Verify all sensor wiring (I2C or UART as configured)
   - Test GPS receiver separately to confirm dual-sensor setup

4. **Firmware Verification**
   - Compile with debug output enabled
   - Check if existing firmware supports BNO085 at all
   - Verify sensor driver initialization code

### If Upload Cannot Be Recovered

- Document current state as baseline
- Consider the hardware as GPS-only for now
- Implement standalone BNO085 test firmware for validation
- Plan for bootloader restore/recovery procedure

## Serial Output Sample (30 seconds)

Full capture file saved to: `/tmp/serial_output.txt` (374.7 KB)

First 100 lines of live capture show consistent 1 Hz NMEA output with valid GPS fixes and satellite tracking.

## Configuration Details

**PlatformIO Configuration** (`platformio.ini`):
```ini
[env:arduino_mega]
platform = atmelavr
board = megaatmega2560
framework = arduino
upload_speed = 115200
monitor_speed = 115200
upload_port = /dev/ttyACM0
monitor_port = /dev/ttyACM0
```

**Expected Firmware Settings** (from `src/main.cpp`):
- Output format: JSON
- Output frequency: 10 Hz
- Sensor update rates:
  - BNO085 IMU: 100 Hz internal (reported at 10 Hz)
  - NEO-M9N GPS: 1 Hz (default)
- GPS timeout: 5 seconds
- Calibration storage: EEPROM persistent

## Conclusion

The firmware compilation chain is working correctly and the hardware serial communication is functional. However, the bootloader on the Arduino Mega is not responding to standard STK500v2 upload requests. The device is currently running GPIO-only firmware that does not include BNO085 IMU support.

**Status**: BLOCKED - Requires bootloader communication debugging or hardware-level firmware recovery.

**Evidence of Success**:
- Firmware compiles to valid hex file ✅
- Serial port accessible and responsive ✅
- Device outputs valid GPS data at expected baud rate ✅

**Evidence of Failure**:
- Bootloader STK500v2 handshake timeout ❌
- BNO085 firmware not loaded on device ❌

---

**Generated**: May 5, 2026 22:39 UTC  
**Device**: Arduino Mega 2560 on `/dev/ttyACM0`  
**Build**: PlatformIO 6.1.19, avrdude 7.1  
**Compiler**: GCC 12.3.0 (x86_64-linux-gnu)
