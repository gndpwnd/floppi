/**
 * Guided-tuning EEPROM persistence — implementation.
 *
 * See tune_storage.h for the 19-byte PID block and variable-length cal-blob
 * layouts. Uses Arduino <EEPROM.h> directly (this is the Uno / AVR — that is
 * the correct, lightest API).
 *
 * The whole file is guarded for AVR: on a non-AVR host syntax-check or native
 * test build there is no EEPROM hardware, so the functions compile to harmless
 * no-ops. Matches the guard idiom in uno_balance_app.cpp.
 */

#include "tune_storage.h"

#if defined(__AVR__) || defined(ARDUINO_ARCH_AVR)

#include <Arduino.h>     // Serial, F(), dtostrf
#include <EEPROM.h>
#include <string.h>
#include <stdlib.h>      // dtostrf

// CRC-8-CCITT (poly 0x07, init 0x00) — the shared leaf in src/util/crc8.h.
// This module used to carry a byte-identical private crc8_ccitt() copy; it now
// delegates to the one canonical implementation (tech-debt finding R1,
// docs/findings/ao_uno_techdebt_2026-05-20.md §D1). The header is inline-only,
// so it adds no link-time dependency — important for this Uno env's
// build_src_filter and for the native test compile. CRC output is unchanged.
#include "../../util/crc8.h"

