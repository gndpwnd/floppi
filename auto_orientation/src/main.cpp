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

#if ENABLE_SNAPSHOT_RECORDER
#include "features/snapshot_recorder.h"
#include "sensors/button_input.h"
#endif

BNO085 imu;
SensorOutputManager output_manager;

#if ENABLE_SNAPSHOT_RECORDER
SnapshotRecorder snapshot_recorder;
ButtonInput button_input;
#endif

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

#if ENABLE_SNAPSHOT_RECORDER
  // Initialize snapshot recorder (if SNAPSHOT_MODE enabled)
  CAL_PRINTLN("Board: Initializing snapshot recorder...");
  if (!snapshot_recorder.initialize()) {
    CAL_PRINTLN("WARNING: Snapshot recorder initialization failed");
    // Non-fatal: continue without snapshot feature
  } else {
    CAL_PRINTLN("✓ Snapshot recorder OK");
  }

  // Initialize button input (if SNAPSHOT_MODE enabled)
  CAL_PRINTLN("Board: Initializing button input...");
  if (!button_input.initialize()) {
    CAL_PRINTLN("WARNING: Button input initialization failed");
    // Non-fatal: continue without button feature
  } else {
    CAL_PRINTLN("✓ Button input OK");
  }
#endif

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

  // Get current orientation data
  const OrientationData& orientation = imu.getOrientation();

  // Update output manager with latest orientation
  output_manager.update(orientation);

#if ENABLE_SNAPSHOT_RECORDER
  // Check for button press and record snapshot if triggered
  if (button_input.is_pressed()) {
    if (snapshot_recorder.is_ready()) {
      if (snapshot_recorder.record_snapshot_from_orientation(orientation, millis())) {
        Serial.printf("✓ Snapshot #%lu recorded\n", snapshot_recorder.get_snapshot_count());
      } else {
        Serial.println("ERROR: Failed to record snapshot");
      }
    } else {
      Serial.println("ERROR: Snapshot recorder not ready");
    }
  }
#endif

  // Output when ready (frequency-controlled to ~10 Hz)
  if (output_manager.shouldOutput()) {
    char buffer[512];
    uint16_t len = output_manager.getFormattedOutput(buffer, sizeof(buffer));
    if (len > 0) {
      Serial.println(buffer);
    }
  }
}
