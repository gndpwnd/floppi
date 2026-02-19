"""E2E tests for serial protocol features through the full pipeline.

Tests: simulate_serial.py -> socat pty -> fc_tool --headless -> stdout.
Verifies data integrity, format handling, and multi-feature coexistence.

Markers: e2e
"""

import time

import pytest

from parsers import parse_line, parse_dashboard_line


# ============================================================================
# Multi-plot routing
# ============================================================================

class TestMultiPlotRouting:
    """Tests that multi-plot data routes correctly through the serial pipeline."""

    @pytest.mark.e2e
    def test_multi_plot_data_flows(self, run_headless):
        """5-plot scenario data should flow through socat and fc_tool intact."""
        fc_proc, sim_proc, port = run_headless("multi_plot", rate=50, duration=2)
        time.sleep(2.5)
        fc_proc.terminate()
        stdout, stderr = fc_proc.communicate(timeout=5)
        lines = [l for l in stdout.strip().split("\n") if l.strip()]
        assert len(lines) > 0, f"No output received. stderr: {stderr}"

        # Verify lines parse and contain multiple plot IDs
        plot_ids_seen = set()
        for line in lines:
            result = parse_line(line)
            if result:
                for dp in result:
                    plot_ids_seen.add(dp.plot_id)

        assert len(plot_ids_seen) == 5, (
            f"Expected 5 different plot IDs, got {plot_ids_seen}"
        )

    @pytest.mark.e2e
    def test_plot_ids_preserved(self, run_headless):
        """Plot IDs 0-4 should all appear in the pipeline output."""
        fc_proc, sim_proc, port = run_headless("multi_plot", rate=50, duration=2)
        time.sleep(2.5)
        fc_proc.terminate()
        stdout, _ = fc_proc.communicate(timeout=5)
        lines = [l for l in stdout.strip().split("\n") if l.strip()]

        plot_ids_seen = set()
        for line in lines:
            result = parse_line(line)
            if result:
                for dp in result:
                    plot_ids_seen.add(dp.plot_id)

        expected = {0, 1, 2, 3, 4}
        assert plot_ids_seen == expected, (
            f"Expected plot IDs {expected}, got {plot_ids_seen}"
        )


# ============================================================================
# Format cycling
# ============================================================================

class TestFormatCycling:
    """Tests that different protocol formats survive the serial pipeline."""

    @pytest.mark.e2e
    def test_protocol_format_cycling(self, run_headless):
        """Protocol scenario cycles through 4 formats; all should appear in output."""
        fc_proc, sim_proc, port = run_headless("protocol", rate=10, duration=2)
        time.sleep(2.5)
        fc_proc.terminate()
        stdout, stderr = fc_proc.communicate(timeout=5)
        lines = [l for l in stdout.strip().split("\n") if l.strip()]
        assert len(lines) >= 4, (
            f"Expected at least 4 lines for a full format cycle, got {len(lines)}"
        )

        # All lines should parse (protocol scenario uses all parseable formats)
        parsed_count = 0
        for line in lines:
            result = parse_line(line)
            if result:
                parsed_count += 1
        assert parsed_count >= 4, (
            f"Expected at least 4 parseable lines, got {parsed_count}"
        )

    @pytest.mark.e2e
    def test_mixed_formats_parse(self, run_headless):
        """Mixed scenario should produce parseable lines through the pipeline."""
        fc_proc, sim_proc, port = run_headless("mixed", rate=50, duration=2)
        time.sleep(2.5)
        fc_proc.terminate()
        stdout, _ = fc_proc.communicate(timeout=5)
        lines = [l for l in stdout.strip().split("\n") if l.strip()]
        assert len(lines) > 0

        parsed_count = 0
        for line in lines:
            result = parse_line(line)
            if result:
                parsed_count += 1
        assert parsed_count > 0, "No lines parsed as valid data from mixed scenario"


# ============================================================================
# Dashboard + plotter coexistence
# ============================================================================

