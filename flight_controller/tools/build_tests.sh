#!/bin/bash
# ============================================================================
# build_tests.sh — native (host-side, plain g++) unit-test runner.
# ============================================================================
#
# Run:
#   cd flight_controller && bash tools/build_tests.sh
#
# What it does:
#   - AUTO-DISCOVERS every  tests/native/test_*.cpp  file (glob loop).
#   - Compiles each as a SELF-CONTAINED standalone binary with a UNIFORM
#     command line:
#       g++ -std=c++11 -O2 -DUNIT_TEST -Itests/native -o <tmpbin> <file>
#     No src/ is linked — native tests are pure-logic tests that copy or
#     include only portable math under test.
#   - Runs each binary; reports [build] / [PASS] / [FAIL] per file.
#   - Prints a total / passed / failed summary; exits non-zero if any fail.
#
# Why glob discovery:
#   Follow-up agents add tests simply by dropping a new
#   tests/native/test_*.cpp file that #includes test_helpers.h. It is picked
#   up automatically — zero edits to this script, zero merge collisions.
#
# Resource discipline: plain g++ only. No PlatformIO, no pip, no Docker. The
# whole suite run is bounded; callers may additionally wrap it in `timeout`.
# ============================================================================

set -uo pipefail

# Resolve repo paths relative to this script so it can be invoked from
# anywhere. SCRIPT_DIR = flight_controller/tools, REPO_ROOT = flight_controller.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${REPO_ROOT}"

TEST_DIR="tests/native"
# Per-binary temp dir, cleaned on exit. Built binaries never land in the repo.
BIN_DIR="$(mktemp -d "${TMPDIR:-/tmp}/fc_native_tests.XXXXXX")"
trap 'rm -rf "${BIN_DIR}"' EXIT

# Uniform compile flags — the canonical native-test command line.
CXX="${CXX:-g++}"
CXXFLAGS=(-std=c++11 -O2 -DUNIT_TEST -I"${TEST_DIR}")

# Tally counters.
TOTAL=0
PASSED=0
FAILED=0
FAILED_NAMES=()

echo
echo "============================================================"
echo " flight_controller — native g++ unit-test suite"
echo " repo root: ${REPO_ROOT}"
echo " discovering: ${TEST_DIR}/test_*.cpp"
echo "============================================================"
echo

# ----------------------------------------------------------------------------
# Glob discovery. `nullglob` makes the pattern expand to nothing (rather than
# the literal string) when no test files exist.
# ----------------------------------------------------------------------------
shopt -s nullglob
TEST_FILES=("${TEST_DIR}"/test_*.cpp)
shopt -u nullglob

if [[ ${#TEST_FILES[@]} -eq 0 ]]; then
    echo "[warn] no ${TEST_DIR}/test_*.cpp files found — nothing to run"
    echo
    echo "============================================================"
    echo " summary: total=0  passed=0  failed=0"
    echo "============================================================"
    exit 0
fi

for src in "${TEST_FILES[@]}"; do
    name="$(basename "${src}" .cpp)"
    out="${BIN_DIR}/${name}"
    TOTAL=$((TOTAL + 1))

    echo "[build] ${name}"
    echo "        ${CXX} ${CXXFLAGS[*]} -o ${out} ${src}"

    # --- compile -------------------------------------------------------------
    if ! build_log="$("${CXX}" "${CXXFLAGS[@]}" -o "${out}" "${src}" 2>&1)"; then
        echo "[ FAIL ] ${name}  (compile error)"
        printf '%s\n' "${build_log}" | sed 's/^/        /'
        FAILED=$((FAILED + 1))
        FAILED_NAMES+=("${name} (build)")
        echo
        continue
    fi
    # Surface any compiler warnings even on a successful build.
    if [[ -n "${build_log}" ]]; then
        printf '%s\n' "${build_log}" | sed 's/^/        /'
    fi

    # --- run -----------------------------------------------------------------
    if run_log="$("${out}" 2>&1)"; then
        printf '%s\n' "${run_log}" | sed 's/^/        /'
        echo "[ PASS ] ${name}"
        PASSED=$((PASSED + 1))
    else
        rc=$?
        printf '%s\n' "${run_log}" | sed 's/^/        /'
        echo "[ FAIL ] ${name}  (exit ${rc})"
        FAILED=$((FAILED + 1))
        FAILED_NAMES+=("${name} (run)")
    fi
    echo
done

# ----------------------------------------------------------------------------
# Summary
# ----------------------------------------------------------------------------
echo "============================================================"
echo " Build + run summary"
echo "============================================================"
echo "   total:  ${TOTAL}"
echo "   passed: ${PASSED}"
echo "   failed: ${FAILED}"
if (( FAILED > 0 )); then
    echo "   failures:"
    for n in "${FAILED_NAMES[@]}"; do
        echo "     - ${n}"
    done
fi
echo

if (( FAILED > 0 )); then
    exit 1
fi
exit 0
