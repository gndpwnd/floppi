# Phase 4M.13 Landed — Velocity/Position Outer Loop (Cascade)

**Agent:** ao-phase-4m13@floppi:1
**Date:** 2026-05-20
**Workstream:** F.2 (second half of Workstream F, architecture_plan_2026-05-20.md §2.6 / §4)
**Status:** landed, builds green

---

## The cascade design

The balance stack is now a two-stage cascade:

```
encoder wheel velocity ─► PositionLoop ─► pitch-setpoint nudge (deg)
                                         ─► inner balance PID ─► motor PWM
```

- **Inner loop** — the existing pitch PID in `step_run_()`. Unchanged. It
  keeps the bot upright; it just tracks a slowly-moving setpoint now instead
  of a fixed `0.0`.
- **Outer loop** — `PositionLoop`. Watches the mean wheel velocity, integrates
  it (leakily) into a drift estimate, and emits a small clamped, slew-limited
  pitch-setpoint nudge so the bot leans *against* its own drift and holds
  station instead of creeping across the floor.

The outer loop's bandwidth is held well below the inner loop's by the slew
limit (`SLEW_DEG_S = 2°/s`): a setpoint that can only crawl cannot fight the
pitch PID. Design source: `research_wheel_encoders_mega_2026-05-19.md` §6.1.

## position_loop API (`src/control/position_loop.{h,cpp}` — NEW)

```cpp
class PositionLoop {
  PositionLoop();
  void  reset();                              // clear integrator + slew memory
  float update(float wheel_vel, float dt);    // m/s, s -> nudge in degrees
  float position_m() const;                   // inspection / telemetry
  float last_nudge_deg() const;
};
```

`update()` does, in order:
1. **Leaky position integrator** — `position_m_ += v·dt; position_m_ *= POS_LEAK`.
   The leak keeps the integrator bounded so a small encoder bias can never
   wind up a permanent setpoint offset.
2. **Control law** — `nudge = -K_POS·position - K_VEL·v`. The velocity term
   damps the position term.
3. **Magnitude clamp** to `±MAX_NUDGE_DEG` — keeps the nudge inside the inner
   loop's linear region.
4. **Slew limit** to `±SLEW_DEG_S·dt` — bounds the cascade bandwidth.

`dt <= 0` returns the cached nudge unchanged (clock-not-advanced guard).

No `<Arduino.h>`, no dynamic allocation, no STL — same constraints as the rest
of `control/`. The class compiles everywhere; `balance_app` only *calls* it
under `USE_WHEEL_ENCODERS`.

## Hardcoded gains — Phase 4M.13 sequencing flag (architecture_plan §7)

The five outer-loop gains are **intentionally hardcoded** for this phase, with
a clear in-code comment at the definitions in `position_loop.h`:

```cpp
// HARDCODED for Phase 4M.13 — auto-derivation is Phase 4M.14. Do NOT bench-tune
// these in isolation; see architecture_plan_2026-05-20.md §7.
constexpr float POSLOOP_K_POS         = 6.0f;    // deg nudge per m of drift
constexpr float POSLOOP_K_VEL         = 3.0f;    // deg nudge per m/s
constexpr float POSLOOP_MAX_NUDGE_DEG = 2.0f;    // hard clamp
constexpr float POSLOOP_POS_LEAK      = 0.999f;  // per-tick washout (~5 s τ)
constexpr float POSLOOP_SLEW_DEG_S    = 2.0f;    // max nudge rate
```

These are conservative defaults from RWE §6.1 — a slow, gentle station-keeper,
not a fast position servo. **They have NOT been bench-tuned or "optimized"** in
this session, per the §7 sequencing-discipline rule (the recurring failure mode
is editing a constant instead of building the mechanism that retires it).

**Phase 4M.14 is the planned follow-up** and remains on the roadmap: it will
derive `K_POS`/`K_VEL` analytically from the encoder-verified `K_motor`
(pole-placement, the same trick the inner Kp/Kd already use), retiring these
constants. Until 4M.14 lands, the gains above are a working **mechanism**, not
a tuned value — they must not be bench-tuned standalone.

## Integration point in step_run_

In `balance_app.cpp::step_run_()`, the former unconditional `pid_.set_setpoint(0.0f)`
(directly before the inner PID compute) is now gated:

- **`USE_WHEEL_ENCODERS` defined (mega_balance):** reads
  `0.5·(enc_left_.read_velocity_mps + enc_right_.read_velocity_mps)`, calls
  `position_loop_.update(v_mps, dt_sec)`, and feeds the returned nudge to
  `pid_.set_setpoint()`. The same `now_ms` is reused by the existing stall-
  detection encoder reads further down so the velocity windows stay coherent.
- **Undefined (uno_balance / native_test):** `pid_.set_setpoint(0.0f)` exactly
  as before — behaviour is byte-identical.

`position_loop_` is reset in `enter_state_(RUN)` (gated) so each balance
session holds station from wherever the bot currently sits — no drift carried
over from a prior RUN or HELD episode. This mirrors the `enc_*.reset_ticks()`
discipline established by Phase 4M.2.

A `PositionLoop position_loop_` member was added to `balance_app.h` inside the
existing `USE_WHEEL_ENCODERS` member block; `position_loop.h` is included
alongside `wheel_encoder.h` under the same gate.

## Build verification

| Env          | Result  | Flash            | RAM             |
|--------------|---------|------------------|-----------------|
| mega_balance | SUCCESS | 38192 B (15.0%)  | 1484 B (18.1%)  |
| uno_balance  | SUCCESS | 30222 B (93.7%)  | 1273 B (62.2%)  |

Deltas vs. the Phase 4M.2 baseline (`phase_4m2_landed_2026-05-20.md`):

- **mega_balance:** +594 B flash, +8 B RAM (the two `PositionLoop` floats).
  Well inside the Mega's 256 KB / 8 KB budget.
- **uno_balance:** **byte-identical** to the 4M.2 baseline — zero regression.
  The entire cascade is `#ifdef USE_WHEEL_ENCODERS`'d out on the Uno path.

## Constraints honoured

- Only the 5 WRITE_ZONE files touched. No edits to `wheel_encoder.*`,
  `plant_identifier.*`, `pid_controller.*`, `main.cpp`, `platformio.ini`, or
  any tests.
- Collision detection, the 3-gate detector, PWM_DISCOVERY, the `F()` shim,
  `held_entry_reason_` and the 4M.2 K cross-check were not modified.
- No git commit made.
