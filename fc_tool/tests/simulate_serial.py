#!/usr/bin/env python3
"""Serial data simulator for fc_tool testing.

Generates fake serial data in fc_tool's plotter protocol formats and writes
it to a serial port (virtual pty via socat) or stdout. Used for testing the
serial monitor, plotter, and ANSI rendering without real hardware.

Derivative of flight_controller/tools/serial_monitor.py — same termios-based
serial approach, but generates data instead of reading commands.

Usage:
    # Write to a virtual serial port (pair created by socat)
    python3 simulate_serial.py /tmp/vserial1 --scenario imu

    # Write to stdout (for piping or inspection)
    python3 simulate_serial.py --stdout --scenario mixed

    # Custom rate and duration
    python3 simulate_serial.py /tmp/vserial1 --scenario sine --rate 50 --duration 30

    # List available scenarios
    python3 simulate_serial.py --list

Scenarios:
    imu         - 6-axis IMU data (ax,ay,az,gx,gy,gz) on 2 plots
    sine        - Multiple sine waves at different frequencies on 1 plot
    mixed       - Mix of named values, CSV, and plain numbers
    ansi        - ANSI-colored status messages (tests terminal rendering)
    stress      - High-rate data across many plots (tests performance limits)
    dashboard   - Key=value pairs that update in place (tests monitor display)
    protocol    - All 4 protocol formats in sequence (regression test)
    noise       - Random noise with occasional spikes (tests plot scaling)
"""

import argparse
import math
import os
import random
import sys
import time

# Reuse termios-based serial open from flight_controller's serial_monitor.py
# but simplified — we only need write access
try:
    import fcntl
    import termios

    HAS_TERMIOS = True
except ImportError:
    HAS_TERMIOS = False


def open_serial_write(port, baud=115200):
    """Open serial port for writing with raw termios settings."""
    if not HAS_TERMIOS:
        raise RuntimeError("termios not available (Windows?). Use --stdout instead.")

    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)

    try:
        attrs = termios.tcgetattr(fd)
        attrs[0] = 0  # iflag
        attrs[1] = 0  # oflag
        attrs[3] = 0  # lflag
        attrs[2] &= ~(termios.CSIZE | termios.PARENB | termios.CSTOPB | termios.HUPCL)
        attrs[2] |= termios.CS8 | termios.CLOCAL | termios.CREAD

        baud_const = getattr(termios, f"B{baud}", None)
        if baud_const is None:
            baud_const = baud
        attrs[4] = baud_const
        attrs[5] = baud_const
        attrs[6][termios.VMIN] = 0
        attrs[6][termios.VTIME] = 0

        termios.tcsetattr(fd, termios.TCSANOW, attrs)

        flags = fcntl.fcntl(fd, fcntl.F_GETFL)
        fcntl.fcntl(fd, fcntl.F_SETFL, flags & ~os.O_NONBLOCK)
    except Exception:
        os.close(fd)
        raise

    return fd


def write_line(fd, line):
    """Write a line to fd (file descriptor or sys.stdout)."""
    data = (line + "\n").encode()
    if fd == "stdout":
        sys.stdout.write(line + "\n")
        sys.stdout.flush()
    else:
        os.write(fd, data)


# ============================================================================
# Scenarios
# ============================================================================


def scenario_imu(fd, rate, duration):
    """Simulate 6-axis IMU data on 2 plots (accel on plot 0, gyro on plot 1)."""
    t = 0
    dt = 1.0 / rate
    end = time.time() + duration

    while time.time() < end:
        # Accelerometer: gravity + vibration noise
        ax = 0.02 + random.gauss(0, 0.01)
        ay = -0.01 + random.gauss(0, 0.01)
        az = 1.0 + random.gauss(0, 0.02)

        # Gyroscope: slow drift + noise
        gx = 0.5 * math.sin(t * 0.3) + random.gauss(0, 0.2)
        gy = 0.3 * math.cos(t * 0.2) + random.gauss(0, 0.2)
        gz = random.gauss(0, 0.1)

        write_line(fd, f"ax@0:{ax:.4f} ay@0:{ay:.4f} az@0:{az:.4f} gx@1:{gx:.4f} gy@1:{gy:.4f} gz@1:{gz:.4f}")

        t += dt
        time.sleep(dt)


