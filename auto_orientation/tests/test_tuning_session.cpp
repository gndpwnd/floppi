/**
 * Unit Tests for TuningSession
 *   (src/applications/balancing_robot_uno/tuning_session.{h,cpp})
 *
 * Purpose
 * -------
 * Pure-native unit tests for the guided-tuning state machine that landed
 * 2026-05-20 (workstream UT-C of uno_guided_tuning_design_2026-05-20.md §2).
 * TuningSession walks the operator IDLE -> STAGE_P -> STAGE_I -> STAGE_D ->
 * REVIEW, masking not-yet-tuned terms to zero and pushing masked gains live
 * via UnoBalanceApp::apply_gains(). Its observable behaviour is: which stage
 * it is in, and what gains it pushes to the balance app.
 *
 * Compile flags
 * -------------
 *   -DUNO_GUIDED_TUNING   REQUIRED — every declaration in tuning_session.{h,cpp}
 *                         is gated by this. Without it the file compiles empty.
 *
 * How the REAL state machine is exercised on the host
 * ----------------------------------------------------
 * tuning_session.cpp #includes <Arduino.h>, "uno_balance_app.h",
 * "balance_constants.h" and "tune_storage.h". The REAL uno_balance_app.h drags
 * in the sensor/motor/PID framework headers; TuningSession itself only ever
 * calls app.apply_gains() and app.get_work_gains(). So we shadow three headers
 * with lightweight test doubles placed FIRST on the include path:
 *
 *   - <Arduino.h>        : stubs Serial / F() / dtostrf — no behaviour under test.
 *   - "uno_balance_app.h": a UnoBalanceApp test double that RECORDS every
 *                          apply_gains() call (this is the spy that lets us
 *                          assert what the state machine pushed).
 *   - "tune_storage.h"   : a TuneBlock + a tune_storage::save_tuning() spy that
 *                          records the block written by REVIEW 'w'.
 *
 * The real tuning_session.cpp logic — stage transitions, gain masking, step
 * sizing, coarse toggle, clamping, snapshots — runs unmodified against these.
 *
 * Canonical compile line
 * ----------------------
 * tuning_session.cpp uses QUOTE-includes ("uno_balance_app.h" etc.) which the
 * compiler resolves relative to tuning_session.cpp's OWN directory FIRST —
 * -I order cannot beat that. So the stubs are force-injected with -include:
 * each stub uses the SAME include guard as the real header, so once -include
 * has pulled the stub in, the later quote-include of the real header is a
 * no-op. This shadows the real headers in BOTH translation units.
 *
 *   STUBDIR=$(mktemp -d)
 *   # write Arduino.h, uno_balance_app.h, tune_storage.h into $STUBDIR
 *   # (full bodies are reproduced in the STUB_HEADERS comment block below)
 *   g++ -std=c++11 -O2 -fpermissive -DUNO_GUIDED_TUNING \
 *       -I"$STUBDIR" -Iinclude -Isrc \
 *       -include "$STUBDIR/Arduino.h" \
 *       -include "$STUBDIR/uno_balance_app.h" \
 *       -include "$STUBDIR/tune_storage.h" \
 *       -o /tmp/test_tuning_session_run \
 *       tests/test_tuning_session.cpp \
 *       src/applications/balancing_robot_uno/tuning_session.cpp
 *   /tmp/test_tuning_session_run
 *
 * Conventions
 * -----------
 *   - printf-based asserts (matches test_calibration_storage.cpp / test_json_formatter.cpp).
 *   - One void function per scenario; main() runs them all and prints
 *     "Tests run: N, passed: M".
 *   - Exits 0 on full pass, non-zero on any failure.
 *
 * Author / Date: test-writer-tuning agent, 2026-05-20
 */

