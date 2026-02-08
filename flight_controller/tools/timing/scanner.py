"""Parse config.h to detect enabled features for timing analysis."""

import os
import re


# Feature flags we look for in config.h
FEATURE_FLAGS = [
    "USE_OPTIMIZATION",
    "USE_RACING",
    "USE_MPU6050",
    "USE_MPU9250",
    "USE_ANGLE_CONTROLLER",
    "USE_RATE_CONTROLLER",
    "USE_WEB_SERVER",
    "USE_API_SERVER",
]

DISPLAY_FLAGS = [
    "DISPLAY_SSD1306_128X32",
    "DISPLAY_SSD1306_128X64",
    "DISPLAY_SH1106_128X64",
]

# Platforms that have USE_ESP32 + USE_WIFI set via platformio.ini
ESP32_PLATFORMS = {"esp32", "esp32s3", "esp32_calibration", "esp32s3_calibration"}

# Regex patterns
RE_ACTIVE_DEFINE = re.compile(r'^\s*#define\s+(\w+)')
RE_COMMENTED_DEFINE = re.compile(r'^\s*//\s*#define\s+(\w+)')
RE_DEFINE_VALUE = re.compile(r'^\s*#define\s+(\w+)\s+(\d+)')
RE_IFDEF_ESP32_WIFI = re.compile(
    r'^\s*#if\s+defined\s*\(\s*USE_ESP32\s*\)\s*&&\s*defined\s*\(\s*USE_WIFI\s*\)'
)
RE_ENDIF = re.compile(r'^\s*#endif')


def scan_config(config_path, platform_key="esp32"):
    """
    Parse config.h to detect which features are enabled.

    Args:
        config_path: Path to config.h
        platform_key: Platform name (affects conditional blocks like USE_WEB_SERVER)

    Returns dict with feature flags and settings.
    """
    if not os.path.exists(config_path):
        raise FileNotFoundError(f"config.h not found: {config_path}")

    is_esp32 = platform_key in ESP32_PLATFORMS

    # Track which flags are active vs commented
    active = set()
    commented = set()
    values = {}

    # Simple conditional block tracking for the ESP32+WiFi guard
    in_esp32_wifi_block = False
    block_depth = 0

    with open(config_path, 'r') as f:
        for line in f:
            # Track ESP32+WiFi conditional block
            if RE_IFDEF_ESP32_WIFI.match(line):
                in_esp32_wifi_block = True
                block_depth = 1
                continue
            if in_esp32_wifi_block:
                if line.strip().startswith('#if'):
                    block_depth += 1
                elif RE_ENDIF.match(line):
                    block_depth -= 1
                    if block_depth == 0:
                        in_esp32_wifi_block = False
                        continue

            # Check for commented-out defines first
            m = RE_COMMENTED_DEFINE.match(line)
            if m:
                flag = m.group(1)
                if flag in FEATURE_FLAGS or flag in DISPLAY_FLAGS:
                    commented.add(flag)
                continue

            # Check for active defines (with optional value)
            m_val = RE_DEFINE_VALUE.match(line)
            if m_val:
                flag = m_val.group(1)
                val = int(m_val.group(2))
                values[flag] = val
                if flag in FEATURE_FLAGS or flag in DISPLAY_FLAGS:
                    # Inside ESP32+WiFi block: only active if platform is ESP32
                    if in_esp32_wifi_block:
                        if is_esp32:
                            active.add(flag)
                        else:
                            commented.add(flag)
                    else:
                        active.add(flag)
                continue

            m = RE_ACTIVE_DEFINE.match(line)
            if m:
                flag = m.group(1)
                if flag in FEATURE_FLAGS or flag in DISPLAY_FLAGS:
                    if in_esp32_wifi_block:
                        if is_esp32:
                            active.add(flag)
                        else:
                            commented.add(flag)
                    else:
                        active.add(flag)

    # Derive high-level features
    use_spi = "USE_MPU9250" in active
    imu = "MPU9250" if use_spi else "MPU6050"
    display_enabled = any(d in active for d in DISPLAY_FLAGS)

    # Loop frequency: use platform defaults.
    # config.h has LOOP_FREQUENCY_HZ inside #ifdef USE_ESP32 / #else blocks,
    # so the raw value isn't meaningful without full preprocessor evaluation.
    # Platform defaults: ESP32 = 1000 Hz, Teensy = 2000 Hz.
    if is_esp32:
        loop_hz = 1000
    else:
        loop_hz = 2000

    return {
        "use_optimization": "USE_OPTIMIZATION" in active,
        "use_racing": "USE_RACING" in active,
        "use_spi": use_spi,
        "use_web_server": "USE_WEB_SERVER" in active,
        "use_api_server": "USE_API_SERVER" in active,
        "display_enabled": display_enabled,
        "loop_frequency_hz": loop_hz,
        "imu": imu,
        "is_esp32": is_esp32,
    }


def find_config_path(script_dir=None):
    """Auto-detect config.h path relative to tools/ directory."""
    if script_dir is None:
        script_dir = os.path.dirname(os.path.abspath(__file__))
    # tools/timing/ -> tools/ -> flight_controller/
    base = os.path.dirname(os.path.dirname(script_dir))
    config_path = os.path.join(base, "include", "config.h")
    if os.path.exists(config_path):
        return config_path
    return None
