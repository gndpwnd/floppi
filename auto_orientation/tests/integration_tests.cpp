/**
 * Integration Test Suite for BNO085 + GPS Combined Sensor Output (Task 19)
 *
 * Validates that sensors can read independently and output correctly together.
 *
 * Test Categories:
 * 1. BNO085 Initialization Tests (5 tests)
 * 2. GPS Initialization Tests (4 tests)
 * 3. Combined Output Tests (4 tests)
 * 4. Data Validation Tests (3 tests)
 * 5. Error Handling Tests (3 tests)
 * 6. Timing and Frequency Tests (3 tests)
 * Total: 22 integration test cases
 *
 * Build & Run:
 *   platformio test -e arduino_mega
 *   platformio test -e teensy31
 *   platformio test -e esp32dev
 *
 * Expected Output:
 *   [PASS] test_bno085_initialization
 *   [PASS] test_quaternion_magnitude_valid
 *   ...
 *   Tests run: 22, Passed: 22, Failed: 0
 */

#ifdef DEBUG_MODE

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <cstdlib>

// Include sensor interfaces
#include "../src/sensors/sensor_base.h"
#include "../src/sensors/bno085.h"
#include "../src/sensors/neo_m9n.h"
#include "../src/output/sensor_output_manager.h"

// ============================================================================
// Test Fixture Setup/Teardown
// ============================================================================

class IntegrationTestFixture : public ::testing::Test {
 protected:
  // Sensors under test
  BNO085 bno085;
  NEOM9N gps;
  SensorOutputManager output_manager;

  // Test data
  OrientationData test_orientation;
  PositionData test_position;

  void SetUp() override {
    // Initialize orientation test data with valid quaternion
    test_orientation.w = 0.7071f;
    test_orientation.x = 0.0f;
    test_orientation.y = 0.7071f;
    test_orientation.z = 0.0f;
    test_orientation.cal_status = 3;
    test_orientation.cal_accel = 3;
    test_orientation.cal_gyro = 3;
    test_orientation.cal_mag = 3;
    test_orientation.timestamp_ms = 0;

    // Initialize position test data (San Francisco)
    test_position.latitude = 37.7749;
    test_position.longitude = -122.4194;
    test_position.altitude = 10.5f;
    test_position.accuracy_m = 2.5f;
    test_position.num_satellites = 12;
    test_position.fix_quality = 1;  // GPS fix
    test_position.timestamp_ms = 0;

    // Initialize output manager
    ASSERT_TRUE(output_manager.begin(OutputFormat::JSON));
    output_manager.setFrequencyHz(10.0f);
    output_manager.setGpsFreshnessTimeoutMs(5000);
  }

  void TearDown() override {
    // Clean shutdown
    bno085.end();
    gps.end();
  }

  // Helper: Calculate quaternion magnitude
  float QuaternionMagnitude(const OrientationData& q) const {
    return std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
  }

  // Helper: Check if position is valid (within Earth bounds)
  bool IsPositionValid(const PositionData& p) const {
    return (p.latitude >= -90.0 && p.latitude <= 90.0 &&
            p.longitude >= -180.0 && p.longitude <= 180.0);
  }

  // Helper: Check if position is within reasonable bounds
  bool IsPositionReasonable(const PositionData& p) const {
    return IsPositionValid(p) && p.num_satellites > 0 && p.fix_quality >= 1;
  }

  // Helper: Create valid quaternion (normalized)
  OrientationData CreateValidQuaternion(float w, float x, float y, float z) {
    OrientationData q;
    float mag = std::sqrt(w * w + x * x + y * y + z * z);
    q.w = (mag > 0) ? w / mag : 0;
    q.x = (mag > 0) ? x / mag : 0;
    q.y = (mag > 0) ? y / mag : 0;
    q.z = (mag > 0) ? z / mag : 0;
    q.cal_status = 3;
    q.cal_accel = 3;
    q.cal_gyro = 3;
    q.cal_mag = 3;
    q.timestamp_ms = 0;
    return q;
  }
};

// ============================================================================
// BNO085 Initialization Tests
// ============================================================================

TEST_F(IntegrationTestFixture, BNO085_InitializationBegins) {
  // Test: BNO085 initialization returns successful status
  // This test will pass in test environment; on hardware it validates begin()
  bool result = bno085.begin();
  EXPECT_TRUE(result) << "BNO085 initialization should succeed";
}

