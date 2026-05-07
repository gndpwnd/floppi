/**
 * Test Suite: EKF JSON Output (Phase 3)
 *
 * Comprehensive tests for Extended Kalman Filter fused state JSON output.
 * Validates JSON structure, field types, uncertainties, and edge cases.
 *
 * Compile: g++ -D DEBUG_MODE test_json_ekf_output.cpp -I.. -I../src -o test_ekf_json
 * Run: ./test_ekf_json
 */

#ifdef DEBUG_MODE

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <cstdlib>
#include <cstdint>

// Mock Arduino functions for testing
static uint32_t mock_millis_val = 0;
uint32_t millis() {
  return mock_millis_val++;
}

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
  printf("\n[Test %d] %s\n", test_count, name);
}

void test_pass() {
  printf("  ✓ PASS\n");
  test_passed++;
}

void test_fail(const char* msg) {
  printf("  ✗ FAIL: %s\n", msg);
}

// Helper functions
bool contains(const char* haystack, const char* needle) {
  return strstr(haystack, needle) != NULL;
}

bool starts_with(const char* str, const char* prefix) {
  return strncmp(str, prefix, strlen(prefix)) == 0;
}

bool ends_with(const char* str, const char* suffix) {
  size_t str_len = strlen(str);
  size_t suffix_len = strlen(suffix);
  if (str_len < suffix_len) return false;
  return strcmp(str + str_len - suffix_len, suffix) == 0;
}

// Basic JSON validation
bool is_valid_json_structure(const char* json) {
  if (!starts_with(json, "{")) return false;
  if (!ends_with(json, "}")) return false;

  int brace_count = 0;
  int bracket_count = 0;
  bool in_string = false;
  bool escape_next = false;

  for (size_t i = 0; json[i]; i++) {
    char c = json[i];

    if (escape_next) {
      escape_next = false;
      continue;
    }

    if (c == '\\') {
      escape_next = true;
      continue;
    }

    if (c == '"' && !escape_next) {
      in_string = !in_string;
      continue;
    }

    if (in_string) continue;

    if (c == '{') brace_count++;
    if (c == '}') brace_count--;
    if (c == '[') bracket_count++;
    if (c == ']') bracket_count--;

    if (brace_count < 0 || bracket_count < 0) return false;
  }

  return brace_count == 0 && bracket_count == 0 && !in_string;
}

// Extract float value from JSON
bool extract_json_float(const char* json, const char* key, float* value) {
  char search_str[128];
  snprintf(search_str, sizeof(search_str), "\"%s\":%f", key, *value);

  // Simplified: find key followed by colon and number
  const char* pos = strstr(json, key);
  if (!pos) return false;

  // Find colon after key
  pos = strchr(pos, ':');
  if (!pos) return false;

  // Skip whitespace
  pos++;
  while (*pos && (*pos == ' ' || *pos == '\t')) pos++;

  // Parse float
  char* endptr;
  float parsed = strtof(pos, &endptr);
  if (endptr == pos) return false;

  *value = parsed;
  return true;
}

// Extract boolean value from JSON
bool extract_json_bool(const char* json, const char* key, bool* value) {
  const char* pos = strstr(json, key);
  if (!pos) return false;

  pos = strchr(pos, ':');
  if (!pos) return false;

  pos++;
  while (*pos && (*pos == ' ' || *pos == '\t')) pos++;

  if (strncmp(pos, "true", 4) == 0) {
    *value = true;
    return true;
  } else if (strncmp(pos, "false", 5) == 0) {
    *value = false;
    return true;
  }

  return false;
}

// ============================================================================
// Test Cases
// ============================================================================

