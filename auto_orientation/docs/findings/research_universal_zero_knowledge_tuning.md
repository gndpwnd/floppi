# Zero-Knowledge Universal Tuning for Inverted Pendulums — Research Deep-Dive
Status: research. Companion to `UNIVERSAL_BALANCE_BOT_VISION.md` and `AUTO_TUNING_REALITY_CHECK.md`.
Last updated: 2026-05-12

## 0. The question, precisely stated

Can a controller be dropped onto an arbitrary small two-wheeled inverted pendulum — with only a calibrated IMU as its prior — and bootstrap to a working balance loop, with **no per-bot config** (mass, length, motor specs, wheel diameter, gear ratio)?

This document does not re-argue *why* we want that (`UNIVERSAL_BALANCE_BOT_VISION.md`) nor *that* the bootstrap must be sequential (`AUTO_TUNING_REALITY_CHECK.md`). It answers: **what algorithms exist, what they guarantee, what they cost on AVR, which one we should ship.**

Up front: **yes, within a declared size class with a conservative hardcoded seed.** "Universal" across all inverted pendulums (5 cm to 5 m, any actuator) is an open research problem. "Universal within bench-scale (0.1–1 kg, 0.1–1 m, brushed DC + L298N-class driver)" is achievable today with off-the-shelf adaptive-control theory.

## 1. The theoretical reality

### 1.1 Persistent excitation is non-negotiable

Åström & Wittenmark, *Adaptive Control*, 2nd ed., 1995, Ch. 2: you cannot identify a plant parameter that doesn't shape the data. If the regressor `u(t)` is zero, no estimator — RLS, MRAC, neural net — recovers `K_motor`. Ljung's *System Identification* (2nd ed., 1999) formalises this as the **persistent excitation (PE) condition of order n**: the regressor covariance must be bounded below by a positive-definite matrix over every finite window.

For an inverted pendulum this is a *gift*: the plant is unstable, so disturbances drive non-zero output. With any non-trivial controller closed around the loop, the resulting limit cycle (or small perturbation response) gives natural excitation. The Åström-Hägglund relay-feedback method (Åström & Hägglund, *PID Controllers*, 2nd ed., 1995) exploits exactly this — the controller *creates* excitation by switching, then measures `Ku` and `Pu` from the resulting oscillation. PE is not a blocker for a closed-loop balance bot.

### 1.2 Identifiability from IMU alone

Given only `gyro_xyz`, `accel_xyz`, and commanded PWM, the **observable** plant subspace is: the lumped scalar `K_motor = ∂α_pitch / ∂pwm_total` (slope of pitch acceleration vs PWM); the lumped scalar `g_eff = m·g·d / I` (gravitational restoring coefficient — offset when you regress `α_pitch` against `sin(pitch)`); the stiction band; direction asymmetry on combined torque; sensor mounting offset (already handled by `OnlineMountingEstimator`).

**Unobservable**: per-wheel torque asymmetry (both wheels enter the regressor identically — must inject yaw-producing differential and read `gyro_z`); wheel slip vs traction; absolute battery voltage (unless a divider is added).

Critical: mass, height, CoG, wheel radius, motor Kv, gear ratio **all collapse into `K_motor` and `g_eff`**. They are not individually observable, and don't need to be. The plant is parametrically a one-DoF rigid pendulum with one input gain, regardless of how many physical parameters produced it.

### 1.3 Four theoretical paths

**(a) Identification-based adaptive control (STR).** Åström & Wittenmark Ch. 5. Online RLS estimates plant parameters; fixed control law maps to gains. Requires PE. Stable when parameter space is convex and controller is locally robust.

**(b) Lyapunov-based adaptive (MRAC, direct adaptive).** Slotine & Li, *Applied Nonlinear Control*, 1991, Ch. 8; Ioannou & Sun, *Robust Adaptive Control*, 1996/2012, Ch. 6–7. Update law derived from a Lyapunov function so closed-loop signals stay bounded *by construction*, even before parameters converge. Doesn't require PE for stability — only for parameter convergence. Price: design depends on a plant parameterisation.

**(c) Model-free / data-driven (MFAC).** Hou & Jin, *Model Free Adaptive Control: Theory and Applications*, CRC Press, 2013 (the user's brief had the second author as "Wang"; it's Jin). Replaces the plant model with a local "pseudo-partial-derivative" estimated online from input-output data. Works under "generalised Lipschitz" conditions. Practical, theoretically lighter than (b).

**(d) Passivity-based control.** Ortega & Spong (1989); the IDA-PBC line through Ortega, Spong, Gómez-Estern, Blankenstein (*IEEE TAC* 47(8), 2002). Shapes closed-loop energy. Works *without* identification when the port-Hamiltonian structure is known. For inverted pendulums the structure *is* known — but the design requires solving a PDE offline and the resulting controller has parameters that still depend on mass/inertia. Shifts the problem rather than eliminating it.

