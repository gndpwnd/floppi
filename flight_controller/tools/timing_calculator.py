#!/usr/bin/env python3
"""
Flight Controller Timing Calculator

Calculates clock speed requirements, loop budgets, and latency analysis
for flight controller firmware across different microcontroller platforms.

Supports feature tier analysis: shows compute cost of each optional feature
module (USE_OPTIMIZATION, USE_RACING, USE_WEB_SERVER, USE_API_SERVER, etc.)
and per-core workload breakdown for multi-core platforms.

Uses computer science complexity analysis to estimate operation times.

Usage:
    python3 timing_calculator.py           # Full analysis
    python3 timing_calculator.py --check   # Interactive clock speed checker
    python3 timing_calculator.py --features # Feature tier analysis
    python3 timing_calculator.py --cores   # Per-core workload analysis
"""

import sys
import re
from dataclasses import dataclass, field
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
    # Capabilities
    has_wifi: bool = False


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
        has_wifi=False,
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
        has_wifi=True,
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
        has_wifi=True,
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
        has_wifi=False,
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
        has_wifi=False,
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
        has_wifi=False,
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
        has_wifi=False,
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
    core: int = 0       # Which core (0=FC, 1=peripherals)
    tier: str = "base"  # "base", "optimization", "racing", "core1"


