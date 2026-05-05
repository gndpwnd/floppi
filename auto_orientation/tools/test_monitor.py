#!/usr/bin/env python3
"""Unit tests for real_time_monitor.py

Tests:
- Quaternion to Euler angle conversion
- JSON parsing and data extraction
- Calibration status formatting
- Data rate calculations
"""

import json
import math
import sys
import unittest
from datetime import datetime, timedelta
from io import StringIO
from unittest.mock import MagicMock, patch

# Import the monitor module
sys.path.insert(0, '/home/devel/floppi/auto_orientation/tools')
from real_time_monitor import (
    OrientationData, PositionData, CalibrationStatus, SensorStats, JSONSensorMonitor
)


class TestQuaternionToEuler(unittest.TestCase):
    """Test quaternion to Euler angle conversion."""

    def test_identity_quaternion(self):
        """Identity quaternion (1,0,0,0) should give roll/pitch/yaw = 0."""
        ori = OrientationData(w=1.0, x=0.0, y=0.0, z=0.0)
        ori.compute_euler()

        self.assertAlmostEqual(ori.roll, 0.0, places=1)
        self.assertAlmostEqual(ori.pitch, 0.0, places=1)
        self.assertAlmostEqual(ori.yaw, 0.0, places=1)

    def test_90_degree_roll(self):
        """Rotation of 90 degrees around x-axis."""
        # q = cos(45°) + sin(45°)*i = 0.707 + 0.707*i
        ori = OrientationData(w=0.707, x=0.707, y=0.0, z=0.0)
        ori.compute_euler()

        self.assertAlmostEqual(ori.roll, 90.0, places=0)
        self.assertAlmostEqual(ori.pitch, 0.0, places=1)
        self.assertAlmostEqual(ori.yaw, 0.0, places=1)

    def test_90_degree_pitch(self):
        """Rotation of 90 degrees around y-axis."""
        # q = cos(45°) + sin(45°)*j = 0.707 + 0.707*j
        ori = OrientationData(w=0.707, x=0.0, y=0.707, z=0.0)
        ori.compute_euler()

        self.assertAlmostEqual(ori.roll, 0.0, places=1)
        # Allow some numerical precision error (gimbal lock proximity)
        self.assertTrue(85.0 < ori.pitch < 90.0)
        self.assertAlmostEqual(ori.yaw, 0.0, places=1)

    def test_90_degree_yaw(self):
        """Rotation of 90 degrees around z-axis."""
        # q = cos(45°) + sin(45°)*k = 0.707 + 0.707*k
        ori = OrientationData(w=0.707, x=0.0, y=0.0, z=0.707)
        ori.compute_euler()

        self.assertAlmostEqual(ori.roll, 0.0, places=1)
        self.assertAlmostEqual(ori.pitch, 0.0, places=1)
        self.assertAlmostEqual(ori.yaw, 90.0, places=0)


class TestCalibrationStatus(unittest.TestCase):
    """Test calibration status formatting."""

    def test_no_calibration(self):
        """All sensors uncalibrated (level 0)."""
        cal = CalibrationStatus(system=0, accel=0, gyro=0, mag=0)
        result = str(cal)
        self.assertIn("░░░ SYS", result)
        self.assertIn("░░░ ACC", result)
        self.assertIn("░░░ GYR", result)
        self.assertIn("░░░ MAG", result)

    def test_partial_calibration(self):
        """Some sensors partially calibrated."""
        cal = CalibrationStatus(system=3, accel=2, gyro=1, mag=0)
        result = str(cal)
        self.assertIn("███ SYS", result)
        self.assertIn("██░ ACC", result)
        self.assertIn("█░░ GYR", result)
        self.assertIn("░░░ MAG", result)

    def test_full_calibration(self):
        """All sensors fully calibrated (level 3)."""
        cal = CalibrationStatus(system=3, accel=3, gyro=3, mag=3)
        result = str(cal)
        self.assertIn("███ SYS", result)
        self.assertIn("███ ACC", result)
        self.assertIn("███ GYR", result)
        self.assertIn("███ MAG", result)


class TestPositionData(unittest.TestCase):
    """Test position data parsing."""

    def test_fix_type_strings(self):
        """Test fix type string conversion."""
        pos = PositionData(fix_quality=0)
        self.assertEqual(pos.fix_type_str, "No fix")

        pos.fix_quality = 1
        self.assertEqual(pos.fix_type_str, "GPS")

        pos.fix_quality = 2
        self.assertEqual(pos.fix_type_str, "DGPS")

        pos.fix_quality = 4
        self.assertEqual(pos.fix_type_str, "RTK")

    def test_gps_data_parsing(self):
        """Test GPS position parsing from JSON."""
        pos = PositionData(
            valid=True,
            latitude=30.27123,
            longitude=-97.74156,
            altitude_m=158.2,
            accuracy_m=4.5,
            num_satellites=8,
            fix_quality=1
        )

        self.assertTrue(pos.valid)
        self.assertAlmostEqual(pos.latitude, 30.27123, places=5)
        self.assertAlmostEqual(pos.longitude, -97.74156, places=5)
        self.assertAlmostEqual(pos.altitude_m, 158.2, places=1)


class TestSensorStats(unittest.TestCase):
    """Test statistics tracking."""

    def test_uptime_formatting(self):
        """Test uptime string formatting."""
        stats = SensorStats()
        stats.start_time = datetime.now() - timedelta(hours=1, minutes=30, seconds=45)

        uptime = stats.uptime
        self.assertIn("01h 30m", uptime)

    def test_data_rates(self):
        """Test data rate calculation."""
        stats = SensorStats(orientation_samples=100, position_samples=50)
        stats.start_time = datetime.now() - timedelta(seconds=10)
        stats.update_rates()

        # After 10 seconds with 100 samples: 10 Hz
        self.assertAlmostEqual(stats.orientation_rate, 10.0, places=0)
        self.assertAlmostEqual(stats.position_rate, 5.0, places=0)


