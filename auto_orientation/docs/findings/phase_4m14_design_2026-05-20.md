# Phase 4M.14 Design — Analytical Auto-Derivation of the Outer-Loop Gains

**Agent:** ao-phase-4m14-design@floppi:2
**Date:** 2026-05-20
**Status:** DESIGN SPEC — no code. Contract for the follow-up `4M.14-impl` workstream.
**Workstream:** F.3 (the auto-derivation that retires the Phase 4M.13 hardcoded gains)

> **Re-run note.** A prior instance of this agent was lost to an account usage
> limit before it could write to disk. This is the re-run; the file did not
> exist when `ao_roadmap_post_4m14_2026-05-20.md` was written, which is why that
> roadmap had to proceed from `architecture_plan §7` and the RWE build order.
> §3 of that roadmap should now be reconciled against this document.

---

## 0. Reading order and source contract

This spec is the answer to one open procedural gate. `workstream_f_review_2026-05-20.md`
finding **4M.13-13 [P2-SEQ-1]** flags the five outer-loop gains in
`position_loop.h:60-80` as **hardcoded**, with the in-code comment
"`Do NOT bench-tune these in isolation`" (`position_loop.h:47-48`). The
reviewer's Recommended Follow-Up #1 is explicit: *"4M.13 hardcoded gains are
acceptable only if 4M.14 is committed and lands before anyone bench-tunes 4M.13
standalone."* Phase 4M.14 is that commitment. This document designs it so it
**can** land.

Sources synthesised here:

- `workstream_f_review_2026-05-20.md` — §4M.13-13 (the gate), EEPROM slot map.
- `architecture_plan_2026-05-20.md` — §7 (sequencing-discipline checklist, row
  F.2), §2.6 / §4 (4M.13/4M.14 placement).
- `research_wheel_encoders_mega_2026-05-19.md` (RWE) — §6.1 (cascade design, why
  conservative gains), "Recommended build order" item 5 (4M.14 mandate).
- `phase_4m13_landed_2026-05-20.md` — the five hardcoded values as shipped.
- `ao_roadmap_post_4m14_2026-05-20.md` — frames `4M.14-impl` as top-of-queue.
- `src/control/position_loop.{h,cpp}` (READ-ONLY) — the cascade control law.
- `src/control/plant_identifier.h` (READ-ONLY) — the pole-placement mapping the
  inner loop *already* uses; 4M.14 is the same trick one cascade level up.

---

## 1. Problem statement

Phase 4M.13 landed a working two-stage cascade: encoder wheel velocity →
`PositionLoop` → a pitch-setpoint nudge → the existing inner pitch PID → motor
PWM (`phase_4m13_landed_2026-05-20.md` §"The cascade design"). The mechanism is
sound and review-cleared. But its five tuning constants —

```
POSLOOP_K_POS         = 6.0f      (deg nudge per metre of drift)   position_loop.h:60
POSLOOP_K_VEL         = 3.0f      (deg nudge per m/s)              position_loop.h:64
POSLOOP_MAX_NUDGE_DEG = 2.0f      (hard clamp)                     position_loop.h:69
POSLOOP_POS_LEAK      = 0.999f    (per-tick washout, ~5 s tau)     position_loop.h:75
POSLOOP_SLEW_DEG_S    = 2.0f      (max nudge rate)                 position_loop.h:80
```

— are **hardcoded numeric values**. `scope.md §"The rule"` treats a hardcoded
numeric that is not a pin assignment or a structural constant as a scope
violation: the project's recurring failure mode is "editing a constant instead
of building the mechanism that retires it" (architecture_plan §5 risk #2). The
inner loop already obeys this rule — its `Kp`/`Kd` are not bench-tuned, they are
*derived* from a BOOTSTRAP-measured plant gain `K_motor` via closed-form
pole-placement (`plant_identifier.h:26-30`).

**The problem 4M.14 solves:** make `K_POS` and `K_VEL` *derived* the same way —
a closed-form function of quantities already measured during BOOTSTRAP — so that
no operator ever picks a number for the position loop. The deliverable is a
**derivation mechanism**, not a tuned value. After 4M.14 lands, the
`position_loop.h:47-48` "`HARDCODED ... Do NOT bench-tune`" comment is removed,
because the gains are no longer hardcoded.

Non-goal: 4M.14 does **not** bench-tune anything. It is analytical derivation
only. Bench validation (§8) confirms the derived gains hold station; it does not
iterate them.

---

## 2. Plant model — what the outer loop is controlling

### 2.1 The cascade plant as seen by `PositionLoop`

The outer loop's "plant" is everything downstream of its output: the nudge it
emits, fed into the inner PID's setpoint, producing a wheel velocity. We need a
simplified linear model of nudge(deg) → wheel-velocity(m/s) → position(m).

The key simplifying fact, established by the 4M.13 design (RWE §6.1) and the
slew limiter (`position_loop.h:76-80`): **the outer loop is bandwidth-separated
from the inner loop by design.** The slew limit means the setpoint nudge can
only crawl (a couple of deg/s); the inner pitch PID settles in `ts ≈ 0.5 s`
(`plant_identifier.h:91-92`, default settling time). From the outer loop's slow
vantage, the inner loop is effectively a **static gain**: hand it a steady
setpoint nudge `n` degrees, and after the inner loop settles the bot holds a
steady lean of `n` degrees, which produces a steady forward acceleration, which
the wheels integrate into velocity. Concretely the inner-loop transient is
treated as instantaneous relative to the outer bandwidth — the standard cascade
assumption, valid whenever the inner loop is ≥ ~5× faster than the outer.

### 2.2 The lean-to-velocity relationship

> **⚠️ CORRECTION (2026-05-20) — this section's `g_lean` formula is dimensionally
> wrong; do NOT re-derive from it.** The implementation review
> (`phase_4m14_review_2026-05-20.md` §1) found `g_lean = g_eff·π/180·r` below is
> wrong twice over: the `·r` (wheel radius) is spurious — wheel radius converts
> wheel-angular to tread-linear velocity and has no place in a CoM
> lean→acceleration relation — and `g_eff` is the *angular* tipping coefficient
> (deg/s²), not the *linear* lean-to-acceleration gain. The correct outer-plant
> gain is `G_outer = g·(π/180) ≈ 0.171 m/s²/deg` with `g_lean = g = 9.81` (pure
> inverted-pendulum kinematics, `a = g·sinθ ≈ gθ`). Phase 4M.14 shipped with the
> corrected gain — see `balance_app.cpp::derive_position_gains_()`. The
> pole-placement formulas in §3 are correct and unaffected.

