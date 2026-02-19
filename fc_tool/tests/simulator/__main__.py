"""CLI entry point for running simulator as a package: python3 -m simulator"""

import argparse
import os
import sys

from .core import open_serial_write
from . import SCENARIOS


def main():
    parser = argparse.ArgumentParser(
        description="Serial data simulator for fc_tool testing"
    )
    parser.add_argument(
        "port",
        nargs="?",
        help="Serial port to write to (e.g. /tmp/vserial1). Omit with --stdout.",
    )
    parser.add_argument(
        "--stdout",
        action="store_true",
        help="Write to stdout instead of a serial port",
    )
    parser.add_argument(
        "--scenario", "-s",
        default="imu",
        choices=list(SCENARIOS.keys()),
        help="Data scenario to simulate (default: imu)",
    )
    parser.add_argument(
        "--rate", "-r",
        type=int,
        default=50,
        help="Lines per second (default: 50)",
    )
    parser.add_argument(
        "--duration", "-d",
        type=float,
        default=10.0,
        help="Duration in seconds (default: 10)",
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=115200,
        help="Baud rate for serial port (default: 115200)",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="List available scenarios and exit",
    )
    args = parser.parse_args()

    if args.list:
        print("Available scenarios:")
        for name, (_, desc) in sorted(SCENARIOS.items()):
            print(f"  {name:12s}  {desc}")
        return

    if not args.port and not args.stdout:
        parser.error("Specify a port or use --stdout")

    if args.stdout:
        fd = "stdout"
        print(f"Simulator: scenario={args.scenario} rate={args.rate}Hz "
              f"duration={args.duration}s → stdout", file=sys.stderr)
    else:
        fd = open_serial_write(args.port, args.baud)
        print(f"Simulator: scenario={args.scenario} rate={args.rate}Hz "
              f"duration={args.duration}s → {args.port} @ {args.baud}", file=sys.stderr)

    scenario_fn, _ = SCENARIOS[args.scenario]
    try:
        scenario_fn(fd, args.rate, args.duration)
    except KeyboardInterrupt:
        print("\nSimulator stopped", file=sys.stderr)
    finally:
        if fd != "stdout":
            os.close(fd)


if __name__ == "__main__":
    main()
