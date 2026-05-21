# Phase 4M.14 Review — Analytical Auto-Derivation of the Outer-Loop Gains

**Agent:** ao-phase-4m14-review@floppi:1
**Date:** 2026-05-20
**Scope:** READ-ONLY review of Phase 4M.14 (auto-derivation of `PositionLoop`
`K_POS` / `K_VEL` / `POS_LEAK`). The implementation deviated from its design
spec; the central question is whether that deviation is physically sound or a
reverse-fit.
**Reviewed:** `phase_4m14_design_2026-05-20.md`, `phase_4m14_landed_2026-05-20.md`,
`research_wheel_encoders_mega_2026-05-19.md` §6, `phase_4m13_landed_2026-05-20.md`,
`src/control/position_loop.{h,cpp}`, `src/applications/balancing_robot/balance_app.{h,cpp}`,
`src/control/plant_identifier.h`, `tests/test_position_loop.cpp`.

---

## 0. Central-question verdict — UP FRONT

**Verdict: (a) the implementation is physically correct; the design spec §2.2
has a unit/dimensional bug.** The implementation's `g_lean = g = 9.81 m/s²/rad`
is the correct outer-loop plant gain. The spec's `g_lean = g_eff · π/180 · r`
is dimensionally wrong twice over (a spurious `·r` and a misuse of `g_eff`).

**BUT** — this is graded (a)-with-a-serious-caveat, escalated to **P1**: the
implementation arrived at the right number for a *partially wrong reason*, and
its own landing report frames the choice as "it reproduces the 4M.13 hardcodes,"
which is reverse-fit language. The physics genuinely supports `g_lean = g`, so
4M.14 is **not** a reverse-fit in substance — but the *inline justification is
not airtight*, and one secondary value (`K_VEL`) silently fails the spec's OQ-1
sanity band. Both need a documentation/derivation fix before the bench-tuning
gate can honestly be called "lifted." See §1 and §11.

---

## 1. THE CENTRAL QUESTION — dimensional analysis from first principles

### 1.1 What the cascade actually is

Outer loop emits a pitch-setpoint **nudge `n` in degrees**. The inner pitch PID,
treated (correctly, §2.1 of the spec) as a fast static unit-gain follower,
makes the body hold a steady lean `θ`. That lean produces a horizontal
acceleration of the bot. The encoders integrate acceleration → velocity →
position. So the outer "plant" is:

```
n (deg) → θ (rad) → a (m/s²) → v (m/s) → x (m)
          [deg→rad]  [g_lean]   [∫dt]     [∫dt]
```

`G_outer` is the gain from `n` (deg) to `a` (m/s²). The integrator chain
`1/s²` is structurally fixed; only `G_outer`'s magnitude is in question.

### 1.2 The lean-to-acceleration gain — first principles

For an inverted-pendulum / segway-type bot held at a small steady lean `θ`
(radians), the centre of mass is offset horizontally from the contact patch.
The standard small-angle result: a body whose CoM is held at lean `θ` requires
the contact patch to accelerate so the CoM "falls forward" at

  `a = g · sin θ ≈ g · θ`     (m/s² per radian)

This is the *same physics* as a segway leaning to drive forward, and it is the
textbook double-integrator-with-gravity-gain plant. The gain is **`g` itself**
(9.81 m/s²/rad). It does **not** depend on wheel radius, motor constant, or the
chassis tipping coefficient — those affect *how the inner loop achieves* the
lean, not the lean→accel conversion, which is pure kinematics of a leaning mass
in a gravity field.

Converting the input from radians to degrees:

  **`G_outer = g · (π/180) ≈ 9.81 · 0.017453 ≈ 0.1713  m/s² per degree`**

This is exactly `balance_app.cpp:1613` (`g_outer = g_lean * deg2rad`,
`g_lean = 9.81`). **The implementation's G_outer is dimensionally correct.**

### 1.3 Why the spec's §2.2 formula is wrong — two independent errors

The spec derives `g_lean = g_eff · π/180 · r`. Working the units:

