"""Shared pytest fixtures for fc_tool tests.

Provides:
  - fc_tool_bin: auto-discovers debug/release binary, skips if not found
  - socat_pair: creates virtual serial port pair, cleans up after test
  - run_simulator_stdout: factory to run simulate_serial.py in --stdout mode
  - run_headless: factory to start fc_tool headless + simulator via socat
"""

import os
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path

import pytest

# Resolve paths relative to this file
TESTS_DIR = Path(__file__).parent
FC_TOOL_DIR = TESTS_DIR.parent
SIMULATOR_SCRIPT = TESTS_DIR / "simulate_serial.py"

# Virtual serial port names (different from bash tests to avoid collision)
VSERIAL_BASE = "/tmp/vserial_pytest"


@pytest.fixture(scope="session")
def fc_tool_bin():
    """Discover fc_tool binary (debug preferred, then release). Skip if not found."""
    # Check standard Tauri build output locations
    candidates = [
        FC_TOOL_DIR / "src-tauri" / "target" / "debug" / "fc-tool",
        FC_TOOL_DIR / "src-tauri" / "target" / "release" / "fc-tool",
        FC_TOOL_DIR / "src-tauri" / "target" / "debug" / "fc_tool",
        FC_TOOL_DIR / "src-tauri" / "target" / "release" / "fc_tool",
    ]
    for path in candidates:
        if path.exists() and os.access(path, os.X_OK):
            return str(path)

    pytest.skip("fc_tool binary not found (run 'npm run tauri build' first)")


@pytest.fixture
def socat_pair():
    """Create a virtual serial port pair using socat.

    Returns (port_a, port_b) paths. port_a is for the simulator,
    port_b is for fc_tool to read from. Cleans up on teardown.
    """
    if not shutil.which("socat"):
        pytest.skip("socat not installed")

    # Use PID-based unique names to allow parallel test runs
    port_a = f"{VSERIAL_BASE}_{os.getpid()}_a"
    port_b = f"{VSERIAL_BASE}_{os.getpid()}_b"

    proc = subprocess.Popen(
        [
            "socat",
            f"pty,raw,echo=0,link={port_a}",
            f"pty,raw,echo=0,link={port_b}",
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    # Wait for pty symlinks to appear
    deadline = time.time() + 5
    while time.time() < deadline:
        if os.path.exists(port_a) and os.path.exists(port_b):
            break
        time.sleep(0.1)
    else:
        proc.kill()
        pytest.fail(f"socat failed to create virtual serial ports within 5s")

    yield port_a, port_b

    # Teardown
    proc.terminate()
    try:
        proc.wait(timeout=3)
    except subprocess.TimeoutExpired:
        proc.kill()
    # Clean up symlinks
    for p in (port_a, port_b):
        try:
            os.unlink(p)
        except OSError:
            pass


@pytest.fixture
def run_simulator_stdout():
    """Factory fixture: run simulate_serial.py with --stdout and capture output.

    Usage:
        lines = run_simulator_stdout("imu", rate=50, duration=1)
        assert len(lines) > 0
    """
    procs = []

    def _run(scenario: str, rate: int = 50, duration: float = 1.0) -> list[str]:
        proc = subprocess.Popen(
            [
                sys.executable,
                str(SIMULATOR_SCRIPT),
                "--stdout",
                "--scenario", scenario,
                "--rate", str(rate),
                "--duration", str(duration),
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        procs.append(proc)
        stdout, stderr = proc.communicate(timeout=duration + 10)
        lines = [l for l in stdout.strip().split("\n") if l.strip()]
        return lines

    yield _run

    for proc in procs:
        if proc.poll() is None:
            proc.kill()


@pytest.fixture
def run_headless(fc_tool_bin, socat_pair):
    """Factory fixture: start fc_tool in headless mode connected via socat.

    Returns (fc_proc, sim_proc, port_b) where:
      - fc_proc: fc_tool headless process (stdout is serial output)
      - sim_proc: simulator process writing to port_a
      - port_b: the port fc_tool is reading from

    Usage:
        fc_proc, sim_proc, port = run_headless("imu", rate=50, duration=3)
        output = fc_proc.stdout.read()
    """
    port_a, port_b = socat_pair
    instances = []

    def _run(scenario: str, rate: int = 50, duration: float = 3.0,
             extra_args: list[str] | None = None):
        # Start simulator writing to port_a
        sim_proc = subprocess.Popen(
            [
                sys.executable,
                str(SIMULATOR_SCRIPT),
                port_a,
                "--scenario", scenario,
                "--rate", str(rate),
                "--duration", str(duration),
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            text=True,
        )

        # Give simulator a moment to open the port
        time.sleep(0.3)

        # Start fc_tool in headless mode reading from port_b
        cmd = [fc_tool_bin, "--headless", "--port", port_b, "--baud", "115200"]
        if extra_args:
            cmd.extend(extra_args)

        fc_proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

        instances.append((fc_proc, sim_proc))
        return fc_proc, sim_proc, port_b

    yield _run

    for fc_proc, sim_proc in instances:
        for proc in (fc_proc, sim_proc):
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    proc.kill()
