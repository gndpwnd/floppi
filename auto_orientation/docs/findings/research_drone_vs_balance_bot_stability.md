# Why a Drone Is Easier Than a Balance Bot
A cross-project comparison for the auto_orientation framework.
Last updated: 2026-05-12

---

## 1. The user's question

> "the /home/devel/floppi/flight_controller keeps the drone stable in the air without flipping over so how does it do its PID tuning? can we not use the same idea?"

The answer is "partly yes, mostly no," and the *why* matters more than the verdict.

---

## 2. The TL;DR

The drone is **open-loop neutrally stable** in attitude; the balance bot is **open-loop unstable**. That single property — the stability of the *uncontrolled* plant — dominates everything else. It is why dRehmFlight ships with hard-coded PID gains that most users never touch, while the balance bot in `auto_orientation/` has been chewing through bench sessions trying to find gains that survive thirty seconds upright.

At hover, gravity and lift cancel and small attitude errors do not produce torques that grow them. The balance bot fights gravity continuously, and any tilt produces a torque that makes the tilt *bigger*. Everything below is consequence.

---

## 3. The control-theory taxonomy

Three families of plant, distinguished by what happens with zero control input.

**3a. Open-loop stable.** Temperature controllers, motor velocity loops. With no input, the plant settles. PID adds tracking and disturbance rejection, but the plant survives controller failure. Gains have wide tolerance.

**3b. Open-loop neutrally stable.** A drone in attitude, a satellite in free space, a marble on a flat table. With no input the plant neither converges nor diverges — perturbations stay the same size. PID adds a restoring force that was never there in the physics. The system survives mediocre tuning by drifting slowly.

**3c. Open-loop unstable.** Inverted pendulum, balance bot, magnetic levitation. With no input, errors grow exponentially. Without control, the system fails in seconds. PID is *mandatory*, and the stabilising-gain window is narrow.

Slotine & Li ([Applied Nonlinear Control][slotine-li], Prentice Hall 1991) give the formal definitions in their Lyapunov chapters: a stable equilibrium has all eigenvalues with non-positive real parts; an unstable one has at least one with positive real part. The linearisation of an inverted pendulum about upright has a strictly positive eigenvalue, so the nonlinear equilibrium is unstable too. This is the mathematical reason the balance bot tips.

---

## 4. Why a drone is neutral, not unstable

Quadrotor attitude dynamics around hover are derived in [Bouabdallah, Murrieri & Siegwart (2004)][bouabdallah], "Design and Control of an Indoor Micro Quadrotor," ICRA. The linearised attitude equations of a symmetric quad about hover are approximately

    J φ̈ = τ_roll
    J θ̈ = τ_pitch
    J_z ψ̈ = τ_yaw

That is, **pure integrators**. No gravitational restoring term and no destabilising term. With zero differential thrust the angular acceleration is zero — small attitude perturbations sit where they were left.

Concrete intuition: hover a quad, cut motors, it falls straight down. It may spin, but there is no roll-angle-dependent torque preferentially tipping it. The system is the classical marginally-stable double integrator every textbook uses as the "simplest non-trivial plant." [Hoffmann et al. (Stanford GNC 2007)][stanford-quad] notes explicitly that around hover the linearised quad has *marginal stability* below a critical error angle.

Marginal stability is the easy regime. PID on a double integrator works trivially: P gives position feedback, D gives damping, I removes bias. Any positive-definite gain choice gives a stable closed loop. The dRehmFlight defaults are not magic numbers — they are almost any reasonable numbers.

---

## 5. Why a balance bot is unstable

Linearise an inverted pendulum about upright and the dynamics are

    J θ̈ = m·g·L·sin(θ) − τ_wheel ≈ m·g·L·θ − τ_wheel

The crucial sign: `+m·g·L·θ`. The gravitational torque acts *in the same direction* as the tilt. A 1° tilt produces a torque that increases the tilt. Errors grow exponentially with time constant `τ = √(J/(m·g·L))`.