**Error 1 — the spurious `· r`.** Multiplying by wheel radius `r` (metres)
appears to come from confusing *angular* wheel quantities with *linear* CoM
quantities. Wheel radius converts wheel **angular** velocity (rad/s) to **tread
linear** velocity (m/s): `v_tread = ω_wheel · r`. But the outer-loop plant is
the CoM lean → CoM acceleration relationship — there is no wheel-angular term
in it at all. `r` has no place in `G_outer`. Including it multiplies the gain
by ~0.0325 and is simply a category error. (The spec even half-knows this:
§3.6 says "neither K estimate feeds the gain formula directly" — `r` shouldn't
either.)

**Error 2 — misusing `g_eff`.** `g_eff` is defined in `plant_identifier.h:9`
as the coefficient in `α_pitch ≈ K_motor·pwm + g_eff·sin(pitch)`. Here
`α_pitch` is in **deg/s²** — it is the body's *angular* tipping acceleration
about the wheel axle. `g_eff` (≈ 50 deg/s²) is a *rotational* coefficient
`≈ m·g·d/I` (gravity torque ÷ moment of inertia). It is **not** a linear
acceleration and **not** numerically equal to `g`. The lean→CoM-acceleration
gain the outer loop needs is the *linear* `g`, a different physical quantity in
different units. The spec conflates the body's angular fall-rate with the CoM's
linear fall-rate. They share the word "gravitational" and nothing else.

**Net spec error:** `g_eff·π/180·r ≈ 50 · 0.01745 · 0.0325 ≈ 0.0284` vs the
correct `g·π/180 ≈ 0.1713` — and worse, the spec's value is in nonsense units
(`deg/s²·rad·m`). With the spec's `G_outer ≈ 0.0284`, pole-placement gives
`K_POS = ω_o²/G_outer = 1/0.0284 ≈ 35`… the landing report says the literal
reading yields ≈ 2000; either way it is grossly wrong and the discrepancy is
the §2.2 unit bug, exactly as OQ-1 predicted ("if they do not [match within
3×], the §2.2 `g_lean ↔ g_eff` relationship is wrong"). **OQ-1 fired. The spec
lost.**

### 1.4 Is `g = 9.81` physically correct or a number picked to hit 6.0?

**It is physically correct.** Independent confirmation, not circular:

- `g·θ` is the first-order CoM acceleration for *any* small-angle inverted
  pendulum — derivable without reference to the 4M.13 value.
- The pole-placement chain then gives, with `ω_o = (4/0.5)/8 = 1 rad/s`,
  `ζ_o = 1`:
  - `G_outer = 9.81 · π/180 = 0.17127`
  - `K_POS = ω_o²/G_outer = 1 / 0.17127 = 5.84`  ✓ (landing report: 5.84)
  - `K_VEL = 2·ζ_o·ω_o/G_outer = 2 / 0.17127 = 11.68`  ✓
- `K_POS ≈ 5.84` lands at **1.03×** the 4M.13 hand-pick of 6.0 — and this is a
  *consequence* of correct physics, not an input to it. The RWE author picked
  6.0 "conservatively" with the same slow-station-keeper intent; the physics
  confirms the hand-pick was good. That is the *happy accident* §3.5 explicitly
  anticipated, not a reverse-fit.

So `K_POS` is genuinely derived. **However** the inline comment
(`balance_app.cpp:1556-1560`) leans on "using it makes the derivation reproduce
the 4M.13 hardcodes … as §3.5/OQ-1 require" as the *justification*. That is the
wrong framing: the justification is the inverted-pendulum kinematics; matching
6.0 is a *check*, not a *reason*. The substance is sound; the rhetoric in the
comment is reverse-fit-flavoured and should be rewritten to lead with the
physics. **P2 (documentation), see §11-D.**

### 1.5 The `K_VEL` problem — OQ-1 partially still open

`K_VEL ≈ 11.68` is **3.9× the 4M.13 hand-pick of 3.0** — *outside* the §3.5 /
OQ-1 "within ~3×" sanity band. The landing report (lines 145-150) notices this
and waves it through as "expected — critical damping yields a larger velocity
term." That explanation is *plausible* but incomplete:

