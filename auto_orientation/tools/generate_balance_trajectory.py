#!/usr/bin/env python3
"""
generate_balance_trajectory.py

Simulate an inverted-pendulum balance robot under the legacy
SelfBallancingRobot3.ino PID controller and emit a CSV fixture for
tests/scenario_test_balancing.cpp.

Model (small balance-bot, ~500 g, ~10 cm half-height):

    state:    theta (rad), theta_dot (rad/s)
    input:    motor torque u (Nm)
    dynamics: theta_ddot = (g/L) sin(theta)
                          - (b / (m * L^2)) * theta_dot
                          + u / (m * L^2)

Closed loop:

    raw_pitch_deg       = degrees(theta)
    corrected_pitch_deg = raw_pitch_deg - PITCH_OFFSET   (PITCH_OFFSET = -8.6)
    pid_output          = Kp*err + Ki*int(err) + Kd*d(err)/dt
                          clamped to +/- 255, dead-band 15 (stiction)
    motor torque u      = k_torque * pid_output           (PWM -> Nm)

The .ino's gains: Kp=65, Ki=12, Kd=38, sample 5 ms.

Self-contained, stdlib only. Numpy is *not* required.

Usage:
    generate_balance_trajectory.py \\
        --output tests/data/balancing_reference_trajectory.csv

Optional:
    --duration_ms 3000   (default 3000)
    --seed 42            (no stochastic terms today, kept for future use)
"""

from __future__ import annotations

import argparse
import csv
import math
import os
import random
import sys
from pathlib import Path

# ------------------------------------------------------------------
# Constants matching SelfBallancingRobot3.ino
# ------------------------------------------------------------------
KP = 65.0
KI = 12.0
KD = 38.0
PITCH_OFFSET_DEG = -8.6
SAMPLE_DT_MS = 5
SAMPLE_DT_S = SAMPLE_DT_MS / 1000.0
PWM_MAX = 255.0
STICTION_DEADBAND = 15.0  # PID output below this magnitude -> motors off

# ------------------------------------------------------------------
# Plant parameters (small balance robot)
# ------------------------------------------------------------------
L = 0.10        # m,   half-height (pendulum length to CoM)
M_KG = 0.5      # kg,  total mass
G = 9.81        # m/s^2
B_FRICTION = 0.001  # N*m*s, rotational friction coefficient

# Map PID PWM output (-255..+255) to physical motor torque.
# Calibrated so that full PWM produces enough torque to recover from
# a moderate initial tilt -- not from a precise datasheet, just keeps
# the closed-loop sim bounded under the legacy gains.
TORQUE_PER_PWM = 0.002  # Nm per PWM unit; full PWM -> 0.51 Nm

# Initial perturbation
THETA0_RAD = 0.05  # ~2.86 degrees

# Simulation integration sub-steps per control sample (RK-ish via
# midpoint, just enough fidelity for a 5 ms control loop).
SUBSTEPS = 4


def pendulum_accel(theta: float, theta_dot: float, u: float) -> float:
    """Inverted-pendulum angular acceleration (theta_ddot)."""
    inertia = M_KG * L * L  # I = m * L^2 (point mass at L)
    return (G / L) * math.sin(theta) \
        - (B_FRICTION / inertia) * theta_dot \
        + u / inertia


def integrate_step(theta: float, theta_dot: float, u: float, dt: float) -> tuple[float, float]:
    """One midpoint (RK2) step of length dt with constant torque u."""
    sub_dt = dt / SUBSTEPS
    for _ in range(SUBSTEPS):
        a1 = pendulum_accel(theta, theta_dot, u)
        # midpoint
        th_mid = theta + 0.5 * sub_dt * theta_dot
        thd_mid = theta_dot + 0.5 * sub_dt * a1
        a2 = pendulum_accel(th_mid, thd_mid, u)
        theta = theta + sub_dt * thd_mid
        theta_dot = theta_dot + sub_dt * a2
    return theta, theta_dot


def apply_stiction_clamp(pid_output: float) -> float:
    """Mirror the .ino's stiction deadband + +/-255 clamp."""
    if abs(pid_output) < STICTION_DEADBAND:
        return 0.0
    if pid_output > PWM_MAX:
        return PWM_MAX
    if pid_output < -PWM_MAX:
        return -PWM_MAX
    return pid_output


