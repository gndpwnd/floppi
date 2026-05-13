/**
 * Persistent Storage HAL (Hardware Abstraction Layer)
 *
 * Provides a uniform byte-addressable persistent-storage API across multiple
 * MCU families. Replaces direct use of `<EEPROM.h>` so that platforms whose
 * EEPROM wrapper silently fails (notably ESP32) work correctly without any
 * source-level change in the calling code.
 *
 * ============================================================================
 * Why this layer exists
 * ============================================================================
 *
 * The current `calibration_storage.cpp` calls `EEPROM.read/write` directly.
 * Behaviour by platform:
 *
 *   AVR (Nano/Mega):    Real on-chip EEPROM. Byte-addressable. Writes happen
 *                       immediately (~3.3 ms/byte). No begin/commit needed.
 *
 *   Teensy 4.x:         PJRC's `<EEPROM.h>` is flash-emulated. Same API as AVR.
 *                       Writes are buffered. `EEPROM.update()` is preferred
 *                       (compare-before-write) to reduce flash wear.
 *
 *   ESP32 / ESP32-S3:   The Arduino-ESP32 `<EEPROM.h>` wrapper is deprecated
 *                       and requires `EEPROM.begin(size)` BEFORE any read/write
 *                       and `EEPROM.commit()` after writes. Without those calls
 *                       writes are silently dropped on reset. The preferred
 *                       native path is the `Preferences` (NVS) API.
 *
 * This HAL hides those differences. Callers see a flat byte-address space
 * starting at offset 0; the back-end maps that onto the platform's native
 * persistent store.
 *
 * ============================================================================
 * API contract
 * ============================================================================
 *
 *   ps::begin(capacity_hint)
 *     Must be called once early in setup() before any read/write/clear call.
 *     `capacity_hint` is advisory: AVR ignores it (EEPROM size is fixed at
 *     link time); ESP32 passes it to the NVS / EEPROM-compat layer.
 *     Returns true on success.
 *
 *   ps::read(offset, buf, len)
 *     Reads `len` bytes starting at `offset` into `buf`.
 *     Returns false if (offset + len) exceeds capacity() or buf is NULL.
 *
 *   ps::write(offset, buf, len)
 *     Writes `len` bytes from `buf` starting at `offset`.
 *     Returns false on bounds or argument error.
 *     On AVR/Teensy, bytes are committed immediately.
 *     On ESP32, may be buffered until commit() depending on back-end.
 *
 *   ps::commit()
 *     Flushes any pending writes to non-volatile storage.
 *     - AVR:    no-op (returns true) — every write() is immediate.
 *     - Teensy: no-op (returns true) — EEPROM.update() commits per write.
 *     - ESP32:  forces NVS / EEPROM commit. Call once after a batch of writes.
 *     Returns true on success.
 *
 *   ps::clear(offset, len)
 *     Marks the region [offset, offset+len) as erased. Subsequent reads
 *     return 0xFF (matching the behaviour of a freshly-erased EEPROM cell).
 *     Out-of-range or NULL-buf-equivalent inputs are silently ignored.
 *
 *   ps::capacity()
 *     Returns the total byte capacity of the back-end. Zero if begin() has
 *     not yet been called or failed.
 *
 * ============================================================================
 * Integer width on size_t
 * ============================================================================
 *
 * `size_t` is uint16_t on AVR and uint32_t on 32-bit MCUs. To make the public
 * API portable without forcing every caller to cast, the API uses uint16_t
 * directly. The maximum addressable region per call is 65535 bytes — far
 * larger than any current or planned calibration blob.
 */

#ifndef PERSISTENT_STORAGE_H
#define PERSISTENT_STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

namespace ps {

  /**
   * @brief Initialise the persistent-storage back-end.
   *
   * Must be called once during setup() before any read/write/clear/commit.
   *
   * @param capacity_hint  Advisory capacity in bytes. AVR ignores; ESP32 uses
   *                       it to size the NVS / EEPROM region (default 512).
   * @return true on success, false if the underlying driver failed to start.
   */
  bool begin(uint16_t capacity_hint = 512);

  /**
   * @brief Read raw bytes from persistent storage.
   *
   * @param offset  Byte offset (0-based) into the storage region.
   * @param buf     Destination buffer (must not be NULL).
   * @param len     Number of bytes to read.
   * @return true on success; false on NULL buf, len == 0, or out-of-range.
   */
  bool read(uint16_t offset, uint8_t* buf, uint16_t len);

  /**
   * @brief Write raw bytes to persistent storage.
   *
   * On AVR/Teensy this commits immediately. On ESP32 the data may be buffered
   * until commit() is called; callers should issue ps::commit() once at the
   * end of a batch of writes to guarantee persistence across reset.
   *
   * @param offset  Byte offset (0-based) into the storage region.
   * @param buf     Source buffer (must not be NULL).
   * @param len     Number of bytes to write.
   * @return true on success; false on NULL buf, len == 0, or out-of-range.
   */
  bool write(uint16_t offset, const uint8_t* buf, uint16_t len);

  /**
   * @brief Flush any pending writes to non-volatile storage.
   *
   * No-op on AVR and Teensy back-ends (writes are already persistent).
   * On ESP32 this calls the underlying NVS / EEPROM commit.
   *
   * @return true on success; false if the commit failed.
   */
  bool commit();

  /**
   * @brief Erase a region of persistent storage to 0xFF.
   *
   * Subsequent reads in the cleared region return 0xFF, matching the
   * behaviour of a freshly-erased EEPROM cell.
   *
   * @param offset  Byte offset where the clear begins.
   * @param len     Number of bytes to clear.
   */
  void clear(uint16_t offset, uint16_t len);

  /**
   * @brief Total byte capacity of the persistent-storage back-end.
   *
   * @return Capacity in bytes. Zero if begin() has not yet been called or
   *         the back-end failed to initialise.
   */
  uint16_t capacity();

} // namespace ps

#endif // PERSISTENT_STORAGE_H
