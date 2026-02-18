#!/usr/bin/env python3
"""Serial monitor for flight controller calibration and testing.

Works with both Teensy USB CDC and ESP32 USB-UART (CP2102/CH340).

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

    # Skip DTR reset (don't reboot the board on connect)
    python3 serial_monitor.py /dev/ttyACM0 --no-dtr-reset

    # Wait for specific pattern in output (useful for boot detection)
    python3 serial_monitor.py /dev/ttyACM0 --wait-for "READY" --timeout 15

Teensy USB CDC notes:
    Standard pyserial in_waiting polling doesn't work with Teensy USB CDC
    because TIOCINQ queries the tty buffer, not the USB endpoint buffer.
    This script uses select() + os.read() with cfmakeraw()-equivalent
    termios settings (VMIN=1, HUPCL cleared) to match serialport-rs behavior.
"""

import argparse
import fcntl
import os
import select
import struct
import subprocess
import sys
import termios
import threading
import time


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


def open_serial(port, baud=115200, dtr_reset=True):
    """Open serial port with cfmakeraw()-equivalent termios settings.

    This matches the behavior of Rust's serialport-rs crate, which works
    reliably with Teensy USB CDC. Key differences from stock pyserial:
    - HUPCL cleared (prevents DTR drops and stale CDC state)
    - VMIN=1, VTIME=0 (blocking read waits for USB transfer, not tty poll)
    - iflag=0, lflag=0 (equivalent to cfmakeraw, no leftover tty flags)
    - Single clean DTR assertion (no toggle)
    """
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)

    try:
        attrs = termios.tcgetattr(fd)

        # cfmakeraw() equivalent — zero all processing flags
        attrs[0] = 0  # iflag: no input processing
        attrs[1] = 0  # oflag: no output processing
        attrs[3] = 0  # lflag: no local processing (no echo, no canonical)

        # cflag: 8N1 + CLOCAL + CREAD, explicitly clear HUPCL
        attrs[2] &= ~(termios.CSIZE | termios.PARENB | termios.CSTOPB | termios.HUPCL)
        attrs[2] |= termios.CS8 | termios.CLOCAL | termios.CREAD

        # Set baud rate
        baud_const = getattr(termios, f"B{baud}", None)
        if baud_const is None:
            print(f"Warning: non-standard baud {baud}, trying anyway", file=sys.stderr)
            baud_const = baud
        attrs[4] = baud_const  # ispeed
        attrs[5] = baud_const  # ospeed

        # VMIN=1, VTIME=0: block in read() until at least 1 byte arrives.
        attrs[6][termios.VMIN] = 1
        attrs[6][termios.VTIME] = 0

        # Apply atomically
        termios.tcsetattr(fd, termios.TCSANOW, attrs)

        # Clear O_NONBLOCK for normal blocking reads
        flags = fcntl.fcntl(fd, fcntl.F_GETFL)
        fcntl.fcntl(fd, fcntl.F_SETFL, flags & ~os.O_NONBLOCK)

        # Flush any stale data in kernel buffers
        termios.tcflush(fd, termios.TCIOFLUSH)

        # DTR handling
        TIOCMBIS = 0x5416  # Set modem bits
        TIOCMBIC = 0x5417  # Clear modem bits
        TIOCM_DTR = 0x002

        if dtr_reset:
            # Toggle DTR to reset the board
            fcntl.ioctl(fd, TIOCMBIC, struct.pack("I", TIOCM_DTR))
            time.sleep(0.1)
            fcntl.ioctl(fd, TIOCMBIS, struct.pack("I", TIOCM_DTR))
        else:
            # Just assert DTR without toggle
            fcntl.ioctl(fd, TIOCMBIS, struct.pack("I", TIOCM_DTR))

    except Exception:
        os.close(fd)
        raise

    return fd


def read_serial(fd, timeout=0.1):
    """Read available bytes from serial fd using select() for timeout."""
    ready, _, _ = select.select([fd], [], [], timeout)
    if ready:
        try:
            return os.read(fd, 4096)
        except OSError:
            return b""
    return b""


def write_serial(fd, data):
    """Write string or bytes to serial fd."""
    if isinstance(data, str):
        data = data.encode()
    os.write(fd, data)


def drain_until_quiet(fd, output_lines, quiet_time=0.5, max_time=5.0):
    """Read serial data until silence for quiet_time seconds or max_time elapsed.

    Returns the text captured during the drain.
    """
    captured = []
    start = time.monotonic()
    last_data = start

    while True:
        now = time.monotonic()
        if now - start > max_time:
            break
        if now - last_data > quiet_time:
            break

        remaining = min(quiet_time - (now - last_data), 0.1)
        ready, _, _ = select.select([fd], [], [], max(remaining, 0.01))
        if ready:
            try:
                data = os.read(fd, 4096)
                if data:
                    text = data.decode("utf-8", errors="replace")
                    sys.stdout.write(text)
                    sys.stdout.flush()
                    output_lines.append(text)
                    captured.append(text)
                    last_data = time.monotonic()
                else:
                    break  # EOF
            except OSError:
                break

    return "".join(captured)


def reader_thread(fd, output_lines, stop_event):
    """Read serial data using select() + os.read() (works with Teensy USB CDC)."""
    while not stop_event.is_set():
        try:
            ready, _, _ = select.select([fd], [], [], 0.1)
            if ready:
                data = os.read(fd, 4096)
                if data:
                    text = data.decode("utf-8", errors="replace")
                    sys.stdout.write(text)
                    sys.stdout.flush()
                    output_lines.append(text)
                else:
                    # EOF — device disconnected
                    break
        except OSError:
            break


