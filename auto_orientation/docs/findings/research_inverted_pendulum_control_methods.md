# Inverted Pendulum Control Methods — An Opinionated Survey

Status: Research note. Targeted at one decision: which controller architecture to commit to for the universal balancing-robot framework.
Last updated: 2026-05-12

## Framing

Target plant: two-wheeled inverted pendulum. Target hardware: Arduino Uno (ATmega328P, 2 KB SRAM, 32 KB flash, no FPU), BNO055 IMU, L298N, cheap brushed DC motors, 200 Hz control rate. Target outcome: drop the same firmware on any chassis in this class and have it balance after a brief self-identification phase. No per-bot magic numbers.

Two hard constraints knock out most of the literature: (1) the plant is open-loop unstable, so no controller can ever be "off" during identification; (2) 2 KB of RAM and no FPU make almost every state-space method that the textbooks recommend either painful or impossible. Methods are judged on theoretical merit, AVR feasibility, prior-knowledge requirement, and whether they survive the "universal" goal in `UNIVERSAL_BALANCE_BOT_VISION.md`. I will be opinionated.

---

## 1. Classical PID with hand-tuned gains

Three scalar gains on error, integral, derivative. The current baseline.

Seminal: Minorsky, N. (1922). "Directional stability of automatically steered bodies." Journal of the American Society of Naval Engineers, 34(2), 280–309 — first rigorous PID treatment for an unstable plant (a ship's heading).

Prior knowledge: none analytically, but the operator must hand-tune three numbers per chassis. AVR cost: ~30 LOC, <20 B RAM, <10 µs per tick. Trivial. Universal? No. Kp=65/Ki=12/Kd=38 for the BNO055 Mega bot in `docs/archive/balancing_robot_reference/` does not transfer to a Uno with cheap motors. The gains encode an implicit linear plant model, and that model is per-bot.

Recommendation: keep as the inner loop. Replace the *tuning step* with something automated. Every method below either tunes PID online or layers around it.

---

## 2. Auto-tuned PID via relay feedback (Åström–Hägglund)

Wrap a relay around the error signal, measure the resulting limit-cycle period `T_u` and amplitude `a`, compute `K_u = 4d/(πa)`, then apply Ziegler-Nichols or AMIGO formulae.

Seminal: Åström, K. J. and Hägglund, T. (1984). "Automatic tuning of simple regulators with specifications on phase and amplitude margins." Automatica, 20(5), 645–651. AMIGO refinement: Hägglund and Åström (2002), Journal of Process Control, 14(6), 635–650.

Prior knowledge: none — relay-amplitude `d` is the only knob. AVR cost: ~600–900 B flash, ~80 B RAM. Already implemented in this repo as `AutoPIDTuner` with `RelayFeedbackStrategy` (Phase 4.5b).

Universal? Yes — *if* you can reach the start condition. Chicken-and-egg: relay feedback REQUIRES the bot to already be balancing within ±10°. It is a refinement step, not a cold-start tuner. `AUTO_TUNING_REALITY_CHECK.md` walks through this: an inverted pendulum is open-loop unstable, so no algorithm produces gains "from scratch" without something keeping the bot alive during the experiment. Bootstrap path: ship conservative seed gains (current Kp=15/Ki=0/Kd=8 is reasonable), let `OnlineMountingEstimator` (Phase 4.4) settle the offset, then trigger the relay tuner. The seed gains are the universality leak; they are bot-size-dependent.

Recommendation: keep. Correct, cheap, already built. Not, by itself, the answer.

---

## 3. Linear Quadratic Regulator (LQR)

Solve the algebraic Riccati equation for the linearised plant `ẋ = Ax + Bu` to get optimal state-feedback gain `K = R⁻¹Bᵀ P`. The textbook cart-pole controller (Ogata, *Modern Control Engineering* 5th ed., 2010, ch. 10).

Seminal: Kalman, R. E. (1960). "Contributions to the theory of optimal control." Boletín de la Sociedad Matemática Mexicana, 5, 102–119.

Prior knowledge: full linearised dynamics — `M`, `m`, `l`, `I`, `g`, plus motor gain. The whole model. Plus weight matrices `Q` and `R` that encode "what does optimal even mean for this bot." AVR cost: Riccati solver offline, gains baked into flash — trivial runtime cost. Solving Riccati on a Uno is out.

Universal? Categorically not. You cannot use LQR without knowing the plant. Recommendation: no by itself. Useful as the *output* of an online system-identification stage (the Phase 4.10 RLS step) — then LQR-on-the-fly becomes the gain-design routine.

## 4. Pole placement

Choose desired closed-loop poles, compute state-feedback gain `K` analytically via Ackermann's formula.

Seminal: Ackermann, J. (1972). "Der Entwurf linearer Regelungssysteme im Zustandsraum." Regelungstechnik und Prozessdatenverarbeitung, 20, 297–300.

Same prior-knowledge requirement as LQR (full plant), same AVR feasibility (offline gain computation), same universality verdict (no). Both are downstream of system identification, not replacements for it. Recommendation: no.

---

## 5. Model Reference Adaptive Control (MRAC)

Define a reference model that produces the desired closed-loop response. The controller adapts its gains online so the plant tracks the reference. Lyapunov-stable variants guarantee bounded error.

Seminal: Whitaker, H. P., Yamron, J. and Kezer, A. (1958). "Design of model-reference adaptive control systems for aircraft." MIT Instrumentation Laboratory Report R-164 (the MIT rule). Modern treatment: Åström, K. J. and Wittenmark, B. (1995). *Adaptive Control*, 2nd ed., Addison-Wesley, ch. 5.

Prior knowledge: a reference model. If generic (critically-damped second-order, ω_n = 3 rad/s, ζ = 0.7), no per-bot info. AVR cost: three scalar update equations for three PID gains. ~150–200 B RAM, ~1.5 KB flash. Tractable.

Universal? Partially. The hard part is proving stability when the reference model bandwidth exceeds what the hardware can deliver — MRAC can chase an impossible reference and wind up unstable. Well-known robustness pitfalls with unmodeled high-frequency dynamics (Rohrs et al. 1985). Recommendation: maybe, but the self-tuning regulator (§8) is simpler and more diagnosable.

## 6. L1 adaptive control

Decouples adaptation speed from robustness via a low-pass filter on the control signal. Allows arbitrarily fast adaptation without sacrificing stability margins. Designed for systems with parametric uncertainty — exactly the universal-bot case.

Seminal: Hovakimyan, N. and Cao, C. (2010). *L1 Adaptive Control Theory: Guaranteed Robustness with Fast Adaptation*. SIAM Advances in Design and Control, vol. 21. Earlier: Cao, C. and Hovakimyan, N. (2008). "Design and analysis of a novel L1 adaptive control architecture with guaranteed transient performance." IEEE Trans. Autom. Control, 53(2), 586–591.

Prior knowledge: reference model + low-pass filter bandwidth. AVR cost: ~400 B RAM, ~3–5 KB flash. Real but tight on a Uno.

Universal? Strong theoretical case. L1's "guaranteed transient performance" is exactly what you want when you do not know the plant. The challenge is implementation — most L1 demonstrations use full state-space representations. Recommendation: maybe, high-effort. The right answer if you have a Teensy 4.0 / ESP32 and want bullet-proof transient guarantees. On a Uno, over-engineered.

## 7. Sliding-mode control (SMC)

Define a sliding surface `s(x) = 0` (e.g., `s = ė + λe`). Switch the control signal so the state is driven onto the surface and slides along it. Robust to bounded uncertainty by construction.

Seminal: Utkin, V. I. (1977). "Variable structure systems with sliding modes." IEEE Trans. Autom. Control, AC-22(2), 212–222.

Prior knowledge: surface design needs the relative degree (2 for balancing) and a bound on the disturbance. AVR cost: trivial — a switching law on a linear combination of pitch and pitch-rate. ~50 B RAM, <1 KB flash.

Universal? Yes in principle. But chattering is real and gets worse with discrete brushed DC motors. The standard mitigation (boundary-layer smoothing, or higher-order sliding modes — Levant, 2003) reintroduces a free parameter that is *itself* per-bot. Chattering with an L298N + cheap motors will fry brushes. Recommendation: no — no advantage over a self-tuning PID for this hardware class.

---

## 8. Self-tuning regulator (STR)

Recursive least-squares (RLS) online to identify plant parameters. Periodically recompute controller gains from the current estimates via pole placement, LQR, or minimum-variance design.

Seminal: Åström, K. J. and Wittenmark, B. (1973). "On self-tuning regulators." Automatica, 9(2), 185–199. Textbook: Åström and Wittenmark (1995). *Adaptive Control*, 2nd ed., Addison-Wesley, ch. 3 (RLS) and ch. 5 (STR).

Prior knowledge: model structure (linear ARX, order), not parameters. Phase 4.10 picks order 1: `α_pitch = K_motor · u + (m·g·d/I) · sin(pitch)`, learning `K_motor`. AVR cost: scalar RLS is three multiply-adds per tick (~30 µs at 16 MHz), ~12 B state. Gain recompute at 1 Hz — one division. Under 200 B RAM and ~2 KB flash.

Universal? Yes. K_motor and g_eff collapse every per-bot dimension (mass, length, CoG, motor characteristics, battery state) into two scalars learned from operating data. The bot drifts; the estimator tracks it; the gains follow. Generalises cleanly: a Teensy/ESP32 build extends the regressor to a 2nd-order ARX without changing the framework.

Recommendation: **yes — this is the architecture to commit to.**

---

## 9. Lyapunov-based adaptive PID

Derive PID-gain update laws directly from a Lyapunov function `V(e, θ̃)` so `V̇ ≤ 0` is guaranteed. Provably stable while learning.

Seminal: Slotine, J.-J. E. and Li, W. (1991). *Applied Nonlinear Control*, Prentice-Hall, ch. 8. Also Sastry, S. and Bodson, M. (1989). *Adaptive Control: Stability, Convergence, and Robustness*, Prentice-Hall.

Prior knowledge: a Lyapunov function (`V = ½e² + ½γ⁻¹θ̃²` is standard) plus plant structure (relative degree, sign of gain). AVR cost: ~200 B RAM, ~2 KB flash. Universal? Yes, with caveats — proofs assume matching conditions that real bots violate in small ways. Recommendation: maybe. Mathematically the most satisfying. Practically harder to debug than STR — when it misbehaves, there is no obvious knob to twist; STR's RLS estimator is more diagnosable (you can print `K_motor` and see if it converged).

## 10. Iterative learning control (ILC)

Improve performance over repeated executions of the same trajectory. Iteration `k+1` uses the error from iteration `k` as feed-forward.

Seminal: Arimoto, S., Kawamura, S. and Miyazaki, F. (1984). "Bettering operation of robots by learning." Journal of Robotic Systems, 1(2), 123–140.

Recommendation: no. Balancing has no repeated trial structure — disturbances come at random times. ILC is right for pick-and-place arms, wafer steppers, batch processes. Dismissed.

## 11. Reinforcement learning (DQN, PPO, SAC) for cart-pole

Train a policy network from interaction with a simulator (or the real plant) to maximize "time upright."

Seminal: DQN — Mnih, V. et al. (2015). "Human-level control through deep reinforcement learning." Nature, 518(7540), 529–533. PPO — Schulman, J. et al. (2017). "Proximal policy optimization algorithms." arXiv:1707.06347. SAC — Haarnoja, T. et al. (2018). "Soft actor-critic." ICML 2018.

Prior knowledge: a simulator accurate enough that the policy transfers (the sim-to-real gap). AVR cost: inference of a small policy could fit (~1 KB flash); training cannot. The sim-to-real gap requires domain randomization (Tobin et al. 2017) plus on-robot fine-tuning. The trained policy is opaque — battery sag changes the plant and the policy cannot react. Recommendation: no for the AVR-deployable universal goal. Maybe for a Teensy/ESP32 *with* a simulator pipeline; even then STR is more diagnosable.

## 12. Fuzzy logic controllers

Linguistic IF-THEN rules over fuzzy sets (e.g., "if pitch is positive-small and pitch-rate is negative, output is zero").

Seminal: Mamdani, E. H. and Assilian, S. (1975). "An experiment in linguistic synthesis with a fuzzy logic controller." International Journal of Man-Machine Studies, 7(1), 1–13 — the steam-engine paper.

The rule base IS the per-bot calibration, dressed in linguistic clothing. "Pitch is positive-small" requires defining what "small" means, and that definition is bot-specific. Popular in academic demos because they make memorable lectures, not because they generalise. Recommendation: no.

## 13. Genetic algorithm / particle-swarm PID tuning

Run many trial gain sets, score each, evolve the population.

Seminal: Holland, J. H. (1975). *Adaptation in Natural and Artificial Systems*. Applied to PID: Krohling, R. A. and Rey, J. P. (2001). "Design of optimal disturbance rejection PID controllers using genetic algorithms." IEEE Trans. Evol. Comput., 5(1), 78–82.

Prior knowledge: a simulator or a bench that survives many falls. Iteration time on real hardware is the killer (each fall costs 30+ seconds of operator intervention). If you have a simulator, RL is the modern, better-tooled answer. Recommendation: no.

## 14. Direct neural-network policy

Train a small MLP to map state to action. Deploy the weights.

Seminal: Lillicrap, T. P. et al. (2016). "Continuous control with deep reinforcement learning." ICLR 2016 (DDPG).

A 4-layer MLP with 16 hidden units fits in ~2 KB flash with int8 quantization. Universal only if trained on a randomized family. No online adaptation — same opacity problem as §11. Recommendation: no for this hardware. Maybe as a Phase-N research direction on Teensy/ESP32.

---

## Verdict — ranked recommendation

**Single best recommendation:** implement the **Self-Tuning Regulator (STR)** with scalar RLS plant identification, keep the existing relay-feedback auto-tuner as a one-shot refinement step, and keep PID as the inner law. This is exactly the Phase 4.10 design in `dynamic_pwm_accel_learning.md`. The math collapses every per-bot dimension into two scalars (`K_motor`, `g_eff`) that an RLS estimator learns from IMU and PWM data alone — no encoders, no current sense, no chassis specs. It fits in 200 B of SRAM and 2 KB of flash. It is provably stable under standard adaptive-control assumptions. It tracks battery sag, payload, and wear automatically. And it is diagnosable: a live `K_motor` estimate is a free hardware-fault indicator. The relay-feedback tuner (Phase 4.5b) becomes a refinement step that runs once the bot is balancing on STR-derived gains.

**If you only have AVR-class hardware and want universal:** implement STR (method 8). PID-inner + scalar RLS + slow gain recomputation.

**If you have Teensy/ESP32-class hardware later:** L1 adaptive control (method 6) becomes feasible — its guaranteed-transient-performance theorem is exactly the assurance you want when you cannot pre-characterize the plant. Lyapunov-based adaptive PID (method 9) is the runner-up.

**For someone willing to write a Python simulator and offload tuning:** PPO (method 11) trained on a domain-randomized family of inverted-pendulum dynamics, deployed as a frozen policy on Teensy/ESP32. Skip the AVR target — there is no point quantizing an MLP for a Uno when a Teensy 4.0 costs $20.

## The most surprising thing

How *little* the academic literature actually addresses the universality problem. Every textbook treatment of the inverted pendulum begins with "let the cart mass be M, the pendulum mass be m, the length be l" — i.e., assumes the plant is known. The cart-pole *is* the canonical benchmark, but it is invariably the *known* cart-pole. The few methods that explicitly address parametric uncertainty (L1, MRAC, STR) are presented as advanced topics, often with state-space formulations far heavier than a 2 KB MCU can carry. The "universal balancing bot" problem is a moderately exotic intersection of adaptive control and embedded systems — and the simplest answer (scalar RLS plus a switched controller) turns out to be a 1973 paper.

## See also

- [auto_pid_tuning_research.md](auto_pid_tuning_research.md) — relay-feedback tuner already in the repo.
- [dynamic_pwm_accel_learning.md](dynamic_pwm_accel_learning.md) — Phase 4.10 STR design (the recommendation above).
- [../AUTO_TUNING_REALITY_CHECK.md](../AUTO_TUNING_REALITY_CHECK.md) — why no algorithm tunes an unstable plant in one shot.
- [../UNIVERSAL_BALANCE_BOT_VISION.md](../UNIVERSAL_BALANCE_BOT_VISION.md) — the design goal this survey serves.
- [../MINIMIZE_ACCELERATIONS_PHILOSOPHY.md](../MINIMIZE_ACCELERATIONS_PHILOSOPHY.md) — controller-architecture companion.
