"""E2E resilience tests for fc_tool headless mode.

Tests edge cases: intermittent data, bursts, simulator restart,
long-running stability.

Markers: e2e
"""

import time
import subprocess

import pytest


# ============================================================================
# Intermittent resilience
# ============================================================================

class TestIntermittentResilience:
    """Tests that fc_tool survives data gaps without crashing."""

    @pytest.mark.e2e
    def test_survives_data_gaps(self, run_headless):
        """fc_tool should stay alive through 1s-on / 1s-off data cycles."""
        fc_proc, sim_proc, port = run_headless("intermittent", rate=50, duration=4)
        time.sleep(4.5)
        assert fc_proc.poll() is None, (
            f"fc_tool crashed during intermittent data (exit code: {fc_proc.returncode})"
        )
        fc_proc.terminate()
        stdout, stderr = fc_proc.communicate(timeout=5)
        lines = [l for l in stdout.strip().split("\n") if l.strip()]
        assert len(lines) > 0, f"No output received despite data periods. stderr: {stderr}"


# ============================================================================
# Burst handling
# ============================================================================

class TestBurstHandling:
    """Tests that fc_tool handles rapid bursts of data."""

    @pytest.mark.e2e
    def test_survives_bursts(self, run_headless):
        """fc_tool should handle bursts of 50 lines followed by silence."""
        fc_proc, sim_proc, port = run_headless("burst", rate=50, duration=3)
        time.sleep(3.5)
        assert fc_proc.poll() is None, (
            f"fc_tool crashed during burst data (exit code: {fc_proc.returncode})"
        )
        fc_proc.terminate()
        stdout, stderr = fc_proc.communicate(timeout=5)
        lines = [l for l in stdout.strip().split("\n") if l.strip()]
        assert len(lines) > 0, f"No output received from burst scenario. stderr: {stderr}"


# ============================================================================
# Calibration stream
# ============================================================================

class TestCalibrationStream:
    """Tests that mixed calibration content does not crash fc_tool."""

    @pytest.mark.e2e
    def test_mixed_content_no_crash(self, run_headless):
        """Interleaved text, ANSI, telemetry, and dashboard should not crash fc_tool."""
        fc_proc, sim_proc, port = run_headless("calibration", rate=10, duration=3)
        time.sleep(3.5)
        assert fc_proc.poll() is None, (
            f"fc_tool crashed during calibration stream (exit code: {fc_proc.returncode})"
        )
        fc_proc.terminate()
        stdout, stderr = fc_proc.communicate(timeout=5)
        lines = [l for l in stdout.strip().split("\n") if l.strip()]
        assert len(lines) > 0, f"No output received. stderr: {stderr}"

        # Verify output contains both parseable and non-parseable lines
        from parsers import parse_line
        parseable = 0
        non_parseable = 0
        for line in lines:
            result = parse_line(line)
            if result:
                parseable += 1
            else:
                non_parseable += 1

        assert parseable > 0, "Should have some parseable telemetry lines"
        assert non_parseable > 0, "Should have some non-parseable text/status lines"


# ============================================================================
# Long-running stability
# ============================================================================

class TestLongRunning:
    """Tests long-running stability of fc_tool headless mode."""

    @pytest.mark.e2e
    @pytest.mark.slow
    def test_30s_stability(self, run_headless):
        """fc_tool should remain stable for 30 seconds of continuous data."""
        fc_proc, sim_proc, port = run_headless("imu", rate=50, duration=30)
        time.sleep(31)
        assert fc_proc.poll() is None, (
            f"fc_tool crashed during 30s run (exit code: {fc_proc.returncode})"
        )
        fc_proc.terminate()
        stdout, stderr = fc_proc.communicate(timeout=5)
        lines = [l for l in stdout.strip().split("\n") if l.strip()]

        # At 50Hz for 30s, expect at least 500 lines (generous margin for system load)
        assert len(lines) > 500, (
            f"Expected >500 lines for 30s at 50Hz, got {len(lines)}. stderr: {stderr}"
        )
