# Auto-PID Tuning Research

Research target: a generic, sensor-agnostic auto-tuner usable for (1) inverted pendulum / self-balancing robot on Arduino Mega (AVR, 8 KB SRAM / 256 KB flash), (2) multirotor flight controllers on Teensy 4.x / ESP32, and (3) any single-axis PID loop. The goal is a single C++ strategy framework that compiles down to only the algorithm(s) selected, so AVR builds stay small.

## Recommendation Summary

- **Inverted pendulum (primary target):** use **amplitude-limited relay feedback (Åström–Hägglund)**. Safest for a constrained stand, converges in 4–6 oscillations (~5–10 s), and fits in <1 KB flash on AVR. The relay's symmetric switching is bounded so the cart/wheel never commands enough effort to topple the stand.
- **Multirotor:** use **closed-loop relay feedback per axis with throttle hold and ±5°/s amplitude limit**, gated by an arm switch and tilt-divergence watchdog. RLS model-ID is an attractive future upgrade once a quadcopter model library exists, but pure relay is the only method that needs no prior model and still terminates safely on a 1 KB MCU budget.
- **Generic single-axis:** ship **relay-feedback as the default strategy**; expose **twiddle/coordinate-descent** as an optional offline-style fallback for plants where forced oscillation is undesirable (e.g., thermal loops, valve position). Both share the same `AutoPIDTuner` strategy interface and only one is compiled in per build.

---

## 1. Algorithm Comparison

Cost numbers are AVR-GCC 7.x `-Os` estimates from the static footprint of similar implementations (e.g., the dRehmFlight PID core in `flight_controller/`, the Arduino PID Autotune Library, and prototyping in PlatformIO). RAM is the per-instance state struct; flash is the algorithm's `.text` only (not its trig/float-lib dependencies).

| Algorithm | Convergence | RAM (AVR) | Flash (AVR) | Needs oscillation? | Needs model? | Safety profile | Best app class |
|---|---|---|---|---|---|---|---|
| **Relay-feedback (Åström–Hägglund 1984)** | 4–6 cycles (~5–15 s) | ~80 B | ~600–900 B | Yes (bounded) | No | Excellent — relay amplitude is *the* safety knob | Pendulum, drone (per axis), generic |
| **Revised relay w/ hysteresis (Hägglund–Åström 2002)** | 4–6 cycles | ~110 B | ~900–1200 B | Yes (smaller swing) | No | Excellent — hysteresis rejects noise, reduces over-excitation | Pendulum, drone, noisy generic |
| **Ziegler–Nichols ultimate-cycle (manual K_u/T_u)** | Ramp-up loop until critical gain (~30–120 s, very unsafe) | ~40 B | ~300 B | Yes (sustained) | No | Poor — drives plant to instability; cannot bound amplitude | Bench plants only, not pendulum, not drone |
| **Extremum-seeking (ESC)** | Slow (30 s – minutes) | ~150 B | ~1.2–1.8 KB | Sinusoidal perturbation | No | Medium — small perturbations, but slow & needs scalar cost | Drone hover refinement, generic |
| **Fuzzy auto-tuner** | Continuous online | ~250 B + rule LUT (~200 B) | ~2–3 KB | No | Rule base | Medium — adapts gradually, but rules are plant-specific | Generic / supervisory adjustment |
| **RLS model-ID + analytical PID (e.g., AMIGO, pole placement)** | One excitation burst (~3–10 s) + algebra | ~300–500 B (2nd-order: P, θ, K) | ~3–5 KB (matrix ops, log/exp) | Step or chirp | Yes (chosen model order) | Good if model order matches; risky if mismatched | Drone (offline analysis), generic with model knowledge |
| **Twiddle / coordinate descent** | 20–200 trials (minutes; needs reset between trials) | ~60 B | ~500 B | No (just runs setpoints) | No | Excellent for non-destructive plants; bad for unstable ones | Generic single-axis, **not** pendulum (cannot reset) |

Drone-specific note: on Teensy 4.0 (1 MB flash, 1 MB RAM) and ESP32 (4 MB flash, 520 KB RAM) all of these fit trivially. The numbers matter for AVR.

---

## 2. Recommended Algorithm Per Application

### 2.1 Inverted Pendulum on a Stand — Amplitude-Limited Relay Feedback

The pendulum is open-loop unstable, so we cannot disable the controller during tuning. The trick (Åström–Hägglund) is to wrap a *relay* around the existing balance controller's error signal: the controller output is forced to `+d` or `-d` and we measure the resulting limit-cycle period `T_u` and amplitude `a`. Ultimate gain is `K_u = 4d / (π·a)`. Then standard Z-N or AMIGO formulae give Kp, Ki, Kd.

