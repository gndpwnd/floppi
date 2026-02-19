"""Performance benchmarks for parsers.

Tests throughput of each parser with large datasets. Not for CI — run
manually to check for regressions.

Markers: performance
"""

import time

import pytest

from parsers import parse_line, parse_ansi, parse_dashboard_line, format_val


# ============================================================================
# parse_line throughput
# ============================================================================

class TestParseLineThroughput:
    """Benchmark parse_line on large datasets."""

    @pytest.mark.performance
    def test_named_plot_100k_lines(self):
        """Parse 100K named@plot:value lines."""
        line = "ax@0:0.0234 ay@0:-0.0089 az@0:1.0012 gx@1:0.4567 gy@1:0.2345 gz@1:-0.0123"
        start = time.perf_counter()
        for _ in range(100_000):
            parse_line(line)
        elapsed = time.perf_counter() - start
        rate = 100_000 / elapsed
        print(f"\nparse_line (named): {rate:.0f} lines/sec ({elapsed:.2f}s for 100K)")
        assert elapsed < 10, f"Too slow: {elapsed:.1f}s for 100K lines"

    @pytest.mark.performance
    def test_csv_fallback_100k_lines(self):
        """Parse 100K CSV fallback lines."""
        line = "1.234, 5.678, -9.012, 3.456, 7.890, -2.345"
        start = time.perf_counter()
        for _ in range(100_000):
            parse_line(line)
        elapsed = time.perf_counter() - start
        rate = 100_000 / elapsed
        print(f"\nparse_line (CSV): {rate:.0f} lines/sec ({elapsed:.2f}s for 100K)")
        assert elapsed < 10, f"Too slow: {elapsed:.1f}s for 100K lines"

    @pytest.mark.performance
    def test_no_match_100k_lines(self):
        """Parse 100K non-matching lines (worst case for fallback)."""
        line = "FLIGHT CONTROLLER READY — no data here"
        start = time.perf_counter()
        for _ in range(100_000):
            parse_line(line)
        elapsed = time.perf_counter() - start
        rate = 100_000 / elapsed
        print(f"\nparse_line (no match): {rate:.0f} lines/sec ({elapsed:.2f}s for 100K)")
        assert elapsed < 10, f"Too slow: {elapsed:.1f}s for 100K lines"


# ============================================================================
# parse_ansi throughput
# ============================================================================

class TestParseAnsiThroughput:
    """Benchmark parse_ansi on large datasets."""

    @pytest.mark.performance
    def test_ansi_100k_lines(self):
        """Parse 100K ANSI-styled lines."""
        line = "\033[1;32mOK\033[0m: IMU initialized — \033[2mloop=1247us\033[0m"
        start = time.perf_counter()
        for _ in range(100_000):
            parse_ansi(line)
        elapsed = time.perf_counter() - start
        rate = 100_000 / elapsed
        print(f"\nparse_ansi: {rate:.0f} lines/sec ({elapsed:.2f}s for 100K)")
        assert elapsed < 10, f"Too slow: {elapsed:.1f}s for 100K lines"


# ============================================================================
# parse_dashboard_line throughput
# ============================================================================

class TestDashboardThroughput:
    """Benchmark parse_dashboard_line on large datasets."""

    @pytest.mark.performance
    def test_dashboard_100k_lines(self):
        """Parse 100K dashboard key=value lines."""
        line = "battery=3.95V loop=1147us alt=11.2m armed=YES rssi=-52dBm temp=28.3C"
        start = time.perf_counter()
        for _ in range(100_000):
            parse_dashboard_line(line)
        elapsed = time.perf_counter() - start
        rate = 100_000 / elapsed
        print(f"\nparse_dashboard: {rate:.0f} lines/sec ({elapsed:.2f}s for 100K)")
        assert elapsed < 10, f"Too slow: {elapsed:.1f}s for 100K lines"


# ============================================================================
# format_val throughput
# ============================================================================

class TestFormatValThroughput:
    """Benchmark format_val."""

    @pytest.mark.performance
    def test_format_val_1m_calls(self):
        """Format 1M values."""
        start = time.perf_counter()
        for i in range(1_000_000):
            format_val(i * 0.001)
        elapsed = time.perf_counter() - start
        rate = 1_000_000 / elapsed
        print(f"\nformat_val: {rate:.0f} calls/sec ({elapsed:.2f}s for 1M)")
        assert elapsed < 10, f"Too slow: {elapsed:.1f}s for 1M calls"
