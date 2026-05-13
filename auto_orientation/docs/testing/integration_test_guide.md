# Integration Test Suite Guide (Task 19)

## Overview

This guide covers the BNO085 + GPS combined sensor integration test suite (`tests/integration_tests.cpp`).

The test suite validates that:
1. Each sensor can initialize and operate independently
2. Both sensors can read data simultaneously
3. Combined output (JSON/CSV) is valid and complete
4. Data validation checks catch invalid readings
5. Error conditions are handled gracefully
6. Timing and frequency requirements are met

## Test Statistics

- **Total Tests**: 24
- **Test Categories**: 8
  - BNO085 Initialization (5 tests)
  - GPS Initialization (4 tests)
  - Combined Output (4 tests)
  - Data Validation (3 tests)
  - Error Handling (3 tests)
  - Timing & Frequency (3 tests)
  - Output Formatting (3 tests)
  - Sensor Health (3 tests)

## Building Tests

### Prerequisites

```bash
# Install PlatformIO
pip install platformio

# Install Google Test (used as test framework)
# PlatformIO handles this automatically
```

### Build Commands

**Arduino Mega:**
```bash
cd /home/devel/floppi/auto_orientation
platformio test -e arduino_mega
```

**Teensy 3.x:**
```bash
platformio test -e teensy31
```

**ESP32:**
```bash
platformio test -e esp32dev
```

**All platforms:**
```bash
platformio test
```

### Expected Build Output

```
PlatformIO: Processing arduino_mega (platform: atmelavr; board: megaatmega2560)
Building test...
Compiling .pio/build/arduino_mega/test/integration_tests.o
Linking .pio/build/arduino_mega/test/integration_tests.elf
Building .pio/build/arduino_mega/test/integration_tests.hex
Testing... [PASSED]
```

## Test Categories

### 1. BNO085 Initialization Tests (5 tests)

Tests the BNO085 orientation sensor initialization and basic operation.

#### Test: `BNO085_InitializationBegins`
- **Purpose**: Verify BNO085 begin() returns true
- **On Hardware**: Tests actual UART communication with IMU
- **On PC/Simulator**: Always passes (mocked sensor)
- **Expected Result**: PASS

#### Test: `BNO085_IsInitializedCheck`
- **Purpose**: After begin(), isInitialized() returns true
- **Validates**: Initialization state tracking
- **Expected Result**: PASS

#### Test: `BNO085_QuaternionMagnitudeValid`
- **Purpose**: Quaternion magnitude is normalized (≈ 1.0)
- **Validates**: Quaternion data quality
- **Formula**: magnitude = √(w² + x² + y² + z²)
- **Expected Range**: 0.95 - 1.05
- **Expected Result**: PASS

#### Test: `BNO085_CalibrationStatusValid`
- **Purpose**: Calibration status in range 0-3
- **Validates**: Calibration level tracking
- **Status Meanings**:
  - 0 = Uncalibrated
  - 1 = Low
  - 2 = Medium
  - 3 = High (fully calibrated)
- **Expected Result**: PASS

#### Test: `BNO085_CalibrationComponentsValid`
- **Purpose**: Each calibration component (accel, gyro, mag) in range 0-3
- **Validates**: Per-axis calibration tracking
- **Expected Result**: PASS

### 2. GPS Initialization Tests (4 tests)

Tests the NEO-M9N GPS receiver initialization and data structure.

#### Test: `NEO_M9N_InitializationBegins`
- **Purpose**: Verify NEO-M9N begin() returns true
- **On Hardware**: Tests UART connection and NMEA reception
- **Expected Result**: PASS

#### Test: `NEO_M9N_IsInitializedCheck`
- **Purpose**: After begin(), isInitialized() returns true
- **Validates**: Initialization state
- **Expected Result**: PASS

#### Test: `NEO_M9N_PositionDataStructure`
- **Purpose**: All position fields are finite numbers
- **Validates**: Data structure integrity
- **Fields Checked**:
  - latitude, longitude, altitude (finite floats)
  - accuracy_m (finite float)
  - num_satellites (non-negative)
  - fix_quality (non-negative)
