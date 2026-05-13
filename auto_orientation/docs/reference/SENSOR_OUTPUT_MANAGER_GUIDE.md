# Sensor Output Manager - Quick Reference

## What It Does

Combines BNO085 orientation (10 Hz) and NEO-M9N GPS (1 Hz) data into a single output stream with intelligent buffering and frequency control.

## Quick Start

### 1. Initialization
```cpp
#include "output/sensor_output_manager.h"

SensorOutputManager output_manager;

// In setup():
output_manager.begin(OutputFormat::JSON);
output_manager.setFrequencyHz(10.0f);        // Output 10 times per second
output_manager.setGpsFreshnessTimeoutMs(5000); // GPS timeout = 5 seconds
```

### 2. Feed Sensor Data
```cpp
// In loop():
if (imu.read() && imu.hasNewData()) {
  output_manager.update(imu.getOrientation());
}

if (gps.read() && gps.hasNewData()) {
  output_manager.update(gps.getPosition());
}
```

### 3. Output When Ready
```cpp
if (output_manager.shouldOutput()) {
  char buffer[256];
  uint16_t len = output_manager.getFormattedOutput(buffer, sizeof(buffer));
  
  if (len > 0) {
    Serial.println(buffer);
  }
}
```

## Output Formats

### JSON (default)
Perfect for web APIs and real-time visualization.
```json
{"timestamp":45230,"orientation":{"w":0.7071,"x":0.0000,"y":0.7071,"z":0.0000,"magnitude":1.0000,"calibration":{"system":3,"accel":3,"gyro":3,"mag":3}},"position":{"latitude":37.774900,"longitude":-122.419400,"altitude":10.50,"accuracy_m":2.50,"satellites":12,"fix_quality":1}}
```

### CSV
Perfect for data logging to spreadsheets.
```
timestamp_ms,quat_w,quat_x,quat_y,quat_z,quat_magnitude,cal_system,cal_accel,cal_gyro,cal_mag,gps_lat,gps_lon,gps_alt_m,gps_acc_m,gps_sats,gps_fix
45230,0.7071,0.0000,0.7071,0.0000,1.0000,3,3,3,3,37.774900,-122.419400,10.50,2.50,12,1
```

## Configuration Options

| Method | Purpose | Default |
|--------|---------|---------|
| `setFormat(OutputFormat)` | Change output format | JSON |
| `setFrequencyHz(float)` | Set output rate | 10.0 Hz |
| `setGpsFreshnessTimeoutMs(uint32_t)` | GPS stale timeout | 5000 ms |
| `getOutputFormat()` | Query current format | - |

## Important Behaviors

### Frequency Control
- Only outputs when `shouldOutput()` returns true
- Time is tracked per output
- 10 Hz = output every 100 ms
- 1 Hz = output every 1000 ms

### GPS Freshness
- GPS updates are timestamped when `update(PositionData)` is called
- If GPS data is older than `gps_freshness_timeout_ms_`, it's omitted from output
- Output falls back to orientation-only if GPS is stale
- Useful for detecting GPS lock loss

### Data Validity
- Orientation-only output if GPS never received or is stale
- Both orientation and position if GPS is fresh
- Quaternion magnitude included for validation (should be ~1.0)

## Memory Requirements

| Item | Usage |
|------|-------|
| Class instance | ~150 bytes |
| JSON output (max) | ~200 bytes |
| CSV output (max) | ~150 bytes |
| Recommended buffer | 256 bytes |

**Arduino Mega**: Uses 66% RAM, 10.5% Flash

## Troubleshooting

### No Output Appearing
1. Check `shouldOutput()` - is time interval reached?
2. Check `orientation_valid_` - did you call `update(orientation)`?
3. Check buffer size - minimum 256 bytes

### JSON Missing Position
1. GPS data not fresh (check `setGpsFreshnessTimeoutMs()`)
2. GPS never updated (check `update(PositionData)` calls)
3. GPS timeout expired (default 5 seconds)

### CSV Has Empty GPS Fields
- This is normal when GPS is stale
- Position fields are intentionally blank to preserve CSV column alignment
- Indicates GPS data was older than timeout

### Compile Errors
- Include `<Arduino.h>` before header files
- Use C headers: `<math.h>` not `<cmath>`, `<stdio.h>` not `<cstdio>`
- Arduino Mega ATmega2560 supported, others untested

## Integration Example

Complete working example in `src/main.cpp`:
```cpp
#include <Arduino.h>
#include "config/pins.h"
#include "sensors/sensor_base.h"
#include "output/sensor_output_manager.h"
#include "sensors/bno085.h"
#include "sensors/neo_m9n.h"

BNO085 imu;
NEOM9N gps;
SensorOutputManager output_manager;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(100);

  imu.begin();
  gps.begin();
  output_manager.begin(OutputFormat::JSON);
  output_manager.setFrequencyHz(10.0f);
}

void loop() {
  if (imu.read() && imu.hasNewData()) {
    output_manager.update(imu.getOrientation());
  }
  if (gps.read() && gps.hasNewData()) {
    output_manager.update(gps.getPosition());
  }
  if (output_manager.shouldOutput()) {
    char buffer[256];
    uint16_t len = output_manager.getFormattedOutput(buffer, sizeof(buffer));
    if (len > 0) Serial.println(buffer);
  }
}
```

## Performance

- Compile time: ~0.5 seconds (PlatformIO)
- Output generation: <1 ms per call
- Memory overhead: ~300 bytes total

## API Reference

### Methods
- `bool begin(OutputFormat)` - Initialize
- `void setFormat(OutputFormat)` - Change output format
- `void setFrequencyHz(float)` - Set output frequency
- `void setGpsFreshnessTimeoutMs(uint32_t)` - Set GPS timeout
- `void update(const OrientationData&)` - Feed orientation
- `void update(const PositionData&)` - Feed position
- `bool shouldOutput()` - Check if ready to output
- `uint16_t getFormattedOutput(char*, uint16_t)` - Get formatted string
- `OutputFormat getOutputFormat()` - Query current format
- `uint16_t getCSVHeader(char*, uint16_t)` - Get CSV header

### Data Structures

**OrientationData**
```cpp
struct OrientationData {
  float w, x, y, z;        // Quaternion
  uint8_t cal_status;      // Calibration status (0-3)
  uint8_t cal_accel;       // Accel calibration (0-3)
  uint8_t cal_gyro;        // Gyro calibration (0-3)
  uint8_t cal_mag;         // Mag calibration (0-3)
  uint32_t timestamp_ms;   // Timestamp
};
```

**PositionData**
```cpp
struct PositionData {
  double latitude;         // Degrees
  double longitude;        // Degrees
  float altitude;          // Meters
  float accuracy_m;        // CEP in meters
  uint8_t num_satellites;  // Satellite count
  uint8_t fix_quality;     // 0=invalid, 1=GPS, 2=DGPS, etc.
  uint32_t timestamp_ms;   // Timestamp
};
```

## Files

- `src/output/sensor_output_manager.h` - Header
- `src/output/sensor_output_manager.cpp` - Implementation
- `src/main.cpp` - Updated to use manager
- `tests/test_sensor_output_manager.cpp` - Unit tests
- `docs/implementation/sensor_output_manager.md` - Detailed docs

## Status

✅ Implemented and compiling  
✅ JSON/CSV output formatting  
✅ Frequency control  
✅ GPS freshness detection  
✅ Integrated with main.cpp  
✅ Memory optimized  
⏳ Ready for hardware integration and field testing

---

For more details, see `docs/implementation/sensor_output_manager.md`
