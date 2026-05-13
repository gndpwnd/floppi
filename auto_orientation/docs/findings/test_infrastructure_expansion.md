# Test Infrastructure Expansion — auto_orientation

**Date**: 2026-05-12
**Scope**: Grow the existing 143+ Unity/native-test suite into a tiered framework covering HIL, multi-MCU CI, scenario replay, and offline calibration analysis.

## Recommendation Summary

- **Cheap wins first**: `tools/build_matrix.sh` and `tests/scenario_test_balancing.cpp` cost <1 day each and immediately guard against regressions during auto-PID-tune and BNO055/BNO085 abstraction work.
- **Skip dedicated HIL hardware for now**; replay-over-serial via `tools/replay_trajectory.py` delivers 80% of the value with 0% of the FT232H bill of materials.
- **Codify a pre-merge checklist** (unit + scenario + compile-matrix mandatory; HIL/field optional) so contributors know what "green" means.

## 1. Test Taxonomy

| Tier | Mechanism | When run | What it catches | Maintainer |
| ---- | --------- | -------- | --------------- | ---------- |
| **Unit (native)** | PlatformIO `test`, Unity, host g++ | Every commit, pre-push hook | Math errors in `math/`, sensor mock contracts, off-by-ones | Module author |
| **Integration (host)** | `integration_test_*.cpp` + `simple_test_runner.cpp` | Per-PR CI | Interface drift between subsystems; broken JSON/CSV formatters | Entry-point owner |
| **Scenario replay** | `scenario_test_ekf.cpp` style — load CSV, assert tolerances | Per-PR CI | Behavioural regressions: "EKF still converges on the recorded replay" | Trajectory recorder |
| **HIL (proposed)** | Real MCU + host Python pushing synthetic I2C/serial; capture motor PWM | Nightly / release-gated | Driver bugs, ISR races, I2C clock-stretching | Hardware lead |
| **Field test** | Real robot, real GPS, real motors | Manual, before tagging | Mechanical/EMI/thermal issues unit tests miss | Project owner |
| **Compile matrix** | `pio run -e <env>` across every env | Every push (CI) | "Builds on Mega but breaks ESP32"; AVR-only headers leaking to ARM | Anyone; failures block merge |

Rows 1–3 are covered today. Compile matrix is the highest-leverage add: every env in `platformio.ini` (10+) compiles in CI for cents.

## 2. Balance-Robot Scenario Regression Test

**File**: `tests/scenario_test_balancing.cpp`. **Fixture**: `tests/data/balancing_reference_trajectory.csv` (synthetic; `t_ms,pitch_deg,pwm_ref`).

**Premise**: Lock down legacy `SelfBallancingRobot3.ino` behaviour before refactoring into `src/applications/balancing_robot/`. A *synthetic* trajectory (pitch step 0°→5°, 2 Hz sinusoid, NaN dropout) generated from the legacy PID gains is enough to catch architecture-level regressions until we have a real bench recording.

**Assertion contract**: feed new PID the same `pitch_deg` series with `PITCH_OFFSET=-8.6°` and legacy gains (`Kp=65, Ki=12, Kd=38`, dt=5 ms, deadband=15); require output within **±5 PWM units** of `pwm_ref` at every sample. Tolerance absorbs float-order differences but flags algorithmic divergence. After auto-tune ships, regenerate the CSV and version-bump — the test asserts structural equivalence, not frozen PID values.

Skeleton (Unity, native env):

```cpp
// tests/scenario_test_balancing.cpp
#include <unity.h>
#include "applications/balancing_robot/pid_controller.h"
#include "test_helpers.h"  // CSV loader

void test_legacy_pid_matches_recorded_trajectory() {
    auto samples = LoadCsv("tests/data/balancing_reference_trajectory.csv");
    BalancePid pid(65.0f, 12.0f, 38.0f, /*dt_ms=*/5, -255, 255, /*deadband=*/15);
    for (const auto& s : samples) {
        int16_t pwm = pid.step(s.pitch_deg - kPitchOffset);
        TEST_ASSERT_INT16_WITHIN(5, s.pwm_ref, pwm);
    }
}
void setUp() {} void tearDown() {}
int main() { UNITY_BEGIN(); RUN_TEST(test_legacy_pid_matches_recorded_trajectory); return UNITY_END(); }
```

CSV loader belongs in `test_helpers.h` (already `DEBUG_MODE`-scoped).

## 3. Multi-MCU Compile Matrix

**Local** (`tools/build_matrix.sh`): parse `platformio.ini` for `[env:*]`, run `pio run -e <env>` for each, capture stdout to `tests/results/<env>.log`, emit a Markdown table with flash/RAM percentages parsed from PlatformIO's size line. Mirror `flight_controller/tools/complexity_calculator.py` reporting style.

