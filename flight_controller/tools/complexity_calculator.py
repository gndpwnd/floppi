#!/usr/bin/env python3
"""
Flight Controller Complexity Calculator

Analyzes CPU timing, memory usage, and source code complexity for the flight
controller across different microcontroller platforms. Auto-detects features
from config.h and dynamically scans source code — no manual updates needed.

Usage:
    python3 complexity_calculator.py                        # CPU analysis (ESP32 default)
    python3 complexity_calculator.py -p teensy40             # Specific platform
    python3 complexity_calculator.py --all                   # Compare all platforms
    python3 complexity_calculator.py --clock 240 --cores 2 --fpu   # Custom hardware
    python3 complexity_calculator.py --memory                # Memory analysis (needs build)
    python3 complexity_calculator.py --source                # Source FP operation scan
    python3 complexity_calculator.py --full                  # Everything
    python3 complexity_calculator.py --builds                # List available builds
"""

import sys
import os
import argparse
from dataclasses import replace

# Add tools/ to path so complexity package is importable
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from complexity.platforms import Platform, PLATFORMS, PLATFORM_ARCH
from complexity.scanner import scan_config, find_config_path, find_project_root
from complexity.source_scanner import scan_all_sources
from complexity.build_analyzer import analyze_build, list_available_builds
from complexity.calc import calculate_loop_timing
from complexity.report import (
    print_cpu_report, print_memory_report, print_source_report,
    print_comparison, print_clock_requirements,
)


def _build_custom_platform(args):
    """Create a Platform from CLI flags (no preset needed)."""
    clock = args.clock or 240.0
    cores = args.cores or 1
    fpu = not args.no_fpu
    name = f"Custom ({clock:.0f} MHz, {cores} core{'s' if cores > 1 else ''}, {'FPU' if fpu else 'no FPU'})"
    return Platform(
        name=name, clock_mhz=clock, cores=cores, fpu=fpu,
        flash_kb=0, ram_kb=0,
    )


def _build_platform(args):
    """Build platform from preset + optional overrides."""
    plat = PLATFORMS[args.platform]
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
        fpu_str = "FPU" if overrides.get("fpu", plat.fpu) else "no FPU"
        cores = overrides.get("cores", plat.cores)
        clock = overrides.get("clock_mhz", plat.clock_mhz)
        overrides["name"] = f"{plat.name} (override: {clock:.0f} MHz, {cores}c, {fpu_str})"
        plat = replace(plat, **overrides)
    return plat


def _platform_key(args):
    """Determine platform key for cycle profile lookup."""
    if args.platform:
        return args.platform
    # Custom hardware: use generic key based on FPU
    return "custom"


def main():
    parser = argparse.ArgumentParser(
        description="FC complexity calculator — CPU timing, memory usage, source analysis",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""examples:
  %(prog)s                          CPU analysis for ESP32
  %(prog)s -p teensy40              CPU analysis for Teensy 4.0
  %(prog)s --all                    Compare all platforms
  %(prog)s --clock 133 --no-fpu     Custom hardware (RP2040-like)
  %(prog)s --memory                 Memory usage from last build
  %(prog)s --full                   CPU + memory + source scan
  %(prog)s --builds                 List available build artifacts"""
    )

    # Platform selection
    parser.add_argument(
        "-p", "--platform", default=None,
        choices=list(PLATFORMS.keys()),
        help="Platform preset (default: esp32)"
    )

    # Custom hardware
    parser.add_argument(
        "--clock", type=float, default=None,
        help="Clock speed in MHz (uses preset default or 240)"
    )
    parser.add_argument(
        "--cores", type=int, default=None,
        help="Core count (default: from preset or 1)"
    )
    fpu_group = parser.add_mutually_exclusive_group()
    fpu_group.add_argument(
        "--fpu", action="store_true", default=None,
        help="Hardware FPU (default)"
    )
    fpu_group.add_argument(
        "--no-fpu", action="store_true",
        help="Software float (no FPU)"
    )

    # Analysis modes
    parser.add_argument(
        "--all", action="store_true",
        help="Compare all platform presets"
    )
    parser.add_argument(
        "--memory", action="store_true",
        help="Analyze memory from build artifacts (requires pio run)"
    )
    parser.add_argument(
        "--source", action="store_true",
        help="Show source code FP operation scan"
    )
    parser.add_argument(
        "--full", action="store_true",
        help="Full analysis: CPU + memory + source scan"
    )
    parser.add_argument(
        "--builds", action="store_true",
        help="List available build environments"
    )

    # Build environment for memory analysis
    parser.add_argument(
        "-e", "--env", default=None,
        help="Build environment for memory analysis (default: matches platform)"
    )

    # Config
    parser.add_argument(
        "--config", default=None,
        help="Path to config.h (default: auto-detect)"
    )

    args = parser.parse_args()

    # --full enables all analysis modes
    if args.full:
        args.memory = True
        args.source = True

    # Find project root and config
    project_root = find_project_root()

    config_path = args.config
    if config_path is None:
        config_path = find_config_path()
    if config_path is None or not os.path.exists(config_path):
        print("Error: config.h not found. Use --config to specify path.", file=sys.stderr)
        sys.exit(1)

    # Default platform
    if args.platform is None and args.clock is None and args.cores is None:
        args.platform = "esp32"

    # Determine platform key for config scanning
    platform_key = args.platform or "esp32"

    # Scan config.h
    features = scan_config(config_path, platform_key)

    # --builds: list available build artifacts and exit
    if args.builds:
        envs = list_available_builds(project_root)
        if envs:
            print(f"\nAvailable builds ({len(envs)}):")
            for env in envs:
                elf = os.path.join(project_root, ".pio", "build", env, "firmware.elf")
                age_s = __import__("time").time() - os.path.getmtime(elf)
                age = f"{age_s / 3600:.1f}h ago" if age_s > 3600 else f"{age_s / 60:.0f}m ago"
                print(f"  {env:<30} (built {age})")
        else:
            print("\nNo builds found. Run: pio run -e <env>")
        return

    # Scan source files for FP operations
    per_file, op_totals = scan_all_sources(project_root)

    # --all: comparison mode
    if args.all:
        build_results = None
        if args.memory:
            build_results = []
            for env in list_available_builds(project_root):
                build_results.append(analyze_build(project_root, env))

        print_comparison(features, op_totals, build_results)
        print_clock_requirements(features, op_totals)

        if args.source:
            print_source_report(per_file, op_totals)
        return

    # Single platform mode
    if args.platform:
        platform = _build_platform(args)
        platform_key = args.platform
    else:
        platform = _build_custom_platform(args)
        platform_key = "custom"

    # CPU timing analysis
    timing = calculate_loop_timing(platform, platform_key, features, op_totals)
    print_cpu_report(timing, platform, platform_key, features)
    print_clock_requirements(features, op_totals)

    # Memory analysis
    if args.memory:
        env_name = args.env
        if env_name is None:
            # Try to match platform to build env
            env_name = args.platform or "esp32"
        build_data = analyze_build(project_root, env_name)
        print_memory_report(build_data)

    # Source scan
    if args.source:
        print_source_report(per_file, op_totals)


if __name__ == "__main__":
    main()