Safety constraint: the relay amplitude `d` is the *only* knob that controls how violently the pendulum swings. Set `d` so the worst-case excursion (≈ `a_max`) stays below the stand's mechanical limit (e.g., ±10° tilt). Start at `d = 0.1·u_max` and ramp up only if oscillation amplitude is too small to measure.

```text
# Relay feedback for inverted pendulum (per axis)
init:
    d         = 0.10 * u_max          # relay amplitude, safety-limited
    hysteresis= noise_rms * 2          # avoid chatter on quiet signal
    n_cycles  = 6                      # converge in ~6 half-periods
    peaks[]   = [], troughs[] = []
    t_cross[] = []
    abort_tilt= 10 deg                 # tripwire

loop every dt:
    e = setpoint - measurement
    if |measurement| > abort_tilt: ABORT(reason=DIVERGENCE)
    if user_button_pressed: ABORT(reason=USER)
    if (millis() - t_start) > 15000:   ABORT(reason=TIMEOUT)

    # Relay with hysteresis
    if e >  hysteresis: u =  d
    if e < -hysteresis: u = -d
    # else hold last u

    apply(u)

    # Detect zero-crossings of e to time the period
    on sign_change(e):
        record t_cross, amplitude since last cross

    if len(t_cross) >= 2*n_cycles:
        T_u = mean(diff(t_cross)) * 2
        a   = mean(|peaks - troughs|) / 2
        K_u = 4*d / (PI * a)
        # AMIGO rules for PI/PID (less aggressive than Z-N classic):
        Kp = 0.45 * K_u
        Ti = 0.85 * T_u    -> Ki = Kp / Ti
        Td = 0.125 * T_u   -> Kd = Kp * Td
        DONE
```

### 2.2 Multirotor — In-Flight Relay With Throttle Hold

Two viable approaches:

1. **In-flight relay (recommended for v1).** Pilot hovers, flips a "tune roll" switch, autopilot holds throttle and yaw and runs amplitude-limited relay on the roll rate loop only. Repeat for pitch and yaw. Relay amplitude expressed as a deg/s setpoint perturbation (e.g., ±5°/s) so the craft only wobbles. Tripwires: tilt > 30°, altitude drop > 1 m, RC failsafe, user "abort" stick gesture → instant exit to manual.
2. **System-ID + pole placement.** Inject a short multisine or doublet on rate command, log to SD/flash, run RLS offline on the laptop to fit a 1st-order rate model `K/(τs+1)`, then place poles for desired closed-loop bandwidth. Safer because excitation is brief and bounded, but requires model order assumption and post-processing — not on-board on AVR.

Trade-off: relay gives you a fully on-board solution at the cost of a wobbly aircraft for ~10 s per axis. RLS gives a smoother flight but needs a ground-station component. Pure relay wins for embedded reference implementation; RLS is a documented "future enhancement."

### 2.3 Generic Single-Axis — Relay Default, Twiddle Fallback

For a non-critical, restartable plant, relay still wins on convergence speed and code size. Twiddle is a useful fallback when *forcing oscillation is unacceptable* (e.g., a temperature loop with thermal limits). The strategy pattern lets both ship in the same codebase.

---

## 3. Generic API Design (header sketch)

The class lives at `src/features/auto_pid_tuner.{h,cpp}` and follows the project's existing module layout (one folder under `src/features/`, one class per file). Strategies are separate translation units selected by `#ifdef` at the bottom of `auto_pid_tuner.cpp`, so AVR builds pull in only one.

