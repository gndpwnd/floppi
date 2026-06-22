#!/bin/bash
# ============================================================================
# build_tests.sh — build every active native test binary under tests/.
# ============================================================================
#
# Run:
#   bash tools/build_tests.sh
#
# What it does:
#   - Removes previously-built test executables under tests/ (source files
#     are left untouched — only binaries are wiped).
#   - For every test binary, invokes g++ with the canonical compile line
#     copied verbatim from the test's own docblock (see each test_*.cpp's
#     header comment).
#   - Per-test STATUS: prints PASS / FAIL with build output preserved.
#   - Returns non-zero if any build fails.
#
# Why this script exists:
#   investigation_held_state_machine_failure_2026-05-19.md root-caused a
#   stale binary (test_held_state_machine compiled without
#   -DUSE_BALANCE_HELD_DETECTION) silently producing 3 fail / 3 pass. The
#   cure is to (a) codify every per-test compile line here, and (b) keep
#   the script idempotent so a fresh run never sees yesterday's binary.
#   audit_build_system_2026-05-20.md §17–§18 motivates the .gitignore
#   companion edit (INFRA-B).
#
# Tests requiring -DUSE_BALANCE_HELD_DETECTION:
#   - test_held_state_machine   (also guarded by #error inside the .cpp)
#
# Tests that need gtest / Unity / Arduino headers and are NOT host-buildable
# from this script are listed under SKIPPED at the bottom with a one-line
# reason. Run those via `pio test -e native_test` or on hardware.
# ============================================================================

set -euo pipefail

# Resolve repo paths relative to this script so it can be invoked from
# anywhere. SCRIPT_DIR = auto_orientation/tools, REPO_ROOT = auto_orientation.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${REPO_ROOT}"

BIN_DIR="tests"

# Tally counters
TOTAL_BUILDS=0
PASSED_BUILDS=0
FAILED_BUILDS=0
FAILED_NAMES=()

# ----------------------------------------------------------------------------
# Idempotent cleanup: remove only executables, keep source files.
# ----------------------------------------------------------------------------
echo "[clean] removing previously-built test executables under ${BIN_DIR}/"
# `find -type f -executable` keeps .cpp/.h/.py/.md/.ino out of harm's way.
# The `! -name '*.cpp' ...` belt-and-braces is in case anyone marks a source
# file executable.
find "${BIN_DIR}" -maxdepth 1 -type f -executable \
    ! -name '*.cpp' ! -name '*.h' ! -name '*.in' \
    ! -name '*.md'  ! -name '*.py' ! -name '*.ino' \
    -delete 2>/dev/null || true

# ----------------------------------------------------------------------------
# build_test <name> <defines> <extra_sources_csv> [extra_flags]
#
# <name>            test binary name (and matching tests/<name>.cpp source)
# <defines>         space-separated list of -D / -U flags (may be empty)
# <extra_sources>   space-separated list of additional .cpp files relative
#                   to REPO_ROOT (may be empty)
# <extra_flags>     optional extra cflags / linker flags (e.g. -fpermissive
#                   -lunity); pass empty string to skip.
#
# Always compiles with -std=c++11 -O2. Adds -fpermissive / -I... only when
# explicitly requested via <extra_flags>.
# ----------------------------------------------------------------------------
build_test() {
    local name="$1"
    local defines="$2"
    local extra_sources="$3"
    local extra_flags="${4:-}"

    TOTAL_BUILDS=$((TOTAL_BUILDS + 1))

    local src="${BIN_DIR}/${name}.cpp"
    local out="${BIN_DIR}/${name}"
    if [[ ! -f "${src}" ]]; then
        echo "[FAIL] ${name}: source ${src} not found"
        FAILED_BUILDS=$((FAILED_BUILDS + 1))
        FAILED_NAMES+=("${name}")
        return
    fi

    # Build the g++ command. shellcheck-friendly array form.
    local cmd=(g++ -std=c++11 -O2)
    # shellcheck disable=SC2206
    cmd+=(${defines})
    # shellcheck disable=SC2206
    cmd+=(${extra_flags})
    cmd+=(-o "${out}" "${src}")
    # shellcheck disable=SC2206
    cmd+=(${extra_sources})

    echo "[build] ${name}"
    echo "        ${cmd[*]}"
    # Capture output so we can show it on failure.
    local log
    if log=$("${cmd[@]}" 2>&1); then
        echo "[ PASS ] ${name}"
        PASSED_BUILDS=$((PASSED_BUILDS + 1))
        # Print any warnings the compile produced.
        if [[ -n "${log}" ]]; then
            printf '%s\n' "${log}" | sed 's/^/        /'
        fi
    else
        local rc=$?
        echo "[ FAIL ] ${name} (g++ exit ${rc})"
        printf '%s\n' "${log}" | sed 's/^/        /'
        FAILED_BUILDS=$((FAILED_BUILDS + 1))
        FAILED_NAMES+=("${name}")
    fi
}

