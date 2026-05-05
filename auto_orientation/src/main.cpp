/**
 * Auto Orientation: BNO085 + GPS Integration
 *
 * Reads absolute orientation (quaternions) from BNO085 IMU and
 * position (lat/lon/alt) from NEO-M9N GPS, combines into single
 * data stream with persistent calibration.
 *
 * Hardware:
 * - BNO085 (Adafruit) via UART (RX/TX pins configured in config/pins.h)
 * - NEO-M9N GPS via USB serial (auto-detected)
 * - Arduino-compatible board (Nano, Mega, Teensy, ESP32, etc.)
 *
 * See docs/README.md for usage and wiring guide.
 */

#include <Arduino.h>
#include "config/pins.h"
#include "sensors/sensor_base.h"
#include "output/serial_output.h"

// Sensor instances
#include "sensors/bno085.h"

BNO085 imu;
uint32_t last_output_ms = 0;
const uint32_t output_interval_ms = 1000 / SERIAL_OUTPUT_FREQUENCY_HZ;  // 100ms for 10 Hz

void setup() {
  Serial.begin(115200);

  // Wait for serial connection
  while (!Serial) {
    delay(100);
  }

  Serial.println("\n=== Auto Orientation System ===");
  Serial.println("Board: Initializing BNO085 sensor...");

  // Initialize BNO085 sensor
  if (!imu.begin()) {
    Serial.println("ERROR: BNO085 initialization failed!");
    Serial.println("Check UART connections and pins.h configuration.");
    while (1) {
      delay(500);
      Serial.println("Waiting for manual reset...");
    }
  }

  Serial.println("BNO085 initialized successfully!");
  Serial.println("Reading orientation data...\n");
}

void loop() {
  // Read from BNO085
  if (imu.read()) {
    // Successfully read new sensor data
  }

  // Output at configured frequency
  uint32_t now_ms = millis();
  if (now_ms - last_output_ms >= output_interval_ms) {
    last_output_ms = now_ms;

    if (imu.hasNewData()) {
      // Get latest orientation data
      const OrientationData& orientation = imu.getOrientation();

      // Compute quaternion magnitude for validation
      float q_mag = sqrt(orientation.w * orientation.w +
                         orientation.x * orientation.x +
                         orientation.y * orientation.y +
                         orientation.z * orientation.z);

      // Output format: timestamp | quat(w,x,y,z) | magnitude | calibration status
      Serial.print(now_ms);
      Serial.print(" | Q: ");
      Serial.print(orientation.w, 4);
      Serial.print(",");
      Serial.print(orientation.x, 4);
      Serial.print(",");
      Serial.print(orientation.y, 4);
      Serial.print(",");
      Serial.print(orientation.z, 4);
      Serial.print(" | Mag: ");
      Serial.print(q_mag, 4);
      Serial.print(" | Status: ");
      Serial.println(imu.getStatusString());
    } else {
      Serial.println("No new data from BNO085");
    }
  }
}
