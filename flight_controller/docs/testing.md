# Running the Tests — A Primer

There are **two** independent test suites in this project. They test different
things and have different requirements.

| Suite | What it tests | Needs hardware? | Runner |
|-------|---------------|-----------------|--------|
| **Calibration suite** | Live firmware on a flashed board — boot banner, telemetry format, calibration command output | **Yes** — a Teensy/ESP32 on a serial port | `tests/test_calibration.sh` |
| **Native unit tests** | Pure portable math (attitude, filters, mixer, baro compensation) | **No** — host `g++` only | `tools/build_tests.sh` |

The calibration suite is **18 tests / 42 assertions** (see
[`findings/test_infrastructure_v2_2026-05-20.md`](findings/test_infrastructure_v2_2026-05-20.md)).

---

## 1. Native unit tests (no hardware)

The fast inner loop. No board, no PlatformIO, no pip — just `g++`.

```bash
cd flight_controller
bash tools/build_tests.sh
```

It auto-discovers every `tests/native/test_*.cpp`, compiles each as a
standalone binary (`g++ -std=c++11 -O2 -DUNIT_TEST`), runs it, and prints
`[build] / [PASS] / [FAIL]` per file plus a total/passed/failed summary. Exit
code is non-zero if any test fails. Adding a test = drop a new
`tests/native/test_*.cpp` that includes `test_helpers.h`; it is picked up
automatically.

## 2. Calibration suite (needs a flashed board)

This drives a real board over serial and checks its responses.

**Prerequisites:**

1. **Python + pyserial** installed (the suite shells out to
   `tools/serial_monitor.py`).
2. **ModemManager stopped** — it probes USB CDC and corrupts Teensy serial:
   ```bash
   sudo systemctl stop ModemManager
   ```
   (`dev.sh` does this for you and warns if it is active.)
3. A **`_calibration` env flashed** to the board (not a live env) — the tests
   expect calibration-mode command output:
   ```bash
   pio run -e teensy40_calibration -t upload
   ```
4. Know your port — usually `/dev/ttyACM0` (Teensy) or `/dev/ttyUSB0` (ESP32).

**Run everything:**

```bash
cd flight_controller
./tests/test_calibration.sh                 # all tests on /dev/ttyACM0
./tests/test_calibration.sh /dev/ttyACM0 all
```

**Run a single test** (second arg is the test name, e.g. the command it
exercises):

```bash
./tests/test_calibration.sh /dev/ttyACM0 help
```

`tests/test_calibration.sh` is a thin wrapper; the real logic lives in
`tests/lib/harness.sh` (shared helpers) and `tests/suites/test_calibration.sh`
(the 18 tests).

### Where output goes

Per-test serial captures are written to **`tests/results/*.txt`** (e.g.
`test_help.txt`, `test_telemetry.txt`, `full_calibration_test.txt`). When a test
fails, open the matching file to see the raw serial the board sent.

### Reading pass/fail

Each test prints a pass/fail line as it runs and the suite prints a final
summary. A failing assertion names the function and the expected string — diff
that against the live `tools/calibrate.sh` output, or against the expected
strings in `tests/suites/test_calibration.sh`.

## When a test fails

Use the decision tree: [`diagnose_decision_tree.md`](diagnose_decision_tree.md)
flow 6 ("`dev.sh test` fails") maps the common failure shapes (boot banner never
reached, telemetry format changed, wrong env flashed, serial-port errors) to
fixes. For port problems specifically, run `tools/dev.sh diagnose` and fall back
to flow 1.