A small steady pitch lean `θ` (radians) on an inverted-pendulum bot produces a
horizontal acceleration of the contact patch. To first order, for a bot whose
inner loop is holding it at lean `θ`, the wheels must accelerate to keep the
CoM over the contact patch; the body's gravitational tipping torque is balanced
by wheel reaction. The net effect is a **lean-to-acceleration gain** `g_lean`
(m/s² per radian of commanded lean), so:

  `a_wheel ≈ g_lean · θ`           ... (Eq. 1)

with the position integrator chain `v = ∫a dt`, `x = ∫v dt`.

`g_lean` is **not a free parameter** — it is fixed by the same chassis physics
the inner loop already characterises. The inverted-pendulum restoring term in
`plant_identifier.h:9` is `g_eff · sin(pitch)`, where `g_eff` (deg/s² per unit
sin of pitch) is the gravitational tipping coefficient, held to a class-typical
constant (`plant_identifier.h:86-89`, default 50 deg/s²). For a bot that the
inner loop holds *upright* against that tipping torque, the same coefficient
governs how a commanded lean translates into a steady drive acceleration. So
`g_lean` can be written directly from `g_eff` and the small-angle conversion
(deg ↔ rad, deg/s² ↔ m/s² via an effective pendulum length). The 4M.14
implementation should expose `g_eff` from `PlantIdentifier` (it is already a
member; see `set_g_eff`, `plant_identifier.h:86-89`) and combine it with the
wheel/geometry constants below — it does not need to *learn* `g_lean`.

### 2.3 Quantities available — all already measured by BOOTSTRAP

The derivation must consume only quantities the bot already has by the end of
BOOTSTRAP. Every one of these exists today:

| Quantity | Symbol | Source | Notes |
|---|---|---|---|
| Motor plant gain (gyro) | `K_motor` | BOOTSTRAP pulse response → `PlantIdentifier::seed_k_motor()` (`plant_identifier.h:109-114`) | deg/s² per PWM unit |
| Motor plant gain (encoder) | `K_motor_encoder` | Phase 4M.2 cross-check, `balance_app.cpp:1358-1379` | Δ(wheel vel)/Δ(PWM·t); independent estimate |
| Gravitational tipping coeff | `g_eff` | `PlantIdentifier`, default 50 deg/s² (`plant_identifier.h:86-89`) | class-typical constant, hardcoded a priori per `dynamic_pwm_accel_learning.md §4a` |
| Wheel radius | `r` | EEPROM slot `0x220`, encoder cal (4M.11) | metres; `WheelEncoder::wheel_radius_m()` |
| Inner-loop settling time | `ts` | `PlantIdentifier` target, default 0.5 s (`plant_identifier.h:91-92`) | inner-loop bandwidth proxy |
| Inner-loop natural freq | `ω_n,inner = 4/ts` | derived from `ts` (`plant_identifier.h:26`) | 8 rad/s at default `ts` |
| PID tick period | `dt` | `cfg_.pid_sample_ms`, 5 ms (200 Hz) | for `POS_LEAK` ↔ tau conversion |

The crucial point: **the derivation closes over quantities a fully autonomous
BOOTSTRAP already produces.** There is no new measurement phase. 4M.14 is a
*computation* appended to the existing characterisation, not a new
characterisation.

### 2.4 The linearised cascade transfer function

Combining Eq. 1 with the integrator chain, and treating the inner loop as a
static unit-gain setpoint follower (§2.1), the outer plant from nudge `n`
(degrees) to position `x` (metres) is, in the Laplace domain:

  `x(s) / n(s) ≈ (g_lean · π/180) / s²`           ... (Eq. 2)

i.e. a **double integrator with a known gain** — call that gain
`G_outer = g_lean · π/180` (m/s² per degree). This is the textbook plant for
pole-placement: a double integrator stabilised by a PD controller is exactly
the structure of `PositionLoop`'s control law `nudge = -K_POS·x - K_VEL·v`
(`position_loop.cpp:46`). `K_POS` is the proportional (position) term, `K_VEL`
is the derivative (velocity) term. The cascade was *built* PD-shaped; 4M.14
just supplies the two PD gains analytically instead of by hand.

---

## 3. Derivation method — four candidates, one recommendation

### 3.1 Candidate A — Pole-placement (RECOMMENDED)

The outer plant (Eq. 2) is a double integrator with gain `G_outer`. Closing the
PD loop `n = -K_POS·x - K_VEL·v` gives the closed-loop characteristic
polynomial:

  `s² + G_outer·K_VEL·s + G_outer·K_POS = 0`

Match this to a standard critically-shaped second-order target with outer
natural frequency `ω_o` and damping `ζ_o`:

  `s² + 2·ζ_o·ω_o·s + ω_o² = 0`

Equate coefficients — this is the entire derivation, two lines of algebra:

  **`K_POS = ω_o² / G_outer`**
  **`K_VEL = 2·ζ_o·ω_o / G_outer`**

This is *structurally identical* to the inner loop's mapping
`Kp = ω_n²/K_motor`, `Kd = 2ζ·ω_n/K_motor` (`plant_identifier.h:28-29`) — the
"same trick the inner Kp/Kd already use" that RWE build-order item 5 and
architecture_plan §7 both name. `G_outer` plays the role `K_motor` plays inside.

**Choosing `ω_o` — the bandwidth-separation constraint.** `ω_o` is *not* free.
The cascade is only stable if the outer loop is decisively slower than the
inner loop, otherwise the two loops fight (the failure mode RWE §6.1 designed
the slew limit to prevent). The rule: pick

  **`ω_o = ω_n,inner / N`**, with `N ≥ 5` (recommend **N = 8**, conservative).