- With `ζ_o = 1`, `K_VEL/K_POS = 2ζ_o/ω_o = 2`. The 4M.13 ratio is `3.0/6.0 =
  0.5`. The hand-picked 4M.13 loop was therefore **far more lightly damped**
  (effective `ζ ≈ 0.25`) than critically damped — i.e. the 4M.13 author and the
  4M.14 `ζ_o = 1` choice *disagree on the damping target*, and 4M.14's `K_VEL`
  is right *only if* `ζ_o = 1` is right.
- `ζ_o = 1` is well-argued in design §3.1 (station-keeper must not overshoot).
  I concur with `ζ_o = 1` on the merits. So `K_VEL = 11.68` is most likely the
  *better* value and 3.0 was under-damped. But that means **OQ-1's "within 3×"
  test was mis-specified** — it assumed the 4M.13 hand-picks were near-optimal
  in *both* gains, and they were not in `K_VEL`.

Verdict on `K_VEL`: not a fault, but the band-violation must not be "noted and
moved on from." It is a **P2** — the §8.2 bench protocol now genuinely matters
for `K_VEL` (does the bot hunt or hold?), and the OQ-1 test threshold in any
future `test_outer_loop_gain_derivation.cpp` must use `~4×` for `K_VEL` or the
test will fail a *correct* derivation. The implementation never wrote that test
(§4 below), so the trap is latent.

### 1.6 Central-question summary

| Question | Answer |
|---|---|
| Correct `G_outer`? | `g · π/180 ≈ 0.1713 m/s²/deg`. Gain is `g`, **not** `g·r`, **not** `g_eff`. |
| Is impl's `g=9.81` correct? | **Yes** — first-principles inverted-pendulum kinematics. Not reverse-fit in substance. |
| Spec §2.2 wrong, or misread? | **Spec §2.2 is wrong** — spurious `·r` + `g_eff` (angular) used where `g` (linear) is needed. |
| Verdict | **(a)** implementation correct, spec had a bug. Caveat: inline rationale is reverse-fit-worded (P2); `K_VEL` busts OQ-1 band (P2). |

---

## 2. Correctness of the pole-placement implementation (review item 5)

`derive_position_gains_()` — `balance_app.cpp:1582-1653`.

- **`ω_n,inner = 4/ts`** — `balance_app.cpp:1609`, `ts = POSLOOP_INNER_TS_SEC =
  0.5` → 8 rad/s. Matches `plant_identifier.h:26,91-92`. ✓
- **`ω_o = ω_n,inner / N`** — `:1610`, `N = POSLOOP_INNER_OUTER_BW_RATIO = 8` →
  `ω_o = 1 rad/s`. Matches design §3.1. ✓
- **`K_POS = ω_o²/G_outer`** — `:1621`. ✓ structurally identical to inner
  `Kp = ω_n²/K_motor` (`plant_identifier.h:28`).
- **`K_VEL = 2·ζ_o·ω_o/G_outer`** — `:1622-1623`, `ζ_o = POSLOOP_OUTER_DAMPING
  = 1.0`. ✓ (magnitude caveat — §1.5.)
- **`POS_LEAK = exp(-dt/tau)`** — `:1624`, `expf(-dt_sec/POSLOOP_WASHOUT_TAU_S)`,
  `tau = 20 s`, `dt = pid_sample_ms·0.001 = 0.005 s`. `exp(-0.00025) = 0.99975`.
  Matches design §5.2 and the landing report. ✓
- **Sign / control law** — `position_loop.cpp:70` `nudge = -(k_pos_·pos) -
  (k_vel_·v)`, characteristic poly `s² + G·K_VEL·s + G·K_POS`. Matches design
  §3.1. ✓
- **`g_outer > 1e-9` and `dt_sec > 0` guards** — `:1620`. Belt-and-suspenders;
  with `g_lean` a literal `9.81` and `dt` derived from a `uint16_t` ms field
  these can only fail on absurd config, but harmless. ✓

`POS_LEAK` note: the *fallback* `POSLOOP_POS_LEAK_FALLBACK = 0.999` is a ~5 s
tau, but the *derived* value `0.99975` is the 20 s tau. The header comment at
`position_loop.h:79-83` still says "0.999 … ~5 s washout" — accurate for the
fallback, and the next sentence correctly says the derivation replaces it. OK,
but a reader skimming could conflate them. **P3** — minor comment polish.

**Pole-placement math: correct.** No P0/P1 in the formula implementation.

