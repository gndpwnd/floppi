"""Mixed and protocol scenarios: format cycling through all 4 protocol types."""

import math
import random
import time

from .core import write_line


def scenario_mixed(fd, rate, duration):
    """Mix of protocol formats: named@plot, name:value, name=value, plain CSV."""
    t = 0
    dt = 1.0 / rate
    end = time.time() + duration
    line_num = 0

    while time.time() < end:
        line_num += 1
        fmt = line_num % 4

        val1 = math.sin(t) * 10
        val2 = math.cos(t) * 5
        val3 = random.uniform(-1, 1)

        if fmt == 0:
            write_line(fd, f"temp@0:{val1:.2f} pressure@1:{val2:.2f}")
        elif fmt == 1:
            write_line(fd, f"sensor1:{val1:.2f} sensor2:{val2:.2f}")
        elif fmt == 2:
            write_line(fd, f"reading1={val1:.2f} reading2={val2:.2f}")
        else:
            write_line(fd, f"{val1:.2f},{val2:.2f},{val3:.2f}")

        t += dt
        time.sleep(dt)


def scenario_protocol(fd, rate, duration):
    """All 4 protocol formats in strict sequence -- regression test for parser."""
    t = 0
    dt = 1.0 / rate
    end = time.time() + duration
    cycle = 0

    while time.time() < end:
        v = math.sin(t) * 10

        if cycle == 0:
            write_line(fd, f"named_plot@0:{v:.4f}")
        elif cycle == 1:
            write_line(fd, f"colon_fmt:{v:.4f}")
        elif cycle == 2:
            write_line(fd, f"equals_fmt={v:.4f}")
        elif cycle == 3:
            write_line(fd, f"{v:.4f}")
        cycle = (cycle + 1) % 4

        t += dt
        time.sleep(dt)
