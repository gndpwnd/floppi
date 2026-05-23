# Plan: calibrate.sh — Interactive Calibration Wrapper

> Status: Implemented (shipped 2026-02-17 — `tools/calibrate.sh`; see [roadmap](../roadmap.md))
> Priority: High
> Created: 2026-02-13
>
> **This is a historical design record.** The wrapper described below shipped as
> `tools/calibrate.sh`. Kept for the rationale and design decisions; the live tool
> is the source of truth.

## Summary

Create a menu-driven bash wrapper (`tools/calibrate.sh`) around `serial_monitor.py` as the primary user-facing tool for flight controller calibration. Later, a Windows `calibrate.bat` equivalent.

## Problem

Currently, calibrating requires manually invoking `python3 tools/serial_monitor.py` with specific flags and knowing the right command sequences. Interactive calibrations (orientation, radio) need the user to physically move the board and respond to firmware prompts — this is awkward with raw Python CLI invocations.

## Solution

A bash wrapper script that provides:
- Menu-driven terminal UI with screen clearing between operations
- Prerequisite handling (ModemManager, port detection, Python check)
- Three operation modes: display (fire-and-forget), interactive (pass-through to firmware), and tuning (live parameter adjustment)
- CLI mode for scriptable access: `./calibrate.sh /dev/ttyACM0 imu`
- Later: Windows `calibrate.bat` equivalent

## Architecture

### Backend: serial_monitor.py (no changes needed)

The existing `--send CMD --wait 1 --interactive` pattern handles all interactive calibrations. No modifications required.

### Three Operation Categories

**A) Display commands** (h, c, s, g, p, d, t, n): Send command, show output, press Enter to return.
```bash
python3 serial_monitor.py PORT --no-dtr-reset --boot-wait 1 --send CMD --wait 3
```

**B) Interactive calibrations** (i, o, m, r, f, e, a): Send initial command, user types y/n directly to firmware. Ctrl+C returns to menu.
```bash
python3 serial_monitor.py PORT --no-dtr-reset --boot-wait 1 --send CMD --wait 1 --interactive
```

**C) Tuning mode** (g set, p set): Show current values, enter interactive mode for live parameter adjustment.

### Menu Structure

```
--- Information ---
 1) Help menu           2) Calibration status
 3) Channel status      4) PID gains
 5) Filter & limits     6) Dump all values

--- Calibration ---
 7) IMU single-position   8) IMU 6-position
 9) IMU + orientation     10) Radio channel mapping
11) Failsafe detection    12) ESC endpoints

--- Tuning ---
13) PID tuning (interactive)  14) Filter tuning (interactive)
15) Telemetry toggle          16) Network diagnostics

--- Workflow ---
17) Sequential calibration (guided)
 0) Exit
```

### CLI Mode (no menu)

```bash
./calibrate.sh                          # Auto-detect port, launch menu
./calibrate.sh /dev/ttyACM0             # Specific port, launch menu
./calibrate.sh /dev/ttyACM0 imu         # Run IMU calibration directly
./calibrate.sh /dev/ttyACM0 dump        # Dump values directly
./calibrate.sh /dev/ttyACM0 orientation # Run orientation detection
```

### Prerequisites Handling

- Auto-detect port (`/dev/ttyACM*` then `/dev/ttyUSB*`)
- Check ModemManager — offer `sudo systemctl stop` if active
- Check `serial_monitor.py` and Python 3 exist
- Release stale port holders via `fuser`

### Pattern Reference

Follow conventions from `build.sh`: same color palette, menu loop pattern, CLI argument handling, helper functions.

## Files to Create

| File | Description | Est. Lines |
|------|-------------|-----------|
| `tools/calibrate.sh` | Main wrapper script | ~350 |

## Files to Modify

| File | Change |
|------|--------|
| `docs/2_calibration_guide.md` | Add calibrate.sh as primary method, keep pio device monitor as fallback |
| `docs/todo.md` | Add calibrate.sh task |
| `docs/roadmap.md` | Reference calibrate.sh in serial tools section |
| `docs/scope.md` | Update serial tools policy |

## Key Design Decisions

1. **No changes to serial_monitor.py** — existing `--interactive` mode handles everything
2. **No changes to firmware** — firmware's y/n prompt protocol works through pass-through
3. **Ctrl+C to exit interactive** — `|| true` catches cleanly, returns to menu
4. **`--boot-wait 1` not 0** — ensures reader thread is established
5. **Screen clearing** — `clear` before each operation for clean display
6. **Separate from test_calibration.sh** — calibrate.sh is for humans, tests are for CI

## Windows calibrate.bat (Future)

Depends on making `serial_monitor.py` cross-platform first (currently uses Linux-only termios/fcntl/select). The `.bat` file would follow `build.bat` patterns. This is a separate task.

## Verification Plan

1. `./tools/calibrate.sh` — menu displays, navigation works
2. Option 1 (Help) — shows firmware help, returns to menu
3. Option 7 (IMU cal) — enters interactive, user can type y/n, Ctrl+C returns
4. CLI: `./calibrate.sh /dev/ttyACM0 dump` — works without menu
5. ModemManager check: prompts to stop if active
