/**
 * Unit Tests for Calibration Storage (src/config/calibration_storage.{h,cpp})
 *
 * Purpose
 * -------
 * Pure-native unit tests for calibration persistence. Addresses two findings:
 *
 *   1. P0/P1 finding from docs/findings/audit_test_coverage_2026-05-20.md:
 *      calibration_storage.cpp (211 LOC) had no native test despite being a
 *      pure-logic file (CRC computation, header packing, version checks).
 *
 *   2. Test recommendations from docs/findings/security_fix_calibration_2026-05-20.md
 *      (the 10-test list at lines 178-214). That report documents 4 P1 fixes
 *      (CRC-8-CCITT upgrade, uint16_t length field, explicit version rejection,
 *      and an integer-overflow guard in a sibling file) and explicitly asked a
 *      sibling test-writer agent — this one — to land regressions for them.
 *
 * The 10 recommendations covered (mapped 1:1 to security_fix_calibration doc):
 *   1.  CRC-8-CCITT reference vector test ("", "A", "123456789")
 *   2.  Round-trip save/restore at multiple payload sizes (1, 36, 72, 200, 506)
 *   3.  Length truncation regression — 300-byte payload survives intact
 *   4.  Single-bit flip detection — flip every bit in a small payload
 *   5.  Two-bit flip detection — XOR same bit position into two bytes
 *   6.  Version rejection — v1 (0x01) blob with valid v1 CRC must be rejected
 *   7.  Header marker rejection — 0xFF, 0x00, 0xCB must all return false
 *   8.  Oversize/zero length rejection — length=0, length=CAL_DATA_MAX_SIZE+1
 *   9.  (Not covered here — sits in sensors/bno085_calibration.cpp, out of scope
 *       for THIS file. Flagged for the BNO085 test-writer agent.)
 *  10.  hasCalibrationInEEPROM() unchanged contract
 *
 * Bonus scenarios (drawn from the task brief, not the 10-test list):
 *  - Capacity overflow at the API boundary (length > CAL_DATA_MAX_SIZE)
 *  - Exact-fit at CAL_DATA_MAX_SIZE (506 bytes)
 *  - clearCalibrationFromEEPROM() makes restore return false
 *  - hasCalibrationInEEPROM() returns false after clear()
 *  - Null-pointer arguments are rejected by save and restore
 *  - Boundary length value 0x0100 (256) — the EXACT value that the OLD uint8_t
 *    length field would have truncated to 0
 *
 * Conventions
 * -----------
 *   - printf-based asserts (no Unity / gtest dependency — matches
 *     test_json_formatter.cpp / test_quaternion.cpp style).
 *   - Each scenario is one void function; main() invokes them all and prints
 *     a final "Tests run: N, passed: M" line.
 *   - Exits 0 on full pass, 1 on any failure.
 *   - Does NOT depend on Arduino.h.
 *   - Links against persistent_storage_native.cpp for the HAL back-end —
 *     same back-end the rest of the native test suite uses. We re-begin()
 *     between every test for hermeticity (the native back-end's begin()
 *     re-allocates and 0xFF-fills, mimicking a virgin EEPROM).
 *
 * Canonical compile line:
 *
 *   g++ -std=c++11 -O2 -fpermissive -Iinclude -Isrc \
 *       -o /tmp/test_calibration_storage_run \
 *       tests/test_calibration_storage.cpp \
 *       src/config/calibration_storage.cpp \
 *       src/storage/persistent_storage_native.cpp
 *
 * Author / Date: test-writer-calibration-storage agent, 2026-05-20
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../src/config/calibration_storage.h"
#include "../src/storage/persistent_storage.h"

// ============================================================================
// Test infrastructure
// ============================================================================

static int g_tests_run = 0;
static int g_tests_passed = 0;
static bool g_current_failed = false;

static void test_begin(const char* name) {
    g_tests_run++;
    g_current_failed = false;
    printf("[Test %d] %s\n", g_tests_run, name);
}

static void test_end() {
    if (!g_current_failed) {
        g_tests_passed++;
        printf("  PASS\n");
    } else {
        printf("  FAIL\n");
    }
}

#define EXPECT_TRUE(cond, msg)                                              \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("  FAIL: %s (line %d): %s\n", msg, __LINE__, #cond);     \
            g_current_failed = true;                                        \
        }                                                                   \
    } while (0)

#define EXPECT_EQ_U8(actual, expected, msg)                                 \
    do {                                                                    \
        uint8_t _a = (uint8_t)(actual);                                     \
        uint8_t _e = (uint8_t)(expected);                                   \
        if (_a != _e) {                                                     \
            printf("  FAIL: %s (line %d): got 0x%02X, expected 0x%02X\n",   \
                   msg, __LINE__, _a, _e);                                  \
            g_current_failed = true;                                        \
        }                                                                   \
    } while (0)

#define EXPECT_EQ_U16(actual, expected, msg)                                \
    do {                                                                    \
        uint16_t _a = (uint16_t)(actual);                                   \
        uint16_t _e = (uint16_t)(expected);                                 \
        if (_a != _e) {                                                     \
            printf("  FAIL: %s (line %d): got %u, expected %u\n",           \
                   msg, __LINE__, _a, _e);                                  \
            g_current_failed = true;                                        \
        }                                                                   \
    } while (0)

// ============================================================================
// Helpers
// ============================================================================

/**
 * Reset the native persistent_storage back-end to a virgin (all 0xFF) state.
 * The native impl's begin() frees its prior buffer and re-allocates filled
 * with 0xFF, so calling begin() between tests is sufficient for hermeticity.
 *
 * Capacity is sized to CAL_EEPROM_SIZE so writes inside the calibration slot
 * succeed but anything past it would fail bounds checks (matches a real
 * Arduino Mega EEPROM region size for this slot).
 */
