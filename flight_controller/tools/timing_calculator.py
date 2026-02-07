#!/usr/bin/env python3
"""
Flight Controller Timing Calculator

Calculates clock speed requirements, loop budgets, and latency analysis
for flight controller firmware across different microcontroller platforms.

Uses computer science complexity analysis to estimate operation times.

Usage:
    python3 timing_calculator.py           # Full analysis
    python3 timing_calculator.py --check   # Interactive clock speed checker
"""

import sys
import re
from dataclasses import dataclass
from typing import Dict, List, Tuple, Optional


# =============================================================================
# Platform Specifications
# =============================================================================

@dataclass
class Platform:
    """Microcontroller platform specifications."""
    name: str
    clock_mhz: float
    cores: int
    fpu: bool  # Hardware floating-point unit
    flash_kb: int
    ram_kb: int
    # Estimated cycles per operation (with FPU if available)
    cycles_per_float_mul: int
    cycles_per_float_add: int
    cycles_per_float_div: int
    cycles_per_trig: int  # sin/cos/atan2
    cycles_per_sqrt: int
    # I/O latencies
    i2c_read_us: float  # Time to read 14 bytes at max speed
    spi_read_us: float


PLATFORMS = {
    "teensy40": Platform(
        name="Teensy 4.0 (ARM Cortex-M7)",
        clock_mhz=600,
        cores=1,
        fpu=True,
        flash_kb=2048,
        ram_kb=1024,
        cycles_per_float_mul=1,  # Single-cycle FPU
        cycles_per_float_add=1,
        cycles_per_float_div=14,
        cycles_per_trig=20,  # With DSP instructions
        cycles_per_sqrt=14,
        i2c_read_us=100,  # 14 bytes @ 1MHz
        spi_read_us=15,
    ),
    "esp32": Platform(
        name="ESP32 (Xtensa LX6)",
        clock_mhz=240,
        cores=2,
        fpu=True,
        flash_kb=4096,
        ram_kb=520,
        cycles_per_float_mul=1,
        cycles_per_float_add=1,
        cycles_per_float_div=35,
        cycles_per_trig=50,
        cycles_per_sqrt=30,
        i2c_read_us=140,  # 14 bytes @ 400kHz typical
        spi_read_us=20,
    ),
    "esp32s3": Platform(
        name="ESP32-S3 (Xtensa LX7)",
        clock_mhz=240,
        cores=2,
        fpu=True,
        flash_kb=8192,
        ram_kb=512,
        cycles_per_float_mul=1,
        cycles_per_float_add=1,
        cycles_per_float_div=30,
        cycles_per_trig=45,
        cycles_per_sqrt=25,
        i2c_read_us=140,
        spi_read_us=18,
    ),
    "stm32f405": Platform(
        name="STM32F405 (ARM Cortex-M4)",
        clock_mhz=168,
        cores=1,
        fpu=True,
        flash_kb=1024,
        ram_kb=192,
        cycles_per_float_mul=1,
        cycles_per_float_add=1,
        cycles_per_float_div=14,
        cycles_per_trig=25,
        cycles_per_sqrt=14,
        i2c_read_us=120,
        spi_read_us=18,
    ),
    "arduino_uno": Platform(
        name="Arduino Uno (ATmega328P)",
        clock_mhz=16,
        cores=1,
        fpu=False,  # No hardware FPU!
        flash_kb=32,
        ram_kb=2,
        # Software float is VERY slow on AVR
        cycles_per_float_mul=150,  # ~10us at 16MHz
        cycles_per_float_add=100,
        cycles_per_float_div=400,
        cycles_per_trig=1500,  # Very slow software implementation
        cycles_per_sqrt=300,
        i2c_read_us=350,  # 14 bytes @ 400kHz
        spi_read_us=50,
    ),
    "arduino_mega": Platform(
        name="Arduino Mega (ATmega2560)",
        clock_mhz=16,
        cores=1,
        fpu=False,
        flash_kb=256,
        ram_kb=8,
        cycles_per_float_mul=150,
        cycles_per_float_add=100,
        cycles_per_float_div=400,
        cycles_per_trig=1500,
        cycles_per_sqrt=300,
        i2c_read_us=350,
        spi_read_us=50,
    ),
    "rp2040": Platform(
        name="Raspberry Pi Pico (RP2040)",
        clock_mhz=133,
        cores=2,
        fpu=False,  # Cortex-M0+ has no FPU
        flash_kb=2048,
        ram_kb=264,
        cycles_per_float_mul=50,  # Software but faster than AVR
        cycles_per_float_add=30,
        cycles_per_float_div=100,
        cycles_per_trig=300,
        cycles_per_sqrt=80,
        i2c_read_us=140,
        spi_read_us=20,
    ),
}


