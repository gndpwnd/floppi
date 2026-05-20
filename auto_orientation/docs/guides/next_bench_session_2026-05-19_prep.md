# Next Bench Session — Operator Prep (2026-05-19 PM)

Operator-facing checklist for the next physical bench session, written
immediately after the 2026-05-19 PM multi-agent landing wave. Reads as a
companion to [safe_bench_test_workflow.md](safe_bench_test_workflow.md) and
[encoder_bench_bringup.md](encoder_bench_bringup.md) — those two are the
detailed procedures; this doc is the order of operations and the "what to
verify" overlay.

---

## 1. Quick status

As of 2026-05-19 PM the project has the **Mega-universal stack** substantially
extended (collision detection re-landed, wheel encoder driver + integration
landed, Phase 4M.12 PWM auto-discovery code landed, Phase 4.11a position
containment **designed**) and the **Uno-minimal program** materially safer
(P0 startup-delay + ATOMIC_BLOCK fixes, P1 top-5 fixes including new `g`/`p`
operator commands, brute-force tuner contract aligned with the consumer and
Kd accuracy fixed). Open: Phase 4.11a implementation, Phase 4M.12 follow-ups
(collision-aborts-PWMD, EEPROM slot doc/code mismatch, boot-time consumer
wiring), Uno P1 #6-15 + P2 batch, `mega_orientation` RAM diagnosis fixes.
**Ready to bench-test**: (a) Uno minimal with hardcoded reference gains, (b)
Mega universal with BOOTSTRAP + collision detection + encoder integration +
PWM auto-discovery.

All landings sit in working tree — no new commits this session. Last commit
on disk is c3c0c6b "save progress".

---

## 2. What is READY for bench (priority order)

### 2a. Uno minimal program (highest confidence)

- **Env**: `arduino_uno_minimal`
- **Flash**: `pio run -e arduino_uno_minimal -t upload`
- **Expected behaviour**: boots, prints banner + `READY`; waits ~1 s for
  BNO055 NDOF convergence (P0 startup-delay fix landed today); then enters
  the 200 Hz PID balance loop with **hardcoded** reference gains
  (Kp=65, Ki=12, Kd=38, PITCH_OFFSET=-8.6) consumed from
  `balance_constants.h`.
- **Success criteria**: bot held vertical within ±2° at release stays
  upright on the safe mat for ≥30 s. Any abort with `a` cleanly disarms;
  the new `g` command re-arms without reflash.
- **Landed this session**: P0 startup delay + ATOMIC_BLOCK around shared
  `last_pitch_deg_` / `last_pwm_` (the torn-float-read hazard called out
  in [audit_uno_minimal_2026-05-19.md §2](../findings/audit_uno_minimal_2026-05-19.md));
  P1 fixes including `g` arm-after-abort and `p` periodic-telemetry toggle.
- **Tests green**: 33/33 native pass.

### 2b. Mega universal program (more features, more unknowns)

- **Env**: `mega_balance`
- **Flash**: `pio run -e mega_balance -t upload`
- **Expected behaviour**: BOOTSTRAP fires ±PWM pulses to measure `K_motor`,
  derives Kp/Kd/Ki via pole-placement, then enters RUN. Collision detector
  (3-gate: 12 m/s² peak / 8 m/s² sustained 3 ticks / 6 m/s² + 200 dps) routes
  to HELD on impact. Encoders sample left/right wheels (pins 18/19, 2/3) and
  the stall detector routes to HELD with `failure_reason=7` if PWM > stiction
  produces zero velocity. The `p` serial command kicks off the new PWM
  auto-discovery state.
- **Success criteria**: clean BOOTSTRAP (4× `ok=1`, `|g0|<5`, `thr<3`); RUN
  stays balanced ≥10 s on flat mat; collision detector visibly fires HELD on
  a deliberate push; encoders track wheel motion when bot is hand-pushed.
- **Landed this session**: collision detection re-implementation
  (27/27 tests pass); wheel encoder driver `src/sensors/wheel_encoder.{h,cpp}`
  (17/17 tests pass); `balance_app` integration (25 new tests); Phase 4M.12
  PWM-discovery state + `p` command + EEPROM persistence (10 new tests pass
  per [phase_4m12_landed_2026-05-19.md](../findings/phase_4m12_landed_2026-05-19.md)).

---

## 3. Bench test order

Run in this order. Do not skip ahead — early steps catch wiring/firmware
faults that get expensive once the bot is balancing.

1. **Uno minimal, bot lifted** — flash `arduino_uno_minimal`, lift bot off
   ground (cradle in books/cardboard), power on. Verify boot banner →
   `READY` → motors are quiet. Send `s` repeatedly; confirm pitch ≈ 0 when
   chassis is vertical. Send `p` to toggle the new 10 Hz periodic telemetry
   and confirm it streams. Send `a` to disarm; send `g` to re-arm. **Do not
   put on floor yet.**
