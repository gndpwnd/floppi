# Flight Controller PID Lessons for the Balance-Bot Project

Status: cross-project synthesis. Aims to answer "why is the drone simpler?"
Last updated: 2026-05-12

## 1. Question

The user's question, paraphrased from the bench session: *"The drone keeps itself stable in the air without flipping — how does it do its PID tuning? Can we not use the same idea on the balance bot?"* The sibling `flight_controller/` project takes off and hovers on community-supplied PID constants and "just works." Meanwhile the balance bot has spent a week iterating Kp between 15 and 80. The asymmetry is real, and worth understanding precisely.

## 2. The honest answer in one paragraph

The flight controller does **not** auto-tune PID. It uses hardcoded gains inherited from dRehmFlight (`KP_ROLL_RATE = 0.15f`, `KI_ROLL_RATE = 0.15f`, `KD_ROLL_RATE = 0.0004f` — `flight_controller/include/config.h:353-355`) that the open-source quadcopter community has tuned over a decade on a narrow chassis class (5-inch-ish FPV multirotors). The reason a drone "just works" with those numbers is **physical, not algorithmic**: a four-motor quadcopter has redundant control authority, continuous lift, and is open-loop *neutral* in attitude — with no torque commanded, it drifts slowly rather than falling. A balance bot is open-loop *unstable*: gravity is the constant adversary, and zero control output means a fall in under a second. That is a categorically harder problem. Copying the drone's "use a community-tuned constant" pattern is feasible (it is essentially what `auto_orientation`'s `Kp=15 Ki=0 Kd=8` seed gains attempt) but it does not deliver universal auto-tune. True universal auto-tune is an open research problem in both domains — the flight-controller side has not solved it either, just papered over it with sensible defaults.

## 3. What the flight_controller ACTUALLY does

### 3a. Gain source — hardcoded constants from dRehmFlight

All flight-loop gains are `#define` macros in `flight_controller/include/config.h`:

```c
// Roll Rate PID  (config.h:353-356)
#define KP_ROLL_RATE 0.15f
#define KI_ROLL_RATE 0.15f
#define KD_ROLL_RATE 0.0004f
#define I_LIMIT_ROLL 25.0f

// Pitch Rate PID  (config.h:359-362) — identical
// Yaw Rate PID    (config.h:365-368) — different (KP=0.30, KI=0.05, KD=0.00015)

// Roll Angle PID  (config.h:374-376)
#define KP_ROLL_ANGLE 0.20f
#define KI_ROLL_ANGLE 0.00f
#define KD_ROLL_ANGLE 0.05f
```

These numbers come from Nicholas Rehm's dRehmFlight (MIT license, 2020-2024), validated by the community on small carbon-fibre X-frame quadcopters with cheap brushless motors and 4S batteries. They are not tuned for *your* drone; they are tuned for *quadcopters in general*.

### 3b. Cascade — two stacked PIDs per axis

There are two PID controllers, selected at compile time by `USE_RATE_CONTROLLER`:

- **Rate mode** (`controlRATE()`, `flight_controller/src/control.cpp:82-178`): error = `roll_des − GyroX`, PID drives motor mix. Acro mode — sticks command angular velocity.
- **Angle mode** (`controlANGLE()`, `flight_controller/src/control.cpp:184-276`): error = `roll_des − roll_IMU`, PID drives motor mix. Stab mode — sticks command attitude.

Each axis (roll/pitch/yaw) gets its own scalar PID. There is **no cross-coupling in the controller** — the controller is three independent scalar PIDs. Coupling appears only later, in the motor mixer.

Note: the angle-mode controller uses `derivative_roll = -GyroX` directly (`control.cpp:210`) — derivative-on-measurement, using the raw gyro as the derivative term. This is the same pattern the balance bot now uses (`MINIMIZE_ACCELERATIONS_PHILOSOPHY.md` Item 1 raw-gyro D-term).

### 3c. Feed-forward (`USE_RACING` tier only)

`FF_ROLL/PITCH/YAW` multiply the setpoint derivative (`control.cpp:158-160`). Default 0 in base build. Speeds up stick response; irrelevant to a balance bot, which has no operator-commanded setpoint, only "stay at zero pitch."

