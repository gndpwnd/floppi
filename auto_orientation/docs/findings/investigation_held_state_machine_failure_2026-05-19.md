# Investigation — `test_held_state_machine` reports 3 fail / 3 pass on main

**Date:** 2026-05-19
**Investigator:** read-only investigation agent
**Trigger:** Patch agent (collision-detection workstream) reported a pre-existing 3-fail / 3-pass on `tests/test_held_state_machine` after stashing their changes and re-running. Said this was unrelated to their patch, committed at 7a4d27f.
**Scope:** READ-ONLY. No source edits, no commits. One findings note (this file) permitted.

---

## TL;DR

The HELD state-machine **source code and test source code are both correct**. The
test suite passes 8/8 when compiled with the documented build line.

The failure the patch agent observed is **a stale binary**: the `tests/test_held_state_machine`
executable checked into the working tree was compiled **without
`-DUSE_BALANCE_HELD_DETECTION`**, so the entire RUN→HELD entry block in
`balance_app.cpp` (lines 437–477, guarded by `#ifdef USE_BALANCE_HELD_DETECTION`)
was elided. With that block compiled out, the bot can never enter HELD, and
all three tests time out trying to do so — producing exactly the 3 fail / 3
pass pattern.

This is a **build-process / shell-environment bug**, not a HELD logic bug. The
collision-recovery path is not broken. The safety net is intact.

---

## 1. Reproduction commands

All from `/home/devel/floppi/auto_orientation`. **No source edits required.**

### 1a. Reproduce the failure (matches what patch agent saw)

```bash
# Run the pre-built binary that lives in the working tree
./tests/test_held_state_machine
```

Output:
```
====================================================
BalanceApp HELD state-machine tests (Workstream 3)
====================================================

Test: HELD via Phase 2.5 ext_motion (cmd-quiet + pitch-gyro-fast)
  entered HELD: no (after -1 ticks = -5 ms)
  FAIL (tests/test_held_state_machine.cpp:225): RUN -> HELD on ext_motion (cmd quiet + pitch_gyro fast)
  PASS: motors.left == 0 in HELD
  PASS: motors.right == 0 in HELD
  PASS: last_output == 0 in HELD

Test: HELD -> RUN when motion calms (200 ms dwell, lenient)
  FAIL: did not reach HELD before resume test

Test: pitch in HELD does NOT drive motors
  FAIL: did not reach HELD

====================================================
Results: 3 passed, 3 failed
====================================================
exit: 1
```

### 1b. Rebuild correctly — passes 8/8

Build line copied verbatim from the test header comment
(`tests/test_held_state_machine.cpp:15-26`):

```bash
g++ -std=c++11 -O2 -fpermissive -DUNIT_TEST -DUSE_BALANCE_HELD_DETECTION \
    -o /tmp/test_held_correct \
    tests/test_held_state_machine.cpp \
    src/applications/balancing_robot/balance_app.cpp \
    src/applications/balancing_robot/safety.cpp \
    src/control/pid_controller.cpp \
    src/control/auto_pid_tuner.cpp \
    src/control/plant_identifier.cpp \
    src/navigation/mounting_calibration.cpp \
    src/navigation/online_mounting_estimator.cpp

/tmp/test_held_correct
```

Output:
```
Results: 8 passed, 0 failed
exit: 0
```

### 1c. Confirming root cause — drop the define, reproduce exactly

```bash
g++ -std=c++11 -O2 -fpermissive -DUNIT_TEST \
    -o /tmp/test_held_no_flag \
    tests/test_held_state_machine.cpp \
    src/applications/balancing_robot/balance_app.cpp \
    src/applications/balancing_robot/safety.cpp \
    src/control/pid_controller.cpp \
    src/control/auto_pid_tuner.cpp \
    src/control/plant_identifier.cpp \
    src/navigation/mounting_calibration.cpp \
    src/navigation/online_mounting_estimator.cpp

/tmp/test_held_no_flag
```

Output:
```
Results: 3 passed, 3 failed
```

**md5 of `/tmp/test_held_no_flag` matches `tests/test_held_state_machine` exactly**
(`c92c85158ee04296bc81ed22de50a482`). This is the smoking gun — the broken
binary on disk is byte-identical to a fresh build that omits the define.

### 1d. Reproduce from pristine `main` (7a4d27f) to rule out local mutations

```bash
git worktree add --detach /tmp/floppi-main-ro 7a4d27f
cd /tmp/floppi-main-ro/auto_orientation
g++ -std=c++11 -O2 -fpermissive -DUNIT_TEST -DUSE_BALANCE_HELD_DETECTION \
    -o /tmp/test_held_main \
    tests/test_held_state_machine.cpp \
    src/applications/balancing_robot/balance_app.cpp \
    src/applications/balancing_robot/safety.cpp \
    src/control/pid_controller.cpp \
    src/control/auto_pid_tuner.cpp \
    src/control/plant_identifier.cpp \
    src/navigation/mounting_calibration.cpp \
    src/navigation/online_mounting_estimator.cpp

/tmp/test_held_main          # → 8 passed, 0 failed
```

