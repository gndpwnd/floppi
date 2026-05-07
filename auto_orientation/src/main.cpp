/**
 * Auto Orientation: BNO085 IMU + Persistent Calibration
 *
 * Reads absolute orientation (quaternions) from BNO085 IMU with auto-save
 * calibration to EEPROM. On boot, loads previously saved calibration.
 *
 * Hardware:
 * - BNO085 (Adafruit) via I2C (SDA pin 20, SCL pin 21 on Arduino Mega)
 * - Outputs JSON with orientation + calibration status
 *
 * Calibration:
 * - Auto-saves to EEPROM when level 2+ (medium or higher)
 * - Auto-loads on boot if previously saved
 * - Survives code uploads, power cycles, orientation changes
 *
 * See docs/CALIBRATION_GUIDE.md for complete user guide.
 * See docs/ABSOLUTE_ORIENTATION_EXPLAINED.md for theory.
 * See docs/GETTING_STARTED.md for setup instructions.
 */

#include <Arduino.h>
#include "config/pins.h"
#include "config/mode.h"
#include "sensors/sensor_base.h"
#include "output/sensor_output_manager.h"
#include "sensors/bno085.h"

BNO085 imu;
SensorOutputManager output_manager;

void setup() {
  Serial.begin(115200);

  while (!Serial) {
    delay(100);
  }

  // Boot message (calibration mode is verbose, production is minimal)
  if (IS_CALIBRATION_MODE) {
    Serial.println("\n==================================================");
    Serial.println("Auto Orientation System [CALIBRATION MODE]");
    Serial.println("==================================================");
    Serial.println("Initializing sensors...\n");
  } else {
    Serial.println("\nAuto Orientation [PRODUCTION]");
  }

  // Initialize BNO085 sensor
  CAL_PRINTLN("Board: Initializing BNO085 IMU sensor...");
  if (!imu.begin()) {
    ALWAYS_PRINTLN("ERROR: BNO085 initialization failed!");
    while (1) {
      delay(500);
      ALWAYS_PRINTLN("  (waiting for manual reset)");
    }
  }
  CAL_PRINTLN("✓ BNO085 OK\n");

  // Initialize output manager (JSON-only format)
  CAL_PRINTLN("Board: Initializing output manager...");
  if (!output_manager.begin(OutputFormat::JSON)) {
    ALWAYS_PRINTLN("ERROR: Output manager initialization failed!");
    while (1) {
      delay(500);
      ALWAYS_PRINTLN("  (waiting for manual reset)");
    }
  }

  // Boot complete message
  if (IS_CALIBRATION_MODE) {
    Serial.println("✓ All sensors initialized successfully!\n");
    Serial.println("==================================================");
    Serial.println("Calibration progress indicator:");
    Serial.println("  ░░░ = Uncalibrated (0)");
    Serial.println("  █░░ = Low (1)");
    Serial.println("  ██░ = Medium (2) <-- Auto-saves");
    Serial.println("  ███ = High (3)");
    Serial.println("==================================================\n");
  } else {
    Serial.println("✓ Ready");
  }
}

void loop() {
  // Read sensor data
  if (!imu.read()) {
    return;
  }

  if (!imu.hasNewData()) {
    return;
  }

  // Update output manager with latest orientation
  output_manager.update(imu.getOrientation());

  // Output when ready (frequency-controlled to ~10 Hz)
  if (output_manager.shouldOutput()) {
    char buffer[512];
    uint16_t len = output_manager.getFormattedOutput(buffer, sizeof(buffer));
    if (len > 0) {
      Serial.println(buffer);
    }
  }
}
