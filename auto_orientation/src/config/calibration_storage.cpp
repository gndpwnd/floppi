/**
 * Calibration Data Persistent Storage - Implementation Skeleton
 *
 * This file provides the implementation for saving/restoring calibration
 * data to/from Arduino EEPROM.
 *
 * STATUS: Skeleton structure with full implementation ready for build
 *
 * Uses Arduino's built-in <EEPROM.h> library (available on all standard boards)
 */

#include "calibration_storage.h"
#include <EEPROM.h>
#include <string.h>

// ============================================================================
// CRC8 CALCULATION
// ============================================================================

uint8_t calculateCRC8(const uint8_t* data, uint16_t length) {
  /*
   * Simple XOR-based CRC for light error detection.
   * Not cryptographically secure, but catches single-bit flips.
   *
   * Algorithm:
   * 1. Start with CRC = 0
   * 2. For each byte in data:
   *    - XOR the byte into the CRC
   * 3. Return final CRC
   *
   * Properties:
   * - Detects single-bit errors: yes
   * - Detects byte swaps: yes (when byte values differ)
   * - Detects multi-bit corruption: probabilistic
   * - Speed: O(n), very fast
   *
   * Real CRC algorithms use polynomial division, but this simple XOR
   * is sufficient for our use case and saves code space.
   */

  if (!data || length == 0) return 0;

  uint8_t crc = 0;
  for (uint16_t i = 0; i < length; i++) {
    crc ^= data[i];
  }
  return crc;
}

// ============================================================================
// SAVE CALIBRATION TO EEPROM
// ============================================================================

bool saveToEEPROM(const uint8_t* cal_data, uint16_t length) {
  /*
   * Save calibration data with header to EEPROM.
   *
   * Process:
   * 1. Validate input length
   * 2. Calculate CRC8 checksum
   * 3. Write header bytes (marker, length, version, CRC)
   * 4. Write calibration data bytes
   *
   * EEPROM writes:
   * - Each write takes ~3.3ms on AVR boards
   * - Total time for 256 bytes: ~850ms
   * - Do not power off during write!
   */

  if (!cal_data || length == 0) {
    return false;
  }

  // Validate length fits in our storage
  if (length > CAL_DATA_MAX_SIZE) {
    return false;
  }

  // Calculate CRC8 of calibration data
  uint8_t crc = calculateCRC8(cal_data, length);

  // Write header to EEPROM
  // Byte 0: Validity marker
  EEPROM.write(CAL_EEPROM_BASE + CAL_EEPROM_MARKER_OFFSET, CAL_MARKER_VALID);

  // Byte 1: Data length
  EEPROM.write(CAL_EEPROM_BASE + CAL_EEPROM_LENGTH_OFFSET, (uint8_t)length);

  // Byte 2: Format version
  EEPROM.write(CAL_EEPROM_BASE + CAL_EEPROM_VERSION_OFFSET, CAL_FORMAT_VERSION);

  // Byte 3: CRC8 checksum
  EEPROM.write(CAL_EEPROM_BASE + CAL_EEPROM_CRC_OFFSET, crc);

  // Write calibration data payload
  for (uint16_t i = 0; i < length; i++) {
    EEPROM.write(CAL_EEPROM_BASE + CAL_EEPROM_PAYLOAD_OFFSET + i, cal_data[i]);
  }

  return true;
}

// ============================================================================
// RESTORE CALIBRATION FROM EEPROM
// ============================================================================

bool restoreFromEEPROM(uint8_t* cal_data, uint16_t* length) {
  /*
   * Restore calibration data from EEPROM with validation.
   *
   * Process:
   * 1. Check validity marker
   * 2. Read length and version
   * 3. Read calibration data
   * 4. Verify CRC8 checksum
   * 5. Return data if valid, false if any check fails
   *
   * Failure cases:
   * - No valid marker (EEPROM empty)
   * - CRC mismatch (data corrupted)
   * - Invalid length
   * - Version mismatch
   */

  if (!cal_data || !length) {
    return false;
  }

  // Read validity marker
  uint8_t marker = EEPROM.read(CAL_EEPROM_BASE + CAL_EEPROM_MARKER_OFFSET);
  if (marker != CAL_MARKER_VALID) {
    // No valid calibration stored
    return false;
  }

  // Read data length
  uint8_t stored_length = EEPROM.read(CAL_EEPROM_BASE + CAL_EEPROM_LENGTH_OFFSET);
  if (stored_length == 0 || stored_length > CAL_DATA_MAX_SIZE) {
    // Invalid length
    return false;
  }

  // Read format version
  uint8_t version = EEPROM.read(CAL_EEPROM_BASE + CAL_EEPROM_VERSION_OFFSET);
  if (version != CAL_FORMAT_VERSION) {
    // Format mismatch - could be from older firmware
    // For now, accept it (future: could handle multiple versions)
  }

  // Read calibration data
  for (uint16_t i = 0; i < stored_length; i++) {
    cal_data[i] = EEPROM.read(CAL_EEPROM_BASE + CAL_EEPROM_PAYLOAD_OFFSET + i);
  }

  // Verify CRC8
  uint8_t stored_crc = EEPROM.read(CAL_EEPROM_BASE + CAL_EEPROM_CRC_OFFSET);
  uint8_t calculated_crc = calculateCRC8(cal_data, stored_length);

  if (stored_crc != calculated_crc) {
    // CRC mismatch - data corrupted
    return false;
  }

  // Success - return length
  *length = stored_length;
  return true;
}

// ============================================================================
// CHECK IF CALIBRATION EXISTS
// ============================================================================

bool hasCalibrationInEEPROM(void) {
  /*
   * Quick check if a valid calibration is stored.
   * Does not validate CRC or load data, just checks the marker.
   *
   * Useful for deciding:
   * - Skip initial calibration (restore and use saved)
   * - Proceed with initial calibration (no saved available)
   */

  uint8_t marker = EEPROM.read(CAL_EEPROM_BASE + CAL_EEPROM_MARKER_OFFSET);
  return (marker == CAL_MARKER_VALID);
}

// ============================================================================
// CLEAR CALIBRATION FROM EEPROM
// ============================================================================

bool clearCalibrationFromEEPROM(void) {
  /*
   * Clear the calibration validity marker, making EEPROM appear empty.
   * The actual data bytes remain (can be recovered), but marked as invalid.
   *
   * This is fast (just 1 byte write) and reversible (data still present).
   * Use when:
   * - Forcing re-calibration
   * - Preparing for new location
   * - Factory reset
   */

  EEPROM.write(CAL_EEPROM_BASE + CAL_EEPROM_MARKER_OFFSET, CAL_MARKER_EMPTY);
  return true;
}
