# Collision Signature on BNO055 LINEARACCEL — Threshold & Window Derivation

Status: RESEARCH. Written 2026-05-19 to support the sibling agent who is implementing a `COLLISION` event detector in `balance_app.cpp`. The current default is **15 m/s² sustained 2 ticks** (1.5 g, 10 ms at 200 Hz). This doc validates / sharpens that number against datasheet specs, published collision-detection literature, and the project's own scope rule that thresholds must be derived, not declared. It also enumerates the false-positive modes the implementation must guard against — most importantly the documented BNO055 register-tear ±255 LSB glitch, which on the LIA scale **is exactly 2.55 m/s²** and would normally pass a 1.5 g threshold cleanly, *but* a single-tick spike of much greater magnitude (saturated-MSB tear) can easily defeat any threshold below ~10 g.

This doc does **not** replicate the motor-null-space HELD design ([research_motor_null_space_handling_detection.md](research_motor_null_space_handling_detection.md)) — HELD is a *low-magnitude, sustained, idle-motor* test; collision is a *high-magnitude, transient, any-motor-state* test. They are complementary detectors that share one sensor (`VECTOR_LINEARACCEL`).

---

## 1. BNO055 LINEARACCEL specifications (datasheet citations)

| Parameter | Value | Source |
|---|---|---|
| Accelerometer noise density | 150 µg/√Hz (typ), 190 µg/√Hz (max), normal mode, T=25 °C | Bosch BNO055 datasheet, electrical-characteristics table (acc) |
| Accel range in fusion mode | ±4 g (locked by fusion algorithm — not configurable) | Datasheet §3.3.2; MathWorks BNO055 Simulink doc |
| Accel bandwidth in NDOF | 62.5 Hz (default, locked by fusion) | Adafruit / Bosch fusion-mode config |
| **Fusion data output rate (NDOF)** | **100 Hz** | Datasheet §3.3.2; Adafruit/Bosch forum threads |
| LIA register scaling | 100 LSB = 1 m/s² (int16, ±4 g → ±39.23 m/s², ±3923 LSB) | Datasheet §3.6.5; `Adafruit_BNO055.cpp:445-450` |
| Fusion group delay (quat + LIA) | ~20–40 ms (filter dependent; matches our `latency_budget_2026-05-12.md` measurement) | `latency_budget_2026-05-12.md`; Bosch fusion-pipeline reasoning |
| Resting LIA RMS noise | ≈ 0.04–0.08 m/s² (1σ) once calibrated; envelope grows when accel cal drops below 3 | Adafruit forum noise-characterisation threads |
| **Register-tear glitch magnitude** | **±255 LSB = ±2.55 m/s² (LSB-only tear)** or much larger when MSB tears (potentially full-scale ±39 m/s²) | Bosch sensors forum: "BNO055 spike of Linear Acceleration and Angular Velocity"; fix is documented as I²C **burst read** (datasheet §3.7 data register shadowing) |

**Effective sample rate for our stack.** The BNO055 itself produces a *new* LIA value at 100 Hz. The control loop reads at the main-loop rate, which the audit estimates as 250–333 Hz at 100 kHz I²C (~3.3 ms per read_sensors). The control ISR consumes the latest cached value at 200 Hz. **Net: we sample the LIA stream at 2× oversampling.** Two consecutive ticks ≈ 10 ms ≈ 1.0 unique LIA samples on average — a "sustained 2-tick" requirement is **really a "single unique LIA sample" requirement**. This is a critical observation: 2-tick sustain at 200 Hz does **not** give independent debounce against a single bad sample; it only filters intra-sample readout glitches.

**Latency budget.** LIA is delayed by the fusion filter (~30 ms) plus I²C (~3 ms) plus loop staleness (≤5 ms) ≈ **38 ms end-to-end**. A collision detector firing at tick *t* fires on data that's a third-of-a-tick worth of physical-world time *in the past*. Bot has already moved 0.5 cm at 0.15 m/s. Fine for safety actions; useless for "stop before impact."

---

## 2. Collision signature — order-of-magnitude

For a 1 kg bot at v = 0.3 m/s hitting a rigid wall, the momentum change is Δp = 0.3 N·s. If the impact deceleration is roughly half-sine over duration τ, the peak force is F_peak ≈ π·Δp / (2τ), and the peak body-frame acceleration is a_peak = F_peak / m.

