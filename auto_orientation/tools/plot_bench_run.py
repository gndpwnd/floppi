#!/usr/bin/env python3
"""
plot_bench_run.py

Host-side analysis and plotting for AO Mega balance-robot bench telemetry
(Workstream G item G3).

The Mega 'g' serial command emits ONE flat comma-delimited line per poll,
exactly 10 fields, fixed arity:

    G,<millis>,<pitch_deg>,<pitch_sp_deg>,<wheel_vel_mps>,<position_m>,\
<nudge_deg>,<k_pos>,<k_vel>,<pos_leak>

  - Field 0 is the literal tag `G`.
  - Field 1 is millis() (uint, ms).
  - Fields 2-9 are floats.

On a no-encoder (Uno) build the five outer-loop fields print as 0 but the
arity stays 10. A capture file is many such lines (the operator polls 'g'
repeatedly) and may be interleaved with other serial output, so any line
that does not start with `G,` is skipped.

This tool:
  1. Parses a capture file (path arg, or stdin), skipping non-`G,` lines
     and tolerating a partial trailing line.
  2. Plots time-series (x = millis normalized to start = 0): pitch_deg
     overlaid with pitch_sp_deg, wheel_vel_mps, position_m, nudge_deg.
  3. Prints a per-channel summary table (min/max/mean/stddev), sample
     count, capture duration and effective sample rate.
  4. Reports the three derived gains (k_pos / k_vel / pos_leak), which are
     normally constant; warns if any change mid-run (a re-derivation event).

Usage:
    # show the plot interactively
    tools/plot_bench_run.py capture.txt

    # read from a live serial capture via stdin
    tools/serial_monitor.py /dev/ttyACM0 | tools/plot_bench_run.py -

    # write the plot to a PNG instead of showing it (no display needed)
    tools/plot_bench_run.py capture.txt --save run.png

    # summary table only, no plot (works headless)
    tools/plot_bench_run.py capture.txt --no-plot

Standard library + matplotlib only. If matplotlib is missing, --no-plot
still works; plotting modes report the missing dependency and exit 1.
"""

from __future__ import annotations

import argparse
import statistics
import sys

# Field layout of a parsed telemetry record (after the leading `G` tag).
TIME_FIELD = "millis"
PLOT_CHANNELS = ("pitch_deg", "pitch_sp_deg", "wheel_vel_mps",
                 "position_m", "nudge_deg")
GAIN_CHANNELS = ("k_pos", "k_vel", "pos_leak")
# Order matches fields 1..9 of the `G,...` line.
DATA_CHANNELS = (TIME_FIELD,) + PLOT_CHANNELS + GAIN_CHANNELS
EXPECTED_FIELDS = 1 + len(DATA_CHANNELS)  # tag + 9 values = 10


class CaptureError(Exception):
    """Raised for any user-facing capture/parse failure (no traceback)."""


def parse_capture(lines):
    """Parse an iterable of text lines into a list of record dicts.

    Lines not starting with ``G,`` are silently skipped. A line with the
    wrong field count or an unparseable value is counted as malformed and
    skipped (the count is returned for reporting). A partial trailing line
    is tolerated -- it simply fails the field-count check.

    Returns (records, stats) where stats has keys: total_lines,
    skipped_other, malformed.
    """
    records = []
    total_lines = 0
    skipped_other = 0
    malformed = 0

    for raw in lines:
        line = raw.strip()
        if not line:
            continue
        total_lines += 1
        if not line.startswith("G,"):
            skipped_other += 1
            continue
        fields = line.split(",")
        if len(fields) != EXPECTED_FIELDS:
            malformed += 1
            continue
        try:
            values = [float(x) for x in fields[1:]]
        except ValueError:
            malformed += 1
            continue
        record = dict(zip(DATA_CHANNELS, values))
        records.append(record)

    stats = {
        "total_lines": total_lines,
        "skipped_other": skipped_other,
        "malformed": malformed,
    }
    return records, stats


def channel_series(records, channel):
    """Return the list of values for one channel across all records."""
    return [r[channel] for r in records]


def summarize_channel(values):
    """Return a min/max/mean/stddev dict for a list of values."""
    n = len(values)
    return {
        "n": n,
        "min": min(values),
        "max": max(values),
        "mean": statistics.fmean(values),
        "stddev": statistics.pstdev(values) if n > 1 else 0.0,
    }


def gain_summary(records):
    """Inspect the three derived gains.

    Returns (constant_values, changed) where constant_values maps each
    gain channel to its first observed value and ``changed`` is the set of
    channels that take more than one distinct value across the run.
    """
    constant_values = {}
    changed = set()
    for channel in GAIN_CHANNELS:
        series = channel_series(records, channel)
        constant_values[channel] = series[0]
        if len(set(series)) > 1:
            changed.add(channel)
    return constant_values, changed


