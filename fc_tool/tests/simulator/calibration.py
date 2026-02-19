"""Calibration scenario: interleaved text, ANSI, telemetry, and dashboard output."""

import math
import random
import time

from .core import write_line


def scenario_calibration(fd, rate, duration):
    """Mimics real FC calibration output -- mixed content on the same serial stream."""
    t = 0
    dt = 1.0 / rate
    end = time.time() + duration
    step = 0

    while time.time() < end:
        cycle = step % 4

        if cycle == 0:
            stage = (step // 4) % 10 + 1
            write_line(fd, f"Calibrating IMU... step {stage}/10")
        elif cycle == 1:
            msgs = [
                "\033[1;32mOK\033[0m: Gyro bias measured",
                "\033[1;32mOK\033[0m: Accel bias measured",
                "\033[1;33mWARN\033[0m: Vibration detected",
                "\033[1;32mOK\033[0m: Temp compensation applied",
            ]
            write_line(fd, msgs[step // 4 % len(msgs)])
        elif cycle == 2:
            ax = 0.02 + random.gauss(0, 0.005)
            ay = -0.01 + random.gauss(0, 0.005)
            az = 1.00 + random.gauss(0, 0.01)
            gx = 0.5 * math.sin(t * 0.3) + random.gauss(0, 0.1)
            gy = 0.3 * math.cos(t * 0.2) + random.gauss(0, 0.1)
            gz = random.gauss(0, 0.05)
            write_line(fd, f"AccX@0:{ax:.2f} AccY@0:{ay:.2f} AccZ@0:{az:.2f} GyrX@1:{gx:.1f} GyrY@1:{gy:.1f} GyrZ@1:{gz:.1f}")
        else:
            progress = min(100, ((step // 4) % 11) * 10)
            samples = (step // 4) * 150
            quality = "GOOD" if progress > 30 else "WARMUP"
            write_line(fd, f"progress={progress}% quality={quality} samples={samples}")

        step += 1
        t += dt
        time.sleep(dt)