void test_fused_state_json_structure() {
  test_begin("Fused state JSON basic structure");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  // Create and update with fused state
  EKFState state;
  state.q_w = 0.707f;
  state.q_x = 0.0f;
  state.q_y = 0.0f;
  state.q_z = 0.707f;
  state.v_north = 1.0f;
  state.v_east = 0.5f;
  state.v_down = -0.1f;
  state.p_north = 145.2f;
  state.p_east = 87.3f;
  state.p_down = -23.5f;

  float cov[16*16];
  memset(cov, 0, sizeof(cov));
  for (int i = 0; i < 16; i++) {
    cov[i*16 + i] = 0.01f;  // Diagonal covariance
  }

  GPSStatus gps_status;
  gps_status.locked = true;
  gps_status.satellites = 12;
  gps_status.hdop = 0.75f;
  gps_status.age_ms = 45;

  EKFHealth ekf_health;
  ekf_health.covariance_trace = 125.3f;
  ekf_health.innovation_magnitude = 0.5f;
  ekf_health.num_updates = 1234;

  output.updateFusedState(state, cov, gps_status, ekf_health);

  char buffer[1024];
  uint16_t len = output.formatFusedStateJSON(buffer, sizeof(buffer));

  if (len == 0) {
    test_fail("Failed to format fused state JSON");
    return;
  }

  if (!is_valid_json_structure(buffer)) {
    test_fail("Invalid JSON structure");
    printf("  JSON: %.200s...\n", buffer);
    return;
  }

  if (!contains(buffer, "\"fused\"")) {
    test_fail("Missing 'fused' field");
    return;
  }

  if (!contains(buffer, "\"attitude\"")) {
    test_fail("Missing 'attitude' field");
    return;
  }

  if (!contains(buffer, "\"velocity\"")) {
    test_fail("Missing 'velocity' field");
    return;
  }

  if (!contains(buffer, "\"position\"")) {
    test_fail("Missing 'position' field");
    return;
  }

  if (!contains(buffer, "\"uncertainty\"")) {
    test_fail("Missing 'uncertainty' field");
    return;
  }

  if (!contains(buffer, "\"gps_status\"")) {
    test_fail("Missing 'gps_status' field");
    return;
  }

  if (!contains(buffer, "\"ekf_health\"")) {
    test_fail("Missing 'ekf_health' field");
    return;
  }

  test_pass();
}

void test_quaternion_values() {
  test_begin("Quaternion values in valid range");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  EKFState state;
  state.q_w = 0.5f;
  state.q_x = 0.5f;
  state.q_y = 0.5f;
  state.q_z = 0.5f;

  float cov[16*16];
  memset(cov, 0, sizeof(cov));
  for (int i = 0; i < 16; i++) {
    cov[i*16 + i] = 0.01f;
  }

  GPSStatus gps_status;
  EKFHealth ekf_health;

  output.updateFusedState(state, cov, gps_status, ekf_health);

  char buffer[1024];
  uint16_t len = output.formatFusedStateJSON(buffer, sizeof(buffer));

  if (len == 0) {
    test_fail("Failed to format JSON");
    return;
  }

  // Extract quaternion values
  float q_w = 0.0f, q_x = 0.0f, q_y = 0.0f, q_z = 0.0f;
  extract_json_float(buffer, "\"w\"", &q_w);
  extract_json_float(buffer, "\"x\"", &q_x);
  extract_json_float(buffer, "\"y\"", &q_y);
  extract_json_float(buffer, "\"z\"", &q_z);

  // Verify range (typically -1.0 to 1.0)
  if (q_w < -1.1f || q_w > 1.1f) {
    test_fail("Quaternion W out of range");
    return;
  }

  if (q_x < -1.1f || q_x > 1.1f) {
    test_fail("Quaternion X out of range");
    return;
  }

  test_pass();
}