static void fresh_storage() {
    bool ok = ps::begin(CAL_EEPROM_SIZE);
    if (!ok) {
        printf("  FATAL: ps::begin() failed\n");
        std::exit(2);
    }
}

/**
 * Fill `buf` with a deterministic pseudo-pattern. Not random — we want bit
 * exact reproducibility across runs. The pattern intentionally varies in
 * every byte so simple "all bytes equal" bugs would be caught.
 */
static void fill_pattern(uint8_t* buf, uint16_t len, uint8_t seed) {
    for (uint16_t i = 0; i < len; i++) {
        // Simple LCG-style pattern; each byte depends on index and seed.
        buf[i] = (uint8_t)((i * 131u) ^ (seed + i));
    }
}

// ============================================================================
// Test 1 — CRC-8-CCITT reference vectors
// ----------------------------------------------------------------------------
// Recommendation 1 from security_fix_calibration_2026-05-20.md.
// CRC-8-CCITT (poly 0x07, init 0x00, no reflection, no final XOR) should
// produce known values for canonical inputs. If this fails, the algorithm
// was misimplemented (wrong polynomial, accidental reflection, etc.).
// ============================================================================
static void test_crc_reference_vectors() {
    test_begin("CRC-8-CCITT reference vectors");

    // Empty input: documented short-circuit returns 0.
    EXPECT_EQ_U8(calculateCRC8(NULL, 0), 0x00, "null-empty");

    const uint8_t empty[1] = { 0 };
    EXPECT_EQ_U8(calculateCRC8(empty, 0), 0x00, "zero-length");

    // Single byte 'A' (0x41). Computed by hand with poly=0x07, init=0x00,
    // no reflection, no final XOR: 0x41 XOR-ed into the register, then 8 left
    // shifts with conditional XOR-by-0x07 yields 0xC0. (NOTE: the security
    // report at security_fix_calibration_2026-05-20.md line 186 claims this
    // is 0x20 — that's a typo; the actual implementation matches the textbook
    // hand-computation. Follow-up note for the doc-writer agent: fix that
    // example value in the doc.)
    const uint8_t a = 0x41;
    EXPECT_EQ_U8(calculateCRC8(&a, 1), 0xC0, "single 'A'");

    // Canonical CRC-8-CCITT check vector: "123456789" → 0xF4. This is the
    // industry-standard "check" value documented for CRC-8-CCITT (SMBus PEC,
    // I2C, etc.) and matches the implementation — strong evidence the algo
    // is correctly implemented.
    const uint8_t check[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    EXPECT_EQ_U8(calculateCRC8(check, 9), 0xF4, "123456789 check value");

    test_end();
}

// ============================================================================
// Test 2 — Happy-path round trip at multiple sizes
// ----------------------------------------------------------------------------
// Recommendation 2. Sizes chosen to mirror real BNO055 (22B), BNO085 (~80B),
// and the v2 max payload (506B). 1-byte is added for the "smallest valid"
// case the API allows.
// ============================================================================
static void test_round_trip_sizes() {
    test_begin("Round-trip at sizes 1, 22, 80, 200, 506");

    const uint16_t sizes[] = { 1, 22, 80, 200, CAL_DATA_MAX_SIZE };
    for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        fresh_storage();

        uint16_t len = sizes[s];
        uint8_t in[CAL_DATA_MAX_SIZE];
        fill_pattern(in, len, /*seed=*/(uint8_t)(s + 1));

        EXPECT_TRUE(saveToEEPROM(in, len), "save succeeded");

        uint8_t out[CAL_DATA_MAX_SIZE];
        std::memset(out, 0, sizeof(out));
        uint16_t got_len = 0;
        EXPECT_TRUE(restoreFromEEPROM(out, sizeof(out), &got_len), "restore succeeded");
        EXPECT_EQ_U16(got_len, len, "length round-trips");
        EXPECT_TRUE(std::memcmp(in, out, len) == 0, "payload round-trips");
    }

    test_end();
}