---

## 3. Fallback safety (review item 6)

`derive_position_gains_()` initialises `k_pos/k_vel/pos_leak` to the
`*_FALLBACK` constants at `balance_app.cpp:1586-1588` *before* any derivation.
The derived values overwrite **only** inside the innermost
`if (kpos_ok && kvel_ok && leak_ok)` block (`:1636-1641`). Every early-exit
path — `wheel_radius_valid` false, `g_outer`/`dt` guard false, any clamp miss —
leaves the fallback values intact. `set_gains()`/`set_pos_leak()` are then
called unconditionally at `:1646-1647` with whatever survived.

- `PositionLoop` constructor (`position_loop.cpp:25-27`) also seeds the three
  members from the `*_FALLBACK` constants, so even if `derive_position_gains_()`
  were never called (non-encoder build) the loop has usable gains.
- `set_pos_leak()` (`position_loop.cpp:42-48`) additionally rejects any leak
  outside `(0,1)` — defence in depth.
- The `*_FALLBACK` constants are compile-time `6.0 / 3.0 / 0.999` — never 0,
  never NaN.

**No path leaves `K_POS/K_VEL/POS_LEAK` at 0 or NaN.** ✓ Fallback safety is
sound. No finding.

One observation, **P3**: the radius validity gate is `wheel_radius_valid`,
computed at the call site `:1513` as `r > 0 && r < 1.0`. If
`enc_left_.wheel_radius_m()` returns a CRC-valid-but-garbage value *inside*
`(0,1)` (e.g. 0.5 m on a TT-wheel bot), the derivation still runs — but since
`r` no longer enters `G_outer` (the whole point of the deviation), a bad-but-
in-range `r` has **zero effect** on the gains. The gate is now almost vestigial:
it only blocks `r ≤ 0` / `r ≥ 1`. That is fine for safety but the design's
framing of `r` as "the evidence the encoder chain is sound" is now thin — a
trustworthy `r` is no longer load-bearing for the gains. Not a bug; a
documentation drift. See §11.

---

## 4. The verification test that was never written (review item, design §8.1)

Design §8.1 and §4.4 mandate a **NEW** `tests/test_outer_loop_gain_derivation.cpp`
with 6 coverage points — including §8.1-6, the OQ-1 "within ~3×" regression
encoded as a test "so a future plant-model change that breaks it is caught."

**That file does not exist.** `ls tests/` shows `test_position_loop.cpp`,
`test_tune_storage.cpp`, `test_tuning_session.cpp` — no derivation test. The
landing report does **not** mention it at all (it only discusses
`test_position_loop.cpp`). This is a **silent omission of a spec deliverable**.

Consequences:
- The `derive_position_gains_()` math has **zero automated coverage**. The
  `G_outer` deviation — the single most important and most error-prone part of
  4M.14 — is verified only by the implementer's hand-arithmetic in a doc.
- The OQ-1 sanity check exists nowhere executable. The `K_VEL` 3.9× band
  violation (§1.5) is recorded in prose and will be silently forgotten.
- §8.1-5 (`set_gains()` mechanism: `update()` honours new gains, `reset()`
  does not clear them) is also uncovered.

**P1** — a spec-mandated deliverable was dropped without acknowledgement. The
derivation is "trust me" code. Note `derive_position_gains_()` lives in
`BalanceApp` (Arduino-coupled) not `PlantIdentifier`, so it is awkward to unit-
test in isolation — but that is a *consequence* of the §4.1 home deviation and
does not excuse skipping it; the math is extractable.

---

## 5. `failure_reason = 9` (review item 7)

- **Collision check:** `BootstrapResult.failure_reason` codes 1-8 are documented
  at `balance_app.h:132-136`. `9` is unused there. `derive_position_gains_()`
  never writes `bootstrap_result_.failure_reason` — it writes a *separate*
  member `posgains_failure_reason_` (`balance_app.cpp:1652`,
  declared `balance_app.h:974`, accessor `:747`). **No collision.** ✓
