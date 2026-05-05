/**
 * Sensor Output Manager
 *
 * Handles orientation data output with frequency control.
 *
 * Key Features:
 * - Buffers BNO085 orientation updates
 * - JSON output format
 * - Configurable output frequency
 *
 * Data Flow:
 * 1. Orientation updates at 10 Hz (100 ms intervals)
 * 2. Output ready when:
 *    - Orientation data + interval elapsed
 *    - Output orientation data only
 *
 * Example Usage:
 * ```cpp
 * SensorOutputManager output;
 * output.begin(OutputFormat::JSON);
 *
 * loop {
 *   if (bno.read()) {
 *     output.update(bno.getOrientation());
 *   }
 *   if (output.shouldOutput()) {
 *     char buffer[256];
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
  JSON           // JSON format: {"timestamp": 1000, "orientation": {...}}
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
  void update(const OrientationData& orient);

  /**
   * Check if ready to output based on frequency control.
   *
   * Returns true when enough time has elapsed since last output
   * and valid orientation data is available.
   *
   * @return true if output should be generated now
   */
  bool shouldOutput();

  /**
   * Get next formatted JSON output as a string.
   *
   * Includes orientation data only.
   * Updates last output timestamp on success.
   *
   * Buffer must be at least 256 bytes for typical JSON output.
   *
   * @param buffer Destination buffer for output string
   * @param max_len Maximum bytes to write (including null terminator)
   * @return Number of bytes written (excluding null terminator), 0 on error
   */
  uint16_t getFormattedOutput(char* buffer, uint16_t max_len);

  /**
   * Get current output format.
   *
   * @return Current OutputFormat setting
   */
  OutputFormat getOutputFormat() const;


 private:
  OutputFormat format_;
  float frequency_hz_;
  uint32_t output_interval_ms_;
  uint32_t last_output_ms_;

  // Latest sensor data
  OrientationData orientation_;
  uint32_t orientation_timestamp_ms_;

  // Data validity flags
  bool orientation_valid_;

  // Internal helpers
  uint16_t formatJSON(char* buffer, uint16_t max_len);
};

#endif  // SENSOR_OUTPUT_MANAGER_H