**CI** (`.github/workflows/build-matrix.yml`): one job with `strategy.matrix.env` populated from the same parse. Each cell runs `pio run -e ${{ matrix.env }}`, uploads logs, prints a size summary; an aggregation job posts a PR comment with flash deltas vs `main` (critical when Nano's 32 KB is the floor).

Catches: AVR `<cmath>` quirks, ESP32 macros leaking to Teensy, missing `#ifdef GPS_ENABLE` guards, RAM blowups from new lookup tables.

## 4. HIL Harness — Recommendation: Defer

Conceptually: FT232H acts as I2C *master* impersonating a BNO055, firmware runs on real Mega/Teensy, host Python pushes synthetic quaternions and reads motor PWM (via FT232H GPIO or PWM-to-serial in firmware). End state: drive the auto-tuner with reproducible pitch inputs and assert converged gains land in a stability envelope.

**Cost**: ~$15 FT232H, 1–2 weeks integration, fragile single-point-of-failure rig. **Value**: catches ISR/timing bugs unit tests miss — but a 10-min bench test catches the same.

**Verdict**: defer until MCU-only regressions show up. Achieve the same coverage by feeding CSV trajectories over serial to a firmware build with `#ifdef HIL_MODE` swapping the IMU read path for a serial-fed mock. One flag, no extra hardware, runs anywhere.

## 5. Tooling Extensions

| Tool | Purpose |
| ---- | ------- |
| `tools/replay_trajectory.py` | Stream `(t, pitch_deg)` CSV to firmware over serial; record returned PWM. Drives the HIL-lite flow. |
| `tools/auto_calibrate.py` | Host-side magnetometer ellipsoid fit; writes BNO055 22-byte offset blob and BNO085 SH-2 calibration. |
| `tools/quaternion_viewer.py` | pyqtgraph 3D quaternion viewer; consumes `name@plotId:value` telemetry. Bridges until WiFi dashboard ships. |
| `tools/balance_tune_visualizer.py` | matplotlib plot of live auto-PID-tune convergence: gains, cost, settling time. |
| `tools/build_matrix.sh` | Wrap `pio run -e <env>` across every env; emit a flash/RAM Markdown summary. |

All five are thin Python; pytest covers the parsing/math helpers inside them so the tools themselves don't silently rot.

## 6. Test Data Management

Create `tests/data/`. **Commit**: CSVs <1 MB — synthetic trajectories, golden EKF outputs, NMEA snippets, IMU calibration blobs (versioned fixtures, code-reviewed). **Ignore**: large logs/SD dumps under `tests/data/large/` with a checksummed manifest pointing to S3/local cache. **`.gitattributes`**: `*.bin filter=lfs -text` and `*.csv text eol=lf` to stop CRLF rewrites. Naming: `<source>_<scenario>_<date>.csv`; provenance README per fixture.

## 7. Coverage Philosophy

Coverage is a target *distribution*, not a number. **`math/`**: 100% line + 95% branch — pure deterministic functions, the Unity suite is already close. **`sensors/`**: ~70% line with mocks; the remaining 30% is hardware-only (real I2C ACK timing, SH-2 reconnect) — cover via HIL/field, don't mock. **`navigation/` and `applications/`**: scenario-driven, not line-driven — track scenario count, not %. Trade-off: chasing 100% on drivers breeds tautological mocks that lock in implementation details; the brittle-test cost beats the bug-catch value.

## 8. Pre-Merge Checklist

Add `docs/PRE_MERGE_CHECKLIST.md` with this shape:

- [ ] `pio test -e arduino_mega` passes (143+ unit + integration).
- [ ] `tools/build_matrix.sh` reports green across all envs in `platformio.ini`.
- [ ] All scenario tests in `tests/scenario_test_*.cpp` pass.
- [ ] Flash/RAM delta vs `main` is documented in the PR description for any env that changed >2%.
- [ ] If the PR touches `applications/balancing_robot/`, scenario regression is updated or its tolerance bump is justified.
- [ ] HIL run attached (optional; required only for sensor-driver PRs).
- [ ] Field test logbook entry attached (optional; required only for release tags).

Wire this into a PR template so reviewers can tick boxes instead of paging through `docs/testing/`.

## File References

- `/home/devel/floppi/auto_orientation/tests/scenario_test_ekf.cpp` — template for the balancing scenario test.
- `/home/devel/floppi/auto_orientation/tests/test_helpers.h` — host the CSV loader here.
- `/home/devel/floppi/auto_orientation/docs/archive/balancing_robot_reference/DISSECTION_NOTES.md` — legacy PID constants.
- `/home/devel/floppi/auto_orientation/platformio.ini` — env source for the compile matrix.
- `/home/devel/floppi/flight_controller/tools/complexity_calculator.py` — reporting style to mirror.
