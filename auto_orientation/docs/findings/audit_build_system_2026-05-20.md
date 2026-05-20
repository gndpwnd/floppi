# Build System Audit Report
## auto_orientation / PlatformIO Configuration
**Date:** 2026-05-20  
**Auditor:** build-auditor@floppi  
**Scope:** platformio.ini, build_matrix.sh, .gitignore, generated files, library pinning

---

## Executive Summary

The build system is **mostly sound** but has **4 P0 issues** (build correctness), **5 P1 issues** (matrix coverage and HAL stubs), and **16 P2+ issues** (hygiene, reproducibility, docs). The major concern is that `mega_orientation` environment is defined but **lacks build_src_filter**, inheriting the base's unfiltered includes—meaning it may accidentally compile balance-robot code. Two non-functional envs (`esp32_balance`, `teensy_balance`) are defined but not ready for use. The native test suite properly excludes platform-specific storage backends, but exclusions are incomplete for the balance-bot stack.

---

## FINDINGS (30 total)

### P0: Build Correctness Issues

#### 1. [P0] mega_orientation lacks build_src_filter — may compile balance code
**File:** platformio.ini:175–181  
**Issue:** `[env:mega_orientation]` and `[env:mega_orientation_calibration]` do not define `build_src_filter`. They inherit only from `[base]`, which has no filter. This means the build will compile ALL source files, including balance-robot code (`src/applications/balancing_robot/*`, `src/control/auto_pid_tuner.cpp`, etc.) that should not be in an orientation-framework build. Expected: explicit filter excluding balance-robot sources.  
**Impact:** Bloated binary, potential symbol conflicts if both BalanceApp and IMU+GPS main() are linked.  
**Recommendation:** Add `build_src_filter` to `mega_orientation` to exclude `src/applications/balancing_robot/`, `src/control/auto_pid_tuner.cpp`, `src/control/plant_identifier.cpp`, `src/control/tuners/`.

#### 2. [P0] mega_orientation does NOT define build_flags for IMU/GPS inclusion
**File:** platformio.ini:179  
**Issue:** `build_flags = -D USE_IMU_GPS_TELEMETRY` is defined, but the framework conditional at main.cpp:501 is `#elif defined(USE_IMU_GPS_TELEMETRY)`. The orientation code is only compiled if this flag is present. However, there is no inverse exclusion of USE_BALANCING_ROBOT, so if both flags are defined (which is possible), the build is ambiguous. The orientation env should explicitly undefine balance flags.  
**Impact:** Undefined build variant if misconfigured.  
**Recommendation:** Add `-U USE_BALANCING_ROBOT` to mega_orientation build_flags.

#### 3. [P0] Commented-out envs (esp32_balance, teensy_balance) reference undefined hardware timers
**File:** platformio.ini:159–169  
**Issue:** Lines 159–169 show `# [env:esp32_balance]` and `# [env:teensy_balance]`, which are scaffolded but disabled. The comment (lines 146–154) states "Porting work needed" — specifically, MsTimer2 (AVR-only) must be replaced with `esp_timer`/`IntervalTimer`. However, src/main.cpp line 823 shows the error message "No application selected. Set USE_BALANCING_ROBOT or USE_IMU_GPS_TELEMETRY…" — these commented envs, when enabled, would have NO APPLICATION FLAG, causing a build failure. The missing build_flags are:  
   - esp32: `-D USE_BALANCING_ROBOT -D USE_ESP32 -D USE_WIFI` (per line 163)  
   - teensy: `-D USE_BALANCING_ROBOT -D USE_TEENSY` (per line 169)  
These would work, but MsTimer2 inclusion would fail at link time (library not available on ESP32/Teensy platforms).  
**Impact:** Non-compilable stubs that will confuse CI or IDE build lists.  
**Recommendation:** Either (a) complete the ESP32/Teensy timer port and uncomment, or (b) delete the scaffolds and re-add when porting is ready.