def print_summary(records, parse_stats):
    """Print the human-readable summary table to stdout."""
    times = channel_series(records, TIME_FIELD)
    t0, t1 = times[0], times[-1]
    duration_ms = t1 - t0
    n = len(records)
    # Effective rate uses the span between first and last sample.
    if duration_ms > 0 and n > 1:
        rate_hz = (n - 1) / (duration_ms / 1000.0)
    else:
        rate_hz = 0.0

    print("=" * 64)
    print("AO bench telemetry summary")
    print("=" * 64)
    print(f"  valid samples     : {n}")
    print(f"  lines seen        : {parse_stats['total_lines']}")
    print(f"  non-'G,' skipped  : {parse_stats['skipped_other']}")
    print(f"  malformed skipped : {parse_stats['malformed']}")
    print(f"  capture duration  : {duration_ms:.0f} ms "
          f"({duration_ms / 1000.0:.3f} s)")
    print(f"  effective rate    : {rate_hz:.2f} Hz")
    print()

    header = f"  {'channel':<16}{'min':>12}{'max':>12}{'mean':>12}{'stddev':>12}"
    print(header)
    print("  " + "-" * (len(header) - 2))
    for channel in PLOT_CHANNELS:
        s = summarize_channel(channel_series(records, channel))
        print(f"  {channel:<16}{s['min']:>12.4f}{s['max']:>12.4f}"
              f"{s['mean']:>12.4f}{s['stddev']:>12.4f}")
    print()

    constant_values, changed = gain_summary(records)
    print("  derived gains (expected constant):")
    for channel in GAIN_CHANNELS:
        flag = "  <-- CHANGED mid-run!" if channel in changed else ""
        print(f"    {channel:<12}= {constant_values[channel]:.6g}{flag}")
    if changed:
        print()
        print("  WARNING: one or more gains changed during the capture.")
        print("           This indicates a re-derivation event mid-run;")
        print("           treat the run as two regimes when analysing.")
    print("=" * 64)


def make_plot(records, save_path=None):
    """Render the time-series plot. Imports matplotlib lazily so that
    --no-plot works even when matplotlib is not installed.

    Raises CaptureError if matplotlib is unavailable.
    """
    try:
        import matplotlib
        if save_path is not None:
            # Force a non-interactive backend for headless saving.
            matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        raise CaptureError(
            "matplotlib is not installed -- cannot plot. "
            "Re-run with --no-plot for the summary table only, "
            "or install matplotlib.")

    times = channel_series(records, TIME_FIELD)
    t0 = times[0]
    t = [(x - t0) / 1000.0 for x in times]  # seconds from start

    fig, axes = plt.subplots(4, 1, figsize=(10, 11), sharex=True)
    fig.suptitle("AO Mega balance-robot bench run")

    # Subplot 1: pitch vs setpoint overlaid.
    axes[0].plot(t, channel_series(records, "pitch_deg"),
                 label="pitch_deg", color="tab:blue")
    axes[0].plot(t, channel_series(records, "pitch_sp_deg"),
                 label="pitch_sp_deg", color="tab:orange",
                 linestyle="--")
    axes[0].set_ylabel("pitch (deg)")
    axes[0].legend(loc="upper right")
    axes[0].grid(True, alpha=0.3)

    # Subplot 2: wheel velocity.
    axes[1].plot(t, channel_series(records, "wheel_vel_mps"),
                 color="tab:green")
    axes[1].set_ylabel("wheel vel (m/s)")
    axes[1].grid(True, alpha=0.3)

    # Subplot 3: position.
    axes[2].plot(t, channel_series(records, "position_m"),
                 color="tab:red")
    axes[2].set_ylabel("position (m)")
    axes[2].grid(True, alpha=0.3)

    # Subplot 4: nudge.
    axes[3].plot(t, channel_series(records, "nudge_deg"),
                 color="tab:purple")
    axes[3].set_ylabel("nudge (deg)")
    axes[3].set_xlabel("time since start (s)")
    axes[3].grid(True, alpha=0.3)

    fig.tight_layout()

    if save_path is not None:
        fig.savefig(save_path, dpi=120)
        print(f"plot written to {save_path}")
    else:
        plt.show()


def read_lines(path):
    """Yield text lines from a file path, or from stdin if path is '-'.

    Raises CaptureError on a missing/unreadable file.
    """
    if path == "-":
        return sys.stdin.read().splitlines()
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            return fh.read().splitlines()
    except FileNotFoundError:
        raise CaptureError(f"capture file not found: {path}")
    except OSError as exc:
        raise CaptureError(f"cannot read capture file {path}: {exc}")


def build_parser():
    parser = argparse.ArgumentParser(
        prog="plot_bench_run.py",
        description="Analyse and plot AO Mega balance-robot bench "
                    "telemetry ('g' command capture).")
    parser.add_argument(
        "capture",
        help="capture file path, or '-' to read from stdin")
    parser.add_argument(
        "--save", metavar="PATH", default=None,
        help="write the plot to PATH (PNG/PDF/...) instead of showing it")
    parser.add_argument(
        "--no-plot", action="store_true",
        help="print the summary only; do not plot (works headless)")
    return parser


def main(argv=None):
    args = build_parser().parse_args(argv)

    try:
        lines = read_lines(args.capture)
        if not lines:
            raise CaptureError(
                f"capture is empty: {args.capture}")

        records, parse_stats = parse_capture(lines)
        if not records:
            raise CaptureError(
                "no valid 'G,' telemetry lines found "
                f"({parse_stats['total_lines']} line(s) seen, "
                f"{parse_stats['malformed']} malformed). "
                "Is this an AO 'g' command capture?")

        print_summary(records, parse_stats)

        if not args.no_plot:
            make_plot(records, save_path=args.save)

    except CaptureError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("\ninterrupted", file=sys.stderr)
        return 130

    return 0


if __name__ == "__main__":
    sys.exit(main())
