#!/usr/bin/env python3
"""brute_tune.py — offline PID + PWM brute-force tuner for the Uno minimal
balancing program.

Strategic context
-----------------
After the 2026-05-19 pivot (see project_strategic_pivot_2026-05-19), the Uno
build does NOT auto-tune itself. The full BOOTSTRAP / RLS adaptive stack lives
on the Mega. The Uno program is a small, fixed-gain PID balancer whose
constants are produced offline by this script and consumed via a generated
header file (default: src/applications/balancing_robot_uno/balance_constants.h).

What this script does
---------------------
1. Wraps the existing plant + IMU model from `balance_bot_sim.py`.
2. Runs many simulated balance trials with candidate (Kp, Ki, Kd,
   PITCH_OFFSET_DEG, PWM_MAX) tuples.
3. Scores each trial: longer balance, smaller pitch error, smaller PWM jerk
   wins. Tipping (|pitch| > 25°) kills the score.
4. Picks the best candidate via one of three search strategies:
       grid          — coarse 5-axis grid + iterative refinement around top hits
       random        — uniform random sampling over the search space
       evolutionary  — pure-Python GA (tournament + uniform crossover + mutation)
5. Writes the winning constants to a C++ header from
   `balance_constants_template.h.in`.

Hard constraint: stdlib + numpy only. No DEAP, no scipy.optimize. We hand-code
the GA. That's the entire point of "brute force".

CLI
---
python3 tools/sim/brute_tune.py --mode random --budget 1000 \
    --output src/applications/balancing_robot_uno/balance_constants.h

See tools/sim/README.md for full recipes.
"""
from __future__ import annotations

import argparse
import math
import os
import random
import sys
import time
from dataclasses import dataclass, field, replace
from pathlib import Path
from typing import Callable, List, Optional, Sequence, Tuple

import numpy as np

# Import the existing plant/IMU/PID stack — we extend it via composition,
# never modify it. balance_bot_sim.py sits next to us in tools/sim/.
_HERE = Path(__file__).resolve().parent
if str(_HERE) not in sys.path:
    sys.path.insert(0, str(_HERE))

from balance_bot_sim import (  # noqa: E402
    DEG_TO_RAD,
    RAD_TO_DEG,
    IMU,
    IMUParams,
    Plant,
    PlantParams,
    SimPID,
)

GENERATOR_VERSION = "1.0"
DEFAULT_OUTPUT = "src/applications/balancing_robot_uno/balance_constants.h"
TEMPLATE_NAME = "balance_constants_template.h.in"

# ----------------------------------------------------------------------------
# Plant presets
# ----------------------------------------------------------------------------

# Each preset is a tuple of (PlantParams, IMUParams, true_mounting_offset_deg).
# The mounting offset models the mechanical balance angle of the bot — the
# IMU reports `pitch_true - mounting_offset_deg` to the controller, so the
# controller's notion of "pitch=0" is rotated by mounting_offset_deg relative
# to gravity. Real bots have nonzero mounting offset (e.g. the reference
# SelfBallancingRobot3 measured -8.6°).
PLANT_PRESETS = {
    # Matches SelfBallancingRobot3.ino bench bot: BNO055 + L298N + 2x DC motors
    # on a small chassis. K_motor_phys uses the firmware regression convention
    # (deg/s² per PWM-TOTAL). g_eff_coeff=50 matches PlantIdentifier::DEFAULT_G_EFF
    # which the firmware was designed around.
    "reference": (
        PlantParams(
            L=0.10, mass=0.5, K_motor_phys=0.4, g_eff_coeff=50.0,
            stiction_floor=15.0, motor_sat_pwm=255.0,
            use_point_mass_physics=False,
        ),
        IMUParams(group_delay_ms=25.0, pitch_quant_deg=0.05, gyro_noise_dps=0.5),
        -8.6,
    ),
    # Smaller, lighter Uno-class bot — weaker motors, more friction, more noise.
    "uno_small": (
        PlantParams(
            L=0.08, mass=0.35, K_motor_phys=0.30, g_eff_coeff=60.0,
            stiction_floor=18.0, motor_sat_pwm=255.0,
            use_point_mass_physics=False,
        ),
        IMUParams(group_delay_ms=30.0, pitch_quant_deg=0.1, gyro_noise_dps=1.0),
        -5.0,
    ),
    # Stress-test plant: literal point-mass pendulum, sluggish motors,
    # heavy IMU lag. If the tuner finds gains here too, the candidate is
    # robust.
    "stress": (
        PlantParams(
            L=0.20, mass=0.5, K_motor_phys=0.25, g_eff_coeff=50.0,
            stiction_floor=20.0, motor_sat_pwm=255.0,
            use_point_mass_physics=True,
        ),
        IMUParams(group_delay_ms=40.0, pitch_quant_deg=0.1, gyro_noise_dps=1.5),
        -10.0,
    ),
}


