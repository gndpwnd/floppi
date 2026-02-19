"""ANSI scenario: colored status messages for testing terminal rendering."""

import math
import time

from .core import write_line


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
        if idx % 3 == 0:
            val = math.sin(idx * 0.1) * 100
            write_line(fd, f"signal@0:{val:.2f}")
        idx += 1
        time.sleep(dt)
