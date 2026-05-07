/**
 * Snapshot Recorder Implementation
 *
 * Records quaternion + calibration data to SD card in JSONL format.
 * Full implementation when SNAPSHOT_MODE is enabled.
 */

#include "snapshot_recorder.h"

#if ENABLE_SNAPSHOT_RECORDER

#include <Arduino.h>
#include <cmath>
#include <cstdio>
#include <string.h>
#include "../sensors/sensor_base.h"

// ============================================================================
// Constructor
// ============================================================================

SnapshotRecorder::SnapshotRecorder()
    : ready_(false),
      snapshot_count_(0),
      current_file_number_(1) {
  memset(current_filename_, 0, sizeof(current_filename_));
  memset(status_string_, 0, sizeof(status_string_));
}

// ============================================================================
// Initialization
// ============================================================================

bool SnapshotRecorder::initialize() {
  Serial.println("Initializing snapshot recorder...");

  // Initialize SD card
  if (!sd_card_.initialize()) {
    Serial.println("ERROR: SD card initialization failed!");
    snprintf(status_string_, sizeof(status_string_), "SD card init failed");
    ready_ = false;
    return false;
  }

  // Create snapshots directory
  if (!sd_card_.create_directory(SNAPSHOTS_DIRECTORY)) {
    Serial.println("ERROR: Could not create snapshots directory!");
    snprintf(status_string_, sizeof(status_string_), "Could not create snapshots dir");
    ready_ = false;
    return false;
  }

  // Find next available file number
  current_file_number_ = find_next_file_number();

  // Create initial file
  snprintf(current_filename_, sizeof(current_filename_),
           SNAPSHOT_FILENAME_FORMAT, current_file_number_);

  if (!sd_card_.create_file(current_filename_)) {
    Serial.printf("ERROR: Could not create snapshot file: %s\n", current_filename_);
    snprintf(status_string_, sizeof(status_string_), "Could not create snapshot file");
    ready_ = false;
    return false;
  }

  ready_ = true;
  update_status_string();
  Serial.printf("✓ Snapshot recorder ready (file: %s)\n", current_filename_);

  return true;
}

// ============================================================================
// Snapshot Recording
// ============================================================================

bool SnapshotRecorder::record_snapshot(const Quaternion& quat,
                                      uint8_t cal_system, uint8_t cal_accel,
                                      uint8_t cal_gyro, uint8_t cal_mag,
                                      uint32_t timestamp_ms) {
  if (!ready_) {
    Serial.println("ERROR: Snapshot recorder not initialized");
    return false;
  }

  // Build snapshot data
  SnapshotData snap;
  snap.timestamp_ms = timestamp_ms;
  snap.quat_w = quat.w;
  snap.quat_x = quat.x;
  snap.quat_y = quat.y;
  snap.quat_z = quat.z;
  snap.cal_system = cal_system;
  snap.cal_accel = cal_accel;
  snap.cal_gyro = cal_gyro;
  snap.cal_mag = cal_mag;

  // Convert quaternion to Euler angles
  quaternion_to_euler(snap.quat_w, snap.quat_x, snap.quat_y, snap.quat_z,
                      snap.euler_roll_deg, snap.euler_pitch_deg, snap.euler_yaw_deg);

  // Format as JSON
  char json_buffer[SNAPSHOT_JSON_MAX_LENGTH];
  if (!format_snapshot_json(snap, json_buffer, sizeof(json_buffer))) {
    Serial.println("ERROR: Failed to format snapshot JSON");
    return false;
  }

  // Write to SD card
  if (!sd_card_.append_line(current_filename_, json_buffer)) {
    Serial.printf("ERROR: Failed to write to %s\n", current_filename_);
    return false;
  }

  snapshot_count_++;

  // Check if we need to rotate files
  if (snapshot_count_ >= MAX_SNAPSHOT_FILES) {
    if (!rotate_to_next_file()) {
      Serial.println("ERROR: Failed to rotate to next snapshot file");
      ready_ = false;
      return false;
    }
  }

  update_status_string();
  return true;
}

bool SnapshotRecorder::record_snapshot_from_orientation(
    const OrientationData& orientation, uint32_t timestamp_ms) {
  Quaternion q(orientation.w, orientation.x, orientation.y, orientation.z);
  return record_snapshot(q, orientation.cal_status, orientation.cal_accel,
                         orientation.cal_gyro, orientation.cal_mag, timestamp_ms);
}

// ============================================================================
// Status & Control
// ============================================================================

bool SnapshotRecorder::close_current_file() {
  if (current_filename_[0] == '\0') {
    return true;  // No file open
  }
  return sd_card_.close_file(current_filename_);
}

const char* SnapshotRecorder::get_status_string() const {
  return status_string_;
}

