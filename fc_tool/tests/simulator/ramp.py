"""Ramp scenario: linear ramp + step response for zoom/scale auto-fit testing."""

import time

from .core import write_line


def scenario_ramp(fd, rate, duration):
    """Linear ramp from 0 to 100 on plot 0, step from 0 to 50 at midpoint on plot 1."""
    t = 0
    dt = 1.0 / rate
    end = time.time() + duration
    half = duration / 2.0

    while time.time() < end:
        ramp_val = (t / duration) * 100.0
        step_val = 0.0 if t < half else 50.0

        write_line(fd, f"ramp@0:{ramp_val:.2f} step@1:{step_val:.2f}")

        t += dt
        time.sleep(dt)
