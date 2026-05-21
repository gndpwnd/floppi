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
 * This implementation reserves the first 512 bytes of EEPROM for calibration.
 *
 * V2 layout (current — CAL_FORMAT_VERSION = 0x02):
 *
 * Offset  | Size   | Content
 * --------|--------|---------------------------------------------
 * 0x00    | 1 byte | Calibration validity marker (0xCA for valid, 0xFF for empty)
 * 0x01    | 1 byte | Version/format byte (currently 0x02)
 * 0x02    | 2 byte | Data length (uint16_t, little-endian; up to CAL_DATA_MAX_SIZE)
 * 0x04    | 1 byte | CRC8 of the calibration data (CRC-8-CCITT, poly 0x07, init 0x00)
 * 0x05    | 1 byte | Reserved (zero)
 * 0x06    | 506    | Calibration data payload (max CAL_DATA_MAX_SIZE bytes)
 * --------|--------|---------------------------------------------
 * Total   | 512    |
 *
 * V1 layout (legacy — rejected by restoreFromEEPROM): single-byte length at
 * offset 0x01, version at 0x02, XOR-checksum at 0x03. Devices with a v1 blob
 * will be reported as "no calibration"; the operator must re-calibrate. This
 * was a deliberate backward-incompatible bump to ship two integrity fixes at
 * once (CRC algorithm + uint16_t length field).
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
 * - Length field (uint16_t): Know how many bytes to restore; supports payloads
 *   up to CAL_DATA_MAX_SIZE bytes without truncation
 * - Format version: Handle future firmware upgrades; mismatched versions are
 *   rejected outright (no silent garbage-data restore)
 * - CRC8-CCITT (polynomial 0x07, init 0x00): proper polynomial CRC; detects
 *   all single-bit errors, two-bit errors within any 8-byte window, and most
 *   burst errors — strictly stronger than the legacy XOR-sum used in v1
 *
 * On restore, we:
 * - Check validity marker
 * - Reject if version != CAL_FORMAT_VERSION (closes silent-version-mismatch hole)
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
#include <stddef.h>  // size_t (buf_capacity parameter of restoreFromEEPROM)

// EEPROM storage configuration
#define CAL_EEPROM_BASE 0x00         // Start address in EEPROM
#define CAL_EEPROM_SIZE 512          // Reserved size for calibration (includes header)
#define CAL_HEADER_SIZE 6            // V2 header is 6 bytes (see layout above)
#define CAL_DATA_MAX_SIZE 506        // Maximum payload (512 - 6 byte header)

// EEPROM header byte offsets (V2)
#define CAL_EEPROM_MARKER_OFFSET 0      // Validity marker byte
#define CAL_EEPROM_VERSION_OFFSET 1     // Format/version byte
#define CAL_EEPROM_LENGTH_LO_OFFSET 2   // Length low byte (uint16_t LE)
#define CAL_EEPROM_LENGTH_HI_OFFSET 3   // Length high byte (uint16_t LE)
#define CAL_EEPROM_CRC_OFFSET 4         // CRC8-CCITT checksum
#define CAL_EEPROM_RESERVED_OFFSET 5    // Reserved (zero)
#define CAL_EEPROM_PAYLOAD_OFFSET 6     // Start of actual calibration data

// Validity markers
#define CAL_MARKER_VALID 0xCA        // Marker when data is valid
#define CAL_MARKER_EMPTY 0xFF        // Marker when EEPROM is empty

// Format version. Bumped from 0x01 to 0x02 in 2026-05-20 security-fix pass:
// this is a deliberately backward-incompatible bump that simultaneously
// (a) widens the length field to uint16_t and (b) switches from a plain XOR
// "CRC" to a real CRC-8-CCITT polynomial. Devices with a v1 blob in EEPROM
// will be reported as having no calibration and must be re-calibrated.
#define CAL_FORMAT_VERSION 0x02      // Current format version

/**
 * @brief Save calibration profile to Arduino EEPROM
 *
 * Writes calibration data to EEPROM with header information (marker, length, CRC).
 * Overwrites any previously stored calibration at the same location.
 *
 * @param[in] cal_data    Pointer to calibration data from BNO085
 * @param[in] length      Number of bytes to save (max CAL_DATA_MAX_SIZE)
 * @return true if save succeeded, false if data too large or write error
 *
 * @note EEPROM writes are slow (~3.3ms per byte on AVR)
 * @note Total write time: ~850ms for full 256-byte block
 * @note Do not power cycle during save!
 *
 * Process:
 * 1. Validate length <= CAL_DATA_MAX_SIZE
 * 2. Calculate CRC-8-CCITT of calibration data
 * 3. Write header (marker, version, length lo/hi, CRC, reserved)
 * 4. Write calibration data bytes
 */
bool saveToEEPROM(const uint8_t* cal_data, uint16_t length);

/**
 * @brief Restore calibration profile from Arduino EEPROM
 *
 * Reads calibration data previously saved via saveToEEPROM().
 * Performs validity checks (marker, CRC) before returning.
 *
 * @param[out] cal_data     Buffer to receive calibration data
 * @param[in]  buf_capacity Capacity of cal_data in bytes; the stored payload
 *                          is rejected if it would not fit (prevents a stack
 *                          buffer overflow when a caller passes a buffer
 *                          smaller than the stored length)
 * @param[out] length       Pointer to uint16_t; on return contains bytes read
 * @return true if restore succeeded and data is valid, false otherwise
 *
 * Failure cases:
 * - EEPROM is empty (marker != 0xCA)
 * - CRC8 check fails (data corruption detected)
 * - Format version mismatch (rejected outright; caller must re-calibrate)
 * - Stored length is 0 or > CAL_DATA_MAX_SIZE
 * - Stored length exceeds buf_capacity (would overflow caller's buffer)
 *
 * @note If this returns false, caller should skip restoration and proceed
 *       with normal sensor initialization.
 */
bool restoreFromEEPROM(uint8_t* cal_data, size_t buf_capacity,
                       uint16_t* length);

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
 * @brief Calculate CRC-8-CCITT checksum of calibration data
 *
 * Polynomial CRC8 (CRC-8-CCITT, polynomial 0x07, init 0x00, no reflection,
 * no final XOR) for robust detection of data corruption. Catches all
 * single-bit errors, all two-bit errors within an 8-byte window, and the
 * vast majority of burst errors — strictly stronger than the previous XOR
 * sum, which silently missed any even-count bitflip in the same column.
 *
 * @param[in] data        Pointer to data to checksum
 * @param[in] length      Number of bytes
 * @return CRC8 value (0x00 - 0xFF)
 *
 * @note Not cryptographically secure; intended for accidental corruption,
 *       not adversarial tampering.
 */
uint8_t calculateCRC8(const uint8_t* data, uint16_t length);

#endif  // CALIBRATION_STORAGE_H
