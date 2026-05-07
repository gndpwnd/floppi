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

  // Internal helpers
  uint16_t formatJSON(char* buffer, uint16_t max_len);
};

#endif  // SENSOR_OUTPUT_MANAGER_H
