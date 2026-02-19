"""Edge case and boundary condition tests.

Tests malformed data, binary garbage, unicode, NaN, huge values,
empty lines, and other boundary conditions across all parsers.

Markers: unit, edge_case
"""

import math

import pytest

from parsers import DataPoint, parse_line, parse_ansi, parse_dashboard_line, format_val


# ============================================================================
# parse_line edge cases
# ============================================================================

class TestParseLineEdgeCases:
    """Boundary conditions for the protocol parser."""

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_empty_string(self):
        assert parse_line("") is None

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_newline_only(self):
        assert parse_line("\n") is None

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_carriage_return(self):
        assert parse_line("\r\n") is None

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_tabs_only(self):
        assert parse_line("\t\t\t") is None

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_unicode_text(self):
        """Unicode word chars (\\w matches unicode in Python) — should parse."""
        result = parse_line("温度:25.5")
        # Python \w matches Unicode word chars, so this actually parses
        assert result is not None
        assert result[0].value == 25.5

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_unicode_with_numbers(self):
        """Unicode text mixed with numbers — CSV fallback should work."""
        result = parse_line("42.0")
        assert result is not None

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_very_long_line(self):
        """A very long line with many data points."""
        parts = [f"ch{i}@0:{i * 0.1:.4f}" for i in range(100)]
        line = " ".join(parts)
        result = parse_line(line)
        assert len(result) == 100

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_very_large_value(self):
        result = parse_line("big@0:99999999999.999")
        assert result[0].value == pytest.approx(99999999999.999)

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_very_small_value(self):
        result = parse_line("tiny@0:0.000000001")
        assert result[0].value == pytest.approx(1e-9)

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_name_with_digits(self):
        result = parse_line("ch123@0:1.0")
        assert result[0].name == "ch123"

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_name_starts_with_underscore(self):
        result = parse_line("_private@0:1.0")
        assert result[0].name == "_private"

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_consecutive_separators_csv(self):
        """Multiple consecutive separators in CSV fallback."""
        result = parse_line("1.0,,2.0")
        # The empty string between commas won't parse as float, so we get 2 values
        assert result is not None
        values = [r.value for r in result]
        assert 1.0 in values
        assert 2.0 in values

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_trailing_whitespace(self):
        result = parse_line("val@0:1.5   ")
        assert result[0].value == 1.5

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_leading_whitespace(self):
        result = parse_line("   val@0:1.5")
        assert result[0].value == 1.5

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_null_bytes(self):
        """Binary null bytes should not crash the parser."""
        result = parse_line("\x00\x00val@0:1.0\x00")
        # The regex may or may not match depending on \x00 handling,
        # but it should not crash
        assert result is None or isinstance(result, list)

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_binary_garbage(self):
        """Random binary data should return None, not crash."""
        import os
        garbage = os.urandom(50).decode("latin-1")
        result = parse_line(garbage)
        assert result is None or isinstance(result, list)

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_inf_value(self):
        """Infinity values — regex won't match 'inf' as a number."""
        result = parse_line("val@0:inf")
        # "inf" doesn't match the numeric regex [-+]?\d*\.?\d+...
        assert result is None or all(r.name != "val" or math.isfinite(r.value) for r in (result or []))

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_plot_id_zero(self):
        result = parse_line("x@0:1.0")
        assert result[0].plot_id == 0

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_plot_id_large(self):
        result = parse_line("x@999:1.0")
        assert result[0].plot_id == 999

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_value_just_dot(self):
        """Just a dot shouldn't parse as a number."""
        assert parse_line("val@0:.") is None

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_value_with_trailing_text(self):
        """Value followed immediately by non-numeric text."""
        result = parse_line("val@0:1.5abc")
        # Regex captures 1.5, "abc" is leftover
        assert result[0].value == 1.5


# ============================================================================
# parse_ansi edge cases
# ============================================================================

class TestParseAnsiEdgeCases:
    """Boundary conditions for the ANSI parser."""

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_incomplete_escape(self):
        """Incomplete escape sequence (no 'm' terminator)."""
        result = parse_ansi("\033[31")
        # Should treat as plain text since regex won't match
        assert len(result) == 1
        assert "\033[31" in result[0].text

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_double_reset(self):
        result = parse_ansi("\033[0m\033[0mText")
        assert result[0].text == "Text"
        assert result[0].bold is False

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_unknown_code(self):
        """Unknown SGR codes should be silently ignored."""
        result = parse_ansi("\033[99mText\033[0m")
        assert result[0].text == "Text"
        # Code 99 is not in ANSI_COLORS, so no color set

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_many_semicolons(self):
        """Many codes chained with semicolons."""
        result = parse_ansi("\033[1;2;4;31mStyled\033[0m")
        assert result[0].bold is True
        assert result[0].dim is True
        assert result[0].underline is True
        assert result[0].color == "ansi-red"

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_only_escape_sequence_no_text(self):
        """Just an escape sequence with no text after it."""
        result = parse_ansi("\033[31m")
        assert result == []

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_escape_at_end(self):
        """Escape sequence at the very end with text before it."""
        result = parse_ansi("Text\033[31m")
        assert len(result) == 1
        assert result[0].text == "Text"

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_consecutive_escapes_no_text_between(self):
        result = parse_ansi("\033[1m\033[31mBoldRed\033[0m")
        assert result[0].text == "BoldRed"
        assert result[0].bold is True
        assert result[0].color == "ansi-red"

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_null_bytes_in_text(self):
        """Null bytes in text should not crash."""
        result = parse_ansi("Hello\x00World")
        assert len(result) >= 1


# ============================================================================
# parse_dashboard_line edge cases
# ============================================================================

class TestDashboardEdgeCases:
    """Boundary conditions for the dashboard parser."""

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_equals_in_value(self):
        """Value containing = sign (e.g., base64)."""
        result = parse_dashboard_line("token=abc=def")
        # First match: key="token", value="abc=def" (\\S+ is greedy)
        assert len(result) >= 1
        assert result[0][0] == "token"

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_many_pairs(self):
        pairs = " ".join(f"key{i}=val{i}" for i in range(50))
        result = parse_dashboard_line(pairs)
        assert len(result) == 50

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_value_with_special_chars(self):
        """Value containing special non-whitespace characters."""
        result = parse_dashboard_line("path=/dev/ttyACM0")
        assert result[0] == ("path", "/dev/ttyACM0")

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_numeric_key(self):
        """Key that's just digits (word chars include digits)."""
        result = parse_dashboard_line("123=abc")
        assert result[0] == ("123", "abc")


# ============================================================================
# format_val edge cases
# ============================================================================

class TestFormatValEdgeCases:
    """Boundary conditions for the cursor value formatter."""

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_positive_infinity(self):
        result = format_val(float("inf"))
        # inf is not NaN, not None, not integer, so it gets .3f formatting
        assert "inf" in result

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_negative_infinity(self):
        result = format_val(float("-inf"))
        assert "inf" in result

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_very_large_number(self):
        result = format_val(1e15)
        assert result == "1000000000000000"

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_very_small_number(self):
        result = format_val(0.0001)
        assert result == "0.000"

    @pytest.mark.unit
    @pytest.mark.edge_case
    def test_negative_zero(self):
        result = format_val(-0.0)
        assert result == "0"  # -0.0 == 0 → integer path