# Common source bundles (kept as variables so per-test build_test calls stay
# readable and matched against the docblock compile lines verbatim).
SRC_BAL_CORE="src/applications/balancing_robot/balance_app.cpp \
              src/applications/balancing_robot/safety.cpp \
              src/control/pid_controller.cpp \
              src/control/auto_pid_tuner.cpp \
              src/navigation/mounting_calibration.cpp \
              src/navigation/online_mounting_estimator.cpp"

# Same as core + plant_identifier (the BOOTSTRAP/CHARACTERISE/HELD tests
# instantiate paths that link the RLS identifier).
SRC_BAL_FULL="src/applications/balancing_robot/balance_app.cpp \
              src/applications/balancing_robot/safety.cpp \
              src/control/pid_controller.cpp \
              src/control/auto_pid_tuner.cpp \
              src/control/plant_identifier.cpp \
              src/navigation/mounting_calibration.cpp \
              src/navigation/online_mounting_estimator.cpp"

SRC_MATH="src/math/quaternion.cpp \
          src/math/quaternion_conversions.cpp"

echo
echo "============================================================"
echo " Native g++ unit-test build"
echo " Repo root: ${REPO_ROOT}"
echo "============================================================"
echo

# ----------------------------------------------------------------------------
# Legacy math tests (kept from the original tools/build_tests.sh).
# These three need gtest available via pkg-config.
# ----------------------------------------------------------------------------
if pkg-config --exists gtest_main gtest 2>/dev/null; then
    GTEST_FLAGS="$(pkg-config --cflags --libs gtest_main gtest)"

    build_test "test_bno085_extensions" \
        "-Wall -Wextra -I." \
        "${SRC_MATH}" \
        "-std=c++17 ${GTEST_FLAGS}"

    build_test "integration_test_math_pipeline" \
        "-Wall -Wextra -I." \
        "${SRC_MATH}" \
        "-std=c++17 ${GTEST_FLAGS}"

    build_test "benchmark_math" \
        "-Wall -Wextra -O3 -I." \
        "${SRC_MATH}" \
        "-std=c++17 ${GTEST_FLAGS}"
else
    echo "[skip] gtest not found via pkg-config — skipping gtest-based math tests"
    echo "       (test_bno085_extensions, integration_test_math_pipeline, benchmark_math)"
fi

# ----------------------------------------------------------------------------
# Pure-host math / nav tests (no test framework dependency).
# ----------------------------------------------------------------------------
build_test "test_quaternion" \
    "" \
    "${SRC_MATH}"

build_test "test_quaternion_performance" \
    "" \
    "${SRC_MATH}"

build_test "test_plant_identifier" \
    "" \
    "src/control/plant_identifier.cpp"

build_test "test_mounting_calibration" \
    "-Wall" \
    "src/navigation/mounting_calibration.cpp" \
    "-O0 -g"

build_test "test_online_mounting_estimator" \
    "" \
    "src/navigation/online_mounting_estimator.cpp"

