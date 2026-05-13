#!/usr/bin/env python3
"""
replay_trajectory.py

Replay a CSV pitch trajectory over a serial link to a connected MCU, and
concurrently capture whatever the firmware sends back. Used for
hardware-in-the-loop testing of the balance robot: feed synthetic pitch
data into the firmware as if it came from the IMU, then record the
motor-side response over the same UART.

CSV format (produced by generate_balance_trajectory.py):

    timestamp_ms,raw_pitch_deg,corrected_pitch_deg,pid_output,left_pwm,right_pwm

Only the first two columns (`timestamp_ms`, `raw_pitch_deg`) are
required; extra columns are ignored, so any CSV with at least those
two named columns can be replayed.

Protocols on the wire:

    text   (default)  ASCII line per row:   "pitch:%.3f\\n"
    binary             4-byte little-endian IEEE 754 float per row

Status: production-ready (replay side). Firmware-side decoders for the
text protocol exist; the binary protocol is reserved for future firmware
work.

NOTE FOR LINUX OPERATORS: ModemManager probes USB CDC devices on
attach and corrupts the first ~10 s of serial traffic. If reads look
truncated or garbled, stop it before running:

    sudo systemctl stop ModemManager

Usage:
    replay_trajectory.py --csv tests/data/balancing_reference_trajectory.csv \\
                         --port /dev/ttyACM0
    replay_trajectory.py --csv traj.csv --port /dev/ttyACM0 \\
                         --rate-multiplier 0.5 --output captured.txt
    replay_trajectory.py --csv traj.csv --port /dev/ttyACM0 \\
                         --protocol binary --quiet
"""

from __future__ import annotations

import argparse
import csv
import signal
import struct
import sys
import threading
import time
from pathlib import Path


# ----------------------------------------------------------------------
# pyserial import with a friendly install hint
# ----------------------------------------------------------------------
try:
    import serial  # type: ignore
except ImportError:
    print(
        "ERROR: pyserial is not installed.\n"
        "Install with:\n"
        "    python3 -m pip install --user pyserial\n"
        "or your distro's package, e.g. `sudo apt install python3-serial`.",
        file=sys.stderr,
    )
    sys.exit(1)


# ----------------------------------------------------------------------
# Globals for graceful Ctrl-C handling
# ----------------------------------------------------------------------
_stop_flag = threading.Event()


def _handle_sigint(signum, frame):  # noqa: ARG001
    _stop_flag.set()


# ----------------------------------------------------------------------
# CSV loading
# ----------------------------------------------------------------------
def load_trajectory(csv_path: Path) -> list[tuple[int, float]]:
    """
    Return a list of (timestamp_ms, raw_pitch_deg) tuples from the CSV.
    Requires at least those two columns; extra columns are ignored.
    """
    rows: list[tuple[int, float]] = []
    with csv_path.open("r", newline="") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames is None:
            raise ValueError(f"{csv_path}: CSV has no header row")
        missing = {"timestamp_ms", "raw_pitch_deg"} - set(reader.fieldnames)
        if missing:
            raise ValueError(
                f"{csv_path}: CSV missing required column(s): {sorted(missing)}"
            )
        for i, row in enumerate(reader, start=2):  # line 2 = first data row
            try:
                t_ms = int(float(row["timestamp_ms"]))
                pitch = float(row["raw_pitch_deg"])
            except (KeyError, ValueError) as e:
                raise ValueError(f"{csv_path}:{i}: bad row: {e}") from e
            rows.append((t_ms, pitch))
    if not rows:
        raise ValueError(f"{csv_path}: no data rows")
    return rows


# ----------------------------------------------------------------------
# Reader thread: drain the serial port and forward to output + stdout
# ----------------------------------------------------------------------
def reader_loop(
    ser,
    out_file,
    quiet: bool,
    counter: list,
) -> None:
    """
    Continuously read from `ser` until `_stop_flag` is set. Each
    decoded line is written to `out_file` (if not None) and to stdout
    (unless `quiet`). `counter[0]` is incremented per line received.
    """
    buf = bytearray()
    while not _stop_flag.is_set():
        try:
            chunk = ser.read(256)  # blocking up to ser.timeout
        except Exception as e:  # serial closed under us, etc.
            if not quiet:
                print(f"[reader] serial read error: {e}", file=sys.stderr)
            return
        if not chunk:
            continue
        buf.extend(chunk)
        while b"\n" in buf:
            line, _, rest = buf.partition(b"\n")
            buf = bytearray(rest)
            try:
                text = line.decode("utf-8", errors="replace").rstrip("\r")
            except Exception:
                text = repr(bytes(line))
            counter[0] += 1
            if out_file is not None:
                out_file.write(text + "\n")
                out_file.flush()
            if not quiet:
                print(text, flush=True)


# ----------------------------------------------------------------------
# Writer: pace the CSV and send each row
# ----------------------------------------------------------------------
def encode_row(pitch_deg: float, protocol: str) -> bytes:
    if protocol == "text":
        return f"pitch:{pitch_deg:.3f}\n".encode("ascii")
    if protocol == "binary":
        # 4-byte little-endian IEEE 754 float, no framing — firmware
        # is expected to know a fixed-length record arrives.
        return struct.pack("<f", pitch_deg)
    raise ValueError(f"unknown protocol: {protocol}")