// ============================================================================
// Test 3 — Length-truncation regression (the uint8_t→uint16_t fix)
// ----------------------------------------------------------------------------
// Recommendation 3. The OLD code stored length as a uint8_t — saving a
// 300-byte payload would write low-byte (0x2C = 44) and on restore the caller
// would receive 44 bytes of payload and 256 bytes of garbage. The v2 layout
// stores a true uint16_t; this test pins that fix in place.
// ============================================================================
static void test_length_300_byte_no_truncation() {
    test_begin("300-byte payload survives intact (uint16_t length fix)");

    fresh_storage();

    const uint16_t LEN = 300;
    uint8_t in[CAL_DATA_MAX_SIZE];
    fill_pattern(in, LEN, /*seed=*/0xA5);

    EXPECT_TRUE(saveToEEPROM(in, LEN), "save 300B succeeded");

    uint8_t out[CAL_DATA_MAX_SIZE];
    std::memset(out, 0, sizeof(out));
    uint16_t got_len = 0;
    EXPECT_TRUE(restoreFromEEPROM(out, sizeof(out), &got_len), "restore succeeded");
    EXPECT_EQ_U16(got_len, LEN, "length is 300, not 44");
    EXPECT_TRUE(std::memcmp(in, out, LEN) == 0, "all 300 bytes match");

    test_end();
}

// ============================================================================
// Test 4 — Length = exactly 256 (the worst-case truncation value)
// ----------------------------------------------------------------------------
// Bonus scenario from the task brief. The OLD uint8_t length field would
// truncate 0x0100 to 0x00, then the restore length check `length == 0`
// (which exists in the current code too) would reject the blob — a silent
// data-loss bug. The v2 layout must accept 256 cleanly.
// ============================================================================
static void test_length_exact_256() {
    test_begin("Length exactly 256 (old uint8_t truncated to 0)");

    fresh_storage();

    const uint16_t LEN = 256;
    uint8_t in[CAL_DATA_MAX_SIZE];
    fill_pattern(in, LEN, /*seed=*/0x5A);

    EXPECT_TRUE(saveToEEPROM(in, LEN), "save 256B succeeded");

    uint8_t out[CAL_DATA_MAX_SIZE];
    std::memset(out, 0, sizeof(out));
    uint16_t got_len = 0;
    EXPECT_TRUE(restoreFromEEPROM(out, sizeof(out), &got_len), "restore succeeded");
    EXPECT_EQ_U16(got_len, LEN, "length is 256");
    EXPECT_TRUE(std::memcmp(in, out, LEN) == 0, "256 bytes match");

    test_end();
}