### 3d. TPA — Throttle PID Attenuation

`control.cpp:166-171`. When throttle exceeds `TPA_BREAKPOINT = 0.65`, PID outputs are scaled down by up to `TPA_RATE = 0.5`. Rationale: at high throttle motors are near saturation, so smaller PID output yields the same physical torque. Quad-specific — depends on a continuous throttle input that monotonically affects motor authority.

### 3e. Motor mixer — (thrust, roll, pitch, yaw) → 4 motor commands

`control.cpp:285-288`:

```text
m1 = thro_des − pitch_PID + roll_PID + yaw_PID
m2 = thro_des − pitch_PID − roll_PID − yaw_PID
m3 = thro_des + pitch_PID − roll_PID + yaw_PID
m4 = thro_des + pitch_PID + roll_PID − yaw_PID
```

This is the magic that makes a drone tolerate poor tuning: **four motors with opposite-direction roll/pitch/yaw signs**. A wrong roll command asks two motors up and two down — net thrust change is zero, so even mistuned roll just causes attitude oscillation, not altitude loss. A balance bot has no equivalent — its two wheels push the same direction for pitch correction.

### 3f. Air mode (`USE_RACING` + `USE_AIRMODE`)

`control.cpp:292-312`. Shifts motor outputs so PID corrections still apply at zero throttle. Quad concept. No balance-bot analog.

### 3g. Anti-windup — yes, present

Integral clamping on all three axes (`control.cpp:108`, `:209`): `constrain(integral_roll, -I_LIMIT_ROLL, I_LIMIT_ROLL)` with `I_LIMIT_*=25.0`. Integrators reset on arming (`control.cpp:355-357`). Same pattern the balance bot uses (I-term clamp at 40 PWM, reset on HELD→RUN).

### 3h. Low-pass filters on the D-term

Base tier (`config.h:264`): `B_DTERM = 0.15` PT1 coefficient applied as `derivative = (1−B)*prev + B*new` (`control.cpp:113`). With `USE_OPTIMIZATION`, the PT1 is replaced by a biquad LPF at 80 Hz. Same pattern the balance bot uses (`MINIMIZE_ACCELERATIONS_PHILOSOPHY.md`: "D-term measurement LPF, 15 ms τ"). Both projects converged on the same answer: a first-order LPF on the derivative is mandatory.

## 4. What auto-cal does (which IS auto, but is *sensor* cal not *gain* cal)

The flight controller has an extensive auto-calibration system. **None of it tunes PID gains.** All of it is sensor calibration:

- **4a. Gyro bias auto-cal at boot.** `auto-calibration-research.md` §1.1 documents the four-project survey (Betaflight / ArduPilot / INAV / Cleanflight). Pattern: collect ~1024-2048 gyro samples at boot, average for bias, detect movement and restart if anything wiggles. Floppi's `calculate_IMU_error()` uses 2000 samples with no movement detection — a known gap.

- **4b. ESC throttle range cal.** Operator-triggered (`e` serial command, `calibration_mode.cpp:648`). Walks the user through max-then-min throttle to teach ESCs the PWM range. Not online, not automatic.

- **4c. Accelerometer 6-point calibration.** Operator rotates through six orientations (`calibration_mode.cpp:131`). Solves for offset + scale per axis. Done once per board.

- **4d. Mag sphere cal, radio channel mapping, failsafe detection, IMU orientation detection.** All operator-driven serial commands (`m`, `r`, `f`, `o` — `calibration_mode.cpp:602-695`). All sensor-side.

**Critical observation:** every routine is sensor calibration. Not one tunes Kp/Ki/Kd. The `tune_kp_roll` / `tune_ki_roll` / `tune_kd_roll` runtime-overrideable globals (`flight_controller/include/globals.h:116-118`) exist only so the *operator* can type new gains via the `g` serial command (`calibration_mode.cpp:670-737`) during tethered flight. The bot does not adjust them itself.

## 5. What other projects do for PID auto-tune

`auto-calibration-research.md` §4 surveys the field:

- **Betaflight (§4.1) — has NO autotune.** Ships "one-size-fits-most" defaults (Roll P=45, I=80, D=40, F=120) tuned for typical 5-inch FPV quads, plus a slider UI that scales all gains together for "feel." Uses RPM filtering (motor telemetry → notch filters at vibration frequencies) to safely push gains higher. No closed-loop tuning of gain values.
- **ArduPilot AUTOTUNE (§4.2) — the real thing.** Pilot enters AUTOTUNE flight mode while hovering. For each axis sequentially, the controller commands rate steps, measures response (overshoot, settling time, oscillation), and iteratively adjusts P/D/I via modified Ziegler-Nichols. Takes 5-10 minutes per axis in a calm area. **Crucially, requires the quad to already hover** — it is a refinement of working defaults, not a cold start. Same conclusion as `AUTO_TUNING_REALITY_CHECK.md`: step-response and relay-feedback methods need the plant to be operating-but-imperfect. On a quad, "hover poorly" is a stable bootstrap regime. On a balance bot there is no equivalent — any working state is already balancing.
- **Academic methods (§4.3)** — relay feedback, MRAC, extremum seeking, RL — all need either the plant alive, or extensive offline modeling. None is a cold-start tuner for an unstable plant.

Why the FC has not integrated either: from `flight_controller/docs/roadmap.md` and `auto-calibration-research.md` §4.4, autotune is a "Phase 3+ feature" estimated at "several weeks of development and extensive flight testing." From `flight_controller/docs/scope.md`: *"Bare bones yet efficient. The firmware does less at runtime precisely because calibration is thorough and automated upfront."* **The actual answer is that community defaults work well enough on the target hardware class. No clever algorithm — just "0.15 is good for small quads."**

## 6. Why is the drone EASIER than the balance bot?

Here is the unsentimental control-theory comparison:

| Property | Drone (quad) | Balance Bot |
| --- | --- | --- |
| Open-loop attitude stability | Neutral (no torque without motor command) | Unstable (gravity always pulling toward fall) |
| Time to physical limit if motors off | ~1 s (slow attitude drift) | ~0.5 s (active fall to 30°+) |
| Control authority | 4 motors, total thrust > weight | 2 motors, ground-friction-limited |
| Disturbance rejection | Strong — any motor can compensate | Weak — only fore/aft wheel impulses |
| Tunable region | Wide — gains can be off ±50% and still fly | Narrow — gains off ±30% means a fall |
| Bootstrap challenge | None — start in trim hover, gather data | Severe — can't gather data without first balancing |
| Wrong-direction PID output | Causes a wobble, sometimes recovers | Often unrecoverable (wheel slammed wrong way past tipover) |
| Dimensionality | 3-axis (roll, pitch, yaw) decoupled | 1-axis (pitch) but heavily coupled to translation |
| Vibration coupling to control | Strong (props at 200-300 Hz) — needs notch filters | Weak (wheels at <20 Hz, well below PID bandwidth) |

This is what `AUTO_TUNING_REALITY_CHECK.md` calls "the chicken-and-egg problem of unstable plants." The drone does not have it. The balance bot does. The same algorithm choices look completely different in each context: a relay-feedback experiment that kills 30 seconds of a quad's flight is trivial; the same experiment on a balance bot requires the bot to already balance for 30 seconds, which is exactly the thing you were trying to enable.

## 7. What CAN we copy from the flight_controller?

- **7a. Auto-cal philosophy — sensor cal mandatory, gain cal optional.** The FC auto-cals sensors (gyro bias, accel offsets, mag sphere, IMU orientation) but not gains. We are on par (BNO055 EEPROM blob, `OnlineMountingEstimator`, sensor cal at boot). Worth verifying we have Betaflight-style stationary-detection-with-restart on gyro cal — the FC project lists "no movement detection during gyro calibration" as a high-severity gap (`auto-calibration-research.md` §6); we should not inherit that bug.

- **7b. Cascade controller — outer angle loop, inner rate loop.** The FC has `controlANGLE()` and `controlRATE()` as two separate PIDs (`control.cpp:82` and `:184`). Angle mode is in fact two stacked loops: outer angle PID produces a desired body rate, inner rate PID tracks it. Currently the balance bot uses a single PID on pitch with raw gyro as D-term — equivalent in the linear regime but diverges when either loop saturates. A fully cascaded balance controller would have its own anti-windup per loop. Worth prototyping, not urgent.