// ============================================================================
// Private Helper Functions
// ============================================================================

void SnapshotRecorder::quaternion_to_euler(float qw, float qx, float qy, float qz,
                                           float& roll_deg, float& pitch_deg, float& yaw_deg) {
  // Convert quaternion to Euler angles using ZYX convention (yaw, pitch, roll)
  // Reference: https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles

  // Normalize quaternion first (defensive)
  float mag = sqrtf(qw*qw + qx*qx + qy*qy + qz*qz);
  if (mag < 0.001f) {
    roll_deg = 0.0f;
    pitch_deg = 0.0f;
    yaw_deg = 0.0f;
    return;
  }

  qw /= mag;
  qx /= mag;
  qy /= mag;
  qz /= mag;

  // Roll (X-axis rotation)
  float sinr_cosp = 2.0f * (qw * qx + qy * qz);
  float cosr_cosp = 1.0f - 2.0f * (qx * qx + qy * qy);
  roll_deg = atan2f(sinr_cosp, cosr_cosp) * 180.0f / M_PI;

  // Pitch (Y-axis rotation)
  float sinp = 2.0f * (qw * qy - qz * qx);
  if (fabsf(sinp) >= 1.0f) {
    pitch_deg = copysignf(90.0f, sinp);  // 90 or -90 (gimbal lock)
  } else {
    pitch_deg = asinf(sinp) * 180.0f / M_PI;
  }

  // Yaw (Z-axis rotation)
  float siny_cosp = 2.0f * (qw * qz + qx * qy);
  float cosy_cosp = 1.0f - 2.0f * (qy * qy + qz * qz);
  yaw_deg = atan2f(siny_cosp, cosy_cosp) * 180.0f / M_PI;
}

bool SnapshotRecorder::format_snapshot_json(const SnapshotData& snap,
                                            char* output, size_t output_size) {
  int written = snprintf(output, output_size,
    "{"
    "\"timestamp_ms\":%lu,"
    "\"quaternion\":{\"w\":%.6f,\"x\":%.6f,\"y\":%.6f,\"z\":%.6f},"
    "\"euler\":{\"roll_deg\":%.2f,\"pitch_deg\":%.2f,\"yaw_deg\":%.2f},"
    "\"calibration\":{\"system\":%d,\"accel\":%d,\"gyro\":%d,\"mag\":%d}"
    "}",
    (unsigned long)snap.timestamp_ms,
    snap.quat_w, snap.quat_x, snap.quat_y, snap.quat_z,
    snap.euler_roll_deg, snap.euler_pitch_deg, snap.euler_yaw_deg,
    snap.cal_system, snap.cal_accel, snap.cal_gyro, snap.cal_mag);

  if (written < 0 || written >= (int)output_size) {
    Serial.println("ERROR: Snapshot JSON formatting failed (buffer too small)");
    return false;
  }

  return true;
}

uint16_t SnapshotRecorder::find_next_file_number() {
  // Scan snapshots directory to find the highest file number
  // Start from 1 and count up
  uint16_t max_number = 0;

  for (uint16_t i = 1; i <= MAX_SNAPSHOT_FILES; i++) {
    char filename[64];
    snprintf(filename, sizeof(filename), SNAPSHOT_FILENAME_FORMAT, i);
    if (sd_card_.file_exists(filename)) {
      max_number = i;
    }
  }

  // Return next file number (or 1 if no files exist)
  if (max_number == 0) {
    return 1;
  } else if (max_number < MAX_SNAPSHOT_FILES) {
    return max_number + 1;
  } else {
    // Wrap around (rotating)
    return 1;
  }
}

bool SnapshotRecorder::rotate_to_next_file() {
  Serial.println("Rotating to next snapshot file...");

  // Close current file
  if (!close_current_file()) {
    Serial.println("ERROR: Could not close current snapshot file");
    return false;
  }

  // Move to next file number
  current_file_number_++;
  if (current_file_number_ > MAX_SNAPSHOT_FILES) {
    current_file_number_ = 1;
  }

  // Create new file
  snprintf(current_filename_, sizeof(current_filename_),
           SNAPSHOT_FILENAME_FORMAT, current_file_number_);

  if (!sd_card_.create_file(current_filename_)) {
    Serial.printf("ERROR: Could not create new snapshot file: %s\n", current_filename_);
    ready_ = false;
    return false;
  }

  // Reset snapshot count for new file
  snapshot_count_ = 0;

  Serial.printf("✓ Rotated to %s\n", current_filename_);
  return true;
}

void SnapshotRecorder::update_status_string() {
  snprintf(status_string_, sizeof(status_string_),
           "Snapshot recorder: %lu recorded (%s)",
           snapshot_count_, current_filename_);
}

#endif  // ENABLE_SNAPSHOT_RECORDER
