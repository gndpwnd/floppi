# Auto-Orientation Roadmap — Post-4M.14 Forward Plan

**Agent:** ao-roadmap-post4m14@floppi:1 (planning agent)
**Date:** 2026-05-20
**Status:** forward-looking roadmap — scopes the next 1–3 sessions
**Scope of this doc:** names and scopes the workstreams that follow Phase 4M.14, identifies the operator/architect decisions that gate them, and proposes a concrete 2-session dispatch.

> **Note on 4M.14 detail availability:** the sibling 4M.14 design doc
> (`docs/findings/phase_4m14_design_2026-05-20.md`) **did not exist on disk** when
> this roadmap was written. This document therefore proceeds using
> `architecture_plan_2026-05-20.md` §7 and `research_wheel_encoders_mega_2026-05-19.md`
> ("Recommended build order", item 5) as the source of truth for what 4M.14 becomes.
> If the sibling design lands with materially different scope, §3 below should be
> reconciled against it before dispatch.

---

## 1. Executive summary

The Mega balance stack has, over a single multi-agent session, gone from a
reverted collision detector to a complete two-stage cascade controller. Phases
4M.0 (collision restore) through 4M.13 (velocity/position outer loop) are landed
and build green on both `mega_balance` and `uno_balance`
(`phase_4m13_landed_2026-05-20.md` build table). The Uno path, on a parallel
track, shipped a complete guided P→I→D tuning feature, reviewed and cleared for
bench use with 0 P0/P1 findings (`guided_tuning_review_2026-05-20.md` verdict).

