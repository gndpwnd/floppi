/**
 * BNO085 Calibration Profile Management
 *
 * This file provides implementation for BNO085 calibration read/write
 * operations via the SH-2 FRS (Feature Record Store) protocol.
 *
 * The SH-2 library provides sh2_getFrs() and sh2_setFrs() functions that
 * directly access the sensor's calibration records. These are used here to
 * read/write the DYNAMIC_CALIBRATION (0x1F1F) record.
 *
 * STATUS: Full implementation using Adafruit SH-2 FRS API
 */

#include "bno085_calibration.h"
#include "sh2.h"
#include "sh2_err.h"
#include <string.h>
#include <stdio.h>

// Buffer to store last error message
static char error_buffer_[64] = "No error";

/**
 * Convert error code to human-readable string
 * Maps SH-2 protocol error codes to descriptive strings
 */
static const char* sh2_error_to_string(int error_code) {
  switch (error_code) {
    case SH2_OK:                return "OK";
    case SH2_ERR:               return "General error";
    case SH2_ERR_BAD_PARAM:     return "Bad parameter";
    case SH2_ERR_OP_IN_PROGRESS: return "Operation in progress";
    case SH2_ERR_IO:            return "I/O error";
    case SH2_ERR_HUB:           return "Hub error";
    case SH2_ERR_TIMEOUT:       return "Timeout";
    default:                    return "Unknown error";
  }
}

// ============================================================================
// READ CALIBRATION PROFILE
// ============================================================================

bool readCalibrationProfile(uint8_t* buffer, uint16_t* length) {
  // Input validation
  if (!buffer || !length) {
    snprintf(error_buffer_, sizeof(error_buffer_), "NULL pointer passed");
    return false;
  }

  // Allocate temporary buffer for FRS words (max 64 words = 256 bytes).
  // Compile-time sanity: the local buffer must fit inside BNO085_MAX_CAL_DATA.
  static const uint16_t WORDS_BUFFER_CAPACITY = 64;
  uint32_t words_buffer[WORDS_BUFFER_CAPACITY];
  uint16_t num_words = WORDS_BUFFER_CAPACITY;

  // Request the DYNAMIC_CALIBRATION record from FRS
  int result = sh2_getFrs(DYNAMIC_CALIBRATION, words_buffer, &num_words);

  if (result != SH2_OK) {
    snprintf(error_buffer_, sizeof(error_buffer_),
             "sh2_getFrs failed: %s (code %d)",
             sh2_error_to_string(result), result);
    return false;
  }

  // Sensor must report a count that fits in our stack buffer. sh2_getFrs is
  // supposed to clamp to the in/out value we passed, but a buggy firmware
  // could over-report; without this check, the byte_count multiplication and
  // the pack loop below would both run off the end (audit P1-018).
  if (num_words > WORDS_BUFFER_CAPACITY) {
    snprintf(error_buffer_, sizeof(error_buffer_),
             "FRS returned %u words, exceeds buffer (%u)",
             num_words, (unsigned)WORDS_BUFFER_CAPACITY);
    return false;
  }

  // Convert 32-bit words to 8-bit bytes. Each word is stored little-endian.
  // Per audit P1-007 (bno085_calibration.cpp:67): the previous expression
  //   uint16_t byte_count = num_words * 4;
  // performs the multiplication in uint16_t/int promotion and wraps mod
  // 2^16 for num_words >= 16384. Use a wider intermediate type so the
  // BNO085_MAX_CAL_DATA range check below cannot be bypassed by overflow.
  uint32_t byte_count_wide = (uint32_t)num_words * 4u;

  if (byte_count_wide > BNO085_MAX_CAL_DATA) {
    snprintf(error_buffer_, sizeof(error_buffer_),
             "Calibration data too large: %lu bytes (max %u)",
             (unsigned long)byte_count_wide, BNO085_MAX_CAL_DATA);
    return false;
  }

  uint16_t byte_count = (uint16_t)byte_count_wide;

  // Pack 32-bit words into byte buffer (little-endian)
  for (uint16_t i = 0; i < num_words; i++) {
    uint32_t word = words_buffer[i];
    buffer[i*4+0] = (word >>  0) & 0xFF;
    buffer[i*4+1] = (word >>  8) & 0xFF;
    buffer[i*4+2] = (word >> 16) & 0xFF;
    buffer[i*4+3] = (word >> 24) & 0xFF;
  }

  *length = byte_count;
  snprintf(error_buffer_, sizeof(error_buffer_), "No error");
  return true;
}

// ============================================================================
// WRITE CALIBRATION PROFILE
// ============================================================================

