/**
 * Test Suite: JSON Output Integration (Phase 2)
 *
 * Tests the merged JSON output format with orientation + position data.
 * Validates JSON structure, field types, and edge cases.
 *
 * Compile: g++ -D DEBUG_MODE test_json_output.cpp -I.. -o test_json_output
 * Run: ./test_json_output
 */

#ifdef DEBUG_MODE

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "../src/sensors/sensor_base.h"
#include "../src/output/sensor_output_manager.h"
#include "test_helpers.h"

// ============================================================================
// Test Utilities
// ============================================================================

int test_count = 0;
int test_passed = 0;

void test_begin(const char* name) {
  test_count++;
  printf("\n[Test %d] %s...\n", test_count, name);
}

void test_pass() {
  printf("  ✓ PASS\n");
  test_passed++;
}

void test_fail(const char* msg) {
  printf("  ✗ FAIL: %s\n", msg);
}

// Helper: Check if string contains substring
bool contains(const char* haystack, const char* needle) {
  return strstr(haystack, needle) != NULL;
}

// Helper: Check if string starts with prefix
bool starts_with(const char* str, const char* prefix) {
  return strncmp(str, prefix, strlen(prefix)) == 0;
}

// Helper: Check if string ends with suffix
bool ends_with(const char* str, const char* suffix) {
  size_t str_len = strlen(str);
  size_t suffix_len = strlen(suffix);
  if (str_len < suffix_len) return false;
  return strcmp(str + str_len - suffix_len, suffix) == 0;
}

// Helper: Validate JSON structure (basic checks)
bool is_valid_json_structure(const char* json) {
  // Basic: starts with { and ends with }
  if (!starts_with(json, "{")) return false;
  if (!ends_with(json, "}")) return false;

  // Count braces match
  int brace_count = 0;
  for (size_t i = 0; json[i]; i++) {
    if (json[i] == '{') brace_count++;
    if (json[i] == '}') brace_count--;
    if (brace_count < 0) return false;  // Closing brace without opening
  }
  return brace_count == 0;  // All braces matched
}

// ============================================================================
// Test Cases
// ============================================================================

void test_orientation_only() {
  test_begin("JSON format with orientation only");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);  // Allow immediate output

  // Create test orientation data
  OrientationData orient = TestOrientations::NoRotation();
  orient.roll_deg = 45.0f;
  orient.pitch_deg = 30.0f;
  orient.yaw_deg = 60.0f;

  output.updateOrientation(orient);

  char buffer[512];
  uint16_t len = output.getFormattedOutput(buffer, sizeof(buffer));

  if (len == 0) {
    test_fail("Failed to format JSON");
    return;
  }

  // Validate JSON structure
  if (!is_valid_json_structure(buffer)) {
    printf("  JSON: %s\n", buffer);
    test_fail("Invalid JSON structure");
    return;
  }

  // Check required fields
  if (!contains(buffer, "\"timestamp\"")) {
    test_fail("Missing timestamp field");
    return;
  }
  if (!contains(buffer, "\"orientation_valid\":true")) {
    test_fail("Missing or false orientation_valid field");
    return;
  }
  if (!contains(buffer, "\"orientation\"")) {
    test_fail("Missing orientation section");
    return;
  }
  if (!contains(buffer, "\"w\":")) {
    test_fail("Missing w component in quaternion");
    return;
  }
  if (!contains(buffer, "\"euler\"")) {
    test_fail("Missing euler angles");
    return;
  }
  if (!contains(buffer, "\"calibration\"")) {
    test_fail("Missing calibration status");
    return;
  }

  printf("  JSON: %s\n", buffer);
  test_pass();
}

void test_position_only() {
  test_begin("JSON format with position only (using formatPositionJSON)");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  // Create test position data (San Francisco)
  PositionData pos = TestLocations::SanFrancisco();

  output.updatePosition(pos);

  // Note: getFormattedOutput() requires valid orientation (it's the primary sensor)
  // Use formatPositionJSON() for position-only output
  char buffer[512];
  uint16_t len = output.formatPositionJSON(buffer, sizeof(buffer));

  if (len == 0) {
    test_fail("Failed to format JSON");
    return;
  }

  // Validate JSON structure
  if (!is_valid_json_structure(buffer)) {
    printf("  JSON: %s\n", buffer);
    test_fail("Invalid JSON structure");
    return;
  }

  // Check fields
  if (!contains(buffer, "\"position_valid\":true")) {
    test_fail("Missing or false position_valid field");
    return;
  }
  if (!contains(buffer, "\"position\"")) {
    test_fail("Missing position section");
    return;
  }
  if (!contains(buffer, "\"latitude\"")) {
    test_fail("Missing latitude field");
    return;
  }
  if (!contains(buffer, "\"longitude\"")) {
    test_fail("Missing longitude field");
    return;
  }
  if (!contains(buffer, "\"altitude_m\"")) {
    test_fail("Missing altitude field");
    return;
  }

  printf("  JSON: %s\n", buffer);
  test_pass();
}

