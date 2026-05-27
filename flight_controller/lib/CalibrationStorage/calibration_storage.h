/**
 * Calibration Storage — EEPROM-backed persistence for IMU calibration.
 *
 * Vendored from auto_orientation/src/config/calibration_storage.{h,cpp} (post-
 * 2026-05-20 P1 security pass): CRC-8-CCITT, uint16_t length field, buf_capacity
 * overflow guard, version-byte refusal of legacy v1 blobs.
 *
 * Why it lives here (lib/CalibrationStorage/) and not src/:
 *   PlatformIO's LDF auto-discovers anything under lib/<Name>/ as a private
 *   library, so any .cpp in src/ that does `#include "calibration_storage.h"`
 *   pulls this module into the link without touching platformio.ini. Keeping
 *   the backend (EEPROM) inside the lib makes the module self-contained —
 *   matches the "vendor as a self-contained library" guidance.
 *
 * Why a tighter API than auto_orientation:
 *   AO carries history (BNO calibration profile blobs up to 506 B). The FC
 *   only needs to persist 6 floats of MPU6050 offsets (24 B), so the public
 *   surface here is the minimum we need:
 *
 *     bool cs_begin();                               // call once in setup()
 *     bool cs_save(const uint8_t* blob, uint16_t len);
 *     bool cs_load(uint8_t* out, size_t out_cap, uint16_t* out_len);
 *     bool cs_has_valid();
 *
 *   The on-disk layout (marker 0xCA, version 0x02, uint16_t length LE, CRC-8-
 *   CCITT, reserved, payload) is byte-identical to AO so a future cross-
 *   project read is possible.
 *
 * Backend per board:
 *   AVR / Teensy : <EEPROM.h> — real on-chip / flash-emulated EEPROM. Writes
 *                  commit immediately; cs_begin() is a no-op stub.
 *   ESP32        : <EEPROM.h> wraps NVS but REQUIRES EEPROM.begin(size) before
 *                  any read/write, and EEPROM.commit() after each batch or the
 *                  writes are silently dropped on reset. cs_begin()/cs_save()
 *                  handle both. (Known issue KI-1 from the AO HAL notes.)
 *
 * Failure modes that return false:
 *   - cs_begin() failed or not called (ESP32 only)
 *   - Stored marker is not 0xCA  (no calibration / erased EEPROM)
 *   - Stored version != 0x02     (legacy v1 blob — operator must re-cal)
 *   - Stored length is 0 or > CAL_DATA_MAX_SIZE
 *   - Stored length > caller's out_cap (would overflow caller's buffer)
 *   - CRC-8-CCITT mismatch
 */

#ifndef FC_CALIBRATION_STORAGE_H
#define FC_CALIBRATION_STORAGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// EEPROM region layout (matches auto_orientation v2 format)
#define CAL_EEPROM_BASE            0x00
#define CAL_EEPROM_SIZE            512
#define CAL_HEADER_SIZE            6
#define CAL_DATA_MAX_SIZE          506    // 512 - 6 byte header

#define CAL_EEPROM_MARKER_OFFSET   0
#define CAL_EEPROM_VERSION_OFFSET  1
#define CAL_EEPROM_LENGTH_LO_OFFSET 2
#define CAL_EEPROM_LENGTH_HI_OFFSET 3
#define CAL_EEPROM_CRC_OFFSET      4
#define CAL_EEPROM_RESERVED_OFFSET 5
#define CAL_EEPROM_PAYLOAD_OFFSET  6

#define CAL_MARKER_VALID           0xCA
#define CAL_MARKER_EMPTY           0xFF

// Format version. Matches AO's v2 (post-2026-05-20 P1 security pass).
#define CAL_FORMAT_VERSION         0x02

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the EEPROM back-end. Call once in setup() before any
 *        cs_save / cs_load / cs_has_valid.
 *
 * AVR / Teensy: no-op (returns true). EEPROM is always ready.
 * ESP32:        calls EEPROM.begin(CAL_EEPROM_SIZE). Returns false if the
 *               NVS partition could not be opened (very rare).
 */
bool cs_begin(void);

/**
 * @brief Write a calibration blob to EEPROM with header + CRC.
 *
 * @param blob  pointer to payload bytes (non-NULL)
 * @param len   payload length in bytes (1..CAL_DATA_MAX_SIZE)
 * @return true on success; false on argument error, oversize, or write fail.
 *
 * On ESP32 the bytes are buffered and committed atomically before return.
 * On AVR/Teensy each byte is written immediately (~3.3ms/byte on AVR).
 */
bool cs_save(const uint8_t* blob, uint16_t len);

/**
 * @brief Read and verify the calibration blob.
 *
 * @param out      destination buffer (non-NULL)
 * @param out_cap  capacity of `out` in bytes — the stored payload is rejected
 *                 if it would not fit (prevents stack buffer overflow)
 * @param out_len  on success, receives payload length
 * @return true if a valid blob was found and copied to `out`; false otherwise
 *         (no marker, version mismatch, CRC mismatch, oversize, undersize).
 */
bool cs_load(uint8_t* out, size_t out_cap, uint16_t* out_len);

/**
 * @brief Cheap "is there a valid marker?" check (no CRC, no payload load).
 *
 * Useful to skip an explicit cs_load() when the caller only wants to know
 * whether to print "no cal stored" vs. attempt a restore.
 */
bool cs_has_valid(void);

#ifdef __cplusplus
}
#endif

#endif // FC_CALIBRATION_STORAGE_H
