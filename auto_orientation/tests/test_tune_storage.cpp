/**
 * Unit Tests for tune_storage (src/applications/balancing_robot_uno/tune_storage.{h,cpp})
 *
 * Purpose
 * -------
 * Pure-native unit tests for the guided-tuning EEPROM persistence module that
 * landed 2026-05-20 (workstream UT-A of uno_guided_tuning_design_2026-05-20.md).
 * tune_storage stores the operator's hand-tuned PID gains in a 19-byte block
 * at EEPROM base 0x200 — see tune_storage.h for the layout and design doc §2.4.
 *
 * How the REAL logic is exercised on the host
 * --------------------------------------------
 * tune_storage.cpp is platform-guarded: `#if defined(__AVR__) ||
 * defined(ARDUINO_ARCH_AVR)` selects the real save/load/CRC code; otherwise it
 * compiles to no-op stubs (returning false). To test the REAL code path on a
 * non-AVR host we use brief option (a):
 *
 *   1. Compile tune_storage.cpp with -DARDUINO_ARCH_AVR so the real branch is
 *      active.
 *   2. The real branch does `#include <EEPROM.h>` and uses an `EEPROM` object
 *      with .read/.update/.get/.put. We satisfy that include by adding a stub
 *      <EEPROM.h> on the include path (a generated temp dir — see the compile
 *      line). That stub provides a 1 KB byte-array-backed mock `EEPROM` object,
 *      so save_tuning/load_tuning/has_tuning run their genuine serialization +
 *      CRC-8-CCITT logic against the mock medium.
 *
 * This exercises the real code: CRC computation, marker/version/CRC validation,
 * EEPROM.update() wear-sparing, and float (de)serialization via memcpy.
 *
 * Conventions
 * -----------
 *   - printf-based asserts (no Unity / gtest — matches test_calibration_storage.cpp
 *     / test_json_formatter.cpp).
 *   - One void function per scenario; main() runs them all and prints
 *     "Tests run: N, passed: M".
 *   - Exits 0 on full pass, non-zero on any failure.
 *   - Does NOT depend on Arduino.h.
 *
 * Compile flags
 * -------------
 *   -DARDUINO_ARCH_AVR   activates the real (non-stub) code path in tune_storage.cpp
 *
 * Canonical compile line
 * ----------------------
 * The mock <EEPROM.h> is generated into a temp include dir before compiling:
 *
 *   MOCKDIR=$(mktemp -d)
 *   cat > "$MOCKDIR/EEPROM.h" <<'EOF'
 *     // 1KB byte-array mock — see this file's header for the full stub.
 *   EOF
 *   g++ -std=c++11 -O2 -fpermissive -Iinclude -Isrc -I"$MOCKDIR" \
 *       -DARDUINO_ARCH_AVR \
 *       -o /tmp/test_tune_storage_run \
 *       tests/test_tune_storage.cpp \
 *       src/applications/balancing_robot_uno/tune_storage.cpp
 *   /tmp/test_tune_storage_run
 *
 * The exact <EEPROM.h> stub body that must be written to $MOCKDIR is reproduced
 * verbatim in the MOCK_EEPROM_H comment block below so this test is fully
 * reproducible from this file alone.
 *
 * Author / Date: test-writer-tuning agent, 2026-05-20
 */

