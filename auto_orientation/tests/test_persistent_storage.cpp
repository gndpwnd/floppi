/**
 * Unit Tests — persistent_storage HAL (native build)
 *
 * Validates the byte-level contract of the ps:: API against the native
 * heap-backed back-end (`persistent_storage_native.cpp`). Embedded back-ends
 * (AVR/ESP32/Teensy) reuse this same contract and are exercised on hardware
 * via the existing calibration round-trip path.
 *
 * Coverage:
 *   - Round-trip: bytes written are read back identically.
 *   - Boundary: writes at offset 0 and at capacity-1 succeed; writes that
 *     overrun capacity fail (return false).
 *   - Clear:    cleared region reads back as 0xFF.
 *   - Capacity: returns nonzero after begin().
 *   - Argument validation: NULL buffers and zero lengths are rejected.
 *
 * Framework: Unity (PlatformIO `test_framework = unity`).
 * Build env: native (see proposed [env:native_test] in platformio.ini).
 */

#include <unity.h>
#include <stdint.h>
#include <string.h>

#include "../src/storage/persistent_storage.h"

// Capacity used for all tests. 256 B is small enough to exercise boundaries
// fast but large enough to test multi-byte spans.
static const uint16_t TEST_CAPACITY = 256;

// ============================================================================
// Unity fixtures
// ============================================================================

void setUp(void) {
  // Fresh storage for every test — the native back-end's begin() reallocates
  // and re-fills the buffer with 0xFF, giving each test a clean slate.
  TEST_ASSERT_TRUE(ps::begin(TEST_CAPACITY));
}

void tearDown(void) {
  // No explicit teardown — the next setUp() reinitialises the back-end.
}

// ============================================================================
// Test 1: capacity() returns the requested size
// ============================================================================
void test_capacity_returns_nonzero(void) {
  TEST_ASSERT_EQUAL_UINT16(TEST_CAPACITY, ps::capacity());
}

// ============================================================================
// Test 2: fresh storage reads as 0xFF (matches erased-EEPROM semantics)
// ============================================================================
void test_fresh_storage_is_all_0xFF(void) {
  uint8_t buf[16];
  memset(buf, 0x00, sizeof(buf));

  TEST_ASSERT_TRUE(ps::read(0, buf, sizeof(buf)));
  for (uint16_t i = 0; i < sizeof(buf); i++) {
    TEST_ASSERT_EQUAL_HEX8(0xFF, buf[i]);
  }
}

// ============================================================================
// Test 3: round-trip — write bytes, read same bytes back
// ============================================================================
void test_round_trip_single_byte(void) {
  uint8_t write_val = 0xA5;
  uint8_t read_val  = 0x00;

  TEST_ASSERT_TRUE(ps::write(10, &write_val, 1));
  TEST_ASSERT_TRUE(ps::commit());

  TEST_ASSERT_TRUE(ps::read(10, &read_val, 1));
  TEST_ASSERT_EQUAL_HEX8(0xA5, read_val);
}

void test_round_trip_multi_byte(void) {
  uint8_t pattern[8] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE };
  uint8_t readback[8];
  memset(readback, 0, sizeof(readback));

  TEST_ASSERT_TRUE(ps::write(32, pattern, sizeof(pattern)));
  TEST_ASSERT_TRUE(ps::commit());

  TEST_ASSERT_TRUE(ps::read(32, readback, sizeof(readback)));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(pattern, readback, sizeof(pattern));
}

// ============================================================================
// Test 4: boundary — write at offset 0 succeeds
// ============================================================================
void test_write_at_offset_zero(void) {
  uint8_t val = 0x5A;
  TEST_ASSERT_TRUE(ps::write(0, &val, 1));
  TEST_ASSERT_TRUE(ps::commit());

  uint8_t readback = 0;
  TEST_ASSERT_TRUE(ps::read(0, &readback, 1));
  TEST_ASSERT_EQUAL_HEX8(0x5A, readback);
}

// ============================================================================
// Test 5: boundary — write at the last addressable byte succeeds
// ============================================================================
void test_write_at_capacity_minus_one(void) {
  uint16_t last_offset = (uint16_t)(TEST_CAPACITY - 1);
  uint8_t  val         = 0x33;

  TEST_ASSERT_TRUE(ps::write(last_offset, &val, 1));
  TEST_ASSERT_TRUE(ps::commit());

  uint8_t readback = 0;
  TEST_ASSERT_TRUE(ps::read(last_offset, &readback, 1));
  TEST_ASSERT_EQUAL_HEX8(0x33, readback);
}

// ============================================================================
// Test 6: boundary — write that crosses the capacity edge must fail
// ============================================================================
void test_write_across_boundary_fails(void) {
  // Offset just inside capacity, length pushes us past the end.
  uint16_t offset = (uint16_t)(TEST_CAPACITY - 4);
  uint8_t  buf[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };

  // 8 bytes from offset (cap-4) → ends at cap+4 → must fail.
  TEST_ASSERT_FALSE(ps::write(offset, buf, sizeof(buf)));
}

// ============================================================================
// Test 7: boundary — read that crosses the capacity edge must fail
// ============================================================================
void test_read_across_boundary_fails(void) {
  uint16_t offset = (uint16_t)(TEST_CAPACITY - 4);
  uint8_t  buf[8];
  TEST_ASSERT_FALSE(ps::read(offset, buf, sizeof(buf)));
}