void test_combined_orientation_position() {
  test_begin("JSON format with both orientation and position");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  // Create test data
  OrientationData orient = TestOrientations::Roll90();
  PositionData pos = TestLocations::Sydney();

  output.updateOrientation(orient);
  output.updatePosition(pos);

  char buffer[512];
  uint16_t len = output.getFormattedOutput(buffer, sizeof(buffer));

  if (len == 0) {
    test_fail("Failed to format JSON");
    return;
  }

  // Validate structure
  if (!is_valid_json_structure(buffer)) {
    printf("  JSON: %s\n", buffer);
    test_fail("Invalid JSON structure");
    return;
  }

  // Check both sections present
  if (!contains(buffer, "\"orientation_valid\":true")) {
    test_fail("Missing or false orientation_valid");
    return;
  }
  if (!contains(buffer, "\"position_valid\":true")) {
    test_fail("Missing or false position_valid");
    return;
  }
  if (!contains(buffer, "\"orientation\"")) {
    test_fail("Missing orientation section");
    return;
  }
  if (!contains(buffer, "\"position\"")) {
    test_fail("Missing position section");
    return;
  }

  printf("  JSON: %s\n", buffer);
  test_pass();
}

void test_orientation_json_method() {
  test_begin("formatOrientationJSON() method");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);

  OrientationData orient = TestOrientations::Yaw90();
  output.updateOrientation(orient);

  char buffer[512];
  uint16_t len = output.formatOrientationJSON(buffer, sizeof(buffer));

  if (len == 0) {
    test_fail("formatOrientationJSON returned 0");
    return;
  }

  if (!is_valid_json_structure(buffer)) {
    printf("  JSON: %s\n", buffer);
    test_fail("Invalid JSON structure");
    return;
  }

  if (!contains(buffer, "\"orientation_valid\":true")) {
    test_fail("Missing orientation_valid flag");
    return;
  }

  printf("  JSON: %s\n", buffer);
  test_pass();
}

void test_position_json_method() {
  test_begin("formatPositionJSON() method");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);

  PositionData pos = TestLocations::Tokyo();
  output.updatePosition(pos);

  char buffer[512];
  uint16_t len = output.formatPositionJSON(buffer, sizeof(buffer));

  if (len == 0) {
    test_fail("formatPositionJSON returned 0");
    return;
  }

  if (!is_valid_json_structure(buffer)) {
    printf("  JSON: %s\n", buffer);
    test_fail("Invalid JSON structure");
    return;
  }

  if (!contains(buffer, "\"position_valid\":true")) {
    test_fail("Missing position_valid flag");
    return;
  }

  printf("  JSON: %s\n", buffer);
  test_pass();
}

void test_full_json_method() {
  test_begin("formatFullJSON() method");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);

  OrientationData orient = TestOrientations::Pitch90();
  PositionData pos = TestLocations::London();

  output.updateOrientation(orient);
  output.updatePosition(pos);

  char buffer[512];
  uint16_t len = output.formatFullJSON(buffer, sizeof(buffer));

  if (len == 0) {
    test_fail("formatFullJSON returned 0");
    return;
  }

  if (!is_valid_json_structure(buffer)) {
    printf("  JSON: %s\n", buffer);
    test_fail("Invalid JSON structure");
    return;
  }

  // Verify both sections
  if (!contains(buffer, "\"orientation_valid\":true")) {
    test_fail("Missing orientation_valid");
    return;
  }
  if (!contains(buffer, "\"position_valid\":true")) {
    test_fail("Missing position_valid");
    return;
  }

  printf("  JSON: %s\n", buffer);
  test_pass();
}

void test_invalid_position() {
  test_begin("JSON handling of invalid position (no fix)");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  OrientationData orient = TestOrientations::NoRotation();
  PositionData pos = TestLocations::SanFrancisco();
  pos.fix_quality = 0;  // No fix

  output.updateOrientation(orient);
  output.updatePosition(pos);

  char buffer[512];
  uint16_t len = output.getFormattedOutput(buffer, sizeof(buffer));

  if (len == 0) {
    test_fail("Failed to format JSON");
    return;
  }

  // Position should be marked invalid
  if (!contains(buffer, "\"position_valid\":false")) {
    printf("  JSON: %s\n", buffer);
    test_fail("Position should be marked invalid when fix_quality=0");
    return;
  }

  // Orientation should still be valid
  if (!contains(buffer, "\"orientation_valid\":true")) {
    test_fail("Orientation should be valid");
    return;
  }

  printf("  JSON: %s\n", buffer);
  test_pass();
}

