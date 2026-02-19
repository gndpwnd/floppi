"""Dashboard scenario: key=value pairs simulating a dashboard readout."""

import math
import random
import time

from .core import write_line


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