Where the project **actually is**: the Mega cascade is *code-complete and
review-sound* (`workstream_f_review_2026-05-20.md` verdict — "WORKSTREAM F IS
SOUND FOR BENCH DEPLOYMENT") but carries one open procedural gate. The five
outer-loop gains (`K_POS`, `K_VEL`, `MAX_NUDGE_DEG`, `POS_LEAK`, `SLEW_DEG_S`)
landed **hardcoded** as a deliberate sequencing-discipline decision
(`phase_4m13_landed_2026-05-20.md` §"Hardcoded gains"; architecture_plan §7
checklist row F.2). They are a working *mechanism*, not a tuned *value*, and the
review (workstream_f_review §4M.13-13 [P2-SEQ-1]) is explicit: **nobody may
bench-tune 4M.13 standalone until Phase 4M.14 lands the auto-derivation that
retires those constants.**

Where the **next 1–3 sessions** take the project:

1. **Next session — land 4M.14.** Auto-derive `K_POS`/`K_VEL` from the
   encoder-verified `K_motor` via pole-placement, exactly as the inner Kp/Kd are
   already derived. This is the gate; everything bench-facing waits behind it.
2. **Session after — Workstream G (bench-tuning protocol & telemetry).** With
   the cascade fully auto-derived, the first real bench session needs a
   *procedure*: telemetry capture, a gain logbook, plot scripts, and a
   regression baseline. Plus a small batch of tech-debt cleanup that has been
   accumulating across today's reviews.
3. **Session 3 and beyond — bring-up and hardening.** Failure-mode black-box
   recorder (Workstream J), disturbance-rejection benchmark (Workstream K), and
   the cross-bot config-unification question (Workstream M) that the diverging
   Mega/Uno trees are now raising.

The single highest-leverage operator decision (see §7) is **the bench-tuning
policy**: once 4M.14 auto-derives the gains, is operator override of those gains
permitted at all, or is the auto-derived value authoritative? That answer shapes
Workstream G's entire telemetry/logbook design.

---

## 2. State-of-the-bot snapshot

A factual recap, cited to today's landing reports and the Workstream F review.

### Mega path (`src/applications/balancing_robot/`)

| Phase | What landed | Source |
|---|---|---|
| 4M.0 | 3-gate collision detector restored (LIA peak/sustain/kick) | architecture_plan §1 |
| 4M.1 | `wheel_encoder.{h,cpp}` quadrature driver | architecture_plan §2 Workstream C |
| 4M.2 | Encoder-driven `K_motor` cross-check in BOOTSTRAP (`failure_reason=7`) | `phase_4m2_landed_2026-05-20.md` |
| 4M.11 | `e` serial command + EEPROM encoder calibration (slot 0x220) | `phase_4m11_landed_2026-05-20.md` |
| 4M.12 | PWM-range auto-discovery (`CHAR_PWM_RANGE`, `p` command, slot 0x230) | architecture_plan §2 Workstream E |
| 4M.13 | Velocity/position outer loop — `position_loop.{h,cpp}` cascade | `phase_4m13_landed_2026-05-20.md` |
| 4M.14 | Auto-derive outer-loop gains — **IN DESIGN** | architecture_plan §7; this doc §3 |

**Build state** (`phase_4m13_landed_2026-05-20.md` build table):
`mega_balance` — SUCCESS, 38192 B flash (15.0%), 1484 B RAM (18.1%).
`uno_balance` — SUCCESS, 30222 B flash (93.7%), **byte-identical** to the 4M.2
baseline (the whole cascade is `#ifdef USE_WHEEL_ENCODERS`'d out on Uno).

**Bench-ready?** Per `workstream_f_review_2026-05-20.md` "Verdict" — *"WORKSTREAM
F IS SOUND FOR BENCH DEPLOYMENT"*, 0 P0, 4 P1 (all informational/already
defended), 3 P2, 2 P3. The review's "Must-Fix Before Bench" section reads *"None
identified."* **So the code is bench-ready — but the gains are not.** The
review's Recommended Follow-Up #1 makes the 4M.14 dependency a hard procedural
gate (see §3).

### Uno path (`src/applications/balancing_robot_uno/`)

Guided P→I→D tuning feature: **complete + reviewed + cleared for bench use**
(`guided_tuning_review_2026-05-20.md` verdict — "FEATURE SOUND — CLEARED FOR
BENCH USE"). 0 P0, 0 P1, 1 P2 (defensive `apply_gains()` clamp), 1 P3 (style).
Implemented across `tune_storage.{h,cpp}`, `tuning_session.{h,cpp}`,
`uno_balance_app.{h,cpp}`, `main.cpp` and `platformio.ini` (new
`arduino_uno_tuning` env). 19 unit tests landed across `test_tune_storage.cpp`
and `test_tuning_session.cpp` (per git status — untracked test files).

**Important Uno limitation:** the Uno build is *guided-tuning-only*. There is a
flight build (`arduino_uno_minimal`) that reads tuned gains from EEPROM and just
balances — but the Uno has **no autonomous BOOTSTRAP/characterise path**; it
balances on whatever gains the operator tuned. This is by design
(`uno_guided_tuning_design_2026-05-20.md` §6 — "Explicitly NOT in scope: RLS,
BOOTSTRAP, ..."). See Workstream L (§4f) for whether this remains acceptable.

### EEPROM slot map — current state

Per `workstream_f_review_2026-05-20.md` §"EEPROM Slot Map", the **Mega** map is:

| Offset | Size | Purpose |
|---|---|---|
| 0x000–0x0FF | 256 B | BNO055 calibration blob |
| 0x200 | 8 B | Mounting offset |
| 0x210 | 8 B | Actuator (stiction) |
| 0x220 | 16 B | Encoder calibration (4M.11) |
| 0x230 | 8 B | PWM-discovery (4M.12) |
| 0x238–0xFFF | 3528 B | Free |

Workstream F review verdict: *"NO OVERLAPS"* — all magic bytes differ or
addresses are distinct, CRC-8-CCITT used project-wide. **Note** the Uno path
uses a *different* layout: a 19-byte tuning block at 0x200
(`guided_tuning_review_2026-05-20.md` §1) — i.e. the Uno's 0x200 holds the
PID-tune block while the Mega's 0x200 holds the mounting offset. The two trees
have **diverged** here; see Workstream M (§4g).

---

## 3. The 4M.14 gate — next session's main course

Per `architecture_plan_2026-05-20.md` §7 (the F.2 row of the
sequencing-discipline checklist) and `workstream_f_review_2026-05-20.md`
Recommended Follow-Up #1, **Phase 4M.14 is the gating item.** No bench-tuning of
4M.13 may happen until it lands.

### What 4M.14 must do

`research_wheel_encoders_mega_2026-05-19.md` "Recommended build order" item 5
states 4M.14's mandate: *"PlantIdentifier exposes a method to derive `K_POS,
K_VEL` from the verified `K_motor` via pole-placement (same trick as inner
Kp/Kd). Closes the universal-tune loop with no manual gains."*

This is squarely on-philosophy with scope.md §"The rule" — the outer-loop gains
are currently scope violations in the Mega tree (hardcoded numerics that are not
pin assignments), tagged as a stopgap mechanism. 4M.14 retires them by making
them *derived* from the BOOTSTRAP `K_motor` measurement that 4M.2 already
verifies against the encoders.

### Expected design-agent output (sibling, in flight)

The sibling `ao-phase-4m14-design@floppi:1` is writing
`phase_4m14_design_2026-05-20.md`. Expected to specify:

- The pole-placement math: outer-loop bandwidth chosen well below the inner
  loop's (the 4M.13 design holds the cascade slow via `SLEW_DEG_S` — 4M.14 must
  pick the outer poles consistent with that separation).
- A `PlantIdentifier` (or `position_loop`) entry point that consumes the
  verified `K_motor` and emits `K_POS`/`K_VEL`.
- What happens to `MAX_NUDGE_DEG`, `POS_LEAK`, `SLEW_DEG_S` — these are *bounds
  and washout*, not loop gains; they may legitimately stay structural (like
  CHARACTERISE's "6 pulses"), or derive from the inner-loop time constant.
- The seeding handoff: where in BOOTSTRAP/FINALISE the derived outer gains are
  pushed into `position_loop_`.

### Expected implementation workstream after the design lands

A single focused workstream — call it **4M.14-impl** — owning:
`position_loop.{h,cpp}` (accept derived gains; remove or demote the hardcoded
constants), `plant_identifier.{h,cpp}` (the derivation method), and
`balance_app.{h,cpp}` (wire the derived gains into `position_loop_` at FINALISE).
Complexity M. Fully `#ifdef USE_WHEEL_ENCODERS` — zero Uno impact.

### Implied test coverage

A new native test — `test_outer_loop_gain_derivation.cpp` (or
`test_position_loop_autoderive.cpp`) — that feeds a known `K_motor` into the
derivation and asserts `K_POS`/`K_VEL` match the closed-form pole-placement
result, plus edge cases (very small `K_motor`, NaN guard). The existing
`test_position_loop.cpp` (untracked, present on disk per git status) covers the
cascade *mechanism*; the new test covers the *derivation*. Both must stay green.

---

## 4. Post-4M.14 workstreams — proposed, prioritized

Six workstreams selected from the candidate list, plus one (4M.14-impl) that is
the immediate execution of §3. Selection rationale: G, J, K, M, N, O chosen
because they are unblocked by 4M.14 and address concrete gaps the reviews
surfaced; H and L are deferred (justification below the table).

### Workstream cards

| # | Name | Goal | Inputs (gates) | Outputs | Files touched | Size | Disposition |
|---|---|---|---|---|---|---|---|
| G | Bench-tuning protocol & telemetry pipeline | Give the operator a repeatable bench procedure + telemetry capture for the cascade | 4M.14 landed; bench access; **operator bench-tuning policy decided (§7)** | Bench procedure doc, telemetry-dump format, host plot script, gain logbook template, regression baseline | `docs/`, `main.cpp` (telemetry dump), `tools/` (plot script) | M | HYBRID |
| J | Failure-mode black-box recorder | Structured ring-log of the last N pulses'/transitions' summaries for post-bench analysis | None hard — can land code-only; bench access makes it *useful* | `PulseLog` ring extension, `s`-command drain format, native test | `balance_app.{h,cpp}`, `main.cpp`, `tests/` | S–M | HYBRID |
| K | Disturbance-rejection benchmark | Quantify the outer loop's actual drift-rejection (push the bot, measure recovery) | 4M.14 landed; G's telemetry pipeline; bench access + drive surface | Benchmark procedure, a scripted disturbance recipe, pass/fail metric | `docs/`, `tools/` (analysis), possibly `main.cpp` (a test-injection hook) | M | BENCH |
| M | Cross-bot config-unification audit | Decide what (if anything) the Mega and Uno trees should share, and scope the refactor | **Operator decision (§7): pursue now vs defer** | An audit doc; if "pursue", a shared-module extraction plan | `docs/` only (audit); refactor deferred to its own workstream | S (audit) / L (refactor) | CODE-ONLY |
| N | Build-matrix CI / regression | Make every active env compile in CI with flash/RAM budget enforcement | None — pure infra | CI script / test harness; per-env flash+RAM budget asserts | `tools/`, CI config, possibly `tests/` | S–M | CODE-ONLY |
| O | IMU calibration UX review | Confirm the BNO055 cal blob (0x000) UX is clear, documented, operator-friendly | None hard | A UX review doc; small doc/prompt fixes if gaps found | `docs/`, possibly `main.cpp` (prompt strings) | S | CODE-ONLY |

### Per-workstream detail

#### Workstream G — Bench-tuning protocol & telemetry pipeline  *(TOP OF QUEUE after 4M.14)*

- **Why now:** the cascade is code-complete and (after 4M.14) auto-derived, but
  there is *no operator-facing bench procedure*. `phase_4m2_landed` already
  stamps a `PulseLog` summary record (sentinel `pulse_idx=0xFD`) for the K
  cross-check, and `phase_4m13_landed` notes the inner loop now tracks a moving
  setpoint — but nobody has defined how an operator captures, plots, and judges a
  bench run. Without this, the first bench session degenerates into the exact
  "iterate on a constant" failure mode scope.md §"Process doctrine" warns
  against. G is the *anti-trap* deliverable: it makes the bench session produce a
  measurement-backed verdict, not a vibe.
- **Risks:** (1) telemetry dump at RUN rate could blow the serial budget or
  perturb the 5 ms PID tick — must be a low-rate or drain-on-demand design; (2)
  scope creep into a full dashboard — keep it a flat serial format + offline
  plot script; (3) the plot script is host tooling — must not become a field
  dependency (scope.md §"Development" — offline builds); (4) defining a
  "regression baseline" for a physical bot is inherently noisy — the baseline is
  a *band*, not a point.
- **Disposition: HYBRID** — telemetry format + plot script are code-only; the
  procedure and the baseline capture need the bench.

#### Workstream J — Failure-mode black-box recorder

- **Why now:** the balance app now has 8 documented `failure_reason` values
  (`workstream_f_review` §4M.2-4 lists 1–8: pitch_OOR, no_response, k_OOB,
  user_abort, collision, baseline_noisy, k_disagreement, pwm_discovery_timeout).
  When a bench run fails, the operator currently sees one reason code and
  nothing else. A small ring-buffer of the last N `PulseLog` summaries +
  state-transition records, drainable over `s`, turns "it fell over" into a
  diagnosable trace. This pairs naturally with G (G captures *good* runs, J
  captures *failed* ones).
- **Risks:** (1) RAM — a ring of N records costs RAM the Mega has (RAM is 18%)
  but the size must be bounded and documented; (2) the recorder must not itself
  introduce an ISR-safety hazard — writes from the RUN path must follow the
  established ATOMIC_BLOCK discipline (`workstream_f_review` §4M.2-6); (3) risk
  of becoming a "session-summary deliverable" trap (scope.md §"Process
  doctrine" #5) — keep it a *diagnostic primitive*, not a report generator.
- **Disposition: HYBRID** — code-only to build; bench access proves it.

#### Workstream K — Disturbance-rejection benchmark

- **Why now:** the entire point of the 4M.13 outer loop is *station-keeping —
  reject drift, hold position* (`phase_4m13_landed` §"The cascade design").
  Nobody has *quantified* whether it actually does. Once 4M.14 makes the gains
  trustworthy, K answers "by how much, and how fast does it recover?" — a real
  number for the master plan. It depends on G's telemetry pipeline existing
  first (you cannot measure recovery without a capture path).
- **Risks:** (1) repeatable disturbance injection is hard by hand — may need a
  scripted PWM perturbation hook, which itself is a small code addition; (2) the
  metric must be defined *before* the bench session or it becomes subjective;
  (3) bench-only — fully blocked without hardware + a drive surface.
- **Disposition: BENCH.**

#### Workstream M — Cross-bot config-unification audit

- **Why now:** the Mega and Uno trees have **measurably diverged** — different
  state machines (Mega: BOOTSTRAP/RUN/HELD/CHAR_*; Uno: guided TuningSession),
  different EEPROM layouts (Mega 0x200 = mounting offset; Uno 0x200 = PID-tune
  block — see §2), different PID call sites. Both still share a `pid_controller`
  and the `calibration_storage` CRC-8 routine. The question — *should there be a
  shared module for PID / plant-ID / CRC?* — is worth a deliberate **audit**
  before the divergence calcifies further. The audit is cheap (doc-only); the
  *refactor* it might recommend is L-sized and must be its own workstream.
- **Risks:** (1) premature unification is its own anti-pattern — two genuinely
  different programs should not be force-merged; the audit must be willing to
  conclude "leave them separate"; (2) the EEPROM 0x200 collision is a *latent
  footgun* — if any code is ever shared between trees, the conflicting 0x200
  meaning is a bug waiting to happen — flag this regardless of the audit's
  verdict; (3) refactor touches both trees → large conflict surface, must be
  serialized against any other balance-app work.
- **Disposition: CODE-ONLY** (the audit; refactor deferred).

#### Workstream N — Build-matrix CI / regression

- **Why now:** scope.md §O7 ("Test discipline") explicitly calls for a
  "multi-MCU compile-matrix CI so cross-platform breakage is caught at PR time",
  and §"Compile-time regression test" wants a CI grep for hardcoded-constant
  scope violations. Today, **no test compiles the build matrix** — each landing
  report manually runs `pio run -e mega_balance` / `-e uno_balance`. With the
  Uno now having *three* envs (`uno_balance`, `arduino_uno_minimal`,
  `arduino_uno_tuning`) plus the Mega envs, manual verification is fragile.
  Flash budget is the live concern: `uno_balance` sits at 93.7%
  (`phase_4m13_landed` build table) — a CI flash-cap assert would have caught
  any regression automatically.
- **Risks:** (1) CI infrastructure may not exist in this repo yet — N may have
  to bootstrap it; (2) compiling every AVR env is slow — may need a curated
  subset; (3) the scope-violation grep (scope.md's stated goal state) is
  valuable but easy to make over-eager — start with the compile+budget matrix,
  defer the grep.
- **Disposition: CODE-ONLY.**

#### Workstream O — IMU calibration UX review

- **Why now:** the BNO055 calibration blob lives at EEPROM 0x000 (256 B). With
  the bot now sensitive to small pitch nudges from the outer loop
  (`phase_4m13_landed` — nudges of order ±0.6° matter), a stale or poorly
  captured IMU calibration directly degrades the cascade. O is a *review* —
  confirm the cal wizard UX is clear, the workflow documented, the operator
  knows when to re-cal. Small fixes only; if it finds a real gap it scopes a
  follow-up.
- **Risks:** (1) may overlap with the orientation-framework cal path (not just
  balance) — keep the review scoped to the balance bot's use of it; (2) could
  surface KI-2/KI-3 from scope.md §"Known issues" (BNO085 accuracy collapse, no
  sensor-ID byte) — those are framework-level, flag-and-defer.
- **Disposition: CODE-ONLY.**

### Deferred candidates — and why

- **Workstream H — Auto-startup sequence on power-on.** *Deferred.* An
  auto-BOOTSTRAP-on-power-up has real safety implications (a bot that
  characterises itself — pulsing motors — the instant power is applied, with no
  operator ready). This is a genuine operator decision (§7), not a coding
  task, and it should not be scoped until the operator rules on
  power-on-balance-immediate vs operator-arm. Until then, operator-initiated
  BOOTSTRAP (the current `c`/`b` path) is the safe default.
- **Workstream L — Uno autonomous balance mode.** *Deferred.* The Uno path is
  *intentionally* guided-tuning-only (`uno_guided_tuning_design` §6 —
  "Explicitly NOT in scope: RLS, BOOTSTRAP..."; scope.md §"Platform
  bifurcation" — Uno is the "minimal" build). The flight build
  (`arduino_uno_minimal`) **already balances autonomously** on EEPROM-stored
  gains — so "the Uno can't balance autonomously" is not quite accurate; it
  *can*, it just can't *self-tune*. Adding on-MCU adaptation to the Uno would
  re-merge the very bifurcation scope.md §"Platform bifurcation" deliberately
  created. L should only be revisited if the operator explicitly reverses that
  pivot. **Flag: any proposal to add BOOTSTRAP-class logic to the Uno tree is a
  scope.md §"Platform bifurcation" violation.**

---

## 5. Cross-cutting tech-debt

A batch of small items pulled from today's reviews. Each should ride along with
a workstream rather than warrant its own session. None is a correctness blocker.

| # | Item | Source | Severity | Ride with |
|---|---|---|---|---|
| TD-1 | Comment in `balance_app.h` explaining why `raw_gyro_dps_[]` reads are safe (write-side ATOMIC_BLOCK) | workstream_f_review §4M.2-6 [P1-ISR-1] | P2 | 4M.14-impl (touches balance_app) |
| TD-2 | Clarify `failure_reason=7` vs motor-stall comment on `balance_app.cpp:609` (stall → HELD via `held_entry_reason_`; 7 is k_disagreement) | workstream_f_review §HYG-1 [P2-HYG-1] | P2 | 4M.14-impl or J |
| TD-3 | Add K-disagreement threshold rationale comment in `balance_app.h:263-270` (benign 10–20% offset, 2× slip/bind ratio) | workstream_f_review §4M.2-DOC-1 [P2-DOC-1] | P2 | 4M.14-impl |
| TD-4 | Expand `position_loop.h` `POS_LEAK` time-constant derivation comment | workstream_f_review §4M.13-DOC-2 [P2-DOC-1] | P2 | 4M.14-impl (rewrites position_loop) |
| TD-5 | Defensive input clamp in Uno `apply_gains()` (negative kp/ki/kd → 0) | guided_tuning_review §P2 | P2 | Workstream M (touches Uno tree) or a Uno cleanup pass |
| TD-6 | EEPROM 0x200 meaning collision Mega-vs-Uno — document explicitly so no future shared code aliases it | this doc §2; workstream_f_review §"EEPROM Slot Map" | P2 | Workstream M (the audit's job) |
| TD-7 | Encoder velocity bias sensitivity — `position_loop` integrator can wind up over a >10 min RUN despite `POS_LEAK` | workstream_f_review §4M.13-9 [P2-NUM-1] | P2 (informational) | Workstream K (the disturbance benchmark would surface it) |
| TD-8 | `position_loop` divide/NaN guards already present — add a regression test that exercises `dt<=0` and extreme `wheel_vel` | workstream_f_review §4M.13-4/§4M.13-8 | P3 | 4M.14-impl test work |

**Note on the 3 "P2 cleanups":** TD-1, TD-3, TD-4 are the comment-only P2s the
brief refers to from `workstream_f_review`; TD-2 is the `failure_reason=7`
comment clarification (workstream_f_review §HYG-1); TD-5 is the
`guided_tuning_review` P2. All are documentation/defensive — they belong
*bundled with* a workstream that already opens the relevant file, never as a
standalone session.

---

## 6. Hardware-availability dependencies

| Workstream | Hardware needed | Procurement gate? |
|---|---|---|
| 4M.14-impl | None — pure software + native test | No |
| G (bench protocol) | Bench setup: level drive surface, powered bot, encoder cable harness, serial host | **Yes, soft** — needs a confirmed bench session; no *new* parts beyond the existing Mega+BNO055+encoders+L298N rig |
| J (black-box recorder) | None to build; bench access to validate | No (build) / soft (validate) |
| K (disturbance benchmark) | Bench + drive surface; ideally a repeatable disturbance fixture (a calibrated push, a tilt ramp, or a known mass drop) | **Yes, soft** — the *fixture* may need fabrication; a hand-push is a fallback but noisier |
| M (config audit) | None | No |
| N (build-matrix CI) | None — host-side | No |
| O (IMU cal UX) | Bench access helps but a desk + serial host suffices | No |

**Procurement flags:**
- No workstream requires a *new* sensor or MCU — the existing Mega + BNO055 +
  wheel encoders + L298N rig covers everything. The RWE doc
  (`research_wheel_encoders_mega_2026-05-19.md` §References) lists candidate
  encoder models (DFRobot FIT0458, Adafruit N20 4639, Waveshare N20) — these are
  *already-chosen* hardware, not a fresh gate.
- **K's disturbance fixture** is the only thing that might want fabrication. A
  hand-push is an acceptable fallback (the metric becomes a band, not a point),
  so K is *not* hard-blocked — but a fixture would tighten the result.
- **The real gate is bench *time*, not bench *parts*** — G, K, and the
  validation halves of J need a confirmed operator bench session. See §7 and
  §10.

---

## 7. Operator decisions pending

Explicit yes/no/which-option items the operator must rule on. Ordered by how
much downstream work each gates.

1. **Bench-tuning policy — who may touch the gains? (gates the most work.)**
   Once 4M.14 auto-derives `K_POS`/`K_VEL` from `K_motor`, is the auto-derived
   value *authoritative* (operator may observe but not override), or is an
   operator override path permitted? This shapes Workstream G's entire
   telemetry/logbook design — an authoritative-only world needs a *verification*
   logbook; an override-permitted world needs an *adjustment* logbook with
   provenance tracking. scope.md §"The rule" and §"Process doctrine" lean hard
   toward authoritative-only (operator override of a derived gain is the exact
   anti-pattern the doctrine forbids) — **recommended default: auto-derived is
   authoritative; the operator's lever is re-running BOOTSTRAP, not editing a
   gain.** Confirm.

2. **Auto-startup safety — power-on-balance-immediate vs operator-arm?**
   (Gates Workstream H entirely.) Should the bot auto-BOOTSTRAP on power-up
   (pulsing its motors with no operator ready), or stay operator-armed (current
   `c`/`b` path)? Recommended default: **operator-arm** — a self-pulsing bot at
   power-on is a safety hazard for a hobby/research rig (scope.md §"Non-goals" —
   no production safety certification). H stays deferred until this is ruled on.

3. **Cross-bot module unification — pursue the refactor now, or defer?**
   (Gates the refactor half of Workstream M.) The *audit* (M, S-sized) should
   run regardless. The *refactor* it might recommend (L-sized, touches both
   trees) needs an explicit go/no-go. Recommended: **run the audit; defer the
   refactor decision until the audit quantifies the actual shared surface.**

4. **Bench access — is an operator bench session available in the next 1–3
   sessions?** (Gates the bench/validate halves of G, J, K.) If no, those
   workstreams still land their code-only halves green on native tests, and the
   bench-validate gates simply defer — exactly the pattern architecture_plan §5
   risk #6 ("Bench-access dependency") established. Confirm timing.

5. *(Carried-over, low-urgency)* **Wireless posture.** The FC project faces a
   swarm-API auth/TLS decision; AO has no wireless today (scope.md §"What is OUT
   of scope" — "Cloud connectivity or public-Internet exposure" is *permanently*
   out). If AO ever adds ESP32 WiFi telemetry (scope.md §O5 — a *planned* Phase
   6 item), it inherits the same auth question. **Not actionable now** — flagged
   only so the master plan acknowledges the future analog.

---

## 8. Suggested 2-session plan

### Next session — "Land the gate"

**Primary: Workstream 4M.14-impl** (the implementation of the §3 design).
Single focused agent; the design doc is the contract.

Dispatch table:

| Agent | Workstream | Write-zone (exclusive) |
|---|---|---|
| `ao-phase-4m14-impl@floppi:1` | 4M.14-impl | `position_loop.{h,cpp}`, `plant_identifier.{h,cpp}`, `balance_app.{h,cpp}`, `tests/test_outer_loop_gain_derivation.cpp` (NEW) |
| `ao-ci-matrix@floppi:1` | N (build-matrix CI) | `tools/` CI script, CI config — fully orthogonal, no source overlap |

Tech-debt riding along: TD-1, TD-2, TD-3, TD-4, TD-8 all land *inside*
4M.14-impl (it already opens `balance_app.{h,cpp}` and rewrites
`position_loop.{h,cpp}`). Workstream N is dispatched in parallel because it
touches zero source files — pure infra, zero conflict with 4M.14-impl.

**Exit criterion:** `mega_balance` builds green with auto-derived outer gains;
`test_outer_loop_gain_derivation` + `test_position_loop` pass; `uno_balance`
byte-identical. The §3 gate is now *closed* — bench-tuning is unblocked.

### Session after — "Make the bench session real"

**Primary: Workstream G (bench-tuning protocol & telemetry pipeline).**
**Gated by operator decision #1 (§7) — must be answered before G is dispatched**
because it shapes G's logbook design.

Runs alongside:
- **Workstream M (config-unification audit)** — doc-only, orthogonal, no source
  conflict. Carries TD-5, TD-6.
- **Workstream J (black-box recorder)** — code half can land here; touches
  `balance_app.{h,cpp}` + `main.cpp`, so it must serialize *after* 4M.14-impl
  (file conflict) — i.e. this session is the earliest J can run.

Dispatch note: G and J both touch `main.cpp` (telemetry dump). If both run the
same session, either serialize them or partition `main.cpp` carefully — J owns
the `s`-drain extension, G owns the periodic RUN dump. Prefer **G this session,
J the next** if the `main.cpp` partition is awkward.

**Workstream K** follows G (it needs G's telemetry pipeline) — session 3.
**Workstream O** is a low-priority filler that can slot into any session with
spare agent capacity.

---

## 9. Long-range stretch items

Named so the master plan acknowledges them; all are clearly beyond the next 2
sessions, and each is scope-checked.

1. **Fall recovery / self-righting.** A bot that has FALLEN actively driving its
   wheels to flip itself upright. *Scope check:* this is balance-bot application
   logic, **in scope** for `src/applications/balancing_robot/` — but it is a
   large controls problem (a separate maneuver controller) and far past the
   cascade work. Worth naming; not worth scoping yet.

2. **Lateral / heading cascade (Phase 4M.16).** RWE §"See also" already names
   this — left-vs-right wheel velocity disambiguation using `body_heading_unit`
   from Phase 2.7, so the bot holds *heading* as well as *position*. *Scope
   check:* in scope (it is the natural completion of the position cascade), but
   it depends on the single-axis cascade (4M.13/4M.14) being bench-proven first.

3. **ESP32 WiFi telemetry for the balance bot.** scope.md §O5 makes ESP32
   WiFi + dashboard a *planned Phase 6* framework feature, and `esp32_balance`
   is already a scaffolded env (scope.md build-env table). A browser dashboard
   would make Workstream G's telemetry pipeline far richer. *Scope check:* in
   scope as a framework objective — but it is a Phase 6 item, and scope.md
   §"What is OUT of scope" permanently forbids *cloud/public-Internet* exposure,
   so any such work is strictly LAN-only. **Flag:** do not let "telemetry
   dashboard" drift into a hosted/cloud service.

**Scope-creep watchlist** (named so a future session recognizes the trap):
trajectory planning, autonomous obstacle avoidance, and multi-bot coordination
are **permanently out of scope** (scope.md §"What is OUT of scope" —
"Trajectory planning / autonomy", "Multi-bot fleet coordination (use
`swarm_api/`)"). If a roadmap discussion drifts toward "the bot should navigate
to a waypoint", that is a scope violation — the bot's job is *orientation and
station-keeping*, not navigation.

---

## 10. Risks to the roadmap itself

What could derail this plan, and the mitigation.

1. **4M.14 derivation proves intractable (HIGH impact, LOW–MED likelihood).**
   If the pole-placement derivation cannot produce stable outer gains across the
   plausible `K_motor` range — e.g. the outer/inner bandwidth separation the
   4M.13 `SLEW_DEG_S` assumes turns out inconsistent with any derived `K_POS` —
   then 4M.14 cannot retire the hardcoded gains, and the §3 gate stays *open
   indefinitely*. *Mitigation:* the sibling design agent should explicitly state
   the `K_motor` range over which the derivation is valid; if it is narrow, the
   fallback is a *bench-characterised* outer gain (a CHARACTERISE-style
   measurement) rather than a permanent hardcode — still on-philosophy, just a
   different mechanism. **A permanent hardcode is not an acceptable fallback**
   (scope.md §"The rule").

2. **No bench access in the next 1–3 sessions (MED impact, MED likelihood).**
   Workstreams G, K, and J's validation half all need an operator bench
   session. *Mitigation:* per architecture_plan §5 risk #6, the code-only halves
   land green on native tests and the bench-validate gates defer — the roadmap
   does not *stall*, it just accumulates an un-validated queue. The risk is that
   the queue grows long enough that a later bench session is overwhelming.

3. **Operator unavailable for decision #1 (MED impact, LOW likelihood).**
   Workstream G cannot be *correctly* designed without the bench-tuning policy
   answer. *Mitigation:* the recommended default (auto-derived is authoritative)
   is strongly indicated by scope.md doctrine — G could proceed on that
   assumption and be revised if the operator rules otherwise. But proceeding on
   an assumption is a real risk; prefer to get the answer.

4. **Mega RAM/flash creep (LOW impact, LOW likelihood).** architecture_plan §5
   risk #1 flagged Mega RAM as a concern, but the 4M.13 build table shows RAM at
   18.1% and flash at 15.0% — ample headroom. Workstream J's ring buffer is the
   only meaningful RAM consumer ahead, and it is bounded by design.
   *Mitigation:* Workstream N's flash/RAM budget assert catches any regression
   automatically once it lands.

5. **`main.cpp` conflict between G and J (LOW impact, MED likelihood).** Both
   want to add telemetry to `main.cpp`. *Mitigation:* §8 already calls for
   serializing them or partitioning `main.cpp` by responsibility — this is a
   known, managed conflict, not a surprise.

6. **The bifurcated trees diverge faster than Workstream M can audit them (LOW
   impact, MED likelihood).** Every balance-app change widens the Mega/Uno gap.
   *Mitigation:* M is scheduled early (session-after-next) precisely so the
   audit happens before the divergence calcifies; the EEPROM 0x200 collision
   (TD-6) is flagged now so it is not forgotten.

---

## Appendix — source citations

- `architecture_plan_2026-05-20.md` — §1 (4M.0 scope), §2 (workstream
  partition), §4 (parallelization), §5 (risk register), §7
  (sequencing-discipline checklist).
- `workstream_f_review_2026-05-20.md` — EEPROM slot map, finding summary,
  §4M.2-4 (failure_reason enum), §4M.2-6 (ISR-1), §4M.13-9 (NUM-1), §4M.13-13
  (SEQ-1 — the 4M.14 gate), §HYG-1, §DOC-1, Verdict, Recommended Follow-Up.
- `phase_4m2_landed_2026-05-20.md` — K cross-check, `failure_reason=7`,
  `PulseLog` summary record, build table.
- `phase_4m11_landed_2026-05-20.md` — `e` command, EEPROM slot 0x220.
- `phase_4m13_landed_2026-05-20.md` — cascade design, hardcoded gains
  sequencing flag, build table.
- `uno_guided_tuning_design_2026-05-20.md` — Uno bifurcation, "Explicitly NOT in
  scope" (BOOTSTRAP/RLS), env split.
- `guided_tuning_review_2026-05-20.md` — Uno feature verdict, P2 clamp finding,
  EEPROM 0x200 tuning block.
- `research_wheel_encoders_mega_2026-05-19.md` — §6 (cascade integration),
  "Recommended build order" item 5 (4M.14 mandate), §"See also" (4M.16 lateral
  cascade).
- `scope.md` — §"Platform bifurcation", §"The rule", §"Process doctrine",
  §"What is OUT of scope", §O5/§O7.

*Roadmap complete. The §3 gate (Phase 4M.14) is the critical path; everything
bench-facing waits behind it.*
