"""Unit tests for ThresholdTrigger (period-detector.js Python port).

Tests the exact behavior of period detection via threshold crossing,
including rising/falling edge, hysteresis, auto-level, and period collection.

Markers: unit
"""

import math
import pytest
from parsers import ThresholdTrigger, TriggerResult


# ============================================================================
# Helper: generate a sine wave
# ============================================================================

def sine_wave(n_samples, frequency=1.0, amplitude=1.0, offset=0.0, sample_rate=50):
    """Generate n_samples of a sine wave."""
    return [
        offset + amplitude * math.sin(2 * math.pi * frequency * i / sample_rate)
        for i in range(n_samples)
    ]


def ramp_wave(n_samples, low=-1.0, high=1.0, period=20):
    """Generate a sawtooth/ramp wave that crosses zero."""
    result = []
    for i in range(n_samples):
        phase = (i % period) / period
        result.append(low + (high - low) * phase)
    return result


# ============================================================================
# ThresholdTrigger — construction and defaults
# ============================================================================

@pytest.mark.unit
class TestThresholdTriggerDefaults:
    """Constructor defaults match JS implementation."""

    def test_default_level(self):
        t = ThresholdTrigger()
        assert t.level == 0.0

    def test_default_edge(self):
        t = ThresholdTrigger()
        assert t.edge == 'rising'

    def test_default_hysteresis(self):
        t = ThresholdTrigger()
        assert t.hysteresis == 0.0

    def test_default_periods_needed(self):
        t = ThresholdTrigger()
        assert t.periods_needed == 1

    def test_default_sample_rate(self):
        t = ThresholdTrigger()
        assert t.sample_rate == 50.0

    def test_default_min_period_samples(self):
        t = ThresholdTrigger()
        assert t.min_period_samples == 5

    def test_custom_options(self):
        t = ThresholdTrigger(level=1.5, edge='falling', hysteresis=0.2,
                             periods_needed=3, sample_rate=100, min_period_samples=10)
        assert t.level == 1.5
        assert t.edge == 'falling'
        assert t.hysteresis == 0.2
        assert t.periods_needed == 3
        assert t.sample_rate == 100
        assert t.min_period_samples == 10

    def test_initial_state(self):
        t = ThresholdTrigger()
        assert t.prev_value is None
        assert t.armed is True
        assert t.sample_index == 0
        assert t.current_period == []
        assert t.completed_periods == []
        assert t.trigger_count == 0


# ============================================================================
# ThresholdTrigger — rising edge detection
# ============================================================================

@pytest.mark.unit
class TestRisingEdge:
    """Rising edge crossing detection."""

    def test_simple_rising_cross(self):
        """Signal going from negative to positive crosses level=0."""
        t = ThresholdTrigger(level=0.0, min_period_samples=2)
        # Feed a sequence that crosses zero upward twice
        data = [-2, -1, 1, 2, 3, 2, -1, -2, -1, 1, 2]
        results = [t.add_sample(v) for v in data]
        # Should have triggered at least once with a result
        non_none = [r for r in results if r is not None]
        assert len(non_none) >= 1
        # First result is the prefix period (trigger_count=1, no timing yet)
        # After second trigger we get timing data
        last = non_none[-1]
        assert last.trigger_count >= 2

    def test_no_trigger_below_level(self):
        """Signal staying below level should not trigger."""
        t = ThresholdTrigger(level=0.0, min_period_samples=2)
        data = [-5, -4, -3, -2, -1, -2, -3]
        results = [t.add_sample(v) for v in data]
        assert all(r is None for r in results)

    def test_no_trigger_above_level(self):
        """Signal staying above level should not trigger."""
        t = ThresholdTrigger(level=0.0, min_period_samples=2)
        data = [1, 2, 3, 4, 5, 4, 3]
        results = [t.add_sample(v) for v in data]
        assert all(r is None for r in results)

    def test_falling_does_not_trigger_rising(self):
        """Falling edge should not trigger a rising-edge detector."""
        t = ThresholdTrigger(level=0.0, edge='rising', min_period_samples=2)
        # Only crosses downward
        data = [3, 2, 1, -1, -2, -3]
        results = [t.add_sample(v) for v in data]
        assert all(r is None for r in results)


# ============================================================================
# ThresholdTrigger — falling edge detection
# ============================================================================

