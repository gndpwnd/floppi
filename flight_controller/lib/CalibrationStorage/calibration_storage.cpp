/**
 * Calibration Storage — EEPROM-backed persistence (implementation).
 *
 * See calibration_storage.h for the public contract and rationale.
 *
 * Notes on the backend split:
 *   ESP32:  Arduino-ESP32's <EEPROM.h> wraps NVS. It REQUIRES
 *           EEPROM.begin(size) before any read/write and EEPROM.commit()
 *           after a batch of writes — otherwise writes are silently dropped
 *           on reset (the same KI-1 footgun the AO HAL warns about).
 *   AVR/Teensy:  Native / flash-emulated EEPROM. begin/commit are no-ops;
 *           every EEPROM.write() is immediately persistent.
 *
 * The shim around those differences lives below as cs_begin_backend(),
 * cs_commit_backend(), and the read/write byte helpers. The public functions
 * are otherwise byte-identical in behaviour to the AO post-P1 implementation.
 *
 * CRC-8-CCITT is vendored inline as a small static helper (one TU, ~20 lines)
 * rather than as a separate crc8.h/.cpp — keeps the library self-contained
 * with no extra files for the LDF to discover. Parameters match AO exactly
 * (poly 0x07, init 0x00, no reflect, no final XOR) so blobs round-trip
 * across projects.
 */

#include "calibration_storage.h"
#include <EEPROM.h>

// ============================================================================
// CRC-8-CCITT (vendored inline; matches auto_orientation/src/util/crc8.h)
// ============================================================================

static uint8_t cs_crc8_ccitt(const uint8_t* data, uint16_t len) {
  if (!data || len == 0) return 0;
  uint8_t crc = 0x00;
  for (uint16_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 0x80) {
        crc = (uint8_t)((crc << 1) ^ 0x07);
      } else {
        crc = (uint8_t)(crc << 1);
      }
    }
  }
  return crc;
}

// ============================================================================
// Backend shim — wraps <EEPROM.h> differences across boards
// ============================================================================

static bool s_backend_ready = false;

static bool cs_begin_backend(void) {
#if defined(ESP32) || defined(USE_ESP32)
  // ESP32: must call EEPROM.begin(size) before any read/write or the writes
  // are silently dropped on reset. Returns false if NVS init failed.
  if (!EEPROM.begin(CAL_EEPROM_SIZE)) {
    return false;
  }
#endif
  // AVR / Teensy: EEPROM is always available; no begin() call needed.
  s_backend_ready = true;
  return true;
}

static bool cs_commit_backend(void) {
#if defined(ESP32) || defined(USE_ESP32)
  // ESP32: required to flush buffered writes to NVS.
  return EEPROM.commit();
#else
  // AVR / Teensy: writes are already persistent.
  return true;
#endif
}

static inline uint8_t cs_read_byte(uint16_t offset) {
  return EEPROM.read(offset);
}

static inline void cs_write_byte(uint16_t offset, uint8_t value) {
#if defined(ESP32) || defined(USE_ESP32)
  EEPROM.write(offset, value);
#else
  // Teensy/AVR: EEPROM.update() skips the actual write when the cell already
  // holds `value` — saves flash/EEPROM endurance for unchanged bytes.
  EEPROM.update(offset, value);
#endif
}

// ============================================================================
// Public API
// ============================================================================

bool cs_begin(void) {
  return cs_begin_backend();
}

bool cs_save(const uint8_t* blob, uint16_t len) {
  if (!s_backend_ready) {
    // Defensive auto-begin: lets a caller that forgot cs_begin() still work
    // on AVR/Teensy, and gives a chance on ESP32 if NVS is now available.
    if (!cs_begin_backend()) return false;
  }
  if (!blob || len == 0) return false;
  if (len > CAL_DATA_MAX_SIZE) return false;

  const uint8_t crc = cs_crc8_ccitt(blob, len);

  // Write 6-byte header.
  cs_write_byte(CAL_EEPROM_BASE + CAL_EEPROM_MARKER_OFFSET,   CAL_MARKER_VALID);
  cs_write_byte(CAL_EEPROM_BASE + CAL_EEPROM_VERSION_OFFSET,  CAL_FORMAT_VERSION);
  cs_write_byte(CAL_EEPROM_BASE + CAL_EEPROM_LENGTH_LO_OFFSET, (uint8_t)(len & 0xFF));
  cs_write_byte(CAL_EEPROM_BASE + CAL_EEPROM_LENGTH_HI_OFFSET, (uint8_t)((len >> 8) & 0xFF));
  cs_write_byte(CAL_EEPROM_BASE + CAL_EEPROM_CRC_OFFSET,      crc);
  cs_write_byte(CAL_EEPROM_BASE + CAL_EEPROM_RESERVED_OFFSET, 0x00);

  // Write payload.
  for (uint16_t i = 0; i < len; i++) {
    cs_write_byte(CAL_EEPROM_BASE + CAL_EEPROM_PAYLOAD_OFFSET + i, blob[i]);
  }

  // Flush — required on ESP32 (NVS), no-op on AVR/Teensy.
  return cs_commit_backend();
}

bool cs_load(uint8_t* out, size_t out_cap, uint16_t* out_len) {
  if (!s_backend_ready) {
    if (!cs_begin_backend()) return false;
  }
  if (!out || !out_len) return false;

  const uint8_t marker     = cs_read_byte(CAL_EEPROM_BASE + CAL_EEPROM_MARKER_OFFSET);
  const uint8_t version    = cs_read_byte(CAL_EEPROM_BASE + CAL_EEPROM_VERSION_OFFSET);
  const uint8_t len_lo     = cs_read_byte(CAL_EEPROM_BASE + CAL_EEPROM_LENGTH_LO_OFFSET);
  const uint8_t len_hi     = cs_read_byte(CAL_EEPROM_BASE + CAL_EEPROM_LENGTH_HI_OFFSET);
  const uint8_t stored_crc = cs_read_byte(CAL_EEPROM_BASE + CAL_EEPROM_CRC_OFFSET);

  if (marker != CAL_MARKER_VALID) return false;          // empty / erased
  if (version != CAL_FORMAT_VERSION) return false;       // reject legacy v1

  const uint16_t stored_length = (uint16_t)len_lo | ((uint16_t)len_hi << 8);
  if (stored_length == 0 || stored_length > CAL_DATA_MAX_SIZE) return false;
  if ((size_t)stored_length > out_cap) return false;     // overflow guard

  // Read payload.
  for (uint16_t i = 0; i < stored_length; i++) {
    out[i] = cs_read_byte(CAL_EEPROM_BASE + CAL_EEPROM_PAYLOAD_OFFSET + i);
  }

  // Verify CRC.
  if (cs_crc8_ccitt(out, stored_length) != stored_crc) {
    return false;
  }

  *out_len = stored_length;
  return true;
}

bool cs_has_valid(void) {
  if (!s_backend_ready) {
    if (!cs_begin_backend()) return false;
  }
  return cs_read_byte(CAL_EEPROM_BASE + CAL_EEPROM_MARKER_OFFSET) == CAL_MARKER_VALID;
}