For our purposes (a) is simplest, (b) is most rigorous, (c) is most pragmatic.

## 2. What "universal" actually means — bounds

"Universal" is meaningless without a scope. Size classes:

- **5 cm tabletop bot** — natural period ~150 ms; loop ≥200 Hz; sensor noise dominates.
- **30 cm bench-class bot** (target) — period ~500 ms; 100–200 Hz loop adequate.
- **2 m Segway-scale bot** — period several seconds; 50 Hz loop fine. But seed gains differ dramatically — Kp scales roughly with `√(I/g·d)`, spanning >10× across this range.

A single hardcoded seed cannot serve all three. The **adaptation law** *is* universal — once the controller is alive, RLS and gain mapping are dimensionless. The seed that gets the bot to "alive" is not.

- **Topologically universal**: PID + RLS + gain mapping architecture works for any inverted pendulum. Achievable.
- **Parametrically universal**: one binary boots on any size and just works. Achievable *only within a declared size class*.

The honest framework answer: declare the class (bench-scale: 0.1–1 kg, 0.1–1 m, 5–10 cm wheels, brushed DC + PWM driver), hardcode a seed conservative enough to survive any plant in the class, adapt from there.

## 3. The bootstrapping problem

### 3.1 Chicken-and-egg

PE requires motion. Motion requires a controller. A controller requires either a model or a seed. We have no model — so we need a seed.

How conservative is "safe across the class"? Åström-Hägglund gives a clean upper bound: any Kp ≤ ½·Ku preserves phase margin > 30°. For the bench class, `conservative_balance_gains_recommendation.md` (in-repo) suggests Kp ≈ 10–25, Kd ≈ 5–15, Ki = 0 starts every plant in the class without damage. The bot may wobble or drift, but won't lock motors at saturation.

### 3.2 Stability-preserving update laws

Slotine & Li Ch. 8 derives the canonical MRAC update law for second-order mechanical systems via a Lyapunov function `V = ½(e² + φ²/γ)`. The update `φ̇ = -γ·e·u` guarantees `V̇ ≤ 0` — error stays bounded for *any* γ, no matter how wrong the initial estimate. Adaptation cannot destabilise the loop by construction.

Ioannou & Sun (1996/2012) extends this to the *robust* case (unmodelled dynamics, disturbances). The σ-modification and projection operator force estimates to stay in a compact set, preventing "bursting" — the failure mode where a perfectly-tracking adaptive controller drifts when excitation dies. Anderson, "Failures of adaptive control theory and their resolution" (*Communications in Information and Systems* 5(1), 2005), surveys the cautionary tales.

Hovakimyan & Cao, *L1 Adaptive Control Theory* (SIAM, 2010), goes further: decouples adaptation rate from robustness via a low-pass filter on the control signal. Integrator gain can be as high as the CPU permits without sacrificing stability — exactly what a bootstrap wants. Catch: heavier architecture (state observer + adaptation + filter), and choosing the filter bandwidth implicitly requires knowing the unmodelled-dynamics bandwidth.

### 3.3 The practical compromise

For AVR-class hardware the literature converges on: **conservative seed + RLS plant ID + closed-form gain mapping + slow rate-limited application + projection to keep estimates in bounds**. Essentially the Åström-Wittenmark STR with σ-modification. Not the most sophisticated option (L1 adaptive is) — but the most-deployed in embedded control.

## 4. Published algorithms applied to self-balancing robots

- **PSO-tuned PID.** Kennedy & Eberhart (1995) introduced PSO. Hasanah & Alrijadjis, "Modified Particle Swarm Optimization Based PID for Movement Control of Two-Wheeled Balancing Robot" (Semantic Scholar); "Self-Balancing Robot Control Optimization Using PSO" (IEEE 2020, doc 9096470). All run PSO **offline** with a simulator, then flash gains. Useless for zero-config — the simulator requires the model.

- **GA-tuned PID.** "Genetic Algorithm-based Control of a Two-Wheeled Self-Balancing Robot," Springer *JINT*, 2025 (DOI 10.1007/s10846-025-02236-1). Same offline-simulator pattern, same limitation.

- **Reinforcement Learning on cart-pole.** Sutton & Barto, *Reinforcement Learning: An Introduction*, 2nd ed., 2018. OpenAI Gym cart-pole is the canonical toy. Real-hardware transfer: Soares Bellini et al., "A Q-learning approach to the continuous control problem of robot inverted pendulum balancing," *Results in Engineering*, 2023 (S2667305323001382). Sim-to-real gap requires domain randomisation, real-world fine-tuning, or both. Real-time inference on AVR is infeasible (a tiny MLP exceeds 30 µs/layer with no FPU). Plausible on Teensy 4.0; not Mega.

