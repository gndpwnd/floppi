"""Unit tests for the dashboard key=value parser (parse_dashboard_line).

Tests the exact behavior of dashboard.js DASHBOARD_KV_RE via Python port.

Markers: unit
"""

import pytest

from parsers import parse_dashboard_line


# ============================================================================
# Basic key=value parsing
# ============================================================================

class TestBasicDashboard:
    """Tests for basic key=value pair extraction."""

    @pytest.mark.unit
    def test_single_pair(self):
        result = parse_dashboard_line("battery=3.70V")
        assert result == [("battery", "3.70V")]

    @pytest.mark.unit
    def test_multiple_pairs(self):
        result = parse_dashboard_line("battery=3.70V loop=1200us alt=10.5m armed=NO")
        assert len(result) == 4
        assert result[0] == ("battery", "3.70V")
        assert result[1] == ("loop", "1200us")
        assert result[2] == ("alt", "10.5m")
        assert result[3] == ("armed", "NO")

    @pytest.mark.unit
    def test_dotted_key(self):
        result = parse_dashboard_line("imu.temp=25.3")
        assert result == [("imu.temp", "25.3")]

    @pytest.mark.unit
    def test_underscore_key(self):
        result = parse_dashboard_line("loop_time=1247us")
        assert result == [("loop_time", "1247us")]

    @pytest.mark.unit
    def test_numeric_value(self):
        result = parse_dashboard_line("count=42")
        assert result == [("count", "42")]

    @pytest.mark.unit
    def test_value_is_string_not_number(self):
        """Dashboard parser returns raw strings, not numbers."""
        result = parse_dashboard_line("val=3.14")
        assert result[0][1] == "3.14"
        assert isinstance(result[0][1], str)


# ============================================================================
# Simulator dashboard scenario
# ============================================================================

class TestDashboardScenario:
    """Tests matching the simulate_serial.py dashboard scenario format."""

    @pytest.mark.unit
    def test_exact_simulator_format(self):
        line = "battery=3.95V loop=1147us alt=11.2m armed=YES"
        result = parse_dashboard_line(line)
        assert len(result) == 4
        assert result[0] == ("battery", "3.95V")
        assert result[3] == ("armed", "YES")

    @pytest.mark.unit
    def test_armed_no(self):
        line = "armed=NO"
        result = parse_dashboard_line(line)
        assert result == [("armed", "NO")]


# ============================================================================
# Separator conflict with plotter
# ============================================================================

class TestSeparatorConflict:
    """Dashboard uses = only, to avoid conflict with plotter's : format."""

    @pytest.mark.unit
    def test_colon_not_matched(self):
        """key:value lines should NOT match as dashboard pairs."""
        result = parse_dashboard_line("sensor1:3.14 sensor2:2.71")
        assert result == []

    @pytest.mark.unit
    def test_plotter_at_format_not_matched(self):
        """name@plotId:value should NOT match as dashboard pairs."""
        result = parse_dashboard_line("ax@0:1.234")
        assert result == []

    @pytest.mark.unit
    def test_mixed_equals_and_colon(self):
        """Only = pairs should be extracted, : should be ignored."""
        result = parse_dashboard_line("dashboard_val=42 plotter_val:3.14")
        assert len(result) == 1
        assert result[0] == ("dashboard_val", "42")


# ============================================================================
# Edge cases
# ============================================================================

class TestDashboardEdgeCases:
    """Edge cases for dashboard parsing."""

    @pytest.mark.unit
    def test_empty_string(self):
        assert parse_dashboard_line("") == []

    @pytest.mark.unit
    def test_no_equals(self):
        assert parse_dashboard_line("no equals here") == []

    @pytest.mark.unit
    def test_equals_but_no_word_key(self):
        """= with non-word chars before it shouldn't match."""
        result = parse_dashboard_line("=orphan")
        assert result == []

    @pytest.mark.unit
    def test_value_stops_at_whitespace(self):
        """Value capture stops at whitespace (\\S+ pattern)."""
        result = parse_dashboard_line("key=value extra_text")
        assert result[0] == ("key", "value")
        # "extra_text" has no = so it shouldn't be a pair