With the inner loop's default `ω_n,inner = 4/ts = 8 rad/s`, `N = 8` gives
`ω_o = 1 rad/s` — an outer loop with a ~4 s settling time, exactly the "slow,
gentle station-keeper" the 4M.13 design intends (`position_loop.h:53-55`). The
slew limit (`SLEW_DEG_S`, §5) is the *belt-and-suspenders* enforcement of this
same separation; pole-placement sets it analytically, the slew limit caps it
mechanically. `ζ_o = 1.0` (critically damped) is recommended for the outer loop
— unlike the inner loop's `ζ = 0.7`, the outer loop is a station-keeper and
must **not overshoot** (overshoot means the bot drives *past* its station and
has to come back, which looks like creeping). Critically damped is the slowest
non-oscillatory response and is the right call here.

**Verification that this reproduces the 4M.13 hardcodes.** A sanity check, not
a tuning step: with `ω_o = 1`, `ζ_o = 1`, and a plausible bench-class `G_outer`,
the formulas should land `K_POS` and `K_VEL` in the same order of magnitude as
the hand-picked `6.0` / `3.0`. The 4M.13 values were chosen conservatively by
the RWE author with the *same* slow-station-keeper intent; the derivation
should confirm them as a happy accident, not contradict them. If the derived
values differ by more than ~3×, that is a signal the plant model (§2) needs
re-examination *before* implementation — flag it as open question OQ-1 (§10).

### 3.2 Candidate B — Analytical LQR

Solve a 2-state (position, velocity) LQR for the double integrator, with state
weights `Q` and control weight `R`. The closed-form Riccati solution for a
double integrator exists and yields a PD gain pair. **Rejected:** LQR moves the
tuning problem from "pick two poles" to "pick three weights" (`Q_pos`, `Q_vel`,
`R`) — *more* free parameters, not fewer, and the weights have no physical
units, so they cannot be derived from BOOTSTRAP quantities. It also needs a
square-root or iterative Riccati solve, costly on AVR float-only math. LQR's
advantage (optimality over a cost functional) is irrelevant here — we do not
have a cost functional we believe in; we have a bandwidth-separation
*constraint*, which pole-placement expresses directly. Rejected.

### 3.3 Candidate C — Frequency-domain loop-shaping

Design `K_POS`/`K_VEL` to hit a target open-loop crossover frequency and phase
margin. **Rejected:** loop-shaping is the right tool when the plant has
significant unmodelled dynamics or a non-trivial transfer function. Our outer
plant is a *clean double integrator* (Eq. 2) — for a double integrator,
loop-shaping and pole-placement give the *same answer*, and pole-placement gets
there with two lines of algebra instead of a Bode-plot fitting procedure that
would need `log`/`atan` calls on the AVR. No benefit, more cost. Rejected.

### 3.4 Candidate D — Bandwidth-separation heuristic alone

Skip the plant model; just set `ω_o = ω_n,inner / N` and pick `K_POS`/`K_VEL`
from a rule of thumb. **Rejected as the *primary* method** — it is exactly what
pole-placement (Candidate A) *is*, minus the plant model that turns `ω_o` into
actual gain numbers. Without `G_outer` you cannot get from a frequency to a
gain. Candidate A *uses* the bandwidth-separation idea (that is how it picks
`ω_o`); D on its own is incomplete. Not rejected as wrong — absorbed into A.

### 3.5 Recommendation and justification

**Recommend Candidate A — pole-placement.** Justification against the brief's
three criteria:

- **AVR float-only math.** The entire derivation is four multiplies, one
  divide, and a couple of additions — no `sqrt`, no `log`, no `atan`, no
  iteration. It runs once, at BOOTSTRAP exit, not per-tick. Trivially affordable
  (~10 µs, well under the `PlantIdentifier`'s own ~30 µs/tick budget,
  `plant_identifier.h:46`).
- **Stability margin vs inner loop.** Pole-placement makes the
  bandwidth-separation factor `N` an *explicit, auditable* design parameter. The
  reviewer or operator can read `N = 8` and immediately know the cascade has
  an 8× frequency margin. No other method surfaces this so cleanly.
- **Conservatism.** `ζ_o = 1.0` (critically damped) and `N = 8` are both
  deliberately conservative choices, consistent with the 4M.13 "slow gentle
  station-keeper" intent (`position_loop.h:53-55`) and RWE §6.1's reasoning for
  conservative cascade gains. Pole-placement lets both knobs be *named
  constants with physical meaning* (`OUTER_DAMPING`, `INNER_OUTER_BW_RATIO`) —
  structural constants like CHARACTERISE's "6 pulses", not tuned numerics. They
  do not violate `scope.md §"The rule"` because they are dimensionless design
  ratios, not plant-specific values.

It is also the method the inner loop already uses (`plant_identifier.h:26-30`)
— picking the same method keeps one mental model for the whole controls stack
and lets a maintainer who understands the inner derivation immediately
understand the outer one.

### 3.6 Which K_motor estimate feeds the derivation

BOOTSTRAP produces two: `K_motor` (gyro, → `seed_k_motor`) and
`K_motor_encoder` (4M.2 encoder cross-check). `G_outer` in §2.4 derives from
`g_eff` and geometry, **not** directly from `K_motor` — so neither K estimate
feeds the gain formula *directly*. But the K cross-check is the **gate**: the
4M.2 logic already fails BOOTSTRAP with `failure_reason=7` if the two K
estimates disagree by >30% (`balance_app.h:136`, `balance_app.cpp:1507`). The
derivation should run **only after** that gate passes — a passed cross-check is
the evidence that the encoder chain (and therefore `r` from slot `0x220`, and
therefore the geometry the derivation trusts) is sound. If 4M.2 already failed
BOOTSTRAP, control never reaches the derivation. This makes the K cross-check
the implicit precondition for trusting the derived outer gains.

---

## 4. Where the derivation lives in code (architectural sketch — NO code)

### 4.1 The derivation function

