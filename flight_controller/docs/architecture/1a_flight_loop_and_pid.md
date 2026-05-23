# Architecture — Level 1a: Flight Loop & PID Feature Tiers

> Up: [Level 0 — System Overview](0_system_overview.md) ·
> Index: [architecture/](INDEX.md)

The flight loop is the one piece of code that is **always** compiled and must
**never** be blocked. On Teensy it is `loop()`; on ESP32 it is
`flightControlTask()` pinned to Core 0. Both call the same `flightControl()`
body.

## Loop iteration

```mermaid
flowchart TB
    START["Loop iteration @ LOOP_FREQUENCY_HZ (1-2 kHz)"]
    START --> CMD["getCommands() — RadioComm fills channel_X_pwm"]
    CMD --> IMU["getIMUdata() — gyro + accel"]
    IMU --> MADG{"Attitude filter"}
    MADG -->|"6DOF (no mag)"| M6["Madgwick6DOF()"]
    MADG -->|"9DOF (MPU9250 + mag)"| M9["Madgwick() 9DOF"]
    M6 --> DES["getDesState() — sticks to setpoints"]
    M9 --> DES
    DES --> ARM["armedStatus()"]
    ARM --> MODE{"Flight mode<br/>(compile-time)"}
    MODE -->|USE_RATE_CONTROLLER| RATE["controlRATE()"]
    MODE -->|USE_ANGLE_CONTROLLER| ANG["controlANGLE()"]
    RATE --> MIX["controlMixer() — PID to per-motor"]
    ANG --> MIX
    MIX --> SCALE["scaleCommands()"]
    SCALE --> CUT["throttleCut() — safety"]
    CUT --> OUT["commandMotors() — PWM out"]
    OUT --> RGOV["loopRate() — hold target frequency"]
    RGOV --> START
```

## PID feature tiers

Each PID axis (roll/pitch/yaw) computes P, I and D terms. **What filtering and
extras wrap those terms is selected at compile time** via three tiers. Higher
tiers add code only when their `#ifdef` is set — zero runtime cost otherwise.

```mermaid
flowchart TB
    subgraph base["Base tier (always compiled)"]
        B1["P + I + D terms"]
        B2["I-term clamping (anti-windup)"]
        B3["D-term PT1 low-pass filter"]
        B4["Derivative on measurement (rate mode uses -GyroX)"]
    end
    subgraph opt["USE_OPTIMIZATION (cheap/noisy hardware)"]
        O1["Biquad D-term LP (replaces PT1 — steeper rolloff)"]
        O2["Biquad gyro LP + gyro notch filter"]
        O3["Accel 2nd-order LP"]
    end
    subgraph race["USE_RACING (FPV / acro)"]
        R1["Feed-forward (setpoint derivative)"]
        R2["TPA (throttle PID attenuation)"]
        R3["Expo on stick input"]
        R4["Air mode + setpoint smoothing (PT1 on sticks)"]
    end
    base --> opt --> race
```

- **Base** is the honest minimum: a PID with anti-windup and a single-pole
  D-term filter. Good enough to fly a stable frame.
- **USE_OPTIMIZATION** swaps the D-term PT1 for a biquad and adds gyro/accel
  filtering — for boards with noisy IMUs or cheap MCUs that need cleaner
  signals.
- **USE_RACING** adds the responsiveness features (feed-forward, TPA, expo,
  air mode) that acro/FPV pilots expect.

The tiers are independent flags; you can enable optimization without racing or
vice-versa.

## Source anchors

- Loop body + call order: `src/main.cpp` `flightControl()`.
- Loop driver: `flightControlTask()` (ESP32, Core 0) / `loop()` (Teensy).
- PID + tiers: `src/control.cpp` (`controlRATE()`, `controlANGLE()`,
  `controlMixer()`) — `#ifdef USE_OPTIMIZATION` and `#ifdef USE_RACING`
  blocks gate the extras.
- Biquad / PT1 DSP primitives: `include/filters.h`, `src/filters.cpp`.
- Mode selection: `include/config.h` `USE_RATE_CONTROLLER` /
  `USE_ANGLE_CONTROLLER`.