namespace {

// ---- TuneBlock (PID) region offsets — relative to TUNE_EEPROM_BASE -------
const int OFF_MARKER  = 0;   // 0x200
const int OFF_VERSION = 1;   // 0x201
const int OFF_PAYLOAD = 2;   // 0x202 — 16 bytes of float data
const int OFF_CRC     = 18;  // 0x212

const size_t PAYLOAD_BYTES = 16;       // 4 floats
const size_t CRC_SPAN      = 1 + 16;   // version + payload = bytes 0x201..0x211

// ---- Cal-blob region offsets — relative to TUNE_CAL_BASE -----------------
// Layout: marker | version | len_lo | len_hi | payload[len] | crc8
//         0       1         2        3        4..4+len-1     4+len
const int CAL_OFF_MARKER  = 0;          // 0x220
const int CAL_OFF_VERSION = 1;          // 0x221
const int CAL_OFF_LEN_LO  = 2;          // 0x222
const int CAL_OFF_LEN_HI  = 3;          // 0x223
const int CAL_OFF_PAYLOAD = 4;          // 0x224..

// CRC covers version + 2-byte length + payload (1 + 2 + len bytes total).
inline size_t cal_crc_span_(uint16_t payload_len) {
  return 1 + 2 + payload_len;
}

// Print one byte as two uppercase hex digits to Serial.
inline void print_hex_byte_(uint8_t b) {
  if (b < 16) Serial.print(F("0"));
  Serial.print(b, HEX);
}

// Format a float with 4 decimal places into a local buffer and print it. The
// guided-tuning session uses dtostrf with 1 decimal for terse status output;
// the photo-backup needs 4 decimals so the operator-pasted seed exactly equals
// what was running on the chip.
inline void print_float_4dp_(float v) {
  char buf[16];
  dtostrf(v, 0, 4, buf);
  Serial.print(buf);
}

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

// ---------------------------------------------------------------------------
// Variable-length IMU cal blob — marker | version | len(2) | payload | CRC.
// Slot sized for TUNE_CAL_MAX_BYTES so the same EEPROM layout serves BNO055
// (22 B) and BNO085 (~36-72 B) without reformatting.
// ---------------------------------------------------------------------------

bool save_cal_blob(const uint8_t* blob, uint16_t len) {
  if (!blob) return false;
  if (len == 0 || len > TUNE_CAL_MAX_BYTES) return false;

  // Build version + len(2) + payload contiguously so the CRC covers exactly
  // the bytes that land in EEPROM. Stack budget on Uno is fine — 1 + 2 + 72 =
  // 75 bytes worst case, well under the per-frame envelope here.
  uint8_t buf[1 + 2 + TUNE_CAL_MAX_BYTES];
  buf[0] = TUNE_CAL_VERSION;
  buf[1] = (uint8_t)(len & 0xFF);          // len_lo
  buf[2] = (uint8_t)((len >> 8) & 0xFF);   // len_hi
  memcpy(&buf[3], blob, len);

  const size_t span = cal_crc_span_(len);
  const uint8_t crc = util::crc8_ccitt(buf, span);

  EEPROM.update(TUNE_CAL_BASE + CAL_OFF_MARKER, TUNE_CAL_MARKER);
  for (size_t i = 0; i < span; i++) {
    // CAL_OFF_VERSION is the first byte of the buf[] image (version), so the
    // mapping is buf[i] -> CAL_OFF_VERSION + i in the EEPROM image.
    EEPROM.update(TUNE_CAL_BASE + CAL_OFF_VERSION + (int)i, buf[i]);
  }
  EEPROM.update(TUNE_CAL_BASE + (int)(CAL_OFF_VERSION + span), crc);
  return true;
}

bool load_cal_blob(uint8_t* out, uint16_t out_capacity, uint16_t* out_len) {
  if (!out || !out_len) return false;

  if (EEPROM.read(TUNE_CAL_BASE + CAL_OFF_MARKER) != TUNE_CAL_MARKER) {
    return false;  // empty / never written
  }

  const uint8_t ver = EEPROM.read(TUNE_CAL_BASE + CAL_OFF_VERSION);
  if (ver != TUNE_CAL_VERSION) {
    return false;  // wrong format version (e.g. old 22-only blob)
  }

  const uint8_t len_lo = EEPROM.read(TUNE_CAL_BASE + CAL_OFF_LEN_LO);
  const uint8_t len_hi = EEPROM.read(TUNE_CAL_BASE + CAL_OFF_LEN_HI);
  const uint16_t len = (uint16_t)len_lo | ((uint16_t)len_hi << 8);

  if (len == 0 || len > TUNE_CAL_MAX_BYTES) {
    return false;  // corrupt / impossible length
  }
  if (len > out_capacity) {
    return false;  // caller buffer too small
  }

  // Reassemble version + len(2) + payload so the CRC check sees the same bytes
  // save_cal_blob() hashed. Use the same envelope sizing.
  uint8_t buf[1 + 2 + TUNE_CAL_MAX_BYTES];
  buf[0] = ver;
  buf[1] = len_lo;
  buf[2] = len_hi;
  for (uint16_t i = 0; i < len; i++) {
    buf[3 + i] = EEPROM.read(TUNE_CAL_BASE + CAL_OFF_PAYLOAD + (int)i);
  }

  const size_t span = cal_crc_span_(len);
  const uint8_t stored_crc =
      EEPROM.read(TUNE_CAL_BASE + (int)(CAL_OFF_VERSION + span));
  if (util::crc8_ccitt(buf, span) != stored_crc) {
    return false;  // corrupt
  }

  memcpy(out, &buf[3], len);
  *out_len = len;
  return true;
}

bool has_cal_blob() {
  // Validate via a full load. The local buffer is sized for the worst case so
  // we never spuriously fail with "too small" here.
  uint8_t tmp[TUNE_CAL_MAX_BYTES];
  uint16_t tmp_len = 0;
  return load_cal_blob(tmp, sizeof(tmp), &tmp_len);
}

// ---------------------------------------------------------------------------
// Photo-backup printer — output format is FROZEN; sibling agent's tuning_session
// also calls this on commit so the operator gets a single, consistent block to
// photograph regardless of which session produced the new value.
// ---------------------------------------------------------------------------

void print_photo_backup(bool pid_valid, const TuneBlock& pid,
                        bool cal_valid, const uint8_t* cal, uint16_t cal_len,
                        const char* imu_tag) {
  Serial.println(F("==== PHOTO-BACKUP -- paste into balance_constants.h ===="));
  Serial.println(F("// Uno guided session output. Photograph this block."));

  if (pid_valid) {
    Serial.print(F("static const float BALANCE_KP       = "));
    print_float_4dp_(pid.kp);
    Serial.println(F("f;"));

    Serial.print(F("static const float BALANCE_KI       = "));
    print_float_4dp_(pid.ki);
    Serial.println(F("f;"));

    Serial.print(F("static const float BALANCE_KD       = "));
    print_float_4dp_(pid.kd);
    Serial.println(F("f;"));

    Serial.print(F("static const float PITCH_OFFSET_DEG = "));
    print_float_4dp_(pid.pitch_off);
    Serial.println(F("f;"));
  }

  if (cal_valid && cal && imu_tag && cal_len > 0) {
    // Header comment: "<TAG> <len>-byte cal blob ..."
    Serial.print(F("// "));
    Serial.print(imu_tag);
    Serial.print(F(" "));
    Serial.print(cal_len);
    Serial.println(F("-byte cal blob (hex, MSB-first as stored in EEPROM):"));

    // Identifier: "<TAG>_CAL_BLOB[<len>]"
    Serial.print(F("static const uint8_t "));
    Serial.print(imu_tag);
    Serial.print(F("_CAL_BLOB["));
    Serial.print(cal_len);
    Serial.println(F("] = {"));

    Serial.print(F("  "));
    for (uint16_t i = 0; i < cal_len; i++) {
      Serial.print(F("0x"));
      print_hex_byte_(cal[i]);
      if (i + 1 < cal_len) Serial.print(F(", "));
    }
    Serial.println();
    Serial.println(F("};"));
  }

  Serial.println(F("==== END PHOTO-BACKUP -- PHOTOGRAPH THIS SCROLLBACK ===="));
}

}  // namespace tune_storage

#else  // ---- non-AVR (host syntax-check / native test) ---------------------

namespace tune_storage {
bool save_tuning(const TuneBlock&) { return false; }
bool load_tuning(TuneBlock&)       { return false; }
bool has_tuning()                  { return false; }

bool save_cal_blob(const uint8_t*, uint16_t)             { return false; }
bool load_cal_blob(uint8_t*, uint16_t, uint16_t*)        { return false; }
bool has_cal_blob()                                       { return false; }

// Non-AVR is host-only (syntax-check / native tests); no Serial available, so
// the photo printer compiles to a silent no-op there.
void print_photo_backup(bool, const TuneBlock&,
                        bool, const uint8_t*, uint16_t,
                        const char*) {}
}  // namespace tune_storage

#endif  // __AVR__
