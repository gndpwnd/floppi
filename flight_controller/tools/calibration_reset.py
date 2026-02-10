#!/usr/bin/env python3
"""
Calibration Reset Tool

Resets all calibration-specific #define values in config.h back to factory defaults.
Preserves feature flags, non-calibration settings, and file structure.

Usage:
    python3 tools/calibration_reset.py              # Reset config.h (prompts for confirmation)
    python3 tools/calibration_reset.py --yes        # Reset without confirmation
    python3 tools/calibration_reset.py --dry-run    # Show what would change without modifying
    python3 tools/calibration_reset.py --config path/to/config.h  # Custom config path

Calibration values reset:
    - IMU offsets (ACC_ERROR, GYRO_ERROR) → 0.0
    - IMU scale factors (ACC_SCALE) → 1.0
    - Magnetometer offsets/scales → 0.0/1.0
    - Radio channel mapping → Mode 2 defaults (3/1/2/4/5/6)
    - Failsafe values → safe defaults (1000/1500/2000)
    - Filter coefficients → conservative defaults
    - PID gains → conservative defaults
    - CALIBRATED_* status markers → re-commented (uncalibrated)

NOT reset (preserved):
    - Feature flags (USE_OPTIMIZATION, USE_RACING, etc.)
    - IMU/receiver/display selection
    - Control mode selection
    - Pin overrides
    - Debug options
    - Optimization/Racing parameters
"""

import argparse
import os
import re
import sys

# Default calibration values — matches a "fresh" config.h
# Format: (define_name, default_value, comment)
CALIBRATION_DEFAULTS = [
    # IMU offsets
    ("IMU_ACC_ERROR_X", "0.0f", "Accelerometer X offset"),
    ("IMU_ACC_ERROR_Y", "0.0f", "Accelerometer Y offset"),
    ("IMU_ACC_ERROR_Z", "0.0f", "Accelerometer Z offset"),
    # IMU scale factors
    ("IMU_ACC_SCALE_X", "1.0f", "Accelerometer X scale"),
    ("IMU_ACC_SCALE_Y", "1.0f", "Accelerometer Y scale"),
    ("IMU_ACC_SCALE_Z", "1.0f", "Accelerometer Z scale"),
    # Gyro offsets
    ("IMU_GYRO_ERROR_X", "0.0f", "Gyroscope X offset"),
    ("IMU_GYRO_ERROR_Y", "0.0f", "Gyroscope Y offset"),
    ("IMU_GYRO_ERROR_Z", "0.0f", "Gyroscope Z offset"),
    # Radio channel mapping (Mode 2 defaults)
    ("THROTTLE_CHANNEL", "3", "Left stick up/down"),
    ("ROLL_CHANNEL", "1", "Right stick left/right"),
    ("PITCH_CHANNEL", "2", "Right stick up/down"),
    ("YAW_CHANNEL", "4", "Left stick left/right"),
    ("AUX1_CHANNEL", "5", "Switch (throttle cut/arm)"),
    ("AUX2_CHANNEL", "6", "Switch (aux function)"),
    # Failsafe values
    ("FAILSAFE_THROTTLE", "1000", "Minimum throttle"),
    ("FAILSAFE_ROLL", "1500", "Center"),
    ("FAILSAFE_PITCH", "1500", "Center"),
    ("FAILSAFE_YAW", "1500", "Center"),
    ("FAILSAFE_AUX1", "2000", "Throttle cut ON"),
    ("FAILSAFE_AUX2", "2000", "Default high"),
    # Filter coefficients
    ("MADGWICK_BETA", "0.04", "Attitude filter"),
    ("B_ACCEL", "0.14", "Accelerometer LP filter"),
    ("B_GYRO", "0.10", "Gyroscope LP filter"),
    ("B_DTERM", "0.15", "D-term LP filter"),
    ("B_MAG", "1.0", "Magnetometer LP filter"),
    # Magnetometer calibration (MPU9250)
    ("MAG_ERROR_X", "0.0f", "Mag X offset"),
    ("MAG_ERROR_Y", "0.0f", "Mag Y offset"),
    ("MAG_ERROR_Z", "0.0f", "Mag Z offset"),
    ("MAG_SCALE_X", "1.0f", "Mag X scale"),
    ("MAG_SCALE_Y", "1.0f", "Mag Y scale"),
    ("MAG_SCALE_Z", "1.0f", "Mag Z scale"),
    # PID gains — Rate mode
    ("KP_ROLL_RATE", "0.15f", "Roll rate P"),
    ("KI_ROLL_RATE", "0.15f", "Roll rate I"),
    ("KD_ROLL_RATE", "0.0004f", "Roll rate D"),
    ("I_LIMIT_ROLL", "25.0f", "Roll I-term limit"),
    ("KP_PITCH_RATE", "0.15f", "Pitch rate P"),
    ("KI_PITCH_RATE", "0.15f", "Pitch rate I"),
    ("KD_PITCH_RATE", "0.0004f", "Pitch rate D"),
    ("I_LIMIT_PITCH", "25.0f", "Pitch I-term limit"),
    ("KP_YAW_RATE", "0.30f", "Yaw rate P"),
    ("KI_YAW_RATE", "0.05f", "Yaw rate I"),
    ("KD_YAW_RATE", "0.00015f", "Yaw rate D"),
    ("I_LIMIT_YAW", "25.0f", "Yaw I-term limit"),
    # PID gains — Angle mode
    ("KP_ROLL_ANGLE", "0.20f", "Roll angle P"),
    ("KI_ROLL_ANGLE", "0.00f", "Roll angle I"),
    ("KD_ROLL_ANGLE", "0.05f", "Roll angle D"),
    ("KP_PITCH_ANGLE", "0.20f", "Pitch angle P"),
    ("KI_PITCH_ANGLE", "0.00f", "Pitch angle I"),
    ("KD_PITCH_ANGLE", "0.05f", "Pitch angle D"),
    # Max control limits
    ("MAX_ROLL_RATE", "200.0f", "Max roll rate (deg/s)"),
    ("MAX_PITCH_RATE", "200.0f", "Max pitch rate (deg/s)"),
    ("MAX_ROLL_ANGLE", "30.0f", "Max roll angle (deg)"),
    ("MAX_PITCH_ANGLE", "30.0f", "Max pitch angle (deg)"),
    ("MAX_YAW_RATE", "160.0f", "Max yaw rate (deg/s)"),
]

