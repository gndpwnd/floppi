/**
 * Sensor Output Manager Implementation
 *
 * Handles orientation data output with frequency control.
 */

#if defined(ARDUINO) || defined(__AVR__) || defined(__arm__)
#include <Arduino.h>
#else
#include <stdint.h>
#include <stdint.h>
#include <chrono>
static uint32_t mock_millis() {
  static auto start = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}
#define millis() mock_millis()
#endif

#include "sensor_output_manager.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// Provide dtostrf for non-Arduino platforms
#ifndef ARDUINO
#include <stdlib.h>
static char* dtostrf(double val, signed char width, unsigned char prec, char *sout) {
  int len = snprintf(sout, 20, "%*.*f", width, prec, val);
  if (len < 0) {
    sout[0] = '\0';
  }
  return sout;
}
#endif

// ============================================================================
// Constructor
// ============================================================================

SensorOutputManager::SensorOutputManager()
    : format_(OutputFormat::JSON),
      frequency_hz_(10.0f),
      output_interval_ms_(100),  // 1000 / 10 Hz
      last_output_ms_(0),
      orientation_timestamp_ms_(0),
      position_timestamp_ms_(0),
      orientation_valid_(false),
      position_valid_(false) {
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

void SensorOutputManager::updateOrientation(const OrientationData& orient) {
  orientation_ = orient;
  orientation_timestamp_ms_ = millis();
  orientation_valid_ = true;
}

void SensorOutputManager::updatePosition(const PositionData& position) {
  position_ = position;
  position_timestamp_ms_ = millis();
  position_valid_ = (position.fix_quality > 0);
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
  // Delegate to the full JSON formatter
  return formatFullJSON(buffer, max_len);
}

uint16_t SensorOutputManager::formatOrientationJSON(char* buffer, uint16_t max_len) {
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
  char roll_str[12], pitch_str[12], yaw_str[12];
  dtostrf(orientation_.w, 8, 6, w_str);
  dtostrf(orientation_.x, 8, 6, x_str);
  dtostrf(orientation_.y, 8, 6, y_str);
  dtostrf(orientation_.z, 8, 6, z_str);
  dtostrf(q_mag, 8, 6, mag_str);
  dtostrf(orientation_.roll_deg, 8, 2, roll_str);
  dtostrf(orientation_.pitch_deg, 8, 2, pitch_str);
  dtostrf(orientation_.yaw_deg, 8, 2, yaw_str);

  // Build JSON with pre-formatted floats (orientation-only)
  int written = snprintf(buffer, max_len,
      "{\"timestamp\":%u,\"orientation_valid\":true,\"orientation\":{"
      "\"w\":%s,\"x\":%s,\"y\":%s,\"z\":%s,\"magnitude\":%s,"
      "\"euler\":{\"roll_deg\":%s,\"pitch_deg\":%s,\"yaw_deg\":%s},"
      "\"calibration\":{\"system\":%d,\"accel\":%d,\"gyro\":%d,\"mag\":%d}}}",
      now_ms,
      w_str, x_str, y_str, z_str, mag_str,
      roll_str, pitch_str, yaw_str,
      orientation_.cal_status, orientation_.cal_accel, orientation_.cal_gyro, orientation_.cal_mag);

  if (written < 0 || written >= (int)max_len) {
    return 0;
  }

  return written;
}

uint16_t SensorOutputManager::formatPositionJSON(char* buffer, uint16_t max_len) {
  uint32_t now_ms = millis();

  if (!position_valid_) {
    // Invalid position: return minimal JSON
    int written = snprintf(buffer, max_len,
        "{\"timestamp\":%u,\"position_valid\":false,\"position\":{}}",
        now_ms);
    if (written < 0 || written >= (int)max_len) {
      return 0;
    }
    return written;
  }

  // Format position fields with appropriate precision
  char lat_str[16], lon_str[16], alt_str[12], acc_str[12];
  dtostrf(position_.latitude, 12, 8, lat_str);
  dtostrf(position_.longitude, 12, 8, lon_str);
  dtostrf(position_.altitude, 10, 2, alt_str);
  dtostrf(position_.accuracy_m, 10, 2, acc_str);

  // Build position-only JSON
  int written = snprintf(buffer, max_len,
      "{\"timestamp\":%u,\"position_valid\":true,\"position\":{"
      "\"latitude\":%s,\"longitude\":%s,\"altitude_m\":%s,"
      "\"accuracy_m\":%s,\"num_satellites\":%d,\"fix_quality\":%d}}",
      now_ms,
      lat_str, lon_str, alt_str, acc_str,
      position_.num_satellites, position_.fix_quality);

  if (written < 0 || written >= (int)max_len) {
    return 0;
  }

  return written;
}

uint16_t SensorOutputManager::formatFullJSON(char* buffer, uint16_t max_len) {
  uint32_t now_ms = millis();

  if (!buffer || max_len < 50) {
    return 0;
  }

  // Start JSON object with timestamp
  int total_written = 0;
  int written = snprintf(buffer, max_len, "{\"timestamp\":%u", now_ms);
  if (written < 0 || written >= (int)max_len) {
    return 0;
  }
  total_written = written;
  char* ptr = buffer + written;
  uint16_t remaining = max_len - written;

  // Add orientation validity flag
  written = snprintf(ptr, remaining, ",\"orientation_valid\":%s",
      orientation_valid_ ? "true" : "false");
  if (written < 0 || written >= (int)remaining) {
    return 0;
  }
  total_written += written;
  ptr += written;
  remaining -= written;

  // Add orientation data if valid
  if (orientation_valid_) {
    float q_mag = sqrt(
        orientation_.w * orientation_.w +
        orientation_.x * orientation_.x +
        orientation_.y * orientation_.y +
        orientation_.z * orientation_.z
    );

    char w_str[12], x_str[12], y_str[12], z_str[12], mag_str[12];
    char roll_str[12], pitch_str[12], yaw_str[12];
    dtostrf(orientation_.w, 8, 6, w_str);
    dtostrf(orientation_.x, 8, 6, x_str);
    dtostrf(orientation_.y, 8, 6, y_str);
    dtostrf(orientation_.z, 8, 6, z_str);
    dtostrf(q_mag, 8, 6, mag_str);
    dtostrf(orientation_.roll_deg, 8, 2, roll_str);
    dtostrf(orientation_.pitch_deg, 8, 2, pitch_str);
    dtostrf(orientation_.yaw_deg, 8, 2, yaw_str);

    written = snprintf(ptr, remaining,
        ",\"orientation\":{"
        "\"w\":%s,\"x\":%s,\"y\":%s,\"z\":%s,\"magnitude\":%s,"
        "\"euler\":{\"roll_deg\":%s,\"pitch_deg\":%s,\"yaw_deg\":%s},"
        "\"calibration\":{\"system\":%d,\"accel\":%d,\"gyro\":%d,\"mag\":%d}}",
        w_str, x_str, y_str, z_str, mag_str,
        roll_str, pitch_str, yaw_str,
        orientation_.cal_status, orientation_.cal_accel, orientation_.cal_gyro, orientation_.cal_mag);
    if (written < 0 || written >= (int)remaining) {
      return 0;
    }
    total_written += written;
    ptr += written;
    remaining -= written;
  }

  // Add position validity flag
  written = snprintf(ptr, remaining, ",\"position_valid\":%s",
      position_valid_ ? "true" : "false");
  if (written < 0 || written >= (int)remaining) {
    return 0;
  }
  total_written += written;
  ptr += written;
  remaining -= written;

  // Add position data if valid
  if (position_valid_) {
    char lat_str[16], lon_str[16], alt_str[12], acc_str[12];
    dtostrf(position_.latitude, 12, 8, lat_str);
    dtostrf(position_.longitude, 12, 8, lon_str);
    dtostrf(position_.altitude, 10, 2, alt_str);
    dtostrf(position_.accuracy_m, 10, 2, acc_str);

    written = snprintf(ptr, remaining,
        ",\"position\":{"
        "\"latitude\":%s,\"longitude\":%s,\"altitude_m\":%s,"
        "\"accuracy_m\":%s,\"num_satellites\":%d,\"fix_quality\":%d}",
        lat_str, lon_str, alt_str, acc_str,
        position_.num_satellites, position_.fix_quality);
    if (written < 0 || written >= (int)remaining) {
      return 0;
    }
    total_written += written;
    ptr += written;
    remaining -= written;
  }

  // Close JSON object
  if (remaining < 2) {
    return 0;
  }
  *ptr++ = '}';
  *ptr = '\0';
  total_written += 1;

  return total_written;
}

