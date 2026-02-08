# Research: Bare-Bones Flight Controller Features & Algorithms

> Date: 2026-02-07
> Context: What should floppi include, what should it leave out, and what practical improvements matter?

---

## Design Philosophy

Floppi is a bare-bones flight controller. The goal is raw performance: read sensors, filter, PID, output motors — lots of math, really fast. Complex logic (mission planning, in-flight mode switching, aerobatics sequencing, swarm coordination) belongs on an external flight computer that sends commands to the FC.

This is the same philosophy as dRehmFlight and Hackflight: a flight **stabilizer**, not a flight **autopilot**. Betaflight, ArduPilot, and INAV are full autopilots with hundreds of features — that's explicitly not what we're building.

**Guiding principle**: If a feature adds runtime overhead to the flight loop and isn't necessary for stable flight, it doesn't belong in the firmware. It belongs on the flight computer.

---

## 1. What Every Flight Controller Absolutely Needs

These are non-negotiable for a flying vehicle:

| Component | floppi status | Notes |
|-----------|--------------|-------|
| IMU read (accelerometer + gyroscope) | Done | MPU6050/9250 via I2C |
| Attitude estimation (sensor fusion) | Done | Madgwick 6DOF |
| Low-pass filtering on sensor data | Done | Configurable LP filters on accel/gyro |
| PID controller | Done | Rate + angle modes |
| Motor mixing | Done | Quad X, configurable for other layouts |
| Receiver input | Done | SBUS/DSM/PPM/PWM |
| Arming/disarming safety | Done | Throttle low + CH5 |
| Failsafe on signal loss | Done | Zero throttle, center sticks |
| Loop timing enforcement | Done | 1kHz ESP32, 2kHz Teensy |

**floppi already has all the essentials.** Everything beyond this list is an improvement, not a requirement.

---

## 2. Attitude Estimation: Madgwick vs Mahony vs Complementary

### Current: Madgwick 6DOF

floppi uses the Madgwick filter with a single tuning parameter (beta = 0.04). This is a solid choice.

### Comparison

| Filter | Computation | Accuracy | Tuning | Used by |
|--------|------------|----------|--------|---------|
| **Complementary** | Lowest | Adequate for calm flight, drifts under vibration | 1 parameter (alpha) | Simplest DIY FCs |
| **Mahony** | Low | Better roll/pitch stability, fastest computation | 2 parameters (Kp, Ki) | Betaflight (default), ESP-FC |
| **Madgwick** | Medium | Better noise rejection, better yaw accuracy | 1 parameter (beta) | dRehmFlight, many research FCs |
| **EKF (Kalman)** | Highest | Best overall, handles GPS/baro fusion natively | Many parameters | ArduPilot, PX4 |

### Recommendation: Keep Madgwick

- Madgwick is the right choice for a bare-bones FC. Better noise rejection than complementary, simpler than EKF.
- Mahony is slightly faster and used by Betaflight, but the difference is negligible at our loop rates.
- EKF is overkill without GPS/barometer — it's designed for multi-sensor fusion.
- **No change needed.** Madgwick with beta=0.04 is well-suited to our use case.

