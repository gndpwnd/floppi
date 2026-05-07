# Snapshot Feature Guide

**Document Version**: 1.0  
**Date**: 2026-05-07  
**Status**: Complete for Phase 1  
**Reference**: `src/features/snapshot_recorder.h`, `src/config/mode.h`

---

## Overview

The Snapshot Feature enables recording quaternion + Euler angle + calibration status + timestamp to SD card in JSON format. This is useful for:

- **Data Logging**: Continuous recording of orientation data during flight tests
- **Calibration Verification**: Confirm calibration quality over time
- **Post-Flight Analysis**: Download data and analyze orientation changes
- **Debugging**: Record sensor behavior for troubleshooting

---

## Enabling Snapshot Mode

### Build Environments

Three build environments are available:

#### 1. `arduino_mega_calibration` (No Snapshots)
**Build Command**:
```bash
platformio run -e arduino_mega_calibration -t upload
```

**Configuration**: 
- CALIBRATION_MODE: Enabled (verbose debug output)
- SNAPSHOT_MODE: Disabled

**Use Case**: Development and testing (no SD card needed)

#### 2. `arduino_mega_snapshot` (Calibration + Snapshots)
**Build Command**:
```bash
platformio run -e arduino_mega_snapshot -t upload
```

**Configuration**:
- CALIBRATION_MODE: Enabled
- SNAPSHOT_MODE: Enabled

**Use Case**: Field testing with debug output and data logging

#### 3. `arduino_mega_snapshot_only` (Production + Snapshots)
**Build Command**:
```bash
platformio run -e arduino_mega_snapshot_only -t upload
```

**Configuration**:
- CALIBRATION_MODE: Disabled
- SNAPSHOT_MODE: Enabled

**Use Case**: Production deployment with minimal serial output

**Dependencies**: All snapshot modes require the SD library:
```
lib_deps = 
    Adafruit Unified Sensor
    Adafruit BusIO
    SD
```

---

## Hardware Setup

### SD Card Requirements

- **Card Type**: microSD or SD (Class 6 or higher recommended)
- **Capacity**: 1 GB or larger (sufficient for months of data)
- **Filesystem**: FAT32 (standard, reformatted automatically on first use)
- **Max Filesize**: Single snapshots are ~100-200 bytes, so one file can hold 5-10 million snapshots

### Pin Configuration

SD card uses SPI pins (defined in `src/config/pins.h`):

```cpp
#define SD_CS_PIN   10       // Chip Select (Arduino Mega: Pin 10 or 53)
#define SD_MOSI_PIN 11       // SPI MOSI (Arduino Mega: Pin 51)
#define SD_MISO_PIN 12       // SPI MISO (Arduino Mega: Pin 50)
#define SD_SCK_PIN  13       // SPI Clock (Arduino Mega: Pin 52)
```

**Wiring** (SPI Shields typically handle this automatically):
- SD_CS → Chip Select pin
- SD_MOSI → SPI MOSI (11 on Mega)
- SD_MISO → SPI MISO (12 on Mega)
- SD_SCK → SPI Clock (13 on Mega)
- GND → Ground
- 3.3V → VCC (use level shifter if needed)

### Button Trigger (Optional)

Button for manual snapshot triggers (defined in `src/config/pins.h`):

```cpp
#define SNAPSHOT_BUTTON_PIN  2  // Interrupt pin
```

**Wiring**:
- Button pin → Arduino pin 2 (interrupt-capable)
- Other side of button → GND
- Pull-up internally enabled

---

## Using the Snapshot Feature

### Initialization

In `src/main.cpp`, initialize snapshot recorder in `setup()`:

```cpp
#if ENABLE_SNAPSHOT_RECORDER
    snapshot_recorder_init();
    Serial.println("Snapshot recorder initialized");
#endif
```

### Manual Recording

Record a snapshot at any point in code:

```cpp
#if ENABLE_SNAPSHOT_RECORDER
    Quaternion q = imu.get_quaternion();
    uint8_t cal_status = imu.get_calibration_status();
    uint32_t timestamp_ms = millis();
    
    if (snapshot_record(q, cal_status, timestamp_ms)) {
        Serial.println("Snapshot recorded");
    } else {
        Serial.println("ERROR: Failed to record snapshot");
    }
#endif
```