`main` is clean.

---

## 2. Failing-test enumeration

All three failures stem from the SAME root cause (HELD entry block elided →
state never reaches HELD). The 3 "passes" inside the first test are vacuous —
they assert `motors == 0` and `last_output == 0` immediately after `enter_run`,
when the app is briefly settled with PID output ~0 because pitch=0. They
would also pass on a totally broken build.

| # | Test name | Failing assert | Expected | Actual |
|---|---|---|---|---|
| 1 | `test_high_lateral_gyro_triggers_held` | `tests/test_held_state_machine.cpp:225` `TEST_ASSERT(entered_held, "RUN -> HELD on ext_motion (cmd quiet + pitch_gyro fast)")` | After driving `gyro_y=200 dps` with `pitch=0` for up to 100 ticks (500 ms), state transitions RUN→HELD via the Phase 2.5 ext_motion gate (`balance_app.cpp:465`). | Stays in RUN forever. Counter `hold_enter_count_` never increments because the entire `#ifdef USE_BALANCE_HELD_DETECTION` block (`balance_app.cpp:437-477`) is compiled out. |
| 2 | `test_held_quiet_motion_resumes_run` | Early-return at `tests/test_held_state_machine.cpp:253-254` `printf("  FAIL: did not reach HELD before resume test\n"); g_fails++;` | Same setup as #1 to force HELD, then quiet+level for up to 300 ticks → HELD→RUN. | Setup-phase HELD entry fails (same reason as #1), so the test bails before reaching the resume-logic assertion. |
| 3 | `test_pitch_in_held_does_not_drive_motors` | Early-return at `tests/test_held_state_machine.cpp:303-304` `printf("  FAIL: did not reach HELD\n"); g_fails++;` | Same setup as #1 to force HELD, then assert motors stay 0 with jittered pitch. | Same as #2 — setup-phase HELD entry fails, test bails. |

The three "PASS" lines inside test 1 (`motors.left == 0`, `motors.right == 0`,
`last_output == 0`) are spurious survivors — they sample motor state when the
app is sitting in RUN with `pitch=0`, so the PID legitimately commands ~0.
They have nothing to do with HELD behaviour and would pass even on this broken
build.

---

## 3. Root-cause hypotheses

### Hypothesis A — **Environmental / build process bug (CONFIRMED, sole cause).**

**Evidence:**

1. The committed test source is byte-identical between local working tree and
   `7a4d27f`: `diff /tmp/held_test_original/test_held_state_machine.cpp
   tests/test_held_state_machine.cpp` → identical (`exit 0`).
2. The balance-app source in the local tree is unchanged from 7a4d27f
   (`git diff HEAD -- src/applications/balancing_robot/balance_app.{cpp,h}` →
   empty). It was briefly diverged earlier this session per the conversation's
   initial `gitStatus` snapshot, but is now reverted.
3. Compiling the test correctly (with `-DUSE_BALANCE_HELD_DETECTION`) against
   either the local tree OR the pristine `main` worktree gives **8/8 pass**.
4. Compiling without the define gives exactly the observed **3 fail / 3 pass**
   and the produced binary is **byte-identical (md5
   `c92c85158ee04296bc81ed22de50a482`) to the binary currently committed in
   `tests/test_held_state_machine`** at the moment this report was written.
5. The HELD gates in `balance_app.cpp:437-477` are wrapped in
   `#ifdef USE_BALANCE_HELD_DETECTION`. With the flag absent, the only
   transitions into HELD are operator commands and `enter_state_(HELD)` from
   the fall-detection path (also flag-gated) — none of which the test
   exercises. So HELD is structurally unreachable from RUN in this binary.

### Hypothesis B — Test bug. **Ruled out.**

Test source compiles and runs cleanly. Asserts match the documented Phase 2.5
ext_motion semantics in `balance_app.cpp:432-476`. Comments in the test
match the actual thresholds in code (dwell=20 ticks, `last_cmd_mag<20`,
`abs_pitch_gyro>30`). The Mock IMU correctly implements `getRawGyro()` /
`getRawAccel()` which the HELD-entry path reads.

### Hypothesis C — Code bug. **Ruled out.**

`step_run_()` HELD-entry logic is intact; `step_held_()` resume-gate is intact;
state transitions are correct. When built with the right flag, every assertion
in the test fires and passes.

---

## 4. Recommended fix

**This is not a source-code problem. Two complementary fixes belong in the
build/process layer:**

### Fix 1 (mandatory) — delete or rebuild the stale binary

`tests/test_held_state_machine` (the executable, not the `.cpp`) is **not
tracked in git** (`git ls-files --error-unmatch tests/test_held_state_machine`
returns "did not match"). It was produced by an automated process — likely a
bulk rebuild loop that processes every `tests/test_*.cpp` with a generic g++
invocation that doesn't propagate per-test build flags. The other four balance
test binaries built at the same timestamp (16:41-16:42 today) all pass; they
just don't depend on this particular `#ifdef`.