@pytest.mark.unit
class TestFallingEdge:
    """Falling edge crossing detection."""

    def test_simple_falling_cross(self):
        """Signal going from positive to negative crosses level=0."""
        t = ThresholdTrigger(level=0.0, edge='falling', min_period_samples=2)
        # Cross downward twice
        data = [2, 1, -1, -2, -1, 1, 2, 1, -1, -2]
        results = [t.add_sample(v) for v in data]
        non_none = [r for r in results if r is not None]
        assert len(non_none) >= 1

    def test_rising_does_not_trigger_falling(self):
        """Rising edge should not trigger a falling-edge detector."""
        t = ThresholdTrigger(level=0.0, edge='falling', min_period_samples=2)
        # Only crosses upward
        data = [-3, -2, -1, 1, 2, 3]
        results = [t.add_sample(v) for v in data]
        assert all(r is None for r in results)


# ============================================================================
# ThresholdTrigger — period collection
# ============================================================================

@pytest.mark.unit
class TestPeriodCollection:
    """Period accumulation and return behavior."""

    def test_collect_one_period(self):
        """Default periodsNeeded=1: returns result with 1 period each time."""
        t = ThresholdTrigger(level=0.0, periods_needed=1, min_period_samples=2)
        # Two upward crossings to complete one period
        data = [-1, 1, 2, 3, 2, 1, -1, -2, -1, 1]
        results = [t.add_sample(v) for v in data]
        non_none = [r for r in results if r is not None]
        # First trigger returns prefix as a period, second trigger returns a real period
        assert len(non_none) >= 1
        for r in non_none:
            assert len(r.periods) == 1
            assert len(r.periods[0]) > 0

    def test_collect_multiple_periods(self):
        """periodsNeeded=2: returns when 2 complete periods are collected."""
        t = ThresholdTrigger(level=0.0, periods_needed=2, min_period_samples=2,
                             sample_rate=50)
        # Generate a ramp that crosses zero periodically
        data = ramp_wave(200, low=-1.0, high=1.0, period=20)
        results = [t.add_sample(v) for v in data]
        non_none = [r for r in results if r is not None]
        assert len(non_none) >= 1
        # The result should have 2 periods
        assert len(non_none[0].periods) == 2

    def test_period_data_content(self):
        """Period data should contain actual sample values."""
        t = ThresholdTrigger(level=0.0, periods_needed=1, min_period_samples=2)
        data = [-1, 1, 5, 10, 5, 1, -1, -5, -1, 1]
        results = [t.add_sample(v) for v in data]
        non_none = [r for r in results if r is not None]
        assert len(non_none) >= 1
        # Check the last result (has a real period between two triggers)
        period = non_none[-1].periods[0]
        # Period should contain values between the two trigger points
        assert all(isinstance(v, (int, float)) for v in period)
        assert len(period) > 0

    def test_first_trigger_returns_prefix(self):
        """First trigger returns the prefix data as a period (no timing yet)."""
        t = ThresholdTrigger(level=0.0, periods_needed=1, min_period_samples=2)
        # Single upward crossing — returns prefix as initial period
        data = [-2, -1, 1]
        results = [t.add_sample(v) for v in data]
        non_none = [r for r in results if r is not None]
        assert len(non_none) == 1
        # First trigger: no timing data yet (needs 2+ triggers)
        assert non_none[0].frequency_hz == 0.0
        assert non_none[0].period_ms == 0.0
        assert non_none[0].trigger_count == 1


# ============================================================================
# ThresholdTrigger — timing calculations
# ============================================================================

@pytest.mark.unit
class TestTimingCalculations:
    """Period timing, frequency, and millisecond calculations."""

    def test_frequency_calculation(self):
        """Detected frequency should match actual signal frequency."""
        sample_rate = 100
        freq = 5.0  # 5 Hz → 20 samples per period
        t = ThresholdTrigger(level=0.0, periods_needed=1,
                             sample_rate=sample_rate, min_period_samples=2)
        data = sine_wave(200, frequency=freq, sample_rate=sample_rate)
        results = [t.add_sample(v) for v in data]
        non_none = [r for r in results if r is not None]
        assert len(non_none) >= 2  # first result has no timing, need 2+
        # Skip first result (prefix, no timing), check subsequent results
        r = non_none[1]
        assert r.frequency_hz == pytest.approx(freq, rel=0.15)

    def test_period_ms_calculation(self):
        """Period in milliseconds should match sample_rate / period_samples * 1000."""
        sample_rate = 100
        freq = 5.0  # 20 samples per period → 200 ms
        t = ThresholdTrigger(level=0.0, periods_needed=1,
                             sample_rate=sample_rate, min_period_samples=2)
        data = sine_wave(200, frequency=freq, sample_rate=sample_rate)
        results = [t.add_sample(v) for v in data]
        non_none = [r for r in results if r is not None]
        assert len(non_none) >= 2
        r = non_none[1]
        assert r.period_ms == pytest.approx(200.0, rel=0.15)

    def test_period_samples_count(self):
        """Detected period samples should be close to actual period length."""
        sample_rate = 100
        freq = 10.0  # 10 samples per period
        t = ThresholdTrigger(level=0.0, periods_needed=1,
                             sample_rate=sample_rate, min_period_samples=2)
        data = sine_wave(200, frequency=freq, sample_rate=sample_rate)
        results = [t.add_sample(v) for v in data]
        non_none = [r for r in results if r is not None]
        assert len(non_none) >= 2
        r = non_none[1]
        assert r.period_samples == pytest.approx(10, abs=2)


