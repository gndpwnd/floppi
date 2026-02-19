"""Unit tests for the serial protocol parser (parse_line).

Tests the exact behavior of plotter.js parseLine() via Python port.
Covers all 4 protocol formats plus CSV fallback and edge cases.

Markers: unit
"""

import math

import pytest

from parsers import DataPoint, parse_line


# ============================================================================
# Named plot format: name@plotId:value
# ============================================================================

class TestNamedPlotFormat:
    """Tests for the name@plotId:value format (primary protocol)."""

    @pytest.mark.unit
    def test_single_named_plot(self):
        result = parse_line("ax@0:1.234")
        assert result == [DataPoint("ax", 0, 1.234)]

    @pytest.mark.unit
    def test_multiple_named_plots_same_line(self):
        result = parse_line("ax@0:0.02 ay@0:-0.01 az@0:1.00")
        assert len(result) == 3
        assert result[0] == DataPoint("ax", 0, 0.02)
        assert result[1] == DataPoint("ay", 0, -0.01)
        assert result[2] == DataPoint("az", 0, 1.00)

    @pytest.mark.unit
    def test_multiple_plot_ids(self):
        result = parse_line("ax@0:0.5 gx@1:1.2")
        assert result[0].plot_id == 0
        assert result[1].plot_id == 1

    @pytest.mark.unit
    def test_high_plot_id(self):
        result = parse_line("ch5@11:3.14")
        assert result == [DataPoint("ch5", 11, 3.14)]

    @pytest.mark.unit
    def test_negative_value(self):
        result = parse_line("temp@0:-42.5")
        assert result[0].value == -42.5

    @pytest.mark.unit
    def test_positive_sign(self):
        result = parse_line("val@0:+3.14")
        assert result[0].value == pytest.approx(3.14)

    @pytest.mark.unit
    def test_scientific_notation(self):
        result = parse_line("tiny@0:1.5e-3")
        assert result[0].value == pytest.approx(0.0015)

    @pytest.mark.unit
    def test_scientific_notation_uppercase(self):
        result = parse_line("big@0:2.5E+6")
        assert result[0].value == pytest.approx(2500000.0)

    @pytest.mark.unit
    def test_negative_exponent(self):
        result = parse_line("micro@0:-3.2e-4")
        assert result[0].value == pytest.approx(-0.00032)

    @pytest.mark.unit
    def test_integer_value(self):
        result = parse_line("count@0:42")
        assert result[0].value == 42.0

    @pytest.mark.unit
    def test_zero_value(self):
        result = parse_line("zero@0:0")
        assert result[0].value == 0.0

    @pytest.mark.unit
    def test_dotted_name(self):
        result = parse_line("imu.accel.x@0:0.98")
        assert result[0].name == "imu.accel.x"

    @pytest.mark.unit
    def test_underscore_name(self):
        result = parse_line("loop_time@0:1247")
        assert result[0].name == "loop_time"

    @pytest.mark.unit
    def test_imu_scenario_format(self):
        """Test exact format from simulate_serial.py imu scenario."""
        line = "ax@0:0.0234 ay@0:-0.0089 az@0:1.0012 gx@1:0.4567 gy@1:0.2345 gz@1:-0.0123"
        result = parse_line(line)
        assert len(result) == 6
        assert result[0].name == "ax"
        assert result[0].plot_id == 0
        assert result[3].name == "gx"
        assert result[3].plot_id == 1


# ============================================================================
# Colon format: name:value (plotId defaults to 0)
# ============================================================================

class TestColonFormat:
    """Tests for the name:value format (backward compatible)."""

    @pytest.mark.unit
    def test_single_colon(self):
        result = parse_line("sensor1:3.14")
        assert result == [DataPoint("sensor1", 0, 3.14)]

    @pytest.mark.unit
    def test_multiple_colon(self):
        result = parse_line("sensor1:1.5 sensor2:2.5")
        assert len(result) == 2
        assert result[0] == DataPoint("sensor1", 0, 1.5)
        assert result[1] == DataPoint("sensor2", 0, 2.5)

    @pytest.mark.unit
    def test_colon_default_plot_id_zero(self):
        result = parse_line("val:100")
        assert result[0].plot_id == 0

    @pytest.mark.unit
    def test_colon_negative(self):
        result = parse_line("offset:-0.5")
        assert result[0].value == -0.5


# ============================================================================
# Equals format: name=value (also matched by NAMED_RE)
# ============================================================================

class TestEqualsFormat:
    """Tests for the name=value format."""

    @pytest.mark.unit
    def test_single_equals(self):
        result = parse_line("reading1=42.5")
        assert result == [DataPoint("reading1", 0, 42.5)]

    @pytest.mark.unit
    def test_multiple_equals(self):
        result = parse_line("reading1=1.0 reading2=2.0")
        assert len(result) == 2

    @pytest.mark.unit
    def test_equals_with_plot_id(self):
        """name@plotId=value should also work (= is an alternative separator)."""
        result = parse_line("val@2=3.14")
        assert result == [DataPoint("val", 2, 3.14)]


# ============================================================================
# CSV fallback: plain numbers
# ============================================================================

