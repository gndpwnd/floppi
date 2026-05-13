/**
 * Persistent Storage HAL — ESP32 back-end
 *
 * Uses the native `Preferences` library (NVS) rather than the deprecated
 * Arduino-ESP32 `<EEPROM.h>` compatibility wrapper. Reasons:
 *   - The deprecated wrapper requires explicit `begin(size)` and `commit()`;
 *     it has known silent-failure modes if begin() is missed.
 *   - NVS handles wear-levelling, partition management, and atomic commits.
 *   - `Preferences` is part of the ESP32 Arduino core (no extra lib_deps).
 *
 * We synthesise a flat byte-address space on top of NVS by storing a single
 * blob keyed `"data"` under the `"auto_orient"` namespace. Reads and writes
 * operate on an in-RAM mirror; ps::commit() flushes the mirror to NVS.
 *
 * This keeps the API identical to AVR: callers write bytes at offsets and
 * call commit() once at the end of a batch. The first ps::begin() pulls the
 * existing blob (if any) into RAM so subsequent reads work even before a
 * write has occurred.
 *
 * Guarded by `ARDUINO_ARCH_ESP32` so it is only compiled on ESP32 platforms.
 *
 * CRITICAL: do NOT use the deprecated `<EEPROM.h>` wrapper here.
 */

#ifdef ARDUINO_ARCH_ESP32

#include "persistent_storage.h"
#include <Preferences.h>
#include <string.h>

namespace ps {

  // NVS namespace and key used to hold the calibration blob.
  static const char* NVS_NAMESPACE = "auto_orient";
  static const char* NVS_BLOB_KEY  = "data";

  // Preferences handle and RAM mirror of the persistent blob.
  static Preferences g_prefs;
  static uint8_t*    g_buffer        = nullptr;
  static uint16_t    g_capacity      = 0;
  static bool        g_initialised   = false;
  static bool        g_dirty         = false;  // true if buffer differs from NVS

  // ==========================================================================
  // begin()
  // ==========================================================================
  bool begin(uint16_t capacity_hint) {
    /*
     * Allocate a RAM mirror of size capacity_hint, open the NVS namespace
     * read/write, and pull any previously stored blob into the mirror.
     * If no blob exists yet, the mirror is initialised to 0xFF (matches a
     * freshly-erased EEPROM cell).
     */
    if (capacity_hint == 0) {
      return false;
    }

    // Idempotent re-init: free old buffer if present.
    if (g_buffer) {
      delete[] g_buffer;
      g_buffer = nullptr;
    }

    g_buffer = new uint8_t[capacity_hint];
    if (!g_buffer) {
      g_capacity = 0;
      g_initialised = false;
      return false;
    }

    // Initial fill = 0xFF (treat as "empty" so legacy code paths behave).
    memset(g_buffer, 0xFF, capacity_hint);
    g_capacity = capacity_hint;

    // Open NVS namespace for read/write. The second arg `false` = RW mode.
    if (!g_prefs.begin(NVS_NAMESPACE, false)) {
      delete[] g_buffer;
      g_buffer = nullptr;
      g_capacity = 0;
      g_initialised = false;
      return false;
    }

    // Pull existing blob if present. getBytes() returns 0 if key not found.
    size_t stored_len = g_prefs.getBytesLength(NVS_BLOB_KEY);
    if (stored_len > 0) {
      uint16_t read_len = (stored_len > capacity_hint) ? capacity_hint
                                                       : (uint16_t)stored_len;
      g_prefs.getBytes(NVS_BLOB_KEY, g_buffer, read_len);
    }

    g_dirty = false;
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

    // Only mark dirty if the new bytes actually differ — avoids unnecessary
    // NVS commits when commit() is later called.
    if (memcmp(g_buffer + offset, buf, len) != 0) {
      memcpy(g_buffer + offset, buf, len);
      g_dirty = true;
    }
    return true;
  }

  // ==========================================================================
  // commit()
  // ==========================================================================
  bool commit() {
    /*
     * Flush the RAM mirror to NVS as a single blob. Preferences.putBytes()
     * commits atomically; on success the data survives a reset.
     *
     * No-op if nothing has changed since the last commit.
     */
    if (!g_initialised) {
      return false;
    }
    if (!g_dirty) {
      return true;
    }

    size_t written = g_prefs.putBytes(NVS_BLOB_KEY, g_buffer, g_capacity);
    if (written != g_capacity) {
      return false;
    }
    g_dirty = false;
    return true;
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

    // Set region to 0xFF (matches an erased EEPROM cell). Mark dirty so the
    // next commit() persists the erasure.
    for (uint16_t i = 0; i < len; i++) {
      if (g_buffer[offset + i] != 0xFF) {
        g_buffer[offset + i] = 0xFF;
        g_dirty = true;
      }
    }
  }

  // ==========================================================================
  // capacity()
  // ==========================================================================
  uint16_t capacity() {
    return g_capacity;
  }

} // namespace ps

#endif // ARDUINO_ARCH_ESP32
