# Survey of Open-Source Self-Balancing Robot Implementations
Last updated: 2026-05-12
Purpose: ground the auto_orientation framework's design in what real projects actually do.

## 1. Why this survey

Academic papers say "use MRAC / LQR / RL." Real projects mostly use PID with hardcoded hand-tuned gains. Why?

Because PID fits in 2 KB of SRAM, runs at 200 Hz on an 8 MHz AVR with no FPU, and survives the nonlinearities of an L298N driving a geared motor through a polymer tire. A working LQR requires a system-ID step nobody on hobby hardware actually completes. This survey dissects eight open-source balance bots — what they ship, not what their READMEs aspire to.

The reference sketch in this repo (`docs/archive/balancing_robot_reference/SelfBallancingRobot3.ino`) is a representative hand-tuned PID-on-BNO055 bot — `Kp=65, Ki=12, Kd=38`, `PITCH_OFFSET=-8.6°`, 200 Hz, 15-PWM stiction deadband, no fall detection beyond `abs(pitch) < 90`.

## 2. The projects

### 2.1 TKJElectronics / BalancingRobotArduino (Kristian Lauszus)
- **Repo**: https://github.com/TKJElectronics/BalancingRobotArduino
- **Hardware**: ATmega328p + SparkFun 6DOF IMU + dual DC via H-bridge. Bluetooth via USB Host Shield (SPP).
- **Control**: Classical PID on pitch; target angle settable from a remote app.
- **Gain source**: Tunable at runtime over Bluetooth (`'P'/'I'/'D'/'T'` prefixes), persisted via the companion app. No on-robot auto-tune.
- **Sensor fusion**: TKJ's KalmanFilter library — 1D linear Kalman estimating angle + gyro bias from `atan2(accel)` and gyro rate. Single axis. The canonical hobby Kalman; forked thousands of times.
- **Universal?**: No.
- **Asymmetry**: A literal `PIDLeft *= 0.95` line — one scalar, hand-tuned, no feedback. A real hack.
- **Stiction**: None. PID straight to PWM.
- **Pickup**: Pitch-window gates. "Laying down" = pitch ∈ [170°, 190°]; "balancing" = [135°, 225°]. Outside, motors stop. Operator must physically place within ±10° to re-arm.
- **Quality**: ~10-year-old hobby code. Clear structure, no error handling. Pedagogical reference — half the Arduino balance-bot ecosystem traces lineage here.

### 2.2 TKJElectronics / Balanduino
- **Repo**: https://github.com/TKJElectronics/Balanduino
- **Hardware**: ATmega1284P @ 20 MHz (128 KB flash, 16 KB SRAM, 4 KB EEPROM) + MPU-6050 + dual H-bridge + USB Host Shield. Kit.
- **Control**: PID on pitch with encoder-based outer position/velocity loop. 500 Hz.
- **Gain source**: `readEEPROMValues()` at boot. Tunable via Android/PS3/PS4/Wii/Xbox/serial. No auto-tune.
- **Sensor fusion**: Same Lauszus 1D Kalman, with wraparound logic for the 0/360° atan2 discontinuity.
- **Universal?**: No — kit, same chassis every time.
- **Asymmetry**: None. Symmetric drive.
- **Stiction**: None visible. 20 kHz phase-correct PWM but no deadband.
- **Pickup**: ±10° lying-down arming threshold, ±45° tipped-over cutoff. `stopAndReset()` zeroes state. Re-arms automatically near vertical.
- **Quality**: The most polished AVR balance bot in the wild. Multiple controller backends, EEPROM-backed config, sane state machine. Hobby-grade source but a productized kit.