/* ===========================================================================
 * STUB_HEADERS — write these three files verbatim into the temp -I dir.
 * They are reproduced here so the test is reproducible from this file alone.
 * ---------------------------------------------------------------------------
 *
 * ----- Arduino.h -----
 *   #ifndef STUB_ARDUINO_H
 *   #define STUB_ARDUINO_H
 *   #include <stdint.h>
 *   #include <stdio.h>
 *   #include <stdlib.h>
 *   #define F(x) (x)
 *   typedef const char* __FlashStringHelper_t;
 *   class StubSerial {
 *    public:
 *     void print(const char*) {}
 *     void print(int) {}
 *     void println(const char*) {}
 *     void println(int) {}
 *     void println() {}
 *     void begin(long) {}
 *   };
 *   extern StubSerial Serial;
 *   // dtostrf: minimal Arduino-AVR float formatter shim.
 *   static inline char* dtostrf(double v, signed char w, unsigned char p, char* s) {
 *     char fmt[16];
 *     snprintf(fmt, sizeof(fmt), "%%%d.%df", (int)w, (int)p);
 *     sprintf(s, fmt, v);
 *     return s;
 *   }
 *   #endif
 *
 * ----- uno_balance_app.h -----  (TEST DOUBLE — records apply_gains calls)
 *   NOTE: the include guard MUST match the REAL header's guard
 *   (UNO_BALANCE_APP_H). tuning_session.h uses a quote-include
 *   `#include "uno_balance_app.h"` which resolves to the real header in the
 *   same directory; a matching guard makes whichever copy is seen first win,
 *   so including this stub first shadows the real one.
 *   #ifndef UNO_BALANCE_APP_H
 *   #define UNO_BALANCE_APP_H
 *   class UnoBalanceApp {
 *    public:
 *     float boot_kp, boot_ki, boot_kd;          // gains "booted" with
 *     float last_kp, last_ki, last_kd;          // last apply_gains() args
 *     int   apply_count;
 *     UnoBalanceApp() : boot_kp(0), boot_ki(0), boot_kd(0),
 *                       last_kp(0), last_ki(0), last_kd(0), apply_count(0) {}
 *     void apply_gains(float kp, float ki, float kd) {
 *       last_kp = kp; last_ki = ki; last_kd = kd; apply_count++;
 *     }
 *     void get_work_gains(float& kp, float& ki, float& kd) const {
 *       kp = boot_kp; ki = boot_ki; kd = boot_kd;
 *     }
 *   };
 *   #endif
 *
 * ----- tune_storage.h -----  (TEST DOUBLE — records save_tuning calls)
 *   NOTE: guard MUST match the real header's guard (TUNE_STORAGE_H), same
 *   shadowing rationale as uno_balance_app.h above.
 *   #ifndef TUNE_STORAGE_H
 *   #define TUNE_STORAGE_H
 *   #include <stdint.h>
 *   struct TuneBlock { float kp, ki, kd, pitch_off; };
 *   namespace tune_storage {
 *     extern TuneBlock g_saved;
 *     extern int       g_save_count;
 *     extern bool      g_save_ret;
 *     bool save_tuning(const TuneBlock& blk);
 *     bool load_tuning(TuneBlock& out);
 *     bool has_tuning();
 *   }
 *   #endif
 *
 * The test .cpp below DEFINES the storage for the externs (Serial,
 * tune_storage::g_*) and the save_tuning/load_tuning/has_tuning bodies.
 * ===========================================================================
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "uno_balance_app.h"   // resolves to the STUB on the -I path
#include "tune_storage.h"      // resolves to the STUB on the -I path
#include "Arduino.h"           // resolves to the STUB on the -I path

// The real header under test. Gated by UNO_GUIDED_TUNING (set on the cmd line).
#include "../src/applications/balancing_robot_uno/tuning_session.h"

// ----------------------------------------------------------------------------
// Definitions for the stub externs.
// ----------------------------------------------------------------------------
StubSerial Serial;

namespace tune_storage {
TuneBlock g_saved      = { 0, 0, 0, 0 };
int       g_save_count = 0;
bool      g_save_ret   = true;   // save_tuning return value (toggle to test fail path)

bool save_tuning(const TuneBlock& blk) {
    g_saved = blk;
    g_save_count++;
    return g_save_ret;
}
bool load_tuning(TuneBlock&) { return false; }
bool has_tuning()            { return false; }
}  // namespace tune_storage

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

#define EXPECT_FLOAT_EQ(actual, expected, msg)                              \
    do {                                                                    \
        float _a = (float)(actual);                                         \
        float _e = (float)(expected);                                       \
        float _d = _a - _e; if (_d < 0) _d = -_d;                           \
        if (_d > 1e-4f) {                                                   \
            printf("  FAIL: %s (line %d): got %.6f, expected %.6f\n",       \
                   msg, __LINE__, (double)_a, (double)_e);                  \
            g_current_failed = true;                                        \
        }                                                                   \
    } while (0)

// ============================================================================
// Helpers
// ----------------------------------------------------------------------------
// TuningSession exposes no stage getter — its observable behaviour is the
// gains it pushes via apply_gains(). We verify stage indirectly through the
// per-stage masking contract (design §2.1):
//   STAGE_P  : Ki and Kd are forced to 0
//   STAGE_I  : Kd is forced to 0; Kp is the locked value
//   STAGE_D  : full gains active
//   REVIEW   : full gains active (no further masking)
//   IDLE     : entering does not push gains
// "entering a stage" always triggers exactly one apply_gains() (enter_stage_).
// ============================================================================

// Build a session bound to a fresh app double that "booted" with given gains.
struct Fixture {
    UnoBalanceApp app;
    TuningSession session;
    Fixture(float bk = 0, float bi = 0, float bd = 0) {
        app.boot_kp = bk;
        app.boot_ki = bi;
        app.boot_kd = bd;
        session.begin(app);          // seeds work gains from boot gains
        app.apply_count = 0;         // ignore any apply during begin()
    }
};

// ============================================================================
// Test 1 — 't' from IDLE advances to STAGE_P
// ----------------------------------------------------------------------------
// 't' is valid only in IDLE (design §2.2). Entering STAGE_P triggers an
// apply_gains() with the P mask (Ki=Kd=0).
// ============================================================================
static void test_t_idle_to_stage_p() {
    test_begin("'t' from IDLE enters STAGE_P");

    Fixture f(/*boot*/ 30.0f, 8.0f, 12.0f);
    f.session.handle_command('t');

    EXPECT_TRUE(f.app.apply_count >= 1, "entering STAGE_P pushed gains");
    // STAGE_P mask: Ki=Kd=0, Kp = booted Kp (work gains seeded from boot).
    EXPECT_FLOAT_EQ(f.app.last_kp, 30.0f, "STAGE_P keeps booted Kp");
    EXPECT_FLOAT_EQ(f.app.last_ki, 0.0f,  "STAGE_P forces Ki=0");
    EXPECT_FLOAT_EQ(f.app.last_kd, 0.0f,  "STAGE_P forces Kd=0");

    test_end();
}

