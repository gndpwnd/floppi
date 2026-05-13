# Multi-Axis Anomaly Detection for Handling vs. Balancing

Status: PROPOSAL — replaces the current 2-signal HELD detector. Tier 2.5 work.
Last updated: 2026-05-12

## 1. The user's insight

The user, after watching the bot slam its motors during a pickup the current detector missed:

> "when the robot is picked up the motors go to max so it might not be one specific axis like x/y/z that determines picking up .... it could just be a direction that is not seen often that is a merge between multiple axis make sense?"

In pattern-recognition terms: **handling is a novelty event in the IMU feature space, not a single-axis threshold crossing.** During steady balancing, the bot's state lives on a low-dimensional manifold — pitch-axis gyro ±20 deg/s, roll/yaw near zero, accel ≈ 1 g through body-Z, PID output bounded. Pickup, carry, or lay-on-side all produce feature vectors *off* that manifold along different directions. A single-axis detector only catches departures aligned with the axes it watches; diagonals look "normal" until motors saturate.

The right framing is one-class anomaly detection: learn `p(feature_vector | balancing)`, declare HELD when the current sample is far from it in any direction. Same problem as Schölkopf's one-class SVM, Mahalanobis outlier detection, or statistical process control — shrunk to fit a 200 Hz loop on an ATmega328.

## 2. Why the current 2-signal detector fails

The current HELD/FALLEN machine (`docs/findings/balance_held_fallen_state_machine.md`) watches two scalars: lateral gyro √(gx² + gz²) and accel-deviation ||a| − 9.81|. Both are projections of the 6-D input. Several handling patterns project near-zero on *both*:

- **Slow vertical lift along body-Z.** No rotation → lateral gyro in noise floor. Accel deviates only by the small lift impulse (2 m/s² for 0.3 s, barely above gravity noise). Bot thinks it's balancing; PID sees pitch error grow as wheels spin free; motors slam.
- **Gentle tilted carry.** Operator holds the bot at 15° and walks. Pitch gyro spikes briefly (looks like a recovery), lateral gyro and accel quiet. False negative.
- **Hand on top, no movement.** Operator restrains the bot without rotating it. All signals flat. False negative.

Symmetrically, lenient thresholds **false-positive** during real recoveries (a 5° push gives 80–120 deg/s pitch + 1.5–3 m/s² accel deviation); tighten to catch slow lifts and you drop motors mid-recovery.

The limitation: **any single scalar projection is either too sensitive (false HELDs during balancing) or too lax (missed HELD on novel directions).** Adding more single-axis thresholds only covers more corners of the decision boundary; the diagonals stay exposed.

## 3. The feature vector

We need 4–6 dimensions that span the balancing-vs-handling distinction. Candidates, with AVR cost:

| Feature | Cost | Info? |
| --- | --- | --- |
| body-frame gyro `gx,gy,gz` (deg/s) | free | yes — pitch is balance axis, roll/yaw ~0 |
| body-frame accel `ax,ay,az` (m/s²) | free | yes — ay/ax ~0, az ~−9.81 |
| gyro magnitude | 1 sqrt | partial — redundant with components |
| accel-gravity dev `\|a\|−9.81` | 1 sqrt+sub | yes — classic signal |
| Z-alignment `az/\|a\|` | 1 div | yes — drops below 1.0 when tilted |
| cross-axis products `gx·gy` etc. | free mults | maybe — correlated motion |
| **PID output magnitude** | free | **yes — saturation = lost authority** |
| wheel-encoder speed | n/a | not present today |

Recommended 5-D vector for the AVR:

```text
f = [ gx_lateral, gz_lateral, |accel|-9.81, az/|accel|, |pid_out| ]
```

`gx_lateral` and `gz_lateral` are the roll and yaw components — pitch is deliberately excluded since it's the balance axis and we don't want to penalise legitimate pitch swings. `|pid_out|` is included because **saturated PID output is the most direct evidence the controller has lost authority** — exactly the user's "motors go to max" observation. If bench testing shows we need pitch gyro too, add it back with a wider learned σ.

## 4. The "balance manifold" representation

Two feasible options for the AVR:

### 4a. Multivariate Gaussian (Mahalanobis distance)

Model balancing as N(μ, Σ) over the 5-D feature vector. Maintain running mean μ ∈ ℝ⁵ and covariance Σ ∈ ℝ⁵ˣ⁵ (symmetric → 15 unique floats). At each tick:

```text
d² = (x − μ)ᵀ Σ⁻¹ (x − μ)
```

Declare HELD when d² > χ²₅(1 − α); for α = 10⁻⁴ the threshold is ≈ 23.5. d² is unitless and rotation-invariant in feature space, catching **any** direction of departure equally well — including the diagonals the current detector misses.

Storage: μ (20 B) + Σ (60 B) + Σ⁻¹ (60 B, cached) = 140 B. A naive 5×5 inversion is ~125 multiplies + a determinant (~50 µs on ATmega328) — too slow every tick. Two fixes: (a) periodic re-inversion (rank-1 update Σ every tick, re-invert every 50 ticks → 250 ms); or (b) Sherman-Morrison rank-1 update of Σ⁻¹ directly (~50 mults/update, numerically delicate, still needs periodic recompute). Reference: Bishop, *PRML*, §2.3.1 and §1.6.

### 4b. Per-axis z-score combination (simpler, less rigorous)

Track per-feature mean μᵢ and variance σᵢ² via Welford's online algorithm (Welford 1962):

```text
n        += 1
δ         = xᵢ − μᵢ
μᵢ       += δ / n
M2ᵢ      += δ · (xᵢ − μᵢ)
σᵢ²       = M2ᵢ / (n − 1)
```

Compute zᵢ = (xᵢ − μᵢ) / σᵢ. Declare HELD when either `max(|zᵢ|) > k_per_axis` (e.g. 4.0) **or** `Σ zᵢ² > k_total²` (e.g. k_total = 6 — sum-of-squares Mahalanobis-lite that catches diffuse departures with no single extreme axis).

Storage: 44 B. CPU: ~5 µs per update, no inversion. **Loss vs. 4a: off-axis correlations ignored** — if accel deviation and PID magnitude are normally tightly coupled, 4b can't notice when they decouple. In practice off-diagonals are usually small enough that 4b catches ~90% of what 4a would.

### 4c. Why NOT one-class SVM / autoencoder / similar

One-class SVM (Schölkopf et al. 2001) and small autoencoders handle non-convex manifolds the Gaussian misses, but need kilobytes of support vectors or trained weights. **Both blow past the ATmega328's 2 KB RAM / 32 KB flash.** Plausible on Teensy/ESP32 targets; revisit in Phase 5+ on bigger MCUs.

### Recommendation

**Ship 4b first.** Welford z-scores plus a sum-of-squares fallback gets ~90% of the Mahalanobis benefit at ~30% of the code and zero matrix-inversion pathology. **Upgrade to 4a once 4b is on the bench and we have data showing which off-diagonal correlations actually matter.** Shipping Mahalanobis up front is the classic "premature rigour" trap — we don't yet know if the correlations are strong enough to justify modelling.

## 5. Bootstrapping the model

Cold-start problem: first boot has no μ or σ. Three pieces:

1. **Bootstrap window**: first 10 s in RUN accumulate σ only; legacy 2-signal detector stays armed as safety net.
2. **Persisted state**: save μ and M2 to EEPROM like mount offsets (`src/config/calibration_storage.cpp` — once Phase 4.1 persistent-storage HAL lands, route through that instead). Reload on next boot for a hot start.
3. **Forget factor**: pure Welford never forgets. Battery sag, tether, wear all drift the noise profile. Use α = 0.99999 per sample → 5-minute half-life at 200 Hz: slow enough that balancing samples dominate, fast enough to adapt to a swapped battery within minutes.

Recommended policy: persist μ, σ, n on shutdown; clamp restored n to a moderate cap (e.g. 10 000) so the forget factor still works; require 10 s of fresh RUN samples post-boot before the NoveltyDetector can trip HELD.

## 6. False-positive policy

Large balance recoveries WILL trip the detector — that's where the boundary is fuzzy. Two policies:

- **Pause-then-resume**: declare HELD, kill motors, resume RUN after a 200 ms quiet window. Brief motor pauses during big disturbances. Cheap, predictable, safe.
- **Adaptive threshold**: k grows when recent balance activity has been high. Better tail behaviour but adds state and a tuning knob.

