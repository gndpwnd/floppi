# Level 2 — Component detail

The two highest-value internals of the Mega adaptive stack. We stop here on purpose: deeper than this (per-tick RLS math, ISR/loop split, byte-level EEPROM layouts) lives in the per-component docs and the source comments, which are already thorough.

[← Level 1](LEVEL_1_SUBSYSTEMS.md) · [Index](INDEX.md)

- [(1) BOOTSTRAP — K-measurement + gain derivation](#1-bootstrap--k-measurement--gain-derivation)
- [(2) Position outer-loop cascade](#2-position-outer-loop-cascade)

---

## (1) BOOTSTRAP — K-measurement + gain derivation

Source: `src/applications/balancing_robot/balance_app.cpp` (`step_bootstrap_`, phase layout at lines 1144-1172) and `src/control/plant_identifier.h` (pole-placement mapping).

BOOTSTRAP replaces hardcoded PID gains. It fires a fixed sequence of symmetric ±PWM pulses, measures the chassis pitch-acceleration response per pulse to estimate the plant gain `K_motor`, then derives Kp/Kd/Ki in closed form before entering RUN.

### Pulse sequence + quality gates

Timeline (relative to state entry): baseline 300 ms → 4 pulses of 150 ms each separated by 400 ms cooldowns → finalise at ≥2500 ms. Pulse magnitudes are `{180, 180, 240, 240}` PWM/wheel with alternating sign.

```mermaid
flowchart TD
    ENTER["enter_bootstrap()<br/>guard: |pitch − offset| < 10°"] --> P0["PHASE 0 — baseline (0..299 ms)<br/>motors off, capture peak-to-peak gyro_y noise"]
    P0 --> NCHK{"baseline range<br/>> 5 dps?"}
    NCHK -->|yes| FAIL6["fail: baseline_noisy (6)"]
    NCHK -->|no| PULSE["PULSE i (150 ms)<br/>±PWM = {+180,−180,+240,+240}<br/>per wheel, alternating sign"]
    PULSE --> G0CHK{"|gyro_y at start|<br/> > 5 dps?"}
    G0CHK -->|yes| SKIP["skip pulse<br/>(residual momentum)"]
    G0CHK -->|no| COOL["COOLDOWN i (400 ms)<br/>motors off, settle"]
    COOL --> DW["measure |Δω| over pulse<br/>K_i = (|Δω|/τ) / pwm_total"]
    DW --> RESP{"|Δω| above<br/>noise threshold (3× baseline)?"}
    RESP -->|no| NORESP["pulse contributes nothing"]
    RESP -->|yes| ACC["accumulate K_i (and K_enc_i)"]
    SKIP --> NEXT
    NORESP --> NEXT
    ACC --> NEXT{"more pulses?<br/>(4 total)"}
    NEXT -->|yes| PULSE
    NEXT -->|no| FIN["FINALISE (≥2500 ms)"]
    FIN --> VALID{"pulses_valid ≥ 1<br/>AND K in bounds (0.02..5)?"}
    VALID -->|no| FAILX["fail: no_response (2) /<br/>k_out_of_bounds (3) /<br/>k_disagreement (7)"]
    VALID -->|yes| SEED["PlantIdentifier.seed_k_motor(K)<br/>+ derive Kp/Kd/Ki (pole placement)"]
    SEED --> RUN["→ RUN (adaptive_active)"]
```

> Why the gates exist: bench runs (2026-05-18) showed a single poisoned mean from residual momentum (`G0_MAX`) and from an operator handling the bot during baseline (`NOISE_FLOOR_MAX`). Both default to 5 dps. On Mega with encoders, a gyro-vs-encoder `K` disagreement > 30% trips `k_disagreement (7)` — evidence of wheel slip or binding. See balance_app.h:160-277.

### Pole-placement gain derivation

The plant collapses to one DoF: `α_pitch ≈ K_motor · pwm_total + g_eff · sin(pitch)`. Matching the closed-loop polynomial to a target second-order response (ω_n = 4/ts, ζ = 0.7) gives the gains directly from the measured `K_motor` (plant_identifier.h:18-31).

```mermaid
flowchart LR
    K["measured K_motor"] --> WN["ω_n = 4 / ts<br/>(ts = 0.5 s default)"]
    ZETA["ζ = 0.7"] --> KD
    WN --> KP["Kp = ω_n² / K_motor"]
    WN --> KD["Kd = 2·ζ·ω_n / K_motor"]
    KP --> KI["Ki = small fraction of Kp<br/>(keeps mounting estimator alive)"]
    KP --> RAMP["BalanceApp rate-limits gains<br/>(5%/s) before set_tunings()"]
    KD --> RAMP
    KI --> RAMP
    RAMP --> PIDC["PIDController"]
    K -.also seeds.-> RLS["PlantIdentifier RLS<br/>(keeps adapting in RUN)"]
```

> The same `Kp = ω_n²/K` mapping is reused on the **outer** loop for the 4M.14 position-gain derivation (`K_POS = ω_o²/G_outer`) — see component (2). The identifier never slams the controller: it emits *targets*, the app ramps toward them.

---

## (2) Position outer-loop cascade

Source: `src/control/position_loop.{h,cpp}` and `balance_app.cpp` (`step_run_`, `derive_position_gains_`).

The cascade keeps the bot **station-keeping** instead of creeping. A slow outer loop reads wheel velocity and nudges the inner PID's pitch setpoint so the bot leans gently against its own drift.

### Structure + bandwidth separation

```mermaid
flowchart LR
    ENC["mean wheel velocity v (m/s)"] --> INT["leaky integral<br/>position_m += v·dt;<br/>position_m *= POS_LEAK"]
    INT --> LAW["nudge = −K_POS·position_m − K_VEL·v"]
    LAW --> CLAMP["magnitude clamp ±2°<br/>(MAX_NUDGE_DEG)"]
    CLAMP --> SLEW["slew limit ±2°/s<br/>(SLEW_DEG_S)"]
    SLEW --> SP["pitch setpoint nudge (deg)"]
    SP --> INNER["inner pitch PID"]
    INNER --> PWM["PWM → motors"]
    PWM -.bot moves.-> ENC
```

Key safety property: the **clamp and slew are hardcoded saturations, not loop gains** (a 2° lean ceiling keeps the outer loop inside the inner loop's linear region; the slew rate enforces the bandwidth separation analytically set by pole-placement). The *gains* `K_POS`/`K_VEL`/`POS_LEAK` are derived, not tuned.

### Where the outer-loop gains come from (4M.14)

At BOOTSTRAP finalise — after the gyro-vs-encoder K cross-check passes — `derive_position_gains_` computes the outer-loop gains in closed form, mirroring the inner-loop pole-placement.

```mermaid
flowchart TD
    XCHK["4M.2 K cross-check passes<br/>(encoder chain trusted)"] --> WO["ω_o = ω_n,inner / N<br/>(N = 8 bandwidth separation)"]
    WO --> KPOS["K_POS = ω_o² / G_outer"]
    WO --> KVEL["K_VEL = 2·ζ_o·ω_o / G_outer<br/>(ζ_o = 1.0, critically damped)"]
    TAU["washout τ = 20 s"] --> LEAK["POS_LEAK = exp(−dt/τ)"]
    KPOS --> CL{"inside sanity envelope?<br/>(POSLOOP_*_MIN/MAX)"}
    KVEL --> CL
    LEAK --> CL
    CL -->|yes| APPLY["position_loop.set_gains() / set_pos_leak()<br/>(posgains_failure_reason = 0)"]
    CL -->|no| FB["revert to *_FALLBACK gains<br/>(6.0 / 3.0 / 0.999)<br/>(posgains_failure_reason = 9, non-fatal)"]
```

> A rejected derivation is a *degraded success*: the bot still balances on the conservative Phase 4M.13 fallback gains (`POSLOOP_K_POS_FALLBACK` etc.). It is deliberately NOT a BOOTSTRAP abort. See position_loop.h:46-104 and balance_app.h:340-409.

---

That's the floor. For the per-tick RLS update equations, the ISR/loop split, and EEPROM byte layouts, read the source headers (`plant_identifier.h`, `balance_app.h`) — they are the authoritative, comment-heavy reference.