- **7c. TPA pattern — gain scaling with control authority.** For a quad, authority drops at high throttle (motors near saturation). For a balance bot, authority drops as battery sags (PWM-to-torque ratio drops). Same idea, different trigger. Phase 4.10's RLS estimator (`dynamic_pwm_accel_learning.md`) literally does this — by tracking `K_motor` online, gains can scale inversely. We don't need the FC's TPA code; we need the concept.

- **7d. Hardcoded-defaults-from-community pattern.** The big one. The drone "works without auto-tuning" because dRehmFlight defaults work across a wide class of small quads. The balance-bot project could ship defaults that work across a wide class of bench-scale bots — which is what `Kp=15 Ki=0 Kd=8` is attempting. **Question:** is bench-scale-balance-bot a tight enough class for one default the way 5-inch-quad is? We don't know — we've iterated on one bot. Matching the drone's strategy requires validating across multiple chassis (or landing Phase 4.10 properly).

- **7e. Compile-flag inventory — tiered feature sets.** The FC has `USE_OPTIMIZATION`, `USE_RACING`, `USE_AIRMODE`, `USE_OTA`, etc. The balance-bot project could mirror: `BALANCE_BOT_BASIC` / `BALANCE_BOT_ADVANCED` (online K_motor RLS) / `BALANCE_BOT_RESEARCH` (anomaly detector). We already have `USE_BALANCE_HELD_DETECTION` / `USE_BALANCE_FALL_DETECTION` — the pattern is there.

## 8. What CAN'T we copy

- **Mixer math** (`control.cpp:285-288`). Four-motor specific. Two wheels with the same sign do not give the same redundancy.
- **Air mode** (`control.cpp:292-312`). Motor-redundancy specific.
- **Throttle-aware gain scaling (TPA)**. We have no throttle.
- **Multi-axis decoupled control**. We care about pitch only; roll and yaw are not controlled.
- **Trim-hover bootstrapping for AUTOTUNE**. We have no stable bootstrap regime to do system ID from.
- **RPM filtering** (Betaflight). Requires bidirectional DShot ESC telemetry. Our brushed-motor + L298N stack does not have that signal.

## 9. The "is BNO055 calibration off?" question

The user raised this at the bench session. BNO055 cal lives in EEPROM via the framework's persistent-storage HAL, restored at boot by `CalibrationStorage::load()` (see `KNOWN_ISSUES.md` KI-1 — the EEPROM path silently fails on ESP32; on Mega/Uno it works). The driver calls `bno.setSensorOffsets(loadedCal)` after self-init, then polls `bno.getCalibration(&sys, &gyro, &accel, &mag)` — when all four reach 3, the framework treats the chip as calibrated.

Common failure modes:

- **sys=0 indefinitely**: chip never settled (glitchy I2C, brief power loss). Pitch drifts slowly.
- **gyro=0**: chip was moved during gyro-stillness phase. Pitch has a constant bias (looks like mount offset error).
- **accel=0**: insufficient orientation diversity at boot. Pitch noisy but not biased.
- **mag=0**: irrelevant for balance (balance loop never uses mag; see `UNIVERSAL_BALANCE_BOT_VISION.md` §"Magnetometer-optional design").

Diagnostic steps: (1) run `s` and check sys/gyro/accel are all 3; (2) read raw accel, confirm magnitude ~9.81 m/s² stationary in any orientation; (3) compare reported pitch with a physical inclinometer — if it differs by >1-2° at rest, fused output is unreliable. If suspected: re-cal (wave through poses ~1 minute), confirm all four reach 3, save, reboot, verify restore. If that fixes balancing, cal was the issue. If not, gains are.

## 10. Concrete action items for the balance-bot project

In priority order:

1. **Validate BNO055 cal health on next bench session.** Run `s`, write down sys/gyro/accel/mag. If any are below 3, the controller sees drifting orientation and no PID tuning will fix it. 30-second test that gates everything else.