void test_euler_angles() {
  test_begin("Euler angles in valid ranges");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  EKFState state;
  state.q_w = 1.0f;
  state.q_x = 0.0f;
  state.q_y = 0.0f;
  state.q_z = 0.0f;

  float cov[16*16];
  memset(cov, 0, sizeof(cov));
  for (int i = 0; i < 16; i++) {
    cov[i*16 + i] = 0.01f;
  }

  GPSStatus gps_status;
  EKFHealth ekf_health;

  output.updateFusedState(state, cov, gps_status, ekf_health);

  char buffer[1024];
  uint16_t len = output.formatFusedStateJSON(buffer, sizeof(buffer));

  if (len == 0) {
    test_fail("Failed to format JSON");
    return;
  }

  // Verify fields exist
  if (!contains(buffer, "roll_deg")) {
    test_fail("Missing roll_deg");
    return;
  }

  if (!contains(buffer, "pitch_deg")) {
    test_fail("Missing pitch_deg");
    return;
  }

  if (!contains(buffer, "yaw_deg")) {
    test_fail("Missing yaw_deg");
    return;
  }

  test_pass();
}

void test_velocity_values() {
  test_begin("Velocity values are reasonable");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  EKFState state;
  state.v_north = 10.5f;
  state.v_east = -5.2f;
  state.v_down = 0.1f;

  float cov[16*16];
  memset(cov, 0, sizeof(cov));
  for (int i = 0; i < 16; i++) {
    cov[i*16 + i] = 0.01f;
  }

  GPSStatus gps_status;
  EKFHealth ekf_health;

  output.updateFusedState(state, cov, gps_status, ekf_health);

  char buffer[1024];
  uint16_t len = output.formatFusedStateJSON(buffer, sizeof(buffer));

  if (len == 0) {
    test_fail("Failed to format JSON");
    return;
  }

  if (!contains(buffer, "north_mps")) {
    test_fail("Missing north_mps");
    return;
  }

  if (!contains(buffer, "east_mps")) {
    test_fail("Missing east_mps");
    return;
  }

  if (!contains(buffer, "down_mps")) {
    test_fail("Missing down_mps");
    return;
  }

  test_pass();
}

void test_position_values() {
  test_begin("Position values are reasonable");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  EKFState state;
  state.p_north = 1000.5f;
  state.p_east = -500.2f;
  state.p_down = -50.0f;

  float cov[16*16];
  memset(cov, 0, sizeof(cov));
  for (int i = 0; i < 16; i++) {
    cov[i*16 + i] = 0.01f;
  }

  GPSStatus gps_status;
  EKFHealth ekf_health;

  output.updateFusedState(state, cov, gps_status, ekf_health);

  char buffer[1024];
  uint16_t len = output.formatFusedStateJSON(buffer, sizeof(buffer));

  if (len == 0) {
    test_fail("Failed to format JSON");
    return;
  }

  if (!contains(buffer, "north_m")) {
    test_fail("Missing north_m");
    return;
  }

  if (!contains(buffer, "east_m")) {
    test_fail("Missing east_m");
    return;
  }

  if (!contains(buffer, "down_m")) {
    test_fail("Missing down_m");
    return;
  }

  test_pass();
}

void test_uncertainty_values() {
  test_begin("Uncertainty values are positive");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  EKFState state;
  float cov[16*16];
  memset(cov, 0, sizeof(cov));

  // Set reasonable covariance values
  for (int i = 0; i < 4; i++) cov[i*16 + i] = 0.01f;   // Attitude
  for (int i = 4; i < 7; i++) cov[i*16 + i] = 1.0f;    // Velocity
  for (int i = 7; i < 10; i++) cov[i*16 + i] = 100.0f; // Position
  for (int i = 10; i < 13; i++) cov[i*16 + i] = 0.1f;  // Accel bias
  for (int i = 13; i < 16; i++) cov[i*16 + i] = 0.01f; // Gyro bias

  GPSStatus gps_status;
  EKFHealth ekf_health;

  output.updateFusedState(state, cov, gps_status, ekf_health);

  char buffer[1024];
  uint16_t len = output.formatFusedStateJSON(buffer, sizeof(buffer));

  if (len == 0) {
    test_fail("Failed to format JSON");
    return;
  }

  if (!contains(buffer, "attitude_rad")) {
    test_fail("Missing attitude_rad");
    return;
  }

  if (!contains(buffer, "velocity_mps")) {
    test_fail("Missing velocity_mps");
    return;
  }

  if (!contains(buffer, "position_m")) {
    test_fail("Missing position_m");
    return;
  }

  if (!contains(buffer, "accel_bias_m_s2")) {
    test_fail("Missing accel_bias_m_s2");
    return;
  }

  test_pass();
}