class TestJSONParsing(unittest.TestCase):
    """Test JSON line parsing."""

    def test_orientation_json_parsing(self):
        """Test parsing orientation JSON."""
        json_line = json.dumps({
            "timestamp_ms": 123456,
            "orientation": {
                "valid": True,
                "quaternion": {"w": 0.707, "x": 0.0, "y": 0.0, "z": 0.707},
                "calibration": {"system": 3, "accel": 3, "gyro": 3, "mag": 2}
            },
            "position": {
                "valid": False,
                "latitude": None,
                "longitude": None,
                "altitude_m": None,
                "accuracy_m": None,
                "num_satellites": 0,
                "fix_quality": 0
            }
        })

        monitor = JSONSensorMonitor('/dev/null', use_color=False)
        monitor._process_line(json_line)

        with monitor.lock:
            self.assertTrue(monitor.orientation.valid)
            self.assertAlmostEqual(monitor.orientation.w, 0.707, places=2)
            self.assertAlmostEqual(monitor.orientation.z, 0.707, places=2)
            self.assertEqual(monitor.orientation.calibration.system, 3)
            self.assertEqual(monitor.stats.orientation_samples, 1)

    def test_position_json_parsing(self):
        """Test parsing position JSON."""
        json_line = json.dumps({
            "timestamp_ms": 123456,
            "orientation": {
                "valid": False,
                "quaternion": {"w": 0, "x": 0, "y": 0, "z": 0},
                "calibration": {"system": 0, "accel": 0, "gyro": 0, "mag": 0}
            },
            "position": {
                "valid": True,
                "latitude": 30.27123,
                "longitude": -97.74156,
                "altitude_m": 158.2,
                "accuracy_m": 4.5,
                "num_satellites": 8,
                "fix_quality": 1,
                "hdop": 0.9
            }
        })

        monitor = JSONSensorMonitor('/dev/null', use_color=False)
        monitor._process_line(json_line)

        with monitor.lock:
            self.assertTrue(monitor.position.valid)
            self.assertAlmostEqual(monitor.position.latitude, 30.27123, places=5)
            self.assertAlmostEqual(monitor.position.longitude, -97.74156, places=5)
            self.assertEqual(monitor.position.num_satellites, 8)
            self.assertEqual(monitor.stats.position_samples, 1)

    def test_invalid_json_handling(self):
        """Test handling of invalid JSON."""
        monitor = JSONSensorMonitor('/dev/null', use_color=False)
        monitor._process_line("This is not JSON!")

        with monitor.lock:
            self.assertEqual(monitor.stats.json_errors, 1)

    def test_malformed_json_data(self):
        """Test handling of malformed JSON data."""
        json_line = json.dumps({
            "timestamp_ms": 123456,
            "orientation": {
                "valid": True,
                "quaternion": {"w": "not_a_number", "x": 0, "y": 0, "z": 0},
                "calibration": {"system": 3, "accel": 3, "gyro": 3, "mag": 2}
            },
            "position": {"valid": False}
        })

        monitor = JSONSensorMonitor('/dev/null', use_color=False)
        monitor._process_line(json_line)

        with monitor.lock:
            self.assertEqual(monitor.stats.parse_errors, 1)


class TestEulerAngles(unittest.TestCase):
    """Test Euler angle calculations with known values."""

    def test_known_orientation_1(self):
        """Test with known orientation values."""
        # Leveled vehicle: roll=0, pitch=0, yaw=0 should be (1,0,0,0)
        ori = OrientationData(w=1.0, x=0.0, y=0.0, z=0.0)
        ori.compute_euler()

        self.assertAlmostEqual(ori.roll, 0.0, places=1)
        self.assertAlmostEqual(ori.pitch, 0.0, places=1)
        self.assertAlmostEqual(ori.yaw, 0.0, places=1)

    def test_normalized_quaternion(self):
        """Test with normalized quaternion."""
        # Normalized: w² + x² + y² + z² = 1
        ori = OrientationData(w=0.5, x=0.5, y=0.5, z=0.5)
        ori.compute_euler()

        # Just verify it computes without errors
        self.assertIsNotNone(ori.roll)
        self.assertIsNotNone(ori.pitch)
        self.assertIsNotNone(ori.yaw)

    def test_pitch_bounds(self):
        """Test that pitch stays within [-90, 90] degrees."""
        # Try various quaternions
        for i in range(10):
            ori = OrientationData(
                w=0.7 + i * 0.01,
                x=0.1 + i * 0.02,
                y=0.2 + i * 0.01,
                z=0.3 + i * 0.03
            )
            ori.compute_euler()

            # Pitch should be within [-90, 90]
            self.assertGreaterEqual(ori.pitch, -90.0)
            self.assertLessEqual(ori.pitch, 90.0)


def run_tests():
    """Run all tests."""
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()

    suite.addTests(loader.loadTestsFromTestCase(TestQuaternionToEuler))
    suite.addTests(loader.loadTestsFromTestCase(TestCalibrationStatus))
    suite.addTests(loader.loadTestsFromTestCase(TestPositionData))
    suite.addTests(loader.loadTestsFromTestCase(TestSensorStats))
    suite.addTests(loader.loadTestsFromTestCase(TestJSONParsing))
    suite.addTests(loader.loadTestsFromTestCase(TestEulerAngles))

    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)

    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    sys.exit(run_tests())
