"""Serial data simulator package for fc_tool testing.

Generates fake serial data in fc_tool's plotter protocol formats and writes
it to a serial port (virtual pty via socat) or stdout.

Usage as CLI:
    python3 -m simulator --stdout --scenario imu
    python3 -m simulator /tmp/vserial1 --scenario sine --rate 50 --duration 30
    python3 -m simulator --list

Usage as library:
    from simulator.imu import scenario_imu
    from simulator.core import write_line
"""

from .imu import scenario_imu
from .sine import scenario_sine
from .mixed import scenario_mixed, scenario_protocol
from .ansi import scenario_ansi
from .stress import scenario_stress, scenario_noise
from .dashboard import scenario_dashboard
from .ramp import scenario_ramp
from .intermittent import scenario_intermittent
from .burst import scenario_burst
from .calibration import scenario_calibration
from .multi_plot import scenario_multi_plot
from .mixed_dashboard import scenario_mixed_dashboard
from .high_channel import scenario_high_channel

# Scenario registry: name -> (function, description)
SCENARIOS = {
    "imu": (scenario_imu, "6-axis IMU data (accel + gyro) on 2 plots"),
    "sine": (scenario_sine, "Multiple sine waves at different frequencies"),
    "mixed": (scenario_mixed, "Mix of all 4 protocol formats"),
    "ansi": (scenario_ansi, "ANSI-colored status messages"),
    "stress": (scenario_stress, "High-rate data across 15 plots (tests max cap)"),
    "dashboard": (scenario_dashboard, "Key=value dashboard readout"),
    "protocol": (scenario_protocol, "All 4 formats in strict rotation"),
    "noise": (scenario_noise, "Random noise with spikes (tests Y-axis scaling)"),
    "ramp": (scenario_ramp, "Linear ramp + step response (tests zoom/scale auto-fit)"),
    "intermittent": (scenario_intermittent, "Data with gaps/silence (tests stale Hz detection)"),
    "burst": (scenario_burst, "High-rate bursts then silence (tests buffer handling)"),
    "calibration": (scenario_calibration, "Interleaved text, ANSI, telemetry, dashboard"),
    "multi_plot": (scenario_multi_plot, "5 plots with known wave shapes"),
    "mixed_dashboard": (scenario_mixed_dashboard, "Alternating plotter data and dashboard lines"),
    "high_channel": (scenario_high_channel, "24 channels on one line (tests parser limits)"),
}