# =============================================================================
# Operation Complexity Analysis
# =============================================================================

@dataclass
class Operation:
    """Flight controller operation with complexity analysis."""
    name: str
    float_muls: int
    float_adds: int
    float_divs: int
    trig_ops: int
    sqrt_ops: int
    description: str


# Based on actual dRehmFlight code analysis
OPERATIONS = {
    "imu_read_i2c": Operation(
        name="IMU Read (I2C)",
        float_muls=6,   # Scale conversions
        float_adds=6,   # Offset corrections
        float_divs=0,
        trig_ops=0,
        sqrt_ops=0,
        description="Read 6 values (accel + gyro), convert to float",
    ),
    "imu_read_spi": Operation(
        name="IMU Read (SPI)",
        float_muls=6,
        float_adds=6,
        float_divs=0,
        trig_ops=0,
        sqrt_ops=0,
        description="Read 6 values via SPI, convert to float",
    ),
    "calibration_apply": Operation(
        name="Calibration Apply",
        float_muls=6,   # 3 scale factors applied
        float_adds=6,   # 3 offsets for accel, 3 for gyro
        float_divs=0,
        trig_ops=0,
        sqrt_ops=0,
        description="Apply offset and scale calibration",
    ),
    "lowpass_filter": Operation(
        name="Low-pass Filter (6 axes)",
        float_muls=12,  # 2 per axis (prev * (1-B) + new * B)
        float_adds=6,   # 1 per axis
        float_divs=0,
        trig_ops=0,
        sqrt_ops=0,
        description="IIR filter for 6 sensor values",
    ),
    "madgwick_6dof": Operation(
        name="Madgwick Filter (6DOF)",
        float_muls=80,   # Quaternion math, gradient descent
        float_adds=60,
        float_divs=0,
        trig_ops=3,      # atan2, asin for euler angles
        sqrt_ops=2,      # Normalization (invSqrt)
        description="6DOF attitude estimation (no magnetometer)",
    ),
    "madgwick_9dof": Operation(
        name="Madgwick Filter (9DOF)",
        float_muls=120,  # Additional magnetometer fusion
        float_adds=90,
        float_divs=0,
        trig_ops=3,
        sqrt_ops=3,
        description="9DOF attitude estimation with magnetometer",
    ),
    "pid_single_axis": Operation(
        name="PID Controller (1 axis)",
        float_muls=4,   # P, I, D gains + dt
        float_adds=5,   # error, integral sum, derivative
        float_divs=0,
        trig_ops=0,
        sqrt_ops=0,
        description="Single axis PID: P*e + I*integral + D*derivative",
    ),
    "pid_3axis": Operation(
        name="PID Controller (3 axes)",
        float_muls=12,
        float_adds=15,
        float_divs=0,
        trig_ops=0,
        sqrt_ops=0,
        description="Roll, pitch, yaw PID controllers",
    ),
    "motor_mix_quad": Operation(
        name="Motor Mixer (Quad)",
        float_muls=16,  # 4 motors * 4 contributions
        float_adds=12,  # Summing contributions
        float_divs=0,
        trig_ops=0,
        sqrt_ops=0,
        description="Mix throttle, roll, pitch, yaw to 4 motors",
    ),
    "motor_mix_hex": Operation(
        name="Motor Mixer (Hex)",
        float_muls=24,
        float_adds=18,
        float_divs=0,
        trig_ops=0,
        sqrt_ops=0,
        description="Mix to 6 motors",
    ),
    "command_scale": Operation(
        name="Command Scaling",
        float_muls=6,   # Scale to PWM range
        float_adds=6,   # Add offset
        float_divs=0,
        trig_ops=0,
        sqrt_ops=0,
        description="Scale motor/servo commands to PWM",
    ),
    "radio_process": Operation(
        name="Radio Processing",
        float_muls=6,   # Normalize channels
        float_adds=6,   # Center offsets
        float_divs=0,
        trig_ops=0,
        sqrt_ops=0,
        description="Process 6 radio channels to normalized values",
    ),
}