# Calibration status markers — these get re-commented (disabled) on reset
CALIBRATION_MARKERS = [
    "CALIBRATED_IMU",
    "CALIBRATED_IMU_ORIENT",
    "CALIBRATED_RADIO",
    "CALIBRATED_FAILSAFE",
    "CALIBRATED_ESC",
    "CALIBRATED_PID",
    "CALIBRATED_FILTERS",
    "CALIBRATED_MAG",
]


def find_config_h():
    """Find config.h relative to this script's location."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    # tools/ is inside flight_controller/
    config_path = os.path.join(script_dir, "..", "include", "config.h")
    return os.path.normpath(config_path)


def reset_defines(content, dry_run=False):
    """Replace calibration #define values with defaults and re-comment markers.
    Returns (new_content, changes, markers_reset)."""
    changes = []
    markers_reset = []
    lines = content.split('\n')
    new_lines = []

    defaults_map = {name: (value, comment) for name, value, comment in CALIBRATION_DEFAULTS}

    for line in lines:
        # Check for uncommented CALIBRATED_* markers — re-comment them
        marker_match = re.match(r'^(\s*)#define\s+(CALIBRATED_\w+)(.*)', line)
        if marker_match:
            indent = marker_match.group(1)
            marker_name = marker_match.group(2)
            trailing = marker_match.group(3)
            if marker_name in CALIBRATION_MARKERS:
                markers_reset.append(marker_name)
                new_lines.append(f"{indent}//#define {marker_name}{trailing}")
                continue

        # Match #define NAME VALUE pattern (with optional trailing comment)
        match = re.match(r'^(\s*#define\s+)(\w+)\s+(\S+)(.*)', line)
        if match:
            prefix = match.group(1)     # "    #define "
            name = match.group(2)       # "IMU_ACC_ERROR_X"
            old_value = match.group(3)  # "0.123f"
            trailing = match.group(4)   # "  // comment"

            if name in defaults_map:
                new_value, _comment = defaults_map[name]
                if old_value != new_value:
                    changes.append((name, old_value, new_value))
                    new_lines.append(f"{prefix}{name} {new_value}{trailing}")
                    continue

        new_lines.append(line)

    return '\n'.join(new_lines), changes, markers_reset


def main():
    parser = argparse.ArgumentParser(
        description="Reset calibration values in config.h to factory defaults."
    )
    parser.add_argument(
        "--config", "-c",
        help="Path to config.h (auto-detected if not specified)"
    )
    parser.add_argument(
        "--dry-run", "-n",
        action="store_true",
        help="Show what would change without modifying the file"
    )
    parser.add_argument(
        "--yes", "-y",
        action="store_true",
        help="Skip confirmation prompt"
    )
    args = parser.parse_args()

    # Find config.h
    config_path = args.config or find_config_h()
    if not os.path.isfile(config_path):
        print(f"Error: config.h not found at {config_path}")
        print("Use --config to specify the path manually.")
        sys.exit(1)

    print(f"Config file: {config_path}")

    # Read current content
    with open(config_path, 'r') as f:
        content = f.read()

    # Calculate changes
    new_content, changes, markers_reset = reset_defines(content)

    total = len(changes) + len(markers_reset)
    if total == 0:
        print("\nAll calibration values are already at defaults. Nothing to reset.")
        return

    # Show value changes
    if changes:
        print(f"\n{len(changes)} value(s) will be reset:\n")
        print(f"  {'Parameter':<25} {'Current':<15} {'Default':<15}")
        print(f"  {'-'*25} {'-'*15} {'-'*15}")
        for name, old_val, new_val in changes:
            print(f"  {name:<25} {old_val:<15} {new_val:<15}")

    # Show marker changes
    if markers_reset:
        print(f"\n{len(markers_reset)} status marker(s) will be re-commented:")
        for marker in markers_reset:
            print(f"  #define {marker} → //#define {marker}")

    if args.dry_run:
        print("\n[Dry run] No changes made.")
        return

    # Confirm
    if not args.yes:
        response = input(f"\nReset {total} item(s)? [y/N] ")
        if response.lower() not in ('y', 'yes'):
            print("Cancelled.")
            return

    # Write changes
    with open(config_path, 'w') as f:
        f.write(new_content)

    print(f"\nReset {len(changes)} calibration value(s) and {len(markers_reset)} status marker(s).")
    print("Next steps:")
    print("  1. Flash calibration build: pio run -e <board>_calibration --target upload")
    print("  2. Run 'a' for sequential calibration, or individual commands")
    print("  3. Copy output values to config.h, uncomment CALIBRATED_* markers")
    print("  4. Flash live build: pio run -e <board> --target upload")
    print("\nSee docs/features/calibration-guide.md for the full setup guide.")


if __name__ == "__main__":
    main()
