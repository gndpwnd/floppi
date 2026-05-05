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

  // Start building JSON
  int offset = 0;
  int written = 0;

  // Opening and timestamp
  written = snprintf(buffer, max_len - offset,
      "{\"timestamp\":%lu,\"orientation\":{",
      now_ms);
  if (written < 0 || offset + written >= max_len) return 0;
  offset += written;

  // Quaternion data
  written = snprintf(buffer + offset, max_len - offset,
      "\"w\":%.4f,\"x\":%.4f,\"y\":%.4f,\"z\":%.4f,\"magnitude\":%.4f,"
      "\"calibration\":{\"system\":%d,\"accel\":%d,\"gyro\":%d,\"mag\":%d}",
      orientation_.w, orientation_.x, orientation_.y, orientation_.z, q_mag,
      orientation_.cal_status, orientation_.cal_accel, orientation_.cal_gyro, orientation_.cal_mag);
  if (written < 0 || offset + written >= max_len) return 0;
  offset += written;

  // Close orientation object and JSON
  written = snprintf(buffer + offset, max_len - offset, "}}");
  if (written < 0 || offset + written >= max_len) return 0;
  offset += written;

  buffer[offset] = '\0';
  return offset;
}

