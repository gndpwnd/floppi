/**
 * Snapshot Recorder Feature
 *
 * Records quaternion + timestamp + calibration data to SD card in JSONL format.
 * Triggered by button press or manual call to record_snapshot().
 *
 * Only compiled when SNAPSHOT_MODE build flag is enabled.
 *
 * Features:
 * - Auto-rotating filenames: snapshots/snap_001.json, snap_002.json, etc.
 * - JSONL format (one JSON per line) for easy parsing
 * - Error recovery: logs errors but doesn't crash on SD card issues
 * - Memory-efficient: no dynamic allocation during recording
 * - Status tracking: ready state, snapshot count, error state
 *
 * Example Usage:
 *   SnapshotRecorder recorder;
 *   if (recorder.initialize()) {
 *     Quaternion q = {0.707, 0, 0, 0.707};
 *     uint8_t cal_level = 3;
 *     if (recorder.record_snapshot(q, cal_level, millis())) {
 *       Serial.println("Snapshot recorded!");
 *     }
 *   }
 *
 * JSON Format (one line per snapshot):
 *   {
 *     "timestamp_ms": 1234567,
 *     "quaternion": {"w": 0.707107, "x": 0.0, "y": 0.0, "z": 0.707107},
 *     "euler": {"roll_deg": 0.0, "pitch_deg": 0.0, "yaw_deg": 90.0},
 *     "calibration": {"system": 3, "accel": 3, "gyro": 3, "mag": 3}
 *   }
 */

#ifndef SNAPSHOT_RECORDER_H
#define SNAPSHOT_RECORDER_H

#include "config/mode.h"
#include <stdint.h>
#include <stddef.h>

// Forward declarations
class Quaternion;
struct OrientationData;

#if ENABLE_SNAPSHOT_RECORDER

#include "../file_system/sd_card.h"
#include "../math/quaternion.h"

// ============================================================================
// Configuration
// ============================================================================

// Maximum number of snapshot files before rotating (0-999)
#define MAX_SNAPSHOT_FILES 100

// Snapshots directory path
#define SNAPSHOTS_DIRECTORY "snapshots"

// Snapshot filename format (will be formatted with printf)
#define SNAPSHOT_FILENAME_FORMAT "snapshots/snap_%03d.json"

// Maximum length of a single snapshot JSON line
#define SNAPSHOT_JSON_MAX_LENGTH 256

// ============================================================================
// Data Structures
// ============================================================================

/**
 * Snapshot data: captured state at one moment in time
 */
struct SnapshotData {
  uint32_t timestamp_ms;      // Timestamp in milliseconds
  float quat_w, quat_x, quat_y, quat_z;  // Quaternion components
  float euler_roll_deg;       // Roll in degrees
  float euler_pitch_deg;      // Pitch in degrees
  float euler_yaw_deg;        // Yaw in degrees
  uint8_t cal_system;         // System calibration (0-3)
  uint8_t cal_accel;          // Accelerometer calibration (0-3)
  uint8_t cal_gyro;           // Gyroscope calibration (0-3)
  uint8_t cal_mag;            // Magnetometer calibration (0-3)

  SnapshotData()
      : timestamp_ms(0),
        quat_w(1.0f), quat_x(0.0f), quat_y(0.0f), quat_z(0.0f),
        euler_roll_deg(0.0f), euler_pitch_deg(0.0f), euler_yaw_deg(0.0f),
        cal_system(0), cal_accel(0), cal_gyro(0), cal_mag(0) {}
};

// ============================================================================
// Snapshot Recorder Class
// ============================================================================

class SnapshotRecorder {
 public:
  /**
   * Default constructor.
   */
  SnapshotRecorder();

  /**
   * Initialize snapshot recorder.
   * Must be called before recording any snapshots.
   *
   * @return true if initialization successful, false otherwise
   *
   * Behavior:
   * - Initializes SD card
   * - Creates snapshots/ directory if needed
   * - Determines next available file number
   */
  bool initialize();