/* ===========================================================================
 * MOCK_EEPROM_H — write this verbatim to <some-dir>/EEPROM.h before compiling.
 * ---------------------------------------------------------------------------
 * It models the Arduino AVR EEPROM object with a 1 KB backing array. Bytes
 * start at 0xFF (virgin EEPROM). It implements exactly the subset tune_storage
 * uses: read(addr), update(addr,val), plus get/put templates for completeness.
 * The test reaches into this same array (declared `extern`) to corrupt bytes.
 *
 *   #ifndef MOCK_EEPROM_H
 *   #define MOCK_EEPROM_H
 *   #include <stdint.h>
 *   #include <string.h>
 *   static const int MOCK_EEPROM_SIZE = 1024;
 *   extern unsigned char g_mock_eeprom[MOCK_EEPROM_SIZE];
 *   class MockEEPROM {
 *    public:
 *     uint8_t read(int addr) {
 *       if (addr < 0 || addr >= MOCK_EEPROM_SIZE) return 0xFF;
 *       return g_mock_eeprom[addr];
 *     }
 *     void write(int addr, uint8_t v) {
 *       if (addr < 0 || addr >= MOCK_EEPROM_SIZE) return;
 *       g_mock_eeprom[addr] = v;
 *     }
 *     void update(int addr, uint8_t v) {
 *       if (addr < 0 || addr >= MOCK_EEPROM_SIZE) return;
 *       if (g_mock_eeprom[addr] != v) g_mock_eeprom[addr] = v;  // wear-sparing
 *     }
 *     template <typename T> T& get(int addr, T& t) {
 *       memcpy(&t, &g_mock_eeprom[addr], sizeof(T)); return t;
 *     }
 *     template <typename T> const T& put(int addr, const T& t) {
 *       memcpy(&g_mock_eeprom[addr], &t, sizeof(T)); return t;
 *     }
 *     uint8_t operator[](int addr) { return read(addr); }
 *     int length() { return MOCK_EEPROM_SIZE; }
 *   };
 *   extern MockEEPROM EEPROM;
 *   #endif
 *
 * The test .cpp defines g_mock_eeprom[] and the EEPROM instance (below) so the
 * stub header carries only declarations.
 * ===========================================================================
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../src/applications/balancing_robot_uno/tune_storage.h"

// ----------------------------------------------------------------------------
// Mock EEPROM backing store + instance.
// The stub <EEPROM.h> (on the -I path) declares these `extern`; we define them
// here so the single mock medium is shared between tune_storage.cpp and the
// test. 0xFF-filled = virgin EEPROM.
// ----------------------------------------------------------------------------
static const int MOCK_EEPROM_SIZE = 1024;
unsigned char g_mock_eeprom[MOCK_EEPROM_SIZE];

class MockEEPROM {
 public:
    uint8_t read(int addr) {
        if (addr < 0 || addr >= MOCK_EEPROM_SIZE) return 0xFF;
        return g_mock_eeprom[addr];
    }
    void write(int addr, uint8_t v) {
        if (addr < 0 || addr >= MOCK_EEPROM_SIZE) return;
        g_mock_eeprom[addr] = v;
    }
    void update(int addr, uint8_t v) {
        if (addr < 0 || addr >= MOCK_EEPROM_SIZE) return;
        if (g_mock_eeprom[addr] != v) g_mock_eeprom[addr] = v;  // wear-sparing
    }
    template <typename T> T& get(int addr, T& t) {
        std::memcpy(&t, &g_mock_eeprom[addr], sizeof(T));
        return t;
    }
    template <typename T> const T& put(int addr, const T& t) {
        std::memcpy(&g_mock_eeprom[addr], &t, sizeof(T));
        return t;
    }
    uint8_t operator[](int addr) { return read(addr); }
    int length() { return MOCK_EEPROM_SIZE; }
};

MockEEPROM EEPROM;

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

#define EXPECT_FLOAT_EXACT(actual, expected, msg)                           \
    do {                                                                    \
        float _a = (float)(actual);                                         \
        float _e = (float)(expected);                                       \
        /* Bit-exact: round-trip through EEPROM must not perturb a float. */ \
        if (std::memcmp(&_a, &_e, sizeof(float)) != 0) {                    \
            printf("  FAIL: %s (line %d): got %.9g, expected %.9g\n",       \
                   msg, __LINE__, (double)_a, (double)_e);                  \
            g_current_failed = true;                                        \
        }                                                                   \
    } while (0)

// ============================================================================
// Helpers
// ============================================================================

// Reset the mock EEPROM to a virgin (all-0xFF) state — hermeticity between tests.
static void fresh_eeprom() {
    std::memset(g_mock_eeprom, 0xFF, sizeof(g_mock_eeprom));
}

// EEPROM byte offsets (mirror tune_storage.cpp's private constants).
static const int ADDR_MARKER  = TUNE_EEPROM_BASE + 0;   // 0x200
static const int ADDR_VERSION = TUNE_EEPROM_BASE + 1;   // 0x201
static const int ADDR_PAYLOAD = TUNE_EEPROM_BASE + 2;   // 0x202 (16 bytes)
static const int ADDR_CRC     = TUNE_EEPROM_BASE + 18;  // 0x212

static TuneBlock make_block(float kp, float ki, float kd, float pitch_off) {
    TuneBlock b;
    b.kp = kp;
    b.ki = ki;
    b.kd = kd;
    b.pitch_off = pitch_off;
    return b;
}

// ============================================================================
// Test 1 — Round-trip: save_tuning then load_tuning yields byte-identical fields
// ============================================================================
static void test_round_trip() {
    test_begin("Round-trip: save_tuning -> load_tuning preserves all fields");

    fresh_eeprom();
    TuneBlock in = make_block(48.0f, 6.0f, 22.0f, -8.6f);
    EXPECT_TRUE(tune_storage::save_tuning(in), "save_tuning succeeds");

    TuneBlock out = make_block(0, 0, 0, 0);
    EXPECT_TRUE(tune_storage::load_tuning(out), "load_tuning succeeds");

    EXPECT_FLOAT_EXACT(out.kp, in.kp, "kp round-trips");
    EXPECT_FLOAT_EXACT(out.ki, in.ki, "ki round-trips");
    EXPECT_FLOAT_EXACT(out.kd, in.kd, "kd round-trips");
    EXPECT_FLOAT_EXACT(out.pitch_off, in.pitch_off, "pitch_off round-trips");

    // The whole 4-float struct must be byte-identical.
    EXPECT_TRUE(std::memcmp(&in, &out, sizeof(TuneBlock)) == 0,
                "TuneBlock byte-identical after round-trip");

    test_end();
}

