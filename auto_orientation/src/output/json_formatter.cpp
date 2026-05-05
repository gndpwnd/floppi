/**
 * JSON Output Formatter Implementation
 */

#include "json_formatter.h"
#include <stdio.h>
#include <string.h>

JSONFormatter::JSONFormatter(uint8_t decimal_places)
    : decimal_places_(decimal_places) {}

uint16_t JSONFormatter::writeFloat(char* buffer, uint16_t remaining,
                                   float value) {
  if (remaining < 2) return 0;

  // Create format string dynamically based on decimal_places
  char fmt[8];
  snprintf(fmt, sizeof(fmt), "%%.%df", decimal_places_);

  int written = snprintf(buffer, remaining, fmt, value);
  if (written < 0 || written >= (int)remaining) {
    return 0;
  }
  return written;
}

uint16_t JSONFormatter::writeDouble(char* buffer, uint16_t remaining,
                                    double value) {
  if (remaining < 2) return 0;

  // Create format string dynamically based on decimal_places
  char fmt[8];
  snprintf(fmt, sizeof(fmt), "%%.%dlf", decimal_places_);

  int written = snprintf(buffer, remaining, fmt, value);
  if (written < 0 || written >= (int)remaining) {
    return 0;
  }
  return written;
}

uint16_t JSONFormatter::format(const OrientationData& orientation,
                               const PositionData& position, char* buffer,
                               uint16_t max_len) {
  if (!buffer || max_len < 10) {
    return 0;
  }

  char* ptr = buffer;
  uint16_t remaining = max_len;

#define WRITE_STR(s)                          \
  do {                                        \
    uint16_t len = strlen(s);                 \
    if (len > remaining) return 0;            \
    memcpy(ptr, s, len);                      \
    ptr += len;                               \
    remaining -= len;                         \
  } while (0)

#define WRITE_INT(val)                                \
  do {                                              \
    int written = snprintf(ptr, remaining, "%d", val); \
    if (written < 0 || written >= (int)remaining) {   \
      return 0;                                       \
    }                                                 \
    ptr += written;                                   \
    remaining -= written;                             \
  } while (0)

#define WRITE_FLOAT(val)                           \
  do {                                            \
    uint16_t written = writeFloat(ptr, remaining, val); \
    if (written == 0) return 0;                    \
    ptr += written;                               \
    remaining -= written;                         \
  } while (0)

#define WRITE_DOUBLE(val)                           \
  do {                                            \
    uint16_t written = writeDouble(ptr, remaining, val); \
    if (written == 0) return 0;                    \
    ptr += written;                               \
    remaining -= written;                         \
  } while (0)

  // Start JSON object
  WRITE_STR("{\"timestamp_ms\":");
  WRITE_INT(orientation.timestamp_ms);

  // Orientation section
  WRITE_STR(",\"orientation\":{\"valid\":");
  bool orient_valid =
      (orientation.w != 0 || orientation.x != 0 || orientation.y != 0 ||
       orientation.z != 0);
  WRITE_STR(orient_valid ? "true" : "false");

  WRITE_STR(",\"quaternion\":{\"w\":");
  WRITE_FLOAT(orientation.w);
  WRITE_STR(",\"x\":");
  WRITE_FLOAT(orientation.x);
  WRITE_STR(",\"y\":");
  WRITE_FLOAT(orientation.y);
  WRITE_STR(",\"z\":");
  WRITE_FLOAT(orientation.z);
  WRITE_STR("},\"calibration\":{\"system\":");
  WRITE_INT(orientation.cal_status);
  WRITE_STR(",\"accel\":");
  WRITE_INT(orientation.cal_accel);
  WRITE_STR(",\"gyro\":");
  WRITE_INT(orientation.cal_gyro);
  WRITE_STR(",\"mag\":");
  WRITE_INT(orientation.cal_mag);
  WRITE_STR("}}");

  // Position section
  WRITE_STR(",\"position\":{\"valid\":");
  bool pos_valid = (position.fix_quality > 0);
  WRITE_STR(pos_valid ? "true" : "false");

  WRITE_STR(",\"latitude\":");
  if (pos_valid) {
    WRITE_DOUBLE(position.latitude);
  } else {
    WRITE_STR("null");
  }

  WRITE_STR(",\"longitude\":");
  if (pos_valid) {
    WRITE_DOUBLE(position.longitude);
  } else {
    WRITE_STR("null");
  }

  WRITE_STR(",\"altitude_m\":");
  if (pos_valid) {
    WRITE_FLOAT(position.altitude);
  } else {
    WRITE_STR("null");
  }

  WRITE_STR(",\"accuracy_m\":");
  if (pos_valid) {
    WRITE_FLOAT(position.accuracy_m);
  } else {
    WRITE_STR("null");
  }

  WRITE_STR(",\"num_satellites\":");
  WRITE_INT(position.num_satellites);
  WRITE_STR(",\"fix_quality\":");
  WRITE_INT(position.fix_quality);
  WRITE_STR("}}");

  // Null-terminate
  if (remaining < 1) return 0;
  *ptr = '\0';

  return max_len - remaining;

#undef WRITE_STR
#undef WRITE_INT
#undef WRITE_FLOAT
#undef WRITE_DOUBLE
}