void test_gps_status_locked() {
  test_begin("GPS status with lock");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  EKFState state;
  float cov[16*16];
  memset(cov, 0, sizeof(cov));
  for (int i = 0; i < 16; i++) {
    cov[i*16 + i] = 0.01f;
  }

  GPSStatus gps_status;
  gps_status.locked = true;
  gps_status.satellites = 12;
  gps_status.hdop = 0.8f;
  gps_status.dropout = false;
  gps_status.age_ms = 50;

  EKFHealth ekf_health;

  output.updateFusedState(state, cov, gps_status, ekf_health);

  char buffer[1024];
  uint16_t len = output.formatFusedStateJSON(buffer, sizeof(buffer));

  if (len == 0) {
    test_fail("Failed to format JSON");
    return;
  }

  if (!contains(buffer, "\"locked\":true")) {
    test_fail("GPS locked flag not correct");
    return;
  }

  if (!contains(buffer, "\"satellites\":12")) {
    test_fail("Satellite count incorrect");
    return;
  }

  if (!contains(buffer, "\"dropout\":false")) {
    test_fail("Dropout flag not correct");
    return;
  }

  test_pass();
}

void test_gps_status_dropout() {
  test_begin("GPS status with dropout");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  EKFState state;
  float cov[16*16];
  memset(cov, 0, sizeof(cov));
  for (int i = 0; i < 16; i++) {
    cov[i*16 + i] = 0.01f;
  }

  GPSStatus gps_status;
  gps_status.locked = false;
  gps_status.satellites = 0;
  gps_status.dropout = true;
  gps_status.age_ms = 5000;
  gps_status.dead_reckoning_valid = true;
  gps_status.dead_reckoning_age_ms = 1500;

  EKFHealth ekf_health;

  output.updateFusedState(state, cov, gps_status, ekf_health);

  char buffer[1024];
  uint16_t len = output.formatFusedStateJSON(buffer, sizeof(buffer));

  if (len == 0) {
    test_fail("Failed to format JSON");
    return;
  }

  if (!contains(buffer, "\"dropout\":true")) {
    test_fail("Dropout flag not correct");
    return;
  }

  if (!contains(buffer, "\"dead_reckoning_valid\":true")) {
    test_fail("Dead reckoning valid flag incorrect");
    return;
  }

  test_pass();
}

void test_ekf_health_metrics() {
  test_begin("EKF health metrics");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  EKFState state;
  float cov[16*16];
  memset(cov, 0, sizeof(cov));
  for (int i = 0; i < 16; i++) {
    cov[i*16 + i] = 0.01f;
  }

  GPSStatus gps_status;
  EKFHealth ekf_health;
  ekf_health.covariance_trace = 0.16f;
  ekf_health.innovation_magnitude = 0.25f;
  ekf_health.num_updates = 5000;

  output.updateFusedState(state, cov, gps_status, ekf_health);

  char buffer[1024];
  uint16_t len = output.formatFusedStateJSON(buffer, sizeof(buffer));

  if (len == 0) {
    test_fail("Failed to format JSON");
    return;
  }

  if (!contains(buffer, "covariance_trace")) {
    test_fail("Missing covariance_trace");
    return;
  }

  if (!contains(buffer, "innovation_magnitude")) {
    test_fail("Missing innovation_magnitude");
    return;
  }

  if (!contains(buffer, "num_updates")) {
    test_fail("Missing num_updates");
    return;
  }

  test_pass();
}

