"""Core timing calculation functions."""

from dataclasses import replace
from .platforms import Platform
from .operations import OPERATIONS


def cycles_to_us(cycles, clock_mhz):
    """Convert CPU cycles to microseconds."""
    return cycles / clock_mhz


def calculate_operation_time(op, platform):
    """
    Calculate execution time for an operation on a platform.
    Returns (total_cycles, time_us).
    """
    cycles = (
        op.float_muls * platform.cycles_per_float_mul +
        op.float_adds * platform.cycles_per_float_add +
        op.float_divs * platform.cycles_per_float_div +
        op.trig_ops * platform.cycles_per_trig +
        op.sqrt_ops * platform.cycles_per_sqrt
    )
    time_us = cycles_to_us(cycles, platform.clock_mhz)
    return cycles, time_us


def calculate_full_loop(platform, use_spi=False,
                        use_optimization=False, use_racing=False):
    """
    Calculate full flight control loop timing with optional feature tiers.
    Returns breakdown of each phase with separate I/O and compute tracking.
    """
    phases = []
    total_cycles = 0
    total_io_us = 0.0
    total_compute_us = 0.0

    def add_phase(name, op_key, io_us=0.0, tier="base"):
        nonlocal total_cycles, total_io_us, total_compute_us
        op = OPERATIONS[op_key]
        cycles, compute_us = calculate_operation_time(op, platform)
        phase_us = io_us + compute_us
        phases.append((name, cycles, io_us, compute_us, phase_us, tier))
        total_cycles += cycles
        total_io_us += io_us
        total_compute_us += compute_us

    # 1. IMU Read
    if use_spi:
        add_phase("IMU Read", "imu_read_spi", io_us=platform.spi_read_us)
    else:
        add_phase("IMU Read", "imu_read_i2c", io_us=platform.i2c_read_us)

    # 2. Calibration
    add_phase("Calibration", "calibration_apply")

    # 3. Low-pass filter
    add_phase("LP Filter", "lowpass_filter")

    # 3b. Optimization: Gyro biquad LPF
    if use_optimization:
        add_phase("Gyro Biquad LPF", "gyro_biquad_lpf", tier="optimization")

    # 3c. Optimization: Gyro notch filter
    if use_optimization:
        add_phase("Gyro Notch", "gyro_notch", tier="optimization")

    # 4. Madgwick filter
    add_phase("Madgwick 6DOF", "madgwick_6dof")

    # 4b. Optimization: Accel 2nd-stage LP
    if use_optimization:
        add_phase("Accel 2nd LP", "accel_stage2_lp", tier="optimization")

    # 5. Radio processing
    add_phase("Radio Process", "radio_process")

    # 5b. Racing: Expo curves
    if use_racing:
        add_phase("Expo Curves", "expo_curves", tier="racing")

    # 5c. Racing: Setpoint smoothing
    if use_racing:
        add_phase("Setpoint Smooth", "setpoint_smooth", tier="racing")

    # 6. PID control
    add_phase("PID (3-axis)", "pid_3axis")

    # 6b. D-term PT1 filter (base)
    add_phase("D-term PT1 LPF", "dterm_pt1_filter")

    # 6c. Optimization: D-term biquad upgrade
    if use_optimization:
        add_phase("D-term Biquad", "dterm_biquad", tier="optimization")

    # 6d. Racing: Feed-forward
    if use_racing:
        add_phase("Feed-forward", "feedforward", tier="racing")

    # 6e. Racing: TPA
    if use_racing:
        add_phase("TPA", "tpa", tier="racing")

    # 7. Motor mixing
    add_phase("Motor Mix", "motor_mix_quad")

    # 8. Command scaling
    add_phase("Cmd Scale", "command_scale")

    # 9. PWM output (fixed I/O time)
    pwm_us = 10.0
    phases.append(("PWM Output", 0, pwm_us, 0, pwm_us, "base"))
    total_io_us += pwm_us

    total_us = total_io_us + total_compute_us

    return {
        "phases": phases,
        "total_cycles": total_cycles,
        "total_us": total_us,
        "total_io_us": total_io_us,
        "total_compute_us": total_compute_us,
    }


def calculate_max_loop_rate(total_us, overhead_percent=20):
    """
    Calculate maximum achievable loop rate.
    overhead_percent: Reserve for interrupts, task switching, etc.
    """
    available_us = total_us * (1 + overhead_percent / 100)
    return 1_000_000 / available_us


def calculate_utilization(total_us, target_hz):
    """Calculate CPU utilization percentage for a target loop rate."""
    period_us = 1_000_000 / target_hz
    return (total_us / period_us) * 100


def calculate_min_clock(platform, target_hz, use_spi=False,
                        use_optimization=False, use_racing=False,
                        max_utilization=80.0):
    """
    Calculate minimum clock speed (MHz) needed to run the flight loop
    at target_hz with utilization <= max_utilization%.

    I/O time (I2C/SPI read, PWM) is fixed and does not scale with clock.
    Only compute cycles scale with clock speed.

    Returns minimum clock in MHz, or float('inf') if I/O-bound.
    """
    # Get the loop result to find total cycles and I/O time
    result = calculate_full_loop(platform, use_spi, use_optimization, use_racing)

    period_us = 1_000_000 / target_hz
    budget_us = period_us * (max_utilization / 100)
    compute_budget_us = budget_us - result["total_io_us"]

    if compute_budget_us <= 0:
        return float('inf')

    return result["total_cycles"] / compute_budget_us


def calculate_recommended_clock(platform, target_hz, **kwargs):
    """Minimum clock + 10% margin."""
    required = calculate_min_clock(platform, target_hz, **kwargs)
    if required == float('inf'):
        return float('inf')
    return required * 1.10
