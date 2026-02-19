"""Sine scenario: Multiple sine waves at different frequencies on plot 0."""

import math
import time

from .core import write_line


def scenario_sine(fd, rate, duration):
    """Multiple sine waves at different frequencies on plot 0."""
    t = 0
    dt = 1.0 / rate
    end = time.time() + duration

    while time.time() < end:
        s1 = math.sin(2 * math.pi * 1.0 * t)
        s2 = 0.5 * math.sin(2 * math.pi * 2.5 * t)
        s3 = 0.25 * math.sin(2 * math.pi * 5.0 * t + 1.0)

        write_line(fd, f"1Hz@0:{s1:.4f} 2.5Hz@0:{s2:.4f} 5Hz@0:{s3:.4f}")

        t += dt
        time.sleep(dt)
