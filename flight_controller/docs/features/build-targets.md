# Feature: Build Target Separation (Calibration vs Live)

> Status: Planned
> Created: 2026-02-05

## Summary

Separate the firmware into two build modes using PlatformIO environments:
- **Live builds** (default): Lean flight firmware with hard-coded values, no calibration code overhead
- **Calibration builds**: Full calibration support, debug output, mutable offsets

## Motivation

Currently the firmware has calibration routines, debug prints, and flight code all compiled together. There's no clean way to ship a "production" build without calibration overhead. Additionally, two overlapping calibration paths exist (simple versions in main.cpp and advanced versions in lib/Calibration/) creating confusion about which runs when.

## Design

### PlatformIO Environment Setup

Use PlatformIO's `extends` directive so each board gets a calibration variant that inherits all board-specific config:

```ini
# platformio.ini additions

[env:teensy40_calibration]
extends = env:teensy40
build_flags = ${env:teensy40.build_flags} -D CALIBRATION_MODE

[env:teensy41_calibration]
extends = env:teensy41
build_flags = ${env:teensy41.build_flags} -D CALIBRATION_MODE

[env:teensy36_calibration]
extends = env:teensy36
build_flags = ${env:teensy36.build_flags} -D CALIBRATION_MODE
```

### Usage

```bash
# Live builds (default — no calibration code compiled in)
pio run -e teensy40
pio run -e teensy40 --target upload

# Calibration builds (full calibration support)
pio run -e teensy40_calibration
pio run -e teensy40_calibration --target upload
```

### Code Guards in main.cpp

Wrap calibration-specific sections with `#ifdef CALIBRATION_MODE`:

**In `setup()`:**
- CH6-on-startup calibration trigger: only in calibration builds
- Calibration welcome message: only in calibration builds

**In `loop()`:**
- `checkCalibrationMode()` call: only in calibration builds
- Calibration state machine block: only in calibration builds
- Debug print function calls: only in calibration builds (or behind a separate `DEBUG_MODE` flag)

**Live build loop becomes:**
```
loop() {
  getCommands();
  failSafe();
  getIMUdata();
  Madgwick6DOF(...);
  getDesState();
  armedStatus();
  if (armedFly) { controlANGLE(); controlMixer(); }
  scaleCommands();
  throttleCut();
  commandMotors();
  loopRate(LOOP_FREQUENCY_HZ);
}
```

Clean, tight, no calibration branching.

### config.h Guards

The `RUN_*` calibration program flags only take effect in calibration builds:

```c
#ifdef CALIBRATION_MODE
//#define RUN_RADIO_CALIBRATION
//#define RUN_IMU_CALIBRATION
//#define RUN_IMU_ORIENTATION
#endif
```

This prevents accidentally leaving a calibration flag uncommented in a live build.

### Unify Calibration Paths

Currently there are two overlapping IMU calibration implementations:
1. `runAccelGyroCalibration()` in main.cpp — simple, no quality checks
2. `calibrateIMU()` in lib/Calibration/calibration.cpp — advanced, with stability validation and quality checks

**Plan:** Make the CH6-triggered calibration call the calibration.cpp versions:
- CH6 mid (1200-1800) → `calibrateIMU()` from calibration.cpp
- CH6 high (>1800) → `calibrateIMUWithOrientation()` from calibration.cpp

Remove the duplicate simple versions from main.cpp.

### Fix Calibration Output Format

`printIMUCalibrationResults()` in calibration.cpp currently prints:
```
float AccErrorX = 0.123456;
```

Must be changed to match config.h format:
```
#define IMU_ACC_ERROR_X 0.123456f
```

Same fix needed for `printRadioCalibrationResults()` — output should match the `#define` format in config.h exactly.

## Files Changed

| File | Change |
|------|--------|
| `platformio.ini` | Add `_calibration` environments using `extends` |
| `include/config.h` | Wrap `RUN_*` flags in `#ifdef CALIBRATION_MODE` |
| `src/main.cpp` | Add `#ifdef CALIBRATION_MODE` guards around calibration blocks, remove duplicate calibration functions |
| `lib/Calibration/calibration.cpp` | Fix output format in print functions to match config.h `#define` syntax |

## What Does NOT Change

- Core flight loop (IMU, Madgwick, PID, motor mixing, arming, failsafe)
- Pin definitions
- Radio communication module
- Any PID gains, filter coefficients, or control logic
- Existing board environments (teensy40, teensy41, teensy36)

## Risks

- **Low risk**: Changes are purely structural (build flags and `#ifdef` guards). No control logic is modified.
- **Testing**: After implementation, verify both `pio run -e teensy40` and `pio run -e teensy40_calibration` compile cleanly.

## Related

- [auto-calibration-research.md](../findings/auto-calibration-research.md) — research on calibration approaches
- [scope.md](../scope.md) — project boundaries and constraints
- [roadmap.md](../roadmap.md) — Firmware State Machine section
