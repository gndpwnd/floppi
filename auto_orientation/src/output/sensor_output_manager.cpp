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
      position_valid_(false),
      fused_timestamp_ms_(0),
      fused_valid_(false),
      fused_position_error_m_(0.0f) {
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

// ============================================================================
// EKF Fused State Methods (Phase 3)
// ============================================================================

void SensorOutputManager::updateFusedState(const EKFState& state,
                                           const float* covariance_16x16,
                                           const GPSStatus& gps_status,
                                           const EKFHealth& ekf_health) {
  fused_state_ = state;
  gps_status_ = gps_status;
  ekf_health_ = ekf_health;
  fused_timestamp_ms_ = millis();

  // Extract uncertainty from covariance diagonal
  // Covariance is 16x16 row-major: element at [i,i] is at index i*17 (i*16 + i)
  if (covariance_16x16) {
    // Attitude uncertainty (average of quaternion covariances)
    fused_uncertainty_.attitude_rad = sqrt(
        (covariance_16x16[0*16+0] + covariance_16x16[1*16+1] +
         covariance_16x16[2*16+2] + covariance_16x16[3*16+3]) / 4.0f
    );

    // Velocity uncertainty (average of velocity covariances)
    fused_uncertainty_.velocity_mps = sqrt(
        (covariance_16x16[4*16+4] + covariance_16x16[5*16+5] +
         covariance_16x16[6*16+6]) / 3.0f
    );

    // Position uncertainty (average of position covariances)
    fused_uncertainty_.position_m = sqrt(
        (covariance_16x16[7*16+7] + covariance_16x16[8*16+8] +
         covariance_16x16[9*16+9]) / 3.0f
    );

    // Accel bias uncertainty (average of accel bias covariances)
    fused_uncertainty_.accel_bias_mps2 = sqrt(
        (covariance_16x16[10*16+10] + covariance_16x16[11*16+11] +
         covariance_16x16[12*16+12]) / 3.0f
    );

    // Gyro bias uncertainty (average of gyro bias covariances)
    fused_uncertainty_.gyro_bias_rads = sqrt(
        (covariance_16x16[13*16+13] + covariance_16x16[14*16+14] +
         covariance_16x16[15*16+15]) / 3.0f
    );
  }

  fused_valid_ = true;
}

uint16_t SensorOutputManager::formatFusedStateJSON(char* buffer, uint16_t max_len) {
  uint32_t now_ms = millis();

  if (!buffer || max_len < 100) {
    return 0;
  }

  if (!fused_valid_) {
    int written = snprintf(buffer, max_len,
        "{\"timestamp\":%u,\"fused\":{\"valid\":false}}",
        now_ms);
    if (written < 0 || written >= (int)max_len) {
      return 0;
    }
    return written;
  }

  // Calculate Euler angles from quaternion
  float roll_deg, pitch_deg, yaw_deg;
  quaternionToEuler(fused_state_.q_w, fused_state_.q_x,
                     fused_state_.q_y, fused_state_.q_z,
                     &roll_deg, &pitch_deg, &yaw_deg);

  // Format all float components with appropriate precision
  char q_w[12], q_x[12], q_y[12], q_z[12];
  char roll[12], pitch[12], yaw[12];
  char v_n[12], v_e[12], v_d[12];
  char p_n[12], p_e[12], p_d[12];
  char att_unc[12], vel_unc[12], pos_unc[12], bias_unc[12];
  char hdop[12];
  char cov_trace[12], innov_mag[12];

  dtostrf(fused_state_.q_w, 8, 6, q_w);
  dtostrf(fused_state_.q_x, 8, 6, q_x);
  dtostrf(fused_state_.q_y, 8, 6, q_y);
  dtostrf(fused_state_.q_z, 8, 6, q_z);
  dtostrf(roll_deg, 8, 2, roll);
  dtostrf(pitch_deg, 8, 2, pitch);
  dtostrf(yaw_deg, 8, 2, yaw);
  dtostrf(fused_state_.v_north, 8, 3, v_n);
  dtostrf(fused_state_.v_east, 8, 3, v_e);
  dtostrf(fused_state_.v_down, 8, 3, v_d);
  dtostrf(fused_state_.p_north, 8, 2, p_n);
  dtostrf(fused_state_.p_east, 8, 2, p_e);
  dtostrf(fused_state_.p_down, 8, 2, p_d);
  dtostrf(fused_uncertainty_.attitude_rad, 8, 4, att_unc);
  dtostrf(fused_uncertainty_.velocity_mps, 8, 3, vel_unc);
  dtostrf(fused_uncertainty_.position_m, 8, 2, pos_unc);
  dtostrf(fused_uncertainty_.accel_bias_mps2, 8, 4, bias_unc);
  dtostrf(gps_status_.hdop, 8, 2, hdop);
  dtostrf(ekf_health_.covariance_trace, 8, 2, cov_trace);
  dtostrf(ekf_health_.innovation_magnitude, 8, 3, innov_mag);

  // Build the complete JSON
  int total_written = 0;
  int written = snprintf(buffer, max_len,
      "{\"timestamp\":%u,\"fused\":{\"valid\":true,"
      "\"attitude\":{\"quaternion\":{\"w\":%s,\"x\":%s,\"y\":%s,\"z\":%s},"
      "\"euler\":{\"roll_deg\":%s,\"pitch_deg\":%s,\"yaw_deg\":%s}},"
      "\"velocity\":{\"north_mps\":%s,\"east_mps\":%s,\"down_mps\":%s},"
      "\"position\":{\"north_m\":%s,\"east_m\":%s,\"down_m\":%s}},"
      "\"uncertainty\":{"
      "\"attitude_rad\":%s,\"velocity_mps\":%s,\"position_m\":%s,"
      "\"accel_bias_m_s2\":%s},"
      "\"gps_status\":{"
      "\"locked\":%s,\"satellites\":%d,\"hdop\":%s,\"dropout\":%s,"
      "\"age_ms\":%u,\"dead_reckoning_valid\":%s,\"dead_reckoning_age_ms\":%u},"
      "\"ekf_health\":{"
      "\"covariance_trace\":%s,\"innovation_magnitude\":%s,"
      "\"num_updates\":%u}}",
      now_ms,
      q_w, q_x, q_y, q_z, roll, pitch, yaw,
      v_n, v_e, v_d, p_n, p_e, p_d,
      att_unc, vel_unc, pos_unc, bias_unc,
      gps_status_.locked ? "true" : "false",
      gps_status_.satellites,
      hdop,
      gps_status_.dropout ? "true" : "false",
      gps_status_.age_ms,
      gps_status_.dead_reckoning_valid ? "true" : "false",
      gps_status_.dead_reckoning_age_ms,
      cov_trace, innov_mag, ekf_health_.num_updates);

  if (written < 0 || written >= (int)max_len) {
    return 0;
  }

  return written;
}

uint16_t SensorOutputManager::formatComparisonJSON(char* buffer, uint16_t max_len) {
  uint32_t now_ms = millis();

  if (!buffer || max_len < 100) {
    return 0;
  }

  if (!fused_valid_ || !position_valid_) {
    // Need both fused and raw GPS to do comparison
    int written = snprintf(buffer, max_len,
        "{\"timestamp\":%u,\"comparison\":{\"valid\":false,"
        "\"reason\":\"insufficient_data\"}}",
        now_ms);
    if (written < 0 || written >= (int)max_len) {
      return 0;
    }
    return written;
  }

  // Calculate position difference (simple 2D distance for now)
  // Note: This is a simplified version; full implementation would convert
  // from NED to lat/lon for proper comparison
  float pos_diff_m = fused_position_error_m_;

  char gps_lat[16], gps_lon[16];
  char pos_diff[12], rms_error[12];

  dtostrf(position_.latitude, 12, 8, gps_lat);
  dtostrf(position_.longitude, 12, 8, gps_lon);
  dtostrf(pos_diff_m, 8, 2, pos_diff);
  dtostrf(fused_uncertainty_.position_m, 8, 2, rms_error);

  int written = snprintf(buffer, max_len,
      "{\"timestamp\":%u,\"comparison\":{\"valid\":true,"
      "\"raw_gps\":{\"latitude\":%s,\"longitude\":%s,"
      "\"accuracy_m\":%.2f,\"num_satellites\":%d},"
      "\"fused_position\":{\"north_m\":%.2f,\"east_m\":%.2f,\"down_m\":%.2f},"
      "\"difference_m\":%s,\"rms_error_m\":%s,"
      "\"gps_dropout\":%s}}",
      now_ms,
      gps_lat, gps_lon, position_.accuracy_m, position_.num_satellites,
      fused_state_.p_north, fused_state_.p_east, fused_state_.p_down,
      pos_diff, rms_error,
      gps_status_.dropout ? "true" : "false");

  if (written < 0 || written >= (int)max_len) {
    return 0;
  }

  return written;
}

uint16_t SensorOutputManager::quaternionToEuler(float q_w, float q_x, float q_y, float q_z,
                                                float* roll_deg, float* pitch_deg, float* yaw_deg) {
  if (!roll_deg || !pitch_deg || !yaw_deg) {
    return 0;
  }

  // Roll (X-axis rotation)
  float sin_roll = 2.0f * (q_w * q_x + q_y * q_z);
  float cos_roll = 1.0f - 2.0f * (q_x * q_x + q_y * q_y);
  *roll_deg = atan2(sin_roll, cos_roll) * 180.0f / 3.14159265359f;

  // Pitch (Y-axis rotation)
  float sin_pitch = 2.0f * (q_w * q_y - q_z * q_x);
  // Clamp to avoid numerical issues with asin
  if (sin_pitch > 1.0f) sin_pitch = 1.0f;
  if (sin_pitch < -1.0f) sin_pitch = -1.0f;
  *pitch_deg = asin(sin_pitch) * 180.0f / 3.14159265359f;

  // Yaw (Z-axis rotation)
  float sin_yaw = 2.0f * (q_w * q_z + q_x * q_y);
  float cos_yaw = 1.0f - 2.0f * (q_y * q_y + q_z * q_z);
  *yaw_deg = atan2(sin_yaw, cos_yaw) * 180.0f / 3.14159265359f;

  return 1;
}

