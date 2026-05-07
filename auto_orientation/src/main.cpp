/**
 * Auto Orientation: BNO085 IMU + GPS + Persistent Calibration + EKF Fusion (Phase 3)
 *
 * Reads absolute orientation (quaternions) from BNO085 IMU with auto-save
 * calibration to EEPROM. On boot, loads previously saved calibration.
 * Integrates GPS position data for combined orientation+position output.
 * Optionally uses Extended Kalman Filter (EKF) for sensor fusion (Phase 3).
 *
 * Hardware:
 * - BNO085 (Adafruit) via I2C (SDA pin 20, SCL pin 21 on Arduino Mega)
 * - GPS (NEO-M9N or similar) via UART Serial1 (Pins 18/19 on Arduino Mega)
 * - Outputs JSON with orientation + position + calibration status
 * - Phase 3: Also outputs EKF fused state JSON when enabled
 *
 * Sensor Rates:
 * - BNO085: ~100 Hz (1 sample per ~10 ms)
 * - GPS: ~1 Hz (1 sample per ~1000 ms), configurable
 * - Output: ~10 Hz (100 ms intervals), configurable
 * - EKF Fused Output: 10 Hz (optional, Phase 3)
 *
 * Calibration:
 * - Auto-saves to EEPROM when level 2+ (medium or higher)
 * - Auto-loads on boot if previously saved
 * - Survives code uploads, power cycles, orientation changes
 *
 * See docs/CALIBRATION_GUIDE.md for complete user guide.
 * See docs/ABSOLUTE_ORIENTATION_EXPLAINED.md for theory.
 * See docs/GETTING_STARTED.md for setup instructions.
 * See docs/EKF_OUTPUT_FORMAT.md for Phase 3 EKF output specification.
 */

#include <Arduino.h>
#include "config/pins.h"
#include "config/mode.h"
#include "config/ekf_config.h"
#include "sensors/sensor_base.h"
#include "output/sensor_output_manager.h"
#include "sensors/bno085.h"
#include "sensors/gps.h"
#include "math/coordinates.h"
#include "navigation/ekf.h"

#if ENABLE_SNAPSHOT_RECORDER
#include "features/snapshot_recorder.h"
#include "sensors/button_input.h"
#endif

BNO085 imu;
GPS gps;
SensorOutputManager output_manager;

