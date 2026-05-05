# Sensor Output Manager (Task 7)

## Overview

The `SensorOutputManager` is a combined output interface that multiplexes orientation (BNO085 @ 10 Hz) and position (NEO-M9N @ 1 Hz) data streams into a single formatted output.

## Key Features

### 1. Dual-Frequency Data Handling
- **Orientation**: 10 Hz update rate (100 ms intervals)
- **Position**: 1 Hz update rate (1000 ms intervals)
- Intelligent buffering handles frequency mismatch

### 2. Output Control
- Frequency-based throttling (configurable Hz)
- Default: 10 Hz output to match IMU data rate
- Uses elapsed time tracking: `shouldOutput()` returns true when interval expires

### 3. GPS Freshness Detection
- Tracks timestamp of last GPS update
- Default timeout: 5000 ms (5 seconds)
- Automatically omits stale GPS data from output
- Falls back to orientation-only output if GPS stale

### 4. Multiple Output Formats
- **JSON**: Human-readable, nested structure
- **CSV Header**: Column names for data logging
- **CSV Data**: Delimited values for spreadsheet import

## Architecture

### Class Diagram
```
SensorOutputManager
├── Data State
│   ├── OrientationData (latest)
│   ├── PositionData (latest)
│   ├── orientation_valid_ (bool)
│   ├── position_valid_ (bool)
│   ├── position_fresh_ (bool)
│   ├── orientation_timestamp_ms_
│   └── position_timestamp_ms_
│
├── Configuration
│   ├── format_ (OutputFormat)
│   ├── frequency_hz_
│   ├── output_interval_ms_
│   ├── gps_freshness_timeout_ms_
│   └── last_output_ms_
│
└── Methods
    ├── begin(OutputFormat)
    ├── update(const OrientationData&)
    ├── update(const PositionData&)
    ├── shouldOutput() → bool
    ├── getFormattedOutput(buffer, len) → uint16_t
    ├── getOutputFormat() → OutputFormat
    └── setters for configuration
```

### Data Flow

```
BNO085 (10 Hz)      NEO-M9N (1 Hz)
   │                    │
   v                    v
update(orientation)  update(position)
   │                    │
   └─────────┬──────────┘
             v
        Store Latest
        (timestamps)
             │
             v
    shouldOutput() called
             │
        Check interval
             │
        yes/no
             │
    getFormattedOutput()
             │
        Format JSON/CSV
        Include position if fresh
             │
             v
         Output
```

## Usage Example

```cpp
#include "output/sensor_output_manager.h"

// Global instance
SensorOutputManager output_manager;

void setup() {
  // Initialize with JSON format
  output_manager.begin(OutputFormat::JSON);
  
  // Configure frequency
  output_manager.setFrequencyHz(10.0f);      // 10 Hz output
  output_manager.setGpsFreshnessTimeoutMs(5000);  // 5 sec GPS timeout
}

void loop() {
  // Read and feed sensors to manager
  if (imu.read()) {
    if (imu.hasNewData()) {
      output_manager.update(imu.getOrientation());
    }
  }

  if (gps.read()) {
    if (gps.hasNewData()) {
      output_manager.update(gps.getPosition());
    }
  }

  // Output when ready (frequency-controlled)
  if (output_manager.shouldOutput()) {
    char buffer[256];
    uint16_t len = output_manager.getFormattedOutput(buffer, sizeof(buffer));
    
    if (len > 0) {
      Serial.println(buffer);
    }
  }
}
```

## Output Formats

### JSON Format
Example with both orientation and position:
```json
{
  "timestamp": 45230,
  "orientation": {
    "w": 0.7071,
    "x": 0.0000,
    "y": 0.7071,
    "z": 0.0000,
    "magnitude": 1.0000,
    "calibration": {
      "system": 3,
      "accel": 3,
      "gyro": 3,
      "mag": 3
    }
  },
  "position": {
    "latitude": 37.774900,
    "longitude": -122.419400,
    "altitude": 10.50,
    "accuracy_m": 2.50,
    "satellites": 12,
    "fix_quality": 1
  }
}
```

Example with orientation only (stale GPS):
```json
{
  "timestamp": 45330,
  "orientation": {
    "w": 0.7071,
    "x": 0.0000,
    "y": 0.7071,
    "z": 0.0000,
    "magnitude": 1.0000,
    "calibration": {
      "system": 3,
      "accel": 3,
      "gyro": 3,
      "mag": 3
    }
  }
}
```