# ============================================================================
# ThresholdTrigger — hysteresis
# ============================================================================

@pytest.mark.unit
class TestHysteresis:
    """Hysteresis prevents false triggers from noise around the level."""

    def test_hysteresis_prevents_chatter(self):
        """Noisy signal near the level should not cause extra triggers."""
        t = ThresholdTrigger(level=0.0, hysteresis=0.5, min_period_samples=2,
                             periods_needed=1)
        # Signal chatters around zero but only clearly crosses once
        data = [-2, -1, -0.1, 0.1, -0.1, 0.1, 0.2, -0.1,  # chatter near zero
                2, 3, 4, 3, 2, 1, -1, -2,                    # clear cycle
                -1, 0.1, -0.1, 0.1, -0.1,                    # more chatter
                1, 2, 3]                                       # second clear rise
        results = [t.add_sample(v) for v in data]
        non_none = [r for r in results if r is not None]
        # With hysteresis, chatter near zero shouldn't cause multiple triggers
        # We should get at most 1 result (not many from the chatter)
        assert len(non_none) <= 2

    def test_hysteresis_rearms(self):
        """Trigger re-arms when signal moves away from level by hysteresis amount."""
        t = ThresholdTrigger(level=0.0, hysteresis=1.0, min_period_samples=2,
                             periods_needed=1)
        # Clear rising crossing → disarms → goes far below → re-arms → crosses again
        data = [-3, -2, -1, 1, 2, 3,    # first crossing (trigger, disarms)
                 2, 1, -1, -2, -3,        # goes below -1.0 → re-arms
                 -2, -1, 1, 2]            # second crossing (triggers again)
        results = [t.add_sample(v) for v in data]
        # Should eventually get a result (2nd trigger after re-arm)
        non_none = [r for r in results if r is not None]
        assert len(non_none) >= 1


# ============================================================================
# ThresholdTrigger — minimum period samples
# ============================================================================

@pytest.mark.unit
class TestMinPeriodSamples:
    """minPeriodSamples prevents triggers from very short periods."""

    def test_short_period_rejected(self):
        """Crossings closer than minPeriodSamples apart should not trigger."""
        t = ThresholdTrigger(level=0.0, min_period_samples=10)
        # Rapid crossings 3 samples apart — below min_period_samples
        data = [-1, 1, -1, 1, -1, 1, -1, 1]
        results = [t.add_sample(v) for v in data]
        assert all(r is None for r in results)

    def test_long_enough_period_accepted(self):
        """Crossings at least minPeriodSamples apart should trigger."""
        t = ThresholdTrigger(level=0.0, min_period_samples=5, periods_needed=1)
        # Two crossings with >5 samples between them
        data = [-1, -0.8, -0.5, -0.2, 0.5, 1, 2, 3,  # first crossing at sample ~4-5
                 2, 1, -1, -2, -1, -0.5, 0.5, 1]       # second crossing at sample ~14-15
        results = [t.add_sample(v) for v in data]
        non_none = [r for r in results if r is not None]
        assert len(non_none) >= 1


# ============================================================================
# ThresholdTrigger — auto-level
# ============================================================================

