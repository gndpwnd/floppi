/**
 * BNO085 Calibration Integration Example
 *
 * This file shows how to integrate the calibration persistence framework
 * into the BNO085 driver. Use as reference when implementing the actual
 * integration in bno085.cpp
 *
 * STATUS: Reference/Example Only - Do not compile this file
 */

#ifndef BNO085_INTEGRATION_EXAMPLE_H
#define BNO085_INTEGRATION_EXAMPLE_H

#include "bno085.h"
#include "bno085_calibration.h"
#include "../config/calibration_storage.h"

/**
 * INTEGRATION POINT 1: Restore on Startup (in BNO085::begin())
 *
 * After sensor is initialized, try to restore saved calibration.
 * This should happen right after enableReport() and before normal
 * sensor reading starts.
 */
void bno085_integration_example_1() {
  // Pseudo-code for BNO085::begin() implementation

  // ... existing initialization ...
  // imu_->begin_UART(...);
  // imu_->enableReport(...);

  // NEW: Restore calibration from EEPROM if available
  if (hasCalibrationInEEPROM()) {
    uint8_t cal_data[256];
    uint16_t cal_length;

    if (restoreFromEEPROM(cal_data, &cal_length)) {
      // Successfully restored calibration
      if (setCalibrationProfile(cal_data, cal_length)) {
        Serial.println("Restored calibration from EEPROM");
        // Sensor is now using saved calibration
        // Calibration status should jump to level 3 (high) immediately
      } else {
        Serial.println("Failed to write restored calibration to sensor");
        // Fall through to normal calibration flow
      }
    } else {
      Serial.println("EEPROM calibration data is corrupted");
      clearCalibrationFromEEPROM();  // Mark as invalid
      // Fall through to normal calibration flow
    }
  } else {
    Serial.println("No saved calibration in EEPROM - will calibrate on startup");
  }
}

/**
 * INTEGRATION POINT 2: Save After Calibration (in main loop or callback)
 *
 * Once the sensor achieves high calibration (cal_status == 3),
 * save the calibration to EEPROM so it persists.
 *
 * Options:
 * A) Automatic: Save when calibration first reaches level 3
 * B) Manual: User triggers save via serial command
 * C) Periodic: Check every N seconds and save if still at level 3
 */
void bno085_integration_example_2() {
  // Pseudo-code for main loop or dedicated calibration monitor

  static bool calibration_saved = false;
  static uint32_t calibration_achieved_time = 0;

  // Read orientation (done normally in main loop)
  const OrientationData& orient = bno.getOrientation();

  // Check if magnetometer calibration is high (level 3)
  if (orient.cal_mag >= 3) {
    // Calibration is good!

    if (!calibration_saved) {
      // Mark when we first reach high calibration
      if (calibration_achieved_time == 0) {
        calibration_achieved_time = millis();
      }

      // Wait 5 seconds to ensure calibration is stable
      if (millis() - calibration_achieved_time > 5000) {
        // Read current calibration from sensor
        uint8_t cal_data[256];
        uint16_t cal_length;

        if (getCalibrationProfile(cal_data, &cal_length)) {
          // Calibration data is valid
          if (saveToEEPROM(cal_data, cal_length)) {
            Serial.println("Saved calibration to EEPROM");
            calibration_saved = true;
            // Don't save again until next session
          } else {
            Serial.println("EEPROM save failed");
          }
        }
      }
    }
  } else {
    // Calibration dropped below level 3
    // Reset flags to try saving again later if it recovers
    if (calibration_saved) {
      Serial.printf("Calibration dropped to level %d, will re-save if recovers\n",
                    orient.cal_mag);
      calibration_achieved_time = 0;
    }
  }
}

/**
 * INTEGRATION POINT 3: Serial Commands for Manual Control
 *
 * Add commands to allow user to manually save, restore, or clear calibration
 */