# =============================================================================
# Clock Speed Parsing
# =============================================================================

def parse_clock_speed(speed_str: str) -> Optional[float]:
    """
    Parse clock speed string with k/m/g suffix.

    Examples:
        16m or 16M -> 16.0 MHz
        600m -> 600.0 MHz
        1g or 1G -> 1000.0 MHz
        133k -> 0.133 MHz
        240 -> 240.0 MHz (assumed MHz)

    Returns None if parsing fails.
    """
    speed_str = speed_str.strip().lower()

    # Match number with optional suffix
    match = re.match(r'^(\d+\.?\d*)\s*([kmg])?$', speed_str)
    if not match:
        return None

    value = float(match.group(1))
    suffix = match.group(2)

    if suffix == 'k':
        return value / 1000.0  # kHz to MHz
    elif suffix == 'g':
        return value * 1000.0  # GHz to MHz
    else:
        return value  # Already MHz or no suffix (assume MHz)


# =============================================================================
# Timing Calculations
# =============================================================================

def cycles_to_us(cycles: int, clock_mhz: float) -> float:
    """Convert CPU cycles to microseconds."""
    return cycles / clock_mhz


def calculate_operation_time(op: Operation, platform: Platform) -> Tuple[int, float]:
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


def calculate_full_loop(platform: Platform, use_spi: bool = False) -> Dict:
    """
    Calculate full flight control loop timing.
    Returns breakdown of each phase.
    """
    phases = []
    total_cycles = 0
    total_us = 0.0

    # 1. IMU Read
    if use_spi:
        io_time = platform.spi_read_us
        imu_op = OPERATIONS["imu_read_spi"]
    else:
        io_time = platform.i2c_read_us
        imu_op = OPERATIONS["imu_read_i2c"]

    cycles, compute_us = calculate_operation_time(imu_op, platform)
    phase_us = io_time + compute_us
    phases.append(("IMU Read", cycles, io_time, compute_us, phase_us))
    total_cycles += cycles
    total_us += phase_us

    # 2. Calibration
    cycles, us = calculate_operation_time(OPERATIONS["calibration_apply"], platform)
    phases.append(("Calibration", cycles, 0, us, us))
    total_cycles += cycles
    total_us += us

    # 3. Low-pass filter
    cycles, us = calculate_operation_time(OPERATIONS["lowpass_filter"], platform)
    phases.append(("LP Filter", cycles, 0, us, us))
    total_cycles += cycles
    total_us += us

    # 4. Madgwick filter
    cycles, us = calculate_operation_time(OPERATIONS["madgwick_6dof"], platform)
    phases.append(("Madgwick 6DOF", cycles, 0, us, us))
    total_cycles += cycles
    total_us += us

    # 5. Radio processing
    cycles, us = calculate_operation_time(OPERATIONS["radio_process"], platform)
    phases.append(("Radio Process", cycles, 0, us, us))
    total_cycles += cycles
    total_us += us

    # 6. PID control
    cycles, us = calculate_operation_time(OPERATIONS["pid_3axis"], platform)
    phases.append(("PID (3-axis)", cycles, 0, us, us))
    total_cycles += cycles
    total_us += us

    # 7. Motor mixing
    cycles, us = calculate_operation_time(OPERATIONS["motor_mix_quad"], platform)
    phases.append(("Motor Mix", cycles, 0, us, us))
    total_cycles += cycles
    total_us += us

    # 8. Command scaling
    cycles, us = calculate_operation_time(OPERATIONS["command_scale"], platform)
    phases.append(("Cmd Scale", cycles, 0, us, us))
    total_cycles += cycles
    total_us += us

    # 9. PWM output (estimated)
    pwm_us = 10.0  # Approximate for DShot or fast PWM
    phases.append(("PWM Output", 0, pwm_us, 0, pwm_us))
    total_us += pwm_us

    return {
        "phases": phases,
        "total_cycles": total_cycles,
        "total_us": total_us,
    }