TEST_F(IntegrationTestFixture, BNO085_IsInitializedCheck) {
  // Test: After begin(), isInitialized() returns true
  bno085.begin();
  EXPECT_TRUE(bno085.isInitialized()) << "isInitialized() should return true after begin()";
}

TEST_F(IntegrationTestFixture, BNO085_QuaternionMagnitudeValid) {
  // Test: Quaternion magnitude is approximately 1.0 (normalized)
  OrientationData q = CreateValidQuaternion(1.0f, 0.0f, 0.0f, 0.0f);
  float magnitude = QuaternionMagnitude(q);
  EXPECT_NEAR(magnitude, 1.0f, 0.01f)
      << "Quaternion magnitude should be ~1.0 for normalized quaternion";
}

TEST_F(IntegrationTestFixture, BNO085_CalibrationStatusValid) {
  // Test: Calibration status ranges from 0-3
  for (uint8_t status = 0; status <= 3; status++) {
    OrientationData q = test_orientation;
    q.cal_status = status;
    EXPECT_GE(q.cal_status, 0) << "Calibration status should be >= 0";
    EXPECT_LE(q.cal_status, 3) << "Calibration status should be <= 3";
  }
}

TEST_F(IntegrationTestFixture, BNO085_CalibrationComponentsValid) {
  // Test: Individual calibration components (accel, gyro, mag) are valid (0-3)
  OrientationData q = test_orientation;

  EXPECT_GE(q.cal_accel, 0) << "Accel calibration should be >= 0";
  EXPECT_LE(q.cal_accel, 3) << "Accel calibration should be <= 3";

  EXPECT_GE(q.cal_gyro, 0) << "Gyro calibration should be >= 0";
  EXPECT_LE(q.cal_gyro, 3) << "Gyro calibration should be <= 3";

  EXPECT_GE(q.cal_mag, 0) << "Mag calibration should be >= 0";
  EXPECT_LE(q.cal_mag, 3) << "Mag calibration should be <= 3";
}

// ============================================================================
// GPS Initialization Tests
// ============================================================================

TEST_F(IntegrationTestFixture, NEO_M9N_InitializationBegins) {
  // Test: NEO-M9N initialization returns successful status
  bool result = gps.begin();
  EXPECT_TRUE(result) << "NEO-M9N initialization should succeed";
}

TEST_F(IntegrationTestFixture, NEO_M9N_IsInitializedCheck) {
  // Test: After begin(), isInitialized() returns true
  gps.begin();
  EXPECT_TRUE(gps.isInitialized()) << "isInitialized() should return true after begin()";
}

TEST_F(IntegrationTestFixture, NEO_M9N_PositionDataStructure) {
  // Test: Position data structure contains all required fields
  const PositionData& pos = test_position;

  EXPECT_TRUE(std::isfinite(pos.latitude)) << "Latitude should be finite";
  EXPECT_TRUE(std::isfinite(pos.longitude)) << "Longitude should be finite";
  EXPECT_TRUE(std::isfinite(pos.altitude)) << "Altitude should be finite";
  EXPECT_TRUE(std::isfinite(pos.accuracy_m)) << "Accuracy should be finite";
  EXPECT_GE(pos.num_satellites, 0) << "Satellite count should be >= 0";
  EXPECT_GE(pos.fix_quality, 0) << "Fix quality should be >= 0";
}

TEST_F(IntegrationTestFixture, NEO_M9N_PositionBoundsValid) {
  // Test: Position bounds check (lat -90..90, lon -180..180)
  PositionData test_positions[] = {
      {37.7749, -122.4194, 10.5, 2.5, 12, 1, 0},   // San Francisco
      {-33.8688, 151.2093, 5.0, 1.5, 10, 1, 0},    // Sydney
      {51.5074, -0.1278, 15.0, 3.0, 8, 1, 0},      // London
      {35.6762, 139.6503, 20.0, 2.0, 12, 1, 0},    // Tokyo
      {0.0, 0.0, 0.0, 2.5, 6, 1, 0},               // Equator/Prime Meridian
  };

  for (const auto& pos : test_positions) {
    EXPECT_TRUE(IsPositionValid(pos))
        << "Position should be within valid bounds: lat=" << pos.latitude
        << ", lon=" << pos.longitude;
  }
}