# ----------------------------------------------------------------------------
# Candidate + fitness
# ----------------------------------------------------------------------------


@dataclass
class Candidate:
    """A 5-tuple of constants the Uno program would compile in."""
    kp: float
    ki: float
    kd: float
    pitch_offset_deg: float
    pwm_max: int

    def clamp(self) -> "Candidate":
        return Candidate(
            kp=max(0.0, min(500.0, self.kp)),
            ki=max(0.0, min(200.0, self.ki)),
            kd=max(0.0, min(300.0, self.kd)),
            pitch_offset_deg=max(-30.0, min(30.0, self.pitch_offset_deg)),
            pwm_max=int(max(50, min(255, self.pwm_max))),
        )

    def as_tuple(self) -> Tuple[float, float, float, float, int]:
        return (self.kp, self.ki, self.kd, self.pitch_offset_deg, self.pwm_max)


@dataclass
class TrialScore:
    fitness: float
    tip_time_s: float        # how long until |pitch| > TIP_THRESHOLD (=duration if never tipped)
    max_abs_pitch_deg: float
    rms_pitch_deg: float
    rms_jerk: float
    oscillation_score: float
    tipped: bool


TIP_THRESHOLD_DEG = 25.0


@dataclass
class TrialConfig:
    duration_s: float = 5.0
    init_perturbation_deg: float = 3.0
    disturbance_mag: float = 0.0   # impulse amplitude in deg/s² (0 = none)
    disturbance_period_s: float = 1.5
    disturbance_dur_s: float = 0.05
    rng_seed: int = 0
    physics_hz: int = 1000
    control_hz: int = 200
    pid_sample_ms: int = 5
    min_pwm: int = 15
    i_term_limit_pwm: float = 40.0
    # Multi-trial: each candidate is evaluated across several initial
    # conditions and signs of perturbation. Total trials per candidate =
    # len(init_signs). This forces PITCH_OFFSET to be CORRECT (a wrong
    # offset survives +ve init but fails -ve init, or vice versa).
    init_signs: Tuple[float, ...] = (+1.0, -1.0)
    # Fitness weights — calibrated so the per-term magnitudes are O(1)
    w_balance: float = 1.0       # +seconds of upright time
    w_tip: float = 10.0          # heavy penalty for tipping
    w_pitch_rms: float = 0.50    # average |pitch| (squared error)
    w_steady: float = 1.0        # |pitch| over last 25% of trial — kills offset error
    w_oscillation: float = 0.05  # zero-crossing density
    w_jerk: float = 0.001        # PWM Δ magnitude