- **Non-fatal:** `derive_position_gains_()` is called at `:1514`, *after*
  `bootstrap_result_.failure_reason = 0; converged = true` are already set
  (`:1498-1499`), and it never touches `bootstrap_result_` or calls
  `enter_state_(IDLE)`. Control falls straight through to
  `enter_state_(RUN)` at `:1521`. **`9` genuinely does NOT abort BOOTSTRAP.** ✓
- **Cleared on re-entry:** `posgains_failure_reason_ = 0` in the BOOTSTRAP
  `enter_state_` block (`:1088`) and in the constructor init list (`:208`).
  Clean slate per run. ✓

Failure-reason taxonomy is honest. No finding. **P3 nit:** `9` is used as a
bare literal in two places (`:1652`, and described in comments) rather than a
named enum/constant; if Workstream J ever wants the hard slot, grep-ability
suffers. Minor.

---

## 6. ISR / atomicity (review item 8)

`derive_position_gains_()` and `PositionLoop::set_gains()/set_pos_leak()` touch
**only** main-loop-side state: `k_pos_/k_vel_/pos_leak_` members of
`PositionLoop`, `posgains_failure_reason_`, and `cfg_.pid_sample_ms` (read).
The encoder ISR-shared state is the tick counters inside `WheelEncoder`;
`derive_position_gains_()` reads `enc_left_.wheel_radius_m()` (a calibration
constant, not ISR-mutated) at the call site `:1512`, before the function. No
`ATOMIC_BLOCK` needed; none missing. Runs once at BOOTSTRAP finalise, single-
threaded w.r.t. the control loop. ✓ No finding.

---

## 7. `#ifdef` hygiene (review item 9)

- The BOOTSTRAP call site (`:1501-1516`) is wrapped in `#ifdef
  USE_WHEEL_ENCODERS`. ✓
- `derive_position_gains_()` definition (`:1531-1654`) is wrapped
  `#ifdef USE_WHEEL_ENCODERS … #endif`. ✓
- Its declaration `balance_app.h:875` — within the existing encoder member
  block; `posgains_failure_reason_` (`:974`) and accessor (`:747`) likewise.
  The new `POSLOOP_*` constants (`balance_app.h:364-402`) — need to confirm
  they sit inside the encoder gate; they are referenced only from the gated
  function so even if ungated they cost only flash-free `constexpr`. Landing
  report claims `uno_balance` byte-identical (30184 B both baselines).
- `PositionLoop` itself (`position_loop.{h,cpp}`) is deliberately gate-free
  (compiles everywhere, called only under the gate) — `set_gains()` etc. add
  ~12 B RAM to the class unconditionally, but `uno_balance` does not
  instantiate the cascade path, so RAM is reclaimed. The landing report's
  "uno byte-identical" is consistent with the gating being correct.

I cannot independently re-run the build (READ-ONLY, no builds), but the gating
*as written in source* is correct and the byte-identical claim is plausible.
✓ No finding, conditional on the build numbers being truthful.

---

## 8. The `test_position_loop.cpp` 7/8 claim (review item 10)

**Verdict: the failure is a GENUINE pre-existing test-math bug. The impl agent's
characterisation is correct. Not a regression, not a hand-waved module bug.**

The failing case is `test_sign_convention` (`test_position_loop.cpp:105-149`),
specifically its third sub-block (`:128-148`). Worked arithmetic with
`v = 0.02`, `dt = 0.005`, `POSLOOP_POS_LEAK = 0.999`, `POSLOOP_K_POS = 6.0`,
`POSLOOP_K_VEL = 3.0`, `POSLOOP_SLEW_DEG_S = 2.0`:

```
pos_expected = v·dt·L           = 0.02·0.005·0.999 = 9.99e-5
law_expected = -(6.0·9.99e-5) - (3.0·0.02)         = -0.0606
max_step     = 2.0·0.005                            = 0.01
```