// ============================================================================
// Test 8: boundary — write starting at exactly capacity must fail
// ============================================================================
void test_write_at_capacity_fails(void) {
  uint8_t val = 0xAB;
  TEST_ASSERT_FALSE(ps::write(TEST_CAPACITY, &val, 1));
}

// ============================================================================
// Test 9: argument validation — NULL buffer / zero length rejected
// ============================================================================
void test_null_buffer_rejected(void) {
  TEST_ASSERT_FALSE(ps::write(0, NULL, 4));
  TEST_ASSERT_FALSE(ps::read(0, NULL, 4));
}

void test_zero_length_rejected(void) {
  uint8_t buf[1] = { 0 };
  TEST_ASSERT_FALSE(ps::write(0, buf, 0));
  TEST_ASSERT_FALSE(ps::read(0, buf, 0));
}

// ============================================================================
// Test 10: clear() — cleared region reads back as 0xFF
// ============================================================================
void test_clear_region_reads_0xFF(void) {
  // First, fill a region with non-0xFF data.
  uint8_t fill[16];
  for (uint16_t i = 0; i < sizeof(fill); i++) {
    fill[i] = (uint8_t)(i + 1);  // 0x01..0x10
  }
  TEST_ASSERT_TRUE(ps::write(64, fill, sizeof(fill)));
  TEST_ASSERT_TRUE(ps::commit());

  // Confirm it landed.
  uint8_t readback[16];
  TEST_ASSERT_TRUE(ps::read(64, readback, sizeof(readback)));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(fill, readback, sizeof(fill));

  // Clear it.
  ps::clear(64, sizeof(fill));
  TEST_ASSERT_TRUE(ps::commit());

  // Re-read: must be all 0xFF.
  memset(readback, 0x00, sizeof(readback));
  TEST_ASSERT_TRUE(ps::read(64, readback, sizeof(readback)));
  for (uint16_t i = 0; i < sizeof(readback); i++) {
    TEST_ASSERT_EQUAL_HEX8(0xFF, readback[i]);
  }
}

// ============================================================================
// Test 11: clear() leaves untouched bytes alone
// ============================================================================
void test_clear_does_not_affect_neighbors(void) {
  // Write a pattern straddling the clear region.
  uint8_t before_pat[4] = { 0x11, 0x22, 0x33, 0x44 };
  uint8_t middle_pat[4] = { 0x55, 0x66, 0x77, 0x88 };
  uint8_t after_pat[4]  = { 0x99, 0xAA, 0xBB, 0xCC };

  TEST_ASSERT_TRUE(ps::write(100, before_pat, 4));
  TEST_ASSERT_TRUE(ps::write(104, middle_pat, 4));
  TEST_ASSERT_TRUE(ps::write(108, after_pat,  4));
  TEST_ASSERT_TRUE(ps::commit());

  // Clear only the middle 4 bytes.
  ps::clear(104, 4);
  TEST_ASSERT_TRUE(ps::commit());

  uint8_t readback[12];
  TEST_ASSERT_TRUE(ps::read(100, readback, sizeof(readback)));

  // Before region untouched
  TEST_ASSERT_EQUAL_HEX8_ARRAY(before_pat, readback, 4);
  // Middle region cleared
  for (uint16_t i = 4; i < 8; i++) {
    TEST_ASSERT_EQUAL_HEX8(0xFF, readback[i]);
  }
  // After region untouched
  TEST_ASSERT_EQUAL_HEX8_ARRAY(after_pat, readback + 8, 4);
}

// ============================================================================
// Test 12: commit() returns true even when nothing is pending
// ============================================================================
void test_commit_idempotent(void) {
  TEST_ASSERT_TRUE(ps::commit());
  TEST_ASSERT_TRUE(ps::commit());  // second call still succeeds
}

// ============================================================================
// Test 13: overwrite — second write replaces first
// ============================================================================
void test_overwrite_replaces_prior_value(void) {
  uint8_t first[4]  = { 0xAA, 0xBB, 0xCC, 0xDD };
  uint8_t second[4] = { 0x11, 0x22, 0x33, 0x44 };

  TEST_ASSERT_TRUE(ps::write(50, first,  4));
  TEST_ASSERT_TRUE(ps::commit());
  TEST_ASSERT_TRUE(ps::write(50, second, 4));
  TEST_ASSERT_TRUE(ps::commit());

  uint8_t readback[4];
  TEST_ASSERT_TRUE(ps::read(50, readback, 4));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(second, readback, 4);
}

// ============================================================================
// Test runner
// ============================================================================

int main(int /*argc*/, char** /*argv*/) {
  UNITY_BEGIN();

  RUN_TEST(test_capacity_returns_nonzero);
  RUN_TEST(test_fresh_storage_is_all_0xFF);

  RUN_TEST(test_round_trip_single_byte);
  RUN_TEST(test_round_trip_multi_byte);

  RUN_TEST(test_write_at_offset_zero);
  RUN_TEST(test_write_at_capacity_minus_one);
  RUN_TEST(test_write_across_boundary_fails);
  RUN_TEST(test_read_across_boundary_fails);
  RUN_TEST(test_write_at_capacity_fails);

  RUN_TEST(test_null_buffer_rejected);
  RUN_TEST(test_zero_length_rejected);

  RUN_TEST(test_clear_region_reads_0xFF);
  RUN_TEST(test_clear_does_not_affect_neighbors);

  RUN_TEST(test_commit_idempotent);
  RUN_TEST(test_overwrite_replaces_prior_value);

  return UNITY_END();
}