  /**
   * Record a snapshot to SD card.
   * Captures quaternion + timestamp + calibration data as JSON.
   *
   * @param quat Quaternion representing current orientation
   * @param cal_system System calibration status (0-3)
   * @param cal_accel Accelerometer calibration status (0-3)
   * @param cal_gyro Gyroscope calibration status (0-3)
   * @param cal_mag Magnetometer calibration status (0-3)
   * @param timestamp_ms Current timestamp in milliseconds (e.g., millis())
   * @return true if snapshot recorded successfully
   *
   * Behavior:
   * - Converts quaternion to Euler angles internally
   * - Formats data as JSON
   * - Appends JSON line to current snapshot file
   * - Auto-rotates to new file after MAX_SNAPSHOT_FILES
   * - Logs errors to Serial but doesn't crash
   *
   * Note:
   * - Call this from loop() when button is pressed or at desired frequency
   * - Non-blocking: file I/O is synchronous but typically fast
   */
  bool record_snapshot(const Quaternion& quat,
                       uint8_t cal_system, uint8_t cal_accel,
                       uint8_t cal_gyro, uint8_t cal_mag,
                       uint32_t timestamp_ms);

  /**
   * Record a snapshot from OrientationData structure.
   * Convenience overload for use with BNO085 sensor data.
   *
   * @param orientation OrientationData struct containing all calibration + quaternion
   * @param timestamp_ms Current timestamp (uses orientation.timestamp_ms if provided)
   * @return true if snapshot recorded successfully
   *
   * Note: This is defined in the .cpp file and requires including sensor_base.h
   */
  bool record_snapshot_from_orientation(const class OrientationData& orientation,
                                        uint32_t timestamp_ms);

  /**
   * Check if recorder is ready to record.
   * Returns false if SD card not initialized or other error state.
   *
   * @return true if ready to record
   */
  bool is_ready() const { return ready_; }

  /**
   * Get the count of snapshot files recorded so far.
   * Useful for diagnostics and UI feedback.
   *
   * @return Number of snapshots recorded (may include files from previous sessions)
   */
  uint32_t get_snapshot_count() const { return snapshot_count_; }

  /**
   * Get human-readable status string.
   * Useful for serial output and debugging.
   *
   * @return Status message (e.g., "Ready, 42 snapshots recorded")
   */
  const char* get_status_string() const { return status_string_; }

  /**
   * Manually close the current snapshot file.
   * Called automatically after initialization and during rotation.
   * Safe to call multiple times.
   *
   * @return true if successful (or file wasn't open)
   */
  bool close_current_file();

 private:
  bool ready_;
  uint32_t snapshot_count_;
  uint16_t current_file_number_;
  char current_filename_[64];
  char status_string_[128];
  SDCard sd_card_;

  /**
   * Helper: Convert Euler angles from quaternion.
   * Implements quaternion-to-Euler conversion (ZYX convention).
   */
  static void quaternion_to_euler(float qw, float qx, float qy, float qz,
                                  float& roll_deg, float& pitch_deg, float& yaw_deg);

  /**
   * Helper: Format snapshot data as JSON line.
   * Output: single-line JSON, no newline (caller adds it).
   *
   * @param snap Snapshot data to format
   * @param output Buffer to write JSON to
   * @param output_size Size of output buffer
   * @return true if formatting successful
   */
  static bool format_snapshot_json(const SnapshotData& snap,
                                   char* output, size_t output_size);

  /**
   * Helper: Find next available file number (1-MAX_SNAPSHOT_FILES).
   * Scans snapshots/ directory to find the next file to use.
   *
   * @return Next file number, or 1 if no files exist
   */
  uint16_t find_next_file_number();

  /**
   * Helper: Rotate to next file.
   * Called automatically after MAX_SNAPSHOT_FILES snapshots.
   */
  bool rotate_to_next_file();

  /**
   * Helper: Update status string based on current state.
   */
  void update_status_string();
};

#else  // !ENABLE_SNAPSHOT_RECORDER

// Stub implementation when SNAPSHOT_MODE is disabled
class SnapshotRecorder {
 public:
  SnapshotRecorder() {}
  bool initialize() { return true; }
  bool record_snapshot(const Quaternion& quat,
                       uint8_t cal_system, uint8_t cal_accel,
                       uint8_t cal_gyro, uint8_t cal_mag,
                       uint32_t timestamp_ms) { return true; }
  bool record_snapshot_from_orientation(const class OrientationData& orientation,
                                         uint32_t timestamp_ms) { return true; }
  bool is_ready() const { return false; }
  uint32_t get_snapshot_count() const { return 0; }
  const char* get_status_string() const { return "Snapshot mode disabled"; }
  bool close_current_file() { return true; }
};

#endif  // ENABLE_SNAPSHOT_RECORDER

#endif  // SNAPSHOT_RECORDER_H
