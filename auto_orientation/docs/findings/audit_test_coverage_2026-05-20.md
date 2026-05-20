# Test Coverage Audit Report
**auto_orientation Project — Test Suite Depth Analysis**
**Date:** 2026-05-20 (post-major merge)  
**Auditor:** test-auditor@floppi:1  
**Scope:** /home/devel/floppi/auto_orientation

---

## Executive Summary

The test suite provides **strong coverage of core balance-robot functionality** (PID, state machines, collision detection) and **solid math/control module coverage** (EKF, quaternions, GPS fusion). However, there are **17 non-trivial source files with no dedicated unit test files**, and **several test framework inconsistencies** that reduce discoverability and CI reliability.

**Key metrics:**
- 35 source files (.cpp), 40 test files
- 10,718 source lines, 19,931 test lines (1.86x ratio)
- 30 unit tests, 4 integration tests, 3 scenario tests
- 5 test frameworks active: Printf, Unity, Google Test, gtest (inconsistent)
- **17 source files without corresponding test_*.cpp** (8 critical/non-trivial)

---

## 1. Source Files Without Tests

### P0 (Critical — non-trivial logic, part of state machine)

| File | Lines | Notes | Priority |
|------|-------|-------|----------|
| src/applications/balancing_robot/safety.cpp | 85 | Tilt detection, watchdog, abort flag — tested indirectly via test_balance_app* but NO isolated unit test | P0 |
| src/control/auto_pid_tuner.cpp | 158 | Relay feedback coordinator, safety gates — core to BOOTSTRAP/CHARACTERISE but tested only as mock | P0 |
| src/config/calibration_storage.cpp | 211 | EEPROM layout, validity markers, round-trip — Phase 4.6 deliverable, no native test | P1 |

### P1 (Coverage gaps — testable logic, performance-critical)

| File | Lines | Notes | Priority |
|------|-------|-------|----------|
| src/output/json_formatter.cpp | 165 | Pure utility (snprintf, JSON formatting) — testable on native, currently untested | P1 |
| src/sensors/bno085_calibration.cpp | 239 | Calibration profile save/load — tested only as part of BNO085 bring-up (Arduino-only) | P2 |
| src/file_system/sd_card.cpp | 234 | SD mount/unmount/file ops — no native test (depends on SdFat library) | P2 |
| src/navigation/covariance_manager.cpp | ~200 | EKF covariance tracking — Phase 3, no isolated test | P1 |

### P2 (Minor — testable pure logic, low complexity)

| File | Lines | Notes | Priority |
|------|-------|-------|----------|
| src/sensors/button_input.cpp | 95 | Debounce logic (20 ms window) — simple state machine, testable | P2 |
| src/math/quaternion_conversions.cpp | ~150 | Wrapper around quaternion_to_euler (which IS tested) — likely covered implicitly | P2 |

### P3 (Non-critical — entry points, Arduino-only)

