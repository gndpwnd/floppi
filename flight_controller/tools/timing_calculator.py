#!/usr/bin/env python3
"""
Flight Controller Timing Calculator

Calculates loop timing, minimum clock speed requirements, and CPU utilization
for the flight controller across different microcontroller platforms.

Auto-detects enabled features from config.h.

Usage:
    python3 timing_calculator.py                    # Check ESP32 (default)
    python3 timing_calculator.py -p teensy40        # Check specific platform
    python3 timing_calculator.py --all              # Compare all platforms
    python3 timing_calculator.py --breakdown        # Phase-by-phase detail
    python3 timing_calculator.py --clock 133        # Override clock speed
"""

import sys
import os
import argparse
from dataclasses import replace

# Add tools/ to path so timing package is importable
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from timing.platforms import PLATFORMS
from timing.scanner import scan_config, find_config_path
from timing.report import print_check, print_comparison, print_breakdown


def main():
    parser = argparse.ArgumentParser(
        description="FC timing calculator — loop timing & clock requirements"
    )
    parser.add_argument(
        "-p", "--platform", default="esp32",
        choices=list(PLATFORMS.keys()),
        help="Target platform (default: esp32)"
    )
    parser.add_argument(
        "--all", action="store_true",
        help="Compare all platforms"
    )
    parser.add_argument(
        "--breakdown", action="store_true",
        help="Show phase-by-phase breakdown only"
    )
    parser.add_argument(
        "--clock", type=float, default=None,
        help="Override clock speed (MHz)"
    )
    parser.add_argument(
        "--cores", type=int, default=None,
        help="Override core count (default: from platform preset)"
    )
    fpu_group = parser.add_mutually_exclusive_group()
    fpu_group.add_argument(
        "--fpu", action="store_true", default=None,
        help="Platform has FPU (default for most presets)"
    )
    fpu_group.add_argument(
        "--no-fpu", action="store_true",
        help="Platform has no FPU (software float)"
    )
    parser.add_argument(
        "--config", default=None,
        help="Path to config.h (default: auto-detect)"
    )
    args = parser.parse_args()

    # Find config.h
    config_path = args.config
    if config_path is None:
        config_path = find_config_path()
    if config_path is None or not os.path.exists(config_path):
        print(f"Error: config.h not found. Use --config to specify path.",
              file=sys.stderr)
        sys.exit(1)

    # Scan config.h for features
    features = scan_config(config_path, args.platform)

    # Build platform: start from preset, apply overrides
    def build_platform(preset_key):
        plat = PLATFORMS[preset_key]
        overrides = {}
        if args.clock:
            overrides["clock_mhz"] = args.clock
        if args.cores is not None:
            overrides["cores"] = args.cores
        if args.no_fpu:
            overrides["fpu"] = False
        elif args.fpu:
            overrides["fpu"] = True
        if overrides:
            # Update name if hardware is overridden
            fpu_str = "FPU" if overrides.get("fpu", plat.fpu) else "no FPU"
            cores = overrides.get("cores", plat.cores)
            clock = overrides.get("clock_mhz", plat.clock_mhz)
            overrides["name"] = f"Custom ({clock:.0f} MHz, {cores} core{'s' if cores > 1 else ''}, {fpu_str})"
            plat = replace(plat, **overrides)
        return plat

    if args.all:
        print_comparison(features)
    elif args.breakdown:
        platform = build_platform(args.platform)
        print_breakdown(platform, features)
    else:
        platform = build_platform(args.platform)
        print_check(args.platform, platform, features)


if __name__ == "__main__":
    main()
