/**
 * Sensor Output Manager
 *
 * Handles orientation + position data output with frequency control.
 *
 * Key Features:
 * - Buffers BNO085 orientation updates (high frequency: ~100 Hz)
 * - Buffers GPS position updates (low frequency: ~1 Hz)
 * - JSON output format with both orientation and position
 * - Configurable output frequency
 * - Graceful handling of missing sensors
 *
 * Data Flow:
 * 1. Orientation updates at ~100 Hz (BNO085)
 * 2. Position updates at ~1 Hz (GPS)
 * 3. Output ready when:
 *    - Interval elapsed + orientation data available
 *    - Position data included if available (not required for output)
 *
 * Example Usage:
 * ```cpp
 * SensorOutputManager output;
 * output.begin(OutputFormat::JSON);
 *
 * loop {
 *   if (bno.read()) {
 *     output.updateOrientation(bno.getOrientation());
 *   }
 *   if (gps.read()) {
 *     output.updatePosition(gps.getPosition());
 *   }
 *   if (output.shouldOutput()) {
 *     char buffer[512];
 *     output.getFormattedOutput(buffer, sizeof(buffer));
 *     Serial.println(buffer);
 *   }
 * }
 * ```
 */

#ifndef SENSOR_OUTPUT_MANAGER_H
#define SENSOR_OUTPUT_MANAGER_H

#include <stdint.h>
#include "sensors/sensor_base.h"

// ============================================================================
// EKF Fused State Structure (16-dimensional)
// ============================================================================

/**
 * EKF Fused State Structure
 *
 * Represents the complete estimated state from the Extended Kalman Filter:
 * [0-3]:   Quaternion q = [w, x, y, z]
 * [4-6]:   Velocity v_ned = [v_north, v_east, v_down] (m/s)
 * [7-9]:   Position p_ned = [p_north, p_east, p_down] (meters)
 * [10-12]: Accelerometer bias (m/s²)
 * [13-15]: Gyroscope bias (rad/s)
 */
struct EKFState {
  // Quaternion (orientation)
  float q_w, q_x, q_y, q_z;

  // Velocity (NED, m/s)
  float v_north, v_east, v_down;

  // Position (NED, meters)
  float p_north, p_east, p_down;

  // Accelerometer bias (m/s²)
  float bias_accel_x, bias_accel_y, bias_accel_z;

  // Gyroscope bias (rad/s)
  float bias_gyro_x, bias_gyro_y, bias_gyro_z;

  EKFState() :
    q_w(1.0f), q_x(0.0f), q_y(0.0f), q_z(0.0f),
    v_north(0.0f), v_east(0.0f), v_down(0.0f),
    p_north(0.0f), p_east(0.0f), p_down(0.0f),
    bias_accel_x(0.0f), bias_accel_y(0.0f), bias_accel_z(0.0f),
    bias_gyro_x(0.0f), bias_gyro_y(0.0f), bias_gyro_z(0.0f) {}
};

/**
 * EKF Covariance Uncertainty (diagonal elements only)
 * Represents standard deviation (not variance) for human-readable output
 */
struct EKFUncertainty {
  // Attitude uncertainty (rad)
  float attitude_rad;

  // Velocity uncertainty (m/s)
  float velocity_mps;

  // Position uncertainty (m)
  float position_m;

  // Accel bias uncertainty (m/s²)
  float accel_bias_mps2;

  // Gyro bias uncertainty (rad/s)
  float gyro_bias_rads;

  EKFUncertainty() :
    attitude_rad(1.0f), velocity_mps(10.0f),
    position_m(100.0f), accel_bias_mps2(0.5f),
    gyro_bias_rads(0.05f) {}
};

/**
 * GPS Status Information for fused output
 */
struct GPSStatus {
  bool locked;                        // GPS has valid fix
  uint8_t satellites;                 // Number of satellites used
  float hdop;                         // Horizontal dilution of precision
  bool dropout;                       // GPS currently in dropout
  uint16_t age_ms;                    // Age of last GPS measurement
  bool dead_reckoning_valid;          // Dead reckoning mode active
  uint16_t dead_reckoning_age_ms;     // Time since last GPS update

  GPSStatus() :
    locked(false), satellites(0), hdop(0.0f),
    dropout(false), age_ms(0),
    dead_reckoning_valid(false), dead_reckoning_age_ms(0) {}
};

/**
 * EKF Health Metrics
 */
struct EKFHealth {
  float covariance_trace;             // Trace of covariance matrix P
  float innovation_magnitude;         // |z - H*x| for last measurement
  uint32_t num_updates;               // Total number of updates

  EKFHealth() :
    covariance_trace(0.0f), innovation_magnitude(0.0f), num_updates(0) {}
};

enum class OutputFormat {
  JSON           // JSON format: {"timestamp": ..., "orientation": {...}, "position": {...}}
};

class SensorOutputManager {
 public:
  SensorOutputManager();

  /**
   * Initialize output manager with JSON format.
   *
   * @param format OutputFormat::JSON
   * @return true if initialization successful
   */
  bool begin(OutputFormat format);

  /**
   * Set output format (can change at runtime).
   *
   * @param format Output format to use
   */
  void setFormat(OutputFormat format);

  /**
   * Set output frequency in Hz (default: 10 Hz).
   * Controls how often shouldOutput() returns true.
   *
   * @param hz Frequency in Hz (1-10 typical)
   */
  void setFrequencyHz(float hz);

  /**
   * Process new orientation data.
   *
   * Updates internal orientation buffer and timestamp.
   * Marks orientation as valid.
   *
   * @param orient OrientationData from BNO085
   */
  void updateOrientation(const OrientationData& orient);