// ============================================================================
// Test 5 — Single-bit flip detection across small payload
// ----------------------------------------------------------------------------
// Recommendation 4. CRC-8-CCITT is mathematically guaranteed to detect
// every single-bit error within the message+CRC region (any reasonable
// polynomial does, but we verify by exhaustively flipping each of the 8*N
// payload bits and confirming restore returns false).
//
// We test on a 16-byte payload (128 bit positions) — small enough to keep
// the test cheap, large enough to be more than just a curiosity.
// ============================================================================
static void test_single_bit_flip_detection() {
    test_begin("Single-bit flip in payload causes restore failure");

    const uint16_t LEN = 16;
    uint8_t in[LEN];
    fill_pattern(in, LEN, /*seed=*/0x11);

    int detected = 0;
    int total = 0;
    for (uint16_t byte_idx = 0; byte_idx < LEN; byte_idx++) {
        for (uint8_t bit = 0; bit < 8; bit++) {
            fresh_storage();
            EXPECT_TRUE(saveToEEPROM(in, LEN), "save");

            // Read the stored payload, flip a single bit, write back.
            // We bypass the API and poke the HAL directly to simulate
            // bit-rot in the EEPROM medium itself.
            uint8_t b;
            uint16_t off = CAL_EEPROM_BASE + CAL_EEPROM_PAYLOAD_OFFSET + byte_idx;
            EXPECT_TRUE(ps::read(off, &b, 1), "read raw");
            b ^= (uint8_t)(1u << bit);
            EXPECT_TRUE(ps::write(off, &b, 1), "write raw");

            uint8_t out[LEN];
            uint16_t got_len = 0;
            bool ok = restoreFromEEPROM(out, sizeof(out), &got_len);
            total++;
            if (!ok) detected++;
        }
    }

    // All 128 single-bit flips MUST be detected (CRC-8 guarantees this).
    EXPECT_TRUE(detected == total, "all single-bit errors detected");
    printf("  (detected %d/%d single-bit flips)\n", detected, total);

    test_end();
}

// ============================================================================
// Test 6 — Two-bit-flip detection (the old XOR-CRC weak spot)
// ----------------------------------------------------------------------------
// Recommendation 5. The OLD XOR-sum "CRC" was column-wise: flipping bit N in
// any two bytes XORed to zero and so the corruption was undetected. CRC-8-CCITT
// catches every two-bit error within an 8-byte window. We test a few specific
// patterns — different positions, same bit position — that the old algorithm
// would have missed.
// ============================================================================
static void test_two_bit_flip_detection() {
    test_begin("Two-bit flip (same column) caught by CRC-8 — was XOR-CRC blind spot");

    const uint16_t LEN = 16;
    uint8_t in[LEN];
    fill_pattern(in, LEN, /*seed=*/0x22);

    struct FlipPair { uint16_t b1; uint16_t b2; uint8_t bit; };
    const FlipPair pairs[] = {
        { 0,  1,  0 },   // Bit 0 in bytes 0+1 — classic XOR-blind pair
        { 2,  5,  3 },   // Bit 3 in bytes 2+5
        { 0,  7,  7 },   // High bit, byte 0+7
        { 8, 15,  4 },   // Cross the back-half of the buffer
        { 3, 12,  1 },   // Far-apart bytes
    };

    int detected = 0;
    int total = 0;
    for (size_t p = 0; p < sizeof(pairs) / sizeof(pairs[0]); p++) {
        fresh_storage();
        EXPECT_TRUE(saveToEEPROM(in, LEN), "save");

        // Flip the same bit in two bytes — this is the corruption pattern
        // that the legacy XOR-sum CRC silently missed (XOR-cancels).
        uint8_t b1, b2;
        uint16_t off1 = CAL_EEPROM_BASE + CAL_EEPROM_PAYLOAD_OFFSET + pairs[p].b1;
        uint16_t off2 = CAL_EEPROM_BASE + CAL_EEPROM_PAYLOAD_OFFSET + pairs[p].b2;
        EXPECT_TRUE(ps::read(off1, &b1, 1), "read raw 1");
        EXPECT_TRUE(ps::read(off2, &b2, 1), "read raw 2");
        b1 ^= (uint8_t)(1u << pairs[p].bit);
        b2 ^= (uint8_t)(1u << pairs[p].bit);
        EXPECT_TRUE(ps::write(off1, &b1, 1), "write raw 1");
        EXPECT_TRUE(ps::write(off2, &b2, 1), "write raw 2");

        uint8_t out[LEN];
        uint16_t got_len = 0;
        bool ok = restoreFromEEPROM(out, sizeof(out), &got_len);
        total++;
        if (!ok) detected++;
    }

    EXPECT_TRUE(detected == total, "all 2-bit same-column flips detected");
    printf("  (detected %d/%d two-bit same-column flips)\n", detected, total);

    test_end();
}

