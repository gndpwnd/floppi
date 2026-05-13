# tools/

Cross-cutting scripts for the `auto_orientation` project. Everything here
is meant to be run from the repo root unless noted otherwise.

## New cross-cutting tools

### `build_matrix.sh` &mdash; multi-MCU PlatformIO compile matrix

Builds every `[env:*]` defined in `platformio.ini` and prints a one-line
summary per env (status / flash / RAM). Used to make sure refactors don't
break the Mega / Teensy / ESP32 builds.

```
tools/build_matrix.sh                # build every env
tools/build_matrix.sh --quick        # skip heavy envs (full, snapshot)
tools/build_matrix.sh --env arduino_mega   # build a single env (CI)
tools/build_matrix.sh --help
```

Sample output:

```
ENV                          STATUS   FLASH           RAM
---------------------------- ------   -----           ---
arduino_mega                 OK       45998 / 256k    2432 / 8k
arduino_mega_calibration     OK       46210 / 256k    2438 / 8k
esp32dev                     FAIL     -               -
```

Exits `0` if every env built, `1` if any failed, `2` on usage errors.

Requires `pio` on `PATH` (PlatformIO Core). Pure bash, no external deps.

### `generate_balance_trajectory.py` &mdash; balance-bot scenario fixture

Closed-loop simulation of an inverted-pendulum balance bot under the
legacy `SelfBallancingRobot3.ino` PID controller (Kp=65, Ki=12, Kd=38,
5 ms sample, &plusmn;255 PWM, stiction deadband 15, PITCH_OFFSET = -8.6&deg;).
Emits a CSV used as the fixture for `tests/scenario_test_balancing.cpp`.

```
python3 tools/generate_balance_trajectory.py \
    --output tests/data/balancing_reference_trajectory.csv
```

Options: `--duration_ms 3000` (default), `--seed 42`.

The CSV columns are:

```
timestamp_ms,raw_pitch_deg,corrected_pitch_deg,pid_output,left_pwm,right_pwm
```

The generated CSV is committed under `tests/data/` so tests can run
without first executing the tool; regenerate it if the model or gains
change.

Self-contained &mdash; only standard library Python 3.

### `replay_trajectory.py` &mdash; CSV-driven pitch HIL replay (production)

Streams a `generate_balance_trajectory.py`-style CSV (`timestamp_ms,raw_pitch_deg,...`) over serial to a connected MCU at real-time (or scaled) pace and captures the firmware's response on the same port. Used for hardware-in-the-loop testing of the balance bot without an IMU. Text protocol (`pitch:%.3f\n`) is the default; a 4-byte binary float protocol is also available for future firmware. Linux operators: stop `ModemManager` first if reads look truncated.

### `auto_calibrate.py` &mdash; mag ellipsoid-fit blob packer (skeleton)

CLI wrapper for magnetometer calibration: reads a CSV of `mx,my,mz` samples and emits a binary blob in `bno055` (22 B), `bno085` (256 B), or `mpu_external` (48 B float[12]) layout. **Skeleton only** &mdash; currently does mean-as-offset + identity soft-iron, not a real ellipsoid fit. Phase 5.5 follow-up will replace the stub with the Renaudin 2010 least-squares formulation.

## Pre-existing tools (unchanged)

These scripts pre-date this batch and are left untouched:

- `serial_monitor.py` &mdash; minimal line-by-line serial reader with
  optional substring filtering. Usage: `serial_monitor.py /dev/ttyACM0`.
- `simple_monitor.py` &mdash; verbose serial monitor for BNO085 calibration.
- `real_time_monitor.py` &mdash; full-screen terminal UI showing live
  roll/pitch/yaw, GPS, and calibration status from JSON sensor output.
- `test_monitor.py` &mdash; unit tests for `real_time_monitor.py`
  (quaternion conversion, JSON parsing, etc.).
- `demo_monitor.py` &mdash; pseudo-terminal data generator for testing the
  monitor without hardware.
- `bno_calibrate.py` &mdash; BNO085 calibration runner with progress display
  and result recording.
- `test_nmea_parser.py` &mdash; standalone NMEA (GPGGA/GPRMC) parser plus
  test suite for NEO-M9N input.
- `build_tests.sh` &mdash; compiles the desktop gtest targets
  (`test_bno085_extensions`, `integration_test_math_pipeline`).

See `EXAMPLES.md`, `MONITOR_README.md`, and `QUICKSTART.md` in this
directory for hands-on usage of the monitor stack.