# ----------------------------------------------------------------------------
# BalanceApp test suite (printf-based; no Unity / gtest).
# Compile lines copied verbatim from each test's docblock.
# ----------------------------------------------------------------------------
build_test "test_balance_app" \
    "-DUNIT_TEST" \
    "${SRC_BAL_FULL}"

build_test "test_balance_app_bootstrap" \
    "-DUNIT_TEST" \
    "${SRC_BAL_FULL}"

build_test "test_balance_app_collision" \
    "-DUNIT_TEST" \
    "${SRC_BAL_FULL}" \
    "-fpermissive"

build_test "test_balance_app_soft_cutoff" \
    "-DUNIT_TEST" \
    "${SRC_BAL_FULL}" \
    "-fpermissive"

# Baseline gyro/accel noise-floor estimator (noise_floor_estimator.h) — pure
# measurement layer feeding the noise-floor-derived scope violations. Header-
# only module, so no extra source file beyond the standard balance set.
build_test "test_noise_floor_estimator" \
    "-DUNIT_TEST" \
    "${SRC_BAL_FULL}" \
    "-fpermissive"

# PID NaN/Inf safety test (audit P1-003, security_audit_2026-05-22.md). Plain
# g++ (no Unity) so the safety-critical finiteness guards in set_tunings/clamp_
# are always exercised by this script — the Unity test_pid_controller below is
# skipped when unity.h is absent. Links only pid_controller.cpp.
build_test "test_pid_nan_safety" \
    "-DUNIT_TEST" \
    "src/control/pid_controller.cpp"

build_test "test_balance_app_encoder" \
    "-DUNIT_TEST -DNATIVE_TEST -DUSE_WHEEL_ENCODERS" \
    "${SRC_BAL_FULL} src/sensors/wheel_encoder.cpp src/control/position_loop.cpp src/control/cascade_self_audit.cpp"

build_test "test_balance_app_pwm_discovery" \
    "-DUNIT_TEST -DNATIVE_TEST -DUSE_WHEEL_ENCODERS" \
    "${SRC_BAL_FULL} src/sensors/wheel_encoder.cpp src/control/position_loop.cpp src/control/cascade_self_audit.cpp" \
    "-fpermissive"

# Workstream G (item G4) — BalanceApp telemetry-accessor tests. Same native
# encoder build profile as test_balance_app_encoder: USE_WHEEL_ENCODERS pulls
# the outer-loop getters in, NATIVE_TEST switches WheelEncoder to its mock
# backend. Links position_loop.cpp for the PositionLoop pass-throughs.
build_test "test_balance_telemetry" \
    "-DUNIT_TEST -DNATIVE_TEST -DUSE_WHEEL_ENCODERS" \
    "${SRC_BAL_FULL} src/sensors/wheel_encoder.cpp src/control/position_loop.cpp src/control/cascade_self_audit.cpp"

# HELD state-machine test — the canonical reason this rewrite exists.
# REQUIRES -DUSE_BALANCE_HELD_DETECTION (also guarded by #error inside
# tests/test_held_state_machine.cpp itself).
build_test "test_held_state_machine" \
    "-DUNIT_TEST -DUSE_BALANCE_HELD_DETECTION" \
    "${SRC_BAL_FULL}" \
    "-fpermissive"

# ----------------------------------------------------------------------------
# Minimal Uno application tests.
# ----------------------------------------------------------------------------
build_test "test_uno_balance_app" \
    "-DUNIT_TEST" \
    "src/applications/balancing_robot_uno/uno_balance_app.cpp \
     src/control/pid_controller.cpp \
     src/applications/balancing_robot_uno/tune_storage.cpp"

# ----------------------------------------------------------------------------
# Driver / hardware abstraction tests (printf-based).
# ----------------------------------------------------------------------------
build_test "test_l298n_motor" \
    "-DUNIT_TEST" \
    "src/actuators/l298n_motor_driver.cpp" \
    "-Isrc/actuators"

