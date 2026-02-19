"""Unit tests for the ANSI SGR parser (parse_ansi).

Tests the exact behavior of ansi.js parseAnsi() via Python port.
Covers all supported SGR codes, compound sequences, and edge cases.

Markers: unit
"""

import pytest

from parsers import AnsiSpan, parse_ansi


# ============================================================================
# Basic SGR codes
# ============================================================================

class TestBasicCodes:
    """Tests for individual SGR attribute codes."""

    @pytest.mark.unit
    def test_plain_text_no_ansi(self):
        result = parse_ansi("Hello World")
        assert len(result) == 1
        assert result[0] == AnsiSpan("Hello World")

    @pytest.mark.unit
    def test_bold(self):
        result = parse_ansi("\033[1mBold Text\033[0m")
        assert result[0] == AnsiSpan("Bold Text", bold=True)

    @pytest.mark.unit
    def test_dim(self):
        result = parse_ansi("\033[2mDim Text\033[0m")
        assert result[0] == AnsiSpan("Dim Text", dim=True)

    @pytest.mark.unit
    def test_underline(self):
        result = parse_ansi("\033[4mUnderlined\033[0m")
        assert result[0] == AnsiSpan("Underlined", underline=True)

    @pytest.mark.unit
    def test_reset_clears_all(self):
        result = parse_ansi("\033[1mBold\033[0mPlain")
        assert result[0] == AnsiSpan("Bold", bold=True)
        assert result[1] == AnsiSpan("Plain")

    @pytest.mark.unit
    def test_empty_code_is_reset(self):
        """ESC[m (no number) is equivalent to ESC[0m."""
        result = parse_ansi("\033[1mBold\033[mPlain")
        assert result[0].bold is True
        assert result[1].bold is False


# ============================================================================
# Foreground colors (standard 8)
# ============================================================================

class TestStandardColors:
    """Tests for standard foreground colors (30-37)."""

    @pytest.mark.unit
    def test_red(self):
        result = parse_ansi("\033[31mRed\033[0m")
        assert result[0] == AnsiSpan("Red", color="ansi-red")

    @pytest.mark.unit
    def test_green(self):
        result = parse_ansi("\033[32mGreen\033[0m")
        assert result[0].color == "ansi-green"

    @pytest.mark.unit
    def test_yellow(self):
        result = parse_ansi("\033[33mYellow\033[0m")
        assert result[0].color == "ansi-yellow"

    @pytest.mark.unit
    def test_blue(self):
        result = parse_ansi("\033[34mBlue\033[0m")
        assert result[0].color == "ansi-blue"

    @pytest.mark.unit
    def test_magenta(self):
        result = parse_ansi("\033[35mMagenta\033[0m")
        assert result[0].color == "ansi-magenta"

    @pytest.mark.unit
    def test_cyan(self):
        result = parse_ansi("\033[36mCyan\033[0m")
        assert result[0].color == "ansi-cyan"

    @pytest.mark.unit
    def test_white(self):
        result = parse_ansi("\033[37mWhite\033[0m")
        assert result[0].color == "ansi-white"

    @pytest.mark.unit
    def test_black(self):
        result = parse_ansi("\033[30mBlack\033[0m")
        assert result[0].color == "ansi-black"

    @pytest.mark.unit
    def test_all_standard_colors(self):
        """Verify all 8 standard foreground colors map correctly."""
        expected = {
            30: "ansi-black", 31: "ansi-red", 32: "ansi-green", 33: "ansi-yellow",
            34: "ansi-blue", 35: "ansi-magenta", 36: "ansi-cyan", 37: "ansi-white",
        }
        for code, css_class in expected.items():
            result = parse_ansi(f"\033[{code}mX\033[0m")
            assert result[0].color == css_class, f"Code {code} should map to {css_class}"


# ============================================================================
# Bright colors (90-97)
# ============================================================================

class TestBrightColors:
    """Tests for bright foreground colors (90-97)."""

    @pytest.mark.unit
    def test_bright_red(self):
        result = parse_ansi("\033[91mBright Red\033[0m")
        assert result[0].color == "ansi-bright-red"

    @pytest.mark.unit
    def test_bright_green(self):
        result = parse_ansi("\033[92mBright Green\033[0m")
        assert result[0].color == "ansi-bright-green"

    @pytest.mark.unit
    def test_all_bright_colors(self):
        """Verify all 8 bright foreground colors map correctly."""
        expected = {
            90: "ansi-bright-black", 91: "ansi-bright-red",
            92: "ansi-bright-green", 93: "ansi-bright-yellow",
            94: "ansi-bright-blue", 95: "ansi-bright-magenta",
            96: "ansi-bright-cyan", 97: "ansi-bright-white",
        }
        for code, css_class in expected.items():
            result = parse_ansi(f"\033[{code}mX\033[0m")
            assert result[0].color == css_class, f"Code {code} should map to {css_class}"


