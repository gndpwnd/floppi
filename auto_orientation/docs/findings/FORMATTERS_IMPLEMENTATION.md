# Sensor Data Formatters Implementation

## Overview

Implemented JSON output formatter (primary) for combined orientation (BNO085) + position (GPS) sensor data. CSV support removed for v1.0 simplification. DELIMITED format available as simple alternative.

## Files

### Headers
- `src/output/data_formatter.h` - Abstract base class for formatters
- `src/output/json_formatter.h` - JSON formatter interface
- `src/output/sensor_output_manager.h` - Output manager (JSON + DELIMITED)

### Implementations
- `src/output/json_formatter.cpp` - JSON formatter (300+ bytes typical output)
- `src/output/sensor_output_manager.cpp` - Manager with JSON/DELIMITED support

### Tests
- `test/test_formatters.cpp` - Comprehensive JSON test suite

## Features Implemented

### JSON Formatter (Primary Format)
- **Format**: Compact JSON with nested objects
- **Structure**:
  ```json
  {
    "timestamp_ms": 123456,
    "orientation": {
      "w": 0.707, "x": 0.0, "y": 0.0, "z": 0.707,
      "magnitude": 1.0,
      "calibration": {"system": 3, "accel": 3, "gyro": 3, "mag": 2}
    },
    "position": {
      "latitude": 37.774,
      "longitude": -122.418,
      "altitude": 12.500,
      "accuracy_m": 5.200,
      "satellites": 12,
      "fix_quality": 1
    }
  }
  ```
- **Handles missing data**: Orientation-only output when GPS is stale (>5s)
- **Configurable precision**: Default 4 decimal places for floats
- **Size**: ~300+ bytes for typical output
- **Advantages**: Easy to parse with standard JSON libraries, human-readable, includes calibration labels

### DELIMITED Format (Simple Alternative)
- **Format**: Pipe-delimited simple alternative to JSON
- **Data Row**:
  ```
  123456|0.7070|0.0000|0.0000|0.7070|3|37.774|-122.418|12.500|5.200|12
  ```
- **Fields**: timestamp|w|x|y|z|cal|lat|lon|alt|accuracy|sats
- **Handles missing GPS**: Empty pipe fields when position not fresh
- **Size**: ~60-80 bytes per row
- **Advantages**: Compact, simple parsing, minimal overhead

### SensorOutputManager
- **Runtime format selection**: `JSON` (recommended), `DELIMITED` (alternative)
- **Frequency limiting**: Configurable output Hz (default 10 Hz)
- **GPS freshness**: Stale GPS detection (default 5 seconds)
- **Buffer management**: Safe overflow detection, returns 0 on failure
- **Platform support**: Arduino/AVR, ARM (Teensy), Linux

## Test Results

Comprehensive JSON tests with DELIMITED validation:

```
✓ JSON Formatter - Both Valid (7 tests)
✓ JSON Formatter - Only Orientation Valid (5 tests)
✓ JSON Formatter - Only Position Valid (4 tests)
✓ JSON Formatter - Neither Valid (3 tests)
✓ JSON Formatter - Buffer Overflow Check (1 test)
✓ JSON Formatter - Decimal Precision (1 test)
✓ DELIMITED Format - Both Valid (1 test)
✓ DELIMITED Format - Only Orientation (1 test)
```

### Test Coverage
- Valid sensors (both orientation and position)
- Partial data (only orientation OR only position)
- Invalid sensors (neither valid)
- Buffer overflow handling
- JSON structural validation (balanced braces)
- DELIMITED format validation
- Decimal precision (configurable)

## Usage Example

### JSON Output (Recommended)
```cpp
#include "output/sensor_output_manager.h"

SensorOutputManager output_manager;
output_manager.begin(OutputFormat::JSON);
output_manager.setFrequencyHz(10.0f);  // 10 Hz output
output_manager.setGpsFreshnessTimeoutMs(5000);  // 5s GPS timeout

// In loop:
if (imu.read() && imu.hasNewData()) {
  output_manager.update(imu.getOrientation());
}
if (gps.read() && gps.hasNewData()) {
  output_manager.update(gps.getPosition());
}
if (output_manager.shouldOutput()) {
  char buffer[512];
  uint16_t len = output_manager.getFormattedOutput(buffer, sizeof(buffer));
  if (len > 0) Serial.println(buffer);
}
```