- **Expected Result**: PASS

#### Test: `NEO_M9N_PositionBoundsValid`
- **Purpose**: Position coordinates within geographic bounds
- **Test Cases**:
  - San Francisco: (37.7749°N, -122.4194°W)
  - Sydney: (-33.8688°S, 151.2093°E)
  - London: (51.5074°N, -0.1278°E)
  - Tokyo: (35.6762°N, 139.6503°E)
  - Equator/Prime Meridian: (0°, 0°)
- **Constraints**:
  - Latitude: [-90.0, 90.0]
  - Longitude: [-180.0, 180.0]
- **Expected Result**: PASS

### 3. Combined Output Tests (4 tests)

Tests simultaneous operation of both sensors and output formatting.

#### Test: `CombinedSensors_BothInitialize`
- **Purpose**: Both sensors initialize independently
- **Validates**: No initialization interference
- **Expected Result**: PASS

#### Test: `CombinedSensors_SimultaneousOutput`
- **Purpose**: Output manager handles both sensor updates
- **Flow**:
  1. Update orientation data
  2. Update position data
  3. Request formatted output
- **Expected Result**: PASS

#### Test: `CombinedSensors_JSONFormatValid`
- **Purpose**: JSON output is valid and complete
- **Validates**:
  - Output length > 0 and < buffer size
  - Contains "timestamp" field
  - Contains "orientation" section with "w" component
  - Contains "position" section with "latitude"
- **Example Output**:
  ```json
  {
    "timestamp": 12345,
    "orientation": {
      "w": 0.7071,
      "x": 0.0,
      "y": 0.7071,
      "z": 0.0,
      "magnitude": 1.0,
      "calibration": {
        "status": 3,
        "accel": 3,
        "gyro": 3,
        "mag": 3
      }
    },
    "position": {
      "latitude": 37.7749,
      "longitude": -122.4194,
      "altitude": 10.5,
      "accuracy_m": 2.5,
      "satellites": 12,
      "fix_quality": 1
    }
  }
  ```
- **Expected Result**: PASS

#### Test: `CombinedSensors_CSVFormatValid`
- **Purpose**: CSV output contains all expected columns
- **Format**: Comma-separated values with at least 5 fields
- **Example Header**:
  ```
  timestamp_ms,quat_w,quat_x,quat_y,quat_z,quat_magnitude,cal_status,...,gps_lat,gps_lon,gps_alt,...
  ```
- **Expected Result**: PASS

### 4. Data Validation Tests (3 tests)

Tests that sensor data meets validity constraints.

#### Test: `DataValidation_QuaternionMagnitude`
- **Purpose**: Quaternion magnitudes are valid (normalized)
- **Test Cases**:
  - Identity: [1, 0, 0, 0] → mag = 1.0
  - 90° rotation: [0.7071, 0.7071, 0, 0] → mag ≈ 1.0
  - 120° rotation: [0.5, 0.5, 0.5, 0.5] → mag ≈ 1.0
- **Acceptable Range**: 0.95 - 1.05
- **Expected Result**: PASS

#### Test: `DataValidation_PositionBounds`
- **Purpose**: Latitude/longitude within geographic bounds
- **Test Cases**: Edge cases including poles and date line
- **Constraints**:
  - Latitude: [-90, 90]
  - Longitude: [-180, 180]
- **Expected Result**: PASS

#### Test: `DataValidation_CalibrationStatusRange`
- **Purpose**: Calibration status values are valid (0-3)
- **Note**: Out-of-range values should be detected
- **Expected Result**: PASS

### 5. Error Handling Tests (3 tests)

Tests graceful handling of error conditions.

#### Test: `ErrorHandling_InvalidQuaternion`
- **Purpose**: Zero quaternion (all 0s) is detected as invalid
- **Validates**: Data quality checks
- **Expected**: Magnitude ≈ 0.0
- **Expected Result**: PASS

#### Test: `ErrorHandling_InvalidPosition`
- **Purpose**: Out-of-bounds position is rejected
- **Test Case**: lat = 95° (invalid), lon = 200° (invalid)
- **Expected**: IsPositionValid() returns false
- **Expected Result**: PASS