A new function, conceptually `derive_position_gains()`, lives in the control
layer alongside the inner-loop mapping it mirrors. The natural home is
`PlantIdentifier` (`src/control/plant_identifier.{h,cpp}`) — it already owns
`g_eff`, the inner `ω_n`/`ts`/`ζ`, and the inner pole-placement mapping
(`plant_identifier.h:26-30, 86-92`). Adding the outer-loop mapping there keeps
all closed-form gain derivation in one module. It consumes: `g_eff`, `ts` (for
`ω_n,inner`), the wheel radius `r` (passed in, sourced from the encoder cal),
and the two structural constants `INNER_OUTER_BW_RATIO` (= N = 8) and
`OUTER_DAMPING` (= ζ_o = 1.0). It emits a small struct of `{K_POS, K_VEL}`.

Alternative home: a free function or static method in `position_loop.{h,cpp}`
itself. Weaker — `position_loop` deliberately has no `<Arduino.h>` and no
knowledge of `g_eff` or the inner loop. Keeping the *derivation* in
`PlantIdentifier` and the *application* in `PositionLoop` is the cleaner split:
`PlantIdentifier` knows physics, `PositionLoop` runs the loop.

### 4.2 The PositionLoop setter

`PositionLoop` gains a `set_gains(float k_pos, float k_vel)` method. Today the
gains are file-scope `constexpr` (`position_loop.h:60-64`); 4M.14 demotes them
to **private member variables** initialised in the constructor to the current
4M.13 values (which become the *fallback defaults*, see §7), and `set_gains()`
overwrites them. `update()` (`position_loop.cpp:46`) reads the members instead
of the `constexpr`s. This is a small, mechanical change — the control law itself
(`position_loop.cpp:34-65`) is untouched.

The `constexpr POSLOOP_K_POS`/`POSLOOP_K_VEL` are *retained* but renamed to
something like `POSLOOP_K_POS_FALLBACK` / `POSLOOP_K_VEL_FALLBACK` — they are no
longer the operating gains, they are the documented safe fallback for the
encoder-failure path (§7). The `position_loop.h:47-48`
"`HARDCODED ... Do NOT bench-tune`" comment is **removed** and replaced with a
comment explaining the derive/fallback split.

### 4.3 The call site

`derive_position_gains()` runs **once, at the end of BOOTSTRAP, after the 4M.2
K cross-check passes** — i.e. in the BOOTSTRAP finalise path in
`balance_app.cpp`, in the same block where `seed_k_motor()` is called to hand
the measured `K_motor` to the inner loop. The derived `{K_POS, K_VEL}` are then
pushed into `position_loop_` via `set_gains()`. The `phase_4m13_landed` doc
notes `position_loop_` is reset in `enter_state_(RUN)`; the *gains* are set at
BOOTSTRAP finalise, the *integrator* is reset on RUN entry — these are
independent, and `reset()` must **not** clear the gains (`reset()` only zeroes
`position_m_` and `last_nudge_deg_`, `position_loop.cpp:22-25`; that stays).

Sequencing within BOOTSTRAP finalise:
1. 4M.2 K cross-check runs; if it fails → `failure_reason=7`, BOOTSTRAP→IDLE,
   derivation never reached (§3.6).
2. `seed_k_motor(K_motor)` seeds the inner loop (existing behaviour).
3. `derive_position_gains(g_eff, ts, r)` computes `{K_POS, K_VEL}` (NEW).
4. Sanity-clamp the derived gains (§7); if out of bounds → fall back (§7).
5. `position_loop_.set_gains(K_POS, K_VEL)` (NEW).
6. (Optional) cache to EEPROM (§6).

### 4.4 Files that change

| File | Change | Owner workstream |
|---|---|---|
| `src/control/plant_identifier.h` | declare `derive_position_gains()` + result struct; add `INNER_OUTER_BW_RATIO`, `OUTER_DAMPING` constants | 4M.14-impl WS-1 |
| `src/control/plant_identifier.cpp` | implement the four-line pole-placement derivation + sanity clamp | 4M.14-impl WS-1 |
| `src/control/position_loop.h` | demote `K_POS`/`K_VEL` `constexpr` to members; add `set_gains()`; rename fallback constants; **remove the HARDCODED comment** | 4M.14-impl WS-1 |
| `src/control/position_loop.cpp` | `update()` reads members not `constexpr`; constructor seeds members from fallback | 4M.14-impl WS-1 |
| `src/applications/balancing_robot/balance_app.{h,cpp}` | call `derive_position_gains()` + `set_gains()` at BOOTSTRAP finalise; failure-mode fallback wiring | 4M.14-impl WS-2 |
| `src/main.cpp` | (only if EEPROM cache chosen, §6) slot `0x238` read/write helpers | 4M.14-impl WS-3 |
| `tests/test_position_loop.cpp` | (exists, untracked) extend with `set_gains()` coverage | 4M.14-impl WS-1 |
| `tests/test_outer_loop_gain_derivation.cpp` (NEW) | derivation unit test, §8 | 4M.14-impl WS-1 |

No code is written here. The above is the architectural contract for the
`4M.14-impl` agent.

---

## 5. The three secondary constants — keep or derive?

The brief asks each of `MAX_NUDGE_DEG`, `POS_LEAK`, `SLEW_DEG_S` individually.

### 5.1 `MAX_NUDGE_DEG` (2.0°) — **KEEP** (it is a safety saturation)

`MAX_NUDGE_DEG` is the hard clamp on nudge magnitude
(`position_loop.h:66-69`, `position_loop.cpp:48-50`). Its job is to keep the
nudge inside the inner loop's *linear region* — a few degrees wide — so the bot
never leans far enough to lose balance authority. This is a **safety limit**,
not a loop gain. It is in the same category as `TIP_CUTOFF_DEG`
(`balance_constants.h:98`) or the CHARACTERISE pulse count: a structural bound
chosen from the physics of the chassis, not a tuned value.
`scope.md §"The rule"` explicitly exempts safety limits. **Keep it hardcoded.**
It could *optionally* be tied to a fraction of the inner loop's small-angle
linearity ceiling, but that ceiling is itself a hardcoded class constant — the
derivation would be cosmetic. Recommend: keep as a named safety constant, add a
comment that it is a saturation bound, not a gain, so a future reader does not
mistake it for something 4M.14 "missed".