// ============================================================================
// Test 2 — STAGE_P forces Ki=0 and Kd=0 on every push
// ============================================================================
static void test_stage_p_masks_i_and_d() {
    test_begin("STAGE_P always pushes I=D=0");

    Fixture f(20.0f, 5.0f, 7.0f);
    f.session.handle_command('t');                 // -> STAGE_P
    f.session.handle_command('+');                 // adjust Kp
    EXPECT_FLOAT_EQ(f.app.last_ki, 0.0f, "Ki still 0 after '+'");
    EXPECT_FLOAT_EQ(f.app.last_kd, 0.0f, "Kd still 0 after '+'");
    f.session.handle_command('-');
    EXPECT_FLOAT_EQ(f.app.last_ki, 0.0f, "Ki still 0 after '-'");
    EXPECT_FLOAT_EQ(f.app.last_kd, 0.0f, "Kd still 0 after '-'");

    test_end();
}

// ============================================================================
// Test 3 — '+'/'-' adjust Kp by KP_STEP (2.0); '-' clamps at 0
// ============================================================================
static void test_stage_p_step_and_clamp() {
    test_begin("STAGE_P '+'/'-' step Kp by KP_STEP, '-' clamps at >=0");

    Fixture f(10.0f, 0.0f, 0.0f);
    f.session.handle_command('t');                 // STAGE_P, work_kp_=10
    EXPECT_FLOAT_EQ(TuningSession::KP_STEP, 2.0f, "KP_STEP is 2.0");

    f.session.handle_command('+');                 // 10 + 2 = 12
    EXPECT_FLOAT_EQ(f.app.last_kp, 12.0f, "'+' adds KP_STEP");
    f.session.handle_command('+');                 // 14
    EXPECT_FLOAT_EQ(f.app.last_kp, 14.0f, "'+' adds again");
    f.session.handle_command('-');                 // 12
    EXPECT_FLOAT_EQ(f.app.last_kp, 12.0f, "'-' subtracts KP_STEP");

    // Clamp: drive Kp below zero — must stop at 0, never negative.
    for (int i = 0; i < 20; i++) f.session.handle_command('-');
    EXPECT_FLOAT_EQ(f.app.last_kp, 0.0f, "Kp clamps at 0, not negative");

    test_end();
}

