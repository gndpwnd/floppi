"""Unit tests for cursor helpers (format_val).

Tests the exact behavior of cursors.js formatVal() via Python port.

Markers: unit
"""

import math

import pytest

from parsers import format_val


# ============================================================================
# format_val
# ============================================================================

class TestFormatVal:
    """Tests for format_val() — cursor readout value formatter."""

    @pytest.mark.unit
    def test_none(self):
        assert format_val(None) == "--"

    @pytest.mark.unit
    def test_nan(self):
        assert format_val(float("nan")) == "--"

    @pytest.mark.unit
    def test_integer(self):
        assert format_val(42) == "42"

    @pytest.mark.unit
    def test_float_integer_value(self):
        """Float that equals an integer (e.g., 5.0) should show as integer."""
        assert format_val(5.0) == "5"

    @pytest.mark.unit
    def test_float_decimal(self):
        """Non-integer float should show 3 decimal places."""
        assert format_val(3.14159) == "3.142"

    @pytest.mark.unit
    def test_zero_int(self):
        assert format_val(0) == "0"

    @pytest.mark.unit
    def test_zero_float(self):
        assert format_val(0.0) == "0"

    @pytest.mark.unit
    def test_negative_integer(self):
        assert format_val(-10) == "-10"

    @pytest.mark.unit
    def test_negative_float(self):
        assert format_val(-0.12345) == "-0.123"

    @pytest.mark.unit
    def test_small_float(self):
        assert format_val(0.001) == "0.001"

    @pytest.mark.unit
    def test_large_float(self):
        assert format_val(1234.5678) == "1234.568"