#### Test: `ErrorHandling_MissingGPSRecovery`
- **Purpose**: System outputs orientation when GPS is unavailable
- **Scenario**: Update orientation only, no position update
- **Expected**: JSON contains "orientation" field
- **Behavior**: Graceful degradation to orientation-only output
- **Expected Result**: PASS

### 6. Timing and Frequency Tests (3 tests)

Tests that sensor frequencies meet specifications.

#### Test: `Timing_BNO085Frequency10Hz`
- **Purpose**: BNO085 configured for 10 Hz output
- **Expected Period**: 100 ms (100,000 µs)
- **Actual Hardware**: Enforced by Adafruit library:
  ```cpp
  imu_->enableReport(SH2_ROTATION_VECTOR, 100000);  // 100 ms
  ```
- **Expected Result**: PASS

#### Test: `Timing_GPSFrequency1Hz`
- **Purpose**: NEO-M9N configured for 1 Hz output
- **Expected Period**: 1000 ms
- **Note**: GPS frequency determined by NMEA reception rate
- **Expected Result**: PASS

#### Test: `Timing_OutputFrequencyControl`
- **Purpose**: Output manager respects frequency setting
- **Scenario**:
  1. Set output frequency to 1 Hz
  2. Call shouldOutput() → returns true (first call)
  3. Call shouldOutput() immediately → returns false (frequency control)
- **Validates**: No output flooding
- **Expected Result**: PASS

### 7. Output Formatting Tests (3 tests)

Tests output format correctness and robustness.

#### Test: `OutputFormat_JSONStructure`
- **Purpose**: JSON output has valid syntax
- **Validates**:
  - Starts with `{`
  - Ends with `}`
  - Contains quoted strings `"`
- **Expected Result**: PASS

#### Test: `OutputFormat_CSVHeader`
- **Purpose**: CSV header contains expected columns
- **Expected Columns**: timestamp, position (lat/lon), orientation (quat)
- **Format**: Comma-separated, no spaces
- **Expected Result**: PASS

#### Test: `OutputFormat_BufferOverflowProtection`
- **Purpose**: Output doesn't overflow small buffer
- **Scenario**: Buffer size = 10 bytes (too small)
- **Expected**: Written length ≤ 9 bytes (safe margin)
- **Expected Result**: PASS

### 8. Sensor Health Tests (3 tests)

Tests sensor status reporting.

#### Test: `SensorHealth_BNO085StatusString`
- **Purpose**: BNO085 provides non-empty status string
- **Example Output**: "BNO085: Initialized, Cal: 3/3/3/3"
- **Expected Result**: PASS

#### Test: `SensorHealth_NEO_M9NStatusString`
- **Purpose**: NEO-M9N provides non-empty status string
- **Example Output**: "GPS: 12 sats, GPS fix, HDOP 0.9m"
- **Expected Result**: PASS

#### Test: `SensorHealth_BNO085NameString` / `NEO_M9NNameString`
- **Purpose**: Sensors return correct identification strings
- **Expected**:
  - BNO085.name() == "BNO085"
  - GPS.name() == "NEO-M9N GPS"
- **Expected Result**: PASS

## Running Individual Tests

### Using Google Test Filter

```bash
# Run only BNO085 tests
platformio test -e arduino_mega -- --gtest_filter="*BNO085*"

# Run only GPS tests
platformio test -e arduino_mega -- --gtest_filter="*NEO_M9N*"

# Run only combined output tests
platformio test -e arduino_mega -- --gtest_filter="*CombinedSensors*"

# Run only a specific test
platformio test -e arduino_mega -- --gtest_filter="*QuaternionMagnitude*"
```

## Interpreting Test Output

### Successful Test Run

```
[==========] Running 24 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 24 tests from IntegrationTestFixture
[ RUN      ] IntegrationTestFixture.BNO085_InitializationBegins
[       OK ] IntegrationTestFixture.BNO085_InitializationBegins (2 ms)
[ RUN      ] IntegrationTestFixture.BNO085_IsInitializedCheck
[       OK ] IntegrationTestFixture.BNO085_IsInitializedCheck (1 ms)
...
[==========] 24 tests from 1 test suite ran. (48 ms total)
[       OK ] All tests passed!
```

