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

// IMU calibration blob — variable-length slot. The slot is sized for the
// LARGEST supported IMU blob (BNO085 SH-2 DYNAMIC_CALIBRATION FRS record runs
// ~36-72 B; BNO055 adafruit_bno055_offsets_t is exactly 22 B). A 2-byte length
// header lets a single layout serve both IMUs without re-burning EEPROM. The
// slot starts at 0x220 (13 B above the 19-byte tune block at 0x200..0x212) and
// reserves 77 B total (marker + version + 2-byte len + 72 B payload + CRC),
// ending at 0x26C — comfortably under the Uno's 1 KB EEPROM ceiling.
//
// Version bumped 0x02 -> 0x03 with the introduction of the length header: an
// existing fixed-22 blob saved by the previous format will fail load (wrong
// version) and the operator will be prompted to re-run guided cal. This is the
// safe failure mode — mis-reading a 22-byte payload as if it had a length
// header would corrupt the IMU calibration.
#define TUNE_CAL_BASE      0x220   // Start address of the cal slot
#define TUNE_CAL_MARKER    0xCA    // Validity marker (matches CAL_MARKER_VALID)
#define TUNE_CAL_VERSION   0x03    // Block format version — see note above
#define TUNE_CAL_MAX_BYTES 72      // Max payload size (BNO085 worst case)

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

// ---------------------------------------------------------------------------
// IMU calibration blob persistence (variable length)
// ---------------------------------------------------------------------------
// Length-aware API so the same EEPROM slot can serve both BNO055 (22-byte
// adafruit_bno055_offsets_t) and BNO085 (variable-length SH-2 DYNAMIC_CAL FRS
// record, typically 36-72 bytes). Callers pass the actual length on save and
// an output capacity + length pointer on load.
//
// CRC covers: version || len_lo || len_hi || payload[len].

/**
 * @brief Write a cal blob to EEPROM at TUNE_CAL_BASE.
 *
 * Writes marker + version + 2-byte length + payload + CRC8. Uses EEPROM.update()
 * so unchanged bytes are not re-burned (EEPROM wear).
 *
 * @param[in] blob Cal payload (driver-defined opaque bytes).
 * @param[in] len  Length of @p blob in bytes; must be 1..TUNE_CAL_MAX_BYTES.
 * @return true on success; false if blob is null or len is out of range.
 */
bool save_cal_blob(const uint8_t* blob, uint16_t len);

/**
 * @brief Read and validate the cal blob from EEPROM.
 *
 * Validation: marker == TUNE_CAL_MARKER, version == TUNE_CAL_VERSION, stored
 * length in 1..TUNE_CAL_MAX_BYTES AND <= out_capacity, CRC8 match.
 *
 * @param[out] out          Receives the payload bytes on success; untouched on failure.
 * @param[in]  out_capacity Capacity of @p out in bytes.
 * @param[out] out_len      Receives the actual payload length on success; untouched on failure.
 * @return true if a valid blob was loaded, false if empty / corrupt / wrong
 *         version / length out of range / buffer too small.
 */
bool load_cal_blob(uint8_t* out, uint16_t out_capacity, uint16_t* out_len);

/**
 * @brief Quick check whether a usable cal blob is stored.
 *
 * Checks the marker byte AND verifies the CRC, so a true return guarantees
 * load_cal_blob() will also succeed (given a large-enough buffer).
 *
 * @return true if a valid cal blob is present.
 */
bool has_cal_blob();

// ---------------------------------------------------------------------------
// Photo-backup printer (shared between tuning_session and calibration_session)
// ---------------------------------------------------------------------------

/**
 * @brief Print the "PHOTO-BACKUP" block over Serial so the operator can
 *        photograph the scrollback and paste it into balance_constants.h.
 *
 * The output format is FROZEN — both the guided-tuning session (which prints
 * the PID block on commit) and the guided-cal session (which prints the cal
 * blob on commit) call this; the format must be stable so a single photograph
 * is enough to manually rebuild a chip from scratch.
 *
 * @param[in] pid_valid If true, include the four BALANCE_* / PITCH_OFFSET_DEG
 *                      static-const lines populated from @p pid.
 * @param[in] pid       The PID tune block (ignored when @p pid_valid is false).
 * @param[in] cal_valid If true, include the <imu_tag>_CAL_BLOB[<cal_len>] line
 *                      populated from @p cal.
 * @param[in] cal       The cal blob bytes (ignored when @p cal_valid is false;
 *                      may be nullptr in that case).
 * @param[in] cal_len   Length of @p cal in bytes (ignored when @p cal_valid
 *                      is false).
 * @param[in] imu_tag   Short IMU name used in the array identifier
 *                      (e.g. "BNO055" or "BNO085"). Must not be null when
 *                      @p cal_valid is true.
 *
 * Calling with both flags false still prints the banner so the operator gets
 * an unambiguous "nothing to back up yet" signal instead of silence.
 */
void print_photo_backup(bool pid_valid, const TuneBlock& pid,
                        bool cal_valid, const uint8_t* cal, uint16_t cal_len,
                        const char* imu_tag);

}  // namespace tune_storage

#endif  // TUNE_STORAGE_H