**Recommend pause-then-resume.** Matches the existing HELD state machine (`docs/findings/balance_held_fallen_state_machine.md`), debuggable on the bench, and a brief motor pause during a 10° recovery is strictly safer than wrong-direction PID slam during a missed pickup.

## 7. Sketch implementation

`src/applications/balancing_robot/novelty_detector.{h,cpp}`:

```cpp
class NoveltyDetector {
public:
    static constexpr uint8_t N = 5;          // feature dimensionality

    void update(const float feat[N]);        // online μ, σ update (Welford + forget)
    float max_zscore() const;                // max |zᵢ|
    float sumsq_zscore() const;              // Σ zᵢ²
    bool  is_anomalous(float k_per_axis, float k_total_sq) const;

    void  freeze(bool yes);                  // pause learning during HELD/FALLEN
    bool  is_bootstrapped() const;           // true after N_bootstrap samples

    uint16_t save_state(uint8_t* buf) const; // for EEPROM persist
    bool     load_state(const uint8_t* buf, uint16_t len);

private:
    float   mean_[N];
    float   m2_[N];
    float   last_z_[N];
    uint32_t n_;
    bool    frozen_ = false;
    static constexpr uint32_t N_BOOTSTRAP = 2000;   // 10 s @ 200 Hz
    static constexpr float    FORGET      = 0.99999f;
};
```

Estimated cost: ~48 B RAM, ~80 LOC, ~10 µs per `update()` on ATmega328 — comfortably inside the 200 Hz budget.

Wiring: `read_imu_()` computes the 5-D feature vector. `step_run_()` calls `update(feat)` then `is_anomalous(4.0f, 36.0f)`; if anomalous for 30 consecutive ticks (150 ms, matches current debounce), transition to HELD. `enter_held_()`/`enter_fallen_()` call `freeze(true)`; `enter_run_()` calls `freeze(false)`.

## 8. Integration with the current architecture

- The current 2-signal detector stays in the code as the **bootstrap-window fallback** and a lenient belt-and-braces backup after `is_bootstrapped() == true`.
- `OnlineMountingEstimator` (Phase 4.4) keeps estimating bias on the raw IMU vector before features are extracted. No conflict: mounting estimator handles slow drift in the mean; NoveltyDetector handles fast departures from the (drifting) mean.
- `dynamic_pwm_accel_learning.md` (system-ID work) is a sibling effort — both consume the IMU stream, neither writes to the same state.

## 9. Open questions

- **Raw vs. normalised features**: raw — Welford auto-normalises via per-channel σ.
- **Debounce length**: start at 30 consecutive anomalous ticks (matches current detector); tune on the bench.
- **Per-feature deadband**: floor σ at a per-feature minimum (e.g. 0.1 deg/s gyro, 0.05 m/s² accel) so a collapsed σ doesn't dominate the z-score.
- **FALL detection reuse**: if d² stays anomalous for >2 s and `az/|accel|` < 0.3, promote HELD → FALLEN. Same detector, longer time constant, geometric guard.
- **Pitch axis inclusion**: revisit if slow tip-over false-negatives show up on the bench.

## 10. Phase number

**Phase 4.7c.** Replaces the Tier-2 HELD/FALLEN state machine in `docs/IMPLEMENTATION_PLAN.md` and `docs/findings/balance_held_fallen_state_machine.md`. Estimated effort: 4–6 hours including bench tuning. Cleanly depends on Phase 4.1 (persistent-storage HAL) for EEPROM persistence; prototype against the legacy EEPROM path in the meantime.

**This is not urgent.** The existing 2-signal detector works well enough that the bot balances today. This is the *next* improvement, not the missing piece — ship it when you've outgrown the false-positive/false-negative tradeoff of the current scalars.

## 11. References

- Bishop, *Pattern Recognition and Machine Learning*, Springer 2006 — §2.3.1, §1.6.
- Welford, *Technometrics* 4(3):419–420, 1962.
- Schölkopf et al., *Neural Computation* 13(7):1443–1471, 2001 — one-class SVM (context only, doesn't fit on AVR).
- In-repo: `docs/MINIMIZE_ACCELERATIONS_PHILOSOPHY.md`, `docs/findings/balance_held_fallen_state_machine.md`, `docs/findings/dynamic_pwm_accel_learning.md`, `docs/findings/online_adaptive_balance_tracking.md`, `src/config/calibration_storage.cpp` (note KI-1 for ESP32).