def calculate_max_loop_rate(total_us: float, overhead_percent: float = 20) -> float:
    """
    Calculate maximum achievable loop rate.
    overhead_percent: Reserve for interrupts, task switching, etc.
    """
    available_us = total_us * (1 + overhead_percent / 100)
    return 1_000_000 / available_us


def calculate_utilization(total_us: float, target_hz: float) -> float:
    """Calculate CPU utilization percentage for a target loop rate."""
    period_us = 1_000_000 / target_hz
    return (total_us / period_us) * 100


def check_custom_clock(clock_mhz: float, has_fpu: bool = True) -> Dict:
    """
    Check if a custom clock speed can handle the flight controller.
    Returns analysis results.
    """
    # Create a custom platform based on the clock speed
    if has_fpu:
        # Assume modern ARM Cortex-M4/M7 with FPU
        custom = Platform(
            name=f"Custom ({clock_mhz:.1f}MHz, FPU)",
            clock_mhz=clock_mhz,
            cores=1,
            fpu=True,
            flash_kb=256,
            ram_kb=64,
            cycles_per_float_mul=1,
            cycles_per_float_add=1,
            cycles_per_float_div=14,
            cycles_per_trig=25,
            cycles_per_sqrt=14,
            i2c_read_us=140,
            spi_read_us=20,
        )
    else:
        # Assume AVR-like without FPU
        custom = Platform(
            name=f"Custom ({clock_mhz:.1f}MHz, no FPU)",
            clock_mhz=clock_mhz,
            cores=1,
            fpu=False,
            flash_kb=32,
            ram_kb=2,
            cycles_per_float_mul=150,
            cycles_per_float_add=100,
            cycles_per_float_div=400,
            cycles_per_trig=1500,
            cycles_per_sqrt=300,
            i2c_read_us=350,
            spi_read_us=50,
        )

    result = calculate_full_loop(custom)
    max_rate = calculate_max_loop_rate(result["total_us"])

    targets = [250, 500, 1000, 2000]
    feasibility = {}
    for target in targets:
        util = calculate_utilization(result["total_us"], target)
        feasibility[target] = {
            "utilization": util,
            "feasible": util < 80,
            "marginal": 80 <= util < 100,
        }

    return {
        "platform": custom,
        "total_us": result["total_us"],
        "total_cycles": result["total_cycles"],
        "max_rate_hz": max_rate,
        "feasibility": feasibility,
    }


# =============================================================================
# WiFi API Latency Analysis
# =============================================================================

def calculate_wifi_latency() -> Dict:
    """
    Calculate WiFi API latency budget.
    Based on ESP32 typical values.
    """
    components = [
        ("WiFi round-trip (AP mode)", 8.0, 15.0),  # min, max ms
        ("TCP/HTTP overhead", 1.0, 3.0),
        ("JSON parsing", 0.5, 2.0),
        ("Command validation", 0.1, 0.5),
        ("FC loop wait (1kHz)", 0.0, 1.0),
        ("Response generation", 0.5, 1.5),
        ("WiFi TX", 2.0, 5.0),
    ]

    min_total = sum(c[1] for c in components)
    max_total = sum(c[2] for c in components)
    typical = (min_total + max_total) / 2

    return {
        "components": components,
        "min_ms": min_total,
        "max_ms": max_total,
        "typical_ms": typical,
    }


# =============================================================================
# Interactive Mode
# =============================================================================

