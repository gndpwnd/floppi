"""Mixed dashboard scenario: alternating plotter data and dashboard key=value lines."""

import math
import random
import time

from .core import write_line


def scenario_mixed_dashboard(fd, rate, duration):
    """Even lines: plotter data. Odd lines: dashboard key=value pairs."""
    t = 0
    dt = 1.0 / rate
    end = time.time() + duration
    line_num = 0

    while time.time() < end:
        if line_num % 2 == 0:
            signal = math.sin(2 * math.pi * 1.0 * t) * 10
            phase = math.cos(2 * math.pi * 1.0 * t) * 10
            write_line(fd, f"signal@0:{signal:.3f} phase@0:{phase:.3f}")
        else:
            battery = 3.7 + 0.3 * math.sin(t * 0.05)
            rssi = -52 + random.randint(-5, 5)
            armed = "NO" if int(t) % 10 < 5 else "YES"
            loop_us = 1200 + random.randint(-50, 50)
            write_line(fd, f"battery={battery:.1f}V rssi={rssi}dBm armed={armed} loop={loop_us}us")

        line_num += 1
        t += dt
        time.sleep(dt)
