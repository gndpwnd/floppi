/**
 * Calibration Data Persistent Storage
 *
 * Provides abstraction layer for saving and restoring calibration data
 * to/from Arduino EEPROM (or other persistent storage in the future).
 *
 * ============================================================================
 * EEPROM Layout (Arduino Mega - 4096 bytes total)
 * ============================================================================
 *
 * This implementation reserves the first 256 bytes of EEPROM for calibration:
 *
 * Offset  | Size   | Content
 * --------|--------|---------------------------------------------
 * 0x00    | 1 byte | Calibration validity marker (0xCA for valid, 0xFF for empty)
 * 0x01    | 1 byte | Data length (1-255 bytes of actual calibration data)
 * 0x02    | 1 byte | Version/format byte (firmware compat, currently 0x01)
 * 0x03    | 1 byte | CRC8 of the full profile (simple XOR sum)
 * 0x04    | 252    | Calibration data payload (max 252 bytes)
 * --------|--------|---------------------------------------------
 * Total   | 256    | (Leaves room for future expansion)
 *
 * Additional storage blocks (for future use):
 * 0x100   | 256    | Backup/secondary calibration profile (v1.1+)
 * 0x200   | 256    | Metadata block: timestamp, location, firmware version (v1.1+)
 * 0x300   | 256    | SD card/extended storage pointers (v1.1+)
 *
 * ============================================================================
 * Validity Marker Strategy
 * ============================================================================
 *
 * When EEPROM is erased, all bytes read as 0xFF.
 * When calibration is NOT stored, we mark with 0xFF (no valid marker).
 * When calibration IS stored, we write 0xCA (= "CAlib" marker).
 *
 * This allows us to detect:
 * - Empty EEPROM (no marker set)
 * - Valid calibration (marker is 0xCA)
 * - Corrupted data (marker is neither 0xFF nor 0xCA)
 *
 * ============================================================================
 * Usage Pattern
 * ============================================================================
 *
 * 1. Save calibration after successful sensor calibration:
 *    uint8_t cal_data[256];
 *    uint16_t cal_length;
 *    bno.getCalibrationProfile(cal_data, &cal_length);
 *    saveToEEPROM(cal_data, cal_length);  // Saves with marker + metadata
 *
 * 2. Restore calibration on startup:
 *    uint8_t cal_data[256];
 *    uint16_t cal_length;
 *    if (restoreFromEEPROM(cal_data, &cal_length)) {
 *        bno.setCalibrationProfile(cal_data, cal_length);
 *    }
 *
 * 3. Clear stored calibration (force re-calibration):
 *    clearCalibrationFromEEPROM();
 *
 * ============================================================================
 * Data Integrity
 * ============================================================================
 *
 * Each saved profile includes:
 * - Validity marker: Detect if EEPROM slot is in use
 * - Length field: Know how many bytes to restore
 * - Format version: Handle future firmware upgrades
 * - Simple CRC8: Detect single-bit errors (XOR of all data bytes)
 *
 * On restore, we:
 * - Check validity marker
 * - Verify CRC8
 * - Validate with validateCalibrationData() before writing to sensor
 *
 * ============================================================================
 * Arduino Compatibility
 * ============================================================================
 *
 * This implementation uses Arduino's built-in <EEPROM.h> library:
 * - Available on: Uno, Mega, Nano, Leonardo, Due, etc.
 * - EEPROM.write(address, byte) - Write one byte
 * - EEPROM.read(address) - Read one byte
 * - EEPROM.put(address, value) - Write multi-byte value
 * - EEPROM.get(address, value) - Read multi-byte value
 *
 * Board-specific limitations:
 * - Uno/Nano: 1024 bytes total (we use first 256, still plenty room)
 * - Mega: 4096 bytes total (we use first 256)
 * - Due: 2048 bytes total (we use first 256)
 */

#ifndef CALIBRATION_STORAGE_H
#define CALIBRATION_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

// EEPROM storage configuration
#define CAL_EEPROM_BASE 0x00         // Start address in EEPROM
#define CAL_EEPROM_SIZE 512          // Reserved size for calibration (includes header)
#define CAL_DATA_MAX_SIZE 508        // Maximum payload (512 - 4 byte header)