### Button-Triggered Recording

Button press automatically triggers snapshot recording:

```cpp
#if ENABLE_SNAPSHOT_RECORDER
    void button_isr() {
        // ISR detects button press, triggers snapshot
        Quaternion q = imu.get_quaternion();
        uint8_t cal = imu.get_calibration_status();
        snapshot_record(q, cal, millis());
    }
#endif
```

### Automatic Periodic Recording

Record snapshots at regular intervals:

```cpp
#if ENABLE_SNAPSHOT_RECORDER
    unsigned long last_snapshot_time = 0;
    const unsigned long SNAPSHOT_INTERVAL_MS = 100;  // 10 Hz
    
    void loop() {
        unsigned long now = millis();
        
        if (now - last_snapshot_time >= SNAPSHOT_INTERVAL_MS) {
            Quaternion q = imu.get_quaternion();
            uint8_t cal = imu.get_calibration_status();
            snapshot_record(q, cal, now);
            last_snapshot_time = now;
        }
    }
#endif
```

---

## JSON Format Specification

Each snapshot is stored as a single-line JSON object (NDJSON format):

### Example Snapshot

```json
{"ts":1234567,"q":{"w":0.707,"x":0.0,"y":0.707,"z":0.0},"e":{"r":0.0,"p":90.0,"y":0.0},"c":{"s":3,"a":3,"g":3,"m":3}}
```

### Full Format with Keys

```json
{
  "ts": 1234567,                    // timestamp_ms (uint32_t)
  "q": {
    "w": 0.707,                     // quaternion scalar part
    "x": 0.0,                       // quaternion x component
    "y": 0.707,                     // quaternion y component
    "z": 0.0                        // quaternion z component
  },
  "e": {
    "r": 0.0,                       // euler roll (degrees)
    "p": 90.0,                      // euler pitch (degrees)
    "y": 0.0                        // euler yaw (degrees)
  },
  "c": {
    "s": 3,                         // calibration system (0-3)
    "a": 3,                         // calibration accelerometer (0-3)
    "g": 3,                         // calibration gyroscope (0-3)
    "m": 3                          // calibration magnetometer (0-3)
  }
}
```

### Key Abbreviations

| Key | Meaning | Range | Notes |
|-----|---------|-------|-------|
| ts | Timestamp | 0 - 4.3e9 ms | From `millis()`, wraps every ~50 days |
| q | Quaternion | w,x,y,z: [-1, +1] | Normalized (magnitude ≈ 1.0) |
| e | Euler angles | r,p,y: degrees | r,p: [-180, +180], y: [0, 360] |
| c | Calibration | 0-3 | 0=uncalibrated, 3=fully calibrated |

### Calibration Values

BNO085 provides 4 calibration statuses (each 0-3):

| Value | Meaning | Quality |
|-------|---------|---------|
| 0 | Unreliable | Do not use |
| 1 | Low | Marginal |
| 2 | Medium | Good |
| 3 | High | Excellent |

All four statuses must be 3 for full calibration confidence.

---

## File Format and Organization

### File Location

All snapshots stored in `/snapshots/` directory on SD card:

```
SD:
  /snapshots/
    snapshot_001.json
    snapshot_002.json
    snapshot_003.json
    ...
    snapshot_100.json
```

### Filename Format

```
snapshot_NNN.json
```

Where NNN is zero-padded 3-digit counter (001, 002, ..., 100)

### File Rotation

After 100 snapshots, counter resets:
- `snapshot_100.json` → last file of first batch
- `snapshot_001.json` → first file of new batch (overwrites if space limited)

To prevent data loss, immediately download files before counter wraps!

### File Size

Each snapshot line is approximately 100-200 bytes (depends on float precision).

**Estimated Capacity**:
- 1000 snapshots: ~100-200 KB
- 10,000 snapshots: ~1-2 MB
- 1,000,000 snapshots: ~100-200 MB

**Recording Duration** (at 10 Hz):
- 1 GB SD card: ~4-8 million snapshots ≈ ~1-2 million seconds ≈ ~300+ hours

---

## Reading SD Card Files

### Option 1: Read on Computer