### CSV Format
Header:
```
timestamp_ms,quat_w,quat_x,quat_y,quat_z,quat_magnitude,cal_system,cal_accel,cal_gyro,cal_mag,gps_lat,gps_lon,gps_alt_m,gps_acc_m,gps_sats,gps_fix
```

Data rows:
```
45230,0.7071,0.0000,0.7071,0.0000,1.0000,3,3,3,3,37.774900,-122.419400,10.50,2.50,12,1
45330,0.7071,0.0000,0.7071,0.0000,1.0000,3,3,3,3,,,,,
```

## Implementation Details

### Frequency Control
The manager uses a simple elapsed-time check:
```cpp
bool SensorOutputManager::shouldOutput() {
  if (!orientation_valid_) return false;
  
  uint32_t now_ms = millis();
  if (now_ms - last_output_ms >= output_interval_ms_) {
    return true;
  }
  return false;
}
```

When `setFrequencyHz(10.0f)` is called:
- `output_interval_ms_` = 1000 / 10 = 100 ms
- Output is ready every 100 ms

### GPS Freshness
Position data age is computed on-demand:
```cpp
bool SensorOutputManager::isPositionFresh() const {
  if (!position_valid_) return false;
  
  uint32_t now_ms = millis();
  uint32_t age_ms = now_ms - position_timestamp_ms_;
  
  return age_ms <= gps_freshness_timeout_ms_;  // Default 5000 ms
}
```

If GPS data is older than timeout, `position_fresh_` is set to false and position is omitted from output.

### Quaternion Magnitude
Used as data validation check (should be ~1.0 for normalized quaternions):
```cpp
float q_mag = sqrt(w*w + x*x + y*y + z*z);
```

If magnitude deviates significantly from 1.0, indicates potential sensor error.

## Thread Safety

**Current Implementation**: NOT thread-safe.
- Assumes single-threaded Arduino environment
- All sensor reads and output in main `loop()`
- If multi-threaded in future, add mutex protection around:
  - `orientation_` update
  - `position_` update
  - Timestamp fields

## Memory Usage
- Arduino Mega: ~150 bytes RAM (orientation + position buffers)
- Typical JSON output: 150-200 bytes
- Buffer recommendation: 256 bytes minimum, 512 bytes safe

## Integration with Main

The main.cpp has been updated to use SensorOutputManager:

```cpp
// Global instance
SensorOutputManager output_manager;

void setup() {
  // ... sensor initialization ...
  
  // Initialize output manager
  output_manager.begin(OutputFormat::JSON);
  output_manager.setFrequencyHz(10.0f);
  output_manager.setGpsFreshnessTimeoutMs(5000);
}

void loop() {
  // Read sensors
  if (imu.read() && imu.hasNewData()) {
    output_manager.update(imu.getOrientation());
  }

  if (gps.read() && gps.hasNewData()) {
    output_manager.update(gps.getPosition());
  }

  // Output when ready
  if (output_manager.shouldOutput()) {
    char buffer[256];
    uint16_t len = output_manager.getFormattedOutput(buffer, sizeof(buffer));
    
    if (len > 0) {
      Serial.println(buffer);
    }
  }
}
```

## Testing

See `tests/test_sensor_output_manager.cpp` for unit tests covering:
- JSON/CSV formatting
- Frequency control
- Data freshness checks
- Buffer overflow protection
- Format conversion

## Future Enhancements

1. **Selective Field Output**: Allow enabling/disabling specific calibration fields
2. **Custom Timestamps**: Use system clock instead of millis() for higher resolution
3. **Buffering Strategy**: Queue multiple updates before output for better synchronization
4. **Compression**: Optional gzip output for bandwidth-limited links
5. **Stream Validation**: Add checksum/CRC to output for serial transmission verification

## Files Modified

- `src/output/sensor_output_manager.h` - Header
- `src/output/sensor_output_manager.cpp` - Implementation
- `src/main.cpp` - Integration example
- `tests/test_sensor_output_manager.cpp` - Unit tests

## Compilation

Successfully compiles on Arduino Mega (ATmega2560):
- Flash: 26692 / 253952 bytes (10.5%)
- RAM: 5404 / 8192 bytes (66%)
- Build time: ~0.5 seconds

Builds with PlatformIO:
```bash
platformio run -e arduino_mega
```