// EEPROM header byte offsets
#define CAL_EEPROM_MARKER_OFFSET 0   // Validity marker byte
#define CAL_EEPROM_LENGTH_OFFSET 1   // Length of calibration data
#define CAL_EEPROM_VERSION_OFFSET 2  // Format/version byte
#define CAL_EEPROM_CRC_OFFSET 3      // CRC8 checksum
#define CAL_EEPROM_PAYLOAD_OFFSET 4  // Start of actual calibration data

// Validity markers
#define CAL_MARKER_VALID 0xCA        // Marker when data is valid
#define CAL_MARKER_EMPTY 0xFF        // Marker when EEPROM is empty

// Format version
#define CAL_FORMAT_VERSION 0x01      // Current format version

/**
 * @brief Save calibration profile to Arduino EEPROM
 *
 * Writes calibration data to EEPROM with header information (marker, length, CRC).
 * Overwrites any previously stored calibration at the same location.
 *
 * @param[in] cal_data    Pointer to calibration data from BNO085
 * @param[in] length      Number of bytes to save (max 252)
 * @return true if save succeeded, false if data too large or write error
 *
 * @note EEPROM writes are slow (~3.3ms per byte on AVR)
 * @note Total write time: ~850ms for full 256-byte block
 * @note Do not power cycle during save!
 *
 * Process:
 * 1. Validate length <= 252
 * 2. Calculate CRC8 of calibration data
 * 3. Write header (marker, length, version, CRC)
 * 4. Write calibration data bytes
 */
bool saveToEEPROM(const uint8_t* cal_data, uint16_t length);

/**
 * @brief Restore calibration profile from Arduino EEPROM
 *
 * Reads calibration data previously saved via saveToEEPROM().
 * Performs validity checks (marker, CRC) before returning.
 *
 * @param[out] cal_data   Buffer to receive calibration data (min 256 bytes)
 * @param[out] length     Pointer to uint16_t; on return contains bytes read
 * @return true if restore succeeded and data is valid, false otherwise
 *
 * Failure cases:
 * - EEPROM is empty (marker != 0xCA)
 * - CRC8 check fails (data corruption detected)
 * - Format version mismatch
 * - Stored length is 0 or > 252
 *
 * @note If this returns false, caller should skip restoration and proceed
 *       with normal sensor initialization.
 */
bool restoreFromEEPROM(uint8_t* cal_data, uint16_t* length);

/**
 * @brief Check if valid calibration is stored in EEPROM
 *
 * Quick check to determine if a calibration profile is available.
 * Does not validate CRC or load data, just checks the marker.
 *
 * @return true if valid calibration marker found, false otherwise
 *
 * @note Use this to decide whether to:
 *       - Skip initial calibration (restore and use saved cal)
 *       - Proceed with initial calibration (no saved cal available)
 */
bool hasCalibrationInEEPROM(void);

/**
 * @brief Erase calibration data from EEPROM
 *
 * Clears the validity marker, forcing the next check to report no calibration.
 * The actual data bytes are not erased, just marked as invalid.
 * This is fast (just overwrites 1 byte) and reversible (data still in memory).
 *
 * @return true if erase succeeded, false on write error
 *
 * Use cases:
 * - Force re-calibration on next boot
 * - Prepare EEPROM before moving sensor to new location
 * - Factory reset / user-initiated calibration clear
 */
bool clearCalibrationFromEEPROM(void);

/**
 * @brief Calculate CRC8 checksum of calibration data
 *
 * Simple XOR-based CRC for detection of data corruption.
 * Not cryptographically secure, but sufficient for accidental bit flips.
 *
 * @param[in] data        Pointer to data to checksum
 * @param[in] length      Number of bytes
 * @return CRC8 value (0x00 - 0xFF)
 *
 * @note This is a simple XOR sum, not a true CRC polynomial.
 *       Used mainly to catch obvious corruption, not malicious tampering.
 */
uint8_t calculateCRC8(const uint8_t* data, uint16_t length);

#endif  // CALIBRATION_STORAGE_H
