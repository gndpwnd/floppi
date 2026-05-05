/**
 * JSON Output Formatter
 *
 * Formats orientation + position data as JSON with the following structure:
 *
 * {
 *   "timestamp_ms": 123456,
 *   "orientation": {
 *     "valid": true,
 *     "quaternion": {"w": 0.707, "x": 0.0, "y": 0.0, "z": 0.707},
 *     "calibration": {"system": 3, "accel": 3, "gyro": 3, "mag": 2}
 *   },
 *   "position": {
 *     "valid": false,
 *     "latitude": null,
 *     "longitude": null,
 *     "altitude_m": null,
 *     "accuracy_m": null,
 *     "num_satellites": 0,
 *     "fix_quality": 0
 *   }
 * }
 *
 * Features:
 * - Handles invalid sensors gracefully (null values, valid:false)
 * - Compact JSON (no unnecessary whitespace)
 * - Configurable decimal precision for float values (default 3)
 */

#ifndef JSON_FORMATTER_H
#define JSON_FORMATTER_H

#include <stdint.h>
#include "data_formatter.h"

class JSONFormatter : public DataFormatter {
 public:
  /**
   * Constructor
   *
   * @param decimal_places Decimal places for float values (default 3)
   */
  JSONFormatter(uint8_t decimal_places = 3);

  virtual ~JSONFormatter() {}

  /**
   * Format sensor data as JSON string
   *
   * Handles the following cases:
   * - Both orientation and position valid: full JSON with both sections
   * - Only orientation valid: orientation section included, position.valid = false
   * - Only position valid: position section included, orientation.valid = false
   * - Neither valid: both sections present with valid = false
   *
   * @param orientation Orientation data
   * @param position    Position data
   * @param buffer      Output buffer
   * @param max_len     Maximum buffer length
   *
   * @return Number of bytes written (excluding null terminator)
   */
  uint16_t format(const OrientationData& orientation,
                  const PositionData& position,
                  char* buffer,
                  uint16_t max_len) override;

  const char* getFormatName() const override { return "JSON"; }

 private:
  uint8_t decimal_places_;

  /**
   * Helper: write float value with specified decimal places
   *
   * @param buffer      Output buffer
   * @param remaining   Remaining space in buffer
   * @param value       Float value to write
   *
   * @return Number of characters written
   */
  uint16_t writeFloat(char* buffer, uint16_t remaining, float value);

  /**
   * Helper: write double value with specified decimal places
   *
   * @param buffer      Output buffer
   * @param remaining   Remaining space in buffer
   * @param value       Double value to write
   *
   * @return Number of characters written
   */
  uint16_t writeDouble(char* buffer, uint16_t remaining, double value);
};

#endif  // JSON_FORMATTER_H
