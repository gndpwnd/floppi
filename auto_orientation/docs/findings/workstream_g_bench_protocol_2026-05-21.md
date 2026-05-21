# Workstream G — Bench-Tuning Procedure for the Auto-Derived 4M.13/4M.14 Cascade

**Agent:** ao-wsg-docs@floppi:1 (documentation agent)
**Date:** 2026-05-21
**Workstream:** G — bench-tuning protocol & telemetry pipeline
**Status:** operator procedure — first-bench-session edition
**Scope:** the now-auto-derived velocity/position outer-loop cascade on the
Mega balance bot. Inner pitch PID + outer `PositionLoop` (`K_POS`, `K_VEL`,
`POS_LEAK`) as landed by Phases 4M.13 and 4M.14.

---

## 0. What this document is — and is not

Phase 4M.14 retired the Phase 4M.13 "HARDCODED — Do NOT bench-tune" sequencing
flag: the three dynamic outer-loop gains (`K_POS`, `K_VEL`, `POS_LEAK`) are now
**derived in closed form at BOOTSTRAP finalise** by
`BalanceApp::derive_position_gains_()` and pushed into the live loop via
`PositionLoop::set_gains()` / `set_pos_leak()`
(`phase_4m14_landed_2026-05-20.md`). The two safety saturations
(`MAX_NUDGE_DEG`, `SLEW_DEG_S`) stay hardcoded by design.

