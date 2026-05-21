/**
 * Guided-tuning EEPROM persistence — implementation.
 *
 * See tune_storage.h for the 19-byte block layout. Uses Arduino <EEPROM.h>
 * directly (this is the Uno / AVR — that is the correct, lightest API).
 *
 * The whole file is guarded for AVR: on a non-AVR host syntax-check or native
 * test build there is no EEPROM hardware, so the functions compile to harmless
 * no-ops. Matches the guard idiom in uno_balance_app.cpp.
 */

#include "tune_storage.h"

#if defined(__AVR__) || defined(ARDUINO_ARCH_AVR)

#include <EEPROM.h>
#include <string.h>

// CRC-8-CCITT (poly 0x07, init 0x00) — the shared leaf in src/util/crc8.h.
// This module used to carry a byte-identical private crc8_ccitt() copy; it now
// delegates to the one canonical implementation (tech-debt finding R1,
// docs/findings/ao_uno_techdebt_2026-05-20.md §D1). The header is inline-only,
// so it adds no link-time dependency — important for this Uno env's
// build_src_filter and for the native test compile. CRC output is unchanged.
#include "../../util/crc8.h"

namespace {

// In-EEPROM byte offsets relative to TUNE_EEPROM_BASE.
const int OFF_MARKER  = 0;   // 0x200
const int OFF_VERSION = 1;   // 0x201
const int OFF_PAYLOAD = 2;   // 0x202 — 16 bytes of float data
const int OFF_CRC     = 18;  // 0x212

const size_t PAYLOAD_BYTES = 16;       // 4 floats
const size_t CRC_SPAN      = 1 + 16;   // version + payload = bytes 0x201..0x211

}  // namespace

namespace tune_storage {

bool save_tuning(const TuneBlock& blk) {
  // Serialize version + four floats into a contiguous buffer so the CRC is
  // computed over exactly the same bytes that land in EEPROM.
  uint8_t buf[CRC_SPAN];
  buf[0] = TUNE_VERSION;
  memcpy(&buf[1], &blk, PAYLOAD_BYTES);

  const uint8_t crc = util::crc8_ccitt(buf, CRC_SPAN);

  // update() only re-burns bytes that actually changed — spares EEPROM wear.
  EEPROM.update(TUNE_EEPROM_BASE + OFF_MARKER, TUNE_MARKER);
  for (size_t i = 0; i < CRC_SPAN; i++) {
    EEPROM.update(TUNE_EEPROM_BASE + OFF_VERSION + (int)i, buf[i]);
  }
  EEPROM.update(TUNE_EEPROM_BASE + OFF_CRC, crc);
  return true;
}

bool load_tuning(TuneBlock& out) {
  if (EEPROM.read(TUNE_EEPROM_BASE + OFF_MARKER) != TUNE_MARKER) {
    return false;  // empty / never written
  }

  // Read version + payload into one buffer for CRC verification.
  uint8_t buf[CRC_SPAN];
  for (size_t i = 0; i < CRC_SPAN; i++) {
    buf[i] = EEPROM.read(TUNE_EEPROM_BASE + OFF_VERSION + (int)i);
  }

  if (buf[0] != TUNE_VERSION) {
    return false;  // wrong format version
  }

  const uint8_t stored_crc = EEPROM.read(TUNE_EEPROM_BASE + OFF_CRC);
  if (util::crc8_ccitt(buf, CRC_SPAN) != stored_crc) {
    return false;  // corrupt
  }

  memcpy(&out, &buf[1], PAYLOAD_BYTES);
  return true;
}

bool has_tuning() {
  TuneBlock tmp;
  return load_tuning(tmp);  // marker + version + CRC all checked here
}

}  // namespace tune_storage

#else  // ---- non-AVR (host syntax-check / native test) ---------------------

namespace tune_storage {
bool save_tuning(const TuneBlock&) { return false; }
bool load_tuning(TuneBlock&)       { return false; }
bool has_tuning()                  { return false; }
}  // namespace tune_storage

#endif  // __AVR__