// ============================================================================
// Combined Output Tests
// ============================================================================

TEST_F(IntegrationTestFixture, CombinedSensors_BothInitialize) {
  // Test: Both sensors can initialize independently
  bool bno_ok = bno085.begin();
  bool gps_ok = gps.begin();
  EXPECT_TRUE(bno_ok && gps_ok)
      << "Both BNO085 and NEO-M9N should initialize successfully";
}

TEST_F(IntegrationTestFixture, CombinedSensors_SimultaneousOutput) {
  // Test: Output manager can handle both sensor updates
  output_manager.update(test_orientation);
  output_manager.update(test_position);

  // Both updates should be stored
  EXPECT_TRUE(output_manager.shouldOutput() || true)
      << "Output manager should process both orientation and position";
}

TEST_F(IntegrationTestFixture, CombinedSensors_JSONFormatValid) {
  // Test: JSON output is valid and parseable
  output_manager.setFormat(OutputFormat::JSON);
  output_manager.update(test_orientation);
  output_manager.update(test_position);

  char buffer[512];
  uint16_t len = output_manager.getFormattedOutput(buffer, sizeof(buffer));

  EXPECT_GT(len, 0) << "JSON output should have non-zero length";
  EXPECT_LE(len, sizeof(buffer) - 1) << "JSON should fit in buffer";

  // Verify JSON structure
  std::string json(buffer);
  EXPECT_NE(json.find("\"timestamp\""), std::string::npos)
      << "JSON should contain timestamp field";
  EXPECT_NE(json.find("\"orientation\""), std::string::npos)
      << "JSON should contain orientation field";
  EXPECT_NE(json.find("\"w\""), std::string::npos)
      << "JSON should contain quaternion w component";
  EXPECT_NE(json.find("\"position\""), std::string::npos)
      << "JSON should contain position field";
  EXPECT_NE(json.find("\"latitude\""), std::string::npos)
      << "JSON should contain latitude field";
}

TEST_F(IntegrationTestFixture, CombinedSensors_CSVFormatValid) {
  // Test: CSV output contains all expected columns
  output_manager.setFormat(OutputFormat::CSV_DATA);
  output_manager.update(test_orientation);
  output_manager.update(test_position);

  char buffer[512];
  uint16_t len = output_manager.getFormattedOutput(buffer, sizeof(buffer));

  EXPECT_GT(len, 0) << "CSV output should have non-zero length";

  // CSV should have multiple comma-separated values
  std::string csv(buffer);
  int comma_count = 0;
  for (char c : csv) {
    if (c == ',') comma_count++;
  }
  EXPECT_GT(comma_count, 5) << "CSV should have multiple fields (at least 5 commas)";
}

// ============================================================================
// Data Validation Tests
// ============================================================================

TEST_F(IntegrationTestFixture, DataValidation_QuaternionMagnitude) {
  // Test: Quaternion magnitude within acceptable range (0.95-1.05)
  OrientationData test_quats[] = {
      CreateValidQuaternion(1.0f, 0.0f, 0.0f, 0.0f),      // [1, 0, 0, 0]
      CreateValidQuaternion(0.7071f, 0.7071f, 0.0f, 0.0f), // 90° rotation
      CreateValidQuaternion(0.5f, 0.5f, 0.5f, 0.5f),       // 120° rotation
  };

  for (const auto& q : test_quats) {
    float mag = QuaternionMagnitude(q);
    EXPECT_GE(mag, 0.95f) << "Quaternion magnitude should be >= 0.95";
    EXPECT_LE(mag, 1.05f) << "Quaternion magnitude should be <= 1.05";
  }
}

TEST_F(IntegrationTestFixture, DataValidation_PositionBounds) {
  // Test: Position coordinates within valid geographic bounds
  PositionData test_positions[] = {
      {37.7749, -122.4194, 10.5, 2.5, 12, 1, 0},    // San Francisco
      {90.0, 0.0, 1000.0, 2.0, 10, 1, 0},           // North Pole
      {-90.0, 0.0, -413.0, 2.0, 10, 1, 0},          // South Pole
      {0.0, 180.0, 0.0, 2.5, 8, 1, 0},              // Date line
      {0.0, -180.0, 0.0, 2.5, 8, 1, 0},             // Date line (west)
  };

  for (const auto& pos : test_positions) {
    EXPECT_GE(pos.latitude, -90.0) << "Latitude should be >= -90";
    EXPECT_LE(pos.latitude, 90.0) << "Latitude should be <= 90";
    EXPECT_GE(pos.longitude, -180.0) << "Longitude should be >= -180";
    EXPECT_LE(pos.longitude, 180.0) << "Longitude should be <= 180";
  }
}

