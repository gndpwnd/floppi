/**
 * Data Formatter Interface
 *
 * Abstract interface for formatting sensor data in JSON format.
 * Each formatter is responsible for converting combined orientation + position
 * data into a string representation.
 *
 * Usage:
 * ```cpp
 * JSONFormatter json_fmt;
 * char buffer[512];
 * json_fmt.format(orientation, position, buffer, sizeof(buffer));
 * Serial.println(buffer);
 * ```
 */

#ifndef DATA_FORMATTER_H
#define DATA_FORMATTER_H

#include <stdint.h>
#include "../sensors/sensor_base.h"

/**
 * Abstract base class for data formatters
 */
class DataFormatter {
 public:
  virtual ~DataFormatter() {}

  /**
   * Format sensor data into string output
   *
   * @param orientation Orientation data (may be invalid)
   * @param position    Position data (may be invalid)
   * @param buffer      Output buffer for formatted string
   * @param max_len     Maximum buffer length
   *
   * @return Number of bytes written to buffer (excluding null terminator),
   *         or 0 if formatting failed
   */
  virtual uint16_t format(const OrientationData& orientation,
                          const PositionData& position,
                          char* buffer,
                          uint16_t max_len) = 0;

  /**
   * Get human-readable format name
   *
   * @return Format name (e.g., "JSON")
   */
  virtual const char* getFormatName() const = 0;
};

#endif  // DATA_FORMATTER_H
