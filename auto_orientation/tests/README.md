# Integration Tests

This directory contains the integration test suite for the auto_orientation sensor system.

**IMPORTANT:** These tests are only compiled when the `DEBUG_MODE` preprocessor flag is defined. Use `-D DEBUG_MODE` when building tests for debug/development. In production builds, test code is completely excluded at preprocessor time.

## Files

- **integration_tests.cpp** - Main integration test suite (Task 19)
  - 24 comprehensive test cases
  - Tests BNO085 + GPS combined sensor output
  - Validates initialization, data quality, combined output, error handling
  - Uses Google Test framework

- **test_sensor_output_manager.cpp** - Unit tests for output formatting
  - Tests JSON/CSV formatting
  - Tests frequency control
  - Tests data freshness handling

- **test_bno085_calibration_sh2.ino** - Hardware test sketch for BNO085
  - Manual testing on actual hardware
  - Demonstrates calibration procedure

## Quick Start

### Build with DEBUG_MODE

All tests require the `DEBUG_MODE` preprocessor flag to be defined. Update your `platformio.ini` test environment or build with:

```bash
# Build tests with DEBUG_MODE enabled
platformio test -e arduino_mega -D DEBUG_MODE
```

Alternatively, add to your `platformio.ini`:

```ini
[env:test_arduino_mega]
platform = atmelavr
board = megaatmega2560
build_flags = -D DEBUG_MODE
test_framework = googletest
```

### Run All Tests

```bash
cd /home/devel/floppi/auto_orientation
platformio test -e arduino_mega -D DEBUG_MODE
```

### Run Integration Tests Only

```bash
platformio test -e arduino_mega -- --gtest_filter="*Integration*"
```

### Run Specific Test Category

```bash
# BNO085 tests
platformio test -e arduino_mega -- --gtest_filter="*BNO085*"

# GPS tests
platformio test -e arduino_mega -- --gtest_filter="*NEO_M9N*"

# Combined output tests
platformio test -e arduino_mega -- --gtest_filter="*CombinedSensors*"

# Data validation tests
platformio test -e arduino_mega -- --gtest_filter="*DataValidation*"
```

## Test Coverage

### integration_tests.cpp (24 tests)

**BNO085 Initialization (5 tests)**
- Initialization begins successfully
- isInitialized() returns true
- Quaternion magnitude is valid (≈1.0)
- Calibration status in range 0-3
- Calibration components valid

**GPS Initialization (4 tests)**
- Initialization begins successfully
- isInitialized() returns true
- Position data structure is complete
- Position bounds are valid (lat -90..90, lon -180..180)

**Combined Output (4 tests)**
- Both sensors initialize independently
- Both sensors can output simultaneously
- JSON format is valid
- CSV format is valid

**Data Validation (3 tests)**
- Quaternion magnitude check (0.95-1.05)
- Position bounds check
- Calibration status ranges

**Error Handling (3 tests)**
- Invalid quaternion detection
- Invalid position detection
- Missing GPS recovery (orientation-only output)

**Timing & Frequency (3 tests)**
- BNO085 frequency is 10 Hz
- GPS frequency is 1 Hz
- Output frequency control works

**Output Formatting (3 tests)**
- JSON structure validation
- CSV header validation
- Buffer overflow protection

**Sensor Health (3 tests)**
- BNO085 status string
- GPS status string
- Sensor identification strings

## Expected Output

Successful test run:

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

## Platform-Specific Notes

### Arduino Mega (Default)

```bash
platformio test -e arduino_mega
```

- Uses Serial1 for BNO085
- Uses Serial3 for GPS
- Full hardware UART support

### Teensy 3.x

```bash
platformio test -e teensy31
```

- Uses Serial1 for BNO085
- Uses Serial2 for GPS
- Higher clock speed

### ESP32

```bash
platformio test -e esp32dev
```

- Uses Serial1 for both (with pin mapping)
- Note: May need to adjust pins in config/pins.h

## Debugging Failed Tests

### Enable Verbose Output

```bash
platformio test -e arduino_mega -vv
```

### Check Serial Monitor

```bash
platformio device monitor -e arduino_mega
```

### Run Single Test with Full Output

```bash
platformio test -e arduino_mega -- --gtest_filter="BNO085_QuaternionMagnitude*" --gtest_print_time=all
```

### Common Issues

**Test: "BNO085_InitializationBegins - FAILED"**
- Sensor not connected
- Baud rate mismatch
- Serial1 not available on board

**Test: "NEO_M9N_PositionBoundsValid - FAILED"**
- GPS not acquiring satellites (need outdoor location)
- NMEA parser error
- Baud rate mismatch

**Test: "QuaternionMagnitudeValid - FAILED"**
- Quaternion not normalized
- Wrong report type enabled
- Library version issue

See [integration_test_guide.md](../docs/integration_test_guide.md) for detailed troubleshooting.

## Adding Tests

To add a new test to the suite:

```cpp
TEST_F(IntegrationTestFixture, MyNewTest) {
  // Arrange
  OrientationData q = test_orientation;

  // Act
  float mag = QuaternionMagnitude(q);

  // Assert
  EXPECT_NEAR(mag, 1.0f, 0.01f) << "Descriptive error message";
}
```

## Documentation

Comprehensive test documentation available in:
- [integration_test_guide.md](../docs/integration_test_guide.md) - Full test guide with examples
- [integration_tests.cpp](integration_tests.cpp) - Test source with inline comments

## References

- [Google Test Documentation](https://google.github.io/googletest/)
- [PlatformIO Testing](https://docs.platformio.org/en/latest/advanced/unit-testing/)
- [Adafruit BNO08x Library](https://github.com/adafruit/Adafruit_BNO08x_Arduino)
- [u-blox NEO-M9N Specs](https://www.u-blox.com/en/product/neo-m9n-module)