For a bench bot with CoM height L ≈ 0.15 m, `τ = √(L/g) ≈ 0.12 s`. A 1° tilt becomes ~2.7° after 0.12 s, ~7° after 0.24 s, and the small-angle approximation breaks down inside half a second. This matches bench observations in `balance_failure_diagnosis_2026-05-12.md` — when gains are wrong the bot is on the floor before you can react.

Khalil's *Nonlinear Systems* (Prentice Hall, 3rd ed. 2002, Ch. 4) uses precisely this calculation as the canonical unstable equilibrium, and cart-pole treatments back to [Hauser, Sastry & Kokotović (1992)][hauser] all start from the same observation: the operating equilibrium is intrinsically unstable, not nearly-stable.

The drone has no equivalent of `m·g·L·θ`. Its hover equilibrium is a saddle in translation (x, y drift), but in attitude it is flat. That is the entire structural advantage.

---

## 6. Implications for PID tuning

This single sign difference (`+m·g·L·θ` vs `0`) propagates into every practical tuning decision.

**6a. Forgiveness of gain choice.** Drone gains tolerate factor-of-two errors. The Betaflight [PID Tuning Guide][betaflight] recommends P=4.0, I=20, D=5 on pitch/roll and notes the I-term has a "really wide tuning window" most users never change. For the balance bot the window is narrow and shifts with chassis parameters (mass, CoM height, battery state, tether tension). See `conservative_balance_gains_recommendation.md` and `midrange_balance_gains.md`.

**6b. Data acquisition is asymmetric.** A drone gathers flight data continuously regardless of gain quality — it stays airborne with mediocre tuning. A balance bot stops gathering data the moment it falls. The algorithms that learn good gains *need data the bad gains cannot produce*. This chicken-and-egg is catalogued in `docs/AUTO_TUNING_REALITY_CHECK.md`.

**6c. Latency sensitivity.** The drone's double-integrator is patient — 20-40 ms of sensor-to-actuator delay barely matters. The balance bot's fall constant is ~120 ms; 20 ms is one-sixth of a time constant, enough phase lag to destabilise gains that otherwise work. See `latency_budget_2026-05-12.md`.

**6d. Disturbance authority.** A quad's four motors can absorb a disturbance by redistributing thrust. The balance bot's two motors are friction-limited at the wheel; a sufficiently large kick simply tips it, because wheels skid before motors deliver the reaction torque. See `disturbance_compensation_research.md`.

---

## 7. Why dRehmFlight's PID approach works for drones

[dRehmFlight][drehm] (Nicholas Rehm) is a Teensy/Arduino flight controller used by thousands of hobbyists and as a teaching tool in universities. Its philosophy is "ship sane defaults; let the operator tweak via a desktop GUI." It works because:

1. Quad attitude dynamics are forgiving (Section 4).
2. Quads of a given size class share inertia-to-thrust ratios within roughly a factor of two. Two 5-inch FPV quads are dynamically similar in a way two balance bots are not.
3. The hobbyist community has extensively tuned defaults for the dominant size classes. Rehm's defaults stand on that prior art.
4. Mediocre gains still fly. The operator can hand-tune by reading flight logs without crashing. Each iteration is cheap.

Betaflight inherits the same property at industrial scale — its defaults work on a vast spectrum of hardware because the underlying plant is forgiving.

---

## 8. Why those same approaches fail for balance bots

1. **No equivalent community-tuned default class.** Balance bots are too heterogeneous (chassis height varies 5×, mass varies 10×, wheel diameter varies 4×, motor types vary wildly) and the gain window is too narrow for any single default to span the population. The Osoyoo defaults `research_osoyoo_reference_implementation.md` worked for *that one chassis*.
2. **Hand-tuning by observing crashes is impractical.** Each crash takes 10-30 seconds to recover, the bot may damage itself, and the operator's perception of "better" or "worse" gain is corrupted by stress. The drone's "spend an afternoon with Betaflight Configurator" loop has no equivalent.
3. **The "mediocre-but-flying" regime does not exist.** There is no analogue of a slightly-mistuned drone that still hovers passably. The balance bot either stays up or falls.