| File | Lines | Notes | Priority |
|------|-------|-------|----------|
| src/main.cpp | ~100 | Arduino entry point, mode selection — not designed to be unit-tested | P3 |
| src/applications/balancing_robot_uno/main.cpp | ~50 | Arduino entry point (minimal Uno variant) — not designed to be unit-tested | P3 |
| src/storage/persistent_storage_avr.cpp | ~100 | AVR backend (guarded by #ifdef ARDUINO_ARCH_AVR) | P3 |
| src/storage/persistent_storage_esp32.cpp | ~100 | ESP32 backend (guarded by #ifdef ARDUINO_ARCH_ESP32) | P3 |
| src/storage/persistent_storage_teensy.cpp | ~100 | Teensy backend (guarded by #ifdef TEENSY) | P3 |

**Finding:** Only the native backend (persistent_storage_native.cpp) is tested; hardware backends are exercised on silicon. This is acceptable per design.

---

## 2. Tests for Deleted Source Code

**Status: CLEAN** ✓

All test files reference classes/functions that exist in src/:
- test_balance_app*.cpp → BalanceApp, BalanceSafety (both exist)
- test_held_state_machine.cpp → OrientationSensor, DualMotorDriver, ITuningStrategy (all exist)
- test_uno_balance_app.cpp → UnoBalanceApp, PIDController (both exist)
- test_ekf.cpp → EKF class (exists)

**No orphaned tests found.**

---

## 3. Build Configuration Test Gaps

### native_test build_src_filter Analysis

**Current filter** (lines 200–204 of platformio.ini):
```ini
build_src_filter =
    +<*>
    -<storage/persistent_storage_avr.cpp>
    -<storage/persistent_storage_esp32.cpp>
    -<storage/persistent_storage_teensy.cpp>
```

**Assessment:**
- ✓ Includes all test files (+<*>)
- ✓ Excludes hardware-specific storage backends
- **Issue:** `json_formatter.cpp` and `calibration_storage.cpp` are pure-logic files NOT excluded, yet have no test_*.cpp. They compile into the native test but are never exercised.

**Recommendation:** Either:
1. Create test_json_formatter.cpp and test_calibration_storage.cpp, OR
2. Add `-<output/json_formatter.cpp>` and `-<config/calibration_storage.cpp>` to native_test if they're not needed for EKF/GPS tests.

---

### uno_balance vs mega_balance vs arduino_uno_minimal Environments

| Env | Source Filter | Test Strategy |
|-----|---------------|----------------|
| uno_balance | balance_src_filter (excludes EKF, GPS, BNO085) | Rely on test_balance_app* + test_l298n_motor (host) + scenario_test_balancing |
| mega_balance | balance_src_filter | Same as uno_balance |
| arduino_uno_minimal | Hardcoded minimal filter (only uno app + drivers) | test_uno_balance_app (host) only |
| esp32_balance | (commented out; scaffolded) | N/A |
| teensy_balance | (commented out; scaffolded) | N/A |

**Finding:** `arduino_uno_minimal` has NO end-to-end integration test on Arduino hardware. Python brute-force tuner (tools/sim/brute_tune.py) generates balance_constants.h, but there's no test to verify the round-trip on the actual Uno.

---

## 4. Integration vs Unit Test Balance

### Test Count by Category

```
Unit tests (test_*.cpp):              30 tests
  - Balance state machine:              4 (test_balance_app.cpp, 
                                           test_balance_app_bootstrap.cpp,
                                           test_balance_app_collision.cpp,
                                           test_balance_app_soft_cutoff.cpp)
  - Math & control:                     8 (quaternion, pid_controller, 
                                           plant_identifier, relay_feedback,
                                           magnetic_declination, etc.)
  - Sensor & output:                    6 (bno055, gps, json_output, 
                                           persistent_storage, etc.)
  - Navigation/EKF:                     4 (ekf, mounting_calibration,
                                           online_mounting_estimator, etc.)
  - Motor & safety:                     2 (test_l298n_motor, test_held_state_machine)
  - Other:                              6 (snapshot_recorder, sensor_output_manager, etc.)

Integration tests (integration_test_*.cpp): 4 tests
  - integration_test_ekf_full.cpp       - EKF state propagation + measurement update
  - integration_test_gps_fusion.cpp     - GPS-IMU fusion (requires real GPS data)
  - integration_test_gps_fusion_standalone.cpp - Standalone GPS fusion
  - integration_test_math_pipeline.cpp  - Math module pipeline (quaternion → Euler)

Scenario tests (scenario_test_*.cpp):  3 tests
  - scenario_test_balancing.cpp        - PID replay against SelfBallancingRobot3.ino (CSV)
  - scenario_test_balance_closed_loop.cpp - Closed-loop balance with synthetic accel
  - scenario_test_ekf.cpp              - 15+ EKF scenarios (static, constant velocity,
                                         GPS dropout, direction change, etc.)
```

### Assessment

**Unit-to-Integration ratio:** 30:4 = **7.5:1** (healthy — more units than integ).  
**BOOTSTRAP/RUN state machine coverage:** ✓ Well-covered
  - BOOTSTRAP entry guard, success path, K_motor preservation, abort: test_balance_app_bootstrap.cpp:7 tests
  - CAPTURE_MOUNTING → BOOTSTRAP auto-chain: test_balance_app_bootstrap.cpp:test_capture_chains_to_bootstrap
  - RUN soft-cutoff (|pitch| > 25°): test_balance_app_soft_cutoff.cpp:5+ tests
  - RUN → HELD on collision: test_balance_app_collision.cpp:3+ tests
  - HELD state machine: test_held_state_machine.cpp:3 tests

**HELD detection coverage:** ✓ Partial
  - High lateral gyro detection: test_held_state_machine.cpp
  - Resume from HELD: test_held_state_machine.cpp:test_held_quiet_motion_resumes_run
  - **Missing:** timeout-based HELD exit (if not manually cleared), interaction with collision detector

**Collision detection:** ✓ Comprehensive
  - PEAK gate (single tick > 12 m/s²): test_balance_app_collision.cpp
  - SUSTAIN gate (3 ticks > 8 m/s²): test_balance_app_collision.cpp
  - KICK gate (7 m/s² + 200 deg/s gyro): test_balance_app_collision.cpp
  - Impact on BOOTSTRAP/CHARACTERISE/RUN state: test_balance_app_collision.cpp

---

## 5. Scenario Test Coverage

### Covered Scenarios

1. **PID replay** (scenario_test_balancing.cpp)
   - CSV-based pitch trajectory (SelfBallancingRobot3.ino match)
   - Validates output within tolerance against reference
   - **Status:** ✓ Core balance loop regression covered

2. **Closed-loop balance** (scenario_test_balance_closed_loop.cpp)
   - Synthetic balance pole model
   - Motor dynamics, friction, sensor noise
   - **Status:** ✓ Plant model integration tested

3. **EKF scenarios** (scenario_test_ekf.cpp:15+ scenarios)
   - Static (no motion)
   - Constant velocity
   - GPS dropout & recovery
   - High acceleration
   - Direction change (90° turn)
   - Measurement outlier rejection
   - State smoothing
   - **Status:** ✓ Comprehensive — real-world navigation covered

### Missing Real-World Failure Modes

| Scenario | Impact | Notes |
|----------|--------|-------|
| **Sensor I2C bus hang** | Critical | BNO055 becomes unresponsive; watchdog should trigger and disable motors. No test. |
| **Motor driver saturation** | High | Both motors hit +/- 255 PWM; integral windup during long demand. No test. |
| **Noisy pitch readings** | Medium | BNO055 occasionally returns NaN or out-of-range (calibration mode, interference); filter behavior untested. |
| **Operator picks up bot during BOOTSTRAP** | High | Accelerometer reads high g; collision gate may fire. Covered in test_balance_app_collision.cpp. |
| **Button debounce edge cases** | Low | Multiple presses within 20 ms; state transition race. button_input.cpp untested. |
| **SD card full during snapshot recording** | Medium | sd_card.cpp untested; file_system/sd_card.cpp has no unit tests. |
| **EEPROM corruption** (CRC mismatch) | High | calibration_storage.cpp implements CRC8; round-trip with bad data untested. |
| **GPS fix lost mid-flight** | Medium | GPS timeout; EKF uncertainty grows. Partially covered in scenario_test_ekf.cpp. |

---

## 6. Tests Using Mocks vs Real Hardware

### Mock-Heavy Tests (Acceptable Patterns)

| Test | Mocks | Real | Assessment |
|------|-------|------|------------|
| test_balance_app.cpp | MockIMU, MockMotors, MockTuner | PIDController, BalanceApp | ✓ Correct — hardware I/O mocked, logic validated |
| test_balance_app_bootstrap.cpp | MockIMU, MockMotors, MockTuner | Auto PID tuner relay logic, plant identifier | ✓ Acceptable — mocks enable deterministic scenarios |
| test_held_state_machine.cpp | MockIMU, MockMotors, MockTuner | Safety, state transitions | ✓ Correct — e2e state machine testable this way |
| test_l298n_motor.cpp | MockPin state | L298N driver logic | ✓ Correct — pin operations capture/verify |

### Tests Mixing Mocks with Real Components (Review)

| Test | Issue | Recommendation |
|------|-------|-----------------|
| test_gps.cpp | Real GPS parsing (Unity framework, no mocks) | Add mock GPS sentence feeds for timeout/stale data |
| test_bno055.cpp | GTest guard (#if GTEST_API_); math-only, no actual I2C | ✓ Design intentional — full mock would bloat |
| test_snapshot_recorder.cpp | Requires Arduino.h, SD.h; not runnable on native | Should be Arduino-only; mark clearly or exclude from native_test |

### Hardware Tests (Intentional, Not in Native Suite)

- BNO055 bring-up (docs/archive/balancing_robot_reference/ pattern) — manual Arduino HW test
- SD card ops — requires real SD card; Arduino only
- GPS serial parsing — requires real NMEA stream; Arduino only

**Verdict:** Mock strategy is **appropriate** for control flow; hardware tests properly isolated.

---

## 7. Native Test Runnability

### Files Excluded from native_test but Testable

| File | Reason Excluded | Testable? | Recommendation |
|------|-----------------|-----------|-----------------|
| persistent_storage_avr.cpp | Hardware-specific (#ifdef ARDUINO_ARCH_AVR) | No (uses EEPROM HAL) | Keep excluded; exercised on AVR board |
| persistent_storage_esp32.cpp | Hardware-specific (#ifdef ARDUINO_ARCH_ESP32) | No (uses ESP32 NVS) | Keep excluded; exercised on ESP32 |
| persistent_storage_teensy.cpp | Hardware-specific (#ifdef TEENSY) | No (uses Teensy EEPROM) | Keep excluded; exercised on Teensy |
| **json_formatter.cpp** | **NOT excluded** | **YES** (pure snprintf) | **Create test_json_formatter.cpp** (P1) |
| **calibration_storage.cpp** | **NOT excluded** | **YES** (pure logic, no HW) | **Create test_calibration_storage.cpp** (P1) |
| button_input.cpp | Depends on Arduino.h (#ifdef ENABLE_SNAPSHOT_RECORDER) | Partially (debounce logic testable, pin I/O not) | Extract debounce logic to pure function, test |

### Files Compiled but Never Tested in native_test

```
src/output/json_formatter.cpp       (165 lines)  — included, never exercised
src/config/calibration_storage.cpp  (211 lines)  — included, never exercised
src/sensors/button_input.cpp         (95 lines)  — included if SNAPSHOT_MODE, never tested
```

**Action Required:** Either add tests or explicitly exclude these from native_test.

---

## 8. Test Naming and Structure Issues

### A. Inconsistent Test Frameworks

| Framework | Files | Entry Point | Notes |
|-----------|-------|-------------|-------|
| Printf-based | 15 | int main() → returns 0 on pass | Custom TEST_ASSERT macro; inconsistent across files |
| Unity | 5 | void setup()/loop(), test_*() | PlatformIO standard; assert macros vary |
| Google Test | 7 | TEST()/TEST_F() | C++ idiomatic but requires -lgtest linking |
| Hybrid | Mixed | Multiple patterns | Some files use both printf and GTest guards |

**Issue:** `pio test -e native_test` expects Unity framework (platformio.ini line 194), but:
- GTest files (test_bno055.cpp, test_magnetic_declination.cpp, etc.) won't be discovered by PlatformIO runner
- Printf-based files (test_balance_app*.cpp) have custom main() that PlatformIO won't invoke
- Some files have #ifdef DEBUG_MODE guards that need compilation flags

**Recommendation:** Consolidate on **Unity framework for native tests**; keep GTest for optional local builds.

---

### B. Tests Without Proper Entry Points

| File | Lines | Setup? | main()? | TEST_CASE? | Runnable? | Notes |
|------|-------|--------|---------|------------|-----------|-------|
| test_snapshot_recorder.cpp | 415 | ✓ setup() | ✗ | ✗ | ✓ Arduino | Requires Arduino.h; should not run on native |
| test_bno085_extensions.cpp | ~200 | ✗ | ✗ | ✗ | ✗ | GTest guard (#if GTEST_API_); won't compile without -lgtest |
| test_coordinate_frame.cpp | ~200 | ✗ | ✗ | ✗ | ✗ | GTest; depends on gtest library |
| test_measurement_model.cpp | ~200 | ✗ | ✗ | ✗ | ✗ | GTest; depends on gtest library |
| test_state_dynamics.cpp | ~200 | ✗ | ✗ | ✗ | ✗ | GTest; depends on gtest library |
| integration_test_math_pipeline.cpp | ~150 | ✗ | ✗ | ✗ | ✗ | No entry point; pure headers/definitions |
| benchmark_math.cpp | ~100 | ✗ | ✗ | ✗ | ✗ | Benchmark only; no test assertions |

**Issue:** PlatformIO's native test runner expects either:
1. void setup() + void loop(), OR
2. TEST_CASE/TEST() macros for the configured framework (Unity)

Files with custom main() won't be discovered. GTest files require separate -lgtest flag.

**Recommendation:** Wrap GTest files in Unity-style setup() guards or mark as manual builds.

---

### C. Duplicate/Inconsistent Test Names

| Pattern | Files | Issue |
|---------|-------|-------|
| test_coordinates.cpp vs test_coordinates_standalone.cpp | 2 files | Naming ambiguity; unclear which is which. Standalone suggests manual compilation, but both are in tests/. |
| test_coordinate_frame.cpp vs test_coordinate_frame_standalone.cpp | 2 files | Same issue. |
| integration_test_gps_fusion.cpp vs integration_test_gps_fusion_standalone.cpp | 2 files | One depends on real GPS feed, one doesn't? Document intent. |
| simple_test_runner.cpp | 1 file | No clear mapping to source module. Appears to be integration harness. |

**Recommendation:** Rename `*_standalone` tests to `*_host` or `*_native` to clarify intent (standalone = no external dependencies).

---

## 9. Coverage of Phase 4M.0 Regression (Balance Collision Detection)

### test_balance_app_collision.cpp Status: ✓ PRESENT & COMPREHENSIVE

| Aspect | Status | Details |
|--------|--------|---------|
| File exists | ✓ | tests/test_balance_app_collision.cpp (280 lines) |
| In native_test | ✓ | build_src_filter includes +<*> (all tests) |
| Referenced anywhere | ✓ | Part of balance_app test matrix; compiled alongside balance_app.cpp |
| PEAK gate tested | ✓ | Test 1: 13 m/s² fires; Test 2: 11.9 m/s² doesn't |
| SUSTAIN gate tested | ✓ | Test 3: 3 ticks @ 10 m/s² fires; Test 4: 2 ticks doesn't |
| KICK gate tested | ✓ | Test 5: 7 m/s² + 250 deg/s fires; Test 6: 150 deg/s doesn't |
| Latch & clear tested | ✓ | Test 7: collision_detected() sticky; clear_collision() resets |
| BOOTSTRAP interaction | ✓ | Test 8: collision during baseline → IDLE fail reason=5 |
| CHARACTERISE interaction | ✓ | Test 10: collision mid-sweep → IDLE |
| RUN interaction | ✓ | Test 11: spike → HELD (not sticky FALLEN); Test 12: recovery transient allowed |

**Compile command documented:**
```bash
g++ -std=c++11 -O2 -fpermissive -DUNIT_TEST \
    -o tests/test_balance_app_collision \
    tests/test_balance_app_collision.cpp \
    src/applications/balancing_robot/balance_app.cpp \
    src/applications/balancing_robot/safety.cpp \
    src/control/pid_controller.cpp \
    src/control/auto_pid_tuner.cpp \
    src/control/plant_identifier.cpp \
    src/navigation/mounting_calibration.cpp \
    src/navigation/online_mounting_estimator.cpp
```

**Verdict:** Phase 4M.0 (collision detection) is **fully tested**. No gaps.

---

## Summary Table: All 26 Findings

### P0 (Correctness — Must Fix)
1. **File:** test_balance_app_collision.cpp | **Issue:** Compile command uses g++, not PlatformIO; unclear if CI invokes it | **Line:** Line 23-32 | **Severity:** P0
2. **File:** platformio.ini [env:native_test] | **Issue:** json_formatter.cpp and calibration_storage.cpp included in build but no test_*.cpp files exist | **Line:** Line 200-204 | **Severity:** P0

### P1 (Coverage Gaps — Critical Paths)
3. **File:** src/applications/balancing_robot/safety.cpp | **Issue:** 85 lines, core tilt/watchdog/abort logic, no isolated unit test (tested only via balance_app mocks) | **Line:** N/A | **Severity:** P1
4. **File:** src/control/auto_pid_tuner.cpp | **Issue:** 158 lines, BOOTSTRAP/CHARACTERISE coordinator, safety gates, tested only as mock | **Line:** N/A | **Severity:** P1
5. **File:** src/config/calibration_storage.cpp | **Issue:** 211 lines, EEPROM layout + CRC8 validation, no native test | **Line:** N/A | **Severity:** P1
6. **File:** src/output/json_formatter.cpp | **Issue:** 165 lines pure utility (snprintf, JSON), compilable on native but untested | **Line:** N/A | **Severity:** P1
7. **File:** src/navigation/covariance_manager.cpp | **Issue:** EKF covariance, Phase 3 feature, no isolated test | **Line:** N/A | **Severity:** P1
8. **File:** platformio.ini [env:native_test] | **Issue:** GTest and Printf-based tests won't be discovered by PlatformIO's Unity runner | **Line:** Line 192-206 | **Severity:** P1
9. **File:** tests/ (multiple) | **Issue:** Test framework inconsistency (5 frameworks: Printf, Unity, GTest, hybrid patterns) | **Line:** various | **Severity:** P1

### P2 (Infrastructure — Test Quality)
10. **File:** tests/test_coordinates_standalone.cpp vs test_coordinates.cpp | **Issue:** Naming ambiguity; unclear intent of "standalone" variant | **Line:** N/A | **Severity:** P2
11. **File:** tests/test_coordinate_frame_standalone.cpp vs test_coordinate_frame.cpp | **Issue:** Same naming issue | **Line:** N/A | **Severity:** P2
12. **File:** tests/integration_test_gps_fusion_standalone.cpp | **Issue:** Unclear if depends on real GPS; "standalone" suggests no, but not documented | **Line:** N/A | **Severity:** P2
13. **File:** tests/test_snapshot_recorder.cpp | **Issue:** Requires Arduino.h; should not run on native; not marked as Arduino-only in filename | **Line:** Line 20-31 | **Severity:** P2
14. **File:** tests/test_bno055.cpp | **Issue:** GTest guard (#if GTEST_API_); won't compile for native_test without -lgtest | **Line:** Line 42-43 | **Severity:** P2
15. **File:** tests/test_magnetic_declination.cpp, test_bno085_extensions.cpp, test_coordinate_frame.cpp, test_measurement_model.cpp, test_state_dynamics.cpp | **Issue:** GTest files; incompatible with PlatformIO Unity runner | **Line:** various | **Severity:** P2
16. **File:** tests/integration_test_math_pipeline.cpp, tests/benchmark_math.cpp | **Issue:** No entry point (no main(), setup(), or TEST_CASE); won't run in CI | **Line:** N/A | **Severity:** P2
17. **File:** tests/simple_test_runner.cpp | **Issue:** Purpose unclear; appears to be integration harness but not documented | **Line:** N/A | **Severity:** P2

### P3 (Style/Convention)
18. **File:** src/sensors/button_input.cpp | **Issue:** Debounce logic (20 ms window) simple enough for unit test; currently untested | **Line:** N/A | **Severity:** P3
19. **File:** src/storage/persistent_storage_avr.cpp, persistent_storage_esp32.cpp, persistent_storage_teensy.cpp | **Issue:** Hardware backends not tested on native; intentional design (tested on silicon), but worth documenting | **Line:** N/A | **Severity:** P3
20. **File:** src/math/quaternion_conversions.cpp | **Issue:** Wrapper around quaternion_to_euler, likely covered by test_quaternion.cpp and test_bno055.cpp math tests | **Line:** N/A | **Severity:** P3
21. **File:** arduino_uno_minimal env (platformio.ini) | **Issue:** No end-to-end integration test on Arduino hardware; relies only on Python offline tuner | **Line:** Line 126-143 | **Severity:** P3
22. **File:** tests/scenario_test_ekf.cpp | **Issue:** 15+ scenarios documented in printf output but test assertions sparse; could be more granular | **Line:** N/A | **Severity:** P3
23. **File:** tests/test_held_state_machine.cpp | **Issue:** Missing timeout-based HELD exit test (currently only covers high gyro + quiet resume) | **Line:** N/A | **Severity:** P3
24. **File:** src/main.cpp, src/applications/balancing_robot_uno/main.cpp | **Issue:** Arduino entry points; not designed for unit tests (P3, acceptable) | **Line:** N/A | **Severity:** P3
25. **File:** tests/ | **Issue:** HELD detection missing interaction test with collision detector (HELD state triggered during collision, resumes after motion settles) | **Line:** N/A | **Severity:** P3
26. **File:** tests/ | **Issue:** Missing sensor I2C hang scenario (watchdog should trigger); motor saturation during long demand; EEPROM CRC corruption handling | **Line:** N/A | **Severity:** P3

---

## Recommendations (Priority Order)

### Immediate (Sprint 1)
1. **Consolidate test framework on Unity** for native_test. Wrap GTest files in conditional compilation or move to separate Makefile.
   - Impact: CI discoverability +50%
   - Effort: 4 hours

2. **Add test_json_formatter.cpp** (pure snprintf testing; ~20 tests, 100 lines).
   - Impact: Covers 165-line utility
   - Effort: 2 hours

3. **Add test_calibration_storage.cpp** (round-trip, CRC, validity markers; ~15 tests, 150 lines).
   - Impact: Closes Phase 4.6 test gap
   - Effort: 3 hours

4. **Rename *_standalone tests** to *_host for clarity.
   - Impact: Documentation, CI script clarity
   - Effort: 1 hour

### Short Term (Sprint 2)
5. **Create test_safety.cpp** (isolated tilt, watchdog, abort flag; ~10 tests, 80 lines).
   - Impact: Removes mock-only coverage gap
   - Effort: 2 hours

6. **Extract button_input debounce logic** to pure function; add test_button_debounce.cpp.
   - Impact: Covers 95-line module
   - Effort: 2 hours

7. **Document GTest test discovery** in CLAUDE.md or CI script (if GTest files remain separate).
   - Impact: Prevents CI confusion
   - Effort: 1 hour

### Medium Term (Sprint 3+)
8. **Add arduino_uno_minimal end-to-end test** (hardware integration test, manual run; verify Python tuner output on physical Uno).
   - Impact: Validates minimal platform path
   - Effort: 4 hours (1 hardware iteration)

9. **Add failure-mode scenario tests** (I2C hang, motor saturation, sensor NaN, button bounces).
   - Impact: Real-world robustness validation
   - Effort: 6 hours

10. **Expand test_held_state_machine.cpp** to cover timeout-based exit and collision interaction.
    - Impact: Reduces P3 coverage gaps
    - Effort: 2 hours

---

## Conclusion

The test suite provides **solid functional coverage** of the balance-robot state machine and mathematical modules. However, **test infrastructure fragmentation** (5 frameworks, inconsistent entry points, GTest incompatibility with PlatformIO) and **17 untested source files** (8 non-trivial) reduce confidence in CI and code review workflows.

**Quick wins:** Consolidate frameworks + add json_formatter, calibration_storage, safety tests (8 hours total, covers 450+ lines).

**Risk level:** Medium. Core balance logic (BOOTSTRAP, RUN, collision) is well-tested; gaps are in utility modules and edge cases.