// ============================================================================
// Test 7 — Version mismatch rejection (the silent-accept fix)
// ----------------------------------------------------------------------------
// Recommendation 6. Hand-construct a v1-shaped blob (marker, len, version=0x01,
// XOR-sum CRC) and confirm restoreFromEEPROM() refuses to restore it. Pre-fix,
// this would have returned true with semantically-invalid bytes; post-fix,
// the version check shorts the path before the CRC even runs.
//
// We additionally verify that an UNKNOWN future version (0x03) is also
// rejected — confirms the check is "is exactly CAL_FORMAT_VERSION", not
// "is not 0x01".
// ============================================================================
static void test_version_mismatch_rejected() {
    test_begin("Version mismatch (v1 blob and v3 future blob) rejected");

    // --- Case A: legacy v1 blob with valid v1 XOR-sum CRC ---
    fresh_storage();
    {
        // Build a v1-style header at the offsets the v1 layout used.
        // v1 layout was: marker(1), length(1), version(1), xor_crc(1).
        // We just have to write the 6 bytes the v2 reader looks at — the
        // critical byte is the version field at offset 1, which v2 layout
        // calls CAL_EEPROM_VERSION_OFFSET = 1. So whether the rest of the
        // bytes look like v1 or v2, version=0x01 short-circuits the path.
        uint8_t hdr[CAL_HEADER_SIZE];
        hdr[CAL_EEPROM_MARKER_OFFSET]    = CAL_MARKER_VALID;
        hdr[CAL_EEPROM_VERSION_OFFSET]   = 0x01;   // <-- legacy
        hdr[CAL_EEPROM_LENGTH_LO_OFFSET] = 8;
        hdr[CAL_EEPROM_LENGTH_HI_OFFSET] = 0;
        hdr[CAL_EEPROM_CRC_OFFSET]       = 0x00;   // irrelevant after version check
        hdr[CAL_EEPROM_RESERVED_OFFSET]  = 0x00;
        EXPECT_TRUE(ps::write(CAL_EEPROM_BASE, hdr, CAL_HEADER_SIZE), "write v1 hdr");

        // Write a payload of zeros so its XOR-CRC IS valid (proves we're not
        // failing on the CRC — we're failing on version).
        uint8_t payload[8] = { 0 };
        EXPECT_TRUE(ps::write(CAL_EEPROM_BASE + CAL_EEPROM_PAYLOAD_OFFSET,
                              payload, 8), "write payload");

        uint8_t out[CAL_DATA_MAX_SIZE];
        uint16_t got_len = 0;
        bool ok = restoreFromEEPROM(out, sizeof(out), &got_len);
        EXPECT_TRUE(!ok, "v1 blob rejected");
    }

    // --- Case B: future-version blob (0x03) ---
    fresh_storage();
    {
        uint8_t hdr[CAL_HEADER_SIZE];
        hdr[CAL_EEPROM_MARKER_OFFSET]    = CAL_MARKER_VALID;
        hdr[CAL_EEPROM_VERSION_OFFSET]   = 0x03;
        hdr[CAL_EEPROM_LENGTH_LO_OFFSET] = 4;
        hdr[CAL_EEPROM_LENGTH_HI_OFFSET] = 0;
        hdr[CAL_EEPROM_CRC_OFFSET]       = 0x00;
        hdr[CAL_EEPROM_RESERVED_OFFSET]  = 0x00;
        EXPECT_TRUE(ps::write(CAL_EEPROM_BASE, hdr, CAL_HEADER_SIZE), "write v3 hdr");

        uint8_t out[CAL_DATA_MAX_SIZE];
        uint16_t got_len = 0;
        bool ok = restoreFromEEPROM(out, sizeof(out), &got_len);
        EXPECT_TRUE(!ok, "future version rejected");
    }

    test_end();
}