### Sources
- [IEEE: Comparison of EKF, Madgwick and Mahony on Quadcopter Flight Data](https://ieeexplore.ieee.org/document/8453465/)
- [NDSU: Basic, Madgwick and Mahony comparison](https://web.cs.ndsu.nodak.edu/~siludwig/Publish/papers/SPIE20181.pdf)
- [OlliW: IMU Data Fusing](https://www.olliw.eu/2013/imu-data-fusing/)

---

## 3. PID Improvements That Actually Matter

### Current implementation

floppi has a standard textbook PID with:
- Integral anti-windup (I-term clamping via `I_LIMIT_*`)
- Separate rate and angle mode gains
- Integrator reset on arming

### What's missing that matters

**D-term low-pass filter (HIGH PRIORITY)**

The current derivative calculation `(error - error_prev) / dt` amplifies sensor noise directly into motor commands. This is the single most impactful improvement for flight quality.

Every serious FC (Betaflight, ArduPilot, ESP-FC) filters the D-term. Without it, you get motor oscillation and hot motors.

Implementation: simple first-order low-pass filter on the derivative:
```
filtered_derivative = alpha * new_derivative + (1 - alpha) * prev_filtered_derivative
```

Where `alpha` is typically 0.05-0.2 (configurable in config.h). This is ~3 extra multiplications per axis per tick. Negligible cost, major quality improvement.

**Derivative on measurement, not error (MEDIUM PRIORITY)**

The angle mode controller already does this correctly — `derivative_roll = -GyroX` uses the gyro measurement directly. But the rate mode controller uses `(error - error_prev) / dt` which amplifies setpoint changes into D-term spikes (called "derivative kick").

Fix: use `-(measurement - measurement_prev) / dt` instead of `(error - error_prev) / dt` in rate mode. This is a one-line change per axis.

**What we DON'T need (Betaflight feature bloat)**

| Betaflight feature | Why we skip it |
|--------------------|----------------|
| Dynamic notch filter | Requires FFT analysis, complex, for high-vibration racing frames |
| RPM filter | Requires ESC telemetry (DShot bidirectional) |
| Feed-forward term | Optimizes stick response feel for FPV racing |
| TPA (Throttle PID Attenuation) | Racing optimization |
| D-max dynamic boost | Racing optimization |
| Setpoint transition smoothing | Racing optimization |
| Crash recovery | Autonomous feature — flight computer territory |
| Yaw spin recovery | Edge case optimization |
| Absolute control | Oscillation dampening — Betaflight 4.x specific |

### Recommendation

1. **Add D-term low-pass filter** — highest impact, lowest cost. Add a `D_TERM_LPF_ALPHA` define to config.h.
2. **Fix rate mode derivative** to use measurement instead of error — prevents derivative kick.
3. Skip everything else. Good PID tuning matters more than exotic features.

### Sources
- [Oscar Liang: FPV Drone PID Explained](https://oscarliang.com/pid/)
- [MATLAB: Quadcopter PID Derivative Filter](https://www.mathworks.com/matlabcentral/answers/166354)
- [Betaflight PID Tuning Guide](https://www.betaflight.com/docs/wiki/guides/current/PID-Tuning-Guide)

---

## 4. Motor Protocol: PWM vs OneShot125 vs DShot

### Current: Standard PWM (1000-2000us)

| Protocol | Update rate | Signal delay | Digital? | Calibration needed? |
|----------|-----------|-------------|---------|-------------------|
| **Standard PWM** | ~490 Hz | 2ms | No (analog) | Yes |
| **OneShot125** | ~4 kHz | 250us | No (analog) | Yes |
| **DShot300** | ~9 kHz | Digital | Yes | No |
| **DShot600** | ~18 kHz | Digital | Yes | No |

### Recommendation: Keep PWM, add OneShot125 as option

- PWM works. At 1kHz loop rate, PWM's 490Hz update rate is actually a bottleneck, but for a first build it's fine.
- OneShot125 is the natural upgrade — 8x faster, same ESC hardware (most ESCs support it). Already have `USE_ONESHOT125` define in config.h.
- DShot requires precise timing (bit-banging or DMA), platform-specific code, and ESC support. Good future feature, not needed now.
- **No code change needed now.** OneShot125 support is already defined in config.h. DShot can be a backlog item.

### Sources
- [QuadMeUp: PWM, OneShot, DShot comparison](https://blog.quadmeup.com/2016/08/31/pwm-oneshot125-oneshot42-multishot-and-dshot-comparison/)
- [Oscar Liang: What is DShot](https://oscarliang.com/dshot/)
- [ArduPilot: ESC Protocols](https://ardupilot.org/copter/docs/common-brushless-escs.html)

---

## 5. What Betaflight/ArduPilot/INAV Have That We Don't Need

These are full autopilot systems with hundreds of features. Here's what we explicitly skip and why:

| Feature | Why we skip it |
|---------|---------------|
| GPS/waypoint navigation | Flight computer territory |
| Return to home | Flight computer territory |
| Altitude hold (barometer) | Flight computer territory |
| Position hold | Flight computer territory |
| OSD (on-screen display) | We have OLED + WiFi telemetry |
| Blackbox logging | Flight computer can log via WiFi |
| In-flight PID tuning | Calibration mode covers this |
| Dynamic gyro filtering (FFT) | Overkill for DIY builds, complex |
| Multiple flight modes + switching | Compile-time selection. Flight computer handles switching |
| Programmable logic conditions | Way too complex |
| VTX control (SmartAudio) | Not our concern |
| ESC telemetry | Requires DShot bidirectional |
| Buzzer/LED patterns | Maybe LED (simple), buzzer is a backlog nice-to-have |

**The entire navigation/autonomous stack is out of scope.** That's what a flight computer is for. The FC's job is to hold the attitude it's told to hold.

---

## 6. What dRehmFlight Is Missing That Actually Matters

Based on analysis of dRehmFlight's code and community feedback:

| Gap | Impact | Difficulty | Our status |
|-----|--------|-----------|-----------|
| **D-term filtering** | High — motor oscillation, hot motors | Low | Not yet implemented |
| **Rate mode derivative on measurement** | Medium — derivative kick on stick input | Trivial | Not yet implemented |
| **ESC calibration routine** | Medium — needed once per build | Low | On roadmap |
| **Low battery warning** | Medium — prevents crashes from dead battery | Low | On roadmap (voltage ADC) |
| **Motor output limiting** | Low — prevents runaway in case of PID error | Trivial | constrain() already applied |
| **Gyro calibration on startup** | Low — currently requires manual trigger | Low | IMU calibration exists in calibration build |

### What dRehmFlight gets RIGHT that we should keep
- Single-file philosophy (we've modularized, but each module is small and focused)
- Compile-time configuration only
- No runtime menus or mode switching in the flight loop
- Simple, readable PID implementation
- User-customizable mixer

---

## 7. Loop Rate: Is 1kHz/2kHz Enough?

### Current: 1kHz (ESP32), 2kHz (Teensy)

| FC | Loop rate | Notes |
|----|-----------|-------|
| floppi (ESP32) | 1 kHz | FreeRTOS task on Core 0 |
| floppi (Teensy) | 2 kHz | Direct loop |
| Betaflight | 2-8 kHz | 8kHz with SPI gyro |
| ESP-FC | up to 4 kHz | With SPI gyro on ESP32 |
| ArduPilot | 400 Hz | Slower but has navigation stack overhead |
| dRehmFlight | 2 kHz | Teensy only |

### Analysis

- **1kHz is fine for angle mode hovering/filming.** ArduPilot runs at 400Hz and flies large drones reliably.
- **2kHz is good for rate mode.** Diminishing returns above 2kHz unless racing.
- **4-8kHz matters for FPV racing** where latency is critical — not our use case.
- The bottleneck on ESP32 is I2C IMU reads (~1ms for MPU6050 at 400kHz I2C). SPI gyros can read in ~50us, enabling higher rates.
- **No change needed.** Our loop rates are appropriate for our use case.

---

## 8. Failsafe: Current vs Best Practice

### Current
- Signal loss → center sticks, zero throttle, disarmed
- Throttle cut on CH5 switch

### What's adequate for bare-bones
This is fine. The drone will fall if signal is lost, which is the safest behavior for a small DIY drone (no GPS to hold position anyway).

### Future improvements (backlog, not priority)
- **Low battery voltage cutoff**: ADC reading of battery voltage, gradual throttle reduction when low. Simple, prevents LiPo damage and uncontrolled crashes.
- **Motor stop on disarm**: Already implemented (armedFly = false → zero motor output).
- **Watchdog timer**: Reboot if flight loop hangs. ESP32 has built-in watchdog support.

---

## 9. Sensor Fusion: Is 6DOF Enough?

### Current: 6DOF (accelerometer + gyroscope)

**Yes, 6DOF is enough for a bare-bones FC.**

- Accelerometer provides gravity reference → roll/pitch absolute angle
- Gyroscope provides rotation rate → fast response
- Madgwick fuses them → stable attitude estimate
- Yaw will drift slowly without magnetometer, but this doesn't matter for manual/stabilized flight

### When you'd need more
| Sensor | When needed | Our take |
|--------|------------|---------|
| Magnetometer (9DOF) | Heading hold, GPS navigation | Not needed — flight computer territory |
| Barometer | Altitude hold | Not needed — flight computer territory |
| GPS | Position hold, RTH, waypoints | Not needed — flight computer territory |
| Optical flow | Indoor position hold | Not needed — flight computer territory |

**All of these add sensors for autonomous features that belong on the flight computer.** The FC doesn't need to know where it is — just what angle it should be at.

---

## 10. Wind Compensation / Disturbance Rejection

### How it works now
The PID controller already provides basic disturbance rejection:
- Wind pushes drone → angle changes → error increases → PID corrects
- I-term accumulates steady-state error → eliminates constant offset (steady wind)

### What makes it better
1. **Good I-term tuning** — The I-gain determines how aggressively the FC fights steady disturbances. Higher I = faster wind rejection, but can cause oscillation/bounce-back.
2. **D-term filtering** (see section 3) — Clean D-term means the controller responds to real disturbances, not noise.
3. **Loop rate** — Faster loop = faster disturbance rejection. Our 1-2kHz is good.

### Advanced approaches we DON'T need
| Approach | Complexity | Why skip it |
|----------|-----------|-------------|
| Disturbance observer | High | Requires model of drone dynamics |
| Feedforward compensation | Medium | Requires wind measurement (pitot tube or estimation) |
| Adaptive PID gains | High | Complex, hard to tune, hard to debug |
| Model predictive control (MPC) | Very high | Academic, computationally expensive |

### Recommendation
Wind compensation is mostly about **PID tuning**, not special algorithms. With good I-term tuning and D-term filtering, the existing controller will handle moderate wind. For strong wind, the answer is a bigger/heavier drone, not more software.

### Sources
- [Wind Compensation in Drones using PID Control](https://irojournals.com/jscp/article/pdf/7/3/6)
- [Oscar Liang: PID Explained](https://oscarliang.com/pid/)

---

## Summary: What floppi Should Include

### Already done (keep as-is)
- Madgwick 6DOF attitude estimation
- PID with anti-windup (I-term clamping)
- Compile-time rate/angle mode selection
- Arming/disarming + failsafe
- Multi-protocol receiver support
- Multi-platform (Teensy + ESP32)
- OLED display, WiFi telemetry, API client (ESP32)
- 1-2kHz loop rate

### Should add (small, high-impact improvements)
1. **D-term low-pass filter** — prevents motor oscillation, biggest single improvement
2. **Rate mode derivative on measurement** — prevents derivative kick

### Should NOT add (feature creep)
- In-flight mode switching (flight computer's job)
- Dynamic notch/RPM filters (racing optimization)
- GPS/baro/mag integration (flight computer's job)
- DShot protocol (hardware-specific, complex, future backlog)
- Blackbox logging (flight computer can log via WiFi)
- Any autonomous features (RTH, position hold, waypoints)
- In-flight PID tuning (calibration mode + fc_tool covers this)

### The 90% rule
The user stated floppi has ~90% of desired features. This analysis confirms that assessment. The two practical improvements (D-term filter, derivative on measurement) are small code changes, not new features. Everything else is either unnecessary for bare-bones or belongs on the flight computer.

---

## Landscape: Related Projects

| Project | Platform | Scope | Key difference from floppi |
|---------|----------|-------|---------------------------|
| [dRehmFlight](https://github.com/nickrehm/dRehmFlight) | Teensy | Bare-bones stabilizer | Our foundation. Single file, no ESP32. |
| [Hackflight](https://github.com/simondlevy/Hackflight) | Various | Minimal toolkit | Header-only C++, more academic |
| [ESP-FC](https://github.com/rtlopez/esp-fc) | ESP32 | Betaflight-compatible FC | More features, Betaflight configurator compatible |
| [madflight](https://github.com/qqqlab/madflight) | ESP32/RP2040/STM32 | dRehmFlight-inspired, multi-platform | More platforms, similar philosophy |
| [Betaflight](https://github.com/betaflight/betaflight) | STM32 F4/F7/H7 | Full FPV racing firmware | 100+ features, complex, racing-focused |
| [ArduPilot](https://ardupilot.org/) | Various | Full autopilot | Navigation, GPS, waypoints, autonomous |
| [INAV](https://github.com/iNavFlight/inav) | STM32 F4/F7/H7 | Navigation-focused | GPS, RTH, waypoints, OSD |

floppi sits in the same space as dRehmFlight and madflight: bare-bones stabilizer firmware for makers who want to understand and customize their FC. The ESP32 WiFi integration for swarm coordination is what makes floppi unique.

---

## Feature Tiers: Base / Optimization / Racing

The firmware uses compile-time `#ifdef` feature flags for everything. These tiers allow developing advanced features without hindering base performance. When a tier is not enabled, its code doesn't exist in the binary.

### Tier 0: BASE (always compiled)

The core flight stabilizer. This is what ships and what gets tested first.

| Feature | Status | Notes |
|---------|--------|-------|
| Madgwick 6DOF filter | Done | beta = 0.04 |
| PID controller (rate + angle) | Done | Compile-time mode selection |
| I-term anti-windup (clamping) | Done | I_LIMIT_* in config.h |
| Motor mixing (configurable) | Done | Quad X default |
| Multi-protocol receiver | Done | SBUS/DSM/PPM/PWM |
| Arming/disarming + failsafe | Done | Throttle + CH5 |
| Standard PWM output | Done | 1000-2000us |
| **D-term low-pass filter** | TODO | PT1 filter, configurable alpha. Prevents motor noise. |
| **Rate mode derivative on measurement** | TODO | One-line fix per axis. Prevents derivative kick. |

### Tier 1: USE_OPTIMIZATION (config.h flag)

Noise reduction and filtering for cheaper hardware. For people using budget motors, unbalanced props, or flexible frames that produce more vibration. These add a small amount of compute per tick but significantly improve flight quality on noisy hardware.

**Enable in config.h:** `#define USE_OPTIMIZATION`

| Feature | Compute cost | Description |
|---------|-------------|-------------|
| Biquad gyro low-pass filter | ~6 multiplies/axis | Steeper -12dB/octave rolloff vs PT1's -6dB/octave. Better noise rejection at the cost of ~2ms delay at 100Hz cutoff. Selectable cutoff frequency. |
| D-term biquad filter | ~6 multiplies/axis | Upgrade D-term filter from PT1 to biquad. Reduces high-frequency derivative noise more aggressively. |
| Configurable gyro notch filter | ~8 multiplies/axis | Narrow-band rejection filter targeting a specific noise frequency (e.g., motor resonance). Center frequency and bandwidth configurable. |
| Accelerometer second-stage LP | ~3 multiplies/axis | Extra filtering on accelerometer for attitude estimate stability with vibration. |

**Total overhead when enabled**: ~50-70 extra floating-point multiplications per tick. At 1kHz on ESP32 (240MHz FPU), this adds ~5-10us per loop — negligible.

**Config.h parameters** (only exist when `USE_OPTIMIZATION` is defined):
```c
// Gyro biquad LPF cutoff (Hz). Lower = more filtering, more delay. 80-150 typical.
#define GYRO_LPF_CUTOFF_HZ 100
// D-term biquad LPF cutoff (Hz). Usually lower than gyro. 60-120 typical.
#define DTERM_LPF_CUTOFF_HZ 80
// Gyro notch filter (Hz). Set to 0 to disable. Targets motor noise frequency.
#define GYRO_NOTCH_CENTER_HZ 0
#define GYRO_NOTCH_WIDTH_HZ 30
```

### Tier 2: USE_RACING (config.h flag)

Performance optimizations borrowed from Betaflight for aggressive/racing flying. These modify PID behavior and stick response for sharper handling. Not for beginners.

**Enable in config.h:** `#define USE_RACING`

| Feature | Compute cost | Description |
|---------|-------------|-------------|
| Feed-forward term | ~3 multiplies/axis | Adds setpoint derivative to PID output. Improves stick response speed without increasing P gain. Reduces PID lag during fast maneuvers. |
| TPA (Throttle PID Attenuation) | ~2 multiplies | Reduces PID gains at high throttle to prevent oscillation. Configurable start point and curve. |
| Setpoint smoothing | ~3 multiplies/axis | Low-pass filter on stick input to reduce step changes. Smoother stick response while keeping fast reaction. |
| Air mode | ~1 branch | Keeps PID active even at zero throttle, allowing full attitude control during low/no throttle maneuvers (flips, rolls). |
| Expo curves | ~4 multiplies/axis | Non-linear stick response. Gentle near center, aggressive at extremes. Configurable exponent. |

**Total overhead when enabled**: ~30-40 extra floating-point multiplications per tick. Negligible.

**Config.h parameters** (only exist when `USE_RACING` is defined):
```c
// Feed-forward gain (0.0 = disabled, 0.5-2.0 typical)
#define FF_ROLL 0.0f
#define FF_PITCH 0.0f
#define FF_YAW 0.0f
// TPA: start reducing at this throttle (0.0-1.0), 0.65 typical
#define TPA_BREAKPOINT 0.65f
#define TPA_RATE 0.5f   // How much to reduce (0.0 = none, 1.0 = full)
// Setpoint smoothing cutoff (Hz). 0 = disabled.
#define SETPOINT_SMOOTH_CUTOFF_HZ 0
// Air mode: keep PID active at zero throttle
// #define USE_AIRMODE
// Expo (0.0 = linear, 0.5 = moderate, 0.8 = aggressive)
#define EXPO_ROLL 0.0f
#define EXPO_PITCH 0.0f
#define EXPO_YAW 0.0f
```

### Tier interaction

- **Base alone**: Stable flight, works with any hardware. Good for hovering, filming, testing.
- **Base + Optimization**: Better flight quality on cheap/noisy hardware. Good for DIY builds with budget components.
- **Base + Racing**: Sharper control response for aggressive flying. Good for FPV and acro with good hardware.
- **Base + Optimization + Racing**: Full feature set. Both tiers can be enabled simultaneously.

All three tiers use the same `#ifdef` pattern as existing features. No runtime overhead for disabled tiers.

### Sources
- [Betaflight Filtering 101](https://oscarliang.com/betaflight-filtering/)
- [Betaflight Gyro & Dterm Filtering Recommendations](https://www.betaflight.com/docs/wiki/guides/archive/Gyro-And-Dterm-Filtering-Recommendations-3-1)
- [Emuflight: LowPassFilters](https://emuflight.github.io/filters/LowPassFilters.html)
- [ArduPilot: Managing Gyro Noise with Notch Filters](https://ardupilot.org/copter/docs/common-imu-notch-filtering.html)