@pytest.mark.unit
class TestAutoLevel:
    """Auto-level detection from data range."""

    def test_auto_level_midpoint(self):
        """Level should be set to midpoint of data range."""
        t = ThresholdTrigger()
        t.auto_level([0, 2, 4, 6, 8, 10])
        assert t.level == pytest.approx(5.0)

    def test_auto_level_hysteresis(self):
        """Hysteresis should be set to 5% of data range."""
        t = ThresholdTrigger()
        t.auto_level([0, 10])
        assert t.hysteresis == pytest.approx(0.5)  # 5% of 10

    def test_auto_level_negative_range(self):
        """Works with negative values."""
        t = ThresholdTrigger()
        t.auto_level([-10, -5, 0, 5, 10])
        assert t.level == pytest.approx(0.0)
        assert t.hysteresis == pytest.approx(1.0)  # 5% of 20

    def test_auto_level_small_range(self):
        """Works with small data ranges."""
        t = ThresholdTrigger()
        t.auto_level([0.99, 1.0, 1.01])
        assert t.level == pytest.approx(1.0, abs=0.01)
        assert t.hysteresis == pytest.approx(0.001, abs=0.001)

    def test_auto_level_too_few_samples(self):
        """Does nothing with fewer than 2 samples."""
        t = ThresholdTrigger(level=5.0, hysteresis=1.0)
        t.auto_level([42.0])
        assert t.level == 5.0  # unchanged
        assert t.hysteresis == 1.0  # unchanged

    def test_auto_level_empty(self):
        """Does nothing with empty data."""
        t = ThresholdTrigger(level=5.0)
        t.auto_level([])
        assert t.level == 5.0

    def test_auto_level_none(self):
        """Does nothing with None."""
        t = ThresholdTrigger(level=5.0)
        t.auto_level(None)
        assert t.level == 5.0


# ============================================================================
# ThresholdTrigger — reset
# ============================================================================

@pytest.mark.unit
class TestReset:
    """Reset clears all state for mode switching."""

    def test_reset_clears_state(self):
        t = ThresholdTrigger(level=0.0, min_period_samples=2)
        # Feed some data to accumulate state
        for v in [-1, 1, 2, 3, -1, -2, 1]:
            t.add_sample(v)
        assert t.sample_index > 0
        assert t.trigger_count > 0

        t.reset()
        assert t.prev_value is None
        assert t.armed is True
        assert t.sample_index == 0
        assert t.current_period == []
        assert t.completed_periods == []
        assert t.last_trigger_index == 0
        assert t.detected_period_samples == 0
        assert t.detected_frequency_hz == 0.0
        assert t.detected_period_ms == 0.0
        assert t.trigger_count == 0

    def test_reset_then_reuse(self):
        """After reset, trigger should work fresh."""
        t = ThresholdTrigger(level=0.0, periods_needed=1, min_period_samples=2)
        # First use
        data1 = [-1, 1, 2, 3, 2, -1, -2, -1, 1]
        for v in data1:
            t.add_sample(v)

        t.reset()

        # Second use — should work independently
        data2 = [-2, -1, 1, 2, 3, 2, -1, -2, -1, 1]
        results = [t.add_sample(v) for v in data2]
        non_none = [r for r in results if r is not None]
        assert len(non_none) >= 1


# ============================================================================
# ThresholdTrigger — sine wave integration
# ============================================================================

@pytest.mark.unit
class TestSineWaveIntegration:
    """Full integration test: feed a clean sine wave, verify period detection."""

    def test_sine_1hz_at_50hz_sample_rate(self):
        """1 Hz sine at 50 Hz → period = 50 samples, freq = 1 Hz."""
        t = ThresholdTrigger(level=0.0, periods_needed=1,
                             sample_rate=50, min_period_samples=5)
        data = sine_wave(300, frequency=1.0, sample_rate=50)
        results = [t.add_sample(v) for v in data]
        non_none = [r for r in results if r is not None]
        assert len(non_none) >= 2  # first has no timing, need 2+
        r = non_none[1]  # skip prefix result
        assert r.frequency_hz == pytest.approx(1.0, rel=0.1)
        assert r.period_samples == pytest.approx(50, rel=0.1)
        assert r.period_ms == pytest.approx(1000.0, rel=0.1)

    def test_sine_5hz_at_100hz_sample_rate(self):
        """5 Hz sine at 100 Hz → period = 20 samples, freq = 5 Hz."""
        t = ThresholdTrigger(level=0.0, periods_needed=1,
                             sample_rate=100, min_period_samples=5)
        data = sine_wave(400, frequency=5.0, sample_rate=100)
        results = [t.add_sample(v) for v in data]
        non_none = [r for r in results if r is not None]
        assert len(non_none) >= 2
        r = non_none[1]
        assert r.frequency_hz == pytest.approx(5.0, rel=0.1)
        assert r.period_ms == pytest.approx(200.0, rel=0.1)

    def test_offset_sine(self):
        """Sine with DC offset — auto-level should find the right trigger."""
        t = ThresholdTrigger(sample_rate=100, periods_needed=1, min_period_samples=5)
        data = sine_wave(400, frequency=2.0, amplitude=1.0, offset=5.0, sample_rate=100)
        # Auto-level from first 100 samples
        t.auto_level(data[:100])
        assert t.level == pytest.approx(5.0, abs=0.1)

        results = [t.add_sample(v) for v in data]
        non_none = [r for r in results if r is not None]
        assert len(non_none) >= 2
        r = non_none[1]  # skip prefix result
        assert r.frequency_hz == pytest.approx(2.0, rel=0.15)

    def test_collect_3_periods(self):
        """Collect 3 complete periods from a sine wave."""
        t = ThresholdTrigger(level=0.0, periods_needed=3,
                             sample_rate=100, min_period_samples=5)
        data = sine_wave(500, frequency=5.0, sample_rate=100)
        results = [t.add_sample(v) for v in data]
        non_none = [r for r in results if r is not None]
        assert len(non_none) >= 1
        r = non_none[0]
        assert len(r.periods) == 3
        # Each period should be approximately 20 samples
        for period in r.periods:
            assert len(period) == pytest.approx(20, abs=3)