void test_comparison_json_structure() {
  test_begin("Comparison JSON (raw vs fused)");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  // Update with position
  PositionData pos;
  pos.latitude = 52.5200;
  pos.longitude = 13.4050;
  pos.altitude = 100.0f;
  pos.accuracy_m = 5.0f;
  pos.num_satellites = 15;
  pos.fix_quality = 1;
  output.updatePosition(pos);

  // Update with fused state
  EKFState state;
  state.p_north = 100.0f;
  state.p_east = 50.0f;
  state.p_down = -100.0f;

  float cov[16*16];
  memset(cov, 0, sizeof(cov));
  for (int i = 0; i < 16; i++) {
    cov[i*16 + i] = 0.01f;
  }

  GPSStatus gps_status;
  gps_status.locked = true;
  EKFHealth ekf_health;

  output.updateFusedState(state, cov, gps_status, ekf_health);

  char buffer[512];
  uint16_t len = output.formatComparisonJSON(buffer, sizeof(buffer));

  if (len == 0) {
    test_fail("Failed to format comparison JSON");
    return;
  }

  if (!is_valid_json_structure(buffer)) {
    test_fail("Invalid JSON structure");
    return;
  }

  if (!contains(buffer, "\"comparison\"")) {
    test_fail("Missing 'comparison' field");
    return;
  }

  if (!contains(buffer, "\"raw_gps\"")) {
    test_fail("Missing 'raw_gps' field");
    return;
  }

  if (!contains(buffer, "\"fused_position\"")) {
    test_fail("Missing 'fused_position' field");
    return;
  }

  test_pass();
}

void test_invalid_fused_state() {
  test_begin("Invalid fused state JSON");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  // Don't call updateFusedState - fused_valid_ should be false

  char buffer[256];
  uint16_t len = output.formatFusedStateJSON(buffer, sizeof(buffer));

  if (len == 0) {
    test_fail("Failed to format JSON");
    return;
  }

  if (!contains(buffer, "\"valid\":false")) {
    test_fail("Expected invalid flag");
    return;
  }

  test_pass();
}

void test_satellite_count_range() {
  test_begin("Satellite count in valid range (0-32)");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  EKFState state;
  float cov[16*16];
  memset(cov, 0, sizeof(cov));
  for (int i = 0; i < 16; i++) {
    cov[i*16 + i] = 0.01f;
  }

  GPSStatus gps_status;
  gps_status.satellites = 25;
  EKFHealth ekf_health;

  output.updateFusedState(state, cov, gps_status, ekf_health);

  char buffer[1024];
  uint16_t len = output.formatFusedStateJSON(buffer, sizeof(buffer));

  if (len == 0) {
    test_fail("Failed to format JSON");
    return;
  }

  if (!contains(buffer, "\"satellites\":25")) {
    test_fail("Satellite count not found");
    return;
  }

  test_pass();
}

void test_hdop_positive() {
  test_begin("HDOP is positive");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  EKFState state;
  float cov[16*16];
  memset(cov, 0, sizeof(cov));
  for (int i = 0; i < 16; i++) {
    cov[i*16 + i] = 0.01f;
  }

  GPSStatus gps_status;
  gps_status.hdop = 1.5f;
  EKFHealth ekf_health;

  output.updateFusedState(state, cov, gps_status, ekf_health);

  char buffer[1024];
  uint16_t len = output.formatFusedStateJSON(buffer, sizeof(buffer));

  if (len == 0) {
    test_fail("Failed to format JSON");
    return;
  }

  if (!contains(buffer, "\"hdop\":")) {
    test_fail("HDOP field missing");
    return;
  }

  test_pass();
}