def pwm_to_motor_pwms(pid_output: float) -> tuple[float, float]:
    """
    Legacy .ino uses one symmetric output applied as +pid to one motor
    and -pid to the other. We report both for the CSV so the scenario
    test can validate motor-side signal too.
    """
    return (pid_output, -pid_output)


def simulate(duration_ms: int) -> list[tuple[int, float, float, float, float, float]]:
    """
    Run the closed-loop simulation. Returns a list of rows:
        (timestamp_ms, raw_pitch_deg, corrected_pitch_deg,
         pid_output, left_pwm, right_pwm)
    """
    n_samples = duration_ms // SAMPLE_DT_MS

    theta = THETA0_RAD
    theta_dot = 0.0

    integral = 0.0
    prev_error = None  # None signals first iteration -> dt-deriv = 0

    rows: list[tuple[int, float, float, float, float, float]] = []

    for i in range(n_samples):
        t_ms = i * SAMPLE_DT_MS

        raw_pitch_deg = math.degrees(theta)
        # .ino: corrected = pitch_deg - PITCH_OFFSET, with offset = -8.6
        corrected_pitch_deg = raw_pitch_deg - PITCH_OFFSET_DEG

        # Setpoint is 0 (upright in corrected frame). Error is -corrected.
        error = -corrected_pitch_deg
        integral += error * SAMPLE_DT_S
        if prev_error is None:
            derivative = 0.0
        else:
            derivative = (error - prev_error) / SAMPLE_DT_S
        prev_error = error

        pid_raw = KP * error + KI * integral + KD * derivative
        pid_output = apply_stiction_clamp(pid_raw)
        left_pwm, right_pwm = pwm_to_motor_pwms(pid_output)

        rows.append((
            t_ms,
            raw_pitch_deg,
            corrected_pitch_deg,
            pid_output,
            left_pwm,
            right_pwm,
        ))

        # Advance plant by 5 ms with this torque held.
        u = pid_output * TORQUE_PER_PWM
        theta, theta_dot = integrate_step(theta, theta_dot, u, SAMPLE_DT_S)

        # Safety: if the bot has tipped over hard, the dynamics blow up
        # exponentially; cap to keep the CSV finite.
        if abs(theta) > math.pi:
            # snap to +/- pi to keep numbers sane
            theta = math.copysign(math.pi, theta)
            theta_dot = 0.0

    return rows


def write_csv(path: Path, rows: list) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "timestamp_ms",
            "raw_pitch_deg",
            "corrected_pitch_deg",
            "pid_output",
            "left_pwm",
            "right_pwm",
        ])
        for r in rows:
            t_ms, raw, corrected, pid_out, lpwm, rpwm = r
            writer.writerow([
                t_ms,
                f"{raw:.6f}",
                f"{corrected:.6f}",
                f"{pid_out:.4f}",
                f"{lpwm:.4f}",
                f"{rpwm:.4f}",
            ])


def parse_args(argv: list) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Generate synthetic balance-bot pitch trajectory CSV."
    )
    p.add_argument(
        "--output",
        required=True,
        help="Output CSV path (parent dirs will be created).",
    )
    p.add_argument(
        "--duration_ms",
        type=int,
        default=3000,
        help="Simulation duration in milliseconds (default: 3000).",
    )
    p.add_argument(
        "--seed",
        type=int,
        default=42,
        help="RNG seed (currently unused, reserved for future noise).",
    )
    return p.parse_args(argv)


def main(argv: list) -> int:
    args = parse_args(argv)

    if args.duration_ms <= 0:
        print("ERROR: --duration_ms must be positive", file=sys.stderr)
        return 2
    if args.duration_ms % SAMPLE_DT_MS != 0:
        print(
            f"WARNING: duration_ms={args.duration_ms} is not a multiple of "
            f"sample_dt={SAMPLE_DT_MS} ms; truncating.",
            file=sys.stderr,
        )

    random.seed(args.seed)  # reserved; deterministic for future stochastic terms

    rows = simulate(args.duration_ms)

    out_path = Path(args.output)
    write_csv(out_path, rows)

    final_pitch = rows[-1][1]
    max_abs_pitch = max(abs(r[1]) for r in rows)
    print(
        f"Wrote {len(rows)} rows. "
        f"Final pitch: {final_pitch:.3f} deg, "
        f"max abs pitch: {max_abs_pitch:.3f} deg."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