// ============================================================================
// Test 4 — '*' coarse toggle multiplies the step by COARSE_MULT (5)
// ============================================================================
static void test_coarse_toggle() {
    test_begin("'*' coarse toggle makes the step x5");

    Fixture f(50.0f, 0.0f, 0.0f);
    f.session.handle_command('t');                 // STAGE_P, work_kp_=50
    EXPECT_TRUE(TuningSession::COARSE_MULT == 5, "COARSE_MULT is 5");

    f.session.handle_command('*');                 // coarse ON
    f.session.handle_command('+');                 // 50 + 2*5 = 60
    EXPECT_FLOAT_EQ(f.app.last_kp, 60.0f, "coarse '+' adds KP_STEP*5");

    f.session.handle_command('*');                 // coarse OFF
    f.session.handle_command('+');                 // 60 + 2 = 62
    EXPECT_FLOAT_EQ(f.app.last_kp, 62.0f, "fine '+' adds KP_STEP again");

    test_end();
}

// ============================================================================
// Test 5 — 'n' STAGE_P -> STAGE_I locks Kp; STAGE_I forces Kd=0
// ============================================================================
static void test_n_stage_p_to_i_locks_kp() {
    test_begin("'n' STAGE_P->STAGE_I locks Kp, STAGE_I forces Kd=0");

    Fixture f(40.0f, 9.0f, 15.0f);
    f.session.handle_command('t');                 // STAGE_P, work_kp_=40
    f.session.handle_command('+');                 // work_kp_ = 42
    f.session.handle_command('+');                 // work_kp_ = 44
    f.session.handle_command('n');                 // -> STAGE_I

    // STAGE_I: Kp is the locked (adjusted) value, Ki active, Kd forced 0.
    EXPECT_FLOAT_EQ(f.app.last_kp, 44.0f, "STAGE_I keeps locked Kp=44");
    EXPECT_FLOAT_EQ(f.app.last_ki, 9.0f,  "STAGE_I uses work Ki");
    EXPECT_FLOAT_EQ(f.app.last_kd, 0.0f,  "STAGE_I forces Kd=0");

    // '+' in STAGE_I adjusts Ki by KI_STEP (1.0), Kp stays locked.
    EXPECT_FLOAT_EQ(TuningSession::KI_STEP, 1.0f, "KI_STEP is 1.0");
    f.session.handle_command('+');                 // Ki = 10
    EXPECT_FLOAT_EQ(f.app.last_ki, 10.0f, "'+' steps Ki by KI_STEP");
    EXPECT_FLOAT_EQ(f.app.last_kp, 44.0f, "Kp still locked at 44");
    EXPECT_FLOAT_EQ(f.app.last_kd, 0.0f,  "Kd still 0 in STAGE_I");

    test_end();
}

