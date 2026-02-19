"""Burst scenario: short high-rate bursts followed by silence for buffer testing."""

import time

from .core import write_line


def scenario_burst(fd, rate, duration):
    """Burst of 50 lines as fast as possible, then 0.5s silence, repeating."""
    end = time.time() + duration
    counter = 0

    while time.time() < end:
        # Burst: 50 lines with no sleep
        for i in range(50):
            val = counter * 0.1
            write_line(fd, f"burst@0:{val:.1f}")
            counter += 1
            if time.time() >= end:
                return

        # Silence
        time.sleep(0.5)
