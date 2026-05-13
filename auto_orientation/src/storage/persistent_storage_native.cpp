/**
 * Persistent Storage HAL — Native (host) back-end
 *
 * Heap-backed buffer used by host-side Unity tests under PlatformIO's
 * `platform = native` toolchain. There is no real persistence — the buffer
 * lives only for the lifetime of the test process — but the API contract
 * (round-trip, bounds-checking, clear-to-0xFF, commit() no-op) matches the
 * embedded back-ends exactly.
 *
 * Capacity defaults to 1024 B (configurable via begin(capacity_hint)) so
 * boundary tests can run without exhausting host memory.
 *
 * Guarded so it is ONLY compiled when none of the embedded MCU macros are
 * defined. The PlatformIO test env uses build_src_filter to exclude the
 * AVR/ESP32/Teensy back-ends, so this file is the sole implementation in
 * that build.
 */

#if !defined(ARDUINO_ARCH_AVR) && !defined(ARDUINO_ARCH_ESP32) && !defined(__IMXRT1062__)

#include "persistent_storage.h"
#include <string.h>
#include <stdlib.h>

namespace ps {

  static uint8_t* g_buffer = nullptr;
  static uint16_t g_capacity = 0;
  static bool g_initialised = false;

  // ==========================================================================
  // begin()
  // ==========================================================================
  bool begin(uint16_t capacity_hint) {
    /*
     * Allocate a heap buffer of size capacity_hint, filled with 0xFF to mimic
     * a freshly-erased EEPROM. Re-calling begin() releases the prior buffer
     * (idempotent), so individual tests can reset between cases.
     */
    if (capacity_hint == 0) {
      return false;
    }

    if (g_buffer) {
      free(g_buffer);
      g_buffer = nullptr;
    }

    g_buffer = (uint8_t*)malloc(capacity_hint);
    if (!g_buffer) {
      g_capacity = 0;
      g_initialised = false;
      return false;
    }

    memset(g_buffer, 0xFF, capacity_hint);
    g_capacity = capacity_hint;
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

    memcpy(buf, g_buffer + offset, len);
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

    memcpy(g_buffer + offset, buf, len);
    return true;
  }

  // ==========================================================================
  // commit()
  // ==========================================================================
  bool commit() {
    // Nothing to flush; the buffer is already authoritative.
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

    memset(g_buffer + offset, 0xFF, len);
  }

  // ==========================================================================
  // capacity()
  // ==========================================================================
  uint16_t capacity() {
    return g_capacity;
  }

} // namespace ps

#endif // native guard