class TestCSVFallback:
    """Tests for the CSV/space-separated number fallback."""

    @pytest.mark.unit
    def test_comma_separated(self):
        result = parse_line("1.5,2.5,3.5")
        assert len(result) == 3
        assert result[0] == DataPoint("ch0", 0, 1.5)
        assert result[1] == DataPoint("ch1", 0, 2.5)
        assert result[2] == DataPoint("ch2", 0, 3.5)

    @pytest.mark.unit
    def test_space_separated(self):
        result = parse_line("1.0 2.0 3.0")
        assert len(result) == 3
        assert all(r.plot_id == 0 for r in result)

    @pytest.mark.unit
    def test_tab_separated(self):
        result = parse_line("10\t20\t30")
        assert len(result) == 3
        assert result[0].value == 10.0

    @pytest.mark.unit
    def test_mixed_separators(self):
        result = parse_line("1.0, 2.0\t3.0")
        assert len(result) == 3

    @pytest.mark.unit
    def test_single_number(self):
        result = parse_line("42.0")
        assert result == [DataPoint("ch0", 0, 42.0)]

    @pytest.mark.unit
    def test_negative_csv(self):
        result = parse_line("-1.5, -2.5")
        assert result[0].value == -1.5
        assert result[1].value == -2.5

    @pytest.mark.unit
    def test_csv_auto_naming(self):
        """CSV fallback should name channels ch0, ch1, ch2, ..."""
        result = parse_line("1,2,3,4,5")
        for i, dp in enumerate(result):
            assert dp.name == f"ch{i}"


# ============================================================================
# No data / unparseable
# ============================================================================

class TestNoData:
    """Tests for lines that contain no parseable data."""

    @pytest.mark.unit
    def test_empty_string(self):
        assert parse_line("") is None

    @pytest.mark.unit
    def test_whitespace_only(self):
        assert parse_line("   ") is None

    @pytest.mark.unit
    def test_plain_text(self):
        assert parse_line("Hello world, no numbers here!") is None

    @pytest.mark.unit
    def test_ansi_only(self):
        """ANSI escape sequences with no numeric data."""
        assert parse_line("\033[1;32mOK\033[0m") is None

    @pytest.mark.unit
    def test_status_message(self):
        """Flight controller status messages shouldn't parse as data."""
        assert parse_line("FLIGHT CONTROLLER READY") is None


# ============================================================================
# Protocol priority / mixed
# ============================================================================

class TestProtocolPriority:
    """Tests for how different formats interact on the same line."""

    @pytest.mark.unit
    def test_named_takes_priority_over_csv(self):
        """If named matches exist, CSV fallback should not be used."""
        result = parse_line("temp@0:25.5 extra text 42.0")
        # The NAMED_RE should match temp@0:25.5, and "42" standalone
        # Actually: "42.0" will also be matched by NAMED_RE if preceded by text...
        # Let's check what the regex actually matches
        # "text" contains no [=:] before 42.0, so NAMED_RE won't match "42.0"
        # But "temp@0:25.5" will match. Since results > 0, no CSV fallback.
        assert len(result) >= 1
        assert result[0] == DataPoint("temp", 0, 25.5)

    @pytest.mark.unit
    def test_mixed_equals_and_colon(self):
        """Both = and : separators on the same line."""
        result = parse_line("a:1.0 b=2.0")
        assert len(result) == 2
        assert result[0] == DataPoint("a", 0, 1.0)
        assert result[1] == DataPoint("b", 0, 2.0)

    @pytest.mark.unit
    def test_simulator_protocol_format(self):
        """Test exact format from simulate_serial.py protocol scenario."""
        # Cycle 0: named_plot@0:value
        r0 = parse_line("named_plot@0:5.0000")
        assert r0[0] == DataPoint("named_plot", 0, 5.0)

        # Cycle 1: colon_fmt:value
        r1 = parse_line("colon_fmt:5.0000")
        assert r1[0] == DataPoint("colon_fmt", 0, 5.0)

        # Cycle 2: equals_fmt=value
        r2 = parse_line("equals_fmt=5.0000")
        assert r2[0] == DataPoint("equals_fmt", 0, 5.0)

        # Cycle 3: plain number (CSV fallback)
        r3 = parse_line("5.0000")
        assert r3[0] == DataPoint("ch0", 0, 5.0)


# ============================================================================
# Precision
# ============================================================================

class TestPrecision:
    """Test numeric precision handling."""

    @pytest.mark.unit
    def test_many_decimal_places(self):
        result = parse_line("pi@0:3.14159265358979")
        assert result[0].value == pytest.approx(3.14159265358979)

    @pytest.mark.unit
    def test_leading_zero(self):
        result = parse_line("small@0:0.001")
        assert result[0].value == 0.001

    @pytest.mark.unit
    def test_no_leading_zero(self):
        """Value like .5 (no leading zero)."""
        result = parse_line("half@0:.5")
        assert result[0].value == 0.5

    @pytest.mark.unit
    def test_large_integer(self):
        result = parse_line("big@0:999999")
        assert result[0].value == 999999.0
