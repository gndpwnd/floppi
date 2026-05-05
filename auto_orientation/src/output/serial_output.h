/**
 * Serial Output Handler
 *
 * Formats and streams sensor data over serial (USB/UART).
 * Supports multiple output formats (JSON, CSV, delimited text).
 */

#ifndef SERIAL_OUTPUT_H
#define SERIAL_OUTPUT_H

#include "sensors/sensor_base.h"

enum class OutputFormat {
  JSON,           // JSON format: {"timestamp": 1000, "quat": {...}, "gps": {...}}
  CSV_HEADER,     // CSV with header row
  CSV_DATA,       // CSV data rows only
  DELIMITED       // Space/comma delimited (simple)
};

class SerialOutput {
 public:
  SerialOutput(OutputFormat format = OutputFormat::JSON);

  // Configuration
  void setFormat(OutputFormat format);
  void setFrequencyHz(float hz);

  // Output methods
  void printHeader();
  void printSensorOutput(const SensorOutput& output);
  void printStatus(const char* status_message);
  void printDebug(const char* debug_message);

 private:
  OutputFormat format_;
  float frequency_hz_;
  uint32_t last_output_ms_;
  uint32_t sample_count_;

  // Format-specific methods
  void printJSON(const SensorOutput& output);
  void printCSV(const SensorOutput& output, bool header = false);
  void printDelimited(const SensorOutput& output);

  // Helper: check if enough time has passed for output frequency
  bool shouldOutput();
};

#endif  // SERIAL_OUTPUT_H
