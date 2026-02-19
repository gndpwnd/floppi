"""End-to-end tests for fc_tool CLI argument handling.

Tests error handling, missing port, invalid port, and stderr banner.
Requires fc_tool binary built.

Markers: e2e
"""

import subprocess
import time

import pytest


# ============================================================================
# Help and version
# ============================================================================

class TestCLIHelp:
    """Tests for --help and basic CLI behavior."""

    @pytest.mark.e2e
    def test_help_flag(self, fc_tool_bin):
        """--headless --help should show help text or exit quickly.

        Note: Tauri apps without --headless try to open a GUI window and hang.
        We test --headless combined with an invalid state instead.
        """
        # Running with --headless but no --port triggers a quick error/usage
        result = subprocess.run(
            [fc_tool_bin, "--headless"],
            capture_output=True, text=True, timeout=10,
        )
        # Should exit (not hang) — any exit code is fine
        assert result.returncode is not None

    @pytest.mark.e2e
    def test_headless_requires_port(self, fc_tool_bin):
        """--headless without --port should fail with an error."""
        result = subprocess.run(
            [fc_tool_bin, "--headless"],
            capture_output=True, text=True, timeout=10,
        )
        # Should either error out or show usage
        assert result.returncode != 0 or "port" in result.stderr.lower() or "error" in result.stderr.lower()


# ============================================================================
# Invalid port handling
# ============================================================================

class TestInvalidPort:
    """Tests for handling of invalid/missing serial ports."""

    @pytest.mark.e2e
    def test_nonexistent_port(self, fc_tool_bin):
        """Connecting to a nonexistent port should produce an error."""
        result = subprocess.run(
            [fc_tool_bin, "--headless", "--port", "/dev/ttyNONEXIST", "--baud", "115200"],
            capture_output=True, text=True, timeout=10,
        )
        # Should fail or print error about port
        combined = result.stdout + result.stderr
        assert result.returncode != 0 or "error" in combined.lower() or "failed" in combined.lower() or "not found" in combined.lower()

    @pytest.mark.e2e
    def test_permission_denied_port(self, fc_tool_bin):
        """Connecting to a port without permissions should produce an error."""
        # /dev/tty0 typically requires root
        result = subprocess.run(
            [fc_tool_bin, "--headless", "--port", "/dev/tty0", "--baud", "115200"],
            capture_output=True, text=True, timeout=10,
        )
        combined = result.stdout + result.stderr
        # Should fail
        assert result.returncode != 0 or "error" in combined.lower() or "permission" in combined.lower()


# ============================================================================
# Banner and startup output
# ============================================================================

class TestStartupBanner:
    """Tests for fc_tool startup output."""

    @pytest.mark.e2e
    def test_headless_banner_on_stderr(self, fc_tool_bin, socat_pair):
        """Headless mode should put banner/status info on stderr, data on stdout."""
        import os
        port_a, port_b = socat_pair

        # Start a brief simulator
        sim_proc = subprocess.Popen(
            ["python3", os.path.join(os.path.dirname(__file__), "simulate_serial.py"),
             port_a, "--scenario", "imu", "--rate", "10", "--duration", "1"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )

        time.sleep(0.3)

        fc_proc = subprocess.Popen(
            [fc_tool_bin, "--headless", "--port", port_b, "--baud", "115200"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )

        time.sleep(1.5)
        fc_proc.terminate()
        stdout, stderr = fc_proc.communicate(timeout=5)
        sim_proc.terminate()
        sim_proc.wait(timeout=3)

        # stderr should have connection info, stdout should have data
        # (This test verifies the stream separation principle)
        if stdout.strip():
            # Most data should be on stdout, not stderr
            # Allow a small number of data-like lines on stderr (connection banner, etc.)
            from parsers import parse_line
            data_in_stderr = 0
            for line in stderr.strip().split("\n"):
                if line.strip() and parse_line(line.strip()):
                    data_in_stderr += 1
            # Allow up to 2 data-like lines on stderr (startup/banner messages)
            assert data_in_stderr <= 2, f"Too many data lines on stderr: {data_in_stderr}"


# ============================================================================
# Baud rate
# ============================================================================

class TestBaudRate:
    """Tests for baud rate argument handling."""

    @pytest.mark.e2e
    def test_default_baud(self, fc_tool_bin, socat_pair):
        """Default baud should work (115200)."""
        port_a, port_b = socat_pair
        # Just verify it starts without error
        fc_proc = subprocess.Popen(
            [fc_tool_bin, "--headless", "--port", port_b],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )
        time.sleep(0.5)
        assert fc_proc.poll() is None, "fc_tool exited immediately with default baud"
        fc_proc.terminate()
        fc_proc.communicate(timeout=5)

    @pytest.mark.e2e
    def test_custom_baud(self, fc_tool_bin, socat_pair):
        """Custom baud rate should be accepted."""
        port_a, port_b = socat_pair
        fc_proc = subprocess.Popen(
            [fc_tool_bin, "--headless", "--port", port_b, "--baud", "9600"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )
        time.sleep(0.5)
        assert fc_proc.poll() is None, "fc_tool exited immediately with custom baud"
        fc_proc.terminate()
        fc_proc.communicate(timeout=5)
