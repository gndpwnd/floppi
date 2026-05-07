/**
 * Sensor Output Manager Implementation
 *
 * Handles orientation data output with frequency control.
 */

#if defined(ARDUINO) || defined(__AVR__) || defined(__arm__)
#include <Arduino.h>
#else
#include <stdint.h>
#include <cstdint>
static uint32_t mock_millis() {
  static auto start = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}
#define millis() mock_millis()
#include <chrono>
#endif

#include "sensor_output_manager.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Constructor
// ============================================================================

SensorOutputManager::SensorOutputManager()
    : format_(OutputFormat::JSON),
      frequency_hz_(10.0f),
      output_interval_ms_(100),  // 1000 / 10 Hz
      last_output_ms_(0),
      orientation_timestamp_ms_(0),
      orientation_valid_(false) {
}

// ============================================================================
// Initialization
// ============================================================================

bool SensorOutputManager::begin(OutputFormat format) {
  format_ = format;
  last_output_ms_ = millis();
  return true;
}

void SensorOutputManager::setFormat(OutputFormat format) {
  format_ = format;
}

void SensorOutputManager::setFrequencyHz(float hz) {
  if (hz > 0) {
    frequency_hz_ = hz;
    output_interval_ms_ = (uint32_t)(1000.0f / hz);
  }
}

// ============================================================================
// Data Update Methods
// ============================================================================

void SensorOutputManager::update(const OrientationData& orient) {
  orientation_ = orient;
  orientation_timestamp_ms_ = millis();
  orientation_valid_ = true;
}

// ============================================================================
// Output Control
// ============================================================================

bool SensorOutputManager::shouldOutput() {
  // Only output if we have valid orientation data
  if (!orientation_valid_) {
    return false;
  }

  // Check if enough time has elapsed since last output
  uint32_t now_ms = millis();
  if (now_ms - last_output_ms_ >= output_interval_ms_) {
    return true;
  }

  return false;
}

// ============================================================================
// Output Generation
// ============================================================================

uint16_t SensorOutputManager::getFormattedOutput(char* buffer, uint16_t max_len) {
  if (!buffer || max_len < 2) {
    return 0;
  }

  if (!orientation_valid_) {
    return 0;
  }

  uint16_t len = 0;

  // Only JSON format supported
  len = formatJSON(buffer, max_len);

  if (len > 0) {
    last_output_ms_ = millis();
  }

  return len;
}

OutputFormat SensorOutputManager::getOutputFormat() const {
  return format_;
}

// ============================================================================
// Format Implementations
// ============================================================================

uint16_t SensorOutputManager::formatJSON(char* buffer, uint16_t max_len) {
  uint32_t now_ms = millis();

  // Calculate quaternion magnitude for validation
  float q_mag = sqrt(
      orientation_.w * orientation_.w +
      orientation_.x * orientation_.x +
      orientation_.y * orientation_.y +
      orientation_.z * orientation_.z
  );

  // Arduino snprintf doesn't support %f, so use dtostrf for floats
  char w_str[12], x_str[12], y_str[12], z_str[12], mag_str[12];
  dtostrf(orientation_.w, 8, 6, w_str);
  dtostrf(orientation_.x, 8, 6, x_str);
  dtostrf(orientation_.y, 8, 6, y_str);
  dtostrf(orientation_.z, 8, 6, z_str);
  dtostrf(q_mag, 8, 6, mag_str);

  // Build JSON with pre-formatted floats
  int written = snprintf(buffer, max_len,
      "{\"timestamp\":%lu,\"orientation\":{"
      "\"w\":%s,\"x\":%s,\"y\":%s,\"z\":%s,\"magnitude\":%s,"
      "\"calibration\":{\"system\":%d,\"accel\":%d,\"gyro\":%d,\"mag\":%d}}}",
      now_ms,
      w_str, x_str, y_str, z_str, mag_str,
      orientation_.cal_status, orientation_.cal_accel, orientation_.cal_gyro, orientation_.cal_mag);

  if (written < 0 || written >= (int)max_len) {
    return 0;
  }

  return written;
}