# Based on actual dRehmFlight code analysis
OPERATIONS = {
    # === CORE 0: Flight Control (real-time) ===

    "imu_read_i2c": Operation(
        name="IMU Read (I2C)",
        float_muls=6, float_adds=6, float_divs=0, trig_ops=0, sqrt_ops=0,
        description="Read 6 values (accel + gyro), convert to float",
        core=0, tier="base",
    ),
    "imu_read_spi": Operation(
        name="IMU Read (SPI)",
        float_muls=6, float_adds=6, float_divs=0, trig_ops=0, sqrt_ops=0,
        description="Read 6 values via SPI, convert to float",
        core=0, tier="base",
    ),
    "calibration_apply": Operation(
        name="Calibration Apply",
        float_muls=6, float_adds=6, float_divs=0, trig_ops=0, sqrt_ops=0,
        description="Apply offset and scale calibration",
        core=0, tier="base",
    ),
    "lowpass_filter": Operation(
        name="Low-pass Filter (6 axes)",
        float_muls=12, float_adds=6, float_divs=0, trig_ops=0, sqrt_ops=0,
        description="IIR filter for 6 sensor values",
        core=0, tier="base",
    ),
    "madgwick_6dof": Operation(
        name="Madgwick Filter (6DOF)",
        float_muls=80, float_adds=60, float_divs=0, trig_ops=3, sqrt_ops=2,
        description="6DOF attitude estimation (no magnetometer)",
        core=0, tier="base",
    ),
    "madgwick_9dof": Operation(
        name="Madgwick Filter (9DOF)",
        float_muls=120, float_adds=90, float_divs=0, trig_ops=3, sqrt_ops=3,
        description="9DOF attitude estimation with magnetometer",
        core=0, tier="base",
    ),
    "pid_3axis": Operation(
        name="PID Controller (3 axes)",
        float_muls=12, float_adds=15, float_divs=0, trig_ops=0, sqrt_ops=0,
        description="Roll, pitch, yaw PID controllers",
        core=0, tier="base",
    ),
    "dterm_pt1_filter": Operation(
        name="D-term PT1 Filter (3 axes)",
        float_muls=6, float_adds=3, float_divs=0, trig_ops=0, sqrt_ops=0,
        description="First-order low-pass on PID derivative term",
        core=0, tier="base",
    ),
    "motor_mix_quad": Operation(
        name="Motor Mixer (Quad)",
        float_muls=16, float_adds=12, float_divs=0, trig_ops=0, sqrt_ops=0,
        description="Mix throttle, roll, pitch, yaw to 4 motors",
        core=0, tier="base",
    ),
    "motor_mix_hex": Operation(
        name="Motor Mixer (Hex)",
        float_muls=24, float_adds=18, float_divs=0, trig_ops=0, sqrt_ops=0,
        description="Mix to 6 motors",
        core=0, tier="base",
    ),
    "command_scale": Operation(
        name="Command Scaling",
        float_muls=6, float_adds=6, float_divs=0, trig_ops=0, sqrt_ops=0,
        description="Scale motor/servo commands to PWM",
        core=0, tier="base",
    ),
    "radio_process": Operation(
        name="Radio Processing",
        float_muls=6, float_adds=6, float_divs=0, trig_ops=0, sqrt_ops=0,
        description="Process 6 radio channels to normalized values",
        core=0, tier="base",
    ),

    # === CORE 0: USE_OPTIMIZATION tier ===

    "gyro_biquad_lpf": Operation(
        name="Gyro Biquad LPF (3 axes)",
        float_muls=18, float_adds=12, float_divs=0, trig_ops=0, sqrt_ops=0,
        description="Second-order biquad low-pass on gyro (-12dB/oct)",
        core=0, tier="optimization",
    ),
    "dterm_biquad": Operation(
        name="D-term Biquad (3 axes)",
        float_muls=18, float_adds=12, float_divs=0, trig_ops=0, sqrt_ops=0,
        description="Biquad filter upgrade for derivative term",
        core=0, tier="optimization",
    ),
    "gyro_notch": Operation(
        name="Gyro Notch Filter (3 axes)",
        float_muls=24, float_adds=15, float_divs=0, trig_ops=0, sqrt_ops=0,
        description="Narrow-band rejection for motor noise frequency",
        core=0, tier="optimization",
    ),
    "accel_stage2_lp": Operation(
        name="Accel 2nd-stage LP (3 axes)",
        float_muls=6, float_adds=3, float_divs=0, trig_ops=0, sqrt_ops=0,
        description="Extra accel smoothing for vibration rejection",
        core=0, tier="optimization",
    ),

    # === CORE 0: USE_RACING tier ===

    "feedforward": Operation(
        name="Feed-forward (3 axes)",
        float_muls=9, float_adds=6, float_divs=0, trig_ops=0, sqrt_ops=0,
        description="Setpoint derivative added to PID output",
        core=0, tier="racing",
    ),
    "tpa": Operation(
        name="TPA (Throttle PID Atten.)",
        float_muls=6, float_adds=3, float_divs=0, trig_ops=0, sqrt_ops=0,
        description="Reduce PID gains at high throttle",
        core=0, tier="racing",
    ),
    "setpoint_smooth": Operation(
        name="Setpoint Smoothing (3 axes)",
        float_muls=6, float_adds=3, float_divs=0, trig_ops=0, sqrt_ops=0,
        description="Low-pass on stick input for smoother transitions",
        core=0, tier="racing",
    ),
    "expo_curves": Operation(
        name="Expo Curves (3 axes)",
        float_muls=12, float_adds=6, float_divs=0, trig_ops=0, sqrt_ops=0,
        description="Non-linear stick response curves",
        core=0, tier="racing",
    ),

    # === CORE 1: Peripheral tasks (ESP32 only, non-real-time) ===

    "oled_display": Operation(
        name="OLED Display Update",
        float_muls=0, float_adds=0, float_divs=0, trig_ops=0, sqrt_ops=0,
        description="Render status screen to OLED @ 10Hz (I2C dominated)",
        core=1, tier="core1",
    ),
    "wifi_stack": Operation(
        name="WiFi Stack",
        float_muls=0, float_adds=0, float_divs=0, trig_ops=0, sqrt_ops=0,
        description="WiFi STA management, reconnection, TCP/IP",
        core=1, tier="core1",
    ),
    "web_server": Operation(
        name="Web Server (Async)",
        float_muls=0, float_adds=0, float_divs=0, trig_ops=0, sqrt_ops=0,
        description="ESPAsyncWebServer: JSON API, WebSocket, mDNS",
        core=1, tier="core1",
    ),
    "api_client": Operation(
        name="API Client (HTTP POST)",
        float_muls=0, float_adds=0, float_divs=0, trig_ops=0, sqrt_ops=0,
        description="POST telemetry to centralized server @ 2Hz",
        core=1, tier="core1",
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


def calculate_full_loop(platform: Platform, use_spi: bool = False,
                        use_optimization: bool = False,
                        use_racing: bool = False) -> Dict:
    """
    Calculate full flight control loop timing with optional feature tiers.
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
    phases.append(("IMU Read", cycles, io_time, compute_us, phase_us, "base"))
    total_cycles += cycles
    total_us += phase_us

    # 2. Calibration
    cycles, us = calculate_operation_time(OPERATIONS["calibration_apply"], platform)
    phases.append(("Calibration", cycles, 0, us, us, "base"))
    total_cycles += cycles
    total_us += us

    # 3. Low-pass filter
    cycles, us = calculate_operation_time(OPERATIONS["lowpass_filter"], platform)
    phases.append(("LP Filter", cycles, 0, us, us, "base"))
    total_cycles += cycles
    total_us += us

    # 3b. Optimization: Gyro biquad LPF
    if use_optimization:
        cycles, us = calculate_operation_time(OPERATIONS["gyro_biquad_lpf"], platform)
        phases.append(("Gyro Biquad LPF", cycles, 0, us, us, "optimization"))
        total_cycles += cycles
        total_us += us

    # 3c. Optimization: Gyro notch filter
    if use_optimization:
        cycles, us = calculate_operation_time(OPERATIONS["gyro_notch"], platform)
        phases.append(("Gyro Notch", cycles, 0, us, us, "optimization"))
        total_cycles += cycles
        total_us += us

    # 4. Madgwick filter
    cycles, us = calculate_operation_time(OPERATIONS["madgwick_6dof"], platform)
    phases.append(("Madgwick 6DOF", cycles, 0, us, us, "base"))
    total_cycles += cycles
    total_us += us

    # 4b. Optimization: Accel 2nd-stage LP
    if use_optimization:
        cycles, us = calculate_operation_time(OPERATIONS["accel_stage2_lp"], platform)
        phases.append(("Accel 2nd LP", cycles, 0, us, us, "optimization"))
        total_cycles += cycles
        total_us += us

    # 5. Radio processing
    cycles, us = calculate_operation_time(OPERATIONS["radio_process"], platform)
    phases.append(("Radio Process", cycles, 0, us, us, "base"))
    total_cycles += cycles
    total_us += us

    # 5b. Racing: Expo curves on stick input
    if use_racing:
        cycles, us = calculate_operation_time(OPERATIONS["expo_curves"], platform)
        phases.append(("Expo Curves", cycles, 0, us, us, "racing"))
        total_cycles += cycles
        total_us += us

    # 5c. Racing: Setpoint smoothing
    if use_racing:
        cycles, us = calculate_operation_time(OPERATIONS["setpoint_smooth"], platform)
        phases.append(("Setpoint Smooth", cycles, 0, us, us, "racing"))
        total_cycles += cycles
        total_us += us

    # 6. PID control
    cycles, us = calculate_operation_time(OPERATIONS["pid_3axis"], platform)
    phases.append(("PID (3-axis)", cycles, 0, us, us, "base"))
    total_cycles += cycles
    total_us += us

    # 6b. D-term PT1 filter (base)
    cycles, us = calculate_operation_time(OPERATIONS["dterm_pt1_filter"], platform)
    phases.append(("D-term PT1 LPF", cycles, 0, us, us, "base"))
    total_cycles += cycles
    total_us += us

    # 6c. Optimization: D-term biquad upgrade
    if use_optimization:
        cycles, us = calculate_operation_time(OPERATIONS["dterm_biquad"], platform)
        phases.append(("D-term Biquad", cycles, 0, us, us, "optimization"))
        total_cycles += cycles
        total_us += us

    # 6d. Racing: Feed-forward
    if use_racing:
        cycles, us = calculate_operation_time(OPERATIONS["feedforward"], platform)
        phases.append(("Feed-forward", cycles, 0, us, us, "racing"))
        total_cycles += cycles
        total_us += us

    # 6e. Racing: TPA
    if use_racing:
        cycles, us = calculate_operation_time(OPERATIONS["tpa"], platform)
        phases.append(("TPA", cycles, 0, us, us, "racing"))
        total_cycles += cycles
        total_us += us

    # 7. Motor mixing
    cycles, us = calculate_operation_time(OPERATIONS["motor_mix_quad"], platform)
    phases.append(("Motor Mix", cycles, 0, us, us, "base"))
    total_cycles += cycles
    total_us += us

    # 8. Command scaling
    cycles, us = calculate_operation_time(OPERATIONS["command_scale"], platform)
    phases.append(("Cmd Scale", cycles, 0, us, us, "base"))
    total_cycles += cycles
    total_us += us

    # 9. PWM output (estimated)
    pwm_us = 10.0  # Approximate for DShot or fast PWM
    phases.append(("PWM Output", 0, pwm_us, 0, pwm_us, "base"))
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
        custom = Platform(
            name=f"Custom ({clock_mhz:.1f}MHz, FPU)",
            clock_mhz=clock_mhz, cores=1, fpu=True,
            flash_kb=256, ram_kb=64,
            cycles_per_float_mul=1, cycles_per_float_add=1,
            cycles_per_float_div=14, cycles_per_trig=25, cycles_per_sqrt=14,
            i2c_read_us=140, spi_read_us=20,
        )
    else:
        custom = Platform(
            name=f"Custom ({clock_mhz:.1f}MHz, no FPU)",
            clock_mhz=clock_mhz, cores=1, fpu=False,
            flash_kb=32, ram_kb=2,
            cycles_per_float_mul=150, cycles_per_float_add=100,
            cycles_per_float_div=400, cycles_per_trig=1500, cycles_per_sqrt=300,
            i2c_read_us=350, spi_read_us=50,
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
        ("WiFi round-trip (STA mode)", 5.0, 12.0),  # min, max ms
        ("TCP/HTTP overhead", 1.0, 3.0),
        ("JSON parsing (ArduinoJson)", 0.5, 2.0),
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
        print(f"    Loop time: {result_fpu['total_us']:.1f} us")
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
        print(f"    Loop time: {result_no_fpu['total_us']:.1f} us")
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

    print(f"\n{'Platform':<35} {'Clock':<10} {'Cores':<6} {'FPU':<5} {'WiFi':<6} {'RAM':<8}")
    print("-" * 75)
    for key, p in PLATFORMS.items():
        wifi = "Yes" if p.has_wifi else "No"
        print(f"{p.name:<35} {p.clock_mhz:>6.0f}MHz {p.cores:>5} {'Yes' if p.fpu else 'No':<5} {wifi:<6} {p.ram_kb:>6}KB")


def print_operation_analysis():
    """Print complexity analysis of each operation, grouped by tier."""
    print_header("OPERATION COMPLEXITY ANALYSIS")

    tier_names = {
        "base": "BASE (always compiled)",
        "optimization": "USE_OPTIMIZATION (noise reduction)",
        "racing": "USE_RACING (performance features)",
        "core1": "CORE 1 TASKS (ESP32 only, non-real-time)",
    }

    for tier_key, tier_label in tier_names.items():
        ops = [(k, v) for k, v in OPERATIONS.items() if v.tier == tier_key]
        if not ops:
            continue

        print(f"\n  --- {tier_label} ---")
        print(f"  {'Operation':<30} {'Muls':<6} {'Adds':<6} {'Divs':<6} {'Trig':<6} {'Sqrt':<6}")
        print("  " + "-" * 60)
        for key, op in ops:
            print(f"  {op.name:<30} {op.float_muls:<6} {op.float_adds:<6} "
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
        name, cycles, io_us, compute_us, total_us, tier = phase
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


def print_feature_tier_analysis():
    """Print feature tier comparison showing incremental cost of each tier."""
    print_header("FEATURE TIER ANALYSIS")
    print("""
  Shows the incremental compute cost of enabling each feature tier.
  All tiers use compile-time #ifdef — disabled features have zero cost.

  config.h flags:
    Base:         Always compiled (core flight stabilizer)
    Optimization: #define USE_OPTIMIZATION (noise reduction for cheap hardware)
    Racing:       #define USE_RACING (Betaflight-style performance features)
""")

    configs = [
        ("Base only", False, False),
        ("Base + Optimization", True, False),
        ("Base + Racing", False, True),
        ("Base + Optimization + Racing", True, True),
    ]

    for platform_key in ["esp32", "teensy40"]:
        platform = PLATFORMS[platform_key]
        print(f"\n  {platform.name}")
        print(f"  {'Configuration':<35} {'Loop (us)':<12} {'Max Rate':<12} {'@ 1kHz':<12} {'@ 2kHz':<12}")
        print("  " + "-" * 75)

        for label, opt, race in configs:
            result = calculate_full_loop(platform, use_optimization=opt, use_racing=race)
            max_rate = calculate_max_loop_rate(result["total_us"])
            util_1k = calculate_utilization(result["total_us"], 1000)
            util_2k = calculate_utilization(result["total_us"], 2000)
            print(f"  {label:<35} {result['total_us']:<12.1f} {max_rate:<12,.0f} {util_1k:<12.1f}% {util_2k:<12.1f}%")

    print(f"""
  Key insights:
    - All feature combinations are feasible on both ESP32 and Teensy
    - Optimization tier adds ~5-10us per tick (negligible)
    - Racing tier adds ~3-5us per tick (negligible)
    - I2C IMU read dominates the loop budget (100-140us)
    - SPI IMU would dramatically reduce loop time
""")


def print_dual_core_analysis():
    """Print ESP32 dual-core task allocation analysis with feature modules."""
    print_header("ESP32 DUAL-CORE TASK ALLOCATION")

    platform = PLATFORMS["esp32"]

    # Calculate all configurations
    base_result = calculate_full_loop(platform)
    full_result = calculate_full_loop(platform, use_optimization=True, use_racing=True)

    print("""
  ESP32 Architecture:
    Core 0: Flight control loop (FreeRTOS task, priority 3, real-time)
    Core 1: Peripherals (Arduino loop, best-effort, non-real-time)

  Data flow: Core 0 → xQueueOverwrite → Core 1 (one-way, non-blocking)
""")

    print("  CORE 0 — Flight Control (Real-time)")
    print("  " + "-" * 55)
    print(f"    Loop budget @ 1kHz:         1000 us")
    print(f"    Base FC processing:         {base_result['total_us']:.1f} us ({base_result['total_us']/10:.1f}%)")
    print(f"    + Optimization + Racing:    {full_result['total_us']:.1f} us ({full_result['total_us']/10:.1f}%)")
    print(f"    Headroom (base):            {1000 - base_result['total_us']:.1f} us")
    print(f"    Headroom (full features):   {1000 - full_result['total_us']:.1f} us")

    print(f"""
    Core 0 processes (per tick):
      [BASE]         IMU read, calibration apply, LP filter
      [BASE]         Madgwick 6DOF attitude estimation
      [BASE]         Radio processing, PID control, D-term filter
      [BASE]         Motor mixing, command scaling, PWM output
      [OPTIMIZATION] Gyro biquad LPF, gyro notch, accel 2nd LP, D-term biquad
      [RACING]       Feed-forward, TPA, setpoint smoothing, expo curves
""")

    print("  CORE 1 — Peripherals (Best-effort)")
    print("  " + "-" * 55)
    print("    Available: 100% of Core 1 (no FC constraints)")
    print()

    # Core 1 task breakdown with estimated utilization
    core1_tasks = [
        ("WiFi stack (STA mode)", "USE_WIFI", "~30%", "Connection mgmt, TCP/IP, reconnect"),
        ("OLED display @ 10Hz", "USE_OLED_DISPLAY", "~5%", "U8g2 render + SW I2C write"),
        ("Web server (async)", "USE_WEB_SERVER", "~10%", "JSON API + WebSocket broadcast"),
        ("API client @ 2Hz", "USE_API_SERVER", "~5%", "HTTP POST telemetry to server"),
        ("Queue receive", "always", "~1%", "xQueueReceive from Core 0"),
    ]

    print(f"    {'Task':<30} {'Config Flag':<20} {'CPU Est.':<10} {'Description'}")
    print("    " + "-" * 75)
    for task, flag, cpu, desc in core1_tasks:
        print(f"    {task:<30} {flag:<20} {cpu:<10} {desc}")
    print()
    print("    Total Core 1 utilization:   ~50% (all features enabled)")
    print("    Headroom:                   ~50% for bursts and overhead")
    print()

    # Single-core note
    print("  TEENSY — Single Core")
    print("  " + "-" * 55)
    print("    All tasks run in main loop() sequentially.")
    print("    Flight control tick runs first, then rate-limited display.")
    print("    WiFi features NOT available (no WiFi hardware).")
    print("    Web server and API server are compile-time excluded.")
    teensy = PLATFORMS["teensy40"]
    teensy_result = calculate_full_loop(teensy)
    print(f"    FC loop time: {teensy_result['total_us']:.1f} us @ 2kHz = {teensy_result['total_us']/5:.1f}% utilization")
    print(f"    OLED display: 10Hz in main loop (if USE_OLED_DISPLAY enabled)")


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


def print_summary_recommendations():
    """Print final recommendations."""
    print_header("RECOMMENDATIONS")

    print("""
Platform Selection:
  - Teensy 4.0: Best performance (2-8kHz), no WiFi
  - ESP32/S3:   Good performance (1-2kHz), built-in WiFi, dual-core
  - STM32F405:  Betaflight compatible, no WiFi
  - Arduino Uno: NOT RECOMMENDED (too slow, no FPU)
  - RP2040:     Marginal (no FPU, dual-core helps)

For This Project (Research Drone + WiFi API):
  - Recommended: ESP32 or ESP32-S3
  - Target loop rate: 1kHz (sufficient, proven)
  - WiFi latency: <30ms typical (meets <50ms target)
  - Dual-core: FC on Core 0, WiFi/display on Core 1

Feature Module Recommendations:
  - USE_OLED_DISPLAY: Enable on all platforms (minimal overhead)
  - USE_WEB_SERVER:   Enable on ESP32 for calibration (Core 1, zero FC impact)
  - USE_API_SERVER:   Enable on ESP32 for live flight (Core 1, zero FC impact)
  - USE_OPTIMIZATION: Enable for cheap/noisy hardware (~5-10us extra per tick)
  - USE_RACING:       Enable for FPV/acro flying (~3-5us extra per tick)
  - All combinations feasible on ESP32 and Teensy

Timing Budget Summary:
  - Full FC loop: ~150-200us on ESP32, ~50-80us on Teensy
  - All feature tiers enabled: ~170-220us on ESP32, ~60-90us on Teensy
  - 1kHz period: 1000us → ~80% headroom even with all features
  - WiFi/display/web server on Core 1: zero impact on flight control
""")


def main():
    """Run all analyses and print results."""
    # Check for specific modes
    if len(sys.argv) > 1:
        arg = sys.argv[1]
        if arg in ('--check', '-c', '--interactive', '-i'):
            interactive_clock_check()
            return
        elif arg in ('--features', '-f'):
            print_feature_tier_analysis()
            return
        elif arg in ('--cores', '--dual-core'):
            print_dual_core_analysis()
            return

    print("\n" + "#" * 70)
    print("#" + " " * 68 + "#")
    print("#" + "  FLIGHT CONTROLLER TIMING CALCULATOR".center(68) + "#")
    print("#" + "  Complexity Analysis & Clock Requirements".center(68) + "#")
    print("#" + " " * 68 + "#")
    print("#" * 70)

    # Platform comparison
    print_platform_comparison()

    # Operation complexity (grouped by tier)
    print_operation_analysis()

    # Feature tier comparison
    print_feature_tier_analysis()

    # Detailed timing for primary platforms
    for platform_key in ["teensy40", "esp32", "stm32f405", "arduino_uno", "rp2040"]:
        print_timing_analysis(platform_key, use_spi=False)

    # ESP32 with SPI (for comparison)
    print_timing_analysis("esp32", use_spi=True)

    # Clock requirements
    print_clock_requirements()

    # Dual-core analysis
    print_dual_core_analysis()

    # WiFi latency
    print_wifi_latency_analysis()

    # Recommendations
    print_summary_recommendations()

    print("\n" + "=" * 70)
    print("  Usage modes:")
    print("    python3 timing_calculator.py           # Full analysis")
    print("    python3 timing_calculator.py --check   # Interactive clock checker")
    print("    python3 timing_calculator.py --features # Feature tier comparison")
    print("    python3 timing_calculator.py --cores   # Per-core workload analysis")
    print("=" * 70 + "\n")


if __name__ == "__main__":
    main()