def interactive_clock_check():
    """Interactive mode for checking clock speed feasibility."""
    print("\n" + "=" * 60)
    print("  FLIGHT CONTROLLER CLOCK SPEED CHECKER")
    print("=" * 60)
    print("""
Enter your microcontroller's clock speed to see if it can
handle the flight controller workload.

Clock speed format examples:
  - 16m  or 16M   = 16 MHz (Arduino Uno)
  - 600m or 600M  = 600 MHz (Teensy 4.0)
  - 240m          = 240 MHz (ESP32)
  - 1g   or 1G    = 1 GHz
  - 133k          = 133 kHz (unlikely but supported)
  - 168           = 168 MHz (assumed MHz if no suffix)

Type 'q' to quit.
""")

    while True:
        try:
            user_input = input("\nEnter clock speed (e.g., 600m): ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\n")
            break

        if user_input.lower() in ('q', 'quit', 'exit', ''):
            break

        clock_mhz = parse_clock_speed(user_input)
        if clock_mhz is None:
            print("  Invalid format. Use examples like: 16m, 600M, 1G, 133k")
            continue

        if clock_mhz < 1:
            print(f"  {clock_mhz * 1000:.0f} kHz = {clock_mhz:.3f} MHz - Too slow for any MCU")
            continue

        # Check with FPU (modern ARM)
        print(f"\n  Analyzing {clock_mhz:.1f} MHz...")
        print("-" * 60)

        # With FPU
        result_fpu = check_custom_clock(clock_mhz, has_fpu=True)
        print(f"\n  WITH hardware FPU (ARM Cortex-M4/M7, ESP32):")
        print(f"    Loop time: {result_fpu['total_us']:.1f} µs")
        print(f"    Max loop rate: {result_fpu['max_rate_hz']:,.0f} Hz")
        print(f"\n    Loop Rate Feasibility:")
        for target, info in result_fpu["feasibility"].items():
            util = info["utilization"]
            if info["feasible"]:
                status = f"OK ({util:.1f}% CPU)"
            elif info["marginal"]:
                status = f"MARGINAL ({util:.1f}% CPU)"
            else:
                status = f"NOT FEASIBLE ({util:.1f}% CPU)"
            print(f"      {target:>5} Hz: {status}")

        # Without FPU
        result_no_fpu = check_custom_clock(clock_mhz, has_fpu=False)
        print(f"\n  WITHOUT FPU (AVR, Cortex-M0, RP2040):")
        print(f"    Loop time: {result_no_fpu['total_us']:.1f} µs")
        print(f"    Max loop rate: {result_no_fpu['max_rate_hz']:,.0f} Hz")
        print(f"\n    Loop Rate Feasibility:")
        for target, info in result_no_fpu["feasibility"].items():
            util = info["utilization"]
            if info["feasible"]:
                status = f"OK ({util:.1f}% CPU)"
            elif info["marginal"]:
                status = f"MARGINAL ({util:.1f}% CPU)"
            else:
                status = f"NOT FEASIBLE ({util:.1f}% CPU)"
            print(f"      {target:>5} Hz: {status}")

        # Recommendation
        print("\n  VERDICT:")
        if result_fpu["feasibility"][500]["feasible"]:
            if result_fpu["feasibility"][1000]["feasible"]:
                print("    WITH FPU: Excellent for flight control (1kHz+)")
            else:
                print("    WITH FPU: Adequate for basic flight control (500Hz)")
        else:
            print("    WITH FPU: May struggle even at low rates")

        if result_no_fpu["feasibility"][500]["feasible"]:
            print("    WITHOUT FPU: Can work but not recommended")
        else:
            print("    WITHOUT FPU: NOT recommended for flight control")


# =============================================================================
# Reporting
# =============================================================================

def print_header(title: str):
    """Print a formatted header."""
    print("\n" + "=" * 70)
    print(f"  {title}")
    print("=" * 70)


