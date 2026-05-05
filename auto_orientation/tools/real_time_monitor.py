#!/usr/bin/env python3
"""Real-time terminal monitor for auto_orientation JSON sensor output.

Displays live orientation (roll/pitch/yaw), position (lat/lon/alt), and
calibration status with color-coded updates in a user-friendly terminal UI.

Usage:
    python3 real_time_monitor.py /dev/ttyACM0
    python3 real_time_monitor.py /dev/ttyACM0 --baud 115200
    python3 real_time_monitor.py /dev/ttyACM0 --log output.txt
    python3 real_time_monitor.py /dev/ttyACM0 --no-color

Features:
    - Auto-detect baud rate (tries 115200, 9600, 115200)
    - Parse incoming JSON lines from sensor
    - Convert quaternions to Euler angles (roll/pitch/yaw)
    - Color-coded calibration status indicators
    - Real-time refresh every 100ms
    - Connection loss detection and recovery
    - Optional JSON logging to file
    - Statistics and uptime tracking
"""

import argparse
import json
import math
import os
import re
import serial
import sys
import threading
import time
from datetime import datetime, timedelta
from dataclasses import dataclass, field
from typing import Optional, Dict, Tuple


@dataclass
class CalibrationStatus:
    """Calibration status for each sensor system."""
    system: int = 0
    accel: int = 0
    gyro: int = 0
    mag: int = 0

    def __str__(self) -> str:
        """Return calibration status as bar representation."""
        levels = [self.system, self.accel, self.gyro, self.mag]
        names = ["SYS", "ACC", "GYR", "MAG"]
        bars = []
        for level, name in zip(levels, names):
            # level: 0=red, 1=yellow, 2=green, 3=bright
            if level == 0:
                bars.append(f"░░░ {name}")  # empty bars (red)
            elif level == 1:
                bars.append(f"█░░ {name}")  # one bar (yellow)
            elif level == 2:
                bars.append(f"██░ {name}")  # two bars (green)
            else:  # 3
                bars.append(f"███ {name}")  # three bars (bright)
        return "  ".join(bars)


@dataclass
class OrientationData:
    """Orientation data in quaternions and Euler angles."""
    timestamp_ms: int = 0
    valid: bool = False
    w: float = 0.0
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0
    roll: float = 0.0
    pitch: float = 0.0
    yaw: float = 0.0
    calibration: CalibrationStatus = field(default_factory=CalibrationStatus)

    def compute_euler(self) -> None:
        """Convert quaternion to Euler angles (roll, pitch, yaw in degrees)."""
        w, x, y, z = self.w, self.x, self.y, self.z

        # Roll (x-axis rotation)
        sinr_cosp = 2 * (w * x + y * z)
        cosr_cosp = 1 - 2 * (x * x + y * y)
        self.roll = math.degrees(math.atan2(sinr_cosp, cosr_cosp))

        # Pitch (y-axis rotation)
        sinp = 2 * (w * y - z * x)
        # Clamp to avoid numerical errors in asin
        sinp = max(-1.0, min(1.0, sinp))
        self.pitch = math.degrees(math.asin(sinp))

        # Yaw (z-axis rotation)
        siny_cosp = 2 * (w * z + x * y)
        cosy_cosp = 1 - 2 * (y * y + z * z)
        self.yaw = math.degrees(math.atan2(siny_cosp, cosy_cosp))


@dataclass
class PositionData:
    """Position data from GPS."""
    timestamp_ms: int = 0
    valid: bool = False
    latitude: Optional[float] = None
    longitude: Optional[float] = None
    altitude_m: Optional[float] = None
    accuracy_m: Optional[float] = None
    num_satellites: int = 0
    fix_quality: int = 0
    hdop: Optional[float] = None

    @property
    def fix_type_str(self) -> str:
        """Return human-readable fix type."""
        fix_types = {
            0: "No fix",
            1: "GPS",
            2: "DGPS",
            3: "PPS",
            4: "RTK",
            5: "RTK-Float",
            6: "Estimated",
        }
        return fix_types.get(self.fix_quality, f"Unknown({self.fix_quality})")


