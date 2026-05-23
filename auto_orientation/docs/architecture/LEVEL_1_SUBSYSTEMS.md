# Level 1 — Per-subsystem

Four subsystem diagrams, one per concern. Each is grounded in the cited source. If you only care about one subsystem, jump straight to it.

[← Level 0](LEVEL_0_SYSTEM_OVERVIEW.md) · [Index](INDEX.md) · [Level 2 — components →](LEVEL_2_COMPONENTS.md)

- [(a) Mega adaptive control stack](#a-mega-adaptive-control-stack)
- [(b) Uno minimal path + Python tuner](#b-uno-minimal-path--python-tuner)
- [(c) Sensor + odometry pipeline](#c-sensor--odometry-pipeline)
- [(d) Persistent-storage HAL](#d-persistent-storage-hal)

---

## (a) Mega adaptive control stack

Source: `src/applications/balancing_robot/balance_app.{h,cpp}`, `src/control/{pid_controller,plant_identifier,position_loop}.*`.

The Mega app is a state machine that **measures its own plant before balancing** and **keeps adapting while it balances**. The control law is a cascade: an outer position loop nudges the pitch setpoint; an inner PID holds pitch; an RLS identifier retunes the PID online.

### Control dataflow (RUN state)

```mermaid
flowchart LR
    BNO["BNO055 (NDOF)"] --> PITCH["pitch_deg, raw gyro, linear accel"]
    ENC["wheel encoders"] --> VEL["mean wheel velocity (m/s)"]
    MOUNT["OnlineMountingEstimator"] --> OFF["live offset"]
    PITCH --> CORR["corrected_pitch = pitch − offset"]
    OFF --> CORR
    VEL --> POS["PositionLoop (outer)<br/>leaky integral → nudge ±2°"]
    POS --> SP["pitch setpoint"]
    CORR --> PID["inner PID<br/>compute(corrected_pitch, dt)"]
    SP --> PID
    PID --> PWM["PWM ±255"]
    PWM --> L298["L298N driver"]
    L298 --> BOT["robot"]
    BOT -.feedback.-> BNO
    BOT -.feedback.-> ENC
    PITCH --> RLS["PlantIdentifier (RLS)<br/>learn K_motor → Kp/Kd/Ki targets"]
    PWM --> RLS
    RLS -.rate-limited gains.-> PID
    PITCH --> COLL["3-gate collision detector"]
    COLL -.latch.-> HELD["→ HELD"]
```

> The outer loop and RLS are `#ifdef USE_WHEEL_ENCODERS` / always-on respectively; on a Mega build without encoders, `step_run_` keeps a fixed zero setpoint and skips the cascade.

### State machine

`BalanceAppState` (balance_app.h:88). FALLEN is sticky — only an operator command restarts. `AUTO_TUNE` exists in the enum for ABI but is unreachable (retired by BOOTSTRAP).

```mermaid
stateDiagram-v2
    [*] --> IDLE
    IDLE --> CAPTURE_MOUNTING: short press
    IDLE --> BOOTSTRAP: enter_bootstrap()
    IDLE --> CHAR_ACT: enter_characterise_actuator()
    IDLE --> PWM_DISCOVERY: enter_pwm_discovery() [encoders]
    CAPTURE_MOUNTING --> BOOTSTRAP: capture good
    CAPTURE_MOUNTING --> IDLE: capture fail / abort
    BOOTSTRAP --> RUN: K measured, gains derived
    BOOTSTRAP --> IDLE: failure_reason 1..7
    CHAR_ACT --> IDLE: stiction measured
    PWM_DISCOVERY --> IDLE: MIN/MAX found or timeout
    RUN --> HELD: collision / lift / encoder stall
    HELD --> RUN: resume (dwell or short press)
    HELD --> FALLEN: tilt persists
    RUN --> FALLEN: tilt > limit
    FALLEN --> RUN: operator restart (short press / c / R)
    RUN --> IDLE: long press abort
    HELD --> IDLE: long press abort
```

The BOOTSTRAP → gain-derivation internals and the PositionLoop cascade structure are detailed in [Level 2](LEVEL_2_COMPONENTS.md).

---

## (b) Uno minimal path + Python tuner

Source: `src/applications/balancing_robot_uno/{uno_balance_app,tune_storage,tuning_session}.*`, `tools/sim/{brute_tune,balance_bot_sim}.py`.

No state machine, no learning. The pipeline is literally `pitch → PID → PWM`. Gains arrive from one of two **offline / on-device** paths — never computed during flight.

```mermaid
flowchart TD
    subgraph OFFLINE["Offline (host PC) — optional cold-start seed"]
        SIM["balance_bot_sim.py<br/>plant + IMU + PID model"]
        BRUTE["brute_tune.py<br/>grid / random / GA search"]
        TMPL["balance_constants_template.h.in"]
        SIM --> BRUTE
        BRUTE --> TMPL
        TMPL --> HDR["balance_constants.h<br/>(generated seed)"]
    end

    subgraph DEVICE["On-device"]
        GUIDED["guided P→I→D tuning session<br/>(arduino_uno_tuning env)"]
        EEPROM["EEPROM tune block<br/>(tune_storage, CRC8)"]
        GUIDED --> EEPROM
    end

    HDR -.compile-time seed.-> BEGIN["UnoBalanceApp::begin()"]
    EEPROM -.runtime, wins if present.-> BEGIN
    BEGIN --> PID["fixed PID(Kp,Ki,Kd)"]

    subgraph LOOP["5 ms ISR loop"]
        IMU["BNO055.read() − PITCH_OFFSET"] --> GATE{"|pitch| > TIP_CUTOFF?"}
        GATE -->|yes| STOP["stop motors (loop stays alive)"]
        GATE -->|no| PID
        PID --> PWM["PWM → both wheels (L298N)"]
    end
```

> Boot precedence (uno_balance_app.h): a valid EEPROM tune block wins; otherwise the `balance_constants.h` seed is used. The brute-force tuner is *demoted to an optional seed generator* per the 2026-05-19 pivot — it is not the operational tuning loop.

---

## (c) Sensor + odometry pipeline

Source: `src/sensors/{sensor_base.h,bno055,bno085,wheel_encoder}.*`, `src/navigation/{mounting_calibration,online_mounting_estimator}.*`, `src/math/quaternion.*`.

Two independent feeds: an **orientation** feed (IMU → quaternion → pitch) and an **odometry** feed (encoders → velocity / position). They converge at the application.

```mermaid
flowchart TD
    subgraph ORIENT["Orientation"]
        IMU["BNO055 / BNO085<br/>(OrientationSensor)"] --> QUAT["quaternion (w,x,y,z)"]
        QUAT --> EULER["quaternion_to_euler_degrees()<br/>roll / pitch / yaw"]
        EULER --> MNT["mounting offset<br/>(MountingCalibration one-shot +<br/>OnlineMountingEstimator drift)"]
        MNT --> CPITCH["corrected pitch"]
    end
    subgraph ODO["Odometry (Mega only)"]
        ENC["WheelEncoder L / R<br/>(quadrature, ISR 4× count)"] --> TICKS["ticks"]
        TICKS --> WVEL["read_velocity_mps()<br/>windowed velocity"]
        WVEL --> WMEAN["mean tread velocity"]
    end
    CPITCH --> APP["application control loop"]
    WMEAN --> APP
    ENC --> STALL["stalled() detector"]
    STALL -.HELD.-> APP
```

> The BNO085 + GPS + EKF *navigation* fusion (Phase 3) is documented separately in [getting_started/ARCHITECTURE.md](../getting_started/ARCHITECTURE.md); the balancing apps use only the orientation feed of that stack.

---

## (d) Persistent-storage HAL

Source: `src/storage/persistent_storage.h` + `persistent_storage_{avr,teensy,esp32,native}.cpp`, `src/config/calibration_storage.*`.

A single `ps::` namespace (begin / read / write / commit / clear / capacity) with the concrete backend chosen at **compile time** by architecture macros. This is the fix for KI-1 (calibration storage previously used `<EEPROM.h>` directly, which silently failed on ESP32).

```mermaid
flowchart TD
    CALLER["callers<br/>calibration_storage.cpp · mounting record ·<br/>Uno tune_storage · encoder cal · PWM_DISCOVERY result"]
    CALLER --> PS["ps:: begin / read / write / commit / clear / capacity"]
    PS --> SEL{"compile-time backend<br/>selection via macros"}
    SEL -->|ARDUINO_ARCH_AVR| AVR["persistent_storage_avr<br/>on-chip EEPROM, ~3.3 ms"]
    SEL -->|__IMXRT1062__| TEENSY["persistent_storage_teensy<br/>flash-emulated EEPROM"]
    SEL -->|ARDUINO_ARCH_ESP32| ESP32["persistent_storage_esp32<br/>NVS Preferences blob"]
    SEL -->|none of above| NATIVE["persistent_storage_native<br/>heap buffer (host tests)"]
```

See [implementation/persistent_storage.md](../implementation/persistent_storage.md) for the per-backend timing and the `native_test` `build_src_filter` exclusion.
