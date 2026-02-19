"""Intermittent scenario: data with deliberate gaps for stale Hz detection testing."""

import math
import time

from .core import write_line


def scenario_intermittent(fd, rate, duration):
    """1 second of sine data, then 1 second of silence, repeating."""
    t = 0
    dt = 1.0 / rate
    end = time.time() + duration

    while time.time() < end:
        cycle_pos = t % 2.0
        if cycle_pos < 1.0:
            val = math.sin(2 * math.pi * 1.0 * t)
            write_line(fd, f"heartbeat@0:{val:.4f}")
        # else: silence -- no output for 1 second
        t += dt
        time.sleep(dt)
