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
#include "sensors/neo_m9n.h"

BNO085 imu;
NEOM9N gps;
uint32_t last_output_ms = 0;
const uint32_t output_interval_ms = 1000 / SERIAL_OUTPUT_FREQUENCY_HZ;  // 100ms for 10 Hz

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

  // Initialize NEO-M9N GPS sensor
  Serial.println("Board: Initializing NEO-M9N GPS sensor...");
  if (!gps.begin()) {
    Serial.println("ERROR: NEO-M9N initialization failed!");
    Serial.println("Check USB serial connection and GPS_BAUD_RATE in pins.h.");
    while (1) {
      delay(500);
      Serial.println("  (waiting for manual reset)");
    }
  }
  Serial.println("NEO-M9N initialized successfully!");
  Serial.println("Reading sensor data...\n");
}

void loop() {
  // Read from both sensors
  if (imu.read()) {
    // Successfully read new IMU data
  }

  if (gps.read()) {
    // Successfully read new GPS data
  }

  // Output at configured frequency
  uint32_t now_ms = millis();
  if (now_ms - last_output_ms >= output_interval_ms) {
    last_output_ms = now_ms;

    // Output IMU orientation data
    if (imu.hasNewData()) {
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
      Serial.print(" | IMU: ");
      Serial.print(imu.getStatusString());
      Serial.print(" | GPS: ");
      Serial.println(gps.getStatusString());
    } else {
      Serial.print(now_ms);
      Serial.print(" | No IMU data | GPS: ");
      Serial.println(gps.getStatusString());
    }

    // Optionally output GPS position data when available
    if (gps.hasNewData()) {
      const PositionData& position = gps.getPosition();
      Serial.print("  -> GPS Position: ");
      Serial.print(position.latitude, 6);
      Serial.print(", ");
      Serial.print(position.longitude, 6);
      Serial.print(" (alt: ");
      Serial.print(position.altitude, 1);
      Serial.print("m, acc: ");
      Serial.print(position.accuracy_m, 1);
      Serial.println("m)");
    }
  }
}
