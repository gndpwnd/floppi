"""Integration tests for simulate_serial.py.

Validates that each simulator scenario produces correctly formatted output
that the parsers can consume. Runs simulate_serial.py with --stdout.

Markers: integration
"""

import pytest

from parsers import parse_line, parse_ansi, parse_dashboard_line


# ============================================================================
# IMU scenario
# ============================================================================

class TestIMUScenario:
    """Test the 'imu' scenario output format and content."""

    @pytest.mark.integration
    def test_produces_output(self, run_simulator_stdout):
        lines = run_simulator_stdout("imu", rate=50, duration=1)
        assert len(lines) > 0

    @pytest.mark.integration
    def test_all_lines_parse(self, run_simulator_stdout):
        lines = run_simulator_stdout("imu", rate=50, duration=1)
        for line in lines:
            result = parse_line(line)
            assert result is not None, f"Failed to parse: {line!r}"

    @pytest.mark.integration
    def test_six_channels_per_line(self, run_simulator_stdout):
        lines = run_simulator_stdout("imu", rate=50, duration=0.5)
        for line in lines:
            result = parse_line(line)
            assert len(result) == 6, f"Expected 6 channels, got {len(result)} in: {line!r}"

    @pytest.mark.integration
    def test_correct_plot_ids(self, run_simulator_stdout):
        lines = run_simulator_stdout("imu", rate=50, duration=0.5)
        result = parse_line(lines[0])
        # accel on plot 0, gyro on plot 1
        assert all(r.plot_id == 0 for r in result[:3])
        assert all(r.plot_id == 1 for r in result[3:])

    @pytest.mark.integration
    def test_correct_names(self, run_simulator_stdout):
        lines = run_simulator_stdout("imu", rate=50, duration=0.5)
        result = parse_line(lines[0])
        names = [r.name for r in result]
        assert names == ["ax", "ay", "az", "gx", "gy", "gz"]


# ============================================================================
# Sine scenario
# ============================================================================

class TestSineScenario:
    @pytest.mark.integration
    def test_produces_output(self, run_simulator_stdout):
        lines = run_simulator_stdout("sine", rate=50, duration=0.5)
        assert len(lines) > 0

    @pytest.mark.integration
    def test_three_channels(self, run_simulator_stdout):
        lines = run_simulator_stdout("sine", rate=50, duration=0.5)
        result = parse_line(lines[0])
        assert len(result) == 3
        names = [r.name for r in result]
        assert "1Hz" in names
        assert "2.5Hz" in names
        assert "5Hz" in names


# ============================================================================
# Mixed scenario
# ============================================================================

class TestMixedScenario:
    @pytest.mark.integration
    def test_all_lines_parse(self, run_simulator_stdout):
        lines = run_simulator_stdout("mixed", rate=50, duration=1)
        for line in lines:
            result = parse_line(line)
            assert result is not None, f"Failed to parse: {line!r}"

    @pytest.mark.integration
    def test_four_format_cycle(self, run_simulator_stdout):
        """Lines should cycle through 4 formats."""
        lines = run_simulator_stdout("mixed", rate=10, duration=1)
        assert len(lines) >= 4  # At least one full cycle


# ============================================================================
# ANSI scenario
# ============================================================================

class TestANSIScenario:
    @pytest.mark.integration
    def test_produces_output(self, run_simulator_stdout):
        lines = run_simulator_stdout("ansi", rate=10, duration=0.5)
        assert len(lines) > 0

    @pytest.mark.integration
    def test_contains_ansi_codes(self, run_simulator_stdout):
        lines = run_simulator_stdout("ansi", rate=10, duration=1)
        has_ansi = any("\033[" in line for line in lines)
        assert has_ansi, "ANSI scenario should produce escape codes"

    @pytest.mark.integration
    def test_ansi_lines_parse(self, run_simulator_stdout):
        lines = run_simulator_stdout("ansi", rate=10, duration=1)
        for line in lines:
            if "\033[" in line:
                spans = parse_ansi(line)
                assert len(spans) > 0


# ============================================================================
# Dashboard scenario
# ============================================================================

class TestDashboardScenario:
    @pytest.mark.integration
    def test_produces_key_value_pairs(self, run_simulator_stdout):
        lines = run_simulator_stdout("dashboard", rate=10, duration=0.5)
        for line in lines:
            pairs = parse_dashboard_line(line)
            assert len(pairs) > 0, f"No KV pairs in: {line!r}"

    @pytest.mark.integration
    def test_expected_keys(self, run_simulator_stdout):
        lines = run_simulator_stdout("dashboard", rate=10, duration=0.5)
        pairs = parse_dashboard_line(lines[0])
        keys = [k for k, v in pairs]
        assert "battery" in keys
        assert "loop" in keys
        assert "alt" in keys
        assert "armed" in keys


# ============================================================================
# Protocol scenario (regression)
# ============================================================================

class TestProtocolScenario:
    @pytest.mark.integration
    def test_all_lines_parse(self, run_simulator_stdout):
        lines = run_simulator_stdout("protocol", rate=10, duration=1)
        for line in lines:
            result = parse_line(line)
            assert result is not None, f"Failed to parse: {line!r}"

    @pytest.mark.integration
    def test_format_cycle(self, run_simulator_stdout):
        """Each cycle of 4 lines should use different formats."""
        lines = run_simulator_stdout("protocol", rate=10, duration=1)
        assert len(lines) >= 4
        # Line 0: named_plot@0:value
        r0 = parse_line(lines[0])
        assert r0[0].name == "named_plot" and r0[0].plot_id == 0
        # Line 1: colon_fmt:value
        r1 = parse_line(lines[1])
        assert r1[0].name == "colon_fmt"
        # Line 2: equals_fmt=value
        r2 = parse_line(lines[2])
        assert r2[0].name == "equals_fmt"
        # Line 3: plain number (CSV fallback)
        r3 = parse_line(lines[3])
        assert r3[0].name == "ch0"


# ============================================================================
# Noise scenario
# ============================================================================

class TestNoiseScenario:
    @pytest.mark.integration
    def test_produces_output(self, run_simulator_stdout):
        lines = run_simulator_stdout("noise", rate=50, duration=0.5)
        assert len(lines) > 0

    @pytest.mark.integration
    def test_all_lines_parse(self, run_simulator_stdout):
        lines = run_simulator_stdout("noise", rate=50, duration=0.5)
        for line in lines:
            result = parse_line(line)
            assert result is not None


# ============================================================================
# Stress scenario
# ============================================================================

class TestStressScenario:
    @pytest.mark.integration
    def test_produces_output(self, run_simulator_stdout):
        lines = run_simulator_stdout("stress", rate=50, duration=0.5)
        assert len(lines) > 0

    @pytest.mark.integration
    def test_many_channels_per_line(self, run_simulator_stdout):
        lines = run_simulator_stdout("stress", rate=50, duration=0.5)
        result = parse_line(lines[0])
        assert len(result) == 15  # 15 channels across 15 plots