### 5.2 `POS_LEAK` (0.999) — **DERIVE** (from a washout time-constant)

`POS_LEAK` is the per-tick exponential leak on the position integrator
(`position_loop.h:71-75`, `position_loop.cpp:39`). Unlike `MAX_NUDGE_DEG` it
*is* a dynamic parameter — it sets the washout time-constant of the leaky
integrator, which is a real pole in the outer loop's dynamics. It is currently
hardcoded as `0.999` "≈ 5 s tau". That tau is the actual design quantity; the
`0.999` is just `tau` expressed per-tick. The exact relationship:

  `POS_LEAK = exp(-dt / tau)`, equivalently `tau = -dt / ln(POS_LEAK)`

So `POS_LEAK` should be **derived** from a chosen washout time-constant `tau`
and the known tick period `dt` (`pid_sample_ms`, 5 ms). `tau` itself should be
a named structural constant — recommend `POSLOOP_WASHOUT_TAU_S` — chosen on the
same time-scale as the `OnlineMountingEstimator` (~20 s, per
`position_loop.h:24-26`) and **comfortably slower than the outer loop's settling
time `4/ω_o`** so the washout does not eat into the station-keeping bandwidth.
With `ω_o = 1 rad/s` the outer loop settles in ~4 s; a 20 s washout is 5×
slower, clean separation. The derivation `POS_LEAK = exp(-dt/tau)` needs one
`exp` call — affordable because it runs **once** at BOOTSTRAP finalise, not
per-tick. If even one `exp` is unwelcome, the first-order approximation
`POS_LEAK ≈ 1 - dt/tau` is exact to <0.1% for `dt << tau` and is just a divide
and a subtract.

Verdict: **derive `POS_LEAK` from `tau` and `dt`** at BOOTSTRAP finalise, in the
same `derive_position_gains()` call (extend it to emit three values:
`{K_POS, K_VEL, POS_LEAK}`). `tau` becomes the named structural constant; the
`0.999` literal disappears. This also resolves tech-debt item TD-4
(`ao_roadmap §5` — "expand the `POS_LEAK` time-constant derivation comment"):
the comment becomes the actual formula.

### 5.3 `SLEW_DEG_S` (2.0°/s) — **KEEP** (it is an actuator/bandwidth rate limit)

`SLEW_DEG_S` caps how fast the nudge can change (`position_loop.h:76-80`,
`position_loop.cpp:56`). RWE §6.1 and `position_loop.h:29-31` are explicit: the
slew limit is "*the knob that holds the cascade bandwidth below the inner
loop*". With pole-placement (§3) now setting that bandwidth *analytically* via
`ω_o`, one might think the slew limit is redundant. It is **not** — it is the
**belt-and-suspenders mechanical enforcement**. Pole-placement guarantees the
*small-signal* bandwidth separation; the slew limit guarantees it *also* holds
for large transients (a big sudden drift, a recovered collision) where the
linear model of §2 does not apply. The two are complementary: derivation sets
the design-point bandwidth, the slew limit caps the worst case. Removing the
slew limit would mean trusting the linear model in regimes where it is invalid.

`SLEW_DEG_S` is therefore a **rate saturation**, same category as
`MAX_NUDGE_DEG` — a safety/robustness bound, not a tuned gain. **Keep it
hardcoded.** Recommend one improvement: pick its value to be *consistent with*
the derived `ω_o` (a setpoint moving at `SLEW_DEG_S` should be able to traverse
`MAX_NUDGE_DEG` in roughly the outer loop's settling time `4/ω_o`, so the slew
limit does not artificially throttle the derived dynamics). That makes
`SLEW_DEG_S` a documented function of `ω_o` *for the comment's sake*, but it
stays a hardcoded constant — it is a bound, and bounds are allowed to be
constants.

### 5.4 Summary verdict

| Constant | Verdict | Rationale |
|---|---|---|
| `K_POS` | **DERIVE** | loop gain — pole-placement, §3 |
| `K_VEL` | **DERIVE** | loop gain — pole-placement, §3 |
| `MAX_NUDGE_DEG` | **KEEP** | safety saturation (inner-loop linearity bound) |
| `POS_LEAK` | **DERIVE** | dynamic pole — `exp(-dt/tau)` from a named `tau` |
| `SLEW_DEG_S` | **KEEP** | rate saturation / large-signal bandwidth guard |

Three derived (`K_POS`, `K_VEL`, `POS_LEAK`), two kept as named safety bounds.
After 4M.14, the only hardcoded numerics in `position_loop.h` are two explicit
safety saturations plus the fallback gains — and `scope.md §"The rule"` permits
all three categories. The "`HARDCODED ... Do NOT bench-tune`" comment can be
removed because nothing tuning-relevant is hardcoded any more.

---

## 6. EEPROM persistence — cache or always recompute?

**Recommendation: always recompute. Do NOT cache the derived gains to EEPROM.**

The derived gains are a *pure function* of quantities BOOTSTRAP already
produces: `g_eff` (constant), `ts` (constant), and `r` (from EEPROM slot
`0x220`, already persisted). Caching the *output* of a deterministic function
whose *inputs* are already persisted adds a slot, a CRC, a load path, a
staleness window, and a new failure mode (cached gains disagree with current
geometry after the operator re-runs encoder cal) — for the sake of avoiding
**four multiplies and a divide that run once per BOOTSTRAP**. That is a bad
trade. The inner loop makes the same call: `K_motor` is *measured* each
BOOTSTRAP, not cached; the derived `Kp`/`Kd` follow from it live. The outer loop
should be consistent.

The derivation runs at BOOTSTRAP finalise (§4.3). BOOTSTRAP is already an
explicit operator-initiated step; an operator who re-runs encoder cal (`e`
command, slot `0x220`) and then re-runs BOOTSTRAP automatically gets freshly
derived outer gains with zero staleness risk. Caching would *break* that
property unless the cache were invalidated on every `0x220` write — extra
coupling for negative benefit.

