"""Platform specifications and FPU cycle cost profiles."""

from dataclasses import dataclass


@dataclass
class Platform:
    """Hardware specs that affect timing. Only what matters: clock, cores, FPU."""
    name: str
    clock_mhz: float
    cores: int
    fpu: bool
    i2c_read_us: float = 140.0  # Time to read 14 bytes (bus-speed dependent)
    spi_read_us: float = 20.0


# Cycle costs per float operation — FPU vs software float.
# These are conservative averages across common architectures.
FPU_CYCLES = {
    "mul": 1,      # Single-cycle on most FPUs (ARM Cortex-M4/M7, Xtensa)
    "add": 1,
    "div": 14,     # Multi-cycle even with FPU
    "trig": 25,    # atan2/asin/cos — library call even with FPU
    "sqrt": 14,
}

SOFT_FLOAT_CYCLES = {
    "mul": 100,    # Software emulation — varies by arch (AVR ~150, ARM-M0 ~50)
    "add": 70,
    "div": 300,
    "trig": 1000,  # Very slow without FPU
    "sqrt": 200,
}


def get_cycle_costs(fpu):
    """Return cycle cost dict for FPU or software float."""
    return FPU_CYCLES if fpu else SOFT_FLOAT_CYCLES


# Preset platforms (convenience shortcuts)
PLATFORMS = {
    "teensy40": Platform(
        name="Teensy 4.0 (Cortex-M7)",
        clock_mhz=600, cores=1, fpu=True,
        i2c_read_us=100, spi_read_us=15,
    ),
    "esp32": Platform(
        name="ESP32 (Xtensa LX6)",
        clock_mhz=240, cores=2, fpu=True,
        i2c_read_us=140, spi_read_us=20,
    ),
    "esp32s3": Platform(
        name="ESP32-S3 (Xtensa LX7)",
        clock_mhz=240, cores=2, fpu=True,
        i2c_read_us=140, spi_read_us=18,
    ),
    "stm32f405": Platform(
        name="STM32F405 (Cortex-M4)",
        clock_mhz=168, cores=1, fpu=True,
        i2c_read_us=120, spi_read_us=18,
    ),
    "arduino_uno": Platform(
        name="Arduino Uno (ATmega328P)",
        clock_mhz=16, cores=1, fpu=False,
        i2c_read_us=350, spi_read_us=50,
    ),
    "arduino_mega": Platform(
        name="Arduino Mega (ATmega2560)",
        clock_mhz=16, cores=1, fpu=False,
        i2c_read_us=350, spi_read_us=50,
    ),
    "rp2040": Platform(
        name="RP2040 (Cortex-M0+)",
        clock_mhz=133, cores=2, fpu=False,
        i2c_read_us=140, spi_read_us=20,
    ),
}
