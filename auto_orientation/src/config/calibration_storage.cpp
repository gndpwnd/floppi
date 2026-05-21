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
#include "../util/crc8.h"
#include <string.h>

// ============================================================================
// CRC8 CALCULATION
// ============================================================================

uint8_t calculateCRC8(const uint8_t* data, uint16_t length) {
  /*
   * CRC-8-CCITT polynomial CRC (polynomial 0x07, init 0x00, no reflection,
   * no final XOR). Replaces the prior single-byte XOR sum (per audit P1-015
   * in docs/findings/audit_security_2026-05-20.md), which silently missed any
   * even-count bitflip in a single column. CRC-8-CCITT detects all single-
   * and two-bit errors within an 8-byte window and most burst errors.
   *
   * The algorithm now lives in src/util/crc8.h as util::crc8_ccitt() — one
   * shared leaf replacing what used to be a byte-identical private copy here
   * and in balancing_robot_uno/tune_storage.cpp (tech-debt finding R1,
   * docs/findings/ao_uno_techdebt_2026-05-20.md §D1). calculateCRC8 is kept
   * as the public name so callers in main.cpp and the balance code are
   * unaffected; it is a thin delegating wrapper. The byte output is identical
   * to the previous inline implementation.
   */
  return util::crc8_ccitt(data, length);
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

  // Validate length fits in our storage. CAL_DATA_MAX_SIZE caps the payload;
  // no truncation possible since both this guard and the on-disk length field
  // (uint16_t in v2) are wider than the maximum.
  if (length > CAL_DATA_MAX_SIZE) {
    return false;
  }

  // Calculate CRC-8-CCITT of calibration data
  uint8_t crc = calculateCRC8(cal_data, length);

  // Build the 6-byte v2 header in a small local buffer so we can issue a
  // single ps::write() call. Layout matches the macros in the header file.
  uint8_t header[CAL_HEADER_SIZE];
  header[CAL_EEPROM_MARKER_OFFSET]    = CAL_MARKER_VALID;
  header[CAL_EEPROM_VERSION_OFFSET]   = CAL_FORMAT_VERSION;
  header[CAL_EEPROM_LENGTH_LO_OFFSET] = (uint8_t)(length & 0xFF);
  header[CAL_EEPROM_LENGTH_HI_OFFSET] = (uint8_t)((length >> 8) & 0xFF);
  header[CAL_EEPROM_CRC_OFFSET]       = crc;
  header[CAL_EEPROM_RESERVED_OFFSET]  = 0x00;

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

  // Read 6-byte v2 header.
  uint8_t header[CAL_HEADER_SIZE];
  if (!ps::read(CAL_EEPROM_BASE, header, sizeof(header))) {
    return false;
  }

  uint8_t marker     = header[CAL_EEPROM_MARKER_OFFSET];
  uint8_t version    = header[CAL_EEPROM_VERSION_OFFSET];
  uint8_t len_lo     = header[CAL_EEPROM_LENGTH_LO_OFFSET];
  uint8_t len_hi     = header[CAL_EEPROM_LENGTH_HI_OFFSET];
  uint8_t stored_crc = header[CAL_EEPROM_CRC_OFFSET];

  if (marker != CAL_MARKER_VALID) {
    // No valid calibration stored.
    return false;
  }

  // Per audit P2-016: reject mismatched versions outright rather than silently
  // restoring potentially-incompatible bytes into the sensor's NVM. A v1 blob
  // (still on already-deployed devices) hits this path and the operator is
  // prompted to re-calibrate — that's the intentional backward-compat policy
  // for this security pass; see docs/findings/security_fix_calibration_2026-05-20.md.
  if (version != CAL_FORMAT_VERSION) {
    return false;
  }

  // Length is now a full uint16_t — no more silent high-byte truncation.
  uint16_t stored_length = (uint16_t)len_lo | ((uint16_t)len_hi << 8);
  if (stored_length == 0 || stored_length > CAL_DATA_MAX_SIZE) {
    return false;
  }

  // Read calibration payload in one batch.
  if (!ps::read(CAL_EEPROM_BASE + CAL_EEPROM_PAYLOAD_OFFSET,
                cal_data, stored_length)) {
    return false;
  }

  // Verify CRC-8-CCITT.
  uint8_t calculated_crc = calculateCRC8(cal_data, stored_length);
  if (stored_crc != calculated_crc) {
    // CRC mismatch - data corrupted.
    return false;
  }

  // Success - return length.
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
