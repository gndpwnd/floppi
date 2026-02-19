"""Integration tests for new simulation scenarios.

Validates that each new scenario produces correctly formatted output
that the parsers can consume. Uses --stdout mode (no binary needed).

Markers: integration
"""

import pytest

from parsers import parse_line, parse_ansi, parse_dashboard_line


# ============================================================================
# Ramp scenario
# ============================================================================

class TestRampScenario:
    """Test the 'ramp' scenario: linear ramp on plot 0 + step on plot 1."""

    @pytest.mark.integration
    def test_produces_output(self, run_simulator_stdout):
        lines = run_simulator_stdout("ramp", rate=50, duration=1)
        assert len(lines) > 0

    @pytest.mark.integration
    def test_all_lines_parse(self, run_simulator_stdout):
        lines = run_simulator_stdout("ramp", rate=50, duration=1)
        for line in lines:
            result = parse_line(line)
            assert result is not None, f"Failed to parse: {line!r}"

    @pytest.mark.integration
    def test_two_channels_per_line(self, run_simulator_stdout):
        lines = run_simulator_stdout("ramp", rate=50, duration=0.5)
        for line in lines:
            result = parse_line(line)
            assert len(result) == 2, f"Expected 2 channels, got {len(result)} in: {line!r}"

    @pytest.mark.integration
    def test_correct_plot_ids(self, run_simulator_stdout):
        lines = run_simulator_stdout("ramp", rate=50, duration=0.5)
        for line in lines:
            result = parse_line(line)
            assert result[0].plot_id == 0, f"ramp should be on plot 0, got {result[0].plot_id}"
            assert result[1].plot_id == 1, f"step should be on plot 1, got {result[1].plot_id}"

    @pytest.mark.integration
    def test_ramp_values_increase(self, run_simulator_stdout):
        lines = run_simulator_stdout("ramp", rate=50, duration=1)
        ramp_values = []
        for line in lines:
            result = parse_line(line)
            # First data point is the ramp channel
            ramp_values.append(result[0].value)
        assert ramp_values[0] < ramp_values[-1], (
            f"Ramp should increase: first={ramp_values[0]}, last={ramp_values[-1]}"
        )


# ============================================================================
# Intermittent scenario
# ============================================================================

class TestIntermittentScenario:
    """Test the 'intermittent' scenario: 1s data then 1s silence, repeating."""

    @pytest.mark.integration
    def test_produces_output(self, run_simulator_stdout):
        lines = run_simulator_stdout("intermittent", rate=50, duration=3)
        assert len(lines) > 0

    @pytest.mark.integration
    def test_all_lines_parse(self, run_simulator_stdout):
        lines = run_simulator_stdout("intermittent", rate=50, duration=3)
        for line in lines:
            result = parse_line(line)
            assert result is not None, f"Failed to parse: {line!r}"

    @pytest.mark.integration
    def test_has_gaps(self, run_simulator_stdout):
        """Intermittent produces fewer lines than rate*duration because of silence gaps."""
        rate = 50
        duration = 3
        lines = run_simulator_stdout("intermittent", rate=rate, duration=duration)
        max_possible = rate * duration
        # With 1s on / 1s off, we should get roughly half (with some tolerance)
        assert len(lines) < max_possible * 0.8, (
            f"Expected gaps: got {len(lines)} lines, max possible {max_possible}"
        )


# ============================================================================
# Burst scenario
# ============================================================================

class TestBurstScenario:
    """Test the 'burst' scenario: rapid bursts of 50 lines then 0.5s silence."""

    @pytest.mark.integration
    def test_produces_output(self, run_simulator_stdout):
        lines = run_simulator_stdout("burst", rate=50, duration=2)
        assert len(lines) > 0

    @pytest.mark.integration
    def test_all_lines_parse(self, run_simulator_stdout):
        lines = run_simulator_stdout("burst", rate=50, duration=2)
        for line in lines:
            result = parse_line(line)
            assert result is not None, f"Failed to parse: {line!r}"

    @pytest.mark.integration
    def test_burst_pattern(self, run_simulator_stdout):
        """Bursts produce more lines than rate*duration because bursts ignore rate."""
        rate = 50
        duration = 2
        lines = run_simulator_stdout("burst", rate=rate, duration=duration)
        normal_count = rate * duration
        # Bursts send 50 lines with no sleep, so total should exceed normal rate
        assert len(lines) > normal_count, (
            f"Burst should produce more than {normal_count} lines, got {len(lines)}"
        )


# ============================================================================
# Calibration scenario
# ============================================================================