  /**
   * Process new position data (legacy compatibility).
   *
   * Updates internal orientation buffer. Deprecated; use updateOrientation() instead.
   *
   * @param orient OrientationData from BNO085
   */
  void update(const OrientationData& orient) {
    updateOrientation(orient);
  }

  /**
   * Process new position data.
   *
   * Updates internal position buffer and timestamp.
   * Marks position as valid. Position updates are optional.
   *
   * @param position PositionData from GPS
   */
  void updatePosition(const PositionData& position);

  /**
   * Check if ready to output based on frequency control.
   *
   * Returns true when enough time has elapsed since last output
   * and valid orientation data is available. Position data is optional.
   *
   * @return true if output should be generated now
   */
  bool shouldOutput();

  /**
   * Get next formatted JSON output as a string.
   *
   * Includes orientation and position data (if available).
   * Updates last output timestamp on success.
   *
   * Buffer must be at least 512 bytes for combined JSON output.
   * For orientation-only: 256 bytes typical.
   * For full orientation+position: 512+ bytes recommended.
   *
   * @param buffer Destination buffer for output string
   * @param max_len Maximum bytes to write (including null terminator)
   * @return Number of bytes written (excluding null terminator), 0 on error
   */
  uint16_t getFormattedOutput(char* buffer, uint16_t max_len);

  /**
   * Format orientation data only as JSON.
   *
   * Useful for testing or orientation-only output mode.
   *
   * @param buffer Destination buffer
   * @param max_len Maximum bytes to write
   * @return Number of bytes written, 0 on error
   */
  uint16_t formatOrientationJSON(char* buffer, uint16_t max_len);

  /**
   * Format position data only as JSON.
   *
   * Useful for testing or position-only output mode.
   *
   * @param buffer Destination buffer
   * @param max_len Maximum bytes to write
   * @return Number of bytes written, 0 on error
   */
  uint16_t formatPositionJSON(char* buffer, uint16_t max_len);

  /**
   * Format combined orientation + position JSON.
   *
   * Includes both data sections, with validity flags.
   * Handles cases where one or both sensors unavailable.
   *
   * @param buffer Destination buffer
   * @param max_len Maximum bytes to write
   * @return Number of bytes written, 0 on error
   */
  uint16_t formatFullJSON(char* buffer, uint16_t max_len);

  /**
   * Get current output format.
   *
   * @return Current OutputFormat setting
   */
  OutputFormat getOutputFormat() const;

  /**
   * Get orientation validity status.
   *
   * @return true if orientation data is valid
   */
  bool hasValidOrientation() const { return orientation_valid_; }

  /**
   * Get position validity status.
   *
   * @return true if position data is valid
   */
  bool hasValidPosition() const { return position_valid_; }

  // ========================================================================
  // EKF Fused State Methods (Phase 3)
  // ========================================================================

  /**
   * Update with EKF fused state and covariance.
   *
   * Called after each EKF predict/update cycle to store the fused estimate.
   * Covariance is used to extract uncertainty estimates.
   *
   * @param state 16-element EKF state vector [q, v, p, bias_accel, bias_gyro]
   * @param covariance 16x16 covariance matrix P (row-major storage)
   * @param gps_status Current GPS/dead-reckoning status
   * @param ekf_health EKF health metrics
   */
  void updateFusedState(const EKFState& state, const float* covariance_16x16,
                        const GPSStatus& gps_status, const EKFHealth& ekf_health);

  /**
   * Format fused state as JSON.
   *
   * Includes quaternion, Euler angles, velocity, position, and uncertainty.
   * Gracefully handles missing data.
   *
   * @param buffer Destination buffer
   * @param max_len Maximum bytes to write
   * @return Number of bytes written, 0 on error
   */
  uint16_t formatFusedStateJSON(char* buffer, uint16_t max_len);

  /**
   * Format comparison JSON (raw GPS vs fused position).
   *
   * Used for validation and filter tuning. Shows differences and RMS error.
   * Only meaningful when both GPS and fused position are available.
   *
   * @param buffer Destination buffer
   * @param max_len Maximum bytes to write
   * @return Number of bytes written, 0 on error
   */
  uint16_t formatComparisonJSON(char* buffer, uint16_t max_len);

  /**
   * Check if fused state data is valid and ready to output.
   *
   * @return true if fused state has been updated
   */
  bool hasValidFusedState() const { return fused_valid_; }

  /**
   * Get EKF fused state (for direct access).
   *
   * @return Reference to current fused state
   */
  const EKFState& getFusedState() const { return fused_state_; }

 private:
  OutputFormat format_;
  float frequency_hz_;
  uint32_t output_interval_ms_;
  uint32_t last_output_ms_;

  // Latest sensor data
  OrientationData orientation_;
  PositionData position_;
  uint32_t orientation_timestamp_ms_;
  uint32_t position_timestamp_ms_;

  // Data validity flags
  bool orientation_valid_;
  bool position_valid_;

  // EKF Fused State (Phase 3)
  EKFState fused_state_;
  EKFUncertainty fused_uncertainty_;
  GPSStatus gps_status_;
  EKFHealth ekf_health_;
  uint32_t fused_timestamp_ms_;
  bool fused_valid_;

  // For comparison JSON (raw vs fused)
  PositionData raw_gps_position_;
  float fused_position_error_m_;

  // Internal helpers
  uint16_t formatJSON(char* buffer, uint16_t max_len);
  uint16_t quaternionToEuler(float q_w, float q_x, float q_y, float q_z,
                              float* roll_deg, float* pitch_deg, float* yaw_deg);
};

#endif  // SENSOR_OUTPUT_MANAGER_H
