/**
 * Auto Orientation: BNO085 IMU
 *
 * Reads absolute orientation (quaternions) from BNO085 IMU
 * with persistent calibration.
 *
 * Hardware:
 * - BNO085 (Adafruit) via UART (RX/TX pins configured in config/pins.h)
 * - Arduino-compatible board (Nano, Mega, Teensy, ESP32, etc.)
 *
 * See docs/README.md for usage and wiring guide.
 */

#include <Arduino.h>
#include "config/pins.h"
#include "sensors/sensor_base.h"
#include "output/sensor_output_manager.h"

// Sensor instances
#include "sensors/bno085.h"

BNO085 imu;
SensorOutputManager output_manager;

void setup() {
  Serial.begin(115200);

  // Wait for serial connection
  while (!Serial) {
    delay(100);
  }

  Serial.println("\n=== Auto Orientation System ===");
  Serial.println("Initializing sensors...");

  // Initialize BNO085 sensor
  Serial.println("Board: Initializing BNO085 IMU sensor...");
  if (!imu.begin()) {
    Serial.println("ERROR: BNO085 initialization failed!");
    Serial.println("Check UART connections and pins.h configuration.");
    while (1) {
      delay(500);
      Serial.println("  (waiting for manual reset)");
    }
  }
  Serial.println("BNO085 OK");

  // Initialize output manager (JSON-only format)
  Serial.println("Board: Initializing output manager...");
  if (!output_manager.begin(OutputFormat::JSON)) {
    Serial.println("ERROR: Output manager initialization failed!");
    while (1) {
      delay(500);
      Serial.println("  (waiting for manual reset)");
    }
  }
  output_manager.setFrequencyHz(10.0f);  // 10 Hz output frequency

  Serial.println("Output Manager: JSON format (v1.0), 10 Hz frequency");
  Serial.println("Reading sensor data...\n");
}

void loop() {
  // Read orientation from IMU
  if (imu.read()) {
    if (imu.hasNewData()) {
      // Update output manager with new orientation data
      output_manager.update(imu.getOrientation());
    }
  }

  // Output when ready (frequency controlled by output_manager)
  if (output_manager.shouldOutput()) {
    char buffer[256];
    uint16_t len = output_manager.getFormattedOutput(buffer, sizeof(buffer));

    if (len > 0) {
      Serial.println(buffer);
    } else {
      Serial.println("ERROR: Failed to format output");
    }
  }
}