# ============================================================================
# Compound codes (e.g., ESC[1;31m = bold + red)
# ============================================================================

class TestCompoundCodes:
    """Tests for compound/chained SGR codes."""

    @pytest.mark.unit
    def test_bold_red(self):
        result = parse_ansi("\033[1;31mBold Red\033[0m")
        assert result[0] == AnsiSpan("Bold Red", bold=True, color="ansi-red")

    @pytest.mark.unit
    def test_bold_green(self):
        result = parse_ansi("\033[1;32mBold Green\033[0m")
        assert result[0].bold is True
        assert result[0].color == "ansi-green"

    @pytest.mark.unit
    def test_dim_underline(self):
        result = parse_ansi("\033[2;4mDim Underlined\033[0m")
        assert result[0].dim is True
        assert result[0].underline is True

    @pytest.mark.unit
    def test_bold_underline_cyan(self):
        result = parse_ansi("\033[1;4;36mStyled\033[0m")
        assert result[0].bold is True
        assert result[0].underline is True
        assert result[0].color == "ansi-cyan"

    @pytest.mark.unit
    def test_triple_code(self):
        result = parse_ansi("\033[1;2;4mAll\033[0m")
        assert result[0].bold is True
        assert result[0].dim is True
        assert result[0].underline is True


# ============================================================================
# Specific reset codes
# ============================================================================

class TestSpecificResets:
    """Tests for specific attribute reset codes (22, 24, 39)."""

    @pytest.mark.unit
    def test_code_22_resets_bold_and_dim(self):
        result = parse_ansi("\033[1;2mBoldDim\033[22mNormal\033[0m")
        assert result[0].bold is True
        assert result[0].dim is True
        assert result[1].bold is False
        assert result[1].dim is False

    @pytest.mark.unit
    def test_code_24_resets_underline(self):
        result = parse_ansi("\033[4mUnder\033[24mNoUnder\033[0m")
        assert result[0].underline is True
        assert result[1].underline is False

    @pytest.mark.unit
    def test_code_39_resets_color(self):
        result = parse_ansi("\033[31mRed\033[39mDefault\033[0m")
        assert result[0].color == "ansi-red"
        assert result[1].color is None


# ============================================================================
# Multi-span sequences (from simulate_serial.py ansi scenario)
# ============================================================================

class TestMultiSpan:
    """Tests for real-world multi-span sequences."""

    @pytest.mark.unit
    def test_simulator_ok_message(self):
        """From scenario_ansi: ESC[1;32mOKESC[0m: IMU initialized"""
        result = parse_ansi("\033[1;32mOK\033[0m: IMU initialized")
        assert result[0] == AnsiSpan("OK", bold=True, color="ansi-green")
        assert result[1] == AnsiSpan(": IMU initialized")

    @pytest.mark.unit
    def test_simulator_error_message(self):
        result = parse_ansi("\033[1;31mERROR\033[0m: GPS fix lost")
        assert result[0].bold is True
        assert result[0].color == "ansi-red"
        assert result[1].text == ": GPS fix lost"

    @pytest.mark.unit
    def test_mixed_styles_line(self):
        """From scenario_ansi: Bold + Dim + Underline + colors."""
        result = parse_ansi(
            "\033[1mBold\033[0m \033[2mDim\033[0m \033[4mUnderline\033[0m "
            "\033[31mRed\033[0m \033[32mGreen\033[0m \033[34mBlue\033[0m"
        )
        assert result[0] == AnsiSpan("Bold", bold=True)
        assert result[1] == AnsiSpan(" ")
        assert result[2] == AnsiSpan("Dim", dim=True)
        assert result[3] == AnsiSpan(" ")
        assert result[4] == AnsiSpan("Underline", underline=True)

    @pytest.mark.unit
    def test_nested_color_change(self):
        """Color change mid-text: ESC[1;31m[FAIL]ESC[0m ... ESC[1;33mcheck wiringESC[0m."""
        result = parse_ansi(
            "\033[1;31m[FAIL]\033[0m Self-test: magnetometer — \033[1;33mcheck wiring\033[0m"
        )
        assert result[0] == AnsiSpan("[FAIL]", bold=True, color="ansi-red")
        assert result[1].text == " Self-test: magnetometer — "
        assert result[2] == AnsiSpan("check wiring", bold=True, color="ansi-yellow")

    @pytest.mark.unit
    def test_color_persists_until_reset(self):
        """Color set once should persist across text until reset."""
        result = parse_ansi("\033[31mRed text continues\033[0m")
        assert len(result) == 1
        assert result[0].color == "ansi-red"
        assert result[0].text == "Red text continues"

    @pytest.mark.unit
    def test_empty_string(self):
        result = parse_ansi("")
        assert result == []