### 2.3 JJRobots / B-ROBOT_EVO2
- **Repo**: https://github.com/jjrobots/B-ROBOT_EVO2  (ESP32 port: https://github.com/ghmartin77/B-ROBOT_EVO2_ESP32)
- **Hardware**: ATmega32U4 + MPU-6050 + 2× NEMA-17 **steppers** via DRV8825. Servo arm for pickup recovery.
- **Control**: Cascaded PID. Inner pitch @ 100 Hz on MPU interrupt; outer position/speed @ 7.5 Hz. Three gain sets: `NORMAL`, `RAISEUP`, `POSITION_CONTROL`.
- **Gain source**: `#define KP 0.32`, `#define KD 0.050`. Hardcoded but app-tunable.
- **Sensor fusion**: Hand-rolled complementary: `angle_f = angle_f*0.99 + MPU_angle*0.01`. No Kalman — steppers are open-loop precise, so the IMU only tracks tilt.
- **Universal?**: No — assumes steppers, the JJRobots chassis and app.
- **Asymmetry**: A single `ANGLE_OFFSET` for weight trim. No per-motor (steppers symmetric).
- **Stiction**: N/A for steppers. Step-skipping under load not detected.
- **Pickup**: The standout. If `|angle| > 74°` motors disable; a **servo arm** swings down to physically stand the bot back up, then firmware switches to `RAISEUP` gains. Only project surveyed with real autonomous self-righting.
- **Quality**: Solid hobby code, magic numbers everywhere (`25.0 // empirical`). Commercial-grade product on hobby-grade source.

### 2.4 Rokasbarasa1 / ESP32-self-balancing-robot
- **Repo**: https://github.com/Rokasbarasa1/ESP32-self-balancing-robot
- **Hardware**: ESP32 (FreeRTOS, C) + MPU6050 + QMC5883L mag + optical encoders + TB6612 + 2× geared DC + dual 18650.
- **Control**: Cascaded PID — pitch (inner), yaw (P-only), speed (mostly I). Author: "PID cannot handle disruptions" — pivots to speed regulation as the dominant outer loop.
- **Gain source**: Hand-tuned. README documents: raise P until oscillation, push I high (100+) to kill drift, balance with D. No auto-tune.
- **Sensor fusion**: Complementary filter. Notable observation: filter ratio must scale with loop rate. Mag used directly for yaw because "gyro Z showed nonsense."
- **Asymmetry**: Not mentioned.
- **Stiction**: Tried explicit deadband, **abandoned it** — "adding the offset made the robot more jittery." Settles for letting the I-term absorb stiction.
- **Pickup**: Not implemented.
- **Quality**: Honest, in-progress. README's engineering notes more useful than the code itself. FreeRTOS occasionally hangs I2C for ~1 s — a real bug, owned publicly. Not production.

### 2.5 RahulnKumar / Self-Balancing-Robot (PSO-tuned PID)
- **Repo**: https://github.com/RahulnKumar/Self-Balancing-Robot
- **Hardware**: **MATLAB simulation only**.
- **Control**: PD with I=0 (author asserts I "is not needed" for this plant).
- **Gain source**: PSO with fitness = `sum(theta_i)` over the simulated episode. Runs offline against a plant model.
- **Quality**: 8 commits, 3 forks. Establishes "PSO finds PID gains for simulated cart-pole" — already known. Not transferable to hardware without sim-to-real work.

### 2.6 aaftabnaim / genetic_self_balancing_robot_webots
- **Repo**: https://github.com/aaftabnaim/genetic_self_balancing_robot_webots
- **Hardware**: **Webots simulation only**. Control: LQR with GA-tuned Q/R matrices. README admits "inconsistent fitness function" was the debugging headache.
- **Quality**: 4 stars, Python PoC. Existence proof that GA+LQR converges in sim. Not deployable.

### 2.7 iamAkshayrao / LQR-BalanceBot
- **Repo**: https://github.com/iamAkshayrao/LQR-BalanceBot (archived)
- **Hardware**: Arduino Mega + GY-87 (MPU6050+HMC5883L+BMP180) + quadrature encoders.
- **Control**: LQR on 6-state model (pitch, pitch-rate, position, velocity, yaw, yaw-rate). Q/R hand-picked; optimal gain computed offline in MATLAB and pasted as constants. No on-robot system ID.
- **Sensor fusion / stiction / pickup**: Not addressed.
- **Quality**: Course project. Proves LQR is implementable on AVR if you precompute, but solves none of the practical problems (model ID, mass robustness, restart-from-fall).