// Phase 3: Extended Kalman Filter for sensor fusion
ExtendedKalmanFilter ekf;
bool ekf_initialized = false;

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
  // Initialize EKF on first IMU data (Phase 3)
  // ========================================================================
  if (!ekf_initialized && imu.read() && imu.hasNewData()) {
    // Create initial covariance matrix with large uncertainties
    Matrix16x16 P_init;
    memset(P_init, 0, sizeof(Matrix16x16));

    // Set initial uncertainties (diagonal matrix)
    for (int i = 0; i < 4; i++) {
      P_init[i][i] = 1.0f;  // Quaternion uncertainty
    }
    for (int i = 4; i < 7; i++) {
      P_init[i][i] = 10.0f;  // Velocity uncertainty (m/s)
    }
    for (int i = 7; i < 10; i++) {
      P_init[i][i] = 100.0f;  // Position uncertainty (m)
    }
    for (int i = 10; i < 13; i++) {
      P_init[i][i] = 0.1f;  // Accel bias uncertainty
    }
    for (int i = 13; i < 16; i++) {
      P_init[i][i] = 0.01f;  // Gyro bias uncertainty
    }

    if (ekf.initialize(P_init)) {
      ekf_initialized = true;
      CAL_PRINTLN("EKF: Initialized successfully");
    } else {
      CAL_PRINTLN("WARNING: EKF initialization failed");
    }
  }

  // ========================================================================
  // Read BNO085 Orientation (high frequency: ~100 Hz)
  // ========================================================================
  if (imu.read() && imu.hasNewData()) {
    const OrientationData& orientation = imu.getOrientation();
    output_manager.updateOrientation(orientation);

    // Phase 3: Feed IMU data to EKF for prediction step
    if (ekf_initialized) {
      static uint32_t last_ekf_predict_ms = 0;
      uint32_t now_ms = millis();
      float dt_sec = (now_ms - last_ekf_predict_ms) / 1000.0f;

      // Sanity check on dt
      if (dt_sec > 0.001f && dt_sec < 0.2f && last_ekf_predict_ms > 0) {
        // Get IMU raw measurements (gyro + accel)
        // Note: BNO085 provides quaternion, not raw gyro/accel
        // For Phase 3, we use placeholder sensor data
        // Production code would extract these from BNO085 raw sensor events
        float gyro_rad_s[3] = {0.0f, 0.0f, 0.0f};  // Placeholder
        float accel_m_s2[3] = {0.0f, 0.0f, 9.81f};  // Placeholder (gravity)

        ekf.predict(gyro_rad_s, accel_m_s2, dt_sec);
      }
      last_ekf_predict_ms = now_ms;
    }

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

    // Phase 3: GPS dropout handling via EKF
    if (ekf_initialized) {
      if (position.fix_quality >= 1) {
        // GPS has lock - mark no dropout in EKF
        ekf.set_gps_dropout(false);

        if (IS_CALIBRATION_MODE) {
          CAL_PRINTLN("GPS: Fix acquired");
          Serial.print("  Lat: ");
          Serial.print(position.latitude, 6);
          Serial.print(", Lon: ");
          Serial.print(position.longitude, 6);
          Serial.print(", Alt: ");
          Serial.print(position.altitude, 1);
          Serial.println(" m");
          Serial.print("  Satellites: ");
          Serial.print(position.num_satellites);
          Serial.print(", Fix Quality: ");
          Serial.print(position.fix_quality);
          Serial.print(", Accuracy: ");
          Serial.print(position.accuracy_m, 1);
          Serial.println(" m");
        }

        // TODO: Feed GPS measurement to EKF update step
        // This requires proper NED conversion and implementation
        // of the measurement update in the predict-only loop
      } else {
        // No GPS lock - mark as dropout
        ekf.set_gps_dropout(true);
      }
    }
  } else if (ekf_initialized && gps.isInitialized()) {
    // Periodically check GPS timeout for dropout detection
    static uint32_t last_gps_check_ms = 0;
    uint32_t now_ms = millis();

    if (now_ms - last_gps_check_ms >= 1000) {  // Check every 1 second
      last_gps_check_ms = now_ms;

      // If GPS age > 1 second, mark as dropout
      uint32_t gps_age = ekf.get_gps_age_ms();
      if (gps_age > 1000 || gps_age == UINT32_MAX) {
        ekf.set_gps_dropout(true);
      }

      if (IS_CALIBRATION_MODE && !gps.hasLock()) {
        CAL_PRINTLN("GPS: Waiting for fix...");
      }
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

  // Phase 3: EKF Health Diagnostics (every 10 seconds in calibration mode)
  if (ekf_initialized && IS_CALIBRATION_MODE) {
    static uint32_t last_ekf_diag_ms = 0;
    uint32_t now_ms = millis();
    if (now_ms - last_ekf_diag_ms >= 10000) {
      last_ekf_diag_ms = now_ms;

      Serial.println("\n[EKF Diagnostics]");
      Serial.print("  GPS Locked: ");
      Serial.println(ekf.is_gps_locked() ? "Yes" : "No");
      Serial.print("  GPS Age (ms): ");
      Serial.println(ekf.get_gps_age_ms());
      Serial.print("  Dead Reckoning Age (ms): ");
      Serial.println(ekf.get_dead_reckoning_age_ms());
      Serial.print("  Covariance Trace: ");
      Serial.println(ekf.get_covariance_trace(), 2);
      Serial.print("  Covariance Valid: ");
      Serial.println(ekf.is_covariance_valid() ? "Yes" : "No");
      Serial.print("  Consecutive Dropout Updates: ");
      Serial.println(ekf.get_consecutive_dropout_updates());
      Serial.println();
    }
  }
}
