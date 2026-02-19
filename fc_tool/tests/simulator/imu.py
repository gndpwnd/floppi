"""IMU scenario: 6-axis data (accel + gyro) on 2 plots."""

import math
import random
import time

from .core import write_line


def scenario_imu(fd, rate, duration):
    """Simulate 6-axis IMU data on 2 plots (accel on plot 0, gyro on plot 1)."""
    t = 0
    dt = 1.0 / rate
    end = time.time() + duration

    while time.time() < end:
        ax = 0.02 + random.gauss(0, 0.01)
        ay = -0.01 + random.gauss(0, 0.01)
        az = 1.0 + random.gauss(0, 0.02)

        gx = 0.5 * math.sin(t * 0.3) + random.gauss(0, 0.2)
        gy = 0.3 * math.cos(t * 0.2) + random.gauss(0, 0.2)
        gz = random.gauss(0, 0.1)

        write_line(fd, f"ax@0:{ax:.4f} ay@0:{ay:.4f} az@0:{az:.4f} gx@1:{gx:.4f} gy@1:{gy:.4f} gz@1:{gz:.4f}")

        t += dt
        time.sleep(dt)
