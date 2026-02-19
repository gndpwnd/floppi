"""Multi-plot scenario: controlled data across 5 plots with known wave shapes."""

import math
import time

from .core import write_line


def scenario_multi_plot(fd, rate, duration):
    """5 plots with known shapes: sine, cosine, triangle, sawtooth, square."""
    t = 0
    dt = 1.0 / rate
    end = time.time() + duration

    while time.time() < end:
        # Plot 0: sine, 1Hz
        temp = math.sin(2 * math.pi * 1.0 * t) * 25 + 25

        # Plot 1: cosine, 0.5Hz
        pressure = math.cos(2 * math.pi * 0.5 * t) * 500 + 1013

        # Plot 2: triangle wave (period 2s)
        tri_phase = (t % 2.0) / 2.0
        humidity = (2 * tri_phase if tri_phase < 0.5 else 2 * (1 - tri_phase)) * 100

        # Plot 3: sawtooth (period 1s)
        voltage = (t % 1.0) * 5.0

        # Plot 4: square wave (period 1s)
        current = 1.0 if (t % 1.0) < 0.5 else 0.0

        write_line(fd, f"temp@0:{temp:.2f} pressure@1:{pressure:.1f} humidity@2:{humidity:.1f} voltage@3:{voltage:.3f} current@4:{current:.1f}")

        t += dt
        time.sleep(dt)
