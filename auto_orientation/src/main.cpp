/**
 * Auto Orientation: BNO085 IMU + GPS + Persistent Calibration
 *
 * Reads absolute orientation (quaternions) from BNO085 IMU with auto-save
 * calibration to EEPROM. On boot, loads previously saved calibration.
 * Integrates GPS position data for combined orientation+position output.
 *
 * Hardware:
 * - BNO085 (Adafruit) via I2C (SDA pin 20, SCL pin 21 on Arduino Mega)
 * - GPS (NEO-M9N or similar) via UART Serial1 (Pins 18/19 on Arduino Mega)
 * - Outputs JSON with orientation + position + calibration status
 *
 * Sensor Rates:
 * - BNO085: ~100 Hz (1 sample per ~10 ms)
 * - GPS: ~1 Hz (1 sample per ~1000 ms), configurable
 * - Output: ~10 Hz (100 ms intervals), configurable
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
#include "sensors/gps.h"
#include "math/coordinates.h"

#if ENABLE_SNAPSHOT_RECORDER
#include "features/snapshot_recorder.h"
#include "sensors/button_input.h"
#endif

BNO085 imu;
GPS gps;
SensorOutputManager output_manager;

// Coordinate frame reference point (initialized on first GPS fix)
ECEF coordinate_frame_origin_ecef;
double coordinate_frame_lat_rad;
bool coordinate_frame_initialized = false;

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

  // Initialize GPS sensor (non-fatal if it fails)
  CAL_PRINTLN("Board: Initializing GPS sensor (Serial1)...");
  if (!gps.begin(9600)) {
    CAL_PRINTLN("WARNING: GPS initialization failed (continuing without GPS)");
    // Non-fatal: continue with orientation-only output
  } else {
    CAL_PRINTLN("✓ GPS OK (waiting for fix...)\n");
  }

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
  // ========================================================================
  // Read BNO085 Orientation (high frequency: ~100 Hz)
  // ========================================================================
  if (imu.read() && imu.hasNewData()) {
    const OrientationData& orientation = imu.getOrientation();
    output_manager.updateOrientation(orientation);

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
  }

  // ========================================================================
  // Read GPS Position (low frequency: ~1 Hz, non-blocking)
  // ========================================================================
  if (gps.isInitialized() && gps.read() && gps.hasNewData()) {
    const PositionData& position = gps.getPosition();

    // Initialize coordinate frame on first valid GPS fix
    if (!coordinate_frame_initialized && position.fix_quality >= 1) {
      coordinate_frame_origin_ecef = gps_to_ecef(
          position.latitude, position.longitude, position.altitude);
      coordinate_frame_lat_rad = position.latitude * 3.14159265359 / 180.0;
      coordinate_frame_initialized = true;

      if (IS_CALIBRATION_MODE) {
        CAL_PRINTLN("GPS: CoordinateFrame initialized (first fix acquired)");
      }
    }

    // Update output manager with latest position
    output_manager.updatePosition(position);

    // Debug output in calibration mode
    if (IS_CALIBRATION_MODE && position.fix_quality >= 1) {
      CAL_PRINTLN("GPS: Fix acquired");
      Serial.printf("  Lat: %.6f, Lon: %.6f, Alt: %.1f m\n",
          position.latitude, position.longitude, position.altitude);
      Serial.printf("  Satellites: %d, Fix Quality: %d, Accuracy: %.1f m\n",
          position.num_satellites, position.fix_quality, position.accuracy_m);
    }
  } else if (IS_CALIBRATION_MODE && gps.isInitialized() && !gps.hasLock()) {
    // Debug: show GPS status when waiting for fix
    // (only output periodically to avoid spam)
    static uint32_t last_gps_debug_ms = 0;
    uint32_t now_ms = millis();
    if (now_ms - last_gps_debug_ms >= 5000) {
      last_gps_debug_ms = now_ms;
      CAL_PRINTLN("GPS: Waiting for fix...");
    }
  }

  // ========================================================================
  // Output Sensor Data (frequency-controlled to ~10 Hz)
  // ========================================================================
  if (output_manager.shouldOutput()) {
    char buffer[512];
    uint16_t len = output_manager.getFormattedOutput(buffer, sizeof(buffer));
    if (len > 0) {
      Serial.println(buffer);
    }
  }
}
