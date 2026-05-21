/**
 * Guided-tuning EEPROM persistence — Uno balance build.
 *
 * A deliberately tiny module that stores the operator's hand-tuned PID gains
 * in a 19-byte EEPROM block at base 0x200. It is intentionally NOT routed
 * through calibration_storage / the persistent_storage HAL — that path owns
 * 0x000..0x1FF and is overkill for four floats. This block lives in the free
 * region directly above it.
 *
 * EEPROM layout (19 bytes, base TUNE_EEPROM_BASE = 0x200):
 *
 * Offset | Size  | Content
 * -------|-------|------------------------------------------------
 * 0x200  | 1     | marker   (TUNE_MARKER 0xB5 = valid, 0xFF = empty)
 * 0x201  | 1     | version  (TUNE_VERSION 0x01)
 * 0x202  | 4     | Kp        (float)
 * 0x206  | 4     | Ki        (float)
 * 0x20A  | 4     | Kd        (float)
 * 0x20E  | 4     | pitch_off (float)
 * 0x212  | 1     | crc8      (CRC-8-CCITT over bytes 0x201..0x211)
 * -------|-------|------------------------------------------------
 * Total  | 19    |
 *
 * The CRC covers version + 16 payload bytes (18 bytes), matching the same
 * CRC-8-CCITT algorithm used by calibration_storage (poly 0x07, init 0x00).
 */

#ifndef TUNE_STORAGE_H
#define TUNE_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

// EEPROM region — sits just above the 512-byte calibration blob (0x000..0x1FF).
#define TUNE_EEPROM_BASE 0x200   // Start address of the tune block
#define TUNE_MARKER      0xB5    // Validity marker (0xFF = empty/unwritten)
#define TUNE_VERSION     0x01    // Block format version

/**
 * @brief The four persisted tuning values.
 *
 * pitch_off mirrors the seed offset in phase 1 (not yet guided-tuned); it is
 * still round-tripped so the slot is reserved and a future Stage O can fill it.
 */
struct TuneBlock {
  float kp;
  float ki;
  float kd;
  float pitch_off;
};

namespace tune_storage {

/**
 * @brief Write a tune block to EEPROM at TUNE_EEPROM_BASE.
 *
 * Writes marker + version + four floats + CRC8 (19 bytes). Uses EEPROM.update()
 * so unchanged bytes are not re-burned (EEPROM wear).
 *
 * @param[in] blk  Gains to persist.
 * @return true (always succeeds on AVR; false on non-AVR no-op builds).
 */
bool save_tuning(const TuneBlock& blk);

/**
 * @brief Read and validate the tune block from EEPROM.
 *
 * Validation: marker == TUNE_MARKER, version == TUNE_VERSION, CRC8 match.
 *
 * @param[out] out  Receives the gains on success; untouched on failure.
 * @return true if a valid block was loaded, false if empty/corrupt/wrong-version.
 */
bool load_tuning(TuneBlock& out);

/**
 * @brief Quick check whether a usable tune block is stored.
 *
 * Checks the marker byte AND verifies the CRC, so a true return guarantees
 * load_tuning() will also succeed.
 *
 * @return true if a valid tune block is present.
 */
bool has_tuning();

}  // namespace tune_storage

#endif  // TUNE_STORAGE_H