| Impact stiffness | τ (contact time) | a_peak | Notes |
|---|---|---|---|
| Hard plastic on drywall | ~5 ms | ~95 m/s² (≈ 9.6 g) | Saturates the BNO055 ±4 g fusion clip — peak read = +39 m/s² for one or two LIA samples then ringdown |
| Compliant tire on wall | ~20 ms | ~24 m/s² (≈ 2.4 g) | Within range; clear above noise floor; lasts 2–3 LIA samples |
| Soft bumper / foam | ~50 ms | ~9 m/s² (≈ 0.9 g) | Below 15 m/s² threshold — would be missed |
| Operator gentle push (0.1 m/s, 100 ms) | ~100 ms | ~1.5 m/s² | Below threshold by intent (not a collision) |
| Operator firm shove (1 m/s, 50 ms) | ~50 ms | ~30 m/s² | Triggers — appropriate, this *is* a forced disturbance |

**Distinguishability from balance-recovery transients.** A balance recovery from a 5° step disturbance with our converged controller (ω_n = 8 rad/s, ζ = 0.7) produces a peak pitch-axis angular acceleration of roughly `K_motor · u_peak ≈ 0.3 deg/s² per PWM · 200 PWM = 60 deg/s² ≈ 1.0 rad/s²`. The linear acceleration this creates at the IMU (mounted ~10 cm above axle) is `r · α ≈ 0.10 · 1.0 = 0.10 m/s²` — utterly negligible compared to any collision. The IMU also sees the wheels' direct ground-reaction translation: `v̇ = K_motor_to_translation · u`. Even at saturation (u=±255) this is bounded by the motor's max wheel acceleration, typically <0.5 g for our small DC motors with rubber on smooth flooring. **Self-generated body accel from the balance loop is ≤ 5 m/s² by construction of the plant. A 15 m/s² floor is comfortably above this.**

Frequency content: collision energy concentrates 50–500 Hz; the BNO055's 62.5 Hz internal bandwidth means most of it is aliased / low-passed into the first 1–2 samples — we see a fat peak, not a ringing waveform. This is consistent with the "frequency-based averaging filter beat threshold and median filter" result in Stubber & Ebner (2009).

---

## 3. Threshold recommendation

### Recommended: **12 m/s² peak OR 8 m/s² sustained 3 ticks, both gated by burst-read + isnan**

| Component | Value | Why |
|---|---|---|
| Peak threshold | **12 m/s² (≈ 1.2 g)** on `|a_lin|` | Captures the soft-bumper / tire-on-wall case (~9 m/s²) that 15 m/s² would miss, while staying ~150× the resting noise floor (0.08 m/s² 1σ → 12 m/s² ≈ 150σ — astronomically improbable as noise). Below the LSB-only register-tear value (2.55 m/s²) is safe; below the saturated-MSB tear (39 m/s²) requires the sustain gate. |
| Sustain alternative | **8 m/s² for 3 ticks (15 ms)** | Catches the soft-impact tail when the peak is missed by sampling phase. 3 ticks ≈ 1.5 LIA samples — actually enforces "≥ 2 consecutive new samples" debounce. |
| Pre-filter | **Burst-read of LIA registers 0x28–0x2D in one I²C transaction** | Eliminates the documented MSB/LSB tear glitch (Bosch forum). This is the single most important guard — without it, *any* threshold gives false positives. |
| Pre-filter | **`isnan(a_lin)` guard + clamp to ±40 m/s²** | Handles TA-3 / TA-4 from the theoretical audit propagating into the new detector. Saturated reads are treated as the saturation value, not propagated. |
| Cooldown | **200 ms after a fire** | Prevents the ringdown / contact-loss / second-impact sequence from triple-counting a single event. |

### Why not the original 15 m/s² / 2 ticks?

1. **15 m/s² misses soft impacts.** Foam bumpers, soft furniture, the operator's hand — all produce ~6–10 m/s² peaks. The operator's stated use case includes "getting pushed," not just wall-hits.
2. **2 ticks at 200 Hz ≠ 2 independent samples** when LIA updates at 100 Hz. Effective debounce is single-LIA-sample. A single saturated-MSB register tear (one sample of ~39 m/s², well above any reasonable threshold) defeats the sustain. **Burst read + isnan are the real protection**, not the tick count.
3. **The peak/sustain split** is a textbook technique (NavX FRC, Stubber & Ebner) — a sharp impact may show one fat sample then drop quickly; a sustained shove shows a broader low plateau. Single-threshold detectors miss one of the two.