1. **Remove SD Card** from Arduino shield
2. **Insert into computer** SD card reader
3. **Copy `/snapshots/` directory** to computer
4. **Parse JSON** with any JSON parser:

```python
# Python example
import json

with open('snapshot_001.json', 'r') as f:
    for line in f:
        snapshot = json.loads(line)
        print(f"Time: {snapshot['ts']} ms")
        print(f"Roll: {snapshot['e']['r']:.1f}°")
        print(f"Pitch: {snapshot['e']['p']:.1f}°")
        print(f"Yaw: {snapshot['e']['y']:.1f}°")
        print(f"Calibration: {snapshot['c']['s']}/3")
```

### Option 2: Read via Serial

Arduino can dump snapshot files via serial:

```cpp
void dump_snapshot_file(const char* filename) {
    File file = SD.open(filename);
    if (!file) {
        Serial.println("ERROR: File not found");
        return;
    }
    
    while (file.available()) {
        char c = file.read();
        Serial.write(c);
    }
    file.close();
}

// Usage
dump_snapshot_file("/snapshots/snapshot_001.json");
```

---

## Troubleshooting

### Issue: SD Card Not Detected

**Symptoms**: Serial output shows "ERROR: SD card not ready"

**Diagnosis**:
1. Check SD card is inserted properly
2. Check power (3.3V) is connected
3. Check SPI wiring (MOSI, MISO, SCK, CS)
4. Try another SD card

**Test Code**:
```cpp
void test_sd_card() {
    Serial.println("Testing SD card...");
    
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("SD card initialization failed");
        return;
    }
    
    Serial.println("SD card initialized successfully");
    
    File root = SD.open("/");
    while (File file = root.openNextFile()) {
        Serial.println(file.name());
    }
}
```

### Issue: Snapshots Not Recording

**Symptoms**: Button press or code call shows success, but no files appear on SD card

**Diagnosis**:
1. Check SNAPSHOT_MODE is enabled (build flag in platformio.ini)
2. Check SD card has free space
3. Check `/snapshots/` directory exists
4. Check file permissions

**Test Code**:
```cpp
void test_snapshot_recording() {
    #if ENABLE_SNAPSHOT_RECORDER
        Quaternion q(0.707f, 0.0f, 0.707f, 0.0f);
        uint8_t cal_status = 3;
        uint32_t ts = millis();
        
        if (snapshot_record(q, cal_status, ts)) {
            Serial.println("Snapshot recorded successfully");
        } else {
            Serial.println("ERROR: Snapshot recording failed");
        }
    #else
        Serial.println("ERROR: SNAPSHOT_MODE not enabled");
    #endif
}
```

### Issue: SD Card Write Slow

**Symptoms**: Recording causes noticeable lag in main loop

**Diagnosis**:
1. SD write latency is typically 10-100 ms
2. This is normal for SD cards
3. Consider:
   - Recording less frequently
   - Using non-blocking write (advanced)
   - Upgrading to faster SD card

**Mitigation**:
```cpp
// Record at lower frequency to reduce impact
const unsigned long SNAPSHOT_INTERVAL_MS = 1000;  // 1 Hz instead of 10 Hz
```

### Issue: Corrupted JSON Files

**Symptoms**: JSON parser fails to read snapshot files

**Possible Causes**:
1. Power loss during write
2. SD card corruption
3. Wrong file format (check for extra characters)

**Recovery**:
1. Check file with text editor (look for incomplete lines)
2. Remove incomplete final line if present
3. Use JSON validator online to check format
4. Re-record data

---

## Performance Impact

### Memory Usage

When SNAPSHOT_MODE enabled:
- Buffer: ~1024 bytes (configurable in `src/config/mode.h`)
- SnapshotRecorder object: ~64 bytes
- **Total**: ~1.1 KB overhead

When SNAPSHOT_MODE disabled:
- **Total**: 0 bytes (completely compiled out)

### Computation Time

Recording one snapshot:
- Quaternion-to-Euler conversion: ~8 µs
- JSON serialization: ~5 µs
- SD card write: ~10-100 ms (blocking I/O)

**Main Loop Impact**:
- Without recording: 0 overhead
- Recording every 100 ms (10 Hz): ~0.1-1 ms per loop iteration
- Recording every 1 second: ~0.01-0.1 ms per loop iteration