Mitigation: the next session that touches the tests directory should either
- `rm tests/test_held_state_machine` and rebuild fresh, OR
- add the binary to `.gitignore` so stale copies don't masquerade as truth.

### Fix 2 (recommended) — codify the build in `tools/build_tests.sh`

`tools/build_tests.sh` currently only knows how to build the three legacy math
tests. It does NOT build any of the balancing-robot tests. Whoever has been
rebuilding `tests/test_balance_app*` and `tests/test_held_state_machine` is
doing so out-of-band, evidently sometimes without the right defines.

Suggested addition (single hunk at end of `tools/build_tests.sh`):

```bash
echo "Building HELD state-machine tests..."
g++ -std=c++11 -O2 -fpermissive -DUNIT_TEST -DUSE_BALANCE_HELD_DETECTION \
    -I. \
    -o tests/test_held_state_machine \
    tests/test_held_state_machine.cpp \
    src/applications/balancing_robot/balance_app.cpp \
    src/applications/balancing_robot/safety.cpp \
    src/control/pid_controller.cpp \
    src/control/auto_pid_tuner.cpp \
    src/control/plant_identifier.cpp \
    src/navigation/mounting_calibration.cpp \
    src/navigation/online_mounting_estimator.cpp
```

(NB: as instructed, I have NOT made this edit — diagnosis only.)

### Fix 3 (defensive, optional) — compile-time guard inside the test

To prevent "binary built without the flag silently produces meaningless
failures", add a `#if !defined(USE_BALANCE_HELD_DETECTION)` `#error` at the
top of `tests/test_held_state_machine.cpp`:

```cpp
#if !defined(USE_BALANCE_HELD_DETECTION)
#error "test_held_state_machine REQUIRES -DUSE_BALANCE_HELD_DETECTION; see build line in header comment"
#endif
```

This forces the build to fail loudly instead of silently producing a binary
that fails 3 assertions for unrelated reasons.

---

## 5. Impact assessment — does this mask collision-resilience risk?

**No.** The HELD logic is correct and exercised correctly when built per the
documented build line. The collision-detection patch the parallel agent is
working on can rely on the HELD safety net.

Risks that DO exist but are out of scope for this investigation:

- **Operational silence.** The stale binary has been committed to the working
  tree (not git, but on disk) at least since 16:41 today. Anyone running the
  test by invoking the binary directly will see false-positive 3-fail
  reports, leading to false-alarm investigations like this one. Fixing
  build_tests.sh (Fix 2 above) removes this whole class of failure.
- **No production-flag coverage on the OFF branch.** If a future build env
  ships with `USE_BALANCE_HELD_DETECTION` undefined (e.g. an ultra-tight Uno
  build that strips it for flash), there is currently no test that verifies
  the bot at least behaves sanely (motors keep going / no UB) without HELD.
  Out of scope for this finding, but worth a P3 backlog entry.
- **`tools/build_tests.sh` coverage gap.** Five balance test binaries
  (test_balance_app, test_balance_app_bootstrap, test_balance_app_collision,
  test_balance_app_soft_cutoff, test_held_state_machine) and several others
  (test_l298n_motor, test_pid_controller, test_plant_identifier, etc.) are
  built by some unspecified out-of-band mechanism. The committed
  `tools/build_tests.sh` only knows about the three legacy math binaries.
  This is the deeper bug. The HELD failure is just the most visible symptom.

The HELD state machine itself, and the collision-detection→HELD path that
depends on it, are healthy. The patch agent should proceed.

---

## Appendix — supporting commands

```bash
# Hash equality proof
md5sum tests/test_held_state_machine /tmp/test_held_no_flag
# c92c85158ee04296bc81ed22de50a482  tests/test_held_state_machine
# c92c85158ee04296bc81ed22de50a482  /tmp/test_held_no_flag

# Source equality with main
git diff HEAD -- src/applications/balancing_robot/balance_app.cpp \
                 src/applications/balancing_robot/balance_app.h \
                 tests/test_held_state_machine.cpp
# (no output — sources are clean against 7a4d27f)

# Source-of-truth for the HELD gate (line refs in current tree):
#   src/applications/balancing_robot/balance_app.cpp:437  #ifdef USE_BALANCE_HELD_DETECTION
#   src/applications/balancing_robot/balance_app.cpp:465  ext_motion = (cmd<20) && (|gyY|>30)
#   src/applications/balancing_robot/balance_app.cpp:469  dwell = ext_motion ? 20 : 60
#   src/applications/balancing_robot/balance_app.cpp:477  #endif

# All other balance test binaries pass — confirms the issue is isolated:
./tests/test_balance_app             # 28/28 pass
./tests/test_balance_app_bootstrap   # 27/27 pass
./tests/test_balance_app_collision   # 27/27 pass
./tests/test_balance_app_soft_cutoff # 13/13 pass
./tests/test_held_state_machine      # 3 pass, 3 FAIL  ← only this one
```
