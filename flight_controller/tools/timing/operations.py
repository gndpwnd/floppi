"""Flight controller operation complexity definitions."""

from dataclasses import dataclass


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