- **Line 141** `CHECK(|law_expected| < max_step, ...)` →
  `0.0606 < 0.01` → **FALSE**. The test's own setup assertion is arithmetically
  false: the velocity term `3.0·0.02 = 0.06` alone already exceeds the `0.01`
  slew step. The comment at `:127` ("small so |nudge| stays well inside the
  slew step") is simply wrong — `0.02 m/s` is not small enough.
- **Line 144** `CHECK_NEAR(nudge, law_expected, 1e-4)` then also fails:
  `update()` slew-limits the first step from 0 to `-max_step = -0.01`, so
  `nudge = -0.01`, not `-0.0606`. `|−0.01 − (−0.0606)| = 0.0506 > 1e-4`.

So `test_sign_convention` reports **2 failed checks**, both rooted in the same
test-author error: picking `v = 0.02` believing it dodges the slew limiter when
it does not.

**Is it pre-existing?** Yes, definitively. The loop here runs on the *fallback*
gains `6.0 / 3.0 / 0.999` — `position_loop.cpp:25-27` seeds the members from
`*_FALLBACK`, and `test_position_loop.cpp` never calls `set_gains()`. Those are
**byte-identical to the 4M.13 constants**. The arithmetic above uses only those
constants and the unchanged `update()` control-law + slew logic
(`position_loop.cpp:51-90`, untouched by 4M.14). The test would fail
*identically* against pre-4M.14 code. It is a defect in the test's expected-
value math, not a regression and not a real `PositionLoop` bug.

`test_position_loop.cpp` is in the impl agent's BLOCKED zone (untracked, not a
WRITE_ZONE file) so the agent correctly did **not** edit it. Flagging it as a
pre-existing test bug was the right call.

**P2** — the test bug is real and should be fixed (by whoever owns the test
zone): either drop `v` to ~`0.003` so `|law| < 0.01` actually holds, or rewrite
the sub-block to assert against the *slew-limited* expected value. Tracked here
so it is not lost. It is NOT a 4M.14 fault.

---

## 9. Is the 4M.13-13 sequencing flag genuinely resolved? (review item 11)

**Mostly yes — genuinely resolved in substance, with two loose ends.**

- The operating gains are now produced by `derive_position_gains_()`, a
  closed-form mechanism, not picked by an operator. The `position_loop.h:47-48`
  "HARDCODED — Do NOT bench-tune" comment is removed and replaced by the
  derive/fallback explanation (`position_loop.h:46-68`). ✓
- `K_POS` is a *true* analytical derivation — the physics (§1) supports
  `g_lean = g` independently of the 4M.13 target. The gate is **not** lifted on
  a reverse-fit; it is lifted on correct inverted-pendulum kinematics. ✓
- **Loose end 1:** the inline comment *frames* the choice as reproducing 6.0
  (§1.4). Anyone auditing `balance_app.cpp:1556-1560` reads reverse-fit
  rhetoric and could reasonably conclude the gate is *not* honestly lifted. The
  substance is fine; the words undermine it. **P2.**
- **Loose end 2:** there is no executable test (§4). "Derived, not hardcoded"
  is only as trustworthy as the one un-tested function. Until
  `test_outer_loop_gain_derivation.cpp` exists, the gate rests on a doc.
  **P1.**

So: the sequencing flag is **resolved**, not merely nominally — but the
resolution is under-documented and under-tested. The bench-tuning prohibition
(design §11) still holds: §8.2 is verification, and given the `K_VEL` 3.9×
surprise (§1.5) that bench verification is now *more* important, not less.

---

## 10. Findings summary

| ID | Pri | Finding | Location |
|---|---|---|---|
| F-1 | P1 | Spec-mandated `tests/test_outer_loop_gain_derivation.cpp` (design §8.1, §4.4) was never written; landing report does not acknowledge the omission. `derive_position_gains_()` has zero automated coverage. | `tests/` (absent) |
| F-2 | P1 | Inline derivation rationale is reverse-fit-worded — justifies `g=9.81` by "reproduces the 4M.13 hardcodes" rather than by inverted-pendulum kinematics. Substance is correct; framing makes the gate-lift look dishonest. | `balance_app.cpp:1550-1562` |
| F-3 | P2 | Derived `K_VEL ≈ 11.68` is 3.9× the 4M.13 `3.0` — outside the spec's OQ-1 "within 3×" band. Real (the 4M.13 loop was under-damped), but unverified and the OQ-1 test threshold would need widening for `K_VEL`. | `balance_app.cpp:1622-1623`; `phase_4m14_landed` L145-150 |
| F-4 | P2 | `test_position_loop.cpp::test_sign_convention` fails 2 checks (`:141`, `:144`) on a genuine pre-existing test-math bug (`v=0.02` exceeds the slew step). Correctly out of impl's zone; should be fixed by the test owner. | `test_position_loop.cpp:128-148` |
| F-5 | P2 | Spec §2.2 contains the dimensional bug (`·r` spurious, `g_eff` misused). The spec is a tracked artifact; it should be corrected/annotated so future workstreams don't re-derive from the wrong formula. | `phase_4m14_design §2.2` |
| F-6 | P3 | Wheel-radius validity gate is now near-vestigial — `r` no longer enters `G_outer`, so an in-range-but-wrong `r` has no effect. The design's "r = evidence the encoder chain is sound" framing is now thin. | `balance_app.cpp:1513,1596` |
| F-7 | P3 | `POS_LEAK` header comment (`position_loop.h:79-83`) describes the 0.999 / 5 s *fallback*; derived value is 0.99975 / 20 s. Accurate but skim-confusable. | `position_loop.h:79-83` |
| F-8 | P3 | `failure_reason = 9` is a bare literal in code/comments, not a named constant. Minor grep-ability cost if Workstream J formalises it. | `balance_app.cpp:1652` |

