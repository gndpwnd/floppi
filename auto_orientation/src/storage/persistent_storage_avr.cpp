/**
 * Persistent Storage HAL — AVR back-end
 *
 * Wraps the Arduino `<EEPROM.h>` library on AVR boards (Nano, Mega, Uno, etc.).
 * On AVR the EEPROM is real on-chip non-volatile memory:
 *   - Byte-addressable, no begin()/commit() needed
 *   - Writes take ~3.3 ms each and persist immediately
 *   - Capacity is fixed by the chip (Nano: 1024 B, Mega: 4096 B)
 *
 * `ps::commit()` is therefore a no-op on this back-end.
 *
 * Guarded by `ARDUINO_ARCH_AVR` so it is only compiled on AVR platforms.
 */

#ifdef ARDUINO_ARCH_AVR

#include "persistent_storage.h"
#include <EEPROM.h>

namespace ps {

  // AVR EEPROM size is known at link time via EEPROM.length(); cached on begin().
  static uint16_t g_capacity = 0;
  static bool g_initialised = false;

  // ==========================================================================
  // begin()
  // ==========================================================================
  bool begin(uint16_t /*capacity_hint*/) {
    /*
     * AVR EEPROM is always available; nothing to initialise beyond caching the
     * actual hardware size. The capacity_hint is advisory and ignored here
     * because the EEPROM size is fixed by the chip.
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

    // Use EEPROM.update() (compare-before-write) to spare wear on cells whose
    // value is already correct. Each write is immediately persistent.
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
     * AVR EEPROM writes are immediate; no batched commit is needed. Returning
     * true keeps caller code identical across platforms.
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
      // Silently ignore out-of-range — matches "best-effort erase" semantics.
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

#endif // ARDUINO_ARCH_AVR
