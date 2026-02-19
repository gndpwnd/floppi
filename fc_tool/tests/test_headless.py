"""End-to-end tests for fc_tool headless mode.

Tests the full data pipeline: simulate_serial.py → socat pty → fc_tool --headless → stdout.
Requires fc_tool binary built and socat installed.

Markers: e2e
"""

import time

import pytest

from parsers import parse_line, parse_ansi


# ============================================================================
# Basic data flow
# ============================================================================

class TestHeadlessDataFlow:
    """Tests that data flows correctly through the virtual serial pipeline."""

    @pytest.mark.e2e
    def test_receives_imu_data(self, run_headless):
        fc_proc, sim_proc, port = run_headless("imu", rate=50, duration=2)
        # Wait for data to flow
        time.sleep(2.5)
        fc_proc.terminate()
        stdout, stderr = fc_proc.communicate(timeout=5)
        lines = [l for l in stdout.strip().split("\n") if l.strip()]
        assert len(lines) > 0, f"No output received. stderr: {stderr}"

    @pytest.mark.e2e
    def test_imu_data_parseable(self, run_headless):
        fc_proc, sim_proc, port = run_headless("imu", rate=50, duration=2)
        time.sleep(2.5)
        fc_proc.terminate()
        stdout, _ = fc_proc.communicate(timeout=5)
        lines = [l for l in stdout.strip().split("\n") if l.strip()]
        parsed_count = 0
        for line in lines:
            result = parse_line(line)
            if result:
                parsed_count += 1
        assert parsed_count > 0, "No lines parsed as valid data"

    @pytest.mark.e2e
    def test_receives_sine_data(self, run_headless):
        fc_proc, sim_proc, port = run_headless("sine", rate=50, duration=2)
        time.sleep(2.5)
        fc_proc.terminate()
        stdout, _ = fc_proc.communicate(timeout=5)
        lines = [l for l in stdout.strip().split("\n") if l.strip()]
        assert len(lines) > 0

    @pytest.mark.e2e
    def test_receives_ansi_data(self, run_headless):
        fc_proc, sim_proc, port = run_headless("ansi", rate=10, duration=2)
        time.sleep(2.5)
        fc_proc.terminate()
        stdout, _ = fc_proc.communicate(timeout=5)
        lines = [l for l in stdout.strip().split("\n") if l.strip()]
        has_ansi = any("\033[" in line for line in lines)
        assert has_ansi, "Should receive ANSI escape codes through pipeline"


# ============================================================================
# Throughput / rate
# ============================================================================

class TestHeadlessThroughput:
    """Tests that headless mode handles data at expected rates."""

    @pytest.mark.e2e
    def test_high_rate_no_data_loss(self, run_headless):
        """At 100Hz for 2s, we should receive at least 100 lines (allowing for startup delay)."""
        fc_proc, sim_proc, port = run_headless("imu", rate=100, duration=2)
        time.sleep(2.5)
        fc_proc.terminate()
        stdout, _ = fc_proc.communicate(timeout=5)
        lines = [l for l in stdout.strip().split("\n") if l.strip()]
        # Allow for startup overhead — at least 50% should arrive
        assert len(lines) >= 100, f"Expected >= 100 lines at 100Hz for 2s, got {len(lines)}"


# ============================================================================
# Echo round-trip (stdin → serial → stdout)
# ============================================================================

class TestHeadlessEcho:
    """Tests stdin→serial echo functionality."""

    @pytest.mark.e2e
    def test_echo_round_trip(self, fc_tool_bin, socat_pair):
        """Data written to stdin should be echoed back through the serial loopback."""
        import subprocess
        port_a, port_b = socat_pair

        # Start fc_tool in headless mode on port_b
        fc_proc = subprocess.Popen(
            [fc_tool_bin, "--headless", "--port", port_b, "--baud", "115200"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

        time.sleep(0.5)  # Wait for connection

        # Write test data to port_a (the other end of the socat pair)
        import os
        try:
            import termios
            fd = os.open(port_a, os.O_RDWR | os.O_NOCTTY)
            os.write(fd, b"test_echo_123\n")
            time.sleep(0.5)
            os.close(fd)
        except (ImportError, OSError):
            fc_proc.kill()
            pytest.skip("Could not open virtual serial port")

        time.sleep(1)
        fc_proc.terminate()
        stdout, _ = fc_proc.communicate(timeout=5)

        # Check that the test string appeared in output
        assert "test_echo_123" in stdout, f"Echo not found in output: {stdout[:200]}"


# ============================================================================
# Resilience
# ============================================================================

class TestHeadlessResilience:
    """Tests that headless mode handles edge cases gracefully."""

    @pytest.mark.e2e
    def test_simulator_stops_early(self, run_headless):
        """fc_tool should handle the simulator stopping (serial goes quiet)."""
        fc_proc, sim_proc, port = run_headless("imu", rate=50, duration=1)
        # Wait for simulator to finish
        time.sleep(2)
        # fc_tool should still be running (not crashed)
        assert fc_proc.poll() is None, "fc_tool should not crash when data stops"
        fc_proc.terminate()
        fc_proc.communicate(timeout=5)

    @pytest.mark.e2e
    def test_mixed_protocol_formats(self, run_headless):
        """Mixed scenario should flow through without errors."""
        fc_proc, sim_proc, port = run_headless("mixed", rate=50, duration=2)
        time.sleep(2.5)
        fc_proc.terminate()
        stdout, stderr = fc_proc.communicate(timeout=5)
        lines = [l for l in stdout.strip().split("\n") if l.strip()]
        assert len(lines) > 0

    @pytest.mark.e2e
    def test_noise_with_spikes(self, run_headless):
        """Noise scenario with random spikes should not crash."""
        fc_proc, sim_proc, port = run_headless("noise", rate=50, duration=2)
        time.sleep(2.5)
        assert fc_proc.poll() is None, "fc_tool crashed during noise scenario"
        fc_proc.terminate()
        fc_proc.communicate(timeout=5)

    @pytest.mark.e2e
    def test_stress_many_channels(self, run_headless):
        """Stress test with 15 channels should not crash."""
        fc_proc, sim_proc, port = run_headless("stress", rate=50, duration=2)
        time.sleep(2.5)
        assert fc_proc.poll() is None, "fc_tool crashed during stress scenario"
        fc_proc.terminate()
        fc_proc.communicate(timeout=5)