void test_no_data_available() {
  test_begin("JSON handling when no sensor data available");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  // Don't update anything

  char buffer[512];
  uint16_t len = output.getFormattedOutput(buffer, sizeof(buffer));

  // Should return 0 because no orientation data
  if (len != 0) {
    printf("  Got: %s\n", buffer);
    test_fail("Should return 0 when no orientation data");
    return;
  }

  test_pass();
}

void test_frequency_control() {
  test_begin("Output frequency control");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(10.0f);  // 100 ms intervals

  OrientationData orient = TestOrientations::NoRotation();
  output.updateOrientation(orient);

  char buffer[512];

  // Note: shouldOutput() uses last_output_ms_ which starts at millis() from begin()
  // So first call may not be ready depending on timing. Give it a chance.
  bool first_ready = output.shouldOutput();
  if (first_ready) {
    output.getFormattedOutput(buffer, sizeof(buffer));
  }

  // Immediately call again - should NOT be ready
  if (output.shouldOutput()) {
    test_fail("Immediate second call should not be ready at 10 Hz");
    return;
  }

  test_pass();
}

void test_timestamp_present() {
  test_begin("Timestamp field presence and type");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  OrientationData orient = TestOrientations::NoRotation();
  output.updateOrientation(orient);

  char buffer[512];
  uint16_t len = output.getFormattedOutput(buffer, sizeof(buffer));

  if (!contains(buffer, "\"timestamp\":")) {
    test_fail("Missing timestamp field");
    return;
  }

  // Timestamp should be a number
  if (contains(buffer, "\"timestamp\":null")) {
    test_fail("Timestamp should not be null");
    return;
  }

  printf("  JSON: %s\n", buffer);
  test_pass();
}

void test_calibration_status() {
  test_begin("Calibration status fields in JSON");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  OrientationData orient = TestOrientations::PartiallyCalibrated();
  output.updateOrientation(orient);

  char buffer[512];
  uint16_t len = output.getFormattedOutput(buffer, sizeof(buffer));

  if (!contains(buffer, "\"system\":")) {
    test_fail("Missing system calibration status");
    return;
  }
  if (!contains(buffer, "\"accel\":")) {
    test_fail("Missing accel calibration status");
    return;
  }
  if (!contains(buffer, "\"gyro\":")) {
    test_fail("Missing gyro calibration status");
    return;
  }
  if (!contains(buffer, "\"mag\":")) {
    test_fail("Missing mag calibration status");
    return;
  }

  printf("  JSON: %s\n", buffer);
  test_pass();
}

void test_multiple_updates() {
  test_begin("Multiple sensor updates in sequence");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  // Update 1
  OrientationData orient1 = TestOrientations::NoRotation();
  PositionData pos1 = TestLocations::SanFrancisco();

  output.updateOrientation(orient1);
  output.updatePosition(pos1);

  char buffer[512];
  uint16_t len1 = output.getFormattedOutput(buffer, sizeof(buffer));

  if (len1 == 0) {
    test_fail("First output failed");
    return;
  }

  // Update 2 (different data)
  OrientationData orient2 = TestOrientations::Roll90();
  PositionData pos2 = TestLocations::Sydney();

  output.updateOrientation(orient2);
  output.updatePosition(pos2);

  // Wait for next output interval
  output.setFrequencyHz(1000.0f);  // Faster for testing

  uint16_t len2 = output.getFormattedOutput(buffer, sizeof(buffer));

  if (len2 == 0) {
    test_fail("Second output failed");
    return;
  }

  printf("  Output 1 len: %d, Output 2 len: %d\n", len1, len2);
  test_pass();
}

void test_buffer_overflow_protection() {
  test_begin("Buffer overflow protection");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  OrientationData orient = TestOrientations::NoRotation();
  PositionData pos = TestLocations::Tokyo();

  output.updateOrientation(orient);
  output.updatePosition(pos);

  // Try with very small buffer
  char small_buffer[10];
  uint16_t len = output.getFormattedOutput(small_buffer, sizeof(small_buffer));

  // Should return 0 (insufficient buffer)
  if (len != 0) {
    test_fail("Should return 0 for insufficient buffer");
    return;
  }

  test_pass();
}

