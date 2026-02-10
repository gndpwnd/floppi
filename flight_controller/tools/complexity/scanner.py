"""Parse config.h to detect enabled features and extract parameters."""

import os
import re

# Feature flags we look for
FEATURE_FLAGS = [
    "USE_OPTIMIZATION", "USE_RACING",
    "USE_MPU6050", "USE_MPU9250",
    "USE_ANGLE_CONTROLLER", "USE_RATE_CONTROLLER",
    "USE_WEB_SERVER", "USE_API_SERVER", "USE_OTA",
]
DISPLAY_FLAGS = [
    "DISPLAY_SSD1306_128X32", "DISPLAY_SSD1306_128X64", "DISPLAY_SH1106_128X64",
]
ESP32_PLATFORMS = {"esp32", "esp32s3", "esp32_calibration", "esp32s3_calibration"}

# Regex patterns
RE_ACTIVE_DEFINE = re.compile(r'^\s*#define\s+(\w+)')
RE_COMMENTED_DEFINE = re.compile(r'^\s*//\s*#define\s+(\w+)')
RE_DEFINE_INT = re.compile(r'^\s*#define\s+(\w+)\s+(\d+)')
RE_DEFINE_FLOAT = re.compile(r'^\s*#define\s+(\w+)\s+([\d.]+)f?\b')
RE_IFDEF_ESP32_WIFI = re.compile(
    r'^\s*#if\s+defined\s*\(\s*USE_ESP32\s*\)\s*&&\s*defined\s*\(\s*USE_WIFI\s*\)'
)
RE_ENDIF = re.compile(r'^\s*#endif')

# Numeric parameters to extract from config.h
NUMERIC_PARAMS = [
    "LOOP_FREQUENCY_HZ",
    "MADGWICK_BETA", "B_ACCEL", "B_GYRO", "B_DTERM",
    "GYRO_LPF_CUTOFF_HZ", "DTERM_LPF_CUTOFF_HZ",
    "GYRO_NOTCH_CENTER_HZ", "GYRO_NOTCH_WIDTH_HZ",
    "KP_ROLL_RATE", "KI_ROLL_RATE", "KD_ROLL_RATE",
    "KP_PITCH_RATE", "KI_PITCH_RATE", "KD_PITCH_RATE",
    "KP_YAW_RATE", "KI_YAW_RATE", "KD_YAW_RATE",
    "MAX_ROLL_RATE", "MAX_PITCH_RATE", "MAX_YAW_RATE",
    "MAX_ROLL_ANGLE", "MAX_PITCH_ANGLE",
]


def scan_config(config_path, platform_key="esp32"):
    """Parse config.h for features and numeric parameters."""
    if not os.path.exists(config_path):
        raise FileNotFoundError(f"config.h not found: {config_path}")

    is_esp32 = platform_key in ESP32_PLATFORMS
    active = set()
    commented = set()
    params = {}

    in_esp32_wifi_block = False
    block_depth = 0

    # Track #ifdef USE_ESP32 / #else to resolve platform-conditional defines
    esp32_ifdef_depth = 0
    in_esp32_else = False

    with open(config_path, 'r') as f:
        for line in f:
            stripped = line.strip()

            if RE_IFDEF_ESP32_WIFI.match(line):
                in_esp32_wifi_block = True
                block_depth = 1
                continue
            if in_esp32_wifi_block:
                if stripped.startswith('#if'):
                    block_depth += 1
                elif RE_ENDIF.match(line):
                    block_depth -= 1
                    if block_depth == 0:
                        in_esp32_wifi_block = False
                        continue

            # Track #ifdef USE_ESP32 blocks (for LOOP_FREQUENCY_HZ etc.)
            if re.match(r'^\s*#ifdef\s+USE_ESP32\b', line) and esp32_ifdef_depth == 0:
                esp32_ifdef_depth = 1
                in_esp32_else = False
                continue
            if esp32_ifdef_depth > 0:
                if stripped.startswith('#if'):
                    esp32_ifdef_depth += 1
                elif stripped.startswith('#else') and esp32_ifdef_depth == 1:
                    in_esp32_else = True
                    continue
                elif RE_ENDIF.match(line):
                    esp32_ifdef_depth -= 1
                    if esp32_ifdef_depth == 0:
                        in_esp32_else = False
                        continue

            # Skip defines in wrong platform branch
            in_wrong_branch = (
                (esp32_ifdef_depth > 0 and not in_esp32_else and not is_esp32) or
                (esp32_ifdef_depth > 0 and in_esp32_else and is_esp32)
            )

            # Commented-out defines
            m = RE_COMMENTED_DEFINE.match(line)
            if m:
                flag = m.group(1)
                if flag in FEATURE_FLAGS or flag in DISPLAY_FLAGS:
                    commented.add(flag)
                continue

            if in_wrong_branch:
                continue

            # Extract numeric values (int or float)
            m_float = RE_DEFINE_FLOAT.match(line)
            m_int = RE_DEFINE_INT.match(line)
            if m_float:
                name, val = m_float.group(1), m_float.group(2)
                if name in NUMERIC_PARAMS:
                    params[name] = float(val)
            if m_int:
                name, val = m_int.group(1), int(m_int.group(2))
                if name in NUMERIC_PARAMS:
                    params[name] = val

            # Active defines
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

    # Derive features
    use_spi = "USE_MPU9250" in active
    display_enabled = any(d in active for d in DISPLAY_FLAGS)

    # Loop frequency from config or platform default
    loop_hz = params.get("LOOP_FREQUENCY_HZ", 1000 if is_esp32 else 2000)

    return {
        "use_optimization": "USE_OPTIMIZATION" in active,
        "use_racing": "USE_RACING" in active,
        "use_spi": use_spi,
        "use_web_server": "USE_WEB_SERVER" in active,
        "use_api_server": "USE_API_SERVER" in active,
        "use_ota": "USE_OTA" in active,
        "display_enabled": display_enabled,
        "loop_frequency_hz": int(loop_hz),
        "imu": "MPU9250" if use_spi else "MPU6050",
        "is_esp32": is_esp32,
        "params": params,
    }


def find_config_path():
    """Auto-detect config.h relative to tools/ directory."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    # tools/complexity/ -> tools/ -> flight_controller/
    base = os.path.dirname(os.path.dirname(script_dir))
    config_path = os.path.join(base, "include", "config.h")
    if os.path.exists(config_path):
        return config_path
    return None


def find_project_root():
    """Auto-detect flight_controller/ root directory."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    return os.path.dirname(os.path.dirname(script_dir))
