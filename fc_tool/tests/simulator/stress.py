"""Stress and noise scenarios: performance limits and auto-scaling tests."""

import math
import random
import time

from .core import write_line


def scenario_stress(fd, rate, duration):
    """High-rate data across many plot IDs (tests performance and max plots cap)."""
    t = 0
    dt = 1.0 / rate
    end = time.time() + duration
    num_plots = 15

    while time.time() < end:
        parts = []
        for p in range(num_plots):
            val = math.sin(t * (p + 1) * 0.5) + random.gauss(0, 0.1)
            parts.append(f"ch{p}@{p}:{val:.3f}")
        write_line(fd, " ".join(parts))

        t += dt
        time.sleep(dt)


def scenario_noise(fd, rate, duration):
    """Random noise with occasional spikes -- tests auto-scaling and Y-axis zoom."""
    t = 0
    dt = 1.0 / rate
    end = time.time() + duration

    while time.time() < end:
        base = random.gauss(0, 1)
        if random.random() < 0.01:
            base += random.choice([-1, 1]) * random.uniform(10, 50)

        write_line(fd, f"noise@0:{base:.4f}")

        t += dt
        time.sleep(dt)