void bno085_integration_example_3() {
  // Pseudo-code for serial command handling

  // Process serial input (in void loop() or dedicated handler)
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "CAL_SAVE") {
      uint8_t cal_data[256];
      uint16_t cal_length;

      if (bno.getCalibrationProfile(cal_data, &cal_length)) {
        if (saveToEEPROM(cal_data, cal_length)) {
          Serial.println("OK: Calibration saved to EEPROM");
        } else {
          Serial.println("ERR: Failed to save calibration");
        }
      } else {
        Serial.println("ERR: Failed to read calibration from sensor");
      }
    }

    else if (cmd == "CAL_RESTORE") {
      uint8_t cal_data[256];
      uint16_t cal_length;

      if (restoreFromEEPROM(cal_data, &cal_length)) {
        if (bno.setCalibrationProfile(cal_data, cal_length)) {
          Serial.println("OK: Calibration restored from EEPROM");
        } else {
          Serial.println("ERR: Failed to write calibration to sensor");
        }
      } else {
        Serial.println("ERR: No valid calibration in EEPROM");
      }
    }

    else if (cmd == "CAL_CLEAR") {
      if (clearCalibrationFromEEPROM()) {
        Serial.println("OK: Cleared calibration from EEPROM");
        Serial.println("    Next boot will perform fresh calibration");
      } else {
        Serial.println("ERR: Failed to clear calibration");
      }
    }

    else if (cmd == "CAL_STATUS") {
      if (hasCalibrationInEEPROM()) {
        Serial.println("OK: Valid calibration stored in EEPROM");

        // Try to read and display info
        uint8_t cal_data[256];
        uint16_t cal_length;
        if (restoreFromEEPROM(cal_data, &cal_length)) {
          Serial.printf("    Data size: %u bytes\n", cal_length);
        }
      } else {
        Serial.println("OK: No calibration stored in EEPROM");
      }
    }
  }
}

/**
 * INTEGRATION POINT 4: Continuous Calibration Monitoring
 *
 * Log calibration status to help diagnose if calibration is drifting
 */
void bno085_integration_example_4() {
  // Pseudo-code for periodic logging (e.g., every 10 seconds)

  static uint32_t last_log = 0;
  uint32_t now = millis();

  if (now - last_log > 10000) {
    last_log = now;

    const OrientationData& orient = bno.getOrientation();

    Serial.printf("[CAL] Sys:%d Acc:%d Gyro:%d Mag:%d ",
                  orient.cal_status,
                  orient.cal_accel,
                  orient.cal_gyro,
                  orient.cal_mag);

    // Check if we should save
    if (orient.cal_mag == 3) {
      Serial.println("(Mag high - ready to save)");
      // Could trigger automatic save here
    } else if (orient.cal_mag == 2) {
      Serial.println("(Mag medium - keep moving to improve)");
    } else if (orient.cal_mag == 1) {
      Serial.println("(Mag low - perform figure-8 motion)");
    } else {
      Serial.println("(Mag uncalibrated - starting calibration)");
    }
  }
}

/**
 * INTEGRATION POINT 5: Handle Calibration Failures
 *
 * Gracefully handle cases where calibration read/write fails
 */
void bno085_integration_example_5() {
  // Pseudo-code for error handling

  uint8_t cal_data[256];
  uint16_t cal_length;

  // Case 1: Read failed
  if (!bno.getCalibrationProfile(cal_data, &cal_length)) {
    Serial.printf("ERR: Failed to read calibration: %s\n",
                  getCalibrationError());
    // Continue operation without saving
  }

  // Case 2: Restore failed due to corruption
  if (!restoreFromEEPROM(cal_data, &cal_length)) {
    Serial.println("WAR: EEPROM calibration corrupted, skipping restore");
    // Clear the corrupted data
    clearCalibrationFromEEPROM();
    // Proceed with fresh calibration
  }

  // Case 3: Write failed
  if (!bno.setCalibrationProfile(cal_data, cal_length)) {
    Serial.println("ERR: Failed to write calibration to sensor");
    // Sensor continues with previous calibration
  }
}

// ============================================================================
// INTEGRATION CHECKLIST
// ============================================================================
/*

When integrating into actual BNO085 driver, ensure:

[ ] Headers included in bno085.cpp:
    #include "bno085_calibration.h"
    #include "calibration_storage.h"

[ ] begin() method:
    - After imu_->enableReport()
    - Add restore from EEPROM logic
    - Handle restore failure gracefully

[ ] Main loop calibration monitoring:
    - Track when cal_mag reaches level 3
    - Call getCalibrationProfile() to read from sensor
    - Call saveToEEPROM() to persist
    - Add logging/serial output

[ ] Serial command interface:
    - Parse serial input
    - Implement CAL_SAVE, CAL_RESTORE, CAL_CLEAR, CAL_STATUS
    - Add to serial command dispatcher

[ ] Error handling:
    - Check return values of all calibration functions
    - Log errors to serial for debugging
    - Gracefully degrade if operations fail

[ ] Testing:
    - Boot without saved calibration (fresh sensor)
    - Calibrate and let automatic save trigger
    - Power cycle to verify calibration restored
    - Manual CAL_SAVE and CAL_RESTORE commands
    - CAL_CLEAR to force re-calibration

*/

#endif  // BNO085_INTEGRATION_EXAMPLE_H