**Recommendation**: Record at 10 Hz or lower for minimal impact

### SD Card Write Characteristics

- **Latency**: 10-100 ms per write (normal for SD cards)
- **Throughput**: ~1-2 MB/s (depends on card)
- **Wear**: Minimal (SD cards typically rated for billions of writes)

---

## Example Code

### Complete Snapshot Recording Example

```cpp
#include "src/math/quaternion_conversions.h"
#include "src/sensors/bno085.h"
#include "src/config/mode.h"

BNO085 imu;
unsigned long last_snapshot_ms = 0;
const unsigned long SNAPSHOT_INTERVAL_MS = 100;  // 10 Hz

void setup() {
    Serial.begin(115200);
    
    imu.initialize();
    
    #if ENABLE_SNAPSHOT_RECORDER
        snapshot_recorder_init();
        Serial.println("Snapshot recorder ready");
    #endif
}

void loop() {
    unsigned long now = millis();
    
    // Update IMU
    if (imu.has_reading()) {
        Quaternion q = imu.get_quaternion();
        EulerAngles euler = quaternion_to_euler_degrees(q);
        uint8_t cal_status = imu.get_calibration_status();
        
        // Record snapshot periodically
        #if ENABLE_SNAPSHOT_RECORDER
            if (now - last_snapshot_ms >= SNAPSHOT_INTERVAL_MS) {
                if (snapshot_record(q, cal_status, now)) {
                    Serial.print(".");  // Quiet feedback
                } else {
                    Serial.println("ERROR: Snapshot failed");
                }
                last_snapshot_ms = now;
            }
        #endif
        
        // Serial output (if CALIBRATION_MODE)
        CAL_PRINTF("Q: [%.3f, %.3f, %.3f, %.3f]\n", q.w, q.x, q.y, q.z);
        CAL_PRINTF("Euler: R=%.1f P=%.1f Y=%.1f\n", 
                   euler.roll, euler.pitch, euler.yaw);
    }
}
```

### Dump All Snapshots Example

```cpp
void dump_all_snapshots() {
    File snapshots_dir = SD.open("/snapshots/");
    if (!snapshots_dir) {
        Serial.println("ERROR: /snapshots/ directory not found");
        return;
    }
    
    while (File file = snapshots_dir.openNextFile()) {
        if (!file.isDirectory()) {
            Serial.print("File: ");
            Serial.println(file.name());
            
            while (file.available()) {
                Serial.write(file.read());
            }
            Serial.println();
            file.close();
        }
    }
    
    snapshots_dir.close();
}

// Usage: Call from Serial menu or automated script
// Then copy output to Python for analysis
```

---

## References

- **Configuration**: See `src/config/mode.h` for build flags
- **Implementation**: See `src/features/snapshot_recorder.h`
- **Hardware Setup**: See `HARDWARE_SETUP.md`
- **Testing**: See `PHASE_1_TEST_RESULTS.md`

---

## FAQ

**Q: Can I use SNAPSHOT_MODE without an SD card?**  
A: No, the feature requires an SD card. However, it fails gracefully if SD card is missing.

**Q: How long can I record continuously?**  
A: At 10 Hz on a 1 GB card, approximately 300+ hours. Practically, download data every 10-50 hours.

**Q: What happens if SD card fills up?**  
A: New snapshots cannot be recorded. Download files to clear space.

**Q: Can I change the JSON format?**  
A: Yes, edit `src/features/snapshot_recorder.cpp`. Ensure you document changes.

**Q: Is there a real-time GUI for snapshots?**  
A: Not in Phase 1. Download files and analyze with Python/Excel.

**Q: How do I verify calibration from snapshots?**  
A: Check the `c` (calibration) field. All four values should be 3 for fully calibrated data.

---

## Next Steps

1. **Enable SNAPSHOT_MODE** in build configuration
2. **Compile and upload** to Arduino
3. **Test with SD card** in shield
4. **Record sample data** (1-2 minutes)
5. **Download and analyze** JSON files
6. **Adjust recording frequency** based on performance impact

For Phase 2, planned enhancements:
- Non-blocking SD writes
- Real-time web dashboard
- Automatic SD card management
- Compression for longer recording