// ============================================================================
// Test 6 — 'n' STAGE_I -> STAGE_D: full gains active
// ============================================================================
static void test_n_stage_i_to_d_full_gains() {
    test_begin("'n' STAGE_I->STAGE_D activates full gains");

    Fixture f(40.0f, 9.0f, 15.0f);
    f.session.handle_command('t');                 // STAGE_P
    f.session.handle_command('n');                 // -> STAGE_I
    f.session.handle_command('n');                 // -> STAGE_D

    // STAGE_D: P, I locked at work values, D active. No masking.
    EXPECT_FLOAT_EQ(f.app.last_kp, 40.0f, "STAGE_D keeps Kp");
    EXPECT_FLOAT_EQ(f.app.last_ki, 9.0f,  "STAGE_D keeps Ki");
    EXPECT_FLOAT_EQ(f.app.last_kd, 15.0f, "STAGE_D uses full Kd");

    // '+' steps Kd by KD_STEP (2.0).
    EXPECT_FLOAT_EQ(TuningSession::KD_STEP, 2.0f, "KD_STEP is 2.0");
    f.session.handle_command('+');                 // Kd = 17
    EXPECT_FLOAT_EQ(f.app.last_kd, 17.0f, "'+' steps Kd by KD_STEP");
    EXPECT_FLOAT_EQ(f.app.last_kp, 40.0f, "Kp still locked in STAGE_D");
    EXPECT_FLOAT_EQ(f.app.last_ki, 9.0f,  "Ki still locked in STAGE_D");

    test_end();
}

// ============================================================================
// Test 7 — 'n' STAGE_D -> REVIEW: full gains, no masking
// ============================================================================
static void test_n_stage_d_to_review() {
    test_begin("'n' STAGE_D->REVIEW keeps full gains");

    Fixture f(33.0f, 7.0f, 11.0f);
    f.session.handle_command('t');                 // STAGE_P
    f.session.handle_command('n');                 // STAGE_I
    f.session.handle_command('n');                 // STAGE_D
    int before = f.app.apply_count;
    f.session.handle_command('n');                 // -> REVIEW

    EXPECT_TRUE(f.app.apply_count > before, "entering REVIEW pushed gains");
    EXPECT_FLOAT_EQ(f.app.last_kp, 33.0f, "REVIEW Kp full");
    EXPECT_FLOAT_EQ(f.app.last_ki, 7.0f,  "REVIEW Ki full");
    EXPECT_FLOAT_EQ(f.app.last_kd, 11.0f, "REVIEW Kd full");

    // In REVIEW, '+'/'-' are not adjust commands — they must be inert
    // (handle_command's default stage branch returns without adjusting).
    int rev_apply = f.app.apply_count;
    f.session.handle_command('+');
    EXPECT_TRUE(f.app.apply_count == rev_apply, "'+' inert in REVIEW");
    EXPECT_FLOAT_EQ(f.app.last_kp, 33.0f, "REVIEW Kp unchanged by '+'");

    test_end();
}

// ============================================================================
// Test 8 — 'b' from STAGE_I returns to STAGE_P
// ============================================================================
static void test_b_stage_i_back_to_p() {
    test_begin("'b' from STAGE_I returns to STAGE_P");

    Fixture f(25.0f, 6.0f, 9.0f);
    f.session.handle_command('t');                 // STAGE_P
    f.session.handle_command('n');                 // STAGE_I
    f.session.handle_command('b');                 // back -> STAGE_P

    // Back in STAGE_P: I=D masked to 0 again proves the stage moved back.
    EXPECT_FLOAT_EQ(f.app.last_ki, 0.0f, "back in STAGE_P forces Ki=0");
    EXPECT_FLOAT_EQ(f.app.last_kd, 0.0f, "back in STAGE_P forces Kd=0");
    EXPECT_FLOAT_EQ(f.app.last_kp, 25.0f, "Kp retained on the way back");

    // '+' now steps Kp (proving we're in STAGE_P, not STAGE_I).
    f.session.handle_command('+');                 // Kp = 27
    EXPECT_FLOAT_EQ(f.app.last_kp, 27.0f, "'+' steps Kp -> confirms STAGE_P");
    EXPECT_FLOAT_EQ(f.app.last_ki, 0.0f, "Ki still masked");

    test_end();
}