def print_platform_comparison():
    """Compare all platforms."""
    print_header("PLATFORM SPECIFICATIONS")

    print(f"\n{'Platform':<35} {'Clock':<10} {'Cores':<6} {'FPU':<5} {'RAM':<8}")
    print("-" * 70)
    for key, p in PLATFORMS.items():
        print(f"{p.name:<35} {p.clock_mhz:>6.0f}MHz {p.cores:>5} {'Yes' if p.fpu else 'No':<5} {p.ram_kb:>6}KB")


def print_operation_analysis():
    """Print complexity analysis of each operation."""
    print_header("OPERATION COMPLEXITY ANALYSIS")

    print(f"\n{'Operation':<25} {'Muls':<6} {'Adds':<6} {'Divs':<6} {'Trig':<6} {'Sqrt':<6}")
    print("-" * 70)
    for key, op in OPERATIONS.items():
        print(f"{op.name:<25} {op.float_muls:<6} {op.float_adds:<6} "
              f"{op.float_divs:<6} {op.trig_ops:<6} {op.sqrt_ops:<6}")


def print_timing_analysis(platform_key: str, use_spi: bool = False):
    """Print detailed timing analysis for a platform."""
    platform = PLATFORMS[platform_key]
    result = calculate_full_loop(platform, use_spi)

    imu_type = "SPI" if use_spi else "I2C"
    print_header(f"TIMING ANALYSIS: {platform.name} ({imu_type})")

    print(f"\n{'Phase':<20} {'Cycles':<10} {'I/O (us)':<12} {'Compute (us)':<14} {'Total (us)':<12}")
    print("-" * 70)

    for phase in result["phases"]:
        name, cycles, io_us, compute_us, total_us = phase
        print(f"{name:<20} {cycles:<10} {io_us:<12.2f} {compute_us:<14.2f} {total_us:<12.2f}")

    print("-" * 70)
    print(f"{'TOTAL':<20} {result['total_cycles']:<10} {'':<12} {'':<14} {result['total_us']:<12.2f}")

    # Calculate achievable rates
    max_rate = calculate_max_loop_rate(result["total_us"])
    print(f"\nMax loop rate (20% overhead): {max_rate:,.0f} Hz")

    # Check against targets
    targets = [500, 1000, 2000, 4000, 8000]
    print(f"\nLoop Rate Feasibility:")
    for target in targets:
        utilization = calculate_utilization(result["total_us"], target)
        status = "OK" if utilization < 80 else "MARGINAL" if utilization < 100 else "NOT FEASIBLE"
        print(f"  {target:>5} Hz: {utilization:5.1f}% CPU utilization - {status}")


def print_clock_requirements():
    """Print minimum clock requirements for different loop rates."""
    print_header("MINIMUM CLOCK SPEED REQUIREMENTS")

    # Use ESP32 as baseline (since we're targeting it)
    platform = PLATFORMS["esp32"]
    result = calculate_full_loop(platform, use_spi=False)
    base_cycles = result["total_cycles"]

    targets = [250, 500, 1000, 2000, 4000]

    print(f"\n{'Target Rate':<15} {'Required Clock (FPU)':<25} {'Notes'}")
    print("-" * 70)

    for target in targets:
        # Calculate required clock for this rate
        period_us = 1_000_000 / target
        available_us = period_us * 0.8  # 80% utilization max
        required = base_cycles / available_us

        if target <= 500:
            notes = "Any modern MCU with FPU"
        elif target <= 1000:
            notes = "ESP32/STM32 OK"
        elif target <= 2000:
            notes = "Teensy 4.0 recommended"
        else:
            notes = "High-end only"
        print(f"{target:>5} Hz       {required:>8.1f} MHz                  {notes}")


def print_wifi_latency_analysis():
    """Print WiFi API latency breakdown."""
    print_header("WIFI API LATENCY ANALYSIS (ESP32)")

    result = calculate_wifi_latency()

    print(f"\n{'Component':<30} {'Min (ms)':<12} {'Max (ms)':<12}")
    print("-" * 55)

    for name, min_ms, max_ms in result["components"]:
        print(f"{name:<30} {min_ms:<12.1f} {max_ms:<12.1f}")

    print("-" * 55)
    print(f"{'TOTAL':<30} {result['min_ms']:<12.1f} {result['max_ms']:<12.1f}")
    print(f"\nTypical latency: {result['typical_ms']:.1f} ms")
    print(f"\nTarget: <50ms - {'ACHIEVABLE' if result['max_ms'] < 50 else 'AT RISK'}")
    print(f"Target: <100ms - {'ACHIEVABLE' if result['max_ms'] < 100 else 'AT RISK'}")