### Failed Test Example

```
[ RUN      ] IntegrationTestFixture.BNO085_QuaternionMagnitudeValid
tests/integration_tests.cpp:245: Failure
Expected: magnitude near 1.0 (tolerance: 0.01)
Actual: 2.45
[  FAILED  ] IntegrationTestFixture.BNO085_QuaternionMagnitudeValid (1 ms)
```

**Interpretation**: The quaternion is not normalized. Check BNO085 data output.

### Assertion Types Used

```cpp
EXPECT_TRUE(condition)          // Assert condition is true
EXPECT_FALSE(condition)         // Assert condition is false
EXPECT_EQ(actual, expected)     // Assert equal
EXPECT_NE(actual, expected)     // Assert not equal
EXPECT_GE(actual, lower)        // Assert greater than or equal
EXPECT_LE(actual, upper)        // Assert less than or equal
EXPECT_NEAR(actual, expected, tolerance)  // Assert within tolerance
EXPECT_STREQ(str1, str2)        // Assert strings equal
EXPECT_NE(pos, std::string::npos)  // Assert substring found in string
```

## Hardware Testing

### Prerequisites for Hardware Testing

1. **BNO085 IMU**
   - Connection: Serial1 (hardware UART)
   - Baud: 115200
   - Status: Should see "Initialized" in status string

2. **NEO-M9N GPS**
   - Connection: Serial3 (hardware UART) on Mega, Serial2 on others
   - Baud: 115200
   - Status: Should acquire satellites within ~30 seconds outdoors

### Running on Hardware

```bash
# Upload and run tests on Arduino Mega
platformio test -e arduino_mega -vv

# Output will show serial monitor with test results
```

### Expected Hardware Test Results

**BNO085 Tests** (should pass with sensor connected):
- Initialization succeeds
- Quaternion magnitude is 1.0 ±0.01
- Calibration status progresses (0→3 over ~30 seconds)

**GPS Tests** (may take time for fix):
- Initialization succeeds
- Position bounds are valid
- After ~30 seconds (outdoor): fix_quality = 1, satellites > 4

**Combined Tests**:
- JSON and CSV output formats are correct
- Both sensor data appear in combined output
- Output at correct frequencies (BNO @ 10Hz, GPS @ 1Hz)

## Troubleshooting

### Test Failure: "BNO085_InitializationBegins - FAILED"

**Possible Causes**:
1. BNO085 not connected or powered
2. Serial1 not available on board
3. BNO085 firmware issue

**Solution**:
```bash
# Check serial connection
ls /dev/ttyACM*
# Or open serial monitor to verify data arriving
platformio device monitor -e arduino_mega
```

### Test Failure: "NEO_M9N_PositionBoundsValid - FAILED"

**Possible Causes**:
1. GPS returning invalid coordinates (no fix yet)
2. NMEA parser error

**Solution**:
```bash
# Check GPS status
# Look for "Fix Quality: 0" (no fix yet)
# Wait 30+ seconds outdoors for satellite acquisition
```

### Test Failure: "QuaternionMagnitudeValid - FAILED (mag = 2.45)"

**Possible Causes**:
1. Quaternion data not normalized
2. BNO085 library version mismatch
3. Sensor returning raw data instead of quaternion

**Solution**:
1. Verify BNO085 library version
   ```bash
   platformio lib list
   ```
2. Check that SH2_ROTATION_VECTOR report is enabled
3. Verify quaternion normalization in code

## Data Format Reference

### Quaternion (Orientation)

**Structure**:
```cpp
struct OrientationData {
  float w, x, y, z;           // Quaternion (normalized)
  uint8_t cal_status;         // Overall calibration (0-3)
  uint8_t cal_accel;          // Accelerometer cal (0-3)
  uint8_t cal_gyro;           // Gyroscope cal (0-3)
  uint8_t cal_mag;            // Magnetometer cal (0-3)
  uint32_t timestamp_ms;      // Milliseconds since boot
};
```