bool writeCalibrationProfile(const uint8_t* buffer, uint16_t length) {
  // Input validation
  if (!buffer || length == 0) {
    snprintf(error_buffer_, sizeof(error_buffer_), "Invalid buffer or length");
    return false;
  }

  // Validate data before writing to sensor
  if (!validateCalibrationData(buffer, length)) {
    // Error message already set by validateCalibrationData
    return false;
  }

  // Convert 8-bit byte array to 32-bit word array
  // Round up to nearest multiple of 4 bytes
  uint16_t num_words = (length + 3) / 4;

  if (num_words > 64) {
    snprintf(error_buffer_, sizeof(error_buffer_),
             "Data too large: %u words (max 64)", num_words);
    return false;
  }

  // Pack bytes into 32-bit words (little-endian)
  uint32_t words_buffer[64];
  memset(words_buffer, 0, sizeof(words_buffer));

  for (uint16_t i = 0; i < length; i++) {
    uint16_t word_idx = i / 4;
    uint8_t byte_idx = i % 4;
    words_buffer[word_idx] |= ((uint32_t)buffer[i] << (byte_idx * 8));
  }

  // Write the DYNAMIC_CALIBRATION record to FRS
  int result = sh2_setFrs(DYNAMIC_CALIBRATION, words_buffer, num_words);

  if (result != SH2_OK) {
    snprintf(error_buffer_, sizeof(error_buffer_),
             "sh2_setFrs failed: %s (code %d)",
             sh2_error_to_string(result), result);
    return false;
  }

  snprintf(error_buffer_, sizeof(error_buffer_), "No error");
  return true;
}

// ============================================================================
// VALIDATE CALIBRATION DATA
// ============================================================================

bool validateCalibrationData(const uint8_t* data, uint16_t length) {
  /*
   * IMPLEMENTATION PLAN:
   *
   * Performs heuristic validation to catch obvious corrupted data.
   * This is NOT a complete format check (we don't know firmware structure),
   * but catches common error patterns.
   *
   * Checks:
   * 1. Length is reasonable (minimum 36 bytes, maximum 256 bytes)
   *    - BNO085 calibration is usually 50-72 bytes
   *    - Less than 36 would be incomplete
   *    - More than 256 would overflow our buffer
   *
   * 2. Data is not all zeros (0x00, 0x00, 0x00, ...)
   *    - Would indicate uninitialized memory
   *    - Calibration is never all zeros
   *
   * 3. Data is not all 0xFF (0xFF, 0xFF, 0xFF, ...)
   *    - Would indicate erased EEPROM or uninitialized memory
   *    - Unlikely to be valid calibration
   *
   * 4. Data contains some reasonable range of values
   *    - Should have both high and low bytes
   *    - Not all bytes in same value range
   *
   * 5. Optional: CRC check if we can determine the format
   *    - Low priority: BNO085 firmware will validate on write
   *
   * PSEUDO-CODE:
   *
   *   if (!data || length == 0) return false;
   *   if (length < 36 || length > 256) return false;
   *
   *   // Check for all-zeros
   *   bool all_zero = true, all_ff = true;
   *   uint8_t min_val = 0xFF, max_val = 0x00;
   *
   *   for (uint16_t i = 0; i < length; i++) {
   *       if (data[i] != 0x00) all_zero = false;
   *       if (data[i] != 0xFF) all_ff = false;
   *       if (data[i] < min_val) min_val = data[i];
   *       if (data[i] > max_val) max_val = data[i];
   *   }
   *
   *   if (all_zero || all_ff) return false;
   *
   *   // Check for reasonable data range (not all bytes same value)
   *   uint8_t range = max_val - min_val;
   *   if (range < 16) return false;  // Data too uniform
   *
   *   return true;
   */

  if (!data || length == 0) return false;

  // Check length is in reasonable range
  if (length < 36 || length > 256) return false;

  // Check for all-zeros or all-0xFF
  bool all_zero = true;
  bool all_ff = true;
  uint8_t min_val = 0xFF;
  uint8_t max_val = 0x00;

  for (uint16_t i = 0; i < length; i++) {
    if (data[i] != 0x00) all_zero = false;
    if (data[i] != 0xFF) all_ff = false;
    if (data[i] < min_val) min_val = data[i];
    if (data[i] > max_val) max_val = data[i];
  }

  if (all_zero || all_ff) {
    strcpy(error_buffer_, "Data is all zeros or all 0xFF");
    return false;
  }

  // Check data has reasonable range of values
  uint8_t range = max_val - min_val;
  if (range < 16) {
    strcpy(error_buffer_, "Data range too small (may be uninitialized)");
    return false;
  }

  strcpy(error_buffer_, "No error");
  return true;
}

// ============================================================================
// ERROR HANDLING
// ============================================================================

const char* getCalibrationError(void) {
  return error_buffer_;
}