def scenario_sine(fd, rate, duration):
    """Multiple sine waves at different frequencies on plot 0."""
    t = 0
    dt = 1.0 / rate
    end = time.time() + duration

    while time.time() < end:
        s1 = math.sin(2 * math.pi * 1.0 * t)  # 1 Hz
        s2 = 0.5 * math.sin(2 * math.pi * 2.5 * t)  # 2.5 Hz
        s3 = 0.25 * math.sin(2 * math.pi * 5.0 * t + 1.0)  # 5 Hz, phase shifted

        write_line(fd, f"1Hz@0:{s1:.4f} 2.5Hz@0:{s2:.4f} 5Hz@0:{s3:.4f}")

        t += dt
        time.sleep(dt)


def scenario_mixed(fd, rate, duration):
    """Mix of protocol formats: named@plot, name:value, name=value, plain CSV."""
    t = 0
    dt = 1.0 / rate
    end = time.time() + duration
    line_num = 0

    while time.time() < end:
        line_num += 1
        fmt = line_num % 4

        val1 = math.sin(t) * 10
        val2 = math.cos(t) * 5
        val3 = random.uniform(-1, 1)

        if fmt == 0:
            # name@plotId:value
            write_line(fd, f"temp@0:{val1:.2f} pressure@1:{val2:.2f}")
        elif fmt == 1:
            # name:value
            write_line(fd, f"sensor1:{val1:.2f} sensor2:{val2:.2f}")
        elif fmt == 2:
            # name=value
            write_line(fd, f"reading1={val1:.2f} reading2={val2:.2f}")
        else:
            # Plain CSV (fallback)
            write_line(fd, f"{val1:.2f},{val2:.2f},{val3:.2f}")

        t += dt
        time.sleep(dt)


def scenario_ansi(fd, rate, duration):
    """ANSI-colored status messages for testing terminal rendering."""
    messages = [
        "\033[1;32mOK\033[0m: IMU initialized",
        "\033[1;33mWARN\033[0m: Battery voltage low (3.2V)",
        "\033[1;31mERROR\033[0m: GPS fix lost",
        "\033[1;36mINFO\033[0m: Telemetry streaming at 50Hz",
        "\033[2mDEBUG: loop_time=1247us\033[0m",
        "\033[1;35mCALIB\033[0m: Stage 2/5 — \033[4mGyro calibration\033[0m",
        "\033[1;32m[PASS]\033[0m Self-test: accelerometer",
        "\033[1;31m[FAIL]\033[0m Self-test: magnetometer — \033[1;33mcheck wiring\033[0m",
        "Plain text line (no ANSI codes)",
        "\033[1mBold\033[0m \033[2mDim\033[0m \033[4mUnderline\033[0m \033[31mRed\033[0m \033[32mGreen\033[0m \033[34mBlue\033[0m",
    ]

    dt = 1.0 / rate
    end = time.time() + duration
    idx = 0

    while time.time() < end:
        write_line(fd, messages[idx % len(messages)])
        # Also interleave some plotter data to test mixed mode
        if idx % 3 == 0:
            val = math.sin(idx * 0.1) * 100
            write_line(fd, f"signal@0:{val:.2f}")
        idx += 1
        time.sleep(dt)


