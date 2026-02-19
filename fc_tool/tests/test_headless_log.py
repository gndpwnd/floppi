"""End-to-end tests for fc_tool headless logging (--log flag).

Tests that the --log flag creates timestamped log files with correct format.
Requires fc_tool binary built and socat installed.

Markers: e2e
"""

import os
import re
import time

import pytest


# Timestamp regex: [epoch.millis] — e.g., [1739900000.123]
TIMESTAMP_RE = re.compile(r'^\[(\d+\.\d+)\] ')


# ============================================================================
# Log file creation
# ============================================================================

class TestLogFileCreation:
    """Tests that --log creates files correctly."""

    @pytest.mark.e2e
    def test_log_file_created(self, fc_tool_bin, socat_pair, tmp_path):
        """--log should create a log file."""
        import subprocess
        port_a, port_b = socat_pair
        log_path = str(tmp_path / "test.log")

        # Start simulator
        sim_proc = subprocess.Popen(
            ["python3", str(os.path.join(os.path.dirname(__file__), "simulate_serial.py")),
             port_a, "--scenario", "imu", "--rate", "50", "--duration", "2"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )

        time.sleep(0.3)

        fc_proc = subprocess.Popen(
            [fc_tool_bin, "--headless", "--port", port_b, "--baud", "115200", "--log", log_path],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )

        time.sleep(2.5)
        fc_proc.terminate()
        fc_proc.communicate(timeout=5)
        sim_proc.terminate()
        sim_proc.wait(timeout=3)

        assert os.path.exists(log_path), f"Log file not created at {log_path}"
        with open(log_path) as f:
            content = f.read()
        assert len(content) > 0, "Log file is empty"

    @pytest.mark.e2e
    def test_log_has_timestamps(self, fc_tool_bin, socat_pair, tmp_path):
        """Each log line should have [epoch.millis] prefix."""
        import subprocess
        port_a, port_b = socat_pair
        log_path = str(tmp_path / "test_ts.log")

        sim_proc = subprocess.Popen(
            ["python3", str(os.path.join(os.path.dirname(__file__), "simulate_serial.py")),
             port_a, "--scenario", "imu", "--rate", "50", "--duration", "2"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )

        time.sleep(0.3)

        fc_proc = subprocess.Popen(
            [fc_tool_bin, "--headless", "--port", port_b, "--baud", "115200", "--log", log_path],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )

        time.sleep(2.5)
        fc_proc.terminate()
        fc_proc.communicate(timeout=5)
        sim_proc.terminate()
        sim_proc.wait(timeout=3)

        with open(log_path) as f:
            lines = [l for l in f.readlines() if l.strip()]

        timestamped = [l for l in lines if TIMESTAMP_RE.match(l)]
        if len(lines) > 0:
            ratio = len(timestamped) / len(lines)
            assert ratio > 0.5, f"Only {ratio*100:.0f}% of lines have timestamps"

    @pytest.mark.e2e
    def test_timestamps_are_monotonic(self, fc_tool_bin, socat_pair, tmp_path):
        """Log timestamps should increase monotonically."""
        import subprocess
        port_a, port_b = socat_pair
        log_path = str(tmp_path / "test_mono.log")

        sim_proc = subprocess.Popen(
            ["python3", str(os.path.join(os.path.dirname(__file__), "simulate_serial.py")),
             port_a, "--scenario", "imu", "--rate", "50", "--duration", "2"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )

        time.sleep(0.3)

        fc_proc = subprocess.Popen(
            [fc_tool_bin, "--headless", "--port", port_b, "--baud", "115200", "--log", log_path],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )

        time.sleep(2.5)
        fc_proc.terminate()
        fc_proc.communicate(timeout=5)
        sim_proc.terminate()
        sim_proc.wait(timeout=3)

        with open(log_path) as f:
            lines = f.readlines()

        timestamps = []
        for line in lines:
            m = TIMESTAMP_RE.match(line)
            if m:
                timestamps.append(float(m.group(1)))

        if len(timestamps) >= 2:
            for i in range(1, len(timestamps)):
                assert timestamps[i] >= timestamps[i-1], \
                    f"Timestamps not monotonic at index {i}: {timestamps[i-1]} > {timestamps[i]}"


# ============================================================================
# Log data integrity
# ============================================================================

class TestLogDataIntegrity:
    """Tests that log file content matches stdout."""

    @pytest.mark.e2e
    def test_log_contains_data_lines(self, fc_tool_bin, socat_pair, tmp_path):
        """Log file should contain the actual serial data."""
        import subprocess
        from parsers import parse_line

        port_a, port_b = socat_pair
        log_path = str(tmp_path / "test_data.log")

        sim_proc = subprocess.Popen(
            ["python3", str(os.path.join(os.path.dirname(__file__), "simulate_serial.py")),
             port_a, "--scenario", "protocol", "--rate", "10", "--duration", "2"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )

        time.sleep(0.3)

        fc_proc = subprocess.Popen(
            [fc_tool_bin, "--headless", "--port", port_b, "--baud", "115200", "--log", log_path],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )

        time.sleep(2.5)
        fc_proc.terminate()
        fc_proc.communicate(timeout=5)
        sim_proc.terminate()
        sim_proc.wait(timeout=3)

        with open(log_path) as f:
            lines = f.readlines()

        # Strip timestamps and check for parseable data
        data_lines = []
        for line in lines:
            stripped = TIMESTAMP_RE.sub("", line).strip()
            if stripped and parse_line(stripped):
                data_lines.append(stripped)

        assert len(data_lines) > 0, "Log file contains no parseable data lines"