### Derivation, not declaration

Per scope rule: thresholds should come from measured data. The right *long-term* answer is what `multi_axis_anomaly_handling_detection.md` proposes for HELD — Welford running variance of `|a_lin|` during RUN, fire when current sample exceeds `μ + Nσ` (N ≈ 8–10) for the chosen window. **The numbers above are a literature-informed bootstrap.** They should be replaced by a runtime-derived threshold after one bench session of LIA logging during normal RUN, the same way `body_heading_unit` is learned in the null-space proposal.

---

## 4. False-positive enumeration

| # | Mode | Why it triggers | Mitigation |
|---|---|---|---|
| F1 | **BNO055 MSB/LSB register tear** | Separate-byte read with a fusion update in between → MSB and LSB don't match → spike of 2.55 m/s² (LSB-only) up to ±39 m/s² (MSB tear, saturates LIA range) | **Single I²C burst read of 6 bytes 0x28–0x2D.** This is the #1 defect to guard against. Datasheet §3.7 shadow registers exist specifically to make burst reads atomic. Check `bno055.cpp:184-204` template — burst read is already used for raw accel, replicate for LIA. |
| F2 | **Torn float read of cached LIA** (analogue of TA-6 in the audit) | Loop writes 4-byte float; 200 Hz ISR reads it mid-store → wild magnitude or NaN | Wrap `linear_accel_[3]` writes/reads in `ATOMIC_BLOCK(ATOMIC_RESTORESTATE)` exactly as TA-6 requires for `raw_gyro_dps_`. Or use a flip-buffer with a `volatile uint8_t` index. |
| F3 | **Sharp balance recovery from a kick** | If the kick was real, the IMU sees the disturbance accel directly (lateral push → lateral LIA). A 0.5g push *is* a collision-class event. | Accept it — this is a true positive. The detector reports HANDLING / COLLISION; the controller decides whether to soft-cutoff. Operator's stated preference is to detect, not suppress. |
| F4 | **Motor PWM transition noise** (current spike on commutation) | L298N H-bridge switching can radiate into the I²C line → corrupted single byte → likely caught by burst read | Burst read covers most of this. Add 100 nF cap across BNO055 VCC if bench testing shows persistent spikes correlated with PWM transitions. (Already in audit's TA-5 cluster — verify pull-ups.) |
| F5 | **Operator picks bot up** | Vertical lift produces +9.81 m/s² along world-Z over ~100 ms → in body frame this projects across all 3 axes during the tilt → easily 5–10 m/s² peak, may exceed threshold | This **should** trigger — picking up the bot is a handling event. The motor-null-space HELD detector ([research_motor_null_space_handling_detection.md](research_motor_null_space_handling_detection.md)) is the *sustained* version of this; collision is the *transient* version. Both firing is fine. Differentiate via duration: collision → < 50 ms, lift → > 200 ms. |
| F6 | **Cable tug** | Sharp tether jerk during operator interaction → real lateral impulse | True positive. Operator wants to know. |
| F7 | **NDOF mode internal gravity-vector jump** | If accel cal drops, fusion engine re-estimates gravity; LIA briefly shows a step of magnitude up to ~1 g (10 m/s²) over a few samples | Gate detector on `cal_accel >= 2` (sensor consider gravity estimate trusted). Below that, fall back to raw accel minus body-frame gravity (rotated through quat) at the cost of mode coupling. |
| F8 | **Floor seam / wheel on bump** | Wheel hits a 1 mm seam at 0.3 m/s → vertical impulse ~3–5 m/s² for one sample, repeating periodically | Below the 12 m/s² peak threshold. Below 8 m/s² sustained-3. Should not trigger. **If it does in practice**, raise the sustain magnitude before raising the peak. |
| F9 | **Rapid motor reversal in balance loop** | Wheel direction flip generates a brief lateral kick. Magnitude bounded by torque/(m·r). For our motors, ~2 m/s². | Below threshold. |
| F10 | **Loop iteration > 10 ms** (audit §4.2: `s` command jitter) | `raw_gyro_dps_` and (new) `linear_accel_` get stale and may show a *delta* on the next tick that looks like a high derivative | Use `|a_lin|`, not `d/dt(a_lin)` jerk, for the primary threshold (sidesteps phantom-jerk from stale samples). See §5 below. |

**Top FP to guard against:** **F1 (register tear)** — it is documented to produce spikes of magnitude that defeat any reasonable acceleration threshold, and it can fire several times per second on a non-burst-read I²C client. **If the implementer does anything in this doc, it is to confirm the LIA register read is a single 6-byte burst transaction, with the matched-LSB/MSB shadow-register guarantee.**

---

## 5. Supplementary signals — should we add any?

| Signal | Value-add | Cost | Recommendation |
|---|---|---|---|
| `|a_lin|` body-frame magnitude | The primary signal. Direction-agnostic. | Free (sqrt) | **Ship.** |
| Jerk = `d/dt(a_lin)` | Sharper edge for hard impacts; literature standard (NavX, FRC) | +4 floats (prev a), one subtraction; sensitive to loop jitter | **Defer.** Adds value only if the primary `|a|` test misses hard impacts. With our oversampled stream and `~38 ms` fusion lag, jerk computed at 200 Hz is dominated by the LIA group-delay phase noise. Revisit if peak threshold gives too many borderline misses. |
| `|d/dt(pitch_deg)|` (we already track it) | Catches the "bot kicked over" case where the impact rotates more than it translates | Free — already in `gyro_pitch_dps_` | **Cross-arm.** Fire COLLISION if `|a_lin| > peak OR (|a_lin| > 6 AND |gyro_pitch| > 200 deg/s)`. The second clause covers the "kicked-over" mode that pure linear accel underestimates. |
| Command-vs-observed accel mismatch (motor-null residual from research_motor_null_space_handling_detection.md) | Best for "handled while idle" (HELD); also useful for collision since the IMU sees lateral accel a motor command cannot explain | Already proposed for HELD; reuse the same `a_residual` term | **Reuse, don't duplicate.** When the null-space detector lands (Phase 2.7), `|a_residual| > 8 m/s²` is a stronger collision-class signal than raw `|a_lin|` because it removes the legitimate forward-thrust component. Until then, raw `|a_lin|` is the right primary. |
| Cross-axis ratio (impact direction is laterally biased) | A true wall collision usually arrives perpendicular to heading → big lateral component, small longitudinal | Two dot products | **Optional logging only.** Useful for *classification* (front vs side hit, which the operator might want for adaptive behaviour), not for *detection*. Don't gate the detector on it — a head-on backward-rolling impact would be missed. |
| Welford μ/σ of `|a_lin|` during RUN | Replaces hardcoded threshold with a learned `μ + Nσ` rule. Matches scope rule "no hardcoded thresholds." | ~40 B RAM + ~80 B flash (already costed in `multi_axis_anomaly_handling_detection.md`) | **Phase 2.7c — add once null-space lands and we have a μ/σ infrastructure for HELD anyway.** Bootstrap with the literature value (12 m/s²); replace with `max(12, μ + 10σ)` once Welford is wired. |

---

## 6. Concrete recipe for the sibling agent

```text
1.  On read_sensors(): burst-read 6 bytes from 0x28–0x2D in one Wire transaction.
    DO NOT use two getInt16() calls. Mirror the pattern at bno055.cpp:184-204.
2.  Convert to float a_lin[3] = raw / 100.0f.  Clamp each to ±40 m/s² to neutralise
    a saturated-MSB tear that survives the burst-read protection.
3.  Apply isnan guard at the top of any function that consumes a_lin — same
    discipline TA-3 / TA-4 demand for PlantIdentifier and OnlineMountingEstimator.
4.  Wrap a_lin[3] writes (loop side) and reads (ISR side) in ATOMIC_BLOCK
    (same pattern TA-6 demands for raw_gyro_dps_).
5.  Compute mag = sqrtf(a_lin[0]^2 + a_lin[1]^2 + a_lin[2]^2).
6.  Two-arm detector:
       PEAK   = (mag > 12.0f)
       SUSTAIN= (mag > 8.0f for >=3 consecutive ticks)
       KICK   = (mag > 6.0f AND |gyro_pitch_dps| > 200.0f)
    Fire COLLISION on PEAK || SUSTAIN || KICK.
7.  Cooldown 200 ms after any fire — no re-arm until LIA mag drops below 4 m/s²
    for 100 ms AND cooldown timer expired.
8.  Gate on cal_accel >= 2 to suppress fusion-jump FP (F7).
9.  Expose mag + last_fire_ms + fire_count in PlantIdentifierStatus or a new
    CollisionStatus struct so telemetry can show what's happening.
10. Schedule replacement of constants with Welford-learned mu+10*sigma once
    Phase 2.7 lands the running-stats infrastructure.
```

Flash estimate on Uno: ~150 B (burst read wrapper if not already there + sqrt + branches + cooldown timer). RAM: ~28 B (3 floats LIA + prev mag + counters + cooldown). Within current 1702 B free budget per scope.md.

---

## 7. References

1. **Bosch BNO055 datasheet (BST-BNO055-DS000)** — §3.3 fusion modes (NDOF 100 Hz output, accel locked to ±4 g / 62.5 Hz BW), §3.6.5 linear-acceleration register definition (100 LSB = 1 m/s²), §3.7 burst-read data-register shadowing, electrical-characteristics table (accel noise density 150 µg/√Hz typ). [Bosch PDF](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bno055-ds000.pdf) and [Adafruit mirror](https://cdn-shop.adafruit.com/datasheets/BST_BNO055_DS000_12.pdf).
2. **Adafruit forum, "BNO055 values corrupted by spikes"** — community confirmation that ±255 LSB tear spikes resolve via burst read. [forum thread](https://forums.adafruit.com/viewtopic.php?f=19&p=531590).
3. **Adafruit forum, "Unexpected linear acceleration data from BNO055 at rest"** — resting-noise envelope, sensitivity to accel calibration level. [forum thread](https://forums.adafruit.com/viewtopic.php?f=19&p=377316).
4. **Bosch Sensortec community, "BNO055 spike of Linear Acceleration and Angular Velocity"** — root-cause analysis of register-tear glitches with the burst-read fix. [community thread](https://community.bosch-sensortec.com/mems-sensors-forum-jrmujtaw/post/bno055-spike-of-linear-acceleration-and-angular-velocity-Ry6olQzRFRRbUI1).
5. **Stubber & Ebner, "Acceleration Based Collision Detection with a Mobile Robot," IEEE 2009** — comparison of simple-threshold, running-median, and frequency-based filtering for collision detection; frequency-based won. [IEEE Xplore](https://ieeexplore.ieee.org/document/8675623) / [author PDF](https://stubber.math-inf.uni-greifswald.de/~ebner/resources/uniG/collisionDetectionIRC.pdf).
6. **NavX-MXP collision-detection example (Kauailabs / FRC)** — jerk-threshold pattern with operator-tuned threshold; confirms "no universal number, calibrate per robot." [Kauailabs docs](https://pdocs.kauailabs.com/navx-mxp/examples/collision-detection/).
7. **MathWorks BNO055 IMU Sensor Simulink doc** — fusion-mode output rate 100 Hz, accel range locked to ±4 g, internal bandwidth not user-configurable. [MathWorks](https://www.mathworks.com/help/simulink/supportpkg/arduino_ref/bno055imusensor.html).
8. Internal repo cross-refs:
   - [research_motor_null_space_handling_detection.md](research_motor_null_space_handling_detection.md) — HELD detector; collision is the transient counterpart sharing the same LIA sensor.
   - [theoretical_audit_balance_stack.md](theoretical_audit_balance_stack.md) — TA-3, TA-4, TA-5, TA-6 must be heeded by the new detector (isnan guards, I²C 400 kHz, atomic write of cached vector).
   - [latency_budget_2026-05-12.md](latency_budget_2026-05-12.md) — fusion group-delay measurement (~30 ms) supporting the 38 ms end-to-end figure.
   - [multi_axis_anomaly_handling_detection.md](multi_axis_anomaly_handling_detection.md) — Welford running-stats infrastructure that will eventually replace the hardcoded 12 m/s² constant per the scope rule.
   - [scope.md §"The rule"](../scope.md) — derivation-not-declaration mandate.
