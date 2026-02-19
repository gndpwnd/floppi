"""High-channel scenario: 24 variables on a single line for parser limit testing."""

import math
import time

from .core import write_line


def scenario_high_channel(fd, rate, duration):
    """24 channels on plot 0, each a sine at a slightly different frequency."""
    t = 0
    dt = 1.0 / rate
    end = time.time() + duration
    num_channels = 24

    while time.time() < end:
        parts = []
        for ch in range(num_channels):
            freq = 0.5 + ch * 0.1
            val = math.sin(2 * math.pi * freq * t)
            parts.append(f"ch{ch:02d}@0:{val:.4f}")
        write_line(fd, " ".join(parts))

        t += dt
        time.sleep(dt)