def print_dual_core_analysis():
    """Print ESP32 dual-core task allocation analysis."""
    print_header("ESP32 DUAL-CORE TASK ALLOCATION")

    platform = PLATFORMS["esp32"]
    fc_result = calculate_full_loop(platform, use_spi=False)

    print("\nCore 0 - Flight Control (Real-time)")
    print("-" * 40)
    print(f"  Loop budget @ 1kHz: 1000 us")
    print(f"  FC processing time: {fc_result['total_us']:.1f} us")
    print(f"  Utilization: {(fc_result['total_us'] / 1000) * 100:.1f}%")
    print(f"  Headroom: {1000 - fc_result['total_us']:.1f} us")

    print("\nCore 1 - Communications (Best-effort)")
    print("-" * 40)
    print("  Available: 100% (no FC constraints)")
    print("  Tasks:")
    print("    - WiFi stack: ~30% typical")
    print("    - HTTP server: ~10% per request")
    print("    - OLED update @ 10Hz: ~5%")
    print("    - WebSocket: ~5% per message")
    print("  Headroom: ~50% for bursts")

    print("\nConclusion: Dual-core provides strong isolation")
    print("  - FC can run at 1kHz with ~80% headroom")
    print("  - WiFi has dedicated core, no FC impact")


def print_summary_recommendations():
    """Print final recommendations."""
    print_header("RECOMMENDATIONS")

    print("""
Platform Selection:
  - Teensy 4.0: Best performance (2-8kHz), no WiFi
  - ESP32/S3:   Good performance (1-2kHz), built-in WiFi
  - STM32F405:  Betaflight compatible, no WiFi
  - Arduino Uno: NOT RECOMMENDED (too slow, no FPU)
  - RP2040:     Marginal (no FPU, dual-core helps)

For This Project (Research Drone + WiFi API):
  - Recommended: ESP32 or ESP32-S3
  - Target loop rate: 1kHz (sufficient, proven)
  - WiFi latency: <30ms typical (meets <50ms target)
  - Dual-core: FC on Core 0, WiFi on Core 1

Timing Budget Summary:
  - Full FC loop: ~150-200us on ESP32
  - 1kHz period: 1000us
  - Utilization: ~15-20%
  - Conclusion: Plenty of headroom for reliable operation
""")


def main():
    """Run all analyses and print results."""
    # Check for interactive mode
    if len(sys.argv) > 1 and sys.argv[1] in ('--check', '-c', '--interactive', '-i'):
        interactive_clock_check()
        return

    print("\n" + "#" * 70)
    print("#" + " " * 68 + "#")
    print("#" + "  FLIGHT CONTROLLER TIMING CALCULATOR".center(68) + "#")
    print("#" + "  Complexity Analysis & Clock Requirements".center(68) + "#")
    print("#" + " " * 68 + "#")
    print("#" * 70)

    # Platform comparison
    print_platform_comparison()

    # Operation complexity
    print_operation_analysis()

    # Detailed timing for each platform
    for platform_key in ["teensy40", "esp32", "stm32f405", "arduino_uno", "rp2040"]:
        print_timing_analysis(platform_key, use_spi=False)

    # ESP32 with SPI (for comparison)
    print_timing_analysis("esp32", use_spi=True)

    # Clock requirements
    print_clock_requirements()

    # WiFi latency
    print_wifi_latency_analysis()

    # Dual-core analysis
    print_dual_core_analysis()

    # Recommendations
    print_summary_recommendations()

    print("\n" + "=" * 70)
    print("  For interactive clock speed checking, run:")
    print("    python3 timing_calculator.py --check")
    print("=" * 70 + "\n")


if __name__ == "__main__":
    main()