def replay(
    ser,
    rows: list[tuple[int, float]],
    rate_multiplier: float,
    protocol: str,
    quiet: bool,
) -> int:
    """
    Send all rows on the wire, sleeping to honor each row's timestamp
    scaled by `rate_multiplier`. Returns count of rows actually sent.
    """
    if not rows:
        return 0
    t_start_wall = time.monotonic()
    t_first_ms = rows[0][0]
    sent = 0

    for i, (t_ms, pitch) in enumerate(rows):
        if _stop_flag.is_set():
            break

        # Target wall-clock offset for this row.
        target_s = ((t_ms - t_first_ms) / 1000.0) * rate_multiplier
        now_s = time.monotonic() - t_start_wall
        sleep_s = target_s - now_s
        if sleep_s > 0:
            # Sleep in slices so Ctrl-C is responsive.
            end_at = time.monotonic() + sleep_s
            while not _stop_flag.is_set():
                remaining = end_at - time.monotonic()
                if remaining <= 0:
                    break
                time.sleep(min(remaining, 0.05))
            if _stop_flag.is_set():
                break

        try:
            ser.write(encode_row(pitch, protocol))
        except Exception as e:
            if not quiet:
                print(f"[writer] serial write error: {e}", file=sys.stderr)
            break
        sent += 1

        if not quiet and (i % 100 == 0):
            print(
                f"[writer] sent {sent}/{len(rows)} rows "
                f"(t={t_ms} ms, pitch={pitch:.3f} deg)",
                file=sys.stderr,
            )

    try:
        ser.flush()
    except Exception:
        pass
    return sent


# ----------------------------------------------------------------------
# CLI
# ----------------------------------------------------------------------
def parse_args(argv: list) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description=(
            "Replay a CSV pitch trajectory over serial and capture "
            "the firmware's response. CSV must have columns "
            "`timestamp_ms` and `raw_pitch_deg` (produced by "
            "generate_balance_trajectory.py)."
        ),
    )
    p.add_argument(
        "--csv",
        required=True,
        help="Path to the trajectory CSV.",
    )
    p.add_argument(
        "--port",
        required=True,
        help="Serial device, e.g. /dev/ttyACM0.",
    )
    p.add_argument(
        "--baud",
        type=int,
        default=115200,
        help="Baud rate (default: 115200).",
    )
    p.add_argument(
        "--rate-multiplier",
        type=float,
        default=1.0,
        help=(
            "Time scale: 1.0 = real-time, 0.5 = half-speed (rows are "
            "sent twice as far apart), 2.0 = double-speed. Default: 1.0."
        ),
    )
    p.add_argument(
        "--output",
        default=None,
        help=(
            "File path to write the firmware's captured serial output. "
            "If omitted, captured output only goes to stdout."
        ),
    )
    p.add_argument(
        "--protocol",
        choices=["text", "binary"],
        default="text",
        help=(
            "Wire format: `text` sends `pitch:%%.3f\\n` per row; "
            "`binary` sends a 4-byte LE float per row. Default: text."
        ),
    )
    p.add_argument(
        "--quiet",
        action="store_true",
        help="Suppress per-batch stderr progress messages.",
    )
    return p.parse_args(argv)


def main(argv: list) -> int:
    args = parse_args(argv)

    if args.rate_multiplier <= 0:
        print("ERROR: --rate-multiplier must be > 0", file=sys.stderr)
        return 2

    csv_path = Path(args.csv)
    if not csv_path.is_file():
        print(f"ERROR: CSV not found: {csv_path}", file=sys.stderr)
        return 2

    try:
        rows = load_trajectory(csv_path)
    except ValueError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 2

    if not args.quiet:
        print(
            f"[main] loaded {len(rows)} rows from {csv_path}",
            file=sys.stderr,
        )
        print(
            f"[main] opening {args.port} @ {args.baud} baud, "
            f"protocol={args.protocol}, rate_x={args.rate_multiplier}",
            file=sys.stderr,
        )

    # Install Ctrl-C handler before opening the port.
    signal.signal(signal.SIGINT, _handle_sigint)

    try:
        ser = serial.Serial(
            args.port,
            args.baud,
            timeout=0.2,
            write_timeout=2.0,
        )
    except serial.SerialException as e:
        print(f"ERROR: failed to open {args.port}: {e}", file=sys.stderr)
        return 1

    # Give USB CDC a moment to settle (Teensy/ESP32 reboot on DTR toggle
    # on some platforms; harmless otherwise).
    time.sleep(0.2)

    out_fh = None
    if args.output:
        try:
            out_fh = open(args.output, "w", encoding="utf-8")
        except OSError as e:
            print(
                f"ERROR: cannot open --output {args.output}: {e}",
                file=sys.stderr,
            )
            ser.close()
            return 1

    rx_counter = [0]
    t0 = time.monotonic()
    reader_thread = threading.Thread(
        target=reader_loop,
        args=(ser, out_fh, args.quiet, rx_counter),
        daemon=True,
        name="serial-reader",
    )
    reader_thread.start()

    sent = 0
    try:
        sent = replay(
            ser,
            rows,
            args.rate_multiplier,
            args.protocol,
            args.quiet,
        )
    except Exception as e:
        print(f"ERROR during replay: {e}", file=sys.stderr)
    finally:
        # Drain firmware tail-end output before tearing down.
        time.sleep(0.25)
        _stop_flag.set()
        reader_thread.join(timeout=1.0)
        try:
            ser.close()
        except Exception:
            pass
        if out_fh is not None:
            try:
                out_fh.close()
            except Exception:
                pass

    elapsed = time.monotonic() - t0
    print(
        f"[summary] rows sent: {sent}/{len(rows)}, "
        f"lines received: {rx_counter[0]}, "
        f"duration: {elapsed:.2f} s",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
