/**
 * CRC-8-CCITT — shared checksum leaf.
 *
 * One canonical CRC-8-CCITT implementation for the whole project. Both
 * src/config/calibration_storage.cpp (calculateCRC8) and
 * src/applications/balancing_robot_uno/tune_storage.cpp (crc8_ccitt) used to
 * carry byte-identical private copies of this algorithm; two checksums that
 * MUST agree with nothing pinning them together. They now both delegate here.
 *
 * Parameters (fixed — changing any of these changes every stored CRC byte and
 * would break already-deployed EEPROM records, so they are non-negotiable):
 *   polynomial  = 0x07  (x^8 + x^2 + x + 1)
 *   init        = 0x00
 *   reflect_in  = false
 *   reflect_out = false
 *   xor_out     = 0x00
 *
 * Algorithm: standard bitwise polynomial-division CRC. Each input byte is
 * XOR-ed into the CRC register, then the register is shifted left 8 times,
 * XOR-ing the polynomial whenever the high bit was set before the shift.
 * Bitwise (no 256-byte lookup table) to keep flash use low on AVR — ~120 bytes
 * of code vs. 256 bytes of .rodata.
 *
 * Pure leaf: depends only on <stdint.h> — no Arduino, no STL. The function is
 * defined `inline` in this header so it needs no separate translation unit;
 * every TU that includes it (AVR firmware envs and host test compiles alike)
 * gets the one definition with no link-time dependency on a .cpp file.
 *
 * @note Not cryptographically secure; intended for accidental corruption
 *       detection, not adversarial tampering.
 */

#ifndef UTIL_CRC8_H
#define UTIL_CRC8_H

#include <stdint.h>

namespace util {

/**
 * @brief Compute the CRC-8-CCITT checksum of a byte range.
 *
 * @param[in] data  Pointer to the bytes to checksum.
 * @param[in] len   Number of bytes.
 * @return CRC8 value (0x00 - 0xFF). Returns 0x00 for a null pointer or
 *         zero-length input — matching the contract of the two private copies
 *         this function replaces.
 */
inline uint8_t crc8_ccitt(const uint8_t* data, uint16_t len) {
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

}  // namespace util

#endif  // UTIL_CRC8_H