@dataclass
class SensorStats:
    """Statistics for sensor data collection."""
    orientation_samples: int = 0
    position_samples: int = 0
    json_errors: int = 0
    parse_errors: int = 0
    start_time: datetime = field(default_factory=datetime.now)
    last_orientation_time: Optional[datetime] = None
    last_position_time: Optional[datetime] = None
    orientation_rate: float = 0.0  # Hz
    position_rate: float = 0.0  # Hz

    @property
    def uptime(self) -> str:
        """Return formatted uptime."""
        elapsed = datetime.now() - self.start_time
        hours, remainder = divmod(int(elapsed.total_seconds()), 3600)
        minutes, seconds = divmod(remainder, 60)
        return f"{hours:02d}h {minutes:02d}m {seconds:02d}s"

    def update_rates(self) -> None:
        """Update data rates based on sample counts and time."""
        now = datetime.now()
        elapsed_sec = (now - self.start_time).total_seconds()
        if elapsed_sec > 0:
            self.orientation_rate = self.orientation_samples / elapsed_sec
            self.position_rate = self.position_samples / elapsed_sec


class JSONSensorMonitor:
    """Monitor JSON sensor output from serial port."""

    def __init__(self, port: str, baud: int = 115200, log_file: Optional[str] = None,
                 use_color: bool = True, quiet: bool = False):
        """Initialize the monitor.

        Args:
            port: Serial port device path
            baud: Baud rate
            log_file: Optional path to log raw JSON
            use_color: Enable colored output
            quiet: Suppress display updates (logging still works)
        """
        self.port = port
        self.baud = baud
        self.log_file = log_file
        self.use_color = use_color
        self.quiet = quiet

        self.ser: Optional[serial.Serial] = None
        self.running = True
        self.orientation = OrientationData()
        self.position = PositionData()
        self.stats = SensorStats()
        self.lock = threading.Lock()
        self.last_display_time = time.time()
        self.display_interval = 0.1  # 100ms refresh

        # Color codes
        self.colors = {
            'red': '\033[91m',
            'yellow': '\033[93m',
            'green': '\033[92m',
            'bright': '\033[97m',
            'cyan': '\033[96m',
            'reset': '\033[0m',
            'bold': '\033[1m',
        } if self.use_color else {k: '' for k in ['red', 'yellow', 'green', 'bright', 'cyan', 'reset', 'bold']}

        if self.log_file:
            os.makedirs(os.path.dirname(self.log_file) or ".", exist_ok=True)
            self.log_f = open(self.log_file, 'w')
        else:
            self.log_f = None

    def connect(self) -> bool:
        """Open serial connection with retry logic."""
        baud_rates = [self.baud, 115200, 9600]
        if self.baud in baud_rates:
            baud_rates.remove(self.baud)
        baud_rates.insert(0, self.baud)

        for baud in baud_rates:
            try:
                self.ser = serial.Serial(
                    port=self.port,
                    baudrate=baud,
                    timeout=1.0,
                    bytesize=serial.EIGHTBITS,
                    parity=serial.PARITY_NONE,
                    stopbits=serial.STOPBITS_ONE
                )
                print(f"{self.colors['green']}Connected: {self.port} @ {baud} baud{self.colors['reset']}",
                      file=sys.stderr)
                return True
            except (serial.SerialException, OSError) as e:
                if baud == baud_rates[-1]:
                    print(f"{self.colors['red']}Error: Failed to open {self.port}: {e}{self.colors['reset']}",
                          file=sys.stderr)
                    return False
        return False

    def read_loop(self) -> None:
        """Read from serial port in background thread."""
        buffer = ""
        reconnect_delay = 1.0
        reconnect_time = time.time()

        while self.running:
            try:
                if self.ser is None or not self.ser.is_open:
                    if time.time() >= reconnect_time:
                        if not self.connect():
                            reconnect_time = time.time() + reconnect_delay
                        else:
                            reconnect_delay = 1.0
                    else:
                        time.sleep(0.1)
                    continue

                # Read available data
                if self.ser.in_waiting:
                    chunk = self.ser.read(self.ser.in_waiting)
                    buffer += chunk.decode('utf-8', errors='replace')

                    # Process complete lines
                    while '\n' in buffer:
                        line, buffer = buffer.split('\n', 1)
                        line = line.strip()
                        if line:
                            self._process_line(line)
                else:
                    time.sleep(0.01)
            except serial.SerialException as e:
                print(f"{self.colors['yellow']}Serial error: {e}{self.colors['reset']}",
                      file=sys.stderr)
                if self.ser:
                    self.ser.close()
                self.ser = None
                reconnect_time = time.time() + reconnect_delay
                reconnect_delay = min(reconnect_delay * 2, 10.0)
            except Exception as e:
                print(f"{self.colors['red']}Unexpected error: {e}{self.colors['reset']}",
                      file=sys.stderr)
                time.sleep(0.1)

    def _process_line(self, line: str) -> None:
        """Process a single line from serial (expected to be JSON)."""
        try:
            # Try to parse as JSON
            data = json.loads(line)
        except json.JSONDecodeError:
            with self.lock:
                self.stats.json_errors += 1
            return

        # Log raw JSON if requested
        if self.log_f:
            self.log_f.write(line + '\n')
            self.log_f.flush()

        with self.lock:
            try:
                # Parse orientation data
                if 'orientation' in data:
                    ori = data['orientation']
                    self.orientation.timestamp_ms = data.get('timestamp_ms', 0)
                    self.orientation.valid = ori.get('valid', False)

                    if self.orientation.valid:
                        quat = ori.get('quaternion', {})
                        self.orientation.w = float(quat.get('w', 0.0))
                        self.orientation.x = float(quat.get('x', 0.0))
                        self.orientation.y = float(quat.get('y', 0.0))
                        self.orientation.z = float(quat.get('z', 0.0))
                        self.orientation.compute_euler()

                        cal = ori.get('calibration', {})
                        self.orientation.calibration = CalibrationStatus(
                            system=cal.get('system', 0),
                            accel=cal.get('accel', 0),
                            gyro=cal.get('gyro', 0),
                            mag=cal.get('mag', 0)
                        )

                    self.stats.orientation_samples += 1
                    self.stats.last_orientation_time = datetime.now()

                # Parse position data
                if 'position' in data:
                    pos = data['position']
                    self.position.timestamp_ms = data.get('timestamp_ms', 0)
                    self.position.valid = pos.get('valid', False)

                    if self.position.valid:
                        self.position.latitude = pos.get('latitude')
                        self.position.longitude = pos.get('longitude')
                        self.position.altitude_m = pos.get('altitude_m')
                        self.position.accuracy_m = pos.get('accuracy_m')
                        self.position.num_satellites = pos.get('num_satellites', 0)
                        self.position.fix_quality = pos.get('fix_quality', 0)
                        self.position.hdop = pos.get('hdop')

                    self.stats.position_samples += 1
                    self.stats.last_position_time = datetime.now()
            except (KeyError, ValueError, TypeError) as e:
                self.stats.parse_errors += 1

    def display(self) -> None:
        """Display formatted sensor data to terminal."""
        now = time.time()
        if now - self.last_display_time < self.display_interval or self.quiet:
            time.sleep(0.01)
            return

        self.last_display_time = now

        with self.lock:
            self.stats.update_rates()
            ori = self.orientation
            pos = self.position
            stats = self.stats

        # Clear screen and move cursor to top
        print('\033[2J\033[H', end='', flush=True)

        # Header
        print(f"{self.colors['bold']}{self.colors['cyan']}=== Auto Orientation Monitor ==={self.colors['reset']}")
        print()

        # Orientation section
        print(f"{self.colors['bold']}ORIENTATION:{self.colors['reset']}")
        if ori.valid:
            print(f"  Roll:  {ori.roll:7.1f}°  |  Pitch:  {ori.pitch:7.1f}°  |  Yaw:  {ori.yaw:7.1f}°")
            print(f"  Quat:  w={ori.w:.3f}, x={ori.x:.3f}, y={ori.y:.3f}, z={ori.z:.3f}")
            print(f"  Cal:  {ori.calibration}")
        else:
            print("  {self.colors['yellow']}(Waiting for orientation data...){self.colors['reset']}")
        print()

        # Position section
        print(f"{self.colors['bold']}POSITION:{self.colors['reset']}")
        if pos.valid:
            sat_str = f"{pos.num_satellites}" if pos.num_satellites > 0 else "--"
            hdop_str = f"{pos.hdop:.1f}m" if pos.hdop is not None else "--"
            acc_str = f"±{pos.accuracy_m:.1f}m" if pos.accuracy_m is not None else "--"

            print(f"  Fix: {pos.fix_type_str:8} |  Sats: {sat_str:>2}  |  HDOP: {hdop_str:>6}  |  CEP: {acc_str}")

            if pos.latitude is not None and pos.longitude is not None:
                print(f"  Lat:  {pos.latitude:11.5f}°  |  Lon: {pos.longitude:11.5f}°  |  Alt: {pos.altitude_m:7.1f}m"
                      if pos.altitude_m is not None
                      else f"  Lat:  {pos.latitude:11.5f}°  |  Lon: {pos.longitude:11.5f}°")
            else:
                print("  {self.colors['yellow']}(Waiting for position fix...){self.colors['reset']}")
        else:
            print(f"  {self.colors['yellow']}(Waiting for position data...){self.colors['reset']}")
        print()

        # Data rate and statistics
        print(f"{self.colors['bold']}DATA RATE:{self.colors['reset']} {stats.orientation_rate:.1f} Hz orientation, "
              f"{stats.position_rate:.1f} Hz position")
        print(f"{self.colors['bold']}STATS:{self.colors['reset']} "
              f"Samples: {stats.orientation_samples} ori / {stats.position_samples} pos  |  "
              f"Errors: {stats.json_errors} JSON / {stats.parse_errors} parse  |  "
              f"Uptime: {stats.uptime}")
        print()
        print(f"{self.colors['yellow']}(Ctrl+C to exit){self.colors['reset']}")

    def run(self) -> None:
        """Run the monitor."""
        if not self.connect():
            sys.exit(1)

        # Start reader thread
        reader_thread = threading.Thread(target=self.read_loop, daemon=True)
        reader_thread.start()

        try:
            while self.running:
                self.display()
                time.sleep(0.01)
        except KeyboardInterrupt:
            print(f"\n{self.colors['yellow']}Exiting...{self.colors['reset']}", file=sys.stderr)
        finally:
            self.running = False
            reader_thread.join(timeout=2.0)
            if self.ser and self.ser.is_open:
                self.ser.close()
            if self.log_f:
                self.log_f.close()

            # Print final statistics
            with self.lock:
                self.stats.update_rates()
                stats = self.stats

            print(f"\n{self.colors['cyan']}=== Final Statistics ==={self.colors['reset']}",
                  file=sys.stderr)
            print(f"Orientation samples: {stats.orientation_samples}", file=sys.stderr)
            print(f"Position samples: {stats.position_samples}", file=sys.stderr)
            print(f"JSON errors: {stats.json_errors}", file=sys.stderr)
            print(f"Parse errors: {stats.parse_errors}", file=sys.stderr)
            print(f"Average rates: {stats.orientation_rate:.1f} Hz ori, "
                  f"{stats.position_rate:.1f} Hz pos", file=sys.stderr)
            print(f"Uptime: {stats.uptime}", file=sys.stderr)

            if self.log_file:
                print(f"Log saved to: {self.log_file}", file=sys.stderr)


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Real-time terminal monitor for auto_orientation JSON sensor output",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 real_time_monitor.py /dev/ttyACM0
  python3 real_time_monitor.py /dev/ttyACM0 --baud 115200
  python3 real_time_monitor.py /dev/ttyACM0 --log data.jsonl
  python3 real_time_monitor.py /dev/ttyACM0 --no-color
        """
    )

    parser.add_argument(
        'port',
        help='Serial port device path (e.g., /dev/ttyACM0, COM3)'
    )
    parser.add_argument(
        '--baud',
        type=int,
        default=115200,
        help='Baud rate (default: 115200)'
    )
    parser.add_argument(
        '--log',
        dest='log_file',
        help='Log raw JSON to file (JSONL format)'
    )
    parser.add_argument(
        '--no-color',
        action='store_true',
        help='Disable colored output'
    )
    parser.add_argument(
        '-q', '--quiet',
        action='store_true',
        help='Suppress display updates (logging still works)'
    )

    args = parser.parse_args()

    monitor = JSONSensorMonitor(
        port=args.port,
        baud=args.baud,
        log_file=args.log_file,
        use_color=not args.no_color,
        quiet=args.quiet
    )

    monitor.run()


if __name__ == '__main__':
    main()