**If a future operator decision (`ao_roadmap §7` decision #1) reverses this**
— e.g. it is decided the bot should hold derived gains across a power cycle
*without* re-running BOOTSTRAP — then and only then add a cache. For
completeness, the slot design *if* caching is later required:

- **Slot `0x238`** — the next free address (`workstream_f_review` slot map:
  `0x238–0xFFF` is free; `0x230` is the 4M.12 PWM-discovery slot).
- **16 B layout:** `[magic 0xAE][ver 0x01][K_POS f32 LE][K_VEL f32 LE]`
  `[POS_LEAK f32 LE][reserved 1 B][crc8]`.
- **CRC:** `calculateCRC8(buf, 15)` over bytes 0–14, stored at byte 15 — the
  project-standard CRC-8-CCITT from `calibration_storage.cpp`, exactly as
  slots `0x200`/`0x210`/`0x220`/`0x230` do it (`main.cpp:245`, and the 16-byte
  pattern matches the `0x220` encoder slot at `main.cpp:245,255`).
- **Magic `0xAE`** — distinct from `0xAB` (mount), `0xAC` (actuator), `0xAD`
  (encoder cal *and* PWM-discovery — those two are distinguished by address,
  per the `workstream_f_review` slot map). `0xAE` keeps the new slot
  unambiguous.
- **Staleness guard if cached:** the cache must be invalidated (magic zeroed)
  whenever slot `0x220` is rewritten, since `r` is a derivation input.

But the **recommendation stands: do not cache.** Recompute every BOOTSTRAP. The
slot design above is documented only so a future workstream does not have to
re-derive it.

---

## 7. Failure modes and the fallback path

The derivation trusts its inputs. Each input has a failure mode; each needs a
documented response.

### 7.1 Anomalous `g_eff` / geometry → sanity-clamp the derived gains

`g_eff` is a hardcoded class constant (`plant_identifier.h:86-89`) so it cannot
itself be anomalous — but `r` from slot `0x220` could be (a corrupt-but-CRC-
valid value, a mis-entered encoder cal). A bad `r` propagates into `G_outer`
and could produce a wildly large or tiny `K_POS`/`K_VEL`. **Defence:
sanity-clamp the derived gains to a documented `[min, max]` window** before
applying them — the same σ-modification idea the inner loop uses for `K_motor`
(`set_k_motor_bounds`, default `(0.02, 5.0)`, `plant_identifier.h:98-100`).

Recommended bounds, derived from the plausible bench-class range and the 4M.13
hand-picked values as the centre:

- `K_POS ∈ [1.0, 30.0]` — the 4M.13 value `6.0` sits mid-range; the bounds are
  wide enough to admit any sane bench-class bot but reject a 100× error.
- `K_VEL ∈ [0.5, 15.0]` — likewise centred on the 4M.13 `3.0`.
- `POS_LEAK ∈ [0.990, 0.9999]` — corresponds to `tau ∈ ~0.5 s … 50 s`; outside
  this the integrator is either useless or effectively un-leaked.

These bounds are themselves named structural constants (e.g.
`POSLOOP_K_POS_MIN/MAX`), documented as sanity envelopes, not tuning knobs.

### 7.2 Derived gain out of bounds → fall back to the 4M.13 constants

If the clamp in §7.1 *fires* — i.e. the raw derived value was outside the
window — that is evidence the plant model or an input is wrong, and the derived
gain should **not** be trusted even clamped. In that case **fall back to the
hardcoded 4M.13 values** (`K_POS=6.0`, `K_VEL=3.0`, `POS_LEAK=0.999`), which is
exactly why §4.2 *retains* them as `*_FALLBACK` constants rather than deleting
them. The fallback is a known-safe conservative station-keeper — degraded (not
auto-derived) but functional. The bot still balances; it just holds station on
the generic conservative gains instead of chassis-specific ones.

### 7.3 Encoder partial failure → fall back to the 4M.13 constants

If the encoders are degraded — the 4M.2 K cross-check would normally catch a
gross failure with `failure_reason=7` (§3.6) — but a *partial* failure (one
encoder noisy, slot `0x220` CRC-invalid so `r` is unknown) means the geometry
input to the derivation is untrustworthy. **Response: skip the derivation
entirely and use the `*_FALLBACK` constants.** The decision rule at BOOTSTRAP
finalise (§4.3 step 4): *if slot `0x220` did not load a valid radius, OR the
4M.2 cross-check did not run, OR the clamp fired — use the fallback gains.* The
cascade is `#ifdef USE_WHEEL_ENCODERS` anyway; on a build with no encoders the
question is moot.

### 7.4 New `failure_reason=9` (derived_gains_oob) — recommended but optional

Failure reasons 1–8 are currently used (`balance_app.h:132-136`,
`workstream_f_review §4M.2-4`: 1=pitch_OOR, 2=no_response, 3=k_OOB,
4=user_abort, 5=collision, 6=baseline_noisy, 7=k_disagreement,
8=pwm_discovery_timeout). The §7.2 fallback is **not** a BOOTSTRAP failure — the
bot still balances on fallback gains — so it should **not** abort BOOTSTRAP.
However, the operator should be *told* the derivation was rejected, because it
means the chassis-specific tuning silently did not happen.

Recommendation: **do not** add `failure_reason=9` as a BOOTSTRAP-aborting
reason — that would be wrong, the bot is fine. Instead surface it as a
**non-fatal status flag / telemetry line** ("`posgains: derived OOB, using
fallback`") in the BOOTSTRAP finalise report. If a future design *does* want a
hard enum slot for it (e.g. for the Workstream J black-box recorder), `9` is the
next free value and `derived_gains_oob` is the right label — but reserve it,
do not wire it as an abort. This keeps the failure taxonomy honest: a fallback
is a degraded success, not a failure.

---

## 8. Verification plan

### 8.1 Native unit test — `test_outer_loop_gain_derivation.cpp` (NEW)

Pure assertions, no hardware, no bench. Compiles and runs on the host like the
other `tests/*.cpp`. Coverage:

1. **Closed-form check.** Feed a known `g_eff`, `ts`, `r` into
   `derive_position_gains()`; assert `K_POS`, `K_VEL`, `POS_LEAK` equal the
   hand-computed pole-placement result (the `s²+2ζω s+ω²` coefficient match)
   within a tight float epsilon.
2. **Bandwidth-separation invariant.** Assert the derived `ω_o` (back-computed
   from `K_POS` and `G_outer`) is `≤ ω_n,inner / 5` for a sweep of plausible
   inputs — the cascade-stability guarantee.
3. **Clamp / fallback.** Feed an absurd `r` (0, negative, huge); assert the
   §7.1 clamp fires and §7.2 returns the `*_FALLBACK` values.
4. **`POS_LEAK` round-trip.** Assert `tau = -dt/ln(POS_LEAK)` recovers the
   input `tau` within epsilon; assert `POS_LEAK ∈ (0,1)`.
5. **`PositionLoop::set_gains()` mechanism.** Assert that after `set_gains()`,
   `update()` produces a nudge consistent with the *new* gains, and that
   `reset()` does **not** clear them (§4.3).
6. **Regression: 4M.13 sanity.** Assert the derived gains for a nominal
   bench-class chassis land within ~3× of the 4M.13 hardcodes `6.0`/`3.0` —
   the §3.5 sanity check, encoded as a test so a future plant-model change that
   breaks it is caught (OQ-1).

`test_position_loop.cpp` (already on disk, untracked) stays green — it covers
the cascade *mechanism*; the new test covers the *derivation*. Both must pass.

### 8.2 Bench protocol (operator, hardware)

Runs only after a confirmed bench session (gated per `ao_roadmap §7` decision
#4). Procedure:

1. Run encoder cal (`e`) and BOOTSTRAP on the actual chassis so real
   `r`/`K_motor` flow into the derivation.
2. Read back the derived `K_POS`/`K_VEL`/`POS_LEAK` over serial (a one-line
   BOOTSTRAP-finalise telemetry print — confirm they are in-range, not
   fallback).
3. Place the bot in RUN on a level surface. **Acceptance: the bot holds station
   within ±0.2 m of its start point over a 60 s window, with no visible
   oscillation or hunting** — a smooth lean, not a twitch. This is the same
   station-keeping criterion the 4M.13 design targets
   (`phase_4m13_landed §"The cascade design"`).
4. This is **verification, not tuning.** If the bot fails the ±0.2 m / smooth
   criterion, that is a finding against the *plant model* (§2) or the
   bandwidth-separation factor `N` — it is escalated as a design revision, **not**
   resolved by editing a gain. `scope.md §"The rule"` and `architecture_plan §7`
   forbid bench-iterating the constant; the failure feeds back into §3/§10.

### 8.3 Mega flash / RAM estimate

The change is small. The derivation is four multiplies, one divide, one `exp`
(or its first-order approximation) — call it ~150–250 B flash for
`derive_position_gains()` + the clamp + the fallback decision logic.
`PositionLoop` gains one method (`set_gains()`, ~30 B) and three `float`
members replacing three file-scope `constexpr`s — **+12 B RAM** (the
`constexpr`s consumed no RAM; three live floats do). Total estimate:
**~250–350 B flash, ~12 B RAM.** Negligible against the 4M.13 build state
(`mega_balance` 38192 B flash / 15.0%, 1484 B RAM / 18.1% —
`phase_4m13_landed` build table). No EEPROM slot consumed (§6 recommends no
cache). The cascade is `#ifdef USE_WHEEL_ENCODERS`; `uno_balance` stays
**byte-identical**, zero regression — same property every prior 4M phase held.

---

## 9. Implementation workstreams

`4M.14-impl` splits into three workstreams with clean file boundaries. WS-1 and
WS-3 are parallelizable; WS-2 depends on WS-1 (it calls WS-1's new function).

### WS-1 — Derivation math + PositionLoop setter

- **OWNS:** `src/control/plant_identifier.{h,cpp}`,
  `src/control/position_loop.{h,cpp}`,
  `tests/test_outer_loop_gain_derivation.cpp` (NEW),
  `tests/test_position_loop.cpp` (extend).
- **Deliverable:** `derive_position_gains()` (the §3 pole-placement +
  §5.2 `POS_LEAK` derivation + §7.1 clamp), `PositionLoop::set_gains()`,
  the `constexpr`→member demotion, the `*_FALLBACK` rename, removal of the
  `position_loop.h:47-48` HARDCODED comment. Carries TD-4 (`POS_LEAK` comment)
  and TD-8 (`dt<=0` / extreme-`wheel_vel` regression test).
- **Complexity:** M. Self-contained in the `control/` layer.

### WS-2 — BOOTSTRAP integration

- **OWNS:** `src/applications/balancing_robot/balance_app.{h,cpp}`.
- **Deliverable:** the §4.3 call site — invoke `derive_position_gains()` at
  BOOTSTRAP finalise after the 4M.2 cross-check, apply via `set_gains()`, wire
  the §7.3 encoder-failure fallback decision, add the §7.4 non-fatal
  "`derived OOB, using fallback`" telemetry line. Carries the ride-along
  tech-debt the `ao_roadmap §5` table assigns to `4M.14-impl`: TD-1 (`raw_gyro_dps_`
  ATOMIC_BLOCK comment), TD-2 (`failure_reason=7` comment clarification), TD-3
  (K-disagreement rationale comment).
- **DEPENDS ON:** WS-1 (calls its new function). **Serialize after WS-1.**
- **Complexity:** S–M.

### WS-3 — EEPROM cache (OPTIONAL — only if §6 is overridden)

- **OWNS:** `src/main.cpp` (slot `0x238` helpers).
- **Deliverable:** *Only if* the operator reverses the §6 "do not cache"
  recommendation. Slot `0x238`, magic `0xAE`, 16-B layout, `calculateCRC8`,
  `0x220`-write invalidation.
- **Disposition:** **Not dispatched by default.** §6 recommends recompute. This
  workstream exists as a documented contingency, not a planned task. If it is
  ever dispatched it is fully parallel with WS-1 (disjoint files).
- **Complexity:** S.

Default dispatch: **WS-1 then WS-2, serial. WS-3 omitted.** This matches the
`ao_roadmap §8` "Next session — land the gate" plan, which already names a
single `ao-phase-4m14-impl` agent owning `position_loop`, `plant_identifier`,
`balance_app`, and the new test — i.e. WS-1 + WS-2 under one agent is also
acceptable if parallelism is not needed.

---

## 10. Risks and open questions

- **OQ-1 — does the derivation reproduce the 4M.13 hardcodes?** The §3.5 /
  §8.1-6 sanity check: derived `K_POS`/`K_VEL` for a nominal chassis should land
  within ~3× of `6.0`/`3.0`. If they do not, the plant model (§2 — specifically
  the `g_lean` ↔ `g_eff` relationship in §2.2) is wrong and must be revised
  *before* WS-2. This is the single highest-leverage check. **Mitigation:**
  encode it as a test (§8.1-6) so it cannot be skipped.

- **R-1 — inner-loop bandwidth is uncharacterised (HIGH).** The whole §2.1
  cascade simplification rests on the inner loop being ≥5× faster than the
  outer. The inner loop's `ts` is a *target* (`plant_identifier.h:91-92`,
  default 0.5 s), **not a measured** settled bandwidth — the real closed-loop
  bandwidth depends on how well the inner PID actually tracks. If the inner loop
  is slower than its `ts` target, the bandwidth separation `N` shrinks and the
  cascade can hunt. `ao_roadmap §10` risk #1 names this exact concern.
  **Mitigation:** the conservative `N = 8` (§3.1) buys margin — even if the
  inner loop is 2× slower than target, separation is still 4×. The §8.2 bench
  protocol's "no visible oscillation" criterion is the empirical check. A
  follow-up workstream could *measure* the inner-loop step response to replace
  the `ts` target with a measured value — flagged, not scoped here.

- **R-2 — `g_eff` is a class constant, not measured (MEDIUM).** `g_eff` is
  hardcoded at 50 deg/s² (`plant_identifier.h:86-89`) per
  `dynamic_pwm_accel_learning.md §4a` ("known a priori from chassis geometry").
  The outer derivation inherits that assumption. A chassis far from the bench
  class (very tall, very heavy) would have a wrong `g_eff` and therefore wrong
  derived outer gains. **Mitigation:** the §7.1 sanity clamp + §7.2 fallback
  catch the gross case; the §8.2 bench protocol catches the subtle case. A
  chassis that needs a different `g_eff` is out of the current design class.

- **R-3 — slip / bind marginal-pass on the 4M.2 cross-check (MEDIUM).** The
  derivation runs only after 4M.2 passes (§3.6), but the 4M.2 gate is
  deliberately *loose* — 30% K-disagreement tolerance
  (`workstream_f_review §4M.2-1`, "30% is deliberately loose"). A chassis with
  ~25% slip *passes* the gate but feeds a slightly-wrong `r`/geometry into the
  derivation. **Mitigation:** within the 30% band the derived gains are off by
  a comparable fraction — well inside the §7.1 clamp window and the §8.2
  station-keeping tolerance. The conservative `ζ_o = 1` and `N = 8` absorb it.

- **R-4 — `exp()` for `POS_LEAK` on AVR (LOW).** One `exp` call at BOOTSTRAP
  finalise. **Mitigation:** §5.2's first-order approximation
  `POS_LEAK ≈ 1 - dt/tau` is exact to <0.1% for `dt << tau` and uses no
  transcendental. WS-1 picks whichever; either is fine.

- **R-5 — derivation proves intractable across the K range (LOW–MED).**
  `ao_roadmap §10` risk #1: if no derived `K_POS` is stable across the plausible
  chassis range, the gate stays open. **Mitigation:** this design's plant
  (§2.4) is a clean double integrator — pole-placement on a double integrator
  is *always* solvable; the only failure is a wrong `G_outer`, which is OQ-1,
  caught by §8.1-6. A permanent hardcode is **not** an acceptable fallback
  (`scope.md §"The rule"`); the §7.2 fallback is a *degraded-mode* safety net,
  not the operating design.

---

## 11. Sequencing — the discipline checkpoint

Cross-referenced to `architecture_plan_2026-05-20.md §7` (the F.2 row of the
sequencing-discipline checklist) and `workstream_f_review_2026-05-20.md`
Recommended Follow-Up #1:

1. **4M.14 lands the derivation mechanism.** `derive_position_gains()` +
   `set_gains()` + the BOOTSTRAP call site. `K_POS`/`K_VEL`/`POS_LEAK` become
   *derived*; `MAX_NUDGE_DEG`/`SLEW_DEG_S` stay as named safety bounds (§5).

2. **Only after 4M.14 lands** is the `position_loop.h:47-48`
   "`HARDCODED ... Do NOT bench-tune`" comment removed — because at that point
   nothing tuning-relevant is hardcoded. WS-1 owns that removal (§4.2).

3. **No bench-tuning of the position loop until 4M.14 lands.** The
   `workstream_f_review §4M.13-13` rule holds in full until then: 4M.13's
   gains are a *mechanism placeholder*, not a value to iterate. The §8.2 bench
   protocol is **verification** (does the *derived* gain hold station?), never
   **tuning** (it does not adjust gains; a failure escalates to a design
   revision per §8.2 step 4 and §10 OQ-1).

4. **The sequencing-discipline invariant holds.** Every 4M.14 deliverable is a
   *mechanism* (a derivation, a setter, a fallback), never a *value*. The only
   numerics 4M.14 introduces — `INNER_OUTER_BW_RATIO=8`, `OUTER_DAMPING=1.0`,
   `POSLOOP_WASHOUT_TAU_S`, the §7.1 sanity bounds — are dimensionless design
   ratios and structural safety envelopes, the same category as CHARACTERISE's
   "6 pulses". They are not chassis-specific tuned numerics and do not violate
   `scope.md §"The rule"`. With 4M.14 landed, the F.2 checkbox in the
   architecture_plan §7 checklist flips from "**Flag — K_POS/K_VEL hardcoded
   until 4M.14**" to "No — derived from BOOTSTRAP `g_eff`/`ts`/`r`."

*Design complete. This spec is the contract for the `4M.14-impl` workstream
(`ao_roadmap §8`, "Next session — land the gate"). No code, no bench-tuning —
analytical derivation only.*
