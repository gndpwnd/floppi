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

    if args.all:
        print_comparison(features)
    elif args.breakdown:
        platform = PLATFORMS[args.platform]
        if args.clock:
            platform = replace(platform, clock_mhz=args.clock)
        print_breakdown(platform, features)
    else:
        platform = PLATFORMS[args.platform]
        if args.clock:
            platform = replace(platform, clock_mhz=args.clock)
        print_check(args.platform, platform, features)


if __name__ == "__main__":
    main()