```cpp
// src/features/auto_pid_tuner.h  — SKETCH ONLY, do not commit as code yet

namespace floppi {

struct PidGains {
    float kp;
    float ki;
    float kd;
};

struct TuningResult {
    PidGains   gains;
    float      ultimate_gain;       // K_u (relay) or 0 if N/A
    float      ultimate_period_ms;  // T_u  (relay) or 0
    float      est_settling_ms;     // post-tune step estimate
    float      est_phase_margin_deg;
    uint8_t    iterations;
    uint8_t    status;              // see TunerStatus
};

enum TunerStatus : uint8_t {
    IDLE = 0, RUNNING, CONVERGED,
    ABORT_DIVERGENCE, ABORT_TIMEOUT, ABORT_USER, ABORT_BAD_DATA
};

enum AbortReason : uint8_t {
    REASON_NONE = 0, REASON_TILT, REASON_OUTPUT_SAT,
    REASON_TIMEOUT, REASON_BUTTON, REASON_NAN
};

// Strategy interface — every algorithm implements this.
// Kept tiny so AVR vtable cost is ~6 B per instance.
class ITuningStrategy {
public:
    virtual void  begin(float setpoint, float u_max) = 0;
    virtual float step(float measurement, float dt_s) = 0;  // returns control effort
    virtual bool  isDone() const = 0;
    virtual TuningResult result() const = 0;
    virtual ~ITuningStrategy() {}
};

// Safety envelope checked every step, independent of strategy.
struct SafetyLimits {
    float    abs_measurement_max;   // e.g., 10 deg tilt
    float    abs_output_max;        // hard clamp on returned u
    uint32_t timeout_ms;            // overall tuner timeout
    bool     allow_user_abort;      // poll abortRequested()
};

class AutoPIDTuner {
public:
    AutoPIDTuner(ITuningStrategy& strategy, const SafetyLimits& limits);

    void  start(float setpoint, float u_max);
    float update(float measurement, float dt_s);  // call from PID loop
    void  requestAbort(AbortReason r);

    bool          isRunning()  const;
    bool          isFinished() const;
    TunerStatus   status()     const;
    AbortReason   abortReason()const;
    TuningResult  result()     const;             // valid when CONVERGED

private:
    ITuningStrategy& strat_;
    SafetyLimits     lim_;
    TunerStatus      st_;
    AbortReason      abort_;
    uint32_t         t_start_ms_;
};

// Concrete strategies — each in its own .cpp, compiled in via #ifdef.
//   src/features/tuning/relay_feedback.cpp     -> USE_TUNER_RELAY
//   src/features/tuning/twiddle.cpp            -> USE_TUNER_TWIDDLE
//   src/features/tuning/rls_modelid.cpp        -> USE_TUNER_RLS  (Teensy/ESP32 only)
class RelayFeedbackTuner   : public ITuningStrategy { /* ... */ };
class TwiddleTuner         : public ITuningStrategy { /* ... */ };
class RlsModelIdTuner      : public ITuningStrategy { /* ... */ };  // gated by !AVR

} // namespace floppi
```

`TuningResult` is plain-old-data so it serialises directly into EEPROM (Mega: 4 KB EEPROM is plenty). Suggested layout: 4-byte magic, 12 bytes gains, 16 bytes diagnostics, 2-byte CRC = 34 bytes total.

---

## 4. Safety Considerations

Per application, the tuner MUST honour these tripwires before applying any output:

| Concern | Pendulum (Mega) | Multirotor | Generic |
|---|---|---|---|
| Max output clamp | ±0.3·u_max during tune | ±5°/s rate perturbation; throttle hold | ±0.5·u_max |
| Divergence detection | |tilt| > 10° → abort, motors off | |tilt| > 30° or |alt drop| > 1 m → revert to last-good gains, **do not disarm mid-air** unless unrecoverable | configurable |
| Timeout | 15 s | 10 s per axis | 30 s default |
| User abort | physical momentary button on D2 (INT0) | stick gesture (yaw+throttle low for 1 s) + RC failsafe | digital input |
| Failure mode | Motors brake, pendulum falls into padded stand — designed to fall safely | Revert to safe gains in flash; if revert fails, gradual descent (not free-fall disarm) | Output → 0 or last-known-good |
| Data sanity | reject `K_u <= 0`, `NaN`, `T_u < 2·dt` | same + reject `T_u > 2 s` (unphysical for quad rate loop) | same |

The user-abort line should be wired to a hardware interrupt; the ISR sets an `volatile bool abort_flag` polled inside `update()`. On the pendulum, the button must be reachable while the bot is on the stand.

---

## 5. References

1. **Åström, K. J. and Hägglund, T. (1984).** *Automatic tuning of simple regulators with specifications on phase and amplitude margins.* Automatica, 20(5), 645–651. — Original relay-feedback paper; basis of every commercial PID autotuner since.
2. **Hägglund, T. and Åström, K. J. (2002).** *Revisiting the Ziegler–Nichols Step Response Method for PID Control.* Journal of Process Control, 14(6), 635–650. — The AMIGO rules used in §2.1; less aggressive than classic Z-N, better robustness.
3. **Åström, K. J. and Hägglund, T. (2006).** *Advanced PID Control.* ISA Press. — Definitive textbook; chapters 2–4 cover relay tuning, model-ID, and pole placement with the analytical formulae used above.
4. **Ljung, L. (1999).** *System Identification: Theory for the User*, 2nd ed., Prentice Hall. — For the future RLS strategy: recursive least-squares, ARX/ARMAX model orders, and excitation design.
5. **Brunton, S. L. and Kutz, J. N. (2019).** *Data-Driven Science and Engineering*, Cambridge UP, ch. 10 — Modern treatment of extremum-seeking control for the curious reader.
