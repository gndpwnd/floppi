# Test Infrastructure v2 — Modular Refactor
**Date**: 2026-05-20
**Agent**: fc-test-modularizer@flight_controller:1
**Scope**: Workstream 3 subset (refactor only — CI/CD deferred)

---

## 1. Summary

The single-file test harness `tests/test_calibration.sh` (480 lines, 18 tests,
42 assertions) has been split into a reusable harness library and a calibration
test suite. The original entry-point path still works — operators and any
external callers do not need to change anything.

**Goals achieved**:
- Shared functions extracted to `tests/lib/harness.sh` so future suites
  (motors, telemetry, radio-only, etc.) can be added without duplicating
  port/serial/assertion code.
- All 18 test functions and 42 assertions migrated verbatim into
  `tests/suites/test_calibration.sh`.
- Original `tests/test_calibration.sh` preserved as a thin wrapper.
- ESP32 reset path landed as a documented stub (no functional impl yet).

**Out of scope (deferred)**:
- CI/CD workflow files (`.github/workflows/`).
- ESP32 RTS/DTR reset implementation.
- New test suites (motors, telemetry, etc.).
- Changes to `tools/serial_monitor.py` or `tools/dev.sh`.

---

## 2. What Changed

### New files

| Path | Lines | Purpose |
|------|-------|---------|
| `tests/lib/harness.sh` | 246 | Shared helpers, sourced by every suite |
| `tests/suites/test_calibration.sh` | 304 | The 18 calibration tests |

### Modified files

| Path | Before | After | Purpose |
|------|-------|------|---------|
| `tests/test_calibration.sh` | 480 lines, full harness + tests | 21-line `exec` wrapper | Preserve external entry point |

### Functions extracted into `tests/lib/harness.sh`

| Function | Type | Notes |
|---|---|---|
| `test_pass` / `test_fail` | logging | Increment `PASS_COUNT` / `FAIL_COUNT` |
| `info` / `section` | logging | Yellow / cyan banners |
| `release_port` | port mgmt | `fuser -k` any holders of `$PORT` |
| `reboot_teensy` | board reset | Dispatches by `detect_board_type` |
| `detect_board_type` | board reset | **Stub** — always returns `teensy` (TODO) |
| `run_serial` | serial wrapper | `serial_monitor.py` call + CDC recovery |
| `check_output` | assertion | Case-insensitive ERE patterns; dumps capture on fail |
| `assert_pattern_in_output` | alias | Recon-doc naming for `check_output` |
| `check_absent` | assertion | Negative pattern check |
| `harness_check_prereqs` | setup | Validate `serial_monitor.py`, port, ModemManager |
| `harness_print_header` / `harness_print_footer` | reporting | Suite-agnostic banner + result lines |

### Globals exported by the harness

`FC_DIR`, `SERIAL_MON`, `TEENSY_REBOOT`, `RED/GREEN/YELLOW/CYAN/NC`,
`PASS_COUNT`, `FAIL_COUNT`. The harness expects `PORT` and `RESULTS_DIR` to be
set by the suite before sourcing.

---

## 3. New Layout

```
tests/
├── test_calibration.sh          # thin wrapper (operator entry point, unchanged path)
├── lib/
│   └── harness.sh               # shared functions, sourced by suites
├── suites/
│   └── test_calibration.sh      # the 18 calibration tests
└── results/                     # captured output, created at runtime
```

---

## 4. How to Add a New Test Suite

Copy this skeleton to `tests/suites/test_<name>.sh`:

```bash
#!/bin/bash
# Test suite for <subject>.
set -euo pipefail

PORT="${1:-/dev/ttyACM0}"
TEST="${2:-all}"
SUITE_DIR="$(cd "$(dirname "$0")" && pwd)"
RESULTS_DIR="$SUITE_DIR/../results"

mkdir -p "$RESULTS_DIR"
# shellcheck source=../lib/harness.sh
source "$SUITE_DIR/../lib/harness.sh"

test_example() {
    section "Example test"
    local outfile="$RESULTS_DIR/test_example.txt"
    run_serial "$outfile" 1 "h"
    check_output "$outfile" "Example" "CALIBRATION COMMANDS"
}

harness_print_header "<Suite name>" "$TEST"
harness_check_prereqs
release_port

case "$TEST" in
    example) test_example ;;
    all)
        reboot_teensy
        test_example
        ;;
    *) echo "Unknown test: $TEST"; exit 1 ;;
esac

harness_print_footer
[ "$FAIL_COUNT" -eq 0 ]
```

Optional: add a thin wrapper at `tests/test_<name>.sh` if you want a
top-level entry point.

---

## 5. ESP32 Support Status

A skeleton landed. `detect_board_type()` in `tests/lib/harness.sh` currently
always returns `teensy`. `reboot_teensy()` already dispatches on the result
and prints a stub message for the `esp32` branch.

**Still TODO**:
1. **Board detection**: probe `lsusb` for known VID:PID pairs, or inspect the
   device path (`/dev/ttyACM*` vs `/dev/ttyUSB*`), or accept an explicit
   `FC_BOARD=esp32` env var.
2. **ESP32 reset**: implement RTS/DTR toggle. Either add a small Python
   helper, or extend `tools/serial_monitor.py` with a `--reset` flag.
   Touching `tools/` is owned by another workstream — coordinate.
3. **Boot marker**: confirm what string ESP32 calibration firmware prints on
   boot (currently we look for `CALIBRATION MODE|FLIGHT CONTROLLER READY`,
   which should be platform-agnostic).
4. **Function naming**: rename `reboot_teensy` to `reboot_board` once the
   ESP32 path is real, with a deprecation alias.

---

## 6. Migration Notes for Operators

**Nothing changes for day-to-day use.** All of these still work:

```bash
./tests/test_calibration.sh                    # all tests, /dev/ttyACM0
./tests/test_calibration.sh /dev/ttyACM0 help  # single test
./tests/test_calibration.sh /dev/ttyACM0 all   # explicit all
bash tests/test_calibration.sh                 # also fine
```

Internally these now `exec` into `tests/suites/test_calibration.sh`. Output
still lands in `tests/results/`. Exit codes are unchanged (0 = all pass,
non-zero = at least one failure).

If you previously sourced (`. tests/test_calibration.sh`) or grepped the file
for individual test functions, point at the suite file in `tests/suites/`
instead.

---

## 7. Verification

- `bash -n` parses cleanly on all three new/modified shell files.
- Wrapper logic: `tests/test_calibration.sh` resolves its own directory and
  `exec`s `suites/test_calibration.sh` with all positional args forwarded —
  the suite then reads `$1` (port) and `$2` (test selector) exactly as the
  original did.
- Sourcing path: the suite computes `SUITE_DIR` from `$0` and sources
  `$SUITE_DIR/../lib/harness.sh`. The harness in turn computes `FC_DIR` from
  `${BASH_SOURCE[0]}` (`tests/lib/harness.sh` → up two levels), so
  `tools/serial_monitor.py` resolves correctly regardless of CWD.
- All 18 test functions kept their original names, command sequences, wait
  timings, and assertion patterns. No assertion was added, removed, or
  reworded.

**Not verified**: actual execution against hardware — that requires a Teensy
on the bench and is outside this agent's scope.

---

## 8. Open TODOs (carried forward)

- Implement `detect_board_type` (lsusb / device path / env var).
- Implement ESP32 RTS/DTR reset (coordinate with the `tools/` workstream).
- Add `tests/suites/test_motors.sh` once ESCs/motors are connected.
- Add `tests/suites/test_telemetry.sh` (split telemetry checks out of the
  calibration suite for cleaner separation).
- Add `tests/suites/test_radio.sh` for radio-specific assertions (range,
  failsafe response).
- Create `.github/workflows/build_test.yml` (deferred per workstream scope).
- Update `docs/roadmap.md` to mark test-infrastructure-v2 done (deferred —
  outside this agent's write zone).