# ============================================================================
# ThresholdTrigger — edge cases
# ============================================================================

@pytest.mark.unit
@pytest.mark.edge_case
class TestTriggerEdgeCases:
    """Boundary conditions and unusual inputs."""

    def test_constant_signal(self):
        """Constant signal should never trigger."""
        t = ThresholdTrigger(level=0.0, min_period_samples=2)
        data = [5.0] * 100
        results = [t.add_sample(v) for v in data]
        assert all(r is None for r in results)

    def test_single_sample(self):
        """Single sample should not trigger."""
        t = ThresholdTrigger(level=0.0)
        assert t.add_sample(1.0) is None

    def test_exact_level_crossing(self):
        """Value exactly at level should count as crossing for rising edge."""
        t = ThresholdTrigger(level=0.0, min_period_samples=2)
        # -1 → 0 is a rising crossing (prevValue < level && value >= level)
        data = [-2, -1, 0, 1, 2, 1, -1, -2, -1, 0, 1]
        results = [t.add_sample(v) for v in data]
        non_none = [r for r in results if r is not None]
        assert len(non_none) >= 1

    def test_very_high_frequency(self):
        """High frequency signal should still detect periods."""
        t = ThresholdTrigger(level=0.0, sample_rate=1000, min_period_samples=3,
                             periods_needed=1)
        # 100 Hz at 1000 Hz sample rate → 10 samples per period
        data = sine_wave(200, frequency=100, sample_rate=1000)
        results = [t.add_sample(v) for v in data]
        non_none = [r for r in results if r is not None]
        assert len(non_none) >= 2  # first has no timing
        assert non_none[1].frequency_hz == pytest.approx(100.0, rel=0.15)

    def test_non_zero_level(self):
        """Trigger at a non-zero level."""
        t = ThresholdTrigger(level=5.0, min_period_samples=2, periods_needed=1)
        # Signal oscillating between 0 and 10, crossing level=5
        data = [0, 2, 4, 6, 8, 10, 8, 6, 4, 2, 0, 2, 4, 6, 8]
        results = [t.add_sample(v) for v in data]
        non_none = [r for r in results if r is not None]
        assert len(non_none) >= 1

    def test_trigger_count_increments(self):
        """Trigger count should increment with each crossing."""
        t = ThresholdTrigger(level=0.0, min_period_samples=2)
        data = ramp_wave(200, low=-1.0, high=1.0, period=20)
        last_result = None
        for v in data:
            r = t.add_sample(v)
            if r is not None:
                last_result = r
        assert last_result is not None
        assert last_result.trigger_count > 2

    def test_result_type(self):
        """Result should be a TriggerResult with correct fields."""
        t = ThresholdTrigger(level=0.0, periods_needed=1, min_period_samples=2)
        data = [-1, 1, 2, 3, 2, -1, -2, -1, 1]
        for v in data:
            r = t.add_sample(v)
            if r is not None:
                assert isinstance(r, TriggerResult)
                assert isinstance(r.periods, list)
                assert isinstance(r.period_samples, int)
                assert isinstance(r.frequency_hz, float)
                assert isinstance(r.period_ms, float)
                assert isinstance(r.trigger_count, int)
                return
        pytest.fail("Expected at least one TriggerResult")