**Normalization**:
- magnitude = √(w² + x² + y² + z²) ≈ 1.0
- Used for Euler angle conversion, matrix rotation, etc.

### Position (GPS)

**Structure**:
```cpp
struct PositionData {
  double latitude;            // Degrees (-90 to 90)
  double longitude;           // Degrees (-180 to 180)
  float altitude;             // Meters (WGS84 ellipsoid)
  float accuracy_m;           // CEP in meters
  uint8_t num_satellites;     // Satellites used in fix
  uint8_t fix_quality;        // 0=none, 1=GPS, 2=DGPS, etc.
  uint32_t timestamp_ms;      // Milliseconds since boot
};
```

**Fix Quality Meanings**:
- 0: Invalid / No fix
- 1: GPS (SPS)
- 2: DGPS (Differential GPS)
- 3: PPS (Precise Positioning Service)
- 4: Real Time Kinematic (RTK)
- 5: RTK-Float
- 6: Estimated
- 7: Manual
- 8: Simulation

### JSON Output Format

```json
{
  "timestamp": 12345,
  "orientation": {
    "w": 0.7071,
    "x": 0.0,
    "y": 0.7071,
    "z": 0.0,
    "magnitude": 1.0,
    "calibration": {
      "status": 3,
      "accel": 3,
      "gyro": 3,
      "mag": 3
    }
  },
  "position": {
    "latitude": 37.7749,
    "longitude": -122.4194,
    "altitude": 10.5,
    "accuracy_m": 2.5,
    "satellites": 12,
    "fix_quality": 1
  }
}
```

### CSV Output Format

**Header**:
```
timestamp_ms,quat_w,quat_x,quat_y,quat_z,magnitude,cal_status,cal_accel,cal_gyro,cal_mag,gps_lat,gps_lon,gps_alt,gps_accuracy,gps_sats,gps_fix
```

**Data Row**:
```
12345,0.7071,0.0,0.7071,0.0,1.0,3,3,3,3,37.7749,-122.4194,10.5,2.5,12,1
```

## Next Steps

### After Successful Tests

1. **Deploy to Hardware**: Transfer to target device
2. **Validate Real-Time Performance**: Check output frequency and timing
3. **Calibrate Sensors**: Run BNO085 calibration procedure
4. **Log Data**: Collect baseline data over 24 hours
5. **Monitor Temperature**: Verify performance in various conditions

### Continuous Integration

To integrate tests into CI/CD pipeline:

```yaml
# .github/workflows/test.yml
name: Test
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - uses: actions/setup-python@v2
      - run: pip install platformio
      - run: cd auto_orientation && platformio test
```

## References

- [Adafruit BNO08x Library](https://github.com/adafruit/Adafruit_BNO08x_Arduino)
- [u-blox NEO-M9N Documentation](https://www.u-blox.com/en/product/neo-m9n-module)
- [Google Test Framework](https://github.com/google/googletest)
- [PlatformIO Testing](https://docs.platformio.org/en/latest/advanced/unit-testing/index.html)
- [NMEA 0183 Standard](https://en.wikipedia.org/wiki/NMEA_0183)
- [Quaternion Mathematics](https://en.wikipedia.org/wiki/Quaternion)

## Test Maintenance

### Adding New Tests

To add a test to the suite:

```cpp
TEST_F(IntegrationTestFixture, NewTestName) {
  // Arrange
  OrientationData q = test_orientation;

  // Act
  float mag = QuaternionMagnitude(q);

  // Assert
  EXPECT_NEAR(mag, 1.0f, 0.01f) << "Descriptive error message";
}
```

### Updating Baseline Values

If sensor specifications change:

1. Update constants in test setup
2. Adjust tolerance values in assertions
3. Document changes in test comments
4. Re-run all tests

### Performance Profiling

To measure test execution time:

```bash
# Verbose output with timing
platformio test -e arduino_mega -vv

# Profile specific test
platformio test -e arduino_mega -- --gtest_filter="*Quaternion*"
```

## Support

For questions or issues:

1. Check sensor documentation
2. Review test comments and docstrings
3. Verify hardware connections
4. Check serial monitor output
5. Compare against expected data formats above