2. **Add stationary-verification at boot** mirroring Betaflight's reset-on-movement (`auto-calibration-research.md` §1.1). Framework currently does blind averaging — same gap the FC has. Cheap fix, high-value.

3. **Sweep seed gains before adding any algorithm.** Try `Kp ∈ {15, 30, 50, 80}` with the same chassis/battery/surface, document the survival floor, hardcode as new default. This is the "copy the drone's strategy" play.

4. **Consider the cascade explicitly** (§7b). Outer angle PID → inner rate PID, each with own anti-windup. Phase 4.7d candidate.

5. **Resist inventing more state machines.** The drone has one primary state machine (DISARMED → CALIBRATING → ARMED → FAILSAFE). We already have HELD, RUN, FALLEN, optional auto-recovery. Adding more is anti-pattern.

6. **Land Phase 4.10 (RLS online K_motor identification)** as the real fix. The drone gets away with no auto-tune because four motors give wide tolerance to wrong gains. We don't have that luxury — online identification is the answer. `dynamic_pwm_accel_learning.md` already designs it. ~2 days of focused work.

## 11. References

### Flight controller source (cited inline)

- `flight_controller/src/control.cpp:20-76` — `getDesState()`, RC stick → desired setpoint
- `flight_controller/src/control.cpp:82-178` — `controlRATE()` — rate-mode PID
- `flight_controller/src/control.cpp:184-276` — `controlANGLE()` — angle-mode PID
- `flight_controller/src/control.cpp:282-337` — `controlMixer()` — motor mixing
- `flight_controller/src/control.cpp:343-380` — `armedStatus()` — arming state machine
- `flight_controller/include/config.h:353-381` — all hardcoded PID gains
- `flight_controller/include/config.h:264, 414` — D-term LPF coefficients
- `flight_controller/include/globals.h:116-122` — runtime-overrideable `tune_*` gains
- `flight_controller/src/calibration_mode.cpp:602-737` — serial command handler including `g` (set gain) and `p` (set parameter)

### Flight controller research documents

- `flight_controller/docs/findings/auto-calibration-research.md` §1 (gyro cal), §4 (PID auto-tune survey), §6 (gap analysis)
- `flight_controller/docs/findings/bare-bones-fc-research.md` §1-3 (design philosophy, attitude estimation, PID improvements)
- `flight_controller/docs/scope.md` (bare-bones manifesto)
- `flight_controller/docs/roadmap.md` (auto-tune as Phase 3+ future work)

### Balance-bot side

- `auto_orientation/docs/UNIVERSAL_BALANCE_BOT_VISION.md` — the design north star
- `auto_orientation/docs/MINIMIZE_ACCELERATIONS_PHILOSOPHY.md` — controller-level companion
- `auto_orientation/docs/AUTO_TUNING_REALITY_CHECK.md` — why an inverted pendulum cannot be one-shot tuned
- `auto_orientation/docs/findings/research_inverted_pendulum_control_methods.md` — survey of methods (PID, relay-FB, LQR, pole placement, MRAC)
- `auto_orientation/docs/findings/dynamic_pwm_accel_learning.md` — the Phase 4.10 RLS plan
- `auto_orientation/docs/findings/auto_pid_tuning_research.md` — existing relay-feedback `AutoPIDTuner`

### Upstream

- Nicholas Rehm, **dRehmFlight** ([github.com/nickrehm/dRehmFlight](https://github.com/nickrehm/dRehmFlight)) — MIT license, the source of the FC's gain values and overall architecture
- Madgwick, S.O.H. (2010) — the 6DOF attitude filter used by the FC
- Åström, K.J. & Hägglund, T. (1984) — relay-feedback auto-tuning
- ArduPilot AUTOTUNE documentation: [ardupilot.org/copter/docs/autotune.html](https://ardupilot.org/copter/docs/autotune.html)

---

**Bottom line**: the drone is simpler because it is a *physically more forgiving* plant, not because its software is cleverer. Copying the drone's approach to PID tuning *is* what `Kp=15 Ki=0 Kd=8` already attempts; the limit of that approach is whatever class of balance bots one set of constants can cover. Going further than "good defaults" requires online identification, and Phase 4.10 is the right place to put it.