# ----------------------------------------------------------------------------
# Outer-loop (PositionLoop) tests — printf-based, no test framework.
# PositionLoop is a pure control module: link only position_loop.cpp.
# Compile lines copied verbatim from each test's docblock.
# ----------------------------------------------------------------------------
build_test "test_position_loop" \
    "-Iinclude -Isrc" \
    "src/control/position_loop.cpp src/control/cascade_self_audit.cpp" \
    "-fpermissive"

build_test "test_position_gain_derivation" \
    "-Iinclude -Isrc" \
    "src/control/position_loop.cpp src/control/cascade_self_audit.cpp" \
    "-fpermissive"

# M-4 (roadmap) — CascadeSelfAudit: runtime K_VEL self-confirmation that
# closes the deferred F-3 bench observation by measuring closed-loop damping
# from position_m step responses. Pure module: link only cascade_self_audit.cpp.
build_test "test_cascade_self_audit" \
    "-Iinclude -Isrc" \
    "src/control/cascade_self_audit.cpp"

# ----------------------------------------------------------------------------
# Unity-framework tests (compile only if unity is available on the host).
# These also run via `pio test -e native_test`; the host build below is a
# convenience for fast iteration.
# ----------------------------------------------------------------------------
HAS_UNITY=0
# Locate either a unity.h on the include path or a libunity to link against.
if echo '#include <unity.h>' | g++ -E -x c++ - >/dev/null 2>&1; then
    HAS_UNITY=1
fi

if [[ "${HAS_UNITY}" -eq 1 ]]; then
    build_test "test_pid_controller" \
        "" \
        "src/control/pid_controller.cpp" \
        "-lunity"

    build_test "test_relay_feedback" \
        "" \
        "src/control/auto_pid_tuner.cpp \
         src/control/pid_controller.cpp \
         src/control/tuners/relay_feedback.cpp" \
        "-lunity"

    build_test "test_wheel_encoder" \
        "-DNATIVE_TEST -Isrc" \
        "src/sensors/wheel_encoder.cpp" \
        "-lunity"
else
    echo "[skip] unity.h not on host include path — skipping Unity-framework tests"
    echo "       (test_pid_controller, test_relay_feedback, test_wheel_encoder)"
    echo "       Run them via: pio test -e native_test -f <name>"
fi

# ----------------------------------------------------------------------------
# Summary
# ----------------------------------------------------------------------------
echo
echo "============================================================"
echo " Build summary"
echo "============================================================"
echo "   total:  ${TOTAL_BUILDS}"
echo "   passed: ${PASSED_BUILDS}"
echo "   failed: ${FAILED_BUILDS}"
if (( FAILED_BUILDS > 0 )); then
    echo "   failures:"
    for n in "${FAILED_NAMES[@]}"; do
        echo "     - ${n}"
    done
fi
echo

# ----------------------------------------------------------------------------
# SKIPPED — tests not buildable by this host-g++ script. One-liner why.
# ----------------------------------------------------------------------------
# test_bno055.cpp                       — needs gtest + Adafruit_BNO055 mock; gated
# test_coordinates.cpp                  — DEBUG_MODE + gtest only
# test_coordinate_frame.cpp             — DEBUG_MODE + gtest + coordinate_frame.cpp
# test_coordinate_frame_standalone.cpp  — see its own docblock; standalone host build
# test_coordinates_standalone.cpp       — see its own docblock; standalone host build
# test_ekf.cpp                          — no docblock compile line; pio test
# test_gps.cpp                          — needs only g++ but EkfBuild path; pio test
# test_json_ekf_output.cpp              — DEBUG_MODE + test_helpers.h, target-only
# test_json_output.cpp                  — same as above
# test_magnetic_declination.cpp         — DEBUG_MODE + gtest only
# test_measurement_model.cpp            — DEBUG_MODE + gtest only
# test_persistent_storage.cpp           — Unity, native_test env (PlatformIO)
# test_sensor_output_manager.cpp        — DEBUG_MODE + gtest only
# test_snapshot_recorder.cpp            — Arduino target build only
# test_state_dynamics.cpp               — DEBUG_MODE + gtest only

if (( FAILED_BUILDS > 0 )); then
    exit 1
fi
exit 0
