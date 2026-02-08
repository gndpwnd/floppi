"""Clean tabular output for timing analysis. Numbers only, no prose."""

import math
from .platforms import Platform, PLATFORMS
from .calc import (
    calculate_full_loop, calculate_max_loop_rate,
    calculate_utilization, calculate_min_clock, calculate_recommended_clock,
)


def _status_str(min_clock, platform_clock):
    """Return OK or NOT ENOUGH with shortfall percentage."""
    if min_clock == float('inf'):
        return "I/O-BOUND"
    if min_clock > platform_clock:
        pct = (min_clock - platform_clock) / platform_clock * 100
        return f"NOT ENOUGH +{pct:.0f}%"
    return "OK"


def _feature_list(features):
    """Build feature tier string from features dict."""
    tiers = ["base"]
    if features["use_optimization"]:
        tiers.append("optimization")
    if features["use_racing"]:
        tiers.append("racing")
    return " + ".join(tiers)


def _core1_list(features):
    """Build Core 1 task list string."""
    tasks = []
    if features.get("display_enabled"):
        tasks.append("oled")
    if features.get("is_esp32"):
        tasks.append("wifi")
        if features.get("use_web_server"):
            tasks.append("web_server")
        if features.get("use_api_server"):
            tasks.append("api_client")
    return ", ".join(tasks) if tasks else "none"


def print_check(platform_key, platform, features):
    """Primary output: single platform check with pass/fail."""
    use_spi = features["use_spi"]
    use_opt = features["use_optimization"]
    use_race = features["use_racing"]
    target_hz = features["loop_frequency_hz"]

    result = calculate_full_loop(platform, use_spi, use_opt, use_race)
    util = calculate_utilization(result["total_us"], target_hz)
    max_rate = calculate_max_loop_rate(result["total_us"])
    min_clk = calculate_min_clock(platform, target_hz, use_spi, use_opt, use_race)
    rec_clk = calculate_recommended_clock(platform, target_hz,
                                          use_spi=use_spi,
                                          use_optimization=use_opt,
                                          use_racing=use_race)
    status = _status_str(min_clk, platform.clock_mhz)

    # Header
    fpu_str = "FPU" if platform.fpu else "no FPU"
    bus = "SPI" if use_spi else "I2C"
    print(f"Platform:     {platform.name} @ {platform.clock_mhz:.0f} MHz, "
          f"{platform.cores} core{'s' if platform.cores > 1 else ''}, {fpu_str}")
    print(f"Loop rate:    {target_hz} Hz")
    print(f"IMU:          {features['imu']} ({bus})")
    print(f"Features:     {_feature_list(features)}")
    if platform.cores > 1:
        print(f"Core 1:       {_core1_list(features)}")
    print()

    # Phase breakdown
    print(f"{'Phase':<22} {'Cycles':>7}  {'I/O(us)':>8}  {'Compute(us)':>12}  "
          f"{'Total(us)':>10}  {'Tier'}")
    print("-" * 78)
    for name, cycles, io_us, compute_us, total_us, tier in result["phases"]:
        print(f"{name:<22} {cycles:>7}  {io_us:>8.2f}  {compute_us:>12.2f}  "
              f"{total_us:>10.2f}  {tier}")
    print("-" * 78)
    print(f"{'TOTAL':<22} {result['total_cycles']:>7}  {result['total_io_us']:>8.2f}  "
          f"{result['total_compute_us']:>12.2f}  {result['total_us']:>10.2f}")
    print()

    # Summary
    print(f"Utilization:  {util:.1f}% @ {target_hz} Hz")
    print(f"Max rate:     {max_rate:,.0f} Hz")
    if min_clk == float('inf'):
        print(f"Min clock:    I/O time ({result['total_io_us']:.0f} us) "
              f"exceeds loop budget")
    else:
        print(f"Min clock:    {min_clk:.1f} MHz (required), "
              f"{rec_clk:.1f} MHz (recommended)")
    print(f"Status:       {status}")


def print_comparison(features):
    """All-platform comparison table."""
    use_spi = features["use_spi"]
    use_opt = features["use_optimization"]
    use_race = features["use_racing"]
    target_hz = features["loop_frequency_hz"]
    bus = "SPI" if use_spi else "I2C"

    print(f"Features: {_feature_list(features)} | "
          f"IMU: {features['imu']} ({bus}) | Target: {target_hz} Hz")
    print()
    print(f"{'Platform':<32} {'MHz':>5}  {'Cores':>5}  {'FPU':>3}  "
          f"{'Loop(us)':>8}  {'Util%':>6}  {'MaxRate':>7}  "
          f"{'MinClk':>6}  {'Status'}")
    print("-" * 100)

    for key, plat in PLATFORMS.items():
        result = calculate_full_loop(plat, use_spi, use_opt, use_race)
        util = calculate_utilization(result["total_us"], target_hz)
        max_rate = calculate_max_loop_rate(result["total_us"])
        min_clk = calculate_min_clock(plat, target_hz, use_spi, use_opt, use_race)
        status = _status_str(min_clk, plat.clock_mhz)
        fpu = "yes" if plat.fpu else "no"

        min_clk_str = f"{min_clk:.1f}" if min_clk != float('inf') else "inf"
        print(f"{plat.name:<32} {plat.clock_mhz:>5.0f}  {plat.cores:>5}  "
              f"{fpu:>3}  {result['total_us']:>8.1f}  {util:>5.1f}%  "
              f"{max_rate:>7,.0f}  {min_clk_str:>6}  {status}")


def print_breakdown(platform, features):
    """Phase-by-phase detail for a single platform."""
    use_spi = features["use_spi"]
    use_opt = features["use_optimization"]
    use_race = features["use_racing"]

    result = calculate_full_loop(platform, use_spi, use_opt, use_race)

    print(f"{'Phase':<22} {'Cycles':>7}  {'I/O(us)':>8}  {'Compute(us)':>12}  "
          f"{'Total(us)':>10}  {'Tier'}")
    print("-" * 78)
    for name, cycles, io_us, compute_us, total_us, tier in result["phases"]:
        print(f"{name:<22} {cycles:>7}  {io_us:>8.2f}  {compute_us:>12.2f}  "
              f"{total_us:>10.2f}  {tier}")
    print("-" * 78)
    print(f"{'TOTAL':<22} {result['total_cycles']:>7}  {result['total_io_us']:>8.2f}  "
          f"{result['total_compute_us']:>12.2f}  {result['total_us']:>10.2f}")