---

## 9. What CAN we copy from the drone world

The drone is not useless as a reference — large parts of its engineering carry over cleanly.

**9a. Sensor calibration philosophy.** Gyro bias auto-cal at boot with stillness-detect-and-restart (Betaflight pattern), accel offset cal at known orientations, mag soft-iron cal. The plant differences are irrelevant here; sensors are sensors. See `flight_controller/docs/findings/auto-calibration-research.md` and copy directly. The KI-1 bug in `calibration_storage.cpp` is a regression we should fix using the FC's prior art.

**9b. Cascade architecture.** Rate loop inside angle loop. Both projects benefit from the same decomposition. Covered in `findings/research_flight_controller_pid_lessons.md`.

**9c. Anti-windup and integral clamping.** Identical mathematics across domains. Steal directly.

**9d. Operator UX patterns.** `s` for status, simple serial commands, EEPROM persistence for *calibration constants* (not for gains — see Section 11). These are about ergonomics, not control theory, and the FC has the better-developed conventions.

---

## 10. What we CAN'T copy

**10a. The trim-hover bootstrap.** A drone at hover throttle floats. A balance bot at "trim PWM" does not sit still — there is no steady-state for an unstable plant. The whole concept of "set it down at trim and let it learn from there" does not exist.

**10b. Hard-coded gain assumption.** Drone gains generalise within a size class. Balance-bot gains do not generalise as broadly, because chassis variation produces larger relative changes in the unstable dynamics than in the marginally-stable dynamics.

**10c. Multi-axis decoupled control.** Drone roll/pitch/yaw decouple cleanly under linearisation. Balance bot is single-axis (pitch) but the dynamics couple to wheel speed — and we typically do not measure wheel speed. The drone has *more* axes but *easier* coupling.

**10d. The mixer.** Quad mixer maps `(thrust, roll, pitch, yaw)` → 4 motors. Balance bot is a trivial left/right split. The mixer is the drone's hard part, and ours is the wrong shape to inherit it.

---

## 11. The genuinely hard part for the balance bot

Once we accept that hand-tune-and-forget will not work, the question becomes: *how does the bot stay upright long enough for an online learner to converge?* The literature offers three answers.

