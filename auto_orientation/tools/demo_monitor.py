#!/usr/bin/env python3
"""Demo script for real_time_monitor.py

Simulates sensor data by writing JSON lines to a pseudo-terminal.
Useful for testing the monitor without actual hardware.

Usage:
    # In one terminal:
    python3 demo_monitor.py

    # In another terminal:
    python3 real_time_monitor.py /dev/pts/X  (where X is shown by demo)

Features:
    - Simulates realistic orientation changes (rotating 360°)
    - Simulates GPS acquisition (no fix -> 8 satellites)
    - Simulates calibration progression
    - Variable data rates matching realistic sensor output
"""

import json
import math
import os
import pty
import subprocess
import sys
import threading
import time
from datetime import datetime


class SensorSimulator:
    """Simulate BNO085 + GPS sensor data with realistic patterns."""

    def __init__(self):
        """Initialize simulator with starting state."""
        self.timestamp_ms = 0
        self.start_time = time.time()

        # Orientation state
        self.roll_target = 0.0
        self.pitch_target = 0.0
        self.yaw_target = 0.0
        self.roll = 0.0
        self.pitch = 0.0
        self.yaw = 0.0

        # Calibration state
        self.sys_cal = 0
        self.acc_cal = 0
        self.gyr_cal = 0
        self.mag_cal = 0

        # Position state
        self.gps_fix_acquired = False
        self.gps_fix_time = None
        self.latitude = 30.27123
        self.longitude = -97.74156
        self.altitude = 158.2
        self.satellites = 0

    def update(self, elapsed_sec: float) -> None:
        """Update simulator state based on elapsed time."""
        self.timestamp_ms = int(elapsed_sec * 1000)

        # Simulate rotating 360° in 30 seconds
        self.yaw_target = (elapsed_sec % 30) * 360 / 30
        self.roll_target = math.sin(elapsed_sec / 20) * 45
        self.pitch_target = math.cos(elapsed_sec / 15) * 30

        # Smoothly interpolate toward targets
        self.roll += (self.roll_target - self.roll) * 0.1
        self.pitch += (self.pitch_target - self.pitch) * 0.1
        self.yaw += (self.yaw_target - self.yaw) * 0.1

        # Simulate calibration progression
        if elapsed_sec < 5:
            self.sys_cal = int(elapsed_sec)
            self.acc_cal = max(0, int(elapsed_sec - 1))
            self.gyr_cal = max(0, int(elapsed_sec - 2))
            self.mag_cal = max(0, int(elapsed_sec - 3))
        else:
            self.sys_cal = 3
            self.acc_cal = 3
            self.gyr_cal = 3
            self.mag_cal = 2

        # Simulate GPS acquisition
        if not self.gps_fix_acquired and elapsed_sec > 3:
            self.gps_fix_acquired = True
            self.gps_fix_time = time.time()

        if self.gps_fix_acquired:
            # Increase satellite count over 20 seconds
            fix_elapsed = time.time() - self.gps_fix_time
            self.satellites = min(12, int(fix_elapsed / 2))
            # Improve accuracy
            self.altitude += 0.1 * math.sin(fix_elapsed / 5)

    def euler_to_quaternion(self, roll: float, pitch: float, yaw: float) -> tuple:
        """Convert Euler angles (degrees) to quaternion."""
        # Convert to radians
        r = math.radians(roll)
        p = math.radians(pitch)
        y = math.radians(yaw)

        # Use ZYX convention (yaw, pitch, roll)
        cy = math.cos(y * 0.5)
        sy = math.sin(y * 0.5)
        cp = math.cos(p * 0.5)
        sp = math.sin(p * 0.5)
        cr = math.cos(r * 0.5)
        sr = math.sin(r * 0.5)

        w = cr * cp * cy + sr * sp * sy
        x = sr * cp * cy - cr * sp * sy
        y_q = cr * sp * cy + sr * cp * sy
        z = cr * cp * sy - sr * sp * cy

        return (w, x, y_q, z)

    def get_json_line(self) -> str:
        """Generate JSON line with current sensor state."""
        quat = self.euler_to_quaternion(self.roll, self.pitch, self.yaw)

        data = {
            "timestamp_ms": self.timestamp_ms,
            "orientation": {
                "valid": True,
                "quaternion": {
                    "w": round(quat[0], 3),
                    "x": round(quat[1], 3),
                    "y": round(quat[2], 3),
                    "z": round(quat[3], 3)
                },
                "calibration": {
                    "system": self.sys_cal,
                    "accel": self.acc_cal,
                    "gyro": self.gyr_cal,
                    "mag": self.mag_cal
                }
            },
            "position": {
                "valid": self.gps_fix_acquired,
                "latitude": round(self.latitude, 5),
                "longitude": round(self.longitude, 5),
                "altitude_m": round(self.altitude, 1),
                "accuracy_m": round(20 - self.satellites * 1.5, 1) if self.satellites > 0 else None,
                "num_satellites": self.satellites,
                "fix_quality": 1 if self.gps_fix_acquired else 0,
                "hdop": round(5 - self.satellites * 0.3, 1) if self.satellites > 0 else None
            }
        }

        return json.dumps(data)