### 2.8 upkie / upkie (mjbots wheeled biped)
- **Repo**: https://github.com/upkie/upkie
- **Hardware**: mjbots qdd100 / moteus torque-controlled brushless servos + onboard SBC + IMU. Order of magnitude more BOM than the others.
- **Control**: Multiple side-by-side — hand-tuned PD wheel-velocity, self-contained MPC, and RL policies via Stable-Baselines3 — all over the same observation/action interface.
- **Gain source**: PD is `gain = [10.0, 1.0, 0.0, 0.1]` — hardcoded, hand-retunable. MPC from a model. RL trained offline.
- **Asymmetry / stiction**: Eliminated by direct-drive torque-controlled brushless actuators. The "buy your way out of the problem" approach.
- **Pickup**: "Lying genuflection" example suggests a recovery primitive.
- **Quality**: Apache-2.0, 3500+ commits, CI/CD, formal docs. **The only production-grade open-source balance bot here.** That costs $1000+ in mjbots parts — precisely why nobody else looks like this.

### 2.9 Companion: jackw01 / arduino-pid-autotuner
- **Repo**: https://github.com/jackw01/arduino-pid-autotuner
- The canonical Arduino implementation of Åström-Hägglund relay-feedback auto-tune (Ziegler-Nichols cousin). Designed for **self-regulating** processes (temperature, flow). README explicitly disclaims general applicability; issues disabled. Hobbyists routinely try it on balance bots with mixed results — the inverted pendulum is integrating, not self-regulating, so relay feedback's assumptions don't hold.

## 3. Patterns across the field

**What every shipping project does (because it just works):**
- PID on pitch. Even when the marketing says "LQR" or "MPC," look at the loop code and you find P/I/D.
- Some flavor of accel+gyro fusion. 1D Kalman (Lauszus lineage) or simple complementary filter (`α=0.98`). Nobody on AVR runs full Madgwick + mag on the balance axis because the magnetometer doesn't help with pitch.
- A pitch-window gate. If `|pitch| > 35°…45°` → motors off. This is the universal "pickup / fallen" detector. No project uses accelerometer-norm anomaly detection or any other sophisticated method.
- Loop rate 100–500 Hz. 200 Hz is the median.
- Manual tuning, persisted in EEPROM, edited over Bluetooth/serial/WiFi.

**What every shipping project skips:**
- Online auto-tune. **Zero** of the eight on-hardware projects do it. The two that auto-tune are simulation-only.
- Motor stiction compensation. The reference sketch's `±15 PWM` deadband is more sophisticated than most. Rokasbarasa1 explicitly tried it and removed it.
- Real fall recovery. Only B-ROBOT EVO2 has it, and only because it ships a literal servo arm.
- Mass / payload adaptation.
- System identification.

## 4. The "universal" gap

No open-source bot is universal. Every project assumes a specific chassis, a specific motor+driver pair, a specific IMU mounted in a specific orientation, a hand-measured mounting offset baked in as a constant, and gains hand-tuned for *that* combination.

The few projects claiming universality via PSO/GA require a simulation model or a bench setup where the bot can fall safely many times — neither is zero-shot. Upkie is universal only within its own actuator family. The combinatorial space of `{AVR/Teensy/ESP32} × {MPU6050/BNO055/BNO085} × {DC/stepper/brushless} × {L298N/TB6612/DRV8825/moteus}` has no project covering even a quarter of it. A framework that takes "any" IMU and "any" motor pair and arrives at functional gains without a Python script and without falls is genuinely unsolved hobby territory.

## 5. What this means for `auto_orientation`