// ============================================================================
// Test 2 — has_tuning() false on a virgin EEPROM (0xFF marker)
// ============================================================================
static void test_has_tuning_empty() {
    test_begin("has_tuning() false on empty (0xFF) EEPROM");

    fresh_eeprom();
    EXPECT_TRUE(!tune_storage::has_tuning(), "virgin EEPROM has no tuning");

    // load_tuning must also refuse and leave the out param untouched.
    TuneBlock out = make_block(1.0f, 2.0f, 3.0f, 4.0f);
    EXPECT_TRUE(!tune_storage::load_tuning(out), "load_tuning fails on virgin");
    EXPECT_FLOAT_EXACT(out.kp, 1.0f, "out untouched on failure (kp)");
    EXPECT_FLOAT_EXACT(out.ki, 2.0f, "out untouched on failure (ki)");

    test_end();
}

// ============================================================================
// Test 3 — CRC rejection: corrupting one payload byte fails load_tuning
// ============================================================================
static void test_crc_rejection() {
    test_begin("CRC rejection: a flipped payload byte fails load_tuning");

    fresh_eeprom();
    TuneBlock in = make_block(48.0f, 6.0f, 22.0f, -8.6f);
    EXPECT_TRUE(tune_storage::save_tuning(in), "save");
    EXPECT_TRUE(tune_storage::has_tuning(), "valid before corruption");

    // Flip one bit in the middle of the 16-byte float payload. Marker and
    // version stay valid, so only the CRC check can catch this.
    g_mock_eeprom[ADDR_PAYLOAD + 5] ^= 0x01;

    TuneBlock out = make_block(0, 0, 0, 0);
    EXPECT_TRUE(!tune_storage::load_tuning(out),
                "load_tuning rejects corrupt payload");
    EXPECT_TRUE(!tune_storage::has_tuning(),
                "has_tuning rejects corrupt payload");

    test_end();
}

// ============================================================================
// Test 4 — Marker rejection: a wrong marker byte fails load_tuning
// ============================================================================
static void test_marker_rejection() {
    test_begin("Marker rejection: wrong marker byte fails load_tuning");

    // Write a fully-valid block, then stomp ONLY the marker. The version + CRC
    // remain consistent, isolating the marker check.
    const uint8_t bad_markers[] = { 0xFF, 0x00, 0xB4 /* one-bit-off 0xB5 */ };
    for (size_t i = 0; i < sizeof(bad_markers); i++) {
        fresh_eeprom();
        TuneBlock in = make_block(48.0f, 6.0f, 22.0f, -8.6f);
        EXPECT_TRUE(tune_storage::save_tuning(in), "save valid");

        g_mock_eeprom[ADDR_MARKER] = bad_markers[i];

        TuneBlock out = make_block(0, 0, 0, 0);
        EXPECT_TRUE(!tune_storage::load_tuning(out),
                    "load_tuning rejects bad marker");
        EXPECT_TRUE(!tune_storage::has_tuning(),
                    "has_tuning rejects bad marker");
    }

    test_end();
}

// ============================================================================
// Test 5 — Version rejection: a wrong version byte fails load_tuning
// ============================================================================
static void test_version_rejection() {
    test_begin("Version rejection: wrong version byte fails load_tuning");

    // The version byte is inside the CRC span (0x201..0x211). To isolate the
    // version check from the CRC check we must keep the CRC consistent — so we
    // craft the block by hand: marker + version=0x02 + zero payload + a CRC
    // that genuinely matches that (version, payload) pair. load_tuning should
    // then reject on the version test BEFORE the CRC test even runs.
    //
    // CRC-8-CCITT (poly 0x07, init 0x00) over the 17-byte span
    // [0x02, then 16 zero bytes]: computed below to keep the test self-checking.
    fresh_eeprom();

    uint8_t span[17];
    span[0] = 0x02;                       // wrong version
    std::memset(&span[1], 0x00, 16);      // zero payload

    // Inline CRC-8-CCITT identical to tune_storage.cpp's crc8_ccitt().
    uint8_t crc = 0x00;
    for (int i = 0; i < 17; i++) {
        crc ^= span[i];
        for (int b = 0; b < 8; b++)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07)
                               : (uint8_t)(crc << 1);
    }

    g_mock_eeprom[ADDR_MARKER]  = TUNE_MARKER;   // valid marker
    g_mock_eeprom[ADDR_VERSION] = 0x02;          // wrong version
    std::memset(&g_mock_eeprom[ADDR_PAYLOAD], 0x00, 16);
    g_mock_eeprom[ADDR_CRC]     = crc;           // CRC is *valid* for this data

    TuneBlock out = make_block(0, 0, 0, 0);
    EXPECT_TRUE(!tune_storage::load_tuning(out),
                "load_tuning rejects wrong version despite valid CRC");
    EXPECT_TRUE(!tune_storage::has_tuning(),
                "has_tuning rejects wrong version");

    test_end();
}