def run_simulator(fd: int) -> None:
    """Run the simulator, writing JSON lines to fd."""
    simulator = SensorSimulator()
    start_time = time.time()
    sample_count = 0

    print("Simulator started, generating sensor data...", file=sys.stderr)
    print("Orientation: rotating 360° in 30s cycles", file=sys.stderr)
    print("Calibration: progressing from uncalibrated to fully calibrated", file=sys.stderr)
    print("Position: GPS acquiring fix and improving accuracy", file=sys.stderr)
    print()

    try:
        while True:
            elapsed = time.time() - start_time
            simulator.update(elapsed)

            # Generate JSON
            json_line = simulator.get_json_line()

            # Write to pseudo-terminal
            try:
                os.write(fd, (json_line + '\n').encode('utf-8'))
                sample_count += 1

                # Variable data rate (8-12 Hz orientation, 0.5-2 Hz position)
                if sample_count % 8 == 0:
                    # Every 8th sample gets position update details
                    if simulator.gps_fix_acquired:
                        sats_status = f"Satellites: {simulator.satellites:2d}"
                    else:
                        sats_status = "Waiting for GPS fix"
                    print(f"[{elapsed:6.1f}s] Sample {sample_count:5d} - "
                          f"Roll={simulator.roll:6.1f}° - {sats_status}",
                          file=sys.stderr)
            except OSError:
                # Reader disconnected
                print("Reader disconnected", file=sys.stderr)
                break

            # 10 Hz data rate
            time.sleep(0.1)

    except KeyboardInterrupt:
        print(f"\nSimulator stopped after {sample_count} samples", file=sys.stderr)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
    finally:
        try:
            os.close(fd)
        except:
            pass


def main():
    """Main entry point."""
    print("=== Auto Orientation Monitor Demo ===")
    print()
    print("This script creates a virtual serial port and simulates sensor data.")
    print()

    # Create pseudo-terminal
    master_fd, slave_fd = pty.openpty()
    slave_name = os.ttyname(slave_fd)

    print(f"Pseudo-terminal created: {slave_name}")
    print()
    print(f"In another terminal, run:")
    print(f"  python3 real_time_monitor.py {slave_name}")
    print()
    print("Simulation will start in 3 seconds...")
    print("Press Ctrl+C to stop")
    print()

    time.sleep(3)

    # Run simulator in background
    simulator_thread = threading.Thread(
        target=run_simulator,
        args=(master_fd,),
        daemon=True
    )
    simulator_thread.start()

    # Wait for simulator
    simulator_thread.join()

    # Cleanup
    try:
        os.close(master_fd)
    except:
        pass
    try:
        os.close(slave_fd)
    except:
        pass

    print("Demo complete!")


if __name__ == '__main__':
    main()