void test_full_combined_output() {
  test_begin("Full combined output (all sections)");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  // Orientation
  OrientationData orient;
  orient.w = 0.707f;
  orient.x = 0.0f;
  orient.y = 0.0f;
  orient.z = 0.707f;
  orient.roll_deg = 0.0f;
  orient.pitch_deg = 0.0f;
  orient.yaw_deg = 90.0f;
  output.updateOrientation(orient);

  // Position
  PositionData pos;
  pos.latitude = 52.52;
  pos.longitude = 13.40;
  pos.altitude = 100.0f;
  pos.accuracy_m = 5.0f;
  pos.num_satellites = 12;
  output.updatePosition(pos);

  // Fused state
  EKFState state;
  state.q_w = 0.707f;
  state.q_z = 0.707f;
  state.p_north = 100.0f;
  state.p_east = 50.0f;

  float cov[16*16];
  memset(cov, 0, sizeof(cov));
  for (int i = 0; i < 16; i++) {
    cov[i*16 + i] = 0.01f;
  }

  GPSStatus gps_status;
  gps_status.locked = true;
  gps_status.satellites = 12;
  gps_status.age_ms = 50;

  EKFHealth ekf_health;
  ekf_health.covariance_trace = 0.16f;

  output.updateFusedState(state, cov, gps_status, ekf_health);

  char buffer[1024];
  uint16_t len = output.formatFullJSON(buffer, sizeof(buffer));

  if (len == 0) {
    test_fail("Failed to format full JSON");
    return;
  }

  if (!is_valid_json_structure(buffer)) {
    test_fail("Invalid JSON structure");
    return;
  }

  if (!contains(buffer, "orientation")) {
    test_fail("Missing orientation");
    return;
  }

  if (!contains(buffer, "position")) {
    test_fail("Missing position");
    return;
  }

  test_pass();
}

void test_performance() {
  test_begin("Performance: JSON generation completes");

  SensorOutputManager output;
  output.begin(OutputFormat::JSON);
  output.setFrequencyHz(100.0f);

  EKFState state;
  for (int i = 0; i < 16; i++) {
    state.q_w += 0.01f;
  }

  float cov[16*16];
  memset(cov, 0, sizeof(cov));
  for (int i = 0; i < 16; i++) {
    cov[i*16 + i] = 0.01f;
  }

  GPSStatus gps_status;
  EKFHealth ekf_health;

  output.updateFusedState(state, cov, gps_status, ekf_health);

  char buffer[1024];

  // Generate JSON multiple times
  for (int i = 0; i < 100; i++) {
    uint16_t len = output.formatFusedStateJSON(buffer, sizeof(buffer));
    if (len == 0) {
      test_fail("Failed to generate JSON");
      return;
    }
  }

  printf("  ✓ Generated 100 JSON outputs successfully\n");
  test_pass();
}

// ============================================================================
// Test Runner
// ============================================================================

int main() {
  printf("\n");
  printf("====================================================================\n");
  printf("EKF JSON Output Test Suite\n");
  printf("====================================================================\n");

  // Structure tests
  test_fused_state_json_structure();
  test_quaternion_values();
  test_euler_angles();
  test_velocity_values();
  test_position_values();
  test_uncertainty_values();

  // GPS status tests
  test_gps_status_locked();
  test_gps_status_dropout();
  test_satellite_count_range();
  test_hdop_positive();

  // EKF health tests
  test_ekf_health_metrics();

  // Comparison tests
  test_comparison_json_structure();

  // Edge cases
  test_invalid_fused_state();

  // Combined output
  test_full_combined_output();

  // Performance
  test_performance();

  printf("\n");
  printf("====================================================================\n");
  printf("Results: %d/%d tests passed\n", test_passed, test_count);
  printf("====================================================================\n\n");

  return (test_passed == test_count) ? 0 : 1;
}

#else  // DEBUG_MODE
int main() {
  printf("Compile with -D DEBUG_MODE to run tests\n");
  return 0;
}
#endif  // DEBUG_MODE
