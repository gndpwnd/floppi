"""Platform specifications for supported microcontrollers."""

from dataclasses import dataclass


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
        fpu=False,  # No hardware FPU
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