**Adopt:**
- Hardcoded conservative PID gains as the *default* — every shipping bot does this, and it lets the system come up in a known-safe state before any tuning runs. See `docs/findings/conservative_balance_gains_recommendation.md`.
- 1D Kalman on the balance axis (Lauszus pattern) for the BNO055 + MPU6050 paths. The BNO085 already does sensor fusion onboard, so its path skips this.
- Pitch-window gate at ±35° → safe-stop motors. This is the consensus pickup/fallen detector.
- EEPROM-persisted gains, editable over the existing serial / WiFi API. Mirror Balanduino's `readEEPROMValues()` pattern.
- Cascaded inner-pitch / outer-velocity loops (Balanduino, Rokasbarasa1) when encoders are present.
- Mode-switched gain sets (B-ROBOT EVO2's `NORMAL` / `RAISEUP` / `POSITION` idea) — applies cleanly to the planned `IDLE / CAPTURE / TUNE / RUN` state machine.

**Reject:**
- Hand-tuned per-motor asymmetry scalar (Lauszus's `PIDLeft *= 0.95`). Replace with the planned online adaptive balance tracker (`docs/findings/online_adaptive_balance_tracking.md`).
- Magic-number stiction deadband. Make `MIN_MOTOR_PWM` a per-platform config and a *calibration result*, not a constant.
- Off-the-shelf relay auto-tune (jackw01 lineage). Designed for self-regulating processes; the inverted pendulum is integrating. We need the bootstrap protocol from `docs/findings/bootstrap_protocol_unstable_plant.md` instead.

**Where to actually contribute something new:**
- Zero-shot auto-PID for the universal-chassis case. None of the eight projects attempt this. The closest is PSO-tuning in simulation, which requires a model the user doesn't have. A bench-safe bootstrap that establishes "is this bot stable at all" → captures mounting angle → estimates stiction deadband → tunes Kp/Ki/Kd within safety bounds, all on-robot, without falls, would be a real contribution.

## 6. The single best reference

**Balanduino** (https://github.com/TKJElectronics/Balanduino) is the closest in spirit to the auto_orientation goal: kit-shipped, MPU-6050 + 1D Kalman, EEPROM-persisted PID, cascaded inner-pitch / outer-position loops, sane state machine for armed/disarmed/laying-down, multiple control-input backends (Android/PS3/PS4/Wii/Xbox/serial). It is the most polished AVR-class balance robot in the open source ecosystem and demonstrates that a hobby-grade product can be shipped without LQR, without auto-tune, and without anything more exotic than a 1D Kalman filter.

What it teaches us: get the boring parts right (state machine, EEPROM, persistence, multi-input UI) before chasing exotic control theory.

What it doesn't solve, and we should: universality. Balanduino is a kit — same hardware every time. The auto_orientation framework must handle the variation Balanduino sidesteps.

## 7. References

- TKJElectronics / BalancingRobotArduino — https://github.com/TKJElectronics/BalancingRobotArduino
- TKJElectronics / Balanduino — https://github.com/TKJElectronics/Balanduino
- TKJElectronics / KalmanFilter — https://github.com/TKJElectronics/KalmanFilter
- TKJElectronics / Example-Sketch-for-IMU-including-Kalman-filter — https://github.com/TKJElectronics/Example-Sketch-for-IMU-including-Kalman-filter
- TKJ blog, "A practical approach to Kalman filter" — https://blog.tkjelectronics.dk/2012/09/a-practical-approach-to-kalman-filter-and-how-to-implement-it/
- JJRobots / B-ROBOT_EVO2 — https://github.com/jjrobots/B-ROBOT_EVO2
- ghmartin77 / B-ROBOT_EVO2_ESP32 — https://github.com/ghmartin77/B-ROBOT_EVO2_ESP32
- bluino / esp32_wifi_balancing_robot — https://github.com/bluino/esp32_wifi_balancing_robot
- Rokasbarasa1 / ESP32-self-balancing-robot — https://github.com/Rokasbarasa1/ESP32-self-balancing-robot
- RahulnKumar / Self-Balancing-Robot (PSO, sim) — https://github.com/RahulnKumar/Self-Balancing-Robot
- aaftabnaim / genetic_self_balancing_robot_webots (GA+LQR, sim) — https://github.com/aaftabnaim/genetic_self_balancing_robot_webots
- iamAkshayrao / LQR-BalanceBot — https://github.com/iamAkshayrao/LQR-BalanceBot
- kapildevkumara / Self_Balancing_Bot (PID + LQR) — https://github.com/kapildevkumara/Self_Balancing_Bot
- upkie / upkie — https://github.com/upkie/upkie
- jackw01 / arduino-pid-autotuner (Ziegler-Nichols relay) — https://github.com/jackw01/arduino-pid-autotuner
- br3ttb / Arduino-PID-Library (PID_v1, used by the in-repo reference sketch) — https://github.com/br3ttb/Arduino-PID-Library
- In-repo reference sketch — `auto_orientation/docs/archive/balancing_robot_reference/SelfBallancingRobot3.ino`
- In-repo dissection notes — `auto_orientation/docs/archive/balancing_robot_reference/DISSECTION_NOTES.md`

---

*Compiled by: auto_orientation framework planning session, 2026-05-12. Source material verified via WebSearch / WebFetch against the cited repositories.*