def scenario_stress(fd, rate, duration):
    """High-rate data across many plot IDs (tests performance and max plots cap)."""
    t = 0
    dt = 1.0 / rate
    end = time.time() + duration
    # Deliberately try 15 plots (exceeds the 10 cap — tests the limit)
    num_plots = 15

    while time.time() < end:
        parts = []
        for p in range(num_plots):
            val = math.sin(t * (p + 1) * 0.5) + random.gauss(0, 0.1)
            parts.append(f"ch{p}@{p}:{val:.3f}")
        write_line(fd, " ".join(parts))

        t += dt
        time.sleep(dt)


def scenario_dashboard(fd, rate, duration):
    """Key=value pairs simulating a dashboard readout."""
    t = 0
    dt = 1.0 / rate
    end = time.time() + duration

    while time.time() < end:
        battery = 3.7 + 0.5 * math.sin(t * 0.01)
        loop_us = 1200 + random.randint(-100, 100)
        altitude = 10.0 + 2.0 * math.sin(t * 0.1)
        armed = "YES" if int(t) % 20 > 5 else "NO"

        write_line(fd, f"battery={battery:.2f}V loop={loop_us}us alt={altitude:.1f}m armed={armed}")

        t += dt
        time.sleep(dt)


def scenario_protocol(fd, rate, duration):
    """All 4 protocol formats in strict sequence — regression test for parser."""
    t = 0
    dt = 1.0 / rate
    end = time.time() + duration
    cycle = 0

    while time.time() < end:
        v = math.sin(t) * 10

        # Cycle through all 4 formats explicitly
        if cycle == 0:
            write_line(fd, f"named_plot@0:{v:.4f}")
        elif cycle == 1:
            write_line(fd, f"colon_fmt:{v:.4f}")
        elif cycle == 2:
            write_line(fd, f"equals_fmt={v:.4f}")
        elif cycle == 3:
            write_line(fd, f"{v:.4f}")
        cycle = (cycle + 1) % 4

        t += dt
        time.sleep(dt)


def scenario_noise(fd, rate, duration):
    """Random noise with occasional spikes — tests auto-scaling and Y-axis zoom."""
    t = 0
    dt = 1.0 / rate
    end = time.time() + duration

    while time.time() < end:
        base = random.gauss(0, 1)
        # Occasional spike (1% chance)
        if random.random() < 0.01:
            base += random.choice([-1, 1]) * random.uniform(10, 50)

        write_line(fd, f"noise@0:{base:.4f}")

        t += dt
        time.sleep(dt)


SCENARIOS = {
    "imu": (scenario_imu, "6-axis IMU data (accel + gyro) on 2 plots"),
    "sine": (scenario_sine, "Multiple sine waves at different frequencies"),
    "mixed": (scenario_mixed, "Mix of all 4 protocol formats"),
    "ansi": (scenario_ansi, "ANSI-colored status messages"),
    "stress": (scenario_stress, "High-rate data across 15 plots (tests max cap)"),
    "dashboard": (scenario_dashboard, "Key=value dashboard readout"),
    "protocol": (scenario_protocol, "All 4 formats in strict rotation"),
    "noise": (scenario_noise, "Random noise with spikes (tests Y-axis scaling)"),
}

# ============================================================================
# Main
# ============================================================================


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
        "--scenario",
        "-s",
        default="imu",
        choices=list(SCENARIOS.keys()),
        help="Data scenario to simulate (default: imu)",
    )
    parser.add_argument(
        "--rate",
        "-r",
        type=int,
        default=50,
        help="Lines per second (default: 50)",
    )
    parser.add_argument(
        "--duration",
        "-d",
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

    # Open output
    if args.stdout:
        fd = "stdout"
        print(f"Simulator: scenario={args.scenario} rate={args.rate}Hz "
              f"duration={args.duration}s → stdout", file=sys.stderr)
    else:
        fd = open_serial_write(args.port, args.baud)
        print(f"Simulator: scenario={args.scenario} rate={args.rate}Hz "
              f"duration={args.duration}s → {args.port} @ {args.baud}", file=sys.stderr)

    # Run scenario
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