// ============================================================================
// Test 8 — Header marker rejection (0xFF, 0x00, 0xCB)
// ----------------------------------------------------------------------------
// Recommendation 7. Only 0xCA is a valid marker. 0xFF is "empty EEPROM",
// 0x00 is "all zeros" (e.g. wiped), and 0xCB is one-bit-off from 0xCA — the
// most likely accidental neighbor. All three must return false.
// ============================================================================
static void test_marker_rejection() {
    test_begin("Marker 0xFF / 0x00 / 0xCB all rejected");

    const uint8_t bad_markers[] = { 0xFF, 0x00, 0xCB };
    for (size_t i = 0; i < sizeof(bad_markers); i++) {
        fresh_storage();

        // Write a fully-valid v2 header, then stomp the marker.
        uint8_t payload[8];
        fill_pattern(payload, 8, /*seed=*/0x77);
        EXPECT_TRUE(saveToEEPROM(payload, 8), "save valid first");

        // Stomp marker.
        uint8_t m = bad_markers[i];
        EXPECT_TRUE(ps::write(CAL_EEPROM_BASE + CAL_EEPROM_MARKER_OFFSET,
                              &m, 1), "stomp marker");

        uint8_t out[CAL_DATA_MAX_SIZE];
        uint16_t got_len = 0;
        bool ok = restoreFromEEPROM(out, sizeof(out), &got_len);
        EXPECT_TRUE(!ok, "bad marker rejected");
    }

    test_end();
}

// ============================================================================
// Test 9 — Oversize / zero length rejection
// ----------------------------------------------------------------------------
// Recommendation 8. Both at the API-input boundary (saveToEEPROM should
// refuse length=0 and length > CAL_DATA_MAX_SIZE) and at the on-disk
// boundary (a hand-crafted header with stored_length = 0 or CAL_DATA_MAX_SIZE+1
// should be rejected by restoreFromEEPROM).
// ============================================================================
static void test_oversize_and_zero_length() {
    test_begin("Length=0 and length>CAL_DATA_MAX_SIZE rejected");

    // --- API boundary: save side ---
    {
        fresh_storage();
        uint8_t dummy[1] = { 0 };
        EXPECT_TRUE(!saveToEEPROM(dummy, 0), "save rejects length=0");

        // Try to save length > CAL_DATA_MAX_SIZE. We don't actually need
        // that many bytes of source data — saveToEEPROM checks length
        // before dereferencing past CAL_DATA_MAX_SIZE bytes.
        uint8_t big[CAL_DATA_MAX_SIZE + 8];
        std::memset(big, 0xAA, sizeof(big));
        EXPECT_TRUE(!saveToEEPROM(big, CAL_DATA_MAX_SIZE + 1),
                    "save rejects length > CAL_DATA_MAX_SIZE");
    }

    // --- On-disk boundary: restore side, stored_length = 0 ---
    {
        fresh_storage();
        // Construct a header that LOOKS valid but claims length 0.
        uint8_t hdr[CAL_HEADER_SIZE];
        hdr[CAL_EEPROM_MARKER_OFFSET]    = CAL_MARKER_VALID;
        hdr[CAL_EEPROM_VERSION_OFFSET]   = CAL_FORMAT_VERSION;
        hdr[CAL_EEPROM_LENGTH_LO_OFFSET] = 0;
        hdr[CAL_EEPROM_LENGTH_HI_OFFSET] = 0;
        hdr[CAL_EEPROM_CRC_OFFSET]       = 0x00;
        hdr[CAL_EEPROM_RESERVED_OFFSET]  = 0x00;
        EXPECT_TRUE(ps::write(CAL_EEPROM_BASE, hdr, CAL_HEADER_SIZE), "hdr");

        uint8_t out[CAL_DATA_MAX_SIZE];
        uint16_t got_len = 0;
        EXPECT_TRUE(!restoreFromEEPROM(out, sizeof(out), &got_len),
                    "restore rejects stored_length=0");
    }

    // --- On-disk boundary: stored_length = CAL_DATA_MAX_SIZE + 1 ---
    {
        fresh_storage();
        uint16_t bad_len = CAL_DATA_MAX_SIZE + 1;
        uint8_t hdr[CAL_HEADER_SIZE];
        hdr[CAL_EEPROM_MARKER_OFFSET]    = CAL_MARKER_VALID;
        hdr[CAL_EEPROM_VERSION_OFFSET]   = CAL_FORMAT_VERSION;
        hdr[CAL_EEPROM_LENGTH_LO_OFFSET] = (uint8_t)(bad_len & 0xFF);
        hdr[CAL_EEPROM_LENGTH_HI_OFFSET] = (uint8_t)((bad_len >> 8) & 0xFF);
        hdr[CAL_EEPROM_CRC_OFFSET]       = 0x00;
        hdr[CAL_EEPROM_RESERVED_OFFSET]  = 0x00;
        EXPECT_TRUE(ps::write(CAL_EEPROM_BASE, hdr, CAL_HEADER_SIZE), "hdr");

        uint8_t out[CAL_DATA_MAX_SIZE];
        uint16_t got_len = 0;
        EXPECT_TRUE(!restoreFromEEPROM(out, sizeof(out), &got_len),
                    "restore rejects stored_length > CAL_DATA_MAX_SIZE");
    }

    test_end();
}