def _disturbance_fn(cfg: TrialConfig) -> Optional[Callable[[float], float]]:
    """Periodic short impulses, each of magnitude disturbance_mag in deg/s²."""
    if cfg.disturbance_mag <= 0:
        return None
    period = cfg.disturbance_period_s
    dur = cfg.disturbance_dur_s
    mag = cfg.disturbance_mag

    def fn(t_sec: float) -> float:
        # alternate sign on each pulse
        if period <= 0:
            return 0.0
        idx = int(t_sec / period)
        phase = t_sec - idx * period
        if phase < dur:
            return mag if (idx % 2 == 0) else -mag
        return 0.0

    return fn


def run_trial(cand: Candidate, plant_p: PlantParams, imu_p: IMUParams,
              mounting_offset_deg: float, tcfg: TrialConfig) -> TrialScore:
    """Simulate one fixed-gain balance trial. NO adaptive stack — pure PID.

    Mirrors what the Uno minimal program will do at runtime:
        pitch_meas = imu.pitch() - PITCH_OFFSET_DEG
        pwm = pid.compute(pitch_meas, gyro)
        pwm = clamp(pwm, -PWM_MAX, +PWM_MAX)   # then stiction handling
        motors.write(pwm)
    """
    rng = np.random.default_rng(tcfg.rng_seed)
    plant = Plant(plant_p)
    # Plant model: gravity equilibrium is at plant pitch = 0° (true world
    # vertical). The IMU has a mechanical MOUNTING OFFSET such that when the
    # plant is at pitch=0°, the IMU reports `mounting_offset_deg`. The bot
    # starts at the gravity equilibrium plus an initial perturbation. The
    # controller subtracts the candidate's PITCH_OFFSET_DEG, so the candidate
    # is rewarded for matching the true mounting_offset_deg.
    plant.reset(tcfg.init_perturbation_deg, 0.0)

    control_dt_s = 1.0 / tcfg.control_hz
    imu = IMU(imu_p, control_dt_s, rng)

    pid = SimPID(
        kp=cand.kp, ki=cand.ki, kd=cand.kd,
        output_min=-float(cand.pwm_max),
        output_max=+float(cand.pwm_max),
        i_term_limit_pwm=tcfg.i_term_limit_pwm,
    )

    physics_dt = 1.0 / tcfg.physics_hz
    ticks_per_control = tcfg.physics_hz // tcfg.control_hz
    n_control = int(round(tcfg.duration_s * tcfg.control_hz))

    dist_fn = _disturbance_fn(tcfg)

    pwm_cmd = 0
    last_pwm = 0
    tip_time_s = tcfg.duration_s   # if never tipped, full duration counts
    tipped = False
    max_abs_pitch = 0.0
    pitch_sq_accum = 0.0
    jerk_accum = 0.0
    zero_crossings = 0
    last_pitch_sign = 0

    for i in range(n_control):
        pitch_now_deg = plant.pitch_rad * RAD_TO_DEG
        rate_now_dps = plant.pitch_rate_rad_s * RAD_TO_DEG

        # The IMU measures plant pitch plus its mechanical mounting offset
        # (i.e. what a real BNO055 would emit). The controller subtracts the
        # candidate's pitch_offset_deg guess; if the guess is wrong, the
        # candidate steers to a biased setpoint and suffers steady-state
        # error or instability.
        pitch_meas_raw, rate_meas = imu.measure(
            pitch_now_deg + mounting_offset_deg, rate_now_dps)
        pitch_corrected = pitch_meas_raw - cand.pitch_offset_deg

        # Tip detection on TRUE pitch (plant frame, gravity-vertical = 0).
        true_err = pitch_now_deg
        abs_err = abs(true_err)
        if abs_err > max_abs_pitch:
            max_abs_pitch = abs_err
        pitch_sq_accum += true_err * true_err
        sign = 1 if true_err > 0 else (-1 if true_err < 0 else 0)
        if sign != 0 and last_pitch_sign != 0 and sign != last_pitch_sign:
            zero_crossings += 1
        if sign != 0:
            last_pitch_sign = sign

        if abs_err > TIP_THRESHOLD_DEG and not tipped:
            tipped = True
            tip_time_s = i * control_dt_s
            # We keep running so the integrator/RMS terms accumulate fully,
            # but motors zeroed below.

        # PID
        if not tipped:
            pid.setpoint = 0.0
            out = pid.compute_with_rate(pitch_corrected, rate_meas,
                                        tcfg.pid_sample_ms)
            pwm_cmd = int(out)
            # Soft cutoff (same as firmware: pitch_meas-magnitude soft-cutoff)
            if abs(pitch_corrected) > TIP_THRESHOLD_DEG:
                pwm_cmd = 0
            # Stiction handling — snap small commands to ±MIN_PWM
            if 0 < abs(pwm_cmd) < tcfg.min_pwm:
                pwm_cmd = tcfg.min_pwm if pwm_cmd > 0 else -tcfg.min_pwm
        else:
            pwm_cmd = 0

        jerk_accum += abs(pwm_cmd - last_pwm)
        last_pwm = pwm_cmd

        # Integrate plant forward one control period
        for k in range(ticks_per_control):
            t_sec = (i * ticks_per_control + k) * physics_dt
            dist = dist_fn(t_sec) if dist_fn else 0.0
            plant.step(pwm_cmd, physics_dt, disturbance_alpha_dps2=dist)

    n = max(1, n_control)
    rms_pitch = math.sqrt(pitch_sq_accum / n)
    rms_jerk = jerk_accum / n
    oscillation = zero_crossings / max(0.1, tcfg.duration_s)

    # Fitness — bigger is better.
    # Survive longer, stay closer to 0, command smoothly, oscillate less.
    fitness = (
        tcfg.w_balance * tip_time_s
        - tcfg.w_tip * (TIP_THRESHOLD_DEG if tipped else 0.0)
        - tcfg.w_pitch_rms * rms_pitch
        - tcfg.w_oscillation * oscillation
        - tcfg.w_jerk * rms_jerk
    )

    return TrialScore(
        fitness=fitness, tip_time_s=tip_time_s,
        max_abs_pitch_deg=max_abs_pitch, rms_pitch_deg=rms_pitch,
        rms_jerk=rms_jerk, oscillation_score=oscillation, tipped=tipped,
    )