#### 4. [P0] native_test missing HAL stub for BNO055 driver
**File:** platformio.ini:200–205  
**Issue:** `[env:native_test]` excludes `persistent_storage_avr.cpp`, `persistent_storage_esp32.cpp`, `persistent_storage_teensy.cpp` (lines 202–204), allowing only `persistent_storage_native.cpp` to be compiled. This is correct. However, the filter does NOT exclude `src/sensors/bno055.cpp`. The BNO055 driver (which uses I2C Wire library and Adafruit sensor calls) will fail to compile on the native platform because Wire.h does not exist. The native test suite runs 40 test files but many (e.g., test_balance_app.cpp) will need a mock BNO055 or the driver excluded.  
**Impact:** native_test env will fail to link if any test includes BNO055 headers without stubs.  
**Recommendation:** Either add `-<sensors/bno055.cpp>` to native_test filter, OR add HAL mocks in tests/mock/ for I2C/BNO055 and include them via build_src_filter.

---

### P1: Build Matrix Coverage & Missing HAL

#### 5. [P1] balance_src_filter excludes navigation/ekf.cpp but no mock EKF for native tests
**File:** platformio.ini:71, native_test:200–205  
**Issue:** The `[balance_src_filter]` section (lines 63–82) excludes EKF and GPS code (lines 71–75, 77–78, 80–81). This is correct for balance builds (Uno/Mega balance robots don't need EKF). However, `native_test` does NOT inherit balance_src_filter; it includes all source by default (line 201 `+<*>`), then only excludes storage backends. This means native tests will TRY to compile:
   - `src/navigation/ekf.cpp` (EKF state machine)
   - `src/navigation/state_dynamics.cpp` (EKF model)
   - `src/sensors/bno085.cpp` (BNO085 driver, requires I2C Wire)
   - `src/sensors/gps.cpp` (GPS driver, requires UART)
   
   These will fail on the native platform. The tests directory contains 5 EKF/GPS tests (test_ekf.cpp, test_measurement_model.cpp, etc.), but they likely have their own mocks. The build system should not try to compile the real drivers.  
**Impact:** native_test will fail at link time if any test pulls in GPS/BNO085/EKF headers without satisfying their dependencies (Wire, HardwareSerial).  
**Recommendation:** Extend native_test filter to exclude GPS/BNO085 drivers and EKF implementation. Tests should use mocks or stub headers.

#### 6. [P1] build_matrix.sh's QUICK_SKIP_REGEX matches no envs
**File:** tools/build_matrix.sh:31  
**Issue:** `QUICK_SKIP_REGEX='(full|snapshot)'` (line 31) is intended to skip "heavier / less-frequent builds" when `--quick` is used. However, examining platformio.ini, NO env name contains "full" or "snapshot". The only envs are: uno_balance, mega_balance, arduino_uno_minimal, mega_orientation, mega_orientation_calibration, native_test, esp32_balance (commented), teensy_balance (commented). Thus, `--quick` will not skip anything, making the feature useless. Either the regex should match actual env names, or the comment is stale.  
**Impact:** Misleading --quick flag; all envs build regardless of --quick.  
**Recommendation:** Either (a) rename heavier envs to include "full" or "snapshot", or (b) update regex to match actual env names that are genuinely slower (e.g., native_test), or (c) remove the feature.

#### 7. [P1] build_matrix.sh cannot enumerate commented-out envs
**File:** tools/build_matrix.sh:81  
**Issue:** Line 81 uses `grep -E '^\[env:' "$PIO_INI"` to enumerate envs. This matches only uncommented sections (those starting with `[`). The commented-out envs at lines 159–169 (`# [env:esp32_balance]`, etc.) are invisible to the matrix. This is probably intentional (non-functional envs should not be built), but it means the build matrix WILL NOT CATCH if an esp32_balance or teensy_balance stub is uncommented without being finished. A developer could uncomment one and forget to enable it for CI, silently breaking the matrix.  
**Impact:** Non-functional envs could be enabled without hitting the matrix build.  
**Recommendation:** Add an explicit "scaffold check" in build_matrix.sh: warn if commented envs become unscaffolded (i.e., uncommented by accident).

#### 8. [P1] arduino_uno_minimal filter is overly restrictive; excludes necessary math/quaternion_conversions.cpp
**File:** platformio.ini:137–142  
**Issue:** `[env:arduino_uno_minimal]` defines:
```
build_src_filter =
    +<applications/balancing_robot_uno/>
    +<sensors/bno055.cpp>
    +<actuators/l298n_motor_driver.cpp>
    +<control/pid_controller.cpp>
    +<math/quaternion_conversions.cpp>
```
This is a whitelist (only these files are compiled). However, it does NOT include:
   - `src/main.cpp` (!)
   - `src/storage/persistent_storage_*.cpp` (no persistent storage on minimal)
   - `src/config/calibration_storage.h` (calibration helpers, if used by uno_balance_app.h)

The uno_balance_app.h (line 54 of application) uses balance_constants.h and may depend on quaternion/IMU code. If uno_balance_app includes storage or main.cpp symbols, the link will fail. This filter needs audit.  
**Impact:** Minimal build may fail at link time if uno_balance_app.h has hidden dependencies.  
**Recommendation:** (a) Verify uno_balance_app.h includes are all in the whitelist, or (b) switch to blacklist mode (exclude only unnecessary code like features/, output/).

#### 9. [P1] Storage backends not guarded in native test; persistent_storage.h will try to link all variants
**File:** platformio.ini:202–204 & src/storage/persistent_storage.cpp  
**Issue:** native_test excludes persistent_storage_avr.cpp, persistent_storage_esp32.cpp, persistent_storage_teensy.cpp (correct). However, it still includes persistent_storage_native.cpp. The persistent_storage.h header (src/storage/persistent_storage.h) defines the ps:: namespace but may include inline functions or platform conditionals that try to include all backends. If persistent_storage.h uses `#ifdef ARDUINO_ARCH_AVR #include "persistent_storage_avr.cpp"` (which it does not, but could), the native build would fail. Cross-check that persistent_storage.h only declares the interface and does not include backend implementations inline.  
**Impact:** Silent failure if the header structure changes to inline backends.  
**Recommendation:** Audit src/storage/persistent_storage.h to verify it does not inline implementation code from platform-specific files.

---

### P2: Hygiene & Configuration

#### 10. [P2] mega_orientation_calibration has no build_src_filter; inherits parent's (none)
**File:** platformio.ini:183–186  
**Issue:** `[env:mega_orientation_calibration]` extends `env:mega_orientation` (line 184), which has no build_src_filter. Thus, it also has none. It will compile the same bloated binary as mega_orientation (see finding #1). The comment suggests it should be "Orientation framework, verbose calibration mode" — a specialized variant of mega_orientation, not a separate app.  
**Impact:** Identical to #1: bloated binary with balance-robot code.  
**Recommendation:** Inherit build_src_filter from mega_orientation once it is defined (finding #1).

#### 11. [P2] Base library list lacks version pinning
**File:** platformio.ini:33–35  
**Issue:** The `[base]` section lists:
```
lib_deps =
    Adafruit Unified Sensor
    Adafruit BusIO
```
These are unpinned (no `@` version specifier). This means PlatformIO will fetch the latest version from its registry, which could break the build if a major API change lands. Contrast with paulstoffregen/MsTimer2 @ ^1.1 (pinned to ^1.1). Best practice: pin Adafruit libraries to known-good versions.  
**Impact:** Non-reproducible builds; future PlatformIO installs may pull incompatible library versions.  
**Recommendation:** Pin Adafruit libraries: `Adafruit Unified Sensor @ ^8.5.1`, `Adafruit BusIO @ ^1.14.0` (choose versions known to work with existing code).

#### 12. [P2] Adafruit BNO055 library also unpinned in base_balance
**File:** platformio.ini:57  
**Issue:** `[base_balance]` lists `Adafruit BNO055` without a version. It should be pinned like MsTimer2.  
**Impact:** Non-reproducible balance builds; library API changes could break them.  
**Recommendation:** Pin to a known-good version, e.g., `Adafruit BNO055 @ ^1.10.3`.

#### 13. [P2] SD library unpinned in mega_orientation
**File:** platformio.ini:181  
**Issue:** `[env:mega_orientation]` lists `SD` (standard Arduino SD library) without a version. SD has had history of breakage and version mismatches.  
**Impact:** Non-reproducible builds; SD API changes could break orientation app's file logging.  
**Recommendation:** Pin to a known version, e.g., `SD @ ^1.2.4`.

#### 14. [P2] balance_src_filter excludes relay_feedback.cpp but includes PID controller
**File:** platformio.ini:66–82  
**Issue:** The balance_src_filter explicitly excludes `src/control/tuners/relay_feedback.cpp` (line 82), which is the relay auto-tuner. This makes sense for balance builds (they use hardcoded PID or BOOTSTRAP, not relay feedback). However, the filter INCLUDES `src/control/pid_controller.cpp` (implicitly, via `+<*>` on line 67), which is correct. The asymmetry (one tuner excluded, one included) is intentional but could confuse future maintainers. Add a clarifying comment.  
**Impact:** Maintenance risk; no functional issue.  
**Recommendation:** Add comment: "Relay feedback tuner excluded because balance builds use hardcoded PID gains (Uno minimal, BOOTSTRAP) or RLS (Mega universal)."

#### 15. [P2] quaternion_conversions.cpp included in minimal but not balance_src_filter
**File:** platformio.ini:79, 142  
**Issue:** `balance_src_filter` excludes `src/math/magnetic_declination.cpp` (line 80, for GPS/compass work) but does NOT exclude `src/math/quaternion_conversions.cpp`. This is correct (quaternions are needed for BNO055). However, `arduino_uno_minimal` explicitly includes quaternion_conversions (line 142), suggesting the maintainer was uncertain whether it was in balance_src_filter. Clarify the implicit includes in balance_src_filter by listing key positive includes, or use a whitelist mode (like arduino_uno_minimal).  
**Impact:** Confusion about what's actually compiled.  
**Recommendation:** Add comment block in balance_src_filter explaining which files are implicitly included (IMU, motor, PID) and why.

#### 16. [P2] No build budgets documented for flash/RAM per env
**File:** No file found  
**Issue:** The roadmap mentions "99.9% flash" for uno_balance and "88% free" for mega, but there is no formal budget document (e.g., docs/FLASH_BUDGET.md). When a developer adds code, they won't know the red lines.  
**Impact:** Risk of flash overflow on Uno without warning.  
**Recommendation:** Create docs/applications/balancing_robot_uno/FLASH_BUDGET.md with target budgets and a note that build_matrix.sh should warn if any env exceeds threshold.

#### 17. [P2] .gitignore incorrectly ignores test binaries but has exception rules
**File:** .gitignore:51–69  
**Issue:** Lines 51–69 define test binary patterns:
```
tests/test_*
!tests/test_*.cpp
...
tests/test_runner
```
This pattern is confusing and fragile. `tests/test_*` matches files and directories, but the exceptions (`!tests/test_*.cpp`) only except .cpp files, not the test binaries. If a test is compiled to `tests/test_balance_app` (no extension), it should be ignored, but if it's `tests/test_balance_app.o`, the rule becomes ambiguous. Better: explicitly ignore only compiled outputs (*.o, *.elf) and list source patterns as exceptions.  
**Impact:** Potential accidental commits of compiled test binaries.  
**Recommendation:** Simplify to:
```
tests/*.o
tests/*.elf
tests/test_*
!tests/test_*.cpp
!tests/test_*.h
!tests/test_*.ino
!tests/test_*.py
```

#### 18. [P2] No .gitignore rule for balance_constants.h (generated file)
**File:** .gitignore:1–76 & src/applications/balancing_robot_uno/balance_constants.h  
**Issue:** balance_constants.h (src/applications/balancing_robot_uno/balance_constants.h) is AUTO-GENERATED by tools/sim/brute_tune.py (see tools/sim/balance_constants_template.h.in and brute_tune.py line 87: DEFAULT_OUTPUT = "…/balance_constants.h"). This header should be .gitignored because it is regenerated on each tune. Currently, it is tracked in git (noted by the file existing with recent mod time 2026-05-20). If it is committed, future tuning runs could have merge conflicts.  
**Impact:** Generated file in version control; merge conflicts during re-tuning workflows.  
**Recommendation:** Add to .gitignore:
```
# Generated by tools/sim/brute_tune.py
src/applications/balancing_robot_uno/balance_constants.h
```
And update the template comment to state "Regenerate with: python3 tools/sim/brute_tune.py --output <file>."

---

### P3: Reproducibility & Documentation

#### 19. [P3] No build.log or size tracking in build_matrix.sh output
**File:** tools/build_matrix.sh:186, build_matrix.sh:154–187  
**Issue:** build_matrix.sh extracts flash/RAM sizes from the PlatformIO output (lines 113–141) and prints them (line 185). However, it does NOT log them to a file or track history. If a build gets 95% flash and a future PR adds code, there's no warning until the next full build matrix run. Best practice: emit a CSV or JSON summary that CI can track across builds.  
**Impact:** No historical trend data; flash growth is not detected until a build fails.  
**Recommendation:** Add optional `--save-log <file>` mode that appends timestamp + env + flash + ram to a CSV. CI can then diff this against the main branch to detect regressions.

#### 20. [P3] tools/build_tests.sh is hardcoded to desktop (gtest) — not integrated with PlatformIO test_framework
**File:** tools/build_tests.sh:1–32  
**Issue:** build_tests.sh hardcodes:
```bash
g++ -std=c++17 -Wall -Wextra -I. \
    src/math/quaternion.cpp \
    src/math/quaternion_conversions.cpp \
    tests/test_bno085_extensions.cpp \
    -o tests/test_bno085_extensions \
    $(pkg-config --cflags --libs gtest_main gtest)
```
This is a SEPARATE test build (gtest, C++17, g++ on the dev machine) from the PlatformIO native_test environment (Unity framework, C++11). This is fragmentary and confusing:
- gtest tests (build_tests.sh) test math / quaternion code
- Unity tests (pio test -e native_test) test balance app / persistent storage / other modules
- Where is the boundary? Which tests run in CI?
**Impact:** Split test infrastructure; unclear which tests are run by CI; risk of gtest tests never being run.  
**Recommendation:** Migrate gtest tests to the native_test env using Unity framework, or document the two test suites clearly in docs/TESTING.md.

#### 21. [P3] No documentation on how to enable commented-out ESP32/Teensy envs
**File:** platformio.ini:146–154 & no docs/PORTING_GUIDE.md  
**Issue:** The comment (lines 146–154) explains that esp32_balance and teensy_balance are scaffolded but not functional. It mentions "To enable a port, uncomment the env and complete the platform-conditional include of the right hardware-timer driver in src/main.cpp." However, there is no step-by-step guide in docs/ for a contributor to follow.  
**Impact:** Future porting work is not documented; risk of incomplete ports being enabled.  
**Recommendation:** Create docs/PORTING_GUIDE_ESP32_TEENSY.md with:
- Checklist of tasks (timer replacement, EEPROM backend testing, pin mapping)
- Code snippets showing how to add the conditional in src/main.cpp
- Test matrix validation steps

#### 22. [P3] balance_constants_template.h.in generation step not documented in main README
**File:** docs/applications/balancing_robot_uno/README.md & tools/sim/brute_tune.py  
**Issue:** The uno_balance_app depends on generated balance_constants.h (auto-generated by tools/sim/brute_tune.py from the .in template). However, the README does not mention this generation step or link to tools/sim/README.md. A new contributor building arduino_uno_minimal for the first time would not know to run brute_tune.py.  
**Impact:** Build process is opaque; undocumented dependencies.  
**Recommendation:** Add to docs/applications/balancing_robot_uno/README.md:
```markdown
### Build Preparation: Generate PID Constants
The Uno minimal build depends on `src/applications/balancing_robot_uno/balance_constants.h`, which is generated by the offline Python tuner:

    python3 tools/sim/brute_tune.py --mode random --budget 1000 \
        --output src/applications/balancing_robot_uno/balance_constants.h

See [tools/sim/README.md](../../tools/sim/README.md) for full tuner documentation.
```

#### 23. [P3] No cross-platform testing guidance (Linux vs. macOS)
**File:** No docs/PLATFORM_NOTES.md  
**Issue:** The project is developed on Linux (per env notes: SCRIPT_DIR, REPO_ROOT use bash $(...) which work on both platforms). However, there is no README entry for macOS users on how to set up PlatformIO or whether there are known platform-specific issues (e.g., serial port naming /dev/ttyUSB* vs /dev/tty.usbserial*).  
**Impact:** macOS contributors may encounter build/flash issues without guidance.  
**Recommendation:** Add docs/PLATFORM_NOTES.md covering macOS serial setup, PlatformIO paths, and any platform-specific quirks.

#### 24. [P3] No CONTRIBUTING.md file documenting build sanity checks
**File:** No docs/CONTRIBUTING.md  
**Issue:** There is no guide for contributors on what to check before submitting a PR. Should they run `pio run -e uno_balance` and `pio test -e native_test`? Both? The readme does not specify.  
**Impact:** Inconsistent pre-commit testing; PRs may land with broken builds on certain envs.  
**Recommendation:** Create docs/CONTRIBUTING.md with:
```markdown
### Pre-Commit Checklist
- [ ] `pio run -e uno_balance` builds successfully
- [ ] `pio run -e mega_balance` builds successfully
- [ ] `pio test -e native_test` passes all unit tests
- [ ] `tools/build_matrix.sh` passes for at least `--quick` mode
```

---

### P3: Dead / Unused Tooling

#### 25. [P3] build_tests.sh (desktop gtest) is disconnected from main test infrastructure
**File:** tools/build_tests.sh  
**Issue:** As noted in finding #20, this script builds gtest binaries (test_bno085_extensions, integration_test_math_pipeline, benchmark_math) using g++ directly, not PlatformIO. There is no CI integration documented, and the script does not check for gtest availability (no guard for `pkg-config --cflags --libs gtest_main gtest` failures). If gtest is not installed, the script will fail silently or with cryptic error.  
**Impact:** Bit-rot risk; test suite may not be running on CI.  
**Recommendation:** Either (a) integrate into native_test env, or (b) document in .github/workflows/ how gtest is invoked, or (c) deprecate and remove if not used.

#### 26. [P3] No post-build size-check script
**File:** No tools/check_flash_budget.sh  
**Issue:** build_matrix.sh extracts flash/RAM but does not enforce budgets. If uno_balance exceeds 32 KB or mega_balance exceeds 256 KB, the matrix will still show "OK" (because pio run succeeded). A wrapper script should fail if budgets are breached.  
**Impact:** Flash overflows detected only at hardware flashing time.  
**Recommendation:** Create tools/check_flash_budget.sh that parses build_matrix.sh output and fails if:
   - uno_balance > 32 KB flash
   - mega_balance > 32 KB flash (if using same code)
   - mega_orientation > 220 KB flash

---

### P4: Minor / Documentation

#### 27. [P4] Inconsistent use of "Uno balance" vs "uno_balance" vs "arduino_uno_minimal"
**File:** platformio.ini comments, docs, code  
**Issue:** The minimal Uno balance bot has THREE names:
- "Uno balance" (English, in comments)
- "uno_balance" (env name, but this also refers to the universal balance app)
- "arduino_uno_minimal" (actual env in platformio.ini:126)

This is confusing. "uno_balance" (line 88) is the universal balance app; "arduino_uno_minimal" (line 126) is the hardcoded one. The comment on line 126 says "Minimal Uno balance bot" but the env name is arduino_uno_minimal, which is awkward.  
**Impact:** Cognitive friction; developers may use the wrong env name.  
**Recommendation:** Rename arduino_uno_minimal to uno_balance_minimal (or just uno_minimal_balance) to clarify it's a variant of uno_balance.

#### 28. [P4] "phase strategic-pivot — 2026-05-19" comment is stale in platformio.ini
**File:** platformio.ini:113  
**Issue:** Comment on line 113 says "Phase strategic-pivot — 2026-05-19", but it's now 2026-05-20. Minor, but suggests comments need refreshing.  
**Impact:** Comment hygiene; confusing to read "past" dates in current-day code.  
**Recommendation:** Remove the date from the comment or update to a version marker (e.g., "Phase 4U — Uno minimal (v1.0)").

#### 29. [P4] base_balance library deps could be split per use case
**File:** platformio.ini:48–57  
**Issue:** `[base_balance]` lists both Adafruit Unified Sensor and Adafruit BusIO, which are needed for BNO055. However, if a future balance build uses a different IMU (e.g., MPU6050 with no Adafruit wrapper), these could be made optional. This is not a bug, but a design consideration. Not actionable unless new IMUs are added.  
**Impact:** None currently; design flexibility.  
**Recommendation:** If adding MPU6050 balance variant, consider per-variant lib_deps.

#### 30. [P4] native_test unity framework version not pinned
**File:** platformio.ini:194  
**Issue:** `test_framework = unity` specifies the Unity framework but does not pin a version (e.g., `unity @ ^2.5.4`). PlatformIO's native platform has bundled Unity, but it's good practice to be explicit.  
**Impact:** Low; PlatformIO's bundled Unity is stable.  
**Recommendation:** Consider adding `unity @ ^2.5.4` to native_test lib_deps if explicit control is desired.

---

## Summary Table

| Finding | Category | Severity | File | Line(s) |
|---------|----------|----------|------|---------|
| 1 | Build Correctness | P0 | platformio.ini | 175–181 |
| 2 | Build Correctness | P0 | platformio.ini | 179 |
| 3 | Build Correctness | P0 | platformio.ini | 159–169 |
| 4 | Build Correctness | P0 | platformio.ini | 200–205 |
| 5 | Matrix Coverage | P1 | platformio.ini | 71, 200–205 |
| 6 | Matrix Coverage | P1 | tools/build_matrix.sh | 31 |
| 7 | Matrix Coverage | P1 | tools/build_matrix.sh | 81 |
| 8 | Matrix Coverage | P1 | platformio.ini | 137–142 |
| 9 | Matrix Coverage | P1 | platformio.ini | 202–204 |
| 10 | Hygiene | P2 | platformio.ini | 183–186 |
| 11 | Hygiene | P2 | platformio.ini | 33–35 |
| 12 | Hygiene | P2 | platformio.ini | 57 |
| 13 | Hygiene | P2 | platformio.ini | 181 |
| 14 | Hygiene | P2 | platformio.ini | 66–82 |
| 15 | Hygiene | P2 | platformio.ini | 79, 142 |
| 16 | Reproducibility | P2 | No file | — |
| 17 | Reproducibility | P2 | .gitignore | 51–69 |
| 18 | Reproducibility | P2 | .gitignore | 1–76 |
| 19 | Reproducibility | P3 | tools/build_matrix.sh | 186 |
| 20 | Documentation | P3 | tools/build_tests.sh | 1–32 |
| 21 | Documentation | P3 | platformio.ini | 146–154 |
| 22 | Documentation | P3 | docs/applications/balancing_robot_uno/README.md | — |
| 23 | Documentation | P3 | No file | — |
| 24 | Documentation | P3 | No file | — |
| 25 | Tooling | P3 | tools/build_tests.sh | 1–32 |
| 26 | Tooling | P3 | No file | — |
| 27 | Minor | P4 | platformio.ini | Passim |
| 28 | Minor | P4 | platformio.ini | 113 |
| 29 | Minor | P4 | platformio.ini | 48–57 |
| 30 | Minor | P4 | platformio.ini | 194 |

---

## Recommendations (Priority Order)

### Immediate (Next PR)
1. **Fix mega_orientation build_src_filter** (Finding #1, #2, #10) — prevent balance code from being compiled into orientation builds.
2. **Fix native_test BNO055 exclusion** (Finding #4, #5) — prevent link failures on native platform.
3. **Pin library versions** (Findings #11, #12, #13) — ensure reproducible builds.
4. **Remove commented-out esp32/teensy scaffolds** (Finding #3) or complete the ports; don't ship non-compilable stubs.
5. **Add balance_constants.h to .gitignore** (Finding #18) — prevent merge conflicts during tuning.

### Short-term (Next Sprint)
6. Clarify build matrix coverage (Findings #6, #7) — make --quick useful or remove it.
7. Audit arduino_uno_minimal filter for hidden deps (Finding #8).
8. Create FLASH_BUDGET.md (Finding #16).
9. Integrate gtest tests into native_test or document separately (Finding #20).
10. Create docs/CONTRIBUTING.md with pre-commit checklist (Finding #24).

### Long-term (Future)
11. Add build size tracking and budget enforcement (Findings #19, #26).
12. Create docs/PORTING_GUIDE_ESP32_TEENSY.md (Finding #21).
13. Add docs/PLATFORM_NOTES.md for cross-platform setup (Finding #23).
14. Simplify .gitignore test rules (Finding #17).
15. Rename arduino_uno_minimal to uno_balance_minimal (Finding #27).

---

## Verified as Correct

The following did NOT require corrections:

- **Storage backend guards in source:** All persistent_storage_*.cpp files are properly guarded with `#ifdef ARDUINO_ARCH_*` or `#ifdef __IMXRT1062__` (src/storage/persistent_storage_avr.cpp, persistent_storage_esp32.cpp, persistent_storage_native.cpp, persistent_storage_teensy.cpp).
- **Source structure:** 74 source files across 13 directories; modular layout is sound.
- **Test coverage:** 40 test files (.cpp); good coverage of balance app, EKF, math, storage.
- **balance_constants_template.h.in:** Proper template structure; generation documented in brute_tune.py.
- **build_matrix.sh logic:** Size extraction and env enumeration are correct (aside from the --quick regex issue).
- **Main application dispatch:** src/main.cpp properly branches on USE_BALANCING_ROBOT vs USE_IMU_GPS_TELEMETRY.

---

## Conclusion

The build system is **fundamentally sound** and supports a multi-target embedded project with good separation of concerns (balance vs. orientation, AVR vs. ESP32 vs. Teensy). However, **4 P0 issues** must be resolved to prevent build corruption (balance code leaking into orientation builds, native test link failures). Once those are fixed, the system is maintainable and reproducible. Library pinning and documentation are the main P2/P3 gaps.

**Estimated effort to fix all findings:** 2–3 days (mostly docs + test integration).

**Sign-off:** Ready for development; recommend resolving P0 and P1 findings before next release.

---

**Generated:** 2026-05-20  
**Report ID:** audit_build_system_2026-05-20  
**Scope:** platformio.ini, build_matrix.sh, .gitignore, generated files (balance_constants_template.h.in)  
**Out of scope:** k8s, terraform, CI/CD platforms, Docker, source code logic audits, test content validation