- **MRAC for inverted pendulum.** "Simple Direct MRAC of a Self-Balancing Robot mounted on a Ball," IEEE 2019 (doc 8884589) — ballbot. "Model Reference Adaptive Control of a Two-Wheeled Mobile Robot," ICRA 2019 (DOI 10.1109/ICRA.2019.8793633). Recent: "Fractional Gradient-Based MRAC Applied on an Inverted Pendulum-Cart System," 2025. Theoretically clean — but every implementation assumes the reference model is parameterised by the plant. Not fully zero-knowledge; "fewer-knowledge."

- **Self-tuning regulators.** Åström & Wittenmark Ch. 5 is canonical. Guzman, "The Self-Tuning Regulators Revisited" (*IFAC Proceedings*, S1474667017605342). Solid track record on bench robots.

- **L1 adaptive control.** Hovakimyan & Cao 2010. Wide UAV adoption; balance-bot applications emerging (arXiv 1901.07427). Not yet a default for cheap balance bots — architecture is complex relative to the platform.

- **MFAC.** Hou & Jin 2013. Applied to a moving-mass flying robot (Springer JINT 10.1007/s10846-024-02107-1). Direct balance-bot use is rare — the "pseudo-partial-derivative" estimator is itself an RLS, so practical complexity isn't much lower than a vanilla STR.

- **Neural-PID hybrid.** Multiple 2010s papers, uneven quality. Recurring problem: NN needs training data, meaning either a simulator (defeats zero-config) or in-situ training (slow, risky, hard to bound).

## 5. What needs per-bot calibration vs. what doesn't

**Requires one-time per-bot procedure** (cannot be auto-learned from IMU alone):

- Sensor calibration (gyro/accel/mag offsets) — existing wizard, persisted to EEPROM.
- Motor polarity (which wheel direction is "positive"). 2-second routine: spin each wheel at low PWM, check sign of `gyro_y`/`gyro_z`. Persist.
- Mounting offset (sensor body-frame ↔ chassis balance frame). Existing `MountingCalibration`, one button press.

**Learned from operating data** (seconds to minutes):

- `K_motor` — scalar RLS, ~5–30 s with conservative-PID excitation.
- `g_eff` — same regressor, scalar.
- Stiction band edges — histogram-binning, ~60 s.
- Direction asymmetry — sign-split RLS.
- Slow mounting drift — already handled by `OnlineMountingEstimator`.

**Fundamentally unobservable from IMU alone:**

- Per-wheel torque asymmetry — needs deliberate yaw-producing differential burst.
- Wheel slip / surface friction.
- Absolute battery voltage (unless a divider is added).

Per-bot setup is therefore: (1) IMU calibration (exists), (2) motor-polarity test (~2 s), (3) mount-offset capture (~2 s, exists). Everything else is learned online.

## 6. Recommendation — what to ship

### 6.1 Honest assessment

True zero-knowledge across all inverted pendulums is not achievable today. It is an open research problem (Anderson 2005 on adaptive bursting; ongoing L1-adaptive work on wide-class universality).

Zero-knowledge *within a declared bench-scale class* (0.1–1 kg, 0.1–1 m, 5–10 cm wheels, brushed DC + L298N-class driver) is achievable with off-the-shelf adaptive-control machinery, fits on AVR, well-supported by the literature.

### 6.2 The one algorithm

**Scalar Recursive Least-Squares plant ID + closed-form PD-from-Kmotor + rate-limited gain update + σ-modification projection.**

- **Plant ID.** RLS on `α_pitch = K_motor · pwm_total + g_eff · sin(pitch)`. Two scalar params, forgetting `λ ∈ [0.995, 0.999]`. Already designed in `dynamic_pwm_accel_learning.md` §4a. ~120 LOC, ~24 B RAM, ~30 µs/tick.
- **Gain mapping.** Once `K_motor` is identified to within ±20%: `Kp_eff = Kp_target / K_motor`, `Kd_eff = Kd_target / K_motor`. `Kp_target`, `Kd_target` are *dimensionless* control-authority parameters chosen once for the class (settling time = 4× natural period, damping = 0.7). Pole-placement, closed form (Åström & Hägglund §5).
- **Slew limit.** `d(Kp)/dt < 5 %/s`. Bad transient estimates can't destabilise the loop.
- **Projection (σ-modification).** Force `K_motor ∈ [K_min, K_max]`, class-wide bounds (e.g. `[0.5, 20] deg/s²/PWM`). Outside → freeze and alarm. Ioannou & Sun §8.
- **Seed.** Hardcoded `Kp=15, Ki=0, Kd=8`. Bot may drift modestly until estimator converges (~30 s upright).
- **Bootstrap signal.** No deliberate excitation needed; closed-loop disturbance noise + the operator-prop-and-release transient suffice. Optional 200 ms differential burst in IDLE for per-wheel asymmetry.

