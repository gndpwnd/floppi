"""
Analyze PlatformIO build artifacts for actual resource usage.

Parses ELF files via toolchain size commands, extracts flash/RAM usage
from build output. Requires a recent `pio run` for the target environment.
"""

import os
import re
import subprocess
import glob

# Toolchain paths (PlatformIO standard locations)
TOOLCHAIN_PATHS = {
    "teensy": [
        os.path.expanduser("~/.platformio/packages/toolchain-gccarmnoneeabi-teensy/bin/arm-none-eabi-size"),
        os.path.expanduser("~/.platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-size"),
        "arm-none-eabi-size",
    ],
    "esp32": [
        os.path.expanduser("~/.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-size"),
        "xtensa-esp32-elf-size",
    ],
    "esp32s3": [
        os.path.expanduser("~/.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-size"),
        "xtensa-esp32s3-elf-size",
    ],
}

# Platform memory limits for percentage calculations
MEMORY_LIMITS = {
    "teensy40": {"flash": 2048 * 1024, "ram": 1024 * 1024},
    "teensy41": {"flash": 8192 * 1024, "ram": 1024 * 1024},
    "teensy36": {"flash": 1024 * 1024, "ram": 256 * 1024},
    "esp32":    {"flash": 3200 * 1024, "ram": 320 * 1024},   # ~3.2MB app partition
    "esp32s3":  {"flash": 3200 * 1024, "ram": 512 * 1024},
}


def _find_size_tool(env_name):
    """Find the correct size tool for an environment."""
    if "esp32s3" in env_name:
        candidates = TOOLCHAIN_PATHS["esp32s3"]
    elif "esp32" in env_name:
        candidates = TOOLCHAIN_PATHS["esp32"]
    else:
        candidates = TOOLCHAIN_PATHS["teensy"]

    for path in candidates:
        if os.path.exists(path):
            return path
    return None


def _find_elf(project_root, env_name):
    """Find the ELF file for a build environment."""
    elf_path = os.path.join(project_root, ".pio", "build", env_name, "firmware.elf")
    if os.path.exists(elf_path):
        return elf_path
    return None


def _base_env(env_name):
    """Strip _calibration suffix for memory limit lookup."""
    return env_name.replace("_calibration", "")


def parse_size_berkeley(elf_path, size_tool):
    """
    Parse `size` output in Berkeley format.
    Returns: {text, data, bss, flash_used, ram_used}
    """
    try:
        result = subprocess.run(
            [size_tool, elf_path],
            capture_output=True, text=True, timeout=10
        )
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return None

    if result.returncode != 0:
        return None

    # Parse:   text    data     bss     dec     hex filename
    for line in result.stdout.splitlines():
        m = re.match(r'^\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)', line)
        if m:
            text, data, bss, _total = int(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4))
            return {
                "text": text,
                "data": data,
                "bss": bss,
                "flash_used": text + data,
                "ram_used": data + bss,
            }
    return None


def parse_size_sections(elf_path, size_tool):
    """
    Parse `size -A` output for per-section breakdown.
    Returns: {section_name: {size, addr}}
    """
    try:
        result = subprocess.run(
            [size_tool, "-A", elf_path],
            capture_output=True, text=True, timeout=10
        )
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return None

    if result.returncode != 0:
        return None

    sections = {}
    for line in result.stdout.splitlines():
        m = re.match(r'^(\.\S+)\s+(\d+)\s+(\d+)', line)
        if m:
            sections[m.group(1)] = {"size": int(m.group(2)), "addr": int(m.group(3))}
    return sections


def parse_nm_symbols(elf_path, size_tool):
    """
    Parse `nm --print-size --size-sort` for largest symbols.
    Returns sorted list: [{name, size, type, section}]
    """
    # nm tool is in same directory as size tool
    nm_tool = size_tool.replace("-size", "-nm")
    if not os.path.exists(nm_tool):
        return None

    try:
        result = subprocess.run(
            [nm_tool, "--print-size", "--size-sort", "--radix=d", elf_path],
            capture_output=True, text=True, timeout=10
        )
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return None

    if result.returncode != 0:
        return None

    symbols = []
    for line in result.stdout.splitlines():
        m = re.match(r'(\d+)\s+(\d+)\s+(\w)\s+(.+)', line)
        if m:
            symbols.append({
                "addr": int(m.group(1)),
                "size": int(m.group(2)),
                "type": m.group(3),
                "name": m.group(4),
            })
    return symbols


def analyze_build(project_root, env_name):
    """
    Analyze a PlatformIO build for resource usage.

    Returns dict with:
        - size: {text, data, bss, flash_used, ram_used}
        - limits: {flash, ram}
        - pct: {flash_pct, ram_pct}
        - sections: per-section breakdown (or None)
        - top_symbols: largest 20 symbols (or None)
        - env: environment name
        - elf_path: path to ELF
        - error: error message if analysis failed
    """
    elf_path = _find_elf(project_root, env_name)
    if not elf_path:
        return {"env": env_name, "error": f"No build found. Run: pio run -e {env_name}"}

    size_tool = _find_size_tool(env_name)
    if not size_tool:
        return {"env": env_name, "error": "Toolchain size tool not found"}

    size_data = parse_size_berkeley(elf_path, size_tool)
    if not size_data:
        return {"env": env_name, "error": "Failed to parse ELF size"}

    base = _base_env(env_name)
    limits = MEMORY_LIMITS.get(base, {"flash": 0, "ram": 0})

    flash_pct = (size_data["flash_used"] / limits["flash"] * 100) if limits["flash"] else 0
    ram_pct = (size_data["ram_used"] / limits["ram"] * 100) if limits["ram"] else 0

    sections = parse_size_sections(elf_path, size_tool)
    symbols = parse_nm_symbols(elf_path, size_tool)
    top_symbols = symbols[-20:] if symbols else None

    elf_mtime = os.path.getmtime(elf_path)

    return {
        "env": env_name,
        "size": size_data,
        "limits": limits,
        "pct": {"flash_pct": flash_pct, "ram_pct": ram_pct},
        "sections": sections,
        "top_symbols": top_symbols,
        "elf_path": elf_path,
        "elf_age_s": __import__("time").time() - elf_mtime,
    }


def list_available_builds(project_root):
    """List all environments that have a built ELF file."""
    build_dir = os.path.join(project_root, ".pio", "build")
    if not os.path.isdir(build_dir):
        return []
    envs = []
    for d in sorted(os.listdir(build_dir)):
        elf = os.path.join(build_dir, d, "firmware.elf")
        if os.path.isfile(elf):
            envs.append(d)
    return envs