class TestDashboardPlotterCoexistence:
    """Tests that dashboard and plotter data coexist on the same serial stream."""

    @pytest.mark.e2e
    def test_mixed_dashboard_flows(self, run_headless):
        """Both plotter data and dashboard key=value data should appear in output."""
        fc_proc, sim_proc, port = run_headless("mixed_dashboard", rate=50, duration=2)
        time.sleep(2.5)
        fc_proc.terminate()
        stdout, stderr = fc_proc.communicate(timeout=5)
        lines = [l for l in stdout.strip().split("\n") if l.strip()]
        assert len(lines) > 0, f"No output received. stderr: {stderr}"

        has_plotter = False
        has_dashboard = False
        for line in lines:
            parsed = parse_line(line)
            dashboard = parse_dashboard_line(line)
            if parsed and "@" in line:
                has_plotter = True
            if dashboard and "=" in line and "@" not in line:
                has_dashboard = True

        assert has_plotter, "Should have plotter data lines in pipeline output"
        assert has_dashboard, "Should have dashboard key=value lines in pipeline output"


# ============================================================================
# High-channel pipeline
# ============================================================================

class TestHighChannelPipeline:
    """Tests that high-channel-count lines survive the serial pipeline."""

    @pytest.mark.e2e
    def test_24_channels_through_pipeline(self, run_headless):
        """Lines with 24 data points should flow through intact."""
        fc_proc, sim_proc, port = run_headless("high_channel", rate=50, duration=2)
        time.sleep(2.5)
        fc_proc.terminate()
        stdout, stderr = fc_proc.communicate(timeout=5)
        lines = [l for l in stdout.strip().split("\n") if l.strip()]
        assert len(lines) > 0, f"No output received. stderr: {stderr}"

        # At least some lines should have all 24 channels
        full_lines = 0
        for line in lines:
            result = parse_line(line)
            if result and len(result) == 24:
                full_lines += 1
        assert full_lines > 0, "No lines with 24 channels found in pipeline output"

    @pytest.mark.e2e
    def test_no_truncation(self, run_headless):
        """High-channel lines should not be truncated in the pipeline."""
        fc_proc, sim_proc, port = run_headless("high_channel", rate=50, duration=2)
        time.sleep(2.5)
        fc_proc.terminate()
        stdout, _ = fc_proc.communicate(timeout=5)
        lines = [l for l in stdout.strip().split("\n") if l.strip()]

        # Verify that parsed lines actually have all 24 data points
        for line in lines:
            result = parse_line(line)
            if result:
                assert len(result) == 24, (
                    f"Truncation detected: expected 24 channels, got {len(result)} "
                    f"in line of length {len(line)}"
                )


# ============================================================================
# Ramp data integrity
# ============================================================================

class TestRampDataIntegrity:
    """Tests that ramp data values are preserved through the pipeline."""

    @pytest.mark.e2e
    def test_ramp_values_preserved(self, run_headless):
        """Ramp values should be monotonically increasing after passing through pipeline."""
        fc_proc, sim_proc, port = run_headless("ramp", rate=50, duration=2)
        time.sleep(2.5)
        fc_proc.terminate()
        stdout, stderr = fc_proc.communicate(timeout=5)
        lines = [l for l in stdout.strip().split("\n") if l.strip()]
        assert len(lines) > 0, f"No output received. stderr: {stderr}"

        ramp_values = []
        for line in lines:
            result = parse_line(line)
            if result:
                # First data point is the ramp channel (name="ramp")
                ramp_dp = [dp for dp in result if dp.name == "ramp"]
                if ramp_dp:
                    ramp_values.append(ramp_dp[0].value)

        assert len(ramp_values) > 1, "Need at least 2 ramp values to check monotonicity"

        # Check monotonically non-decreasing (allow equal due to timing)
        for i in range(1, len(ramp_values)):
            assert ramp_values[i] >= ramp_values[i - 1], (
                f"Ramp not monotonic at index {i}: "
                f"{ramp_values[i - 1]} -> {ramp_values[i]}"
            )