// ============================================================================
// Test 9 — 'r' resets the current term to its stage-entry snapshot
// ============================================================================
static void test_r_resets_to_stage_entry() {
    test_begin("'r' resets current term to stage-entry value");

    Fixture f(30.0f, 0.0f, 0.0f);
    f.session.handle_command('t');                 // STAGE_P, entry_kp_=30
    f.session.handle_command('+');                 // Kp = 32
    f.session.handle_command('+');                 // Kp = 34
    EXPECT_FLOAT_EQ(f.app.last_kp, 34.0f, "Kp moved to 34");

    f.session.handle_command('r');                 // reset -> entry value 30
    EXPECT_FLOAT_EQ(f.app.last_kp, 30.0f, "'r' restores stage-entry Kp=30");

    // In STAGE_I, 'r' resets Ki to ITS stage-entry snapshot (taken on 'n').
    f.session.handle_command('n');                 // -> STAGE_I, entry_ki_=0
    f.session.handle_command('+');                 // Ki = 1
    f.session.handle_command('+');                 // Ki = 2
    f.session.handle_command('r');                 // reset Ki -> 0
    EXPECT_FLOAT_EQ(f.app.last_ki, 0.0f, "'r' restores stage-entry Ki=0");
    EXPECT_FLOAT_EQ(f.app.last_kp, 30.0f, "'r' in STAGE_I leaves Kp alone");

    test_end();
}

// ============================================================================
// Test 10 — 'q' from any stage returns to IDLE and restores booted gains
// ============================================================================
static void test_q_quits_and_restores_boot_gains() {
    test_begin("'q' from a stage -> IDLE, restores booted gains");

    Fixture f(/*boot*/ 18.0f, 4.0f, 6.0f);
    f.session.handle_command('t');                 // STAGE_P
    f.session.handle_command('n');                 // STAGE_I
    f.session.handle_command('+');                 // perturb Ki
    f.session.handle_command('+');
    f.session.handle_command('q');                 // quit

    // After 'q', work gains are re-read from the app's boot gains. The state
    // machine itself pushes no apply_gains() on 'q' (design: caller restores).
    // We verify the restore by re-entering STAGE_P: the masked push must use
    // the *booted* Kp again, proving the perturbed work gains were discarded.
    f.session.handle_command('t');                 // IDLE->STAGE_P again
    EXPECT_FLOAT_EQ(f.app.last_kp, 18.0f, "work Kp restored to booted 18");
    EXPECT_FLOAT_EQ(f.app.last_ki, 0.0f,  "STAGE_P masks Ki");
    EXPECT_FLOAT_EQ(f.app.last_kd, 0.0f,  "STAGE_P masks Kd");

    // 'q' from REVIEW must also reach IDLE.
    Fixture g(20.0f, 5.0f, 8.0f);
    g.session.handle_command('t');
    g.session.handle_command('n');
    g.session.handle_command('n');
    g.session.handle_command('n');                 // REVIEW
    g.session.handle_command('q');                 // quit from REVIEW
    int before = g.app.apply_count;
    g.session.handle_command('t');                 // only works if in IDLE
    EXPECT_TRUE(g.app.apply_count > before, "'t' worked -> 'q' reached IDLE");

    test_end();
}

