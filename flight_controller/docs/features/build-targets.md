# Feature: Build Target Separation (Calibration vs Live)

> Status: Implemented
> Created: 2026-02-05
> Implemented: 2026-02-05

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

### Calibration Trigger (CH6 Switch)

Calibration routines are triggered via the CH6 3-position switch (hold for 3 seconds, throttle must be low, not armed):

| CH6 Position | Action |
|---|---|
| LOW (<1200us) | Normal flight mode |
| MID (1200-1800us) | `calibrateIMU()` — IMU offset calibration with quality checks |
| HIGH (>1800us) | `calibrateIMUWithOrientation()` — IMU cal + 3-position orientation detection |

Radio calibration (`calibrateRadio()`) currently has no CH6 trigger — it's interactive and needs a serial command mechanism (TBD).

### Calibration Output Format

All calibration print functions output config.h-compatible `#define` format:
```
#define IMU_ACC_ERROR_X 0.123456f
```

### Dead Code Removed

Old duplicate calibration functions removed from main.cpp:
- `runAccelGyroCalibration()`, `runAttitudeCalibration()`, `runRadioCalibration()`
- `calculate_IMU_error()`, `calibrateAttitude()`
- `RUN_*` config flags (no longer needed — CH6 triggers directly)

## Files Changed

| File | Change |
|------|--------|
| `platformio.ini` | Add `_calibration` environments using `extends` |
| `include/config.h` | CH6 trigger documentation, removed `RUN_*` flags |
| `src/main.cpp` | `#ifdef CALIBRATION_MODE` guards, dead code removed, CH6 state machine calls calibration.cpp directly |
| `lib/Calibration/calibration.cpp` | Output format fixed to config.h `#define` syntax, AUX detection bug fixed |
| `include/calibration.h` | `detectMovedChannel` updated to support 5 exclude channels |

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