// ============================================================================
// Test 6 — Float fidelity: representative gains survive round-trip exactly
// ============================================================================
static void test_float_fidelity() {
    test_begin("Float fidelity: Kp/Ki/Kd/pitch_off round-trip bit-exact");

    // Representative values from the task brief plus the balance_constants.h
    // seed values (which have many significant digits — a good fidelity probe).
    struct Case { float kp, ki, kd, off; };
    const Case cases[] = {
        { 48.0f,    6.0f,     22.0f,    -8.6f    },   // brief's representative set
        { 94.4873f, 33.6538f, 90.2880f, -4.3327f },   // balance_constants.h seed
        { 0.0f,     0.0f,     0.0f,     0.0f     },   // all-zero
        { 255.5f,  -1.25f,   1e-6f,    1234.5678f },  // mixed magnitude + sign
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        fresh_eeprom();
        TuneBlock in = make_block(cases[i].kp, cases[i].ki,
                                  cases[i].kd, cases[i].off);
        EXPECT_TRUE(tune_storage::save_tuning(in), "save");

        TuneBlock out = make_block(0, 0, 0, 0);
        EXPECT_TRUE(tune_storage::load_tuning(out), "load");
        EXPECT_FLOAT_EXACT(out.kp,        in.kp,        "kp exact");
        EXPECT_FLOAT_EXACT(out.ki,        in.ki,        "ki exact");
        EXPECT_FLOAT_EXACT(out.kd,        in.kd,        "kd exact");
        EXPECT_FLOAT_EXACT(out.pitch_off, in.pitch_off, "pitch_off exact");
    }

    test_end();
}

// ============================================================================
// Test 7 — EEPROM.update() wear-sparing: saving identical data twice is a no-op
// ============================================================================
static void test_update_wear_sparing() {
    test_begin("EEPROM.update wear-sparing: identical re-save is harmless");

    fresh_eeprom();
    TuneBlock in = make_block(48.0f, 6.0f, 22.0f, -8.6f);

    EXPECT_TRUE(tune_storage::save_tuning(in), "first save succeeds");

    // Snapshot the 19-byte block after the first save.
    uint8_t snapshot[19];
    std::memcpy(snapshot, &g_mock_eeprom[TUNE_EEPROM_BASE], 19);

    // Second save of IDENTICAL data. tune_storage uses EEPROM.update(), which
    // only writes bytes that actually changed — so no error, and the medium is
    // byte-for-byte unchanged.
    EXPECT_TRUE(tune_storage::save_tuning(in), "second identical save succeeds");
    EXPECT_TRUE(std::memcmp(snapshot, &g_mock_eeprom[TUNE_EEPROM_BASE], 19) == 0,
                "block unchanged after identical re-save");

    // And the block is still loadable / valid afterwards.
    TuneBlock out = make_block(0, 0, 0, 0);
    EXPECT_TRUE(tune_storage::load_tuning(out), "still loadable after re-save");
    EXPECT_TRUE(std::memcmp(&in, &out, sizeof(TuneBlock)) == 0,
                "data intact after re-save");

    // Saving DIFFERENT data on top must overwrite cleanly (the block region is
    // 19 bytes; a changed value updates only the affected bytes + CRC).
    TuneBlock in2 = make_block(60.0f, 6.0f, 22.0f, -8.6f);  // only kp differs
    EXPECT_TRUE(tune_storage::save_tuning(in2), "overwrite save succeeds");
    TuneBlock out2 = make_block(0, 0, 0, 0);
    EXPECT_TRUE(tune_storage::load_tuning(out2), "overwritten block loads");
    EXPECT_TRUE(std::memcmp(&in2, &out2, sizeof(TuneBlock)) == 0,
                "overwrite produced the new data");

    test_end();
}

// ============================================================================
// main — runs all scenarios
// ============================================================================

int main() {
    printf("================================================\n");
    printf(" tune_storage native unit tests\n");
    printf("================================================\n\n");

    test_round_trip();
    test_has_tuning_empty();
    test_crc_rejection();
    test_marker_rejection();
    test_version_rejection();
    test_float_fidelity();
    test_update_wear_sparing();

    printf("\n================================================\n");
    printf(" Tests run: %d, passed: %d\n", g_tests_run, g_tests_passed);
    printf("================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