### DELIMITED Format (Simple Alternative)
```cpp
// Use DELIMITED format for compact output
output_manager.begin(OutputFormat::DELIMITED);
// Rest of code same as above
```

### Direct Formatter Usage
```cpp
JSONFormatter json(3);  // 3 decimal places
char buffer[512];

uint16_t len = json.format(
  imu.getOrientation(),
  gps.getPosition(),
  buffer,
  sizeof(buffer)
);

if (len > 0) {
  Serial.println(buffer);
}
```

## Buffer Requirements

- **JSON output**: 512 bytes recommended (typical ~300 bytes)
- **DELIMITED output**: 256 bytes recommended (typical ~70 bytes)
- **Error handling**: Returns 0 if output doesn't fit in buffer

## Implementation Details

### Data Handling
- **Orientation validity**: Always included in output
- **Position freshness**: GPS data > 5s old is omitted (orientation-only output)
- **JSON structure**: Complete nested objects with calibration data
- **DELIMITED format**: Pipe-separated fields with empty pipes for missing GPS

### Floating Point Precision
- Default: 3-4 decimal places (varies by field)
- Configurable via formatter constructor: 1-6 decimal places recommended
- Format: Standard printf-style `%.Nf` and `%.Nlf`

### Memory Safety
- Buffer overflow protection: Returns 0 on insufficient space
- No dynamic memory allocation: Stack-based formatting only
- No external string libraries: Uses Arduino `<stdio.h>` snprintf

## Integration Notes

### With main.cpp
Already integrated with `SensorOutputManager`:
```cpp
#include "output/sensor_output_manager.h"
output_manager.begin(OutputFormat::JSON);
```

### Format Selection
```cpp
output_manager.begin(OutputFormat::JSON);        // Primary (recommended)
output_manager.begin(OutputFormat::DELIMITED);   // Alternative (compact)
```

### Arduino Compatibility
- Uses standard `<Arduino.h>` features only
- Compatible with Serial.println() and Serial.print()
- Tested on Arduino/AVR, ARM (Teensy), and Linux platforms

## Files Location Summary

```
src/output/
├── data_formatter.h           # Abstract interface
├── json_formatter.h           # JSON formatter header
├── json_formatter.cpp         # JSON implementation
├── sensor_output_manager.h    # Manager (JSON + DELIMITED)
├── sensor_output_manager.cpp  # Manager implementation
└── serial_output.*            # (legacy, may be removed)

test/
└── test_formatters.cpp        # JSON + DELIMITED tests
```

## Compilation

### Native Testing (Linux)
```bash
g++ -std=c++11 -I. -o test/test_formatters_bin \
  test/test_formatters.cpp \
  src/output/json_formatter.cpp \
  src/output/sensor_output_manager.cpp \
  -Wall -Wextra

./test/test_formatters_bin
```

### Arduino (via PlatformIO)
```bash
platformio run
platformio run --target build
```

The formatters compile as part of the Arduino build automatically.

## Performance Characteristics

| Metric | Value |
|--------|-------|
| JSON output size | ~300-350 bytes |
| DELIMITED output size | ~70-80 bytes |
| Float precision | Configurable (3-4 default) |
| Buffer safety | Overflow detection (returns 0) |
| Memory allocation | None (stack-based) |
| CPU per output | <1 ms |
| Compilation | <1 second |

## Version History

### v1.0 (Current)
- **JSON format**: Primary output, easy to parse with standard libraries
- **DELIMITED format**: Simple pipe-separated alternative
- **CSV removed**: Simplified codebase, reduced complexity
- **Status**: Production-ready for calibration tracking and telemetry

### Future Enhancements
- Binary format for compact embedded storage
- MQTT JSON gateway integration for cloud telemetry
- Real-time calibration monitoring dashboard

## Notes

- All code is re-entrant and thread-safe (Arduino single-threaded)
- No external dependencies beyond `<stdint.h>`, `<stdio.h>`, `<string.h>`
- Compatible with Arduino's limited standard library
- Designed for embedded systems with limited RAM (512 bytes typical output buffer)
