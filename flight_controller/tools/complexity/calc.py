"""
CPU timing calculations from source-scanned operation counts.

Computes loop timing, utilization, and minimum clock requirements.
"""

from .platforms import get_cycles


# I/O overhead per loop iteration (not affected by clock speed)
IO_PHASES = {
    "i2c": {"name": "IMU Read (I2C)", "tier": "base"},
    "spi": {"name": "IMU Read (SPI)", "tier": "base"},
    "pwm": {"name": "PWM Output", "us": 10.0, "tier": "base"},
}

# Overhead reserve for interrupts, task switching, cache misses
OVERHEAD_FACTOR = 1.20  # 20% reserve
MAX_UTILIZATION = 0.80  # 80% max CPU before we flag it


def calculate_tier_time(ops, cycles, clock_mhz):
    """
    Calculate compute time for a set of operations.

    Args:
        ops: {mul, add, div, trig, sqrt}
        cycles: cycle cost profile from platforms.py
        clock_mhz: clock speed in MHz

    Returns:
        (total_cycles, compute_us)
    """
    total_cycles = (
        ops.get("mul", 0) * cycles["mul"] +
        ops.get("add", 0) * cycles["add"] +
        ops.get("div", 0) * cycles["div"] +
        ops.get("trig", 0) * cycles["trig"] +
        ops.get("sqrt", 0) * cycles["sqrt"]
    )
    compute_us = total_cycles / clock_mhz
    return total_cycles, compute_us


def calculate_loop_timing(platform, platform_key, features, op_totals):
    """
    Calculate full loop timing from source-scanned operation totals.

    Args:
        platform: Platform dataclass
        platform_key: key like "esp32" for cycle profile lookup
        features: from scanner.scan_config()
        op_totals: {tier: {mul, add, div, trig, sqrt}} from source_scanner

    Returns dict with:
        - tiers: [{name, ops, cycles, compute_us, tier}]
        - io_us: total I/O time
        - compute_us: total compute time
        - overhead_us: overhead reserve
        - total_us: io + compute + overhead
        - loop_budget_us: target loop period
        - utilization: fraction of budget used
        - headroom_us: remaining time
    """
    cycles = get_cycles(platform_key, platform.fpu)
    clock = platform.clock_mhz

    # I/O time (fixed, not affected by clock)
    io_us = 0.0
    if features.get("use_spi"):
        io_us += platform.spi_read_us
    else:
        io_us += platform.i2c_read_us
    io_us += 10.0  # PWM output

    # Compute time per tier
    tiers_detail = []
    total_compute_us = 0.0
    total_cycles_count = 0

    # Base tier is always active
    base_ops = op_totals.get("base", {})
    if any(base_ops.get(k, 0) > 0 for k in ("mul", "add", "div", "trig", "sqrt")):
        cyc, us = calculate_tier_time(base_ops, cycles, clock)
        tiers_detail.append({"name": "Base FC loop", "ops": base_ops, "cycles": cyc, "compute_us": us, "tier": "base"})
        total_compute_us += us
        total_cycles_count += cyc

    # Optimization tier
    if features.get("use_optimization"):
        opt_ops = op_totals.get("optimization", {})
        if any(opt_ops.get(k, 0) > 0 for k in ("mul", "add", "div", "trig", "sqrt")):
            cyc, us = calculate_tier_time(opt_ops, cycles, clock)
            tiers_detail.append({"name": "USE_OPTIMIZATION", "ops": opt_ops, "cycles": cyc, "compute_us": us, "tier": "optimization"})
            total_compute_us += us
            total_cycles_count += cyc

    # Racing tier
    if features.get("use_racing"):
        race_ops = op_totals.get("racing", {})
        if any(race_ops.get(k, 0) > 0 for k in ("mul", "add", "div", "trig", "sqrt")):
            cyc, us = calculate_tier_time(race_ops, cycles, clock)
            tiers_detail.append({"name": "USE_RACING", "ops": race_ops, "cycles": cyc, "compute_us": us, "tier": "racing"})
            total_compute_us += us
            total_cycles_count += cyc

    # Overhead
    overhead_us = (io_us + total_compute_us) * (OVERHEAD_FACTOR - 1.0)

    total_us = io_us + total_compute_us + overhead_us

    # Loop budget
    loop_hz = features.get("loop_frequency_hz", 1000)
    budget_us = 1_000_000.0 / loop_hz

    utilization = total_us / budget_us if budget_us > 0 else 0
    headroom_us = budget_us - total_us

    return {
        "tiers": tiers_detail,
        "io_us": io_us,
        "compute_us": total_compute_us,
        "overhead_us": overhead_us,
        "total_us": total_us,
        "total_cycles": total_cycles_count,
        "loop_budget_us": budget_us,
        "loop_hz": loop_hz,
        "utilization": utilization,
        "headroom_us": headroom_us,
        "clock_mhz": clock,
    }


def calculate_min_clock(platform, platform_key, features, op_totals):
    """Calculate minimum clock speed to stay under MAX_UTILIZATION."""
    cycles = get_cycles(platform_key, platform.fpu)

    # I/O is fixed
    io_us = platform.i2c_read_us if not features.get("use_spi") else platform.spi_read_us
    io_us += 10.0

    # Total cycles across active tiers
    total_cyc = 0
    for tier_name in ["base", "optimization", "racing"]:
        if tier_name == "optimization" and not features.get("use_optimization"):
            continue
        if tier_name == "racing" and not features.get("use_racing"):
            continue
        ops = op_totals.get(tier_name, {})
        cyc, _ = calculate_tier_time(ops, cycles, 1.0)  # cycles are clock-independent
        total_cyc += cyc

    loop_hz = features.get("loop_frequency_hz", 1000)
    budget_us = 1_000_000.0 / loop_hz
    available_us = budget_us * MAX_UTILIZATION - io_us * OVERHEAD_FACTOR

    if available_us <= 0:
        return float('inf')  # I/O alone exceeds budget

    # min_clock = total_cycles / available_us (in MHz)
    min_clock = total_cyc / available_us
    return min_clock


def calculate_recommended_clock(min_clock):
    """Add 10% margin to minimum clock."""
    return min_clock * 1.10