No P0. The control math, fallback safety, ISR safety, `#ifdef` gating and the
non-fatal-`9` taxonomy are all correct.

---

## 11. Final verdict and next steps

### Verdict: 4M.14 is SOUND-TO-KEEP, but needs a fix before the gate is honestly closed.

The deviation from the design spec is **correct** — case (a). The implementation
agent caught a real dimensional bug in design §2.2, used the physically-correct
`G_outer = g·π/180`, and the pole-placement math is right. `K_POS ≈ 5.84` is a
genuine first-principles derivation, not a reverse-fit. The fallback path, ISR
safety, `#ifdef` hygiene, and `failure_reason=9` non-fatal handling are all
sound. The `test_position_loop.cpp` 7/8 claim checks out — the one failure is a
genuine pre-existing test-math bug, correctly outside the impl agent's zone.

But the derivation is **under-tested and under-documented**, and one secondary
value is unverified. The bench-tuning gate (4M.13-13) is resolved *in substance*
but the resolution currently rests on a single un-tested function and a comment
that *reads* like a reverse-fit even though it is not. That is a P1-level gap:
keep the code, but do not consider the gate-lift "clean" until F-1 and F-2 are
addressed.

### Actionable next steps (priority order)

1. **(F-1, P1)** Write `tests/test_outer_loop_gain_derivation.cpp` per design
   §8.1. Extract the `derive_position_gains_()` math into a host-testable form
   (a free function in `control/`, or duplicate the four lines in the test).
   Cover: closed-form `K_POS/K_VEL/POS_LEAK` for nominal inputs; the clamp/
   fallback paths; `set_gains()`/`reset()` interaction. Use a **~4×** band for
   the `K_VEL` OQ-1 regression assertion, not 3× (see F-3).
2. **(F-2, P1)** Rewrite the `balance_app.cpp:1550-1562` comment to lead with
   the physics: "the small-angle inverted-pendulum lean→CoM-acceleration gain
   is `g` by kinematics (`a = g·sinθ ≈ gθ`); reproducing the 4M.13 hand-pick is
   a *consequence*, used only as a sanity check." Remove the "as §3.5/OQ-1
   require" phrasing.
3. **(F-5, P2)** Annotate `phase_4m14_design §2.2` with a correction note
   pointing at this review — the `g_eff·π/180·r` formula is dimensionally
   wrong and must not be used by a future re-derivation.
4. **(F-3, P2)** Escalate the `K_VEL = 11.68` vs `3.0` divergence to the §8.2
   bench protocol as a *specific* check: does the bot hunt (`ζ` too high in
   practice) or hold smoothly? The 4M.13 loop was effectively `ζ ≈ 0.25`; 4M.14
   commits to `ζ = 1`. One of them is wrong on the real chassis.
5. **(F-4, P2)** The test-zone owner fixes `test_sign_convention` — drop `v` to
   ~`0.003` or assert against the slew-limited output.
6. **(F-6/F-7/F-8, P3)** Tidy comments; consider naming the `9` literal.

*Review complete. READ-ONLY honoured — only this file written. No builds, no
git, no edits outside this document.*