class TestCalibrationScenario:
    """Test the 'calibration' scenario: mixed text + telemetry + ANSI + dashboard."""

    @pytest.mark.integration
    def test_produces_output(self, run_simulator_stdout):
        lines = run_simulator_stdout("calibration", rate=10, duration=2)
        assert len(lines) > 0

    @pytest.mark.integration
    def test_has_mixed_content(self, run_simulator_stdout):
        """Output should contain data lines, plain text, ANSI, and dashboard lines."""
        lines = run_simulator_stdout("calibration", rate=10, duration=2)

        has_data = False
        has_plain_text = False
        has_ansi = False
        has_dashboard = False

        for line in lines:
            parsed = parse_line(line)
            dashboard = parse_dashboard_line(line)

            if "\033[" in line:
                has_ansi = True
            if parsed is not None and "@" in line:
                has_data = True
            if dashboard and "=" in line and "@" not in line:
                has_dashboard = True
            if parsed is None and "\033[" not in line and "=" not in line:
                has_plain_text = True

        assert has_data, "Should contain telemetry data lines"
        assert has_plain_text, "Should contain plain text lines"
        assert has_ansi, "Should contain ANSI escape codes"
        assert has_dashboard, "Should contain dashboard key=value lines"

    @pytest.mark.integration
    def test_telemetry_lines_parse(self, run_simulator_stdout):
        """Lines containing @ (telemetry format) should parse correctly."""
        lines = run_simulator_stdout("calibration", rate=10, duration=2)
        telemetry_lines = [l for l in lines if "@" in l]
        assert len(telemetry_lines) > 0, "Should have at least some telemetry lines"
        for line in telemetry_lines:
            result = parse_line(line)
            assert result is not None, f"Telemetry line failed to parse: {line!r}"


# ============================================================================
# Multi-plot scenario
# ============================================================================

class TestMultiPlotScenario:
    """Test the 'multi_plot' scenario: 5 variables on 5 different plots."""

    @pytest.mark.integration
    def test_produces_output(self, run_simulator_stdout):
        lines = run_simulator_stdout("multi_plot", rate=50, duration=1)
        assert len(lines) > 0

    @pytest.mark.integration
    def test_five_channels(self, run_simulator_stdout):
        lines = run_simulator_stdout("multi_plot", rate=50, duration=0.5)
        for line in lines:
            result = parse_line(line)
            assert len(result) == 5, f"Expected 5 channels, got {len(result)} in: {line!r}"

    @pytest.mark.integration
    def test_five_different_plots(self, run_simulator_stdout):
        lines = run_simulator_stdout("multi_plot", rate=50, duration=0.5)
        result = parse_line(lines[0])
        plot_ids = sorted(set(r.plot_id for r in result))
        assert plot_ids == [0, 1, 2, 3, 4], f"Expected plots 0-4, got {plot_ids}"

    @pytest.mark.integration
    def test_correct_names(self, run_simulator_stdout):
        lines = run_simulator_stdout("multi_plot", rate=50, duration=0.5)
        result = parse_line(lines[0])
        names = [r.name for r in result]
        assert names == ["temp", "pressure", "humidity", "voltage", "current"]


# ============================================================================
# Mixed dashboard scenario
# ============================================================================

class TestMixedDashboardScenario:
    """Test the 'mixed_dashboard' scenario: alternating plotter and dashboard."""

    @pytest.mark.integration
    def test_produces_output(self, run_simulator_stdout):
        lines = run_simulator_stdout("mixed_dashboard", rate=50, duration=1)
        assert len(lines) > 0

    @pytest.mark.integration
    def test_has_plotter_and_dashboard(self, run_simulator_stdout):
        """Some lines should be plotter data, some should be dashboard data."""
        lines = run_simulator_stdout("mixed_dashboard", rate=50, duration=1)
        plotter_count = 0
        dashboard_count = 0
        for line in lines:
            parsed = parse_line(line)
            dashboard = parse_dashboard_line(line)
            # Plotter lines use @ format
            if parsed is not None and "@" in line:
                plotter_count += 1
            # Dashboard lines use key=value without @ format
            elif dashboard and "=" in line and "@" not in line:
                dashboard_count += 1
        assert plotter_count > 0, "Should have plotter data lines"
        assert dashboard_count > 0, "Should have dashboard key=value lines"

    @pytest.mark.integration
    def test_both_types_present(self, run_simulator_stdout):
        """At least 1 plotter line AND at least 1 dashboard line."""
        lines = run_simulator_stdout("mixed_dashboard", rate=50, duration=1)
        has_plotter = False
        has_dashboard = False
        for line in lines:
            if "@" in line:
                has_plotter = True
            # Dashboard lines have key=value but no @ routing
            if "=" in line and "@" not in line:
                has_dashboard = True
        assert has_plotter, "Must have at least 1 plotter line"
        assert has_dashboard, "Must have at least 1 dashboard line"


# ============================================================================
# High-channel scenario
# ============================================================================

class TestHighChannelScenario:
    """Test the 'high_channel' scenario: 24 channels on one line."""

    @pytest.mark.integration
    def test_produces_output(self, run_simulator_stdout):
        lines = run_simulator_stdout("high_channel", rate=50, duration=0.5)
        assert len(lines) > 0

    @pytest.mark.integration
    def test_24_channels(self, run_simulator_stdout):
        lines = run_simulator_stdout("high_channel", rate=50, duration=0.5)
        for line in lines:
            result = parse_line(line)
            assert len(result) == 24, f"Expected 24 channels, got {len(result)} in: {line!r}"

    @pytest.mark.integration
    def test_all_on_plot_0(self, run_simulator_stdout):
        lines = run_simulator_stdout("high_channel", rate=50, duration=0.5)
        result = parse_line(lines[0])
        for dp in result:
            assert dp.plot_id == 0, f"All channels should be on plot 0, got {dp.plot_id}"

    @pytest.mark.integration
    def test_channel_names(self, run_simulator_stdout):
        lines = run_simulator_stdout("high_channel", rate=50, duration=0.5)
        result = parse_line(lines[0])
        names = [r.name for r in result]
        expected = [f"ch{i:02d}" for i in range(24)]
        assert names == expected, f"Expected {expected}, got {names}"
