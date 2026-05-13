# Multi-Orientation Self-Balancing — Feasibility Analysis

Status: research / proposal. Maps to a future Phase 4.11+ (Level 2) and a hypothetical Phase 5.x / 6.x (Levels 1, 3).
Last updated: 2026-05-12

Rigorous sibling to [`../MULTI_ORIENTATION_BALANCE_VISION.md`](../MULTI_ORIENTATION_BALANCE_VISION.md). The vision names three "levels" of multi-orientation balance. This doc asks: which are buildable on the bench bot (or framework's MCU target list generally), and in what order.

## 1. Scope

From the vision, the three levels:

- **Level 2 — Arbitrary-orientation single-axis balance.** Same two wheels. IMU/chassis mountable any way up. Firmware figures out the wheel axis and balances about it.
- **Level 1 — Rollover-aware axis swap.** Multi-face chassis. Bot tips beyond recovery on one face → switches to the next face's controller and motor pair.
- **Level 3 — Omnidirectional ("ballbot").** Robot stands on a single contact point with two unstable DOFs and a 3-vector control input.

(The vision orders them by ambition; this analysis re-orders by build feasibility — Level 2 first.)

"Feasible" here means four axes: **firmware effort** (LOC, flash, RAM, CPU; anchored to `arduino_uno_balancing` — ATmega328P, 2 KB SRAM, 32 KB flash, no FPU, 16 MHz — and the broader MCU list Mega/Nano/Teensy/ESP32/ESP32-S3); **hardware effort** (motors, drivers, chassis); **algorithmic effort** (new control theory beyond PID + relay tuning + scalar RLS — see [`research_inverted_pendulum_control_methods.md`](research_inverted_pendulum_control_methods.md)); and **per-bot calibration burden** vs. the [`UNIVERSAL_BALANCE_BOT_VISION.md`](../UNIVERSAL_BALANCE_BOT_VISION.md) north star.

## 2. Level 2 — Arbitrary-orientation single-axis balance (firmware-only)

### 2a. The math

Orientation is already a unit quaternion `q_world_to_body`. The low-passed accelerometer measures gravity in body frame: `ĝ_body = -a/|a|`. Define a balance frame:

- `ẑ_balance = -ĝ_body` (direction the chassis leans toward).
- `x̂_balance` — orthogonal to `ẑ_balance` along the wheel-hub axis (the per-bot unknown, see §2c).
- `ŷ_balance = ẑ_balance × x̂_balance` — the rotation axis the motors can actually drive.

The body→balance transform is a fixed quaternion `q_mount` captured once at boot (or at calibration) and stored alongside the existing 22-byte BNO055 cal blob.

The tilt error becomes a scalar that does not break at 90°:

`θ_tilt = atan2( (R(q) · ẑ_body) · x̂_balance,  (R(q) · ẑ_body) · ẑ_world )`

This is the same math `AHRS` uses for accel-only tilt initialization (see references). Cost: two quat-by-vector rotations (~30 flops), one dot product, one `atan2` (~80 cycles soft-float). Under 200 µs at 16 MHz — well inside the 5 ms / 200 Hz budget. The framework already has `rotate_vector_by_quaternion()` in [`src/math/quaternion_conversions.cpp`](../../src/math/quaternion_conversions.cpp); no new primitives.

The gyro side: project body gyro onto `ŷ_balance` — one dot product. `ω_pitch = ω_body · ŷ_balance`.

### 2b. Plumbing changes

- [`src/sensors/sensor_base.h`](../../src/sensors/sensor_base.h) — already exposes raw gyro/accel + quaternion. No change.
- **New** `src/control/balance_frame.{h,cpp}` — owns `q_mount`, produces `θ_tilt` and `ω_pitch` each tick. ~200 LOC, ~60 B RAM.
- **New** boot-time gravity-capture routine in `src/applications/balancing_robot/balance_app.cpp` — 200 ms accel average → unit gravity → quaternion offset. Reuses the gyro-stillness gate from `MountingCalibration` (Phase 4.3, D5 in [MASTER_DESIGN.md](MASTER_DESIGN.md)).
- PID, relay-feedback tuner, `OnlineMountingEstimator`, and the Phase 4.10 scalar-RLS plant identifier all consume scalar tilt + scalar gyro and are **unchanged** — they see the same scalars in a different frame.

Total: ~250 LOC, one compile flag (`USE_BALANCE_FRAME`).

### 2c. What's hard — the wheel-axis problem

Gravity tells the bot "down". It does NOT tell the bot which body-frame axis is the wheel axis. That is a per-bot mounting property with three resolution options:

1. **Hard-coded.** Operator sets `WHEEL_AXIS_BODY = X` (or Y). Trivial. Defeats universality slightly but is honest about what the user actually knows.
2. **Motor-pulse probe.** Apply a brief differential PWM step; whichever body axis spikes on the gyro is the wheel axis (sign disambiguated by a second pulse). Universal, but requires the bot on its wheels with live motors during calibration. Adds a tetherless step (button press → "place on wheels and hold still" → 1 s probe → save).
3. **Magnetometer compass heading.** Only works if the wheel axis is aligned with a magnetic-frame-fixed direction — generally not true. Skip.

**Ship (1) as the v1 default; add (2) as an opt-in once Level 2 is debugged.** A two-byte addition to the EEPROM mount-blob (axis index + sign) covers both. The gyro-probe approach is closely related to how the [Cubli](https://idsc.ethz.ch/research-dandrea/research-projects/archive/cubli.html) team calibrates its reaction-wheel axes.

### 2d. Verdict for Level 2

| Axis | Score |
| --- | --- |
| Firmware effort | Low. ~250 LOC, ~60 B RAM, negligible CPU. |
| Hardware effort | Zero. Existing Uno + BNO055 + L298N + two motors. |
| Algorithmic effort | Zero. Reuses scalar-PID + RLS + relay-tuner stack. |
| Per-bot burden | Same as today (one-time mount capture). |

**Feasibility: high. Universality: high. Estimated effort: ~1 work-week.** Recommended **after Phase 4.10 (universal auto-tune)**. The two changes are orthogonal — 4.10 generalizes in the *scalar plant* dimension, 4.11 in the *body-frame orientation* dimension — but 4.10 unblocks more value first because it removes per-bot PID gains.

## 3. Level 1 — Rollover-aware axis swap (firmware + hardware)

### 3a. The hardware reality

Most balance bots have one preferred orientation; mass distribution makes only one face balanceable. "Fall onto a side and keep balancing" requires a chassis with **multiple balance-stable configurations**:

- **Multi-face wheels.** Two pairs mounted 90° apart on adjacent faces. Mass distribution must accommodate both pairs.
- **Reaction-wheel cube.** ETH's [Cubli](https://idsc.ethz.ch/research-dandrea/research-projects/archive/cubli.html) (Gajamohan et al., ECC 2013) balances on a corner via three reaction wheels; the 2023 [One-Wheel Cubli](https://ethz.ch/en/news-and-events/eth-news/news/2023/03/one-wheel-cubli-with-a-single-reaction-wheel.html) does it with one wheel + asymmetric inertia. Purpose-built single-point balancers, not "fall survivors".
- **Symmetric cube robot.** Every face identically actuated. Rare; research demos.

The L298N has two H-bridges. A multi-face bot needs an extra driver, chassis redesign for side wheels, and power-distribution rework.

### 3b. The control problem

Given the hardware, the firmware is Level 2 + a mux:

- Each face's balance configuration is its own `BalanceFrame` (quaternion offset + wheel-axis + motor-pair mapping).
- Active configuration = `argmax` over per-face dot products of `ẑ_face` with world-up, plus hysteresis (require >300 ms dominance and the previous face's tilt > ~60° before switching).
- Motor mux: lookup table `(active_face) → (pwm_pins)`. The motor driver layer ([Phase 4.7](MASTER_DESIGN.md#47--self-balancing-robot-reference-application)) already abstracts pin assignment.

~150 LOC on top of Level 2 — but **dead code without the chassis**.

### 3c. Verdict for Level 1

| Axis | Score |
| --- | --- |
| Firmware effort | Low. ~150 LOC on top of Level 2. |
| Hardware effort | High. Chassis redesign + 2× motors + extra H-bridge. |
| Algorithmic effort | Low. Same scalar PID per face. |
| Per-bot burden | Higher. Each face needs its own `q_mount` (and optionally its own auto-tune). |

**Feasibility: medium, gated entirely by mechanical design effort.** Defer until someone builds the chassis. Don't speculatively pre-build the firmware mux. But **do** keep `BalanceFrame` a runtime struct (not a compile-time singleton) so a future build can carry a `BalanceFrame[N_FACES]` array — that's a free design choice today.

## 4. Level 3 — Ballbot (omnidirectional 3D balance)

### 4a. The state of the art

The seminal academic reference is CMU's Ballbot — Lauwers, Kantor, and Hollis (2006), Proc. IEEE ICRA 2006, pp. 2884–2889 — and its mature journal version Nagarajan, Kantor, and Hollis (2014), *IJRR* 33(6), 917–930. Adjacent work: ETH Zürich's [Rezero](http://rezero.ethz.ch/) (2010, three 120° omniwheels, 160 Hz LQR, 3.5 m/s, 17° lean); [Kugle](https://www.research-collection.ethz.ch/bitstream/handle/20.500.11850/154271/eth-7943-01.pdf) (open-source quaternion non-linear ballbot model demonstrating LQR, sliding-mode, and ACADO MPC); [Honda U3-X](https://en.wikipedia.org/wiki/Honda_U3-X) (2009 omnidirectional inline-drum prototype, control internals unpublished); ETH's [Cubli](https://idsc.ethz.ch/research-dandrea/research-projects/archive/cubli.html) (corner-balance via reaction wheels, same "two unstable DOFs MIMO" structure).

Across the literature the picture is uniform: **LQR around the linearized upright equilibrium**, often cascaded with an outer-loop velocity / position MPC. Per-axis PID works for demos but does not handle coupling cleanly.

### 4b. What changes vs. the classical 2D pendulum

1. **Two unstable rotational DOFs** (pitch AND roll). The "everything reduces to scalar pitch" math is gone.
2. **Control input is a 3-vector** of wheel torques (for 3 omniwheels). The PWM map is a 3×3 matrix, not a scalar gain.
3. **Yaw is a controlled axis,** not a free direction.
4. **State-space is 12+ dimensional** — quaternion (3 effective DOF) + angular velocity (3) + ball position (2) + ball velocity (2) + optional integrals. Markley & Crassidis (2014), *Fundamentals of Spacecraft Attitude Determination and Control*, Springer ch. 3 and 6, is the standard treatment.
5. **Roll–pitch–yaw coupling** through ball kinematics is non-linear; the Kugle thesis explicitly motivates quaternion-state non-linear models for this reason.

PID does not extend gracefully. The honest answer is **LQR or MPC**, with linearization done offline and `K` baked into flash.

### 4c. AVR feasibility

- A 12-state LQR gain matrix is `K ∈ ℝ^{3×12}` = 36 floats = 144 B — easy. Applied per tick as 3×12 multiply-adds, ~150 µs at 16 MHz. Feasible.
- A 12-state EKF needs measurement-update covariance factorization — ~12³ flops/tick + ~200 floats of state. ~1 KB RAM = half the Uno budget. Tight; survivable only if covariance updates run at 50 Hz rather than 200 Hz.
- Adaptive LQR (recomputing `K` as the plant changes) requires solving the **discrete algebraic Riccati equation (DARE)** online. Standard QZ-pencil / Schur-decomposition solvers are kilo-LOC LAPACK-class. Structure-preserving symplectic methods cut compute ~60% (Bunse-Gerstner et al., *Numer. Math.* 60, 1991) but still well beyond AVR. **Online DARE on AVR is not realistic.**
- Teensy 4.0 (600 MHz M7 + FPU) and ESP32-S3 (240 MHz Xtensa + FPU) both run a 12-state LQR + EKF comfortably.

**Fixed-gain LQR on AVR is borderline; adaptive LQR is not. Teensy/ESP32 are the natural targets.**

### 4d. Verdict for Level 3

| Axis | Score |
| --- | --- |
| Firmware effort | High. New LQR + MIMO controller, possibly MPC. ~2–5 kLOC. |
| Hardware effort | Very high. Sphere + 3 omniwheels + 3 drivers + new chassis. |
| Algorithmic effort | High. New controller and estimator classes. |
| Per-bot burden | Higher. Linearization matrices encode plant geometry; online matrix-valued identification is research work. |

**Feasibility: hard but not closed.** Rezero, Kugle, and CMU Ballbot each took 6–12 months of focused academic effort to first balance. For Floppi's scope — hobbyist framework, Arduino-class hardware — this is **future-work / aspirational**, not on the active roadmap. The only design decision worth making today: don't paint the framework into a corner (§6).

## 5. Recommendations

### Build now (next 2–4 weeks)

- **Finish Phase 4.10** — universal auto-tune (scalar RLS plant ID + relay refinement). Already designed in [`dynamic_pwm_accel_learning.md`](dynamic_pwm_accel_learning.md). Highest value per hour of effort still on the table.

### Build next (post-Phase 4.10, ~1 week of work)

- **Phase 4.11 — Level 2.** Arbitrary-orientation balance via a `BalanceFrame` abstraction + boot-time gravity capture + wheel-axis selector (hard-coded v1, motor-pulse v2). Firmware-only. Compatible with every existing module. Pays for itself the first time a user mounts the IMU 90° rotated and the bot still works.

### Future (defer; don't build until prerequisites exist)

- **Phase 5.x — Level 1.** Rollover axis swap. Wait for a multi-face chassis design. Until then, mostly redundant with Phase 4.11.
- **Phase 6.x — Level 3.** Ballbot. Research / hardware-rebuild scope. Make architectural room for it now (§6); don't speculatively build code until a sphere chassis exists and Teensy/ESP32-S3 is the target MCU.

### Don't build

- **"Custom MIMO controller from scratch"** for Level 3. If/when Level 3 is real, use Kugle's published model + an existing LQR implementation (or precompute offline). Greenfield MIMO controllers are PhD work, not framework work.
- **"Reaction-wheel cube" target.** Cubli is fascinating but a fundamentally different mechanical class. Not on the roadmap unless the user explicitly redirects.

## 6. What this means for the framework architecture today

Even though Levels 1 and 3 are months/years away, a few small Phase 4.11 design choices preserve the option without speculative work:

1. **Keep the control layer quaternion-aware at the data layer.** Don't bake `pitch_deg` deep into APIs. `BalanceFrame::tilt_error()` returns a scalar, but the underlying state stays a quaternion. `OrientationSensor::getOrientation()` already returns a full quaternion — preserve that.
2. **`BalanceFrame` is a runtime object, not a compile-time singleton.** A future Level-1 build needs `BalanceFrame[N_FACES]`. Costs nothing today (`N_FACES = 1`).
3. **Phase 4.10's plant identifier outputs `K_motor` and `g_eff` *for the current balance frame*.** When/if Level 1 lands, each face has its own pair — no new math, just an array.
4. **Reserve `BalanceFrame::wheel_axis` even for single-face builds.** Two bytes in the EEPROM mount-blob. Cheap insurance.
5. **Quaternion stays canonical; Euler stays derived.** The moment anything caches `pitch_deg` as source-of-truth, multi-orientation work becomes a rewrite.

None of these adds work to Phase 4.11. All of them keep Level 3 reachable without speculative scaffolding.

## 7. References

Real citations only; verified via web search.

- Lauwers, T. B., Kantor, G., and Hollis, R. (2006). "A Dynamically Stable Single-Wheeled Mobile Robot with Inverse Mouse-Ball Drive." *Proc. IEEE ICRA 2006*, Orlando, pp. 2884–2889. [Lauwers et al. 2006 PDF](http://www.msl.ri.cmu.edu/publications/pdfs/ballbot_ICRA06_web.pdf).
- Nagarajan, U., Kantor, G., and Hollis, R. (2014). "The ballbot: An omnidirectional balancing mobile robot." *IJRR*, 33(6), 917–930. [Nagarajan et al. 2014 PDF](http://clarinet.msl.ri.cmu.edu/publications/pdfs/ballbotIJRR_final.pdf).
- Gajamohan, M., Muehlebach, M., Widmer, T., and D'Andrea, R. (2013). "The Cubli: A Reaction Wheel Based 3D Inverted Pendulum." *ECC 2013*, Zürich, pp. 268–274. [Cubli ECC 2013 PDF](https://ethz.ch/content/dam/ethz/special-interest/mavt/dynamic-systems-n-control/idsc-dam/Research_DAndrea/flyingPlatform/PersonalPageMichael/Cubli_ECC2013.pdf).
- Markley, F. L. and Crassidis, J. L. (2014). *Fundamentals of Spacecraft Attitude Determination and Control*. Springer, Space Technology Library, vol. 33. [Springer book record for Markley & Crassidis](https://link.springer.com/book/10.1007/978-1-4939-0802-8).
- Lauszus, K. (2012). "A practical approach to Kalman filter and how to implement it." [TKJ Electronics blog post by Lauszus](https://blog.tkjelectronics.dk/2012/09/a-practical-approach-to-kalman-filter-and-how-to-implement-it/). Code: [TKJElectronics/BalancingRobotArduino](https://github.com/TKJElectronics/BalancingRobotArduino).
- Jensen, T. K. (2018). *Kugle — Modelling and Control of a Ball-balancing Robot*. M.Sc. thesis, Aalborg Univ. [Kugle thesis repository](https://github.com/mindThomas/Kugle-MATLAB).
- ETH Zürich Focus Project (2010). *Rezero — Ballbot*. [Rezero project site](http://rezero.ethz.ch/).
- ETH Zürich (2023). [One-Wheel Cubli news release](https://ethz.ch/en/news-and-events/eth-news/news/2023/03/one-wheel-cubli-with-a-single-reaction-wheel.html).
- Honda Motor (2009). [U3-X Personal Mobility Concept Wikipedia article](https://en.wikipedia.org/wiki/Honda_U3-X).
- AHRS docs — [Attitude from gravity (Tilt) reference](https://ahrs.readthedocs.io/en/latest/filters/tilt.html). Canonical accel-only quaternion init used in §2a.
- Åström, K. J. and Hägglund, T. (1984). "Automatic tuning of simple regulators with specifications on phase and amplitude margins." *Automatica*, 20(5), 645–651. Underpins Phase 4.5 relay tuning; see [`research_inverted_pendulum_control_methods.md`](research_inverted_pendulum_control_methods.md).

## 8. Open questions for the user

1. **Bench bot or hypothetical ballbot as Level 2 target?** Decides whether we generalize the framework now or ship bench-bot-specific Level 2 first. *Default: bench bot.*
2. **Level 1 — open to multi-face chassis redesign?** Or is 2-wheel the long-term shape? *Default: no chassis redesign; Level 1 deferred.*
3. **Level 3 — would the target extend to Teensy 4.0 / ESP32-S3,** or is AVR the hard ceiling? *Default: Level 3 is research-class on Teensy/ESP32 if it happens at all.*
4. **Wheel-axis selector — option 1 (hard-coded) or option 2 (motor-pulse probe) for v1?** *Default: ship option 1 in Phase 4.11, add option 2 in 4.11b.*

## See also

- [`../MULTI_ORIENTATION_BALANCE_VISION.md`](../MULTI_ORIENTATION_BALANCE_VISION.md) — the user's framing of Levels 1/2/3.
- [`../UNIVERSAL_BALANCE_BOT_VISION.md`](../UNIVERSAL_BALANCE_BOT_VISION.md) — the "no per-bot config" north star.
- [`../MINIMIZE_ACCELERATIONS_PHILOSOPHY.md`](../MINIMIZE_ACCELERATIONS_PHILOSOPHY.md) — controller-architecture companion.
- [`research_inverted_pendulum_control_methods.md`](research_inverted_pendulum_control_methods.md) — the upstream methods survey that picked STR + PID as the framework's controller.
- [`dynamic_pwm_accel_learning.md`](dynamic_pwm_accel_learning.md) — Phase 4.10 plant identifier; the prerequisite for Phase 4.11 (Level 2).
- [`MASTER_DESIGN.md`](MASTER_DESIGN.md) — overall framework design that this analysis sits on top of.