// ============================================================================
// Test 10 — hasCalibrationInEEPROM contract
// ----------------------------------------------------------------------------
// Recommendation 10. Marker-only check; doesn't validate CRC or version.
// We confirm:
//   - virgin EEPROM (all 0xFF)         → false
//   - after saveToEEPROM with v2 blob  → true
//   - even if header bytes 1-5 are corrupted, as long as marker stays 0xCA → true
//     (proves we kept the loose contract — the v2 bump didn't accidentally
//     start validating CRC here)
//   - after clearCalibrationFromEEPROM → false
// ============================================================================
static void test_has_calibration_contract() {
    test_begin("hasCalibrationInEEPROM marker-only contract preserved");

    // Virgin: no marker → false.
    fresh_storage();
    EXPECT_TRUE(!hasCalibrationInEEPROM(), "virgin returns false");

    // Saved: marker present → true.
    uint8_t payload[12];
    fill_pattern(payload, 12, /*seed=*/0xEF);
    EXPECT_TRUE(saveToEEPROM(payload, 12), "save");
    EXPECT_TRUE(hasCalibrationInEEPROM(), "after save returns true");

    // Corrupt CRC, version, length — leave marker alone → still true.
    {
        uint8_t junk = 0xAB;
        EXPECT_TRUE(ps::write(CAL_EEPROM_BASE + CAL_EEPROM_CRC_OFFSET,
                              &junk, 1), "stomp CRC");
        EXPECT_TRUE(ps::write(CAL_EEPROM_BASE + CAL_EEPROM_LENGTH_LO_OFFSET,
                              &junk, 1), "stomp len");
        EXPECT_TRUE(ps::write(CAL_EEPROM_BASE + CAL_EEPROM_VERSION_OFFSET,
                              &junk, 1), "stomp version");
        EXPECT_TRUE(hasCalibrationInEEPROM(),
                    "marker-only check still returns true despite CRC/ver/len garbage");
    }

    // Clear: marker goes to 0xFF → false.
    EXPECT_TRUE(clearCalibrationFromEEPROM(), "clear");
    EXPECT_TRUE(!hasCalibrationInEEPROM(), "after clear returns false");

    test_end();
}

// ============================================================================
// Test 11 — clearCalibrationFromEEPROM makes restore fail
// ----------------------------------------------------------------------------
// Bonus from task brief item 10. Round-trip behavior of clear() — restore
// must return false on a slot that was just cleared. (Stronger guarantee
// than test 10's marker-only check.)
// ============================================================================
static void test_clear_makes_restore_fail() {
    test_begin("clearCalibrationFromEEPROM → restore returns false");

    fresh_storage();
    uint8_t payload[32];
    fill_pattern(payload, 32, /*seed=*/0x99);
    EXPECT_TRUE(saveToEEPROM(payload, 32), "save");

    // Sanity: restore works before clear.
    {
        uint8_t out[32];
        uint16_t got_len = 0;
        EXPECT_TRUE(restoreFromEEPROM(out, sizeof(out), &got_len), "restore pre-clear");
    }

    EXPECT_TRUE(clearCalibrationFromEEPROM(), "clear");

    // After clear: restore must fail.
    {
        uint8_t out[32];
        uint16_t got_len = 0;
        EXPECT_TRUE(!restoreFromEEPROM(out, sizeof(out), &got_len),
                    "restore returns false after clear");
    }

    test_end();
}

