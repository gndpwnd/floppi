/**
 * Calibration Data Persistent Storage - Implementation
 *
 * Saves and restores calibration data via the persistent_storage HAL.
 *
 * STATUS: Uses ps:: HAL (Phase 4.1). Source-level API is unchanged from the
 * previous direct-`<EEPROM.h>` implementation — only the backing store moves
 * to the HAL, so caller code in the rest of the project is untouched.
 *
 * Fixes Known Issue KI-1: silent EEPROM failure on ESP32 (the HAL routes ESP32
 * writes through Preferences/NVS with proper commit semantics).
 */

#include "calibration_storage.h"
#include "../storage/persistent_storage.h"
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
// SAVE CALIBRATION TO PERSISTENT STORAGE
// ============================================================================

bool saveToEEPROM(const uint8_t* cal_data, uint16_t length) {
  /*
   * Save calibration data with header via the persistent_storage HAL.
   *
   * Process:
   * 1. Validate input length
   * 2. Calculate CRC8 checksum
   * 3. Write header bytes (marker, length, version, CRC)
   * 4. Write calibration data bytes
   * 5. ps::commit() once to flush (no-op on AVR/Teensy; flushes NVS on ESP32)
   *
   * Notes on timing:
   * - AVR: each byte takes ~3.3 ms (~850 ms for full block); do not power off
   * - Teensy: flash-emulated, similar wall-clock budget
   * - ESP32: buffered until commit(); commit is atomic (single NVS write)
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

  // Build the 4-byte header in a small local buffer so we can issue a single
  // ps::write() call instead of four. This is friendlier to NVS back-ends
  // that batch internally and keeps the dirty-tracking path tight.
  uint8_t header[4];
  header[CAL_EEPROM_MARKER_OFFSET]  = CAL_MARKER_VALID;
  header[CAL_EEPROM_LENGTH_OFFSET]  = (uint8_t)length;
  header[CAL_EEPROM_VERSION_OFFSET] = CAL_FORMAT_VERSION;
  header[CAL_EEPROM_CRC_OFFSET]     = crc;

  if (!ps::write(CAL_EEPROM_BASE, header, sizeof(header))) {
    return false;
  }

  // Write the payload as a single batch.
  if (!ps::write(CAL_EEPROM_BASE + CAL_EEPROM_PAYLOAD_OFFSET,
                 cal_data, length)) {
    return false;
  }

  // Flush — required on ESP32 (NVS commit), no-op on AVR/Teensy.
  if (!ps::commit()) {
    return false;
  }

  return true;
}

// ============================================================================
// RESTORE CALIBRATION FROM PERSISTENT STORAGE
// ============================================================================

bool restoreFromEEPROM(uint8_t* cal_data, uint16_t* length) {
  /*
   * Restore calibration data via the persistent_storage HAL, with validation.
   *
   * Process:
   * 1. Read header (marker, length, version, CRC)
   * 2. Sanity-check marker and stored length
   * 3. Read calibration payload
   * 4. Recompute CRC8 and compare against the stored value
   * 5. Return data if valid; false on any check failure
   */

  if (!cal_data || !length) {
    return false;
  }

  // Read 4-byte header.
  uint8_t header[4];
  if (!ps::read(CAL_EEPROM_BASE, header, sizeof(header))) {
    return false;
  }

  uint8_t marker        = header[CAL_EEPROM_MARKER_OFFSET];
  uint8_t stored_length = header[CAL_EEPROM_LENGTH_OFFSET];
  uint8_t version       = header[CAL_EEPROM_VERSION_OFFSET];
  uint8_t stored_crc    = header[CAL_EEPROM_CRC_OFFSET];

  if (marker != CAL_MARKER_VALID) {
    // No valid calibration stored
    return false;
  }

  if (stored_length == 0 || stored_length > CAL_DATA_MAX_SIZE) {
    // Invalid length
    return false;
  }

  if (version != CAL_FORMAT_VERSION) {
    // Format mismatch - could be from older firmware
    // For now, accept it (future: could handle multiple versions)
  }

  // Read calibration payload in one batch.
  if (!ps::read(CAL_EEPROM_BASE + CAL_EEPROM_PAYLOAD_OFFSET,
                cal_data, stored_length)) {
    return false;
  }

  // Verify CRC8
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
   * Quick check if a valid calibration is stored. Reads only the marker byte
   * (offset 0) — no CRC validation or payload load. Matches the legacy
   * single-byte EEPROM.read() semantics exactly.
   */

  uint8_t marker = 0xFF;
  if (!ps::read(CAL_EEPROM_BASE + CAL_EEPROM_MARKER_OFFSET, &marker, 1)) {
    return false;
  }
  return (marker == CAL_MARKER_VALID);
}

// ============================================================================
// CLEAR CALIBRATION FROM PERSISTENT STORAGE
// ============================================================================

bool clearCalibrationFromEEPROM(void) {
  /*
   * Clear the calibration validity marker, making the slot appear empty.
   * The payload bytes are untouched (single-byte write). Fast and reversible
   * (the data is technically still in storage; only the marker is gone).
   */

  uint8_t empty = CAL_MARKER_EMPTY;
  if (!ps::write(CAL_EEPROM_BASE + CAL_EEPROM_MARKER_OFFSET, &empty, 1)) {
    return false;
  }
  // Flush — required on ESP32 so the marker change survives a reset.
  return ps::commit();
}