**This is therefore a *verification* procedure, not an *adjustment* procedure.**
Consistent with the recommended bench-tuning policy
(`ao_roadmap_post_4m14_2026-05-20.md` §7 decision #1 — "auto-derived is
authoritative; the operator's lever is re-running BOOTSTRAP, not editing a
gain"), the operator does **not** hand-edit `K_POS`/`K_VEL`/`POS_LEAK`. The
operator's job at the bench is to:

1. confirm the derivation **ran** and produced gains in the sane envelope,
2. capture telemetry of a real RUN session,
3. judge the captured behaviour against pass/fail bands, and
4. record the result in the gain logbook
   (`docs/guides/gain_logbook_template.md`).

If a band fails, the corrective action is a **firmware/derivation issue to be
escalated** — not a gain the operator twiddles. The only operator-side knobs
are upstream: re-run encoder calibration (`e`), re-run PWM discovery (`p`),
re-run BOOTSTRAP (`b`).

> **Honesty note on numbers.** This document deliberately contains **no
> invented numeric thresholds** for the pass/fail bands. The acceptance bands
> in §5 are bench-measurements: they cannot be predicted in advance and must be
> filled in from the *first good run*. Where a number *is* stated, it is either
> (a) a firmware constant quoted from source, or (b) a derived value quoted
> from `phase_4m14_landed_2026-05-20.md` and explicitly labelled as a
> bench-confirmation target, not a threshold.

---

## 1. Pre-bench checklist

Do not start a RUN session until every box below is checked. These gate the
validity of everything captured afterward.

### 1.1 Build & flash

- [ ] Firmware is the `mega_balance` env (auto-defines `USE_WHEEL_ENCODERS` —
      the entire cascade and the 4M.14 derivation are `#ifdef`'d out on Uno).
- [ ] The exact firmware commit hash is recorded — you will write it into the
      logbook. The cascade behaviour is meaningless without knowing which
      build produced it.
- [ ] Serial monitor open at **115200 baud** (`pio device monitor -b 115200`).

### 1.2 IMU calibration

- [ ] BNO055 calibration blob is fresh (EEPROM slot `0x000`). The outer loop
      nudges the pitch setpoint by sub-degree amounts; a stale IMU calibration
      directly corrupts the cascade. Re-run the IMU cal wizard if in doubt
      (see `docs/guides/FIRST_CALIBRATION.md`).
- [ ] Mounting offset is current (EEPROM slot `0x200`) — recapture via `c`
      (short-press / CAPTURE_MOUNTING) if the bot has been re-assembled.

### 1.3 Encoder check

This is the input the entire outer loop trusts. Run the encoder bring-up first
if it has never been done — `docs/guides/encoder_bench_bringup.md`.

- [ ] Encoder calibration present and valid in EEPROM slot `0x220`
      (counts-per-metre + wheel radius). Re-run the `e` wizard if missing.
- [ ] Direction check passed: hand-rolling each wheel **forward** increases
      ticks (encoder_bench_bringup §3). A wheel that counts backward will fight
      the cascade.
- [ ] Distance verification passed: a hand-pushed 1.00 m reads within ±5%
      (encoder_bench_bringup §6). The wheel radius feeds the 4M.14 derivation's
      encoder-geometry validity gate (`r > 0 && r < 1.0`); a bad radius forces
      the cascade onto the conservative fallback gains.

### 1.4 PWM-discovery run

- [ ] PWM-range auto-discovery has been run (`p` command, Phase 4M.12) and the
      result is saved to EEPROM slot `0x230`. The bot **must be lifted off the
      ground** for the entire ramp — a bot on the floor launches off the bench
      when PWM crosses stiction.
- [ ] The discovered `PWM_MIN`/`PWM_MAX` per motor look sensible
      (encoder_bench_bringup §8 gives a rough sanity range).

### 1.5 BOOTSTRAP / K cross-check

- [ ] A clean BOOTSTRAP completes without a `BootstrapResult.failure_reason`
      (1 pitch_OOR, 2 no_response, 3 k_OOB, 4 user_abort, 5 collision,
      6 baseline_noisy, 7 k_disagreement, 8 pwm_discovery_timeout).
- [ ] In particular, **`failure_reason=7` (k_disagreement) did not fire** — the
      4M.2 encoder-vs-gyro `K_motor` cross-check passed. The 4M.14 derivation
      runs *after* this check; if the cross-check failed, the gains were never
      derived from trusted data.

### 1.6 Bench environment

- [ ] Level drive surface, enough clear floor for the bot to hold station and
      to drift a little without hitting anything.
- [ ] Operator within reach of the **`a` emergency-abort** key for the entire
      session. Encoders are an outer-loop *input*, not a kill switch.
- [ ] Absolute-pitch kill-switch is active in firmware (±20° → force-stop) — a
      backstop, not a substitute for the operator's hand on `a`.

---

## 2. Confirm the derivation ran (before the bot ever balances)

The 4M.14 derivation happens at BOOTSTRAP FINALISE. Before judging RUN
behaviour, confirm what gains were installed.

1. Complete a clean BOOTSTRAP (§1.5).
2. Capture the `g` telemetry line (see §4) and read back the
   `k_pos`, `k_vel`, `pos_leak` fields.
3. Classify the outcome:

   - **Derived path (expected).** `posgains_failure_reason_ == 0`. The gains
     are the pole-placement result. For the nominal bench-class chassis
     `phase_4m14_landed_2026-05-20.md` reports the derivation produces
     `K_POS ≈ 5.84`, `K_VEL ≈ 11.68`, `POS_LEAK ≈ 0.99975`. **These are quoted
     as confirmation targets, not thresholds** — the bot's actual chassis may
     differ; what matters is that the values are on the derived path and inside
     the §7.1 sanity envelope.
   - **Fallback path (degraded success).** `posgains_failure_reason_ == 9`
     (derived_gains_oob), or the encoder geometry was untrusted. The loop runs
     on the conservative 4M.13 `*_FALLBACK` constants `K_POS = 6.0`,
     `K_VEL = 3.0`, `POS_LEAK = 0.999`. BOOTSTRAP still succeeds and the bot
     still balances — but **this is a degraded session.** Record it as such in
     the logbook and treat any bench result as fallback-gain data, not
     derived-gain data. The corrective action is upstream (re-check encoder
     calibration §1.3) — not a gain edit.

Write the three derived values and the `posgains_failure_reason_` outcome into
the logbook **before** proceeding to the RUN procedure.

---

## 3. RUN procedure — step by step

1. **Pre-flight.** All of §1 checked. Bot in IDLE, on the level surface,
   operator on the `a` key.
2. **Start telemetry capture.** Begin logging the serial stream to a file (see
   §4). Note the wall-clock start time.
3. **BOOTSTRAP.** Trigger BOOTSTRAP — `b` (manual, skips CAPTURE_MOUNTING,
   uses the loaded mount offset) or the long-press / `c` chain. Watch for a
   clean finish with no `failure_reason`.
4. **Confirm gains (§2).** Read the `g`-line `k_pos`/`k_vel`/`pos_leak` and the
   `posgains_failure_reason_` outcome. Decide derived vs fallback. Log it.
5. **Enter RUN.** BOOTSTRAP auto-chains into RUN on success. `PositionLoop` is
   `reset()` on every RUN entry — integrator and slew memory start at zero, so
   the bot holds station from wherever it currently stands.
6. **Observe undisturbed station-keeping.** Let the bot balance untouched for a
   recorded interval. The outer loop should keep `position_m` and
   `wheel_vel_mps` small and bounded — the bot holds its spot rather than
   creeping across the floor. Watch the `nudge_deg` field: it should stay small
   and move slowly (it is slew-limited to ±`SLEW_DEG_S` = 2.0 °/s and clamped
   to ±`MAX_NUDGE_DEG` = 2.0°).
7. **Small manual disturbance (optional, gentle).** A light, single nudge of
   the chassis. The outer loop should lean the bot against the resulting drift
   and bring `position_m` back toward zero. Do **not** push hard — a
   quantified disturbance-rejection benchmark is Workstream K, not this
   procedure.
8. **HELD interaction check.** If the bot is picked up / collides and enters
   HELD, then is set back down level and quiet, it auto-resumes RUN — and
   `PositionLoop` is `reset()` again, so no drift carries across the HELD
   episode. Confirm this is what the telemetry shows.
9. **End the session.** Press `a` to abort, or otherwise return to IDLE. Stop
   the telemetry log. Note the wall-clock end time.
10. **Fill in the logbook** (`docs/guides/gain_logbook_template.md`) — this is
    not optional. A bench session with no logbook entry produced no result.

---

## 4. Telemetry capture mechanism — the `g` command

Workstream G adds a `g` serial telemetry command to the balance firmware. It
is the **single capture mechanism** for this procedure. (The pre-existing `s`
command emits a one-shot human-readable status line; `g` is the
machine-parsable cascade telemetry.)

`g` emits a **flat, single-line, comma-separated** record:

```
G,millis,pitch_deg,pitch_sp_deg,wheel_vel_mps,position_m,nudge_deg,k_pos,k_vel,pos_leak
```

| Field | Source | Meaning |
|---|---|---|
| `G` | literal | line tag — makes the stream `grep`-able |
| `millis` | `millis()` | firmware uptime, ms — the time base for all plots |
| `pitch_deg` | `BalanceApp::get_pitch_deg()` | measured pitch |
| `pitch_sp_deg` | inner-loop setpoint | pitch setpoint *after* the outer-loop nudge |
| `wheel_vel_mps` | mean of `enc_*.read_velocity_mps()` | the outer loop's input |
| `position_m` | `PositionLoop::position_m()` | leaky drift integral |
| `nudge_deg` | `PositionLoop::last_nudge_deg()` | outer-loop output (clamped + slew-limited) |
| `k_pos` | `PositionLoop::k_pos()` | the installed position gain |
| `k_vel` | `PositionLoop::k_vel()` | the installed velocity gain |
| `pos_leak` | `PositionLoop::pos_leak()` | the installed integrator leak |

**Design constraints to respect** (`ao_roadmap_post_4m14_2026-05-20.md` §4
Workstream G risks):

- `g` is a **flat line**, not a dashboard. Capture is "log the serial stream
  to a file, plot offline" — the plot script is host tooling and must never
  become a field dependency.
- The emit rate must not perturb the 5 ms PID tick or blow the serial budget —
  it is a low-rate periodic dump (or drain-on-demand), never a per-tick dump.
- `k_pos`/`k_vel`/`pos_leak` are echoed on every line so a single captured
  record self-documents which gains produced the run — no separate lookup.

**Capture recipe.** Redirect the serial monitor to a file for the whole
session, e.g. `pio device monitor -b 115200 | tee run_YYYY-MM-DD.log`, then
filter for the cascade lines with `grep '^G,' run_YYYY-MM-DD.log` before
plotting. Keep the raw log too — the non-`G` lines carry state-transition and
`failure_reason` context.

---

## 5. Acceptance bands — pass/fail

> **These bands are bench-measurements. They CANNOT be invented in advance.**
> The procedure for the *first* good run is: capture a clean derived-gain RUN
> session, observe the values below, and **establish the band from that run**.
> Subsequent sessions are judged against that established band. A
> "regression baseline" for a physical bot is a *band*, not a point
> (`ao_roadmap_post_4m14_2026-05-20.md` §4 — Workstream G risk #4).

Fill each band in from the first good run, then carry it forward:

| # | Band | What it checks | Pass criterion (FILL IN from first good run) |
|---|---|---|---|
| B-1 | `position_m` excursion (undisturbed) | station-keeping — does the bot hold its spot | \_\_\_\_ m peak over the observation window |
| B-2 | `wheel_vel_mps` excursion (undisturbed) | residual creep velocity | \_\_\_\_ m/s peak |
| B-3 | `nudge_deg` excursion | outer-loop authority used in normal balance | \_\_\_\_ ° peak (must stay well inside the ±2.0° clamp) |
| B-4 | `pitch_deg` vs `pitch_sp_deg` tracking error | inner loop tracks the nudged setpoint | \_\_\_\_ ° RMS |
| B-5 | `position_m` recovery after a gentle disturbance | does drift wash back toward zero | returns to within \_\_\_\_ m in \_\_\_\_ s |
| B-6 | `position_m` long-run drift | leaky integrator contains encoder bias over the session | no monotonic wind-up beyond \_\_\_\_ m over \_\_\_\_ min |
| B-7 | No `failure_reason` during RUN | session completes without a HELD-to-fault or abort | zero unexpected `failure_reason` lines |

Notes on individual bands:

- **B-3** has one hard, firmware-defined sub-criterion: `nudge_deg` is
  saturation-clamped to ±`POSLOOP_MAX_NUDGE_DEG` = 2.0° and rate-limited to
  ±`POSLOOP_SLEW_DEG_S` = 2.0 °/s. If `nudge_deg` is *sitting at* ±2.0° for
  sustained periods, the clamp is doing work it should not have to in
  undisturbed balance — flag it. The 2.0° figures are firmware constants, not
  invented thresholds.
- **B-6** exercises the known encoder-bias sensitivity
  (`workstream_f_review_2026-05-20.md` §4M.13-9 [P2-NUM-1]): with the derived
  `POS_LEAK ≈ 0.99975` washout, a small encoder bias is expected and
  acceptable; a *monotonic, non-washing* wind-up is not.
- A band failure is **not** an operator gain-edit. It is a result to record
  and escalate (firmware / derivation review, or an upstream calibration
  re-run per §1).

---

## 6. F-3 — the K_VEL bench-confirmation line item

**This is a named, mandatory check.** The operator must perform it and record
an explicit verdict in the logbook.

**Background.** `phase_4m14_landed_2026-05-20.md` records, under OQ-1, that the
4M.14 derivation with critical damping (`ζ_o = 1.0`) yields
`K_VEL ≈ 11.68` — roughly **3.9× the old Phase 4M.13 hand-picked value of
3.0**. This is *expected* (critical damping produces a larger velocity/damping
term than the 4M.13 author's deliberately conservative hand pick) and it
passes the §7.1 sanity clamp `[0.5, 15]`. It is **not** flagged as a fault.
But the landing report explicitly nominates it as a candidate for the bench
protocol to **confirm** — that is this line item, the
`workstream_f_review` F-3 K_VEL bench-observation item.

**What the operator must do (F-3):**

1. From the §2 / §4 `g`-line readback, confirm the installed `k_vel` is the
   derived value (on the nominal bench chassis, ≈ 11.68 — but use whatever the
   *this-bot* derivation produced; the point is it is the derived path, not
   the 3.0 fallback).
2. During the §3 RUN observation, judge whether the larger velocity term
   produces **acceptable damping behaviour**: the outer loop should look
   well-damped — `wheel_vel_mps` and `position_m` settle without sustained
   oscillation or buzz after a gentle disturbance. A `K_VEL` that is too high
   would show up as a jittery, over-damped-then-chattering `nudge_deg`; too low
   as overshoot/oscillation in `position_m`.
3. **Record an explicit K_VEL verdict** in the logbook: one of
   - *Confirmed* — the ~3.9× K_VEL produces good damping; the derivation is
     vindicated on hardware.
   - *Suspect* — the bot is visibly over- or under-damped; the derivation's
     `ζ_o`/bandwidth choice needs review (escalate — do not hand-edit).
   - *Inconclusive* — not enough clean data; re-run.

**This is the single most important thing the first bench session produces.**
Until F-3 is confirmed on hardware, the 4M.14 `K_VEL ≈ 11.68` is an
analytically-justified but bench-unconfirmed value.

---

## 7. Which results are bench-measurements (cannot be predicted)

Stated plainly so no future reader mistakes a placeholder for an omission:

**Cannot be predicted in advance — must come from the bench:**

- Every acceptance band B-1 … B-7 in §5. They are physical-bot measurements
  with real noise; the band is established from the first good run.
- The F-3 K_VEL verdict (§6) — whether the analytically-derived ≈ 11.68 damps
  well on *this* hardware.
- The actual per-bot derived `K_POS`/`K_VEL`/`POS_LEAK` values — they depend
  on the bot's measured encoder geometry and `K_motor`; the
  `phase_4m14_landed` values (5.84 / 11.68 / 0.99975) are the *nominal-chassis*
  derivation, quoted as a sanity reference only.
- Whether the derivation lands on the derived path or the fallback path for a
  given bot (§2).

**Known in advance (firmware constants / quoted derived values):**

- `MAX_NUDGE_DEG` = 2.0°, `SLEW_DEG_S` = 2.0 °/s — hardcoded safety
  saturations (`position_loop.h`).
- The §7.1 sanity envelopes: `K_POS ∈ [1.0, 30.0]`, `K_VEL ∈ [0.5, 15.0]`,
  `POS_LEAK ∈ [0.990, 0.9999]` (`balance_app.h`).
- The `*_FALLBACK` gains 6.0 / 3.0 / 0.999 (`position_loop.h`).
- The nominal-chassis derived values 5.84 / 11.68 / 0.99975 — confirmation
  targets, **not** acceptance thresholds.

---

## 8. References

- `docs/findings/workstream_f_review_2026-05-20.md` — §4M.13 cascade review,
  §4M.13-9 encoder-bias sensitivity, §4M.13-13 the retired sequencing flag,
  the F-3 K_VEL bench-observation item.
- `docs/findings/phase_4m14_landed_2026-05-20.md` — the auto-derivation as
  landed, the derived nominal values, OQ-1 (the K_VEL ≈ 3.9× note),
  `posgains_failure_reason_` semantics.
- `docs/findings/ao_roadmap_post_4m14_2026-05-20.md` — Workstream G intent,
  §7 decision #1 (bench-tuning policy: auto-derived is authoritative).
- `docs/guides/encoder_bench_bringup.md` — encoder wiring, CPR / radius
  calibration, distance verification (§1.3 prerequisites).
- `docs/guides/safe_bench_test_workflow.md` — the non-encoder bench-test
  procedure; required reading before this one.
- `docs/guides/gain_logbook_template.md` — the fill-in logbook this procedure
  feeds.
- `src/control/position_loop.{h,cpp}` — `PositionLoop` API, the gain members,
  `set_gains()`/`set_pos_leak()`, the saturation constants.
- `src/applications/balancing_robot/balance_app.{h,cpp}` — `derive_position_gains_()`,
  `posgains_failure_reason_`, BOOTSTRAP/RUN/HELD state machine.
- `src/main.cpp` — serial command loop (`c`/`t`/`a`/`s`/`b`/`k`/`e`/`p`;
  `g` added by Workstream G).

---

*Procedure complete. This is a verification protocol: the operator confirms,
captures, and records — the operator does not tune. The acceptance bands in §5
and the F-3 verdict in §6 are produced by the first bench session, not by this
document.*