void test_quaternion_components() {
  test_begin("Quaternion components in JSON");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  OrientationData orient = TestOrientations::Arbitrary();
  output.updateOrientation(orient);

  char buffer[512];
  uint16_t len = output.getFormattedOutput(buffer, sizeof(buffer));

  if (!contains(buffer, "\"w\":")) {
    test_fail("Missing w component");
    return;
  }
  if (!contains(buffer, "\"x\":")) {
    test_fail("Missing x component");
    return;
  }
  if (!contains(buffer, "\"y\":")) {
    test_fail("Missing y component");
    return;
  }
  if (!contains(buffer, "\"z\":")) {
    test_fail("Missing z component");
    return;
  }
  if (!contains(buffer, "\"magnitude\":")) {
    test_fail("Missing magnitude");
    return;
  }

  printf("  JSON: %s\n", buffer);
  test_pass();
}

void test_euler_angles() {
  test_begin("Euler angles in JSON");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  OrientationData orient = TestOrientations::NoRotation();
  orient.roll_deg = 15.5f;
  orient.pitch_deg = 25.3f;
  orient.yaw_deg = 45.2f;
  output.updateOrientation(orient);

  char buffer[512];
  uint16_t len = output.getFormattedOutput(buffer, sizeof(buffer));

  if (!contains(buffer, "\"roll_deg\":")) {
    test_fail("Missing roll_deg");
    return;
  }
  if (!contains(buffer, "\"pitch_deg\":")) {
    test_fail("Missing pitch_deg");
    return;
  }
  if (!contains(buffer, "\"yaw_deg\":")) {
    test_fail("Missing yaw_deg");
    return;
  }

  printf("  JSON: %s\n", buffer);
  test_pass();
}

void test_position_fields() {
  test_begin("Position fields in combined JSON (orientation + position)");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  // Need both orientation and position for getFormattedOutput()
  OrientationData orient = TestOrientations::NoRotation();
  PositionData pos = TestLocations::SanFrancisco();

  output.updateOrientation(orient);
  output.updatePosition(pos);

  char buffer[512];
  uint16_t len = output.getFormattedOutput(buffer, sizeof(buffer));

  if (len == 0) {
    test_fail("Failed to format JSON");
    return;
  }

  if (!contains(buffer, "\"latitude\":")) {
    test_fail("Missing latitude");
    return;
  }
  if (!contains(buffer, "\"longitude\":")) {
    test_fail("Missing longitude");
    return;
  }
  if (!contains(buffer, "\"altitude_m\":")) {
    test_fail("Missing altitude_m");
    return;
  }
  if (!contains(buffer, "\"num_satellites\":")) {
    test_fail("Missing num_satellites");
    return;
  }
  if (!contains(buffer, "\"fix_quality\":")) {
    test_fail("Missing fix_quality");
    return;
  }

  printf("  JSON: %s\n", buffer);
  test_pass();
}

void test_validity_flags() {
  test_begin("Validity flags in JSON");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  OrientationData orient = TestOrientations::NoRotation();
  output.updateOrientation(orient);

  char buffer[512];
  uint16_t len = output.getFormattedOutput(buffer, sizeof(buffer));

  if (!contains(buffer, "\"orientation_valid\":")) {
    test_fail("Missing orientation_valid flag");
    return;
  }
  if (!contains(buffer, "\"position_valid\":")) {
    test_fail("Missing position_valid flag");
    return;
  }

  printf("  JSON: %s\n", buffer);
  test_pass();
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
  printf("========================================\n");
  printf("JSON Output Integration Tests (Phase 2)\n");
  printf("========================================\n");

  test_orientation_only();
  test_position_only();
  test_combined_orientation_position();
  test_orientation_json_method();
  test_position_json_method();
  test_full_json_method();
  test_invalid_position();
  test_no_data_available();
  test_frequency_control();
  test_timestamp_present();
  test_calibration_status();
  test_multiple_updates();
  test_buffer_overflow_protection();
  test_quaternion_components();
  test_euler_angles();
  test_position_fields();
  test_validity_flags();

  printf("\n========================================\n");
  printf("Results: %d/%d tests passed\n", test_passed, test_count);
  printf("========================================\n");

  return (test_passed == test_count) ? 0 : 1;
}

#else

int main() {
  printf("ERROR: This test requires DEBUG_MODE to be defined\n");
  printf("Compile with: -D DEBUG_MODE\n");
  return 1;
}

#endif  // DEBUG_MODE
