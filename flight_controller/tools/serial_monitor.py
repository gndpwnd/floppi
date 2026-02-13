#!/usr/bin/env python3
"""Simple serial monitor for testing flight controller calibration.

Usage:
    # Interactive mode - type commands, see output
    python3 serial_monitor.py /dev/ttyACM0

    # Send commands from args, capture output
    python3 serial_monitor.py /dev/ttyACM0 --send h --send c --wait 3

    # Save output to file
    python3 serial_monitor.py /dev/ttyACM0 --send h --wait 3 --output results/test.txt

    # Custom baud rate
    python3 serial_monitor.py /dev/ttyACM0 --baud 500000 --send h

    # Force-release a stuck port first
    python3 serial_monitor.py /dev/ttyACM0 --release
"""

import argparse
import os
import subprocess
import sys
import threading
import time

import serial


def release_port(port):
    """Force-release a serial port held by stale processes."""
    try:
        result = subprocess.run(
            ["fuser", port], capture_output=True, text=True, timeout=5
        )
        if result.stdout.strip():
            pids = result.stdout.strip().split()
            print(f"Port {port} held by PIDs: {pids}", file=sys.stderr)
            subprocess.run(["fuser", "-k", port], capture_output=True, timeout=5)
            time.sleep(2)
            print(f"Released {port}", file=sys.stderr)
        else:
            print(f"Port {port} is free", file=sys.stderr)
    except FileNotFoundError:
        print("fuser not available, skipping port release", file=sys.stderr)
    except subprocess.TimeoutExpired:
        print("Port release timed out", file=sys.stderr)


def reader_thread(ser, output_lines, stop_event):
    """Read serial data and print/store it."""
    while not stop_event.is_set():
        try:
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                text = data.decode("utf-8", errors="replace")
                sys.stdout.write(text)
                sys.stdout.flush()
                output_lines.append(text)
            else:
                time.sleep(0.01)
        except serial.SerialException:
            break
        except Exception:
            break


def main():
    parser = argparse.ArgumentParser(description="Serial monitor for flight controller")
    parser.add_argument("port", help="Serial port (e.g. /dev/ttyACM0)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--send", action="append", default=[], help="Command to send (repeatable)")
    parser.add_argument("--wait", type=float, default=2.0, help="Seconds to wait after each command (default: 2)")
    parser.add_argument("--boot-wait", type=float, default=3.0, help="Seconds to wait for boot (default: 3)")
    parser.add_argument("--output", "-o", help="Save output to file")
    parser.add_argument("--interactive", "-i", action="store_true", help="Stay in interactive mode after commands")
    parser.add_argument("--release", action="store_true", help="Force-release port before connecting")
    parser.add_argument("--no-dtr-reset", action="store_true", help="Skip DTR toggle reset")
    args = parser.parse_args()

    # Force-release if requested
    if args.release:
        release_port(args.port)

    # Open serial port
    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.1)
    except serial.SerialException as e:
        if "busy" in str(e).lower():
            print(f"Port busy! Try: python3 {sys.argv[0]} {args.port} --release", file=sys.stderr)
        else:
            print(f"Error opening {args.port}: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"Connected: {args.port} @ {args.baud} baud", file=sys.stderr)

    # Toggle DTR to reset the board (Teensy resets on DTR toggle)
    if not args.no_dtr_reset:
        ser.dtr = False
        time.sleep(0.1)
        ser.dtr = True

    output_lines = []
    stop_event = threading.Event()

    # Start reader thread
    reader = threading.Thread(target=reader_thread, args=(ser, output_lines, stop_event), daemon=True)
    reader.start()

    try:
        # Wait for board to boot
        print(f"Waiting {args.boot_wait}s for boot...", file=sys.stderr)
        time.sleep(args.boot_wait)

        if args.send:
            # Scripted mode: send commands with delays
            for cmd in args.send:
                print(f"\n>>> Sending: '{cmd}'", file=sys.stderr)
                ser.write((cmd + "\n").encode())
                ser.flush()
                time.sleep(args.wait)

            if not args.interactive:
                # Give a bit more time for final output
                time.sleep(0.5)
                stop_event.set()
            else:
                print("\n--- Interactive mode (Ctrl+C to exit) ---", file=sys.stderr)
                while True:
                    line = input()
                    ser.write((line + "\n").encode())
                    ser.flush()
        else:
            # Interactive mode
            print("Interactive mode (Ctrl+C to exit)", file=sys.stderr)
            while True:
                line = input()
                ser.write((line + "\n").encode())
                ser.flush()

    except (KeyboardInterrupt, EOFError):
        print("\nExiting...", file=sys.stderr)
    finally:
        stop_event.set()
        reader.join(timeout=1)
        ser.close()

    # Save output if requested
    if args.output:
        os.makedirs(os.path.dirname(args.output) or ".", exist_ok=True)
        full_output = "".join(output_lines)
        with open(args.output, "w") as f:
            f.write(full_output)
        print(f"Output saved to {args.output}", file=sys.stderr)


if __name__ == "__main__":
    main()
