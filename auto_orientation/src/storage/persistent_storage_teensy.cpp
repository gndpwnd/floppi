/**
 * Persistent Storage HAL — Teensy 4.x back-end
 *
 * Teensy 4.0 / 4.1 have no real on-chip EEPROM, but PJRC's `<EEPROM.h>`
 * library provides a flash-emulated EEPROM that is fully API-compatible with
 * AVR. Usable capacity is 4080 B (T4.0) / 4284 B (T4.1).
 *
 * The emulation uses wear-levelling internally and writes are buffered until
 * the cell value actually changes — using `EEPROM.update()` rather than
 * `EEPROM.write()` is essential to avoid burning through flash cycles when
 * the calibration blob is rewritten on every boot.
 *
 * `ps::commit()` is a no-op: the PJRC library commits per-update.
 *
 * Guarded by `__IMXRT1062__` (the Teensy 4.x SoC macro) so it is only
 * compiled for Teensy 4.0 / 4.1.
 */

#ifdef __IMXRT1062__

#include "persistent_storage.h"
#include <EEPROM.h>

namespace ps {

  static uint16_t g_capacity = 0;
  static bool g_initialised = false;

  // ==========================================================================
  // begin()
  // ==========================================================================
  bool begin(uint16_t /*capacity_hint*/) {
    /*
     * Teensy's EEPROM size is fixed at link time; the hint is advisory.
     * EEPROM.length() returns the platform-specific size.
     */
    g_capacity = (uint16_t)EEPROM.length();
    g_initialised = true;
    return true;
  }

  // ==========================================================================
  // read()
  // ==========================================================================
  bool read(uint16_t offset, uint8_t* buf, uint16_t len) {
    if (!g_initialised || !buf || len == 0) {
      return false;
    }
    if ((uint32_t)offset + (uint32_t)len > (uint32_t)g_capacity) {
      return false;
    }

    for (uint16_t i = 0; i < len; i++) {
      buf[i] = EEPROM.read(offset + i);
    }
    return true;
  }

  // ==========================================================================
  // write()
  // ==========================================================================
  bool write(uint16_t offset, const uint8_t* buf, uint16_t len) {
    if (!g_initialised || !buf || len == 0) {
      return false;
    }
    if ((uint32_t)offset + (uint32_t)len > (uint32_t)g_capacity) {
      return false;
    }

    // EEPROM.update() compares-before-writes — critical on flash-emulated
    // EEPROM to avoid unnecessary erase/write cycles on the underlying flash.
    for (uint16_t i = 0; i < len; i++) {
      EEPROM.update(offset + i, buf[i]);
    }
    return true;
  }

  // ==========================================================================
  // commit()
  // ==========================================================================
  bool commit() {
    /*
     * No batched commit needed: PJRC's EEPROM library persists each update
     * to flash internally. Returning true keeps the cross-platform contract.
     */
    return g_initialised;
  }

  // ==========================================================================
  // clear()
  // ==========================================================================
  void clear(uint16_t offset, uint16_t len) {
    if (!g_initialised || len == 0) {
      return;
    }
    if ((uint32_t)offset + (uint32_t)len > (uint32_t)g_capacity) {
      return;
    }

    for (uint16_t i = 0; i < len; i++) {
      EEPROM.update(offset + i, 0xFF);
    }
  }

  // ==========================================================================
  // capacity()
  // ==========================================================================
  uint16_t capacity() {
    return g_capacity;
  }

} // namespace ps

#endif // __IMXRT1062__