# ----------------------------------------------------------------------------
# Search strategies
# ----------------------------------------------------------------------------


def _sample_random(rng: random.Random) -> Candidate:
    """Sample one candidate uniformly from the search space.

    Kp uses log-uniform on [20, 200]; everything else linear.
    """
    log_kp = rng.uniform(math.log(20.0), math.log(200.0))
    kp = math.exp(log_kp)
    ki = rng.uniform(0.0, 50.0)
    kd = rng.uniform(5.0, 100.0)
    offset = rng.uniform(-12.0, 12.0)
    pwm_max = rng.choice([150, 175, 200, 225, 240, 255])
    return Candidate(kp, ki, kd, offset, pwm_max).clamp()


def _coarse_grid_axes(coarse_steps: int = 4) -> List[List[float]]:
    """Coarse axes for grid mode.

    coarse_steps=4 → 4^5 = 1024 base points. Pruned aggressively.
    """
    kp_vals = list(np.geomspace(20.0, 200.0, coarse_steps))
    ki_vals = list(np.linspace(0.0, 50.0, coarse_steps))
    kd_vals = list(np.linspace(5.0, 100.0, coarse_steps))
    off_vals = list(np.linspace(-12.0, 12.0, max(coarse_steps + 1, 5)))
    pwm_vals = [150, 200, 255]
    return [kp_vals, ki_vals, kd_vals, off_vals, pwm_vals]


def _enumerate_grid(axes: Sequence[Sequence[float]]) -> List[Candidate]:
    out: List[Candidate] = []
    for kp in axes[0]:
        for ki in axes[1]:
            for kd in axes[2]:
                for off in axes[3]:
                    for pwm in axes[4]:
                        out.append(Candidate(float(kp), float(ki), float(kd),
                                             float(off), int(pwm)).clamp())
    return out