def main():
    parser = argparse.ArgumentParser(
        description="Serial monitor for flight controller (Teensy + ESP32)"
    )
    parser.add_argument("port", help="Serial port (e.g. /dev/ttyACM0)")
    parser.add_argument(
        "--baud", type=int, default=115200, help="Baud rate (default: 115200)"
    )
    parser.add_argument(
        "--send", action="append", default=[], help="Command to send (repeatable)"
    )
    parser.add_argument(
        "--wait",
        type=float,
        default=2.0,
        help="Seconds to wait after each command (default: 2)",
    )
    parser.add_argument(
        "--boot-wait",
        type=float,
        default=3.0,
        help="Seconds to wait for boot (default: 3)",
    )
    parser.add_argument("--output", "-o", help="Save output to file")
    parser.add_argument(
        "--interactive",
        "-i",
        action="store_true",
        help="Stay in interactive mode after commands",
    )
    parser.add_argument(
        "--release",
        action="store_true",
        help="Force-release port before connecting",
    )
    parser.add_argument(
        "--no-dtr-reset",
        action="store_true",
        help="Don't toggle DTR to reset the board",
    )
    parser.add_argument(
        "--wait-for",
        help="Wait until output contains this pattern (regex), then exit",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=30.0,
        help="Max seconds to wait with --wait-for (default: 30)",
    )
    parser.add_argument(
        "--quiet",
        "-q",
        action="store_true",
        help="Suppress stdout output (still saves to --output file)",
    )
    args = parser.parse_args()

    # Force-release if requested
    if args.release:
        release_port(args.port)

    # Open serial port with proper termios settings
    try:
        fd = open_serial(args.port, args.baud, dtr_reset=not args.no_dtr_reset)
    except OSError as e:
        if "busy" in str(e).lower() or e.errno == 16:
            print(
                f"Port busy! Try: python3 {sys.argv[0]} {args.port} --release",
                file=sys.stderr,
            )
        else:
            print(f"Error opening {args.port}: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"Connected: {args.port} @ {args.baud} baud", file=sys.stderr)

    output_lines = []
    stop_event = threading.Event()

    # Suppress stdout if --quiet
    original_stdout = sys.stdout
    if args.quiet:
        sys.stdout = open(os.devnull, "w")

    # Start reader thread
    reader = threading.Thread(
        target=reader_thread, args=(fd, output_lines, stop_event), daemon=True
    )
    reader.start()

    try:
        if args.wait_for:
            # Wait-for-pattern mode: read until pattern found or timeout
            import re

            pattern = re.compile(args.wait_for)
            print(
                f"Waiting for pattern: '{args.wait_for}' (timeout: {args.timeout}s)",
                file=sys.stderr,
            )
            start = time.monotonic()
            while time.monotonic() - start < args.timeout:
                time.sleep(0.2)
                full = "".join(output_lines)
                if pattern.search(full):
                    print(f"Pattern found after {time.monotonic()-start:.1f}s", file=sys.stderr)
                    # Drain remaining output
                    time.sleep(0.5)
                    break
            else:
                print(f"Timeout waiting for pattern", file=sys.stderr)
        elif args.send:
            # Wait for board to boot
            if args.boot_wait > 0:
                print(f"Waiting {args.boot_wait}s for boot...", file=sys.stderr)
                time.sleep(args.boot_wait)

            # Scripted mode: send commands with delays
            for cmd in args.send:
                print(f"\n>>> Sending: '{cmd}'", file=sys.stderr)
                write_serial(fd, cmd + "\n")
                time.sleep(args.wait)

            if args.interactive:
                print(
                    "\n--- Interactive mode (Ctrl+C to exit) ---", file=sys.stderr
                )
                if args.quiet:
                    sys.stdout = original_stdout
                while True:
                    line = input()
                    write_serial(fd, line + "\n")
            else:
                # Drain until quiet — captures any remaining response data
                stop_event.set()
                reader.join(timeout=1)
                drain_until_quiet(fd, output_lines, quiet_time=0.5, max_time=2.0)
        else:
            # Wait for board to boot
            if args.boot_wait > 0:
                print(f"Waiting {args.boot_wait}s for boot...", file=sys.stderr)
                time.sleep(args.boot_wait)

            # Interactive mode
            if args.quiet:
                sys.stdout = original_stdout
            print("Interactive mode (Ctrl+C to exit)", file=sys.stderr)
            while True:
                line = input()
                write_serial(fd, line + "\n")

    except (KeyboardInterrupt, EOFError):
        print("\nExiting...", file=sys.stderr)
    finally:
        stop_event.set()
        reader.join(timeout=1)
        os.close(fd)

    # Restore stdout if suppressed
    if args.quiet:
        sys.stdout = original_stdout

    # Save output if requested
    if args.output:
        os.makedirs(os.path.dirname(args.output) or ".", exist_ok=True)
        full_output = "".join(output_lines)
        with open(args.output, "w") as f:
            f.write(full_output)
        print(f"Output saved to {args.output} ({len(full_output)} bytes)", file=sys.stderr)

    # Exit with error if no output was captured (helps test scripts)
    if args.send and not output_lines:
        print("WARNING: no output captured", file=sys.stderr)
        sys.exit(2)


if __name__ == "__main__":
    main()