// ============================================================================
// Test 11 — Stage sequence integrity: out-of-stage commands are inert
// ----------------------------------------------------------------------------
// design §2.2 valid-in columns: '+'/'-'/'*'/'r' only in STAGE_P/I/D; 't' only
// in IDLE; 'w' only in REVIEW; 'b' not from STAGE_P or IDLE.
// ============================================================================
static void test_stage_sequence_integrity() {
    test_begin("Stage sequence integrity: invalid commands are inert");

    // --- '+' in IDLE does nothing ---
    {
        Fixture f(10.0f, 2.0f, 3.0f);
        f.session.handle_command('+');             // IDLE — must be inert
        EXPECT_TRUE(f.app.apply_count == 0, "'+' in IDLE pushes no gains");
    }

    // --- 'w' (save) only fires in REVIEW ---
    {
        tune_storage::g_save_count = 0;
        Fixture f(10.0f, 2.0f, 3.0f);
        f.session.handle_command('w');             // IDLE — no save
        f.session.handle_command('t');             // STAGE_P
        f.session.handle_command('w');             // STAGE_P — no save
        EXPECT_TRUE(tune_storage::g_save_count == 0,
                    "'w' outside REVIEW does not save");

        f.session.handle_command('n');             // STAGE_I
        f.session.handle_command('n');             // STAGE_D
        f.session.handle_command('n');             // REVIEW
        f.session.handle_command('w');             // REVIEW — saves
        EXPECT_TRUE(tune_storage::g_save_count == 1,
                    "'w' in REVIEW triggers exactly one save_tuning");
        // The saved block carries the working PID gains.
        EXPECT_FLOAT_EQ(tune_storage::g_saved.kp, 10.0f, "saved Kp");
        EXPECT_FLOAT_EQ(tune_storage::g_saved.ki, 2.0f,  "saved Ki");
        EXPECT_FLOAT_EQ(tune_storage::g_saved.kd, 3.0f,  "saved Kd");
    }

    // --- 'b' from STAGE_P is inert (no stage before P) ---
    {
        Fixture f(12.0f, 0.0f, 0.0f);
        f.session.handle_command('t');             // STAGE_P
        int before = f.app.apply_count;
        f.session.handle_command('b');             // inert — P is first stage
        EXPECT_TRUE(f.app.apply_count == before,
                    "'b' from STAGE_P pushes nothing (no prior stage)");
        // Confirm still in STAGE_P: '+' must step Kp with the P mask.
        f.session.handle_command('+');
        EXPECT_FLOAT_EQ(f.app.last_kp, 14.0f, "still STAGE_P after inert 'b'");
        EXPECT_FLOAT_EQ(f.app.last_ki, 0.0f,  "still STAGE_P mask after 'b'");
    }

    // --- 't' in a non-IDLE stage is inert (no restart mid-session) ---
    {
        Fixture f(20.0f, 0.0f, 0.0f);
        f.session.handle_command('t');             // STAGE_P
        f.session.handle_command('+');             // Kp = 22
        f.session.handle_command('t');             // inert — already tuning
        EXPECT_FLOAT_EQ(f.app.last_kp, 22.0f,
                        "'t' mid-session does not reset Kp");
    }

    // --- 'n' from REVIEW is inert (REVIEW is the last stage) ---
    {
        Fixture f(15.0f, 3.0f, 5.0f);
        f.session.handle_command('t');
        f.session.handle_command('n');
        f.session.handle_command('n');
        f.session.handle_command('n');             // REVIEW
        int before = f.app.apply_count;
        f.session.handle_command('n');             // inert
        EXPECT_TRUE(f.app.apply_count == before,
                    "'n' from REVIEW pushes nothing (last stage)");
    }

    test_end();
}

// ============================================================================
// Test 12 — 'w' SAVE FAILED path: save_tuning() returning false is handled
// ----------------------------------------------------------------------------
// Bonus. tuning_session.cpp branches on save_tuning()'s return value. We force
// the storage double to return false and confirm the call still happens (the
// state machine does not crash / does not mark success).
// ============================================================================
static void test_w_save_failed_path() {
    test_begin("'w' handles save_tuning()==false without crashing");

    tune_storage::g_save_count = 0;
    tune_storage::g_save_ret   = false;            // force failure

    Fixture f(11.0f, 2.5f, 4.0f);
    f.session.handle_command('t');
    f.session.handle_command('n');
    f.session.handle_command('n');
    f.session.handle_command('n');                 // REVIEW
    f.session.handle_command('w');                 // save attempt -> fails

    EXPECT_TRUE(tune_storage::g_save_count == 1,
                "save_tuning still invoked on the failure path");

    tune_storage::g_save_ret = true;               // restore for other tests

    test_end();
}

// ============================================================================
// main — runs all scenarios
// ============================================================================

int main() {
    printf("================================================\n");
    printf(" TuningSession native unit tests\n");
    printf("================================================\n\n");

    test_t_idle_to_stage_p();
    test_stage_p_masks_i_and_d();
    test_stage_p_step_and_clamp();
    test_coarse_toggle();
    test_n_stage_p_to_i_locks_kp();
    test_n_stage_i_to_d_full_gains();
    test_n_stage_d_to_review();
    test_b_stage_i_back_to_p();
    test_r_resets_to_stage_entry();
    test_q_quits_and_restores_boot_gains();
    test_stage_sequence_integrity();
    test_w_save_failed_path();

    printf("\n================================================\n");
    printf(" Tests run: %d, passed: %d\n", g_tests_run, g_tests_passed);
    printf("================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
