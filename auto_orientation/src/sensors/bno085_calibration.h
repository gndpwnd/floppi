/**
 * BNO085 Calibration Profile Management
 *
 * This module provides access to BNO085 calibration data via the SH-2 protocol,
 * specifically using the FRS (Feature Record Store) interface for calibration records.
 *
 * The BNO085 maintains calibration data in internal NVM (non-volatile memory) that can
 * be read and written using SH-2 FRS record operations. We extract the calibration
 * profile and store it in Arduino EEPROM for persistence across power cycles.
 *
 * ============================================================================
 * SH-2 Protocol - FRS Records for Calibration
 * ============================================================================
 *
 * The BNO085 exposes calibration data through Feature Record Store (FRS) records:
 *
 * FRS Record IDs (from sh2.h):
 * - STATIC_CALIBRATION_AGM (0x7979)    - Static calibration (Accel, Gyro, Mag)
 * - DYNAMIC_CALIBRATION (0x1F1F)       - Dynamic/runtime calibration data
 * - NOMINAL_CALIBRATION (0x4D4D)       - Nominal factory calibration
 *
 * Access Methods:
 * 1. sh2_getFrs(uint16_t recordId, uint32_t *pData, uint16_t *words)
 *    - Reads an FRS record (returns data in 32-bit word units)
 *    - recordId: The FRS record to read (e.g., DYNAMIC_CALIBRATION)
 *    - pData: Buffer to receive 32-bit words
 *    - words: [in] buffer size in words, [out] words read
 *    - Returns: SH2_OK (0) on success, negative value on error
 *
 * 2. sh2_setFrs(uint16_t recordId, uint32_t *pData, uint16_t words)
 *    - Writes an FRS record (data in 32-bit word units)
 *    - To delete a record, call with words=0
 *    - Returns: SH2_OK (0) on success
 *
 * NOTE: These functions are called via the sh2_* API, which is part of the
 * Adafruit_BNO08x library's SH-2 protocol implementation.
 *
 * ============================================================================
 * Calibration Data Format
 * ============================================================================
 *
 * The DYNAMIC_CALIBRATION record contains runtime calibration coefficients:
 *
 * Data Layout (variable size, typically ~36-72 bytes):
 * - Accelerometer: Bias/offset values (X, Y, Z)
 * - Gyroscope: Bias/offset values (X, Y, Z)
 * - Magnetometer: Hard-iron offset (X, Y, Z) + Soft-iron scale matrix
 * - System flags: Calibration status and metadata
 *
 * The exact format depends on firmware version, so we treat it as opaque
 * binary data when reading/writing. The BNO085 firmware validates and applies
 * it automatically.
 *
 * ============================================================================
 * Usage Pattern
 * ============================================================================
 *
 * 1. Read calibration from sensor:
 *    uint8_t cal_buffer[256];
 *    uint16_t cal_length;
 *    if (readCalibrationProfile(cal_buffer, &cal_length)) {
 *        // Successfully read ~cal_length bytes of calibration
 *    }
 *
 * 2. Write calibration to sensor:
 *    if (writeCalibrationProfile(cal_buffer, cal_length)) {
 *        // Calibration restored to sensor
 *    }
 *
 * 3. Validate data:
 *    if (validateCalibrationData(cal_buffer, cal_length)) {
 *        // Data structure is valid
 *    }
 *
 * ============================================================================
 * Integration with Adafruit SH-2 API
 * ============================================================================
 *
 * The implementation relies on the Adafruit_BNO08x wrapper having access to
 * sh2_* functions. Currently, these are in the library but may not be directly
 * exposed through the Adafruit_BNO08x class. We may need to:
 *
 * Option A: Use Adafruit_BNO08x's existing wrapper methods (if available)
 * Option B: Access sh2_* functions directly via the _HAL member (internal API)
 * Option C: Create a custom SHTP command layer if FRS is not accessible
 *
 * Status: Investigation in progress - see analysis document.
 */

#ifndef BNO085_CALIBRATION_H
#define BNO085_CALIBRATION_H

#include <stdint.h>
#include <stdbool.h>

// Maximum size for a calibration profile buffer
#define BNO085_MAX_CAL_DATA 256

// FRS Record IDs for calibration data (from sh2.h)
#define STATIC_CALIBRATION_AGM 0x7979
#define DYNAMIC_CALIBRATION 0x1F1F
#define NOMINAL_CALIBRATION 0x4D4D

/**
 * @brief Read the current calibration profile from BNO085
 *
 * Extracts the DYNAMIC_CALIBRATION record via SH-2 FRS protocol.
 * This captures the current runtime calibration state of the sensor.
 *
 * @param[out] buffer     Pointer to buffer to receive calibration data (max 256 bytes)
 * @param[out] length     Pointer to uint16_t; on return contains bytes read
 * @return true if successful, false if read failed
 *
 * @note The sensor must be initialized and ready before calling this.
 * @note Typical payload: 36-72 bytes depending on firmware version
 * @note This should be called after sensor has calibrated (cal_status >= 2)
 *
 * @blocking This call may take 10-50ms to complete the SH-2 transaction
 */
bool readCalibrationProfile(uint8_t* buffer, uint16_t* length);

/**
 * @brief Write a previously saved calibration profile to BNO085
 *
 * Restores a DYNAMIC_CALIBRATION record via SH-2 FRS protocol.
 * The sensor will begin using these calibration coefficients immediately.
 *
 * @param[in] buffer      Pointer to calibration data (from previous readCalibrationProfile)
 * @param[in] length      Number of bytes in buffer (must match original read length)
 * @return true if successful, false if write failed
 *
 * @note The sensor must be initialized before calling this.
 * @note Invalid data will be rejected by the BNO085 firmware.
 * @note After writing, sensor should report higher calibration status immediately.
 *
 * @blocking This call may take 10-50ms to complete the SH-2 transaction
 */
bool writeCalibrationProfile(const uint8_t* buffer, uint16_t length);

/**
 * @brief Validate calibration data structure
 *
 * Performs sanity checks on calibration data before writing to sensor.
 * This helps prevent writing corrupted or incompatible data.
 *
 * Checks performed:
 * - Length is within expected range (>= 36 bytes, <= 256 bytes)
 * - Data is not all zeros (would indicate uninitialized/empty)
 * - Data is not all 0xFF (would indicate erased memory)
 * - Optional: CRC check if firmware version/format is known
 *
 * @param[in] data        Pointer to calibration data
 * @param[in] length      Number of bytes to validate
 * @return true if data appears valid, false if suspicious
 *
 * @note This is a heuristic check; it may not catch all invalid data.
 * @note The BNO085 itself will perform additional validation on write.
 */
bool validateCalibrationData(const uint8_t* data, uint16_t length);

/**
 * @brief Get a human-readable description of last error
 *
 * Returns a string describing the most recent calibration operation error.
 * Used for debugging and logging purposes.
 *
 * @return Pointer to static error string, or "No error" if last op succeeded
 */
const char* getCalibrationError(void);

#endif  // BNO085_CALIBRATION_H