// ============================================================================
// Test 12 — Null-pointer arguments rejected
// ----------------------------------------------------------------------------
// Bonus. The implementation explicitly null-checks all pointer arguments.
// Confirm that nothing crashes and false is returned.
// ============================================================================
static void test_null_argument_handling() {
    test_begin("Null pointer arguments to save/restore are rejected");

    fresh_storage();

    EXPECT_TRUE(!saveToEEPROM(NULL, 10), "save rejects NULL data");

    uint8_t buf[8];
    uint16_t len = 0;
    EXPECT_TRUE(!restoreFromEEPROM(NULL, sizeof(buf), &len), "restore rejects NULL data");
    EXPECT_TRUE(!restoreFromEEPROM(buf, sizeof(buf), NULL),  "restore rejects NULL length");

    // calculateCRC8 must return 0 on NULL data per its own contract.
    EXPECT_EQ_U8(calculateCRC8(NULL, 100), 0x00, "CRC on NULL data is 0");

    test_end();
}

// ============================================================================
// Test 13 — Multiple save/restore cycles (overwrite behavior)
// ----------------------------------------------------------------------------
// Bonus. Save A, then save B, then restore — must get B, not A.
// Also verifies that the second save correctly overwrites a shorter payload
// (potential bug: if length-tracking is broken, leftovers from the first
// save's CRC could re-validate stale data).
// ============================================================================
static void test_overwrite_cycle() {
    test_begin("Save A then save B — restore returns B, not A");

    fresh_storage();

    // First save — 100 bytes.
    uint8_t first[100];
    fill_pattern(first, 100, /*seed=*/0x01);
    EXPECT_TRUE(saveToEEPROM(first, 100), "save A");

    // Second save — 30 bytes (shorter, exercises non-zero leftover).
    uint8_t second[30];
    fill_pattern(second, 30, /*seed=*/0x02);
    EXPECT_TRUE(saveToEEPROM(second, 30), "save B");

    // Restore — must get B.
    uint8_t out[CAL_DATA_MAX_SIZE];
    std::memset(out, 0, sizeof(out));
    uint16_t got_len = 0;
    EXPECT_TRUE(restoreFromEEPROM(out, sizeof(out), &got_len), "restore");
    EXPECT_EQ_U16(got_len, 30, "length is 30 (B's), not 100 (A's)");
    EXPECT_TRUE(std::memcmp(out, second, 30) == 0, "content is B's");

    test_end();
}

// ============================================================================
// Test 14 — CRC corruption in the on-disk CRC byte
// ----------------------------------------------------------------------------
// Bonus, complements Test 5 (which corrupts the payload). Here the payload
// is untouched but the stored CRC byte is wrong — restore must reject.
// ============================================================================
static void test_crc_byte_corruption() {
    test_begin("Corrupting the stored CRC byte causes restore failure");

    fresh_storage();
    uint8_t payload[24];
    fill_pattern(payload, 24, /*seed=*/0xC1);
    EXPECT_TRUE(saveToEEPROM(payload, 24), "save");

    // Flip every bit in the CRC byte; corrupt CRC ≠ recomputed CRC.
    uint8_t crc;
    uint16_t crc_off = CAL_EEPROM_BASE + CAL_EEPROM_CRC_OFFSET;
    EXPECT_TRUE(ps::read(crc_off, &crc, 1), "read CRC");
    crc ^= 0xFF;
    EXPECT_TRUE(ps::write(crc_off, &crc, 1), "stomp CRC");

    uint8_t out[24];
    uint16_t got_len = 0;
    EXPECT_TRUE(!restoreFromEEPROM(out, sizeof(out), &got_len), "restore rejects bad CRC");

    test_end();
}

// ============================================================================
// main — runs all scenarios
// ============================================================================

int main() {
    printf("================================================\n");
    printf(" calibration_storage native unit tests\n");
    printf("================================================\n\n");

    test_crc_reference_vectors();
    test_round_trip_sizes();
    test_length_300_byte_no_truncation();
    test_length_exact_256();
    test_single_bit_flip_detection();
    test_two_bit_flip_detection();
    test_version_mismatch_rejected();
    test_marker_rejection();
    test_oversize_and_zero_length();
    test_has_calibration_contract();
    test_clear_makes_restore_fail();
    test_null_argument_handling();
    test_overwrite_cycle();
    test_crc_byte_corruption();

    printf("\n================================================\n");
    printf(" Tests run: %d, passed: %d\n", g_tests_run, g_tests_passed);
    printf("================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