**Conservative seed gains + slow online adaptation.** Start with a defensible static gain for the chassis class (`conservative_balance_gains_recommendation.md`), then use recursive least squares or MRAC to refine in real time, with safety bounds keeping the adapter inside the stabilising window. This is Phase 4.10 in `dynamic_pwm_accel_learning.md`. The theoretical underpinning is [Åström & Wittenmark's *Adaptive Control*][astrom-wittenmark] (1995), whose thesis is that for an unstable plant you must combine a stabilising controller with an estimator that runs *inside* the stabilised loop.

**Operator-assisted bootstrap.** Operator holds the bot upright while motors apply known PWM excitation. From the wheel response we identify the plant, then deploy with model-based gains. [Åström & Hägglund's relay feedback (1984)][astrom-hagglund] is the stable-plant variant; for unstable plants the operator-hold supplies the human-in-the-loop stabilisation the controller will eventually take over.

**Sim-to-real with deep RL.** Train in Gazebo/PyBullet, transfer to hardware. Out of scope for AVR; [Hou & Jin (2013)][hou-jin] surveys the model-free family, and MFAC variants may fit on ESP32.

The bench-relevant choice is option 1. The framework's job is making it actually work: defensible seed, safe adapter, and the discipline to admit when it has not converged.

---

## 12. Recommendations

For the balance-bot half of `auto_orientation/`:

1. **Do not try to be the drone.** The drone's "ship defaults, let the operator tweak" pipeline does not translate. Phase 4.10's RLS auto-tune is the structurally correct answer. Stop comparing the bench bot to dRehmFlight's smooth defaults — the plants are different.

2. **Steal the calibration hygiene.** Implement Betaflight-pattern boot-time gyro cal with stillness-detect-and-restart. Even when tuning is harder, calibration is identical, and `flight_controller/` has the reference implementation.

3. **State the asymmetry openly in framework docs.** A user expecting drone-like simplicity will be frustrated by balance-bot fragility; the README and `getting_started/` should say up front: "balance bots are intrinsically harder than drones, here is why, here is what we do about it."

4. **Pick a size class and stop pretending universality.** "Bench-scale inverted pendulum, 0.2-1.0 kg / 0.1-0.3 m CoM height" is defensible; "any balance bot" is not. dRehmFlight is universal-within-class, not universal-across-classes — we should aim for the same honest scope.

---

## 13. References

[bouabdallah]: https://www.research-collection.ethz.ch/handle/20.500.11850/82527
[drehm]: https://github.com/nickrehm/dRehmFlight
[slotine-li]: https://lewisgroup.uta.edu/ee5323/notes/Slotine%20and%20Li%20applied%20nonlinear%20control-%20bad%20quality.pdf
[khalil]: https://en.wikipedia.org/wiki/Inverted_pendulum
[hauser]: https://www.scirp.org/reference/referencespapers?referenceid=2068719
[astrom-wittenmark]: https://portal.research.lu.se/en/publications/adaptive-control-2-ed/
[astrom-hagglund]: https://www.sciencedirect.com/science/article/abs/pii/S0959152401000257
[hou-jin]: https://www.scirp.org/reference/referencespapers?referenceid=2462065
[betaflight]: https://betaflight.com/docs/wiki/guides/current/PID-Tuning-Guide
[stanford-quad]: https://ai.stanford.edu/~gabeh/papers/Quadrotor_Dynamics_GNC07.pdf

- [Bouabdallah, S., Murrieri, P., & Siegwart, R. (2004) — Design and Control of an Indoor Micro Quadrotor, IEEE ICRA New Orleans, ETH Research Collection][bouabdallah]
- [Rehm, N. — dRehmFlight, Teensy/Arduino flight controller and stabilisation for small-scale VTOL][drehm]
- [Slotine, J.-J. E., & Li, W. (1991) — Applied Nonlinear Control, Prentice Hall, Ch. 3-4 (Lyapunov theory)][slotine-li]
- [Khalil, H. K. (2002) — Nonlinear Systems (3rd ed.), Prentice Hall, Ch. 4 inverted pendulum example][khalil]
- [Hauser, J., Sastry, S., & Kokotović, P. (1992) — Nonlinear Control via Approximate Input-Output Linearization, IEEE TAC 37(3) 392-398][hauser]
- [Åström, K. J., & Wittenmark, B. (1995) — Adaptive Control (2nd ed.), Addison-Wesley, self-tuning regulators and MRAC][astrom-wittenmark]
- [Åström, K. J., & Hägglund, T. (1984) — Automatic Tuning of Simple Regulators, Automatica 20(5) 645-651; tutorial review by Hang, Åström, Wang (2002)][astrom-hagglund]
- [Hou, Z., & Jin, S. (2013) — Model Free Adaptive Control: Theory and Applications, CRC Press][hou-jin]
- [Betaflight Project — PID Tuning Guide][betaflight]
- [Hoffmann, G. M., et al. (2007) — Quadrotor Helicopter Flight Dynamics and Control: Theory and Experiment, AIAA GNC][stanford-quad]

Cross-project sibling documents:

- `/home/devel/floppi/auto_orientation/docs/findings/research_flight_controller_pid_lessons.md`
- `/home/devel/floppi/auto_orientation/docs/findings/latency_budget_2026-05-12.md`
- `/home/devel/floppi/auto_orientation/docs/findings/dynamic_pwm_accel_learning.md`
- `/home/devel/floppi/auto_orientation/docs/findings/conservative_balance_gains_recommendation.md`
- `/home/devel/floppi/auto_orientation/docs/findings/balance_failure_diagnosis_2026-05-12.md`
- `/home/devel/floppi/auto_orientation/docs/AUTO_TUNING_REALITY_CHECK.md`
- `/home/devel/floppi/flight_controller/docs/findings/auto-calibration-research.md`
