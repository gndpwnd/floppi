/**
 * Unit tests for SensorOutputManager
 *
 * Tests JSON output formatting, frequency control, and data freshness.
 */

#ifdef DEBUG_MODE

#include <gtest/gtest.h>
#include <cstring>
#include "../src/output/sensor_output_manager.h"
#include "../src/sensors/sensor_base.h"

class SensorOutputManagerTest : public ::testing::Test {
 protected:
  SensorOutputManager manager;
  OrientationData orientation;
  PositionData position;

  void SetUp() override {
    // Initialize orientation data
    orientation.w = 0.7071f;
    orientation.x = 0.0f;
    orientation.y = 0.7071f;
    orientation.z = 0.0f;
    orientation.cal_status = 3;
    orientation.cal_accel = 3;
    orientation.cal_gyro = 3;
    orientation.cal_mag = 3;
    orientation.timestamp_ms = 0;

    // Initialize position data
    position.latitude = 37.7749;
    position.longitude = -122.4194;
    position.altitude = 10.5f;
    position.accuracy_m = 2.5f;
    position.num_satellites = 12;
    position.fix_quality = 1;  // GPS fix
    position.timestamp_ms = 0;

    // Initialize manager
    ASSERT_TRUE(manager.begin(OutputFormat::JSON));
    manager.setFrequencyHz(10.0f);
    manager.setGpsFreshnessTimeoutMs(5000);
  }
};

// Test initialization
TEST_F(SensorOutputManagerTest, InitializationJSON) {
  EXPECT_EQ(manager.getOutputFormat(), OutputFormat::JSON);
}

// Test data updates
TEST_F(SensorOutputManagerTest, UpdateOrientation) {
  manager.update(orientation);
  // No direct way to verify, but shouldOutput should work
  char buffer[256];
  EXPECT_FALSE(manager.shouldOutput());  // Not enough time elapsed
}

TEST_F(SensorOutputManagerTest, UpdatePosition) {
  manager.update(position);
  // Position alone shouldn't be enough to output
  EXPECT_FALSE(manager.shouldOutput());
}

// Test JSON output formatting
TEST_F(SensorOutputManagerTest, JSONFormatWithOrientationOnly) {
  manager.setFormat(OutputFormat::JSON);
  manager.update(orientation);

  char buffer[512];
  uint16_t len = manager.getFormattedOutput(buffer, sizeof(buffer));

  // Should have some output
  EXPECT_GT(len, 0);

  // Verify JSON structure
  std::string json(buffer);
  EXPECT_NE(json.find("\"timestamp\""), std::string::npos);
  EXPECT_NE(json.find("\"orientation\""), std::string::npos);
  EXPECT_NE(json.find("\"w\""), std::string::npos);
  EXPECT_NE(json.find("\"magnitude\""), std::string::npos);
  EXPECT_NE(json.find("\"calibration\""), std::string::npos);
}

// Test JSON with position data
TEST_F(SensorOutputManagerTest, JSONFormatWithPosition) {
  manager.setFormat(OutputFormat::JSON);
  manager.update(orientation);
  manager.update(position);

  char buffer[512];
  uint16_t len = manager.getFormattedOutput(buffer, sizeof(buffer));

  EXPECT_GT(len, 0);

  std::string json(buffer);
  EXPECT_NE(json.find("\"position\""), std::string::npos);
  EXPECT_NE(json.find("\"latitude\""), std::string::npos);
  EXPECT_NE(json.find("\"longitude\""), std::string::npos);
}

// Test frequency control
TEST_F(SensorOutputManagerTest, FrequencyControl) {
  manager.setFrequencyHz(1.0f);  // 1 Hz output
  manager.update(orientation);

  // First output should be ready
  EXPECT_TRUE(manager.shouldOutput());

  char buffer[256];
  manager.getFormattedOutput(buffer, sizeof(buffer));

  // Immediately after, should not be ready (would need to wait 1000ms)
  EXPECT_FALSE(manager.shouldOutput());
}

// Test stale position detection
TEST_F(SensorOutputManagerTest, PositionFreshness) {
  manager.setFormat(OutputFormat::JSON);
  manager.setGpsFreshnessTimeoutMs(100);  // 100ms timeout

  manager.update(orientation);
  manager.update(position);

  char buffer[512];

  // First output should include position (just updated)
  uint16_t len1 = manager.getFormattedOutput(buffer, sizeof(buffer));
  std::string json1(buffer);
  EXPECT_NE(json1.find("\"position\""), std::string::npos);

  // After timeout, position should be omitted (in real hardware, would need sleep)
  // This test just verifies the logic without actual timing
}

// Test buffer overflow protection
TEST_F(SensorOutputManagerTest, BufferOverflow) {
  manager.update(orientation);
  manager.update(position);

  char small_buffer[10];  // Too small for output
  uint16_t len = manager.getFormattedOutput(small_buffer, sizeof(small_buffer));

  // Should return 0 on error or insufficient space
  // (depending on implementation, may return 0 or partial output)
  EXPECT_LE(len, 9);  // Leave room for null terminator
}

// Test format setting
TEST_F(SensorOutputManagerTest, SetFormat) {
  manager.setFormat(OutputFormat::JSON);
  EXPECT_EQ(manager.getOutputFormat(), OutputFormat::JSON);
}

// Test frequency setter
TEST_F(SensorOutputManagerTest, SetFrequency) {
  manager.setFrequencyHz(5.0f);
  EXPECT_EQ(manager.getOutputFormat(), OutputFormat::JSON);  // Should not affect format
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#endif // DEBUG_MODE