2. **Uno minimal, collision/push check** — still lifted, hand-tilt the bot
   to ±10° and verify the motors respond in the correct direction (forward
   tilt → wheels forward, etc.). If the direction is wrong, swap motor leads
   at the L298N (do NOT invert in software). Tilt past 25° and verify
   `tipped_` trips and motors stop.
3. **Mega universal, encoder direction check** — flash `mega_balance`,
   bot lifted, follow [encoder_bench_bringup.md §3](encoder_bench_bringup.md#section-3--first-power-verification-before-letting-the-bot-move).
   Hand-rotate each wheel forward and verify ticks increase. (Note: the
   operator-readout commands are still queued — at present, the only way to
   see ticks is via the per-tick telemetry path emitted from `step_run_`.)
4. **Mega universal, PWM auto-discovery (`p`)** — still lifted, wheels free
   per [phase_4m12_landed_2026-05-19.md §6](../findings/phase_4m12_landed_2026-05-19.md#6--operator-usage-instructions).
   Send `p`. Watch `pd#` per-step telemetry. Success looks like
   `sv pd min=<X> max=<Y>` printed when the state returns to IDLE; the
   value gets saved to EEPROM (`EE_PWMDISC_ADDR`). Plausible MIN ≈ 30-80,
   MAX ≈ 240-255. Expect ~4-8 s wall time.
5. **Uno minimal, full balance attempt** — bot on the safe mat, vertical
   within ±2°, foam barriers per [safe_bench_test_workflow §1](safe_bench_test_workflow.md#section-1--test-area-setup),
   serial monitor open. Release. Pass = ≥30 s balance.
6. **Mega universal, full balance with all features** — bot on safe mat,
   release; observe BOOTSTRAP → CHARACTERISE → RUN transition through the
   state log. Deliberately push the bot mid-RUN; verify HELD fires from the
   collision detector (`failure_reason=5`) and that RUN auto-recovers once
   the bot is set vertical and quiet per the resume gate.

---

## 4. Known fragile / risky

- **Uno P0 fixes landed AFTER the last bench crash.** The audit pinned the
  most-likely root cause (torn float read + no startup delay) but it was not
  confirmed live. Bench-validate before assuming the bug is gone.
- **Phase 4M.12 has 3 documented gaps** (see
  [phase_4m12_landed_2026-05-19.md §4](../findings/phase_4m12_landed_2026-05-19.md#4--whats-missing-or-incomplete-from-the-spec)):
  collision detector does NOT abort PWMD (bot ignores hits during the sweep);
  EEPROM slot address differs from the docs (code uses `EE_PWMDISC_ADDR`,
  plan says 0x210, header comment says 0x230); load-back at boot is
  unwired — the discovered MIN is saved to EEPROM but does not currently
  feed `L298NMotorDriver::stiction_min_pwm`. Treat the PWMD result as
  informational this session, not as automatic configuration.
- **Brute-force tuner Kd polished tonight.** Reference preset now lands at
  Kd ≈ 31-63 (was ~16, off 2.4× from reference 38). Stress preset still
  under-tunes; reference preset over-shoots a touch. See
  [tuner_kd_accuracy_2026-05-19.md](../findings/tuner_kd_accuracy_2026-05-19.md).
  Expect bench polish required after a tuner run.
- **Sub-agent Edit permissions were blocked this session** for multiple
  agents (the Phase 4.11a implementation, Uno P1 #6-15, scope retag, INDEX
  cross-link, VISION banner agents all hit `Permission to use Edit has been
  denied`). Future code-change tasks may need to be applied by the
  operator manually or by switching the harness permission setting.
- **`test_held_state_machine` 3/6 fails** — pre-existing red bar, not a
  regression, but worth noting if the operator notices flaky HELD recovery
  on the Mega.

---

## 5. Brute-force tuner workflow

```bash
cd /home/devel/floppi/auto_orientation/tools/sim
python3 brute_tune.py --mode random --budget 1000 --seed 42 \
    --plant reference \
    --output ../../src/applications/balancing_robot_uno/balance_constants.h
cd ../..
pio run -e arduino_uno_minimal -t upload
```

Then bench-test per §3 step 5 above. If the bot is unstable:

- Re-run with `--mode evolutionary --budget 500` for a smoother search,
  or `--mode grid --budget 2000` for the systematic pass.
- Tweak the trial defaults (now `init=8°, dur=8 s, disturbance=500 deg/s²`
  per the Kd-accuracy fix) via `--init-perturbation`, `--duration`,
  `--disturbance` if the bot characteristics differ from the reference plant.
- Same `--seed` always produces the same gains (bit-identical except the
  embedded timestamp) — reproducibility is verified.

---

## 6. Failure recovery

- **Bot falls during balance** — motors auto-disarm via `tipped_` (Uno) or
  HELD (Mega). Catch the bot if you can; let it fall onto mat/foam if not.
  Pick up, set vertical on mat, wait for resume gate (Mega) or send `g` to
  re-arm (Uno).
- **Bot wedged against wall / cable** — emergency stop is `a` over serial
  on either env. On Mega the collision detector will usually trigger HELD
  first; on Uno you must hit `a` yourself. After `a` on Uno, the bot stays
  disarmed until you send `g`.
- **Bot won't BOOTSTRAP (Mega)** — check encoder wiring against
  [encoder_bench_bringup §2](encoder_bench_bringup.md#section-2--wiring-procedure);
  read the `pd fail r=<N>` / `bs fail r=<N>` codes (8 = timeout, 4 = abort,
  5 = collision, 7 = motor stall). Re-run with `b` (BOOTSTRAP only) or `t`
  (abort then BOOTSTRAP).
- **More than 3 collision aborts in a row** — the test area is wrong, not
  the bot. Widen barriers, re-center on access corner.
- **More than 5 BOOTSTRAP failures in a session** — power-cycle the bot.
  BNO055 fusion engine occasionally wedges; a reset is faster to try than
  to diagnose.

---

## 7. What to record from each test

For each test in §3, capture and stash under
`docs/archive/session_records/2026-05-19_<short_label>.md`:

- **Serial telemetry log** — paste the full session capture (banner →
  state transitions → pulse logs → any `failed`/`HELD`/`tipped` lines).
- **Subjective observation** — did it oscillate? At what frequency
  (slow rocking vs fast buzz)? For how long? How far did it drift
  before falling? Which direction?
- **PulseLog quality (Mega BOOTSTRAP)** — `|g0|<5`, `thr<3`, `ok=1` for
  all four pulses? K_mean and per-pulse spread?
- **PWM auto-discovery result (Mega)** — `min=`, `max=` printed values,
  + plausibility check (~30-80 / 240-255).
- **Any new failure modes** with full reproduction steps so the next
  coding session can replay them.

The format is freeform — `safe_bench_test_workflow.md §3` already shows
the per-line interpretation of the PulseLog; quote whichever telemetry
matters and annotate.

---

## 8. Open blockers + next coding-session tasks

Drained from [todo.md](../todo.md) and the Phase 4M plan:

- **Apply blocked sub-agent plans** — Phase 4.11a-1 odometry implementation
  (design complete in
  [phase_4_11a_design_2026-05-19.md](../findings/phase_4_11a_design_2026-05-19.md)),
  Uno P1 #6-15 fixes (from
  [audit_uno_minimal_2026-05-19.md §11](../findings/audit_uno_minimal_2026-05-19.md#11--prioritized-punch-list--top-5-fixes-for-next-coding-session)
  forward), scope retag with `[mega]`/`[uno]` markers, INDEX cross-link
  updates, VISION-doc banner pointing at the strategic pivot.
- **Phase 4M.12 follow-ups** — collision-aborts-PWMD wiring + test;
  EEPROM slot reconciliation between code/plan/header comment; boot-time
  load-back consumer wiring so the discovered MIN actually feeds
  `stiction_min_pwm`.
- **Phase 4.11a-2 outer-loop cascade** — only after 4.11a-1 odometry lands.
  Highest-impact remaining Mega-path behavioural deficit (this is the
  "bot stops wandering" payoff).
- **`mega_orientation` RAM-overflow Phase A fixes** per
  [mega_orientation_ram_overflow_diagnosis_2026-05-19.md](../findings/mega_orientation_ram_overflow_diagnosis_2026-05-19.md).
- **Encoder operator commands** (`e`, `E`, `d`, raw-PWM) per
  [encoder_bench_bringup §11](encoder_bench_bringup.md#section-11--open-work-whats-missing).
  Without these, Sections 3-7 of that guide cannot be exercised
  end-to-end.
- **Brute-tuner stress-preset Kd** — still under-tunes; tune the preset's
  mechanical damping per
  [tuner_kd_accuracy_2026-05-19.md §7](../findings/tuner_kd_accuracy_2026-05-19.md#7-if-kd-is-still-off-after-bench-testing).

---

## See also

- [safe_bench_test_workflow.md](safe_bench_test_workflow.md) — detailed
  bench safety procedure (Sections 2-4 are mandatory before any
  power-on).
- [encoder_bench_bringup.md](encoder_bench_bringup.md) — encoder-specific
  wiring, calibration, stall test.
- [../MEGA_UNIVERSAL_PLAN.md](../MEGA_UNIVERSAL_PLAN.md) — what landed this
  session and what is queued on the Mega path.
- [../todo.md](../todo.md) — full task list with phase tags.
- [../findings/phase_4m12_landed_2026-05-19.md](../findings/phase_4m12_landed_2026-05-19.md) —
  Phase 4M.12 PWM auto-discovery as-built + gaps.
- [../findings/audit_uno_minimal_2026-05-19.md](../findings/audit_uno_minimal_2026-05-19.md) —
  Uno audit; the 5 fixes that landed + the 25 that did not.
- [../findings/tuner_kd_accuracy_2026-05-19.md](../findings/tuner_kd_accuracy_2026-05-19.md) —
  brute-tuner Kd diagnosis + fix verification.
- [../findings/verification_2026-05-19.md](../findings/verification_2026-05-19.md) —
  build/test state immediately before the wave that fixed it.