### 6.3 Complexity envelope (target: ATmega328P)

- LOC: ~300 across `plant_identifier.{h,cpp}` + `gain_mapper.{h,cpp}` + 50 lines in `pid_controller.cpp`.
- RAM: ~100 B. Flash: ~2 KB. CPU: ~80 µs/tick at 200 Hz → 1.6 % on a 16 MHz AVR.

Comfortably inside Mega budget; inside Uno budget too.

### 6.4 What it does not deliver

- No L1-class robustness guarantee. If the plant has an unmodelled high-frequency mode (flexible chassis) RLS can drift; projection limits but doesn't eliminate damage.
- No proof of stability across the entire declared class — a uniform Lyapunov function over class-wide parameter-varying plants is not yet in the literature. We rely on engineering margins and the projection bound.
- Per-wheel asymmetry only via the IDLE differential burst, which the framework must schedule deliberately.

### 6.5 Minimal per-bot operator setup

1. IMU calibration (exists, ~1 min, persisted).
2. Motor-polarity test (~2 s, automatic on first boot).
3. Mounting-offset capture (exists, ~2 s).

Complete. No mass entry, no length entry, no tuning sliders, no PID values in `config.h`.

### 6.6 Achievable today?

For the declared bench class on AVR: **yes, off-the-shelf, ~2 days of work** per `dynamic_pwm_accel_learning.md` §8.

For arbitrary inverted pendulums on arbitrary microcontrollers: **no — open research problem**. Closest contenders (L1 adaptive, IDA-PBC with online inertia estimation) are not yet drop-in firmware for this hardware class.

Recommendation: ship the RLS+gain-mapping algorithm under the declared class, document the class boundary as a known limitation, watch the L1-adaptive embedded literature over the next two-to-three years for the next jump.

## 7. References

- Åström, K. J. & Wittenmark, B. *Adaptive Control*, 2nd ed., Addison-Wesley, 1995.
- Åström, K. J. & Hägglund, T. *PID Controllers: Theory, Design, and Tuning*, 2nd ed., ISA, 1995.
- Ljung, L. *System Identification: Theory for the User*, 2nd ed., Prentice Hall, 1999.
- Slotine, J.-J. E. & Li, W. *Applied Nonlinear Control*, Prentice Hall, 1991.
- Ioannou, P. A. & Sun, J. *Robust Adaptive Control*, Prentice Hall, 1996; Dover reprint, 2012.
- Hovakimyan, N. & Cao, C. *L1 Adaptive Control Theory: Guaranteed Robustness with Fast Adaptation*, SIAM, 2010.
- Hou, Z. & Jin, S. *Model Free Adaptive Control: Theory and Applications*, CRC Press, 2013.
- Sutton, R. & Barto, A. *Reinforcement Learning: An Introduction*, 2nd ed., MIT Press, 2018.
- Ortega, R., Spong, M. W., Gómez-Estern, F. & Blankenstein, G. "Stabilization of a class of underactuated mechanical systems via interconnection and damping assignment," *IEEE Transactions on Automatic Control*, 47(8), 2002.
- Anderson, B. D. O. "Failures of adaptive control theory and their resolution," *Communications in Information and Systems*, 5(1), 2005.
- Kennedy, J. & Eberhart, R. "Particle swarm optimization," *Proc. IEEE Int. Conf. Neural Networks*, 1995.
- Hasanah, R. N. & Alrijadjis, D. "Modified Particle Swarm Optimization Based PID for Movement Control of Two-Wheeled Balancing Robot," Semantic Scholar.
- "Genetic Algorithm-based Control of a Two-Wheeled Self-Balancing Robot," *Journal of Intelligent & Robotic Systems*, Springer, 2025, DOI 10.1007/s10846-025-02236-1.
- "Simple Direct MRAC of a Self-Balancing Robot mounted on a Ball," IEEE, 2019, document 8884589.
- "Model Reference Adaptive Control of a Two-Wheeled Mobile Robot," ICRA 2019, DOI 10.1109/ICRA.2019.8793633.
- Soares Bellini et al., "A Q-learning approach to the continuous control problem of robot inverted pendulum balancing," *Results in Engineering*, 2023, ScienceDirect S2667305323001382.
- In-repo: `UNIVERSAL_BALANCE_BOT_VISION.md`, `AUTO_TUNING_REALITY_CHECK.md`, `findings/dynamic_pwm_accel_learning.md`, `findings/auto_pid_tuning_research.md`, `findings/online_adaptive_balance_tracking.md`, `findings/conservative_balance_gains_recommendation.md`.