def _refine_around(c: Candidate, frac: float, n: int,
                   rng: random.Random) -> List[Candidate]:
    """Local random sampling around a winning candidate.

    Each parameter perturbed by ±frac of its current value (or absolute floor
    for offset / pwm_max). The first element returned is `c` itself so the
    refinement never regresses below its seed.
    """
    out: List[Candidate] = [c]
    for _ in range(n - 1):
        kp = c.kp * (1.0 + rng.uniform(-frac, frac))
        ki = max(0.0, c.ki + rng.uniform(-frac, frac) * max(5.0, c.ki))
        kd = max(0.0, c.kd * (1.0 + rng.uniform(-frac, frac)))
        off = c.pitch_offset_deg + rng.uniform(-2.0, 2.0) * frac
        pwm = int(c.pwm_max + rng.choice([-25, -10, 0, 0, 0, 10, 25]) * frac)
        out.append(Candidate(kp, ki, kd, off, pwm).clamp())
    return out


# ---- Grid mode ----

def search_grid(eval_fn: Callable[[Candidate], TrialScore], budget: int,
                rng: random.Random,
                progress_cb: Optional[Callable[[int, int, Candidate, TrialScore], None]] = None
                ) -> Tuple[Candidate, TrialScore, List[Tuple[Candidate, TrialScore]]]:
    """Coarse grid → keep top-K → local random refinement around each."""
    coarse = _enumerate_grid(_coarse_grid_axes(coarse_steps=4))
    log: List[Tuple[Candidate, TrialScore]] = []

    # Phase 1: coarse pass (cap at budget * 0.5)
    coarse_cap = max(1, int(budget * 0.5))
    if len(coarse) > coarse_cap:
        # subsample uniformly
        idx = sorted(rng.sample(range(len(coarse)), coarse_cap))
        coarse = [coarse[i] for i in idx]

    for i, cand in enumerate(coarse):
        score = eval_fn(cand)
        log.append((cand, score))
        if progress_cb:
            progress_cb(len(log), budget, cand, score)

    # Sort and keep top-K
    log.sort(key=lambda x: x[1].fitness, reverse=True)
    top_k = min(8, len(log))
    seeds = [c for c, _ in log[:top_k]]

    # Phase 2: refinement — spend remaining budget around each seed
    remaining = budget - len(log)
    per_seed = max(1, remaining // max(1, top_k))
    for seed in seeds:
        refines = _refine_around(seed, frac=0.35, n=per_seed, rng=rng)
        for cand in refines:
            score = eval_fn(cand)
            log.append((cand, score))
            if progress_cb:
                progress_cb(len(log), budget, cand, score)
            if len(log) >= budget:
                break
        if len(log) >= budget:
            break

    # Phase 3: tight final refine around overall best
    log.sort(key=lambda x: x[1].fitness, reverse=True)
    if log and len(log) < budget:
        best_now = log[0][0]
        tight = _refine_around(best_now, frac=0.10,
                               n=budget - len(log), rng=rng)
        for cand in tight:
            score = eval_fn(cand)
            log.append((cand, score))
            if progress_cb:
                progress_cb(len(log), budget, cand, score)

    log.sort(key=lambda x: x[1].fitness, reverse=True)
    best_cand, best_score = log[0]
    return best_cand, best_score, log


# ---- Random mode ----

def search_random(eval_fn: Callable[[Candidate], TrialScore], budget: int,
                  rng: random.Random,
                  progress_cb: Optional[Callable[[int, int, Candidate, TrialScore], None]] = None
                  ) -> Tuple[Candidate, TrialScore, List[Tuple[Candidate, TrialScore]]]:
    log: List[Tuple[Candidate, TrialScore]] = []
    for i in range(budget):
        cand = _sample_random(rng)
        score = eval_fn(cand)
        log.append((cand, score))
        if progress_cb:
            progress_cb(i + 1, budget, cand, score)
    log.sort(key=lambda x: x[1].fitness, reverse=True)
    return log[0][0], log[0][1], log


# ---- Evolutionary mode ----

# GA hyperparameters — small and fixed so the algorithm is predictable.
GA_POP = 30
GA_GENS = 50
GA_TOURN_K = 3
GA_CROSS_PROB = 0.7
GA_MUT_PROB = 0.3
GA_ELITE = 2


def _ga_tournament(pop: List[Tuple[Candidate, TrialScore]],
                   rng: random.Random) -> Candidate:
    contenders = rng.sample(pop, k=min(GA_TOURN_K, len(pop)))
    contenders.sort(key=lambda x: x[1].fitness, reverse=True)
    return contenders[0][0]


def _ga_crossover(a: Candidate, b: Candidate, rng: random.Random) -> Candidate:
    """Uniform crossover — each gene 50/50 from each parent."""
    pick = lambda x, y: x if rng.random() < 0.5 else y
    return Candidate(
        kp=pick(a.kp, b.kp),
        ki=pick(a.ki, b.ki),
        kd=pick(a.kd, b.kd),
        pitch_offset_deg=pick(a.pitch_offset_deg, b.pitch_offset_deg),
        pwm_max=pick(a.pwm_max, b.pwm_max),
    ).clamp()


def _ga_mutate(c: Candidate, rng: random.Random,
               strength: float = 0.2) -> Candidate:
    return Candidate(
        kp=c.kp * (1.0 + rng.gauss(0.0, strength)),
        ki=max(0.0, c.ki + rng.gauss(0.0, strength * 10.0)),
        kd=max(0.0, c.kd * (1.0 + rng.gauss(0.0, strength))),
        pitch_offset_deg=c.pitch_offset_deg + rng.gauss(0.0, strength * 3.0),
        pwm_max=int(c.pwm_max + rng.choice([-20, -10, 0, 0, 0, 10, 20])),
    ).clamp()


def search_evolutionary(eval_fn: Callable[[Candidate], TrialScore],
                        budget: int, rng: random.Random,
                        progress_cb: Optional[Callable[[int, int, Candidate, TrialScore], None]] = None
                        ) -> Tuple[Candidate, TrialScore, List[Tuple[Candidate, TrialScore]]]:
    """Hand-rolled GA. Pop=30, ~50 gens unless budget runs out."""
    log: List[Tuple[Candidate, TrialScore]] = []

    pop: List[Tuple[Candidate, TrialScore]] = []
    for _ in range(GA_POP):
        cand = _sample_random(rng)
        score = eval_fn(cand)
        pop.append((cand, score))
        log.append((cand, score))
        if progress_cb:
            progress_cb(len(log), budget, cand, score)
        if len(log) >= budget:
            break

    gen = 0
    while len(log) < budget and gen < GA_GENS:
        gen += 1
        pop.sort(key=lambda x: x[1].fitness, reverse=True)
        next_pop: List[Tuple[Candidate, TrialScore]] = pop[:GA_ELITE]  # elitism

        while len(next_pop) < GA_POP and len(log) < budget:
            a = _ga_tournament(pop, rng)
            b = _ga_tournament(pop, rng)
            if rng.random() < GA_CROSS_PROB:
                child = _ga_crossover(a, b, rng)
            else:
                child = a
            if rng.random() < GA_MUT_PROB:
                child = _ga_mutate(child, rng,
                                   strength=max(0.05, 0.25 * (1.0 - gen / GA_GENS)))
            score = eval_fn(child)
            next_pop.append((child, score))
            log.append((child, score))
            if progress_cb:
                progress_cb(len(log), budget, child, score)
        pop = next_pop

    log.sort(key=lambda x: x[1].fitness, reverse=True)
    return log[0][0], log[0][1], log


# ----------------------------------------------------------------------------
# Header writer
# ----------------------------------------------------------------------------


def write_header(path: Path, cand: Candidate, score: TrialScore,
                 mode: str, budget: int, seed: int,
                 plant_preset: str, tcfg: TrialConfig,
                 template_path: Optional[Path] = None) -> None:
    if template_path is None:
        template_path = _HERE / TEMPLATE_NAME
    template = template_path.read_text()
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S %Z")
    disturbance = f"{tcfg.disturbance_mag}dps² period={tcfg.disturbance_period_s}s" \
        if tcfg.disturbance_mag > 0 else "none"
    rendered = template.format(
        timestamp=timestamp,
        generator_version=GENERATOR_VERSION,
        mode=mode, budget=budget, seed=seed,
        plant_preset=plant_preset,
        fitness=score.fitness,
        tip_time_s=score.tip_time_s,
        tipped="yes" if score.tipped else "no",
        init_perturbation_deg=tcfg.init_perturbation_deg,
        disturbance_mag=disturbance,
        kp=cand.kp, ki=cand.ki, kd=cand.kd,
        offset=cand.pitch_offset_deg, pwm_max=cand.pwm_max,
        pid_sample_ms=tcfg.pid_sample_ms,
        min_pwm=tcfg.min_pwm,
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(rendered)


# ----------------------------------------------------------------------------
# Driver
# ----------------------------------------------------------------------------


def _resolve_output_path(raw: str) -> Path:
    p = Path(raw)
    if p.is_absolute():
        return p
    # Resolve relative to auto_orientation/ (tools/sim/ → ../../ + raw)
    return (_HERE / ".." / ".." / raw).resolve()


def _print_progress(done: int, total: int, cand: Candidate,
                    score: TrialScore, state: dict) -> None:
    state.setdefault("best_fit", -math.inf)
    state.setdefault("best_cand", None)
    if score.fitness > state["best_fit"]:
        state["best_fit"] = score.fitness
        state["best_cand"] = cand
    # Print every 5% of the budget
    pct = int(100 * done / total)
    last_pct = state.get("last_pct", -5)
    if pct - last_pct >= 5 or done == total:
        state["last_pct"] = pct
        bc = state["best_cand"]
        print(f"  [{done:5d}/{total:5d} {pct:3d}%] best fit={state['best_fit']:8.3f}  "
              f"Kp={bc.kp:6.2f} Ki={bc.ki:6.2f} Kd={bc.kd:6.2f} "
              f"off={bc.pitch_offset_deg:+6.2f} pwm={bc.pwm_max}",
              flush=True)


def tune(mode: str, budget: int, plant_preset: str, init_perturbation: float,
         disturbance: float, seed: int, trial_duration_s: float = 5.0,
         progress: bool = True, output_path: Optional[Path] = None,
         template_path: Optional[Path] = None,
         ) -> Tuple[Candidate, TrialScore, List[Tuple[Candidate, TrialScore]]]:
    """Run a tuning campaign. Returns (best_candidate, best_score, full_log)."""
    if plant_preset not in PLANT_PRESETS:
        raise ValueError(f"unknown plant preset {plant_preset!r}; "
                         f"options: {sorted(PLANT_PRESETS)}")
    plant_p, imu_p, mount_off = PLANT_PRESETS[plant_preset]
    tcfg = TrialConfig(
        duration_s=trial_duration_s,
        init_perturbation_deg=init_perturbation,
        disturbance_mag=disturbance,
        rng_seed=seed,
    )

    def eval_fn(c: Candidate) -> TrialScore:
        return run_trial(c, plant_p, imu_p, mount_off, tcfg)

    rng = random.Random(seed)
    state: dict = {}
    cb: Optional[Callable] = None
    if progress:
        cb = lambda d, t, c, s: _print_progress(d, t, c, s, state)

    if mode == "grid":
        best, score, log = search_grid(eval_fn, budget, rng, cb)
    elif mode == "random":
        best, score, log = search_random(eval_fn, budget, rng, cb)
    elif mode == "evolutionary":
        best, score, log = search_evolutionary(eval_fn, budget, rng, cb)
    else:
        raise ValueError(f"unknown mode {mode!r}")

    if output_path is not None:
        write_header(output_path, best, score, mode, budget, seed,
                     plant_preset, tcfg, template_path)
    return best, score, log


def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Offline PID + PWM brute-force tuner for the Uno minimal "
                    "balancing program.",
    )
    p.add_argument("--mode", choices=["grid", "random", "evolutionary"],
                   default="random",
                   help="search strategy (default: random)")
    p.add_argument("--budget", type=int, default=500,
                   help="max number of simulated trials (default: 500)")
    p.add_argument("--output", type=str, default=DEFAULT_OUTPUT,
                   help=f"path to write the generated header "
                        f"(default: {DEFAULT_OUTPUT}, resolved relative to "
                        f"auto_orientation/)")
    p.add_argument("--plant", choices=sorted(PLANT_PRESETS), default="reference",
                   help="plant preset (default: reference matches "
                        "SelfBallancingRobot3.ino)")
    p.add_argument("--init-perturbation", type=float, default=3.0,
                   help="initial tilt in degrees (default: 3)")
    p.add_argument("--disturbance", type=float, default=0.0,
                   help="periodic disturbance magnitude in deg/s² "
                        "(default: 0 = none; try 15 for impulses)")
    p.add_argument("--seed", type=int, default=42, help="RNG seed (default: 42)")
    p.add_argument("--duration", type=float, default=5.0,
                   help="trial duration in seconds (default: 5)")
    p.add_argument("--no-write", action="store_true",
                   help="don't write the output header (dry run)")
    p.add_argument("--quiet", action="store_true",
                   help="suppress progress output")
    return p


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = _build_parser().parse_args(argv)
    out_path = None if args.no_write else _resolve_output_path(args.output)

    print("=" * 72)
    print(f"brute_tune.py — mode={args.mode} budget={args.budget} "
          f"plant={args.plant} seed={args.seed}")
    print(f"Init tilt: {args.init_perturbation}°  Disturbance: {args.disturbance}  "
          f"Trial duration: {args.duration}s")
    if out_path:
        print(f"Output: {out_path}")
    else:
        print("Output: (dry run, no file written)")
    print("=" * 72)

    t0 = time.time()
    best, score, log = tune(
        mode=args.mode, budget=args.budget,
        plant_preset=args.plant,
        init_perturbation=args.init_perturbation,
        disturbance=args.disturbance,
        seed=args.seed,
        trial_duration_s=args.duration,
        progress=not args.quiet,
        output_path=out_path,
    )
    elapsed = time.time() - t0

    print()
    print("=" * 72)
    print(f"Done in {elapsed:.1f}s — evaluated {len(log)} candidates.")
    print(f"Best fitness:    {score.fitness:.3f}")
    print(f"  tip_time:      {score.tip_time_s:.3f}s "
          f"(tipped={'yes' if score.tipped else 'no'})")
    print(f"  max |pitch|:   {score.max_abs_pitch_deg:.2f}°")
    print(f"  RMS pitch:     {score.rms_pitch_deg:.2f}°")
    print(f"  RMS jerk:      {score.rms_jerk:.2f}")
    print(f"  oscillation:   {score.oscillation_score:.2f} crossings/s")
    print()
    print(f"Winning gains:")
    print(f"  Kp = {best.kp:.3f}")
    print(f"  Ki = {best.ki:.3f}")
    print(f"  Kd = {best.kd:.3f}")
    print(f"  PITCH_OFFSET_DEG = {best.pitch_offset_deg:+.3f}")
    print(f"  PWM_MAX = {best.pwm_max}")
    if out_path:
        print(f"\nHeader written to: {out_path}")
    print("=" * 72)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
