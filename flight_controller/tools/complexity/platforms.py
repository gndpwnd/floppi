"""Platform hardware specifications and cycle cost profiles."""

from dataclasses import dataclass


@dataclass
class Platform:
    """Hardware platform for resource analysis."""
    name: str
    clock_mhz: float
    cores: int
    fpu: bool
    flash_kb: int       # Total flash in KB
    ram_kb: int          # Total usable RAM in KB
    i2c_read_us: float = 140.0   # I2C bus read time (6 registers)
    spi_read_us: float = 20.0    # SPI bus read time (6 registers)


# FPU cycle costs per architecture family
# Source: ARM Cortex-M7 TRM, Xtensa ISA summary, ESP32 forum measurements
CYCLE_PROFILES = {
    "cortex_m7": {
        "mul": 1, "add": 1, "div": 14, "trig": 25, "sqrt": 14,
    },
    "xtensa_lx6": {
        "mul": 2, "add": 2, "div": 25, "trig": 150, "sqrt": 25,
    },
    "xtensa_lx7": {
        "mul": 2, "add": 2, "div": 25, "trig": 150, "sqrt": 25,
    },
    "cortex_m4": {
        "mul": 1, "add": 1, "div": 14, "trig": 40, "sqrt": 14,
    },
    "soft_float": {
        "mul": 100, "add": 70, "div": 300, "trig": 1000, "sqrt": 200,
    },
}

# Platform presets with architecture mapping
PLATFORMS = {
    "teensy40": Platform(
        name="Teensy 4.0 (Cortex-M7 @ 600MHz)",
        clock_mhz=600, cores=1, fpu=True,
        flash_kb=2048, ram_kb=1024,
        i2c_read_us=100.0, spi_read_us=15.0,
    ),
    "teensy41": Platform(
        name="Teensy 4.1 (Cortex-M7 @ 600MHz)",
        clock_mhz=600, cores=1, fpu=True,
        flash_kb=8192, ram_kb=1024,
        i2c_read_us=100.0, spi_read_us=15.0,
    ),
    "teensy36": Platform(
        name="Teensy 3.6 (Cortex-M4 @ 180MHz)",
        clock_mhz=180, cores=1, fpu=True,
        flash_kb=1024, ram_kb=256,
        i2c_read_us=120.0, spi_read_us=18.0,
    ),
    "esp32": Platform(
        name="ESP32 (Xtensa LX6 @ 240MHz, dual-core)",
        clock_mhz=240, cores=2, fpu=True,
        flash_kb=4096, ram_kb=320,
        i2c_read_us=140.0, spi_read_us=20.0,
    ),
    "esp32s3": Platform(
        name="ESP32-S3 (Xtensa LX7 @ 240MHz, dual-core)",
        clock_mhz=240, cores=2, fpu=True,
        flash_kb=8192, ram_kb=512,
        i2c_read_us=140.0, spi_read_us=20.0,
    ),
}

# Map platform keys to architecture profiles
PLATFORM_ARCH = {
    "teensy40": "cortex_m7",
    "teensy41": "cortex_m7",
    "teensy36": "cortex_m4",
    "esp32": "xtensa_lx6",
    "esp32s3": "xtensa_lx7",
}


# Map platform keys to their primary build environment
PLATFORM_BUILDS = {
    "teensy40": "teensy40",
    "teensy41": "teensy41",
    "teensy36": "teensy36",
    "esp32": "esp32",
    "esp32s3": "esp32s3",
}


def get_cycles(platform_key, fpu=True):
    """Get cycle cost profile for a platform."""
    if not fpu:
        return CYCLE_PROFILES["soft_float"]
    arch = PLATFORM_ARCH.get(platform_key, "cortex_m7")
    return CYCLE_PROFILES.get(arch, CYCLE_PROFILES["cortex_m7"])
