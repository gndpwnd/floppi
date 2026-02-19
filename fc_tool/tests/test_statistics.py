"""Unit tests for RunningStats and cursor_delta.

Tests the exact behavior of plotter.js statistics accumulation
and cursors.js delta calculations via Python ports.

Markers: unit
"""

import math
import pytest
from parsers import RunningStats, cursor_delta


# ============================================================================
# RunningStats — basic accumulation
# ============================================================================

@pytest.mark.unit
class TestRunningStatsBasic:
    """Core accumulation behavior matching plotter.js."""

    def test_empty_stats(self):
        s = RunningStats()
        assert s.count == 0
        assert s.mean == 0.0
        assert s.min_val == float('inf')
        assert s.max_val == float('-inf')

    def test_single_value(self):
        s = RunningStats()
        s.update(5.0)
        assert s.count == 1
        assert s.min_val == 5.0
        assert s.max_val == 5.0
        assert s.mean == 5.0

    def test_two_values(self):
        s = RunningStats()
        s.update(3.0)
        s.update(7.0)
        assert s.count == 2
        assert s.min_val == 3.0
        assert s.max_val == 7.0
        assert s.mean == 5.0

    def test_negative_values(self):
        s = RunningStats()
        s.update(-10.0)
        s.update(-5.0)
        assert s.min_val == -10.0
        assert s.max_val == -5.0
        assert s.mean == -7.5

    def test_mixed_sign(self):
        s = RunningStats()
        s.update(-5.0)
        s.update(0.0)
        s.update(5.0)
        assert s.min_val == -5.0
        assert s.max_val == 5.0
        assert s.mean == 0.0

    def test_same_value_repeated(self):
        s = RunningStats()
        for _ in range(10):
            s.update(42.0)
        assert s.count == 10
        assert s.min_val == 42.0
        assert s.max_val == 42.0
        assert s.mean == 42.0

    def test_large_count(self):
        s = RunningStats()
        n = 10000
        for i in range(n):
            s.update(i * 0.001)
        assert s.count == n
        expected_mean = (n - 1) * 0.001 / 2  # arithmetic mean of 0, 0.001, ..., 9.999
        assert s.mean == pytest.approx(expected_mean, abs=1e-6)


# ============================================================================
# RunningStats — reset behavior
# ============================================================================

@pytest.mark.unit
class TestRunningStatsReset:
    """Reset clears state and allows fresh accumulation."""

    def test_reset_clears_all(self):
        s = RunningStats()
        s.update(1.0)
        s.update(2.0)
        s.update(3.0)
        s.reset()
        assert s.count == 0
        assert s.mean == 0.0
        assert s.min_val == float('inf')
        assert s.max_val == float('-inf')
        assert s.sum_val == 0.0

    def test_reset_then_accumulate(self):
        s = RunningStats()
        s.update(100.0)
        s.update(200.0)
        s.reset()
        s.update(10.0)
        s.update(20.0)
        assert s.count == 2
        assert s.min_val == 10.0
        assert s.max_val == 20.0
        assert s.mean == 15.0


# ============================================================================
# RunningStats — edge cases
# ============================================================================

@pytest.mark.unit
@pytest.mark.edge_case
class TestRunningStatsEdgeCases:
    """Boundary values and floating point extremes."""

    def test_very_large_values(self):
        s = RunningStats()
        s.update(1e15)
        s.update(2e15)
        assert s.min_val == 1e15
        assert s.max_val == 2e15
        assert s.mean == pytest.approx(1.5e15)

    def test_very_small_values(self):
        s = RunningStats()
        s.update(1e-10)
        s.update(2e-10)
        assert s.min_val == 1e-10
        assert s.max_val == 2e-10
        assert s.mean == pytest.approx(1.5e-10)

    def test_inf_value(self):
        s = RunningStats()
        s.update(float('inf'))
        assert s.max_val == float('inf')
        assert s.mean == float('inf')

    def test_negative_inf(self):
        s = RunningStats()
        s.update(float('-inf'))
        assert s.min_val == float('-inf')


# ============================================================================
# cursor_delta — signed difference
# ============================================================================

@pytest.mark.unit
class TestCursorDelta:
    """Cursor delta calculations matching cursors.js."""

    def test_positive_delta(self):
        assert cursor_delta(1.0, 3.0) == 2.0

    def test_negative_delta(self):
        assert cursor_delta(5.0, 2.0) == -3.0

    def test_zero_delta(self):
        assert cursor_delta(7.0, 7.0) == 0.0

    def test_large_delta(self):
        assert cursor_delta(0.0, 1e6) == 1e6

    def test_negative_values(self):
        assert cursor_delta(-5.0, -2.0) == 3.0

    def test_cross_zero(self):
        assert cursor_delta(-10.0, 10.0) == 20.0

    def test_small_delta(self):
        assert cursor_delta(1.0, 1.001) == pytest.approx(0.001)