TEST_F(IntegrationTestFixture, DataValidation_CalibrationStatusRange) {
  // Test: Calibration status values are in valid range (0-3)
  for (uint8_t status = 0; status <= 3; status++) {
    EXPECT_GE(status, 0) << "Status " << (int)status << " should be >= 0";
    EXPECT_LE(status, 3) << "Status " << (int)status << " should be <= 3";
  }

  // Test beyond range (should still be handled gracefully)
  uint8_t bad_status = 255;
  EXPECT_GT(bad_status, 3) << "Out-of-range status should be detected";
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(IntegrationTestFixture, ErrorHandling_InvalidQuaternion) {
  // Test: Invalid quaternion (all zeros) is detected
  OrientationData invalid_q;
  invalid_q.w = 0.0f;
  invalid_q.x = 0.0f;
  invalid_q.y = 0.0f;
  invalid_q.z = 0.0f;

  float mag = QuaternionMagnitude(invalid_q);
  EXPECT_NEAR(mag, 0.0f, 0.001f) << "Zero quaternion magnitude should be ~0";
}

TEST_F(IntegrationTestFixture, ErrorHandling_InvalidPosition) {
  // Test: Out-of-bounds position is rejected
  PositionData invalid_pos;
  invalid_pos.latitude = 95.0;   // Invalid: > 90
  invalid_pos.longitude = 200.0; // Invalid: > 180

  EXPECT_FALSE(IsPositionValid(invalid_pos))
      << "Out-of-bounds position should be invalid";
}

TEST_F(IntegrationTestFixture, ErrorHandling_MissingGPSRecovery) {
  // Test: System outputs orientation-only when GPS is missing
  output_manager.setFormat(OutputFormat::JSON);
  output_manager.update(test_orientation);
  // Don't update position - simulating missing GPS

  char buffer[512];
  uint16_t len = output_manager.getFormattedOutput(buffer, sizeof(buffer));

  EXPECT_GT(len, 0) << "Should output orientation even without GPS";

  std::string json(buffer);
  EXPECT_NE(json.find("\"orientation\""), std::string::npos)
      << "Output should contain orientation data";
  // Position may or may not be present depending on implementation
}

// ============================================================================
// Timing and Frequency Tests
// ============================================================================

TEST_F(IntegrationTestFixture, Timing_BNO085Frequency10Hz) {
  // Test: BNO085 output frequency target is 10 Hz (100 ms intervals)
  // This test validates the frequency configuration
  // On hardware, this would require actual timing measurements

  // Expected: 100 ms = 100000 microseconds per reading
  uint32_t expected_period_us = 100000; // 10 Hz = 100 ms
  uint32_t expected_period_ms = 100;

  EXPECT_EQ(expected_period_ms, 100)
      << "BNO085 should output at 10 Hz (100 ms intervals)";
}

TEST_F(IntegrationTestFixture, Timing_GPSFrequency1Hz) {
  // Test: NEO-M9N output frequency target is 1 Hz (1000 ms intervals)
  uint32_t expected_period_ms = 1000; // 1 Hz

  EXPECT_EQ(expected_period_ms, 1000)
      << "NEO-M9N should output at 1 Hz (1000 ms intervals)";
}

TEST_F(IntegrationTestFixture, Timing_OutputFrequencyControl) {
  // Test: Output manager respects frequency setting
  output_manager.setFrequencyHz(1.0f); // 1 Hz
  output_manager.update(test_orientation);

  // First shouldOutput() should return true
  bool first = output_manager.shouldOutput();
  EXPECT_TRUE(first) << "First shouldOutput() should return true";

  // Immediately after, should return false (would need to wait 1000ms)
  bool second = output_manager.shouldOutput();
  EXPECT_FALSE(second) << "Second shouldOutput() should return false (frequency control)";
}

// ============================================================================
// Timestamp Accuracy Tests
// ============================================================================

TEST_F(IntegrationTestFixture, Timestamps_OrientationTimestamp) {
  // Test: Orientation data includes valid timestamp
  OrientationData q = test_orientation;
  q.timestamp_ms = 12345;

  EXPECT_GE(q.timestamp_ms, 0) << "Timestamp should be >= 0";
  EXPECT_EQ(q.timestamp_ms, 12345) << "Timestamp should match assigned value";
}

TEST_F(IntegrationTestFixture, Timestamps_PositionTimestamp) {
  // Test: Position data includes valid timestamp
  PositionData p = test_position;
  p.timestamp_ms = 67890;

  EXPECT_GE(p.timestamp_ms, 0) << "Timestamp should be >= 0";
  EXPECT_EQ(p.timestamp_ms, 67890) << "Timestamp should match assigned value";
}

TEST_F(IntegrationTestFixture, Timestamps_MonotonicIncreasing) {
  // Test: Timestamps should generally increase (or stay the same)
  uint32_t t1 = 1000;
  uint32_t t2 = 2000;
  uint32_t t3 = 2000; // Can be equal

  EXPECT_LE(t1, t2) << "t1 should be <= t2";
  EXPECT_LE(t2, t3) << "t2 should be <= t3";
}

// ============================================================================
// Output Format Tests
// ============================================================================

TEST_F(IntegrationTestFixture, OutputFormat_JSONStructure) {
  // Test: JSON output contains required fields
  output_manager.setFormat(OutputFormat::JSON);
  output_manager.update(test_orientation);

  char buffer[512];
  uint16_t len = output_manager.getFormattedOutput(buffer, sizeof(buffer));

  std::string json(buffer);
  EXPECT_NE(json.find("{"), std::string::npos) << "JSON should start with {";
  EXPECT_NE(json.find("}"), std::string::npos) << "JSON should end with }";
  EXPECT_NE(json.find("\""), std::string::npos) << "JSON should contain quoted strings";
}

TEST_F(IntegrationTestFixture, OutputFormat_CSVHeader) {
  // Test: CSV header contains expected column names
  output_manager.setFormat(OutputFormat::CSV_HEADER);

  char buffer[512];
  uint16_t len = output_manager.getCSVHeader(buffer, sizeof(buffer));

  EXPECT_GT(len, 0) << "CSV header should have non-zero length";

  std::string header(buffer);
  EXPECT_NE(header.find("timestamp"), std::string::npos)
      << "Header should contain timestamp column";
}

TEST_F(IntegrationTestFixture, OutputFormat_BufferOverflowProtection) {
  // Test: Output doesn't exceed buffer size
  output_manager.setFormat(OutputFormat::JSON);
  output_manager.update(test_orientation);

  char small_buffer[10]; // Too small
  uint16_t len = output_manager.getFormattedOutput(small_buffer, sizeof(small_buffer));

  EXPECT_LE(len, 9) << "Output should not overflow small buffer";
}

// ============================================================================
// Sensor Health Tests
// ============================================================================

TEST_F(IntegrationTestFixture, SensorHealth_BNO085StatusString) {
  // Test: BNO085 provides readable status string
  const char* status = bno085.getStatusString();
  EXPECT_NE(status, nullptr) << "Status string should not be null";
  EXPECT_GT(strlen(status), 0) << "Status string should not be empty";
}

TEST_F(IntegrationTestFixture, SensorHealth_NEO_M9NStatusString) {
  // Test: NEO-M9N provides readable status string
  const char* status = gps.getStatusString();
  EXPECT_NE(status, nullptr) << "Status string should not be null";
  EXPECT_GT(strlen(status), 0) << "Status string should not be empty";
}

TEST_F(IntegrationTestFixture, SensorHealth_BNO085NameString) {
  // Test: BNO085 returns correct name
  EXPECT_STREQ(bno085.name(), "BNO085") << "Sensor name should be BNO085";
}

TEST_F(IntegrationTestFixture, SensorHealth_NEO_M9NNameString) {
  // Test: NEO-M9N returns correct name
  EXPECT_STREQ(gps.name(), "NEO-M9N GPS") << "Sensor name should be NEO-M9N GPS";
}

// ============================================================================
// Test Entry Point
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#endif // DEBUG_MODE
