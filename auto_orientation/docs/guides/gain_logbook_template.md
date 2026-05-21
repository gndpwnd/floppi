# AO Mega Balance Bot — Outer-Loop Gain Logbook (Template)

A reusable, copy-paste-ready record for a single bench session of the
auto-derived 4M.13/4M.14 cascade. **Copy this file** (do not edit the
template), rename the copy to `gain_log_YYYY-MM-DD_<n>.md`, and fill in every
blank.

Companion procedure: `docs/findings/workstream_g_bench_protocol_2026-05-21.md`.
Run that procedure; record the result here. A bench session with no logbook
entry produced no result.

> This logbook records a **verification** session, not a tuning session. The
> operator does not hand-edit `K_POS`/`K_VEL`/`POS_LEAK` — those are
> auto-derived at BOOTSTRAP. The operator confirms, observes, and judges.

---

## 1. Session header

| Field | Value |
|---|---|
| Session ID (`gain_log_YYYY-MM-DD_<n>`) | \_\_\_\_\_\_\_\_\_\_ |
| Date | \_\_\_\_\_\_\_\_\_\_ |
| Operator | \_\_\_\_\_\_\_\_\_\_ |
| Firmware commit hash | \_\_\_\_\_\_\_\_\_\_ |
| Build env | `mega_balance` |
| Bot / chassis ID | \_\_\_\_\_\_\_\_\_\_ |
| Telemetry log file | \_\_\_\_\_\_\_\_\_\_ |
| Wall-clock start / end | \_\_\_\_\_\_ / \_\_\_\_\_\_ |

---

## 2. Pre-bench checklist (§1 of the procedure)

| Check | Pass? | Note |
|---|---|---|
| Firmware is `mega_balance`, commit recorded | ☐ | |
| BNO055 IMU calibration fresh (slot 0x000) | ☐ | |
| Mounting offset current (slot 0x200) | ☐ | |
| Encoder calibration present + valid (slot 0x220) | ☐ | |
| Encoder direction check passed (forward → ticks up) | ☐ | |
| Encoder distance verification within ±5% | ☐ | |
| PWM-discovery run done, saved (slot 0x230) | ☐ | |
| Clean BOOTSTRAP, no `failure_reason` | ☐ | |
| `failure_reason=7` (k_disagreement) did NOT fire | ☐ | |
| Level surface, `a`-abort within reach | ☐ | |

---

## 3. Derived gains (read from the `g` telemetry line, §2 of the procedure)

| Gain | Derived value (this session) | Nominal-chassis reference | In §7.1 envelope? |
|---|---|---|---|
| `K_POS`   | \_\_\_\_\_\_ | ≈ 5.84    | envelope `[1.0, 30.0]`   ☐ |
| `K_VEL`   | \_\_\_\_\_\_ | ≈ 11.68   | envelope `[0.5, 15.0]`   ☐ |
| `POS_LEAK`| \_\_\_\_\_\_ | ≈ 0.99975 | envelope `[0.990, 0.9999]` ☐ |

| Field | Value |
|---|---|
| `posgains_failure_reason_` | \_\_\_\_ (0 = derived OK; 9 = derived_gains_oob) |
| Derivation path | ☐ DERIVED (authoritative)  ☐ FALLBACK 6.0 / 3.0 / 0.999 (degraded) |

> The nominal-chassis reference values are quoted from
> `phase_4m14_landed_2026-05-20.md` for sanity comparison only — they are NOT
> pass/fail thresholds. A different chassis legitimately derives different
> values.

If the path is **FALLBACK**, this is a degraded session: all data below is
fallback-gain data. Note the suspected cause (re-check encoder calibration §1.3
of the procedure):

```
Fallback cause: ____________________________________________
```

---

## 4. Observed behaviour (§3 of the procedure)

Free-text — describe what the bot did. Undisturbed station-keeping, response
to a gentle nudge, any HELD episodes and auto-resume, anything unusual.

```
________________________________________________________________
________________________________________________________________
________________________________________________________________
________________________________________________________________
```

---

## 5. Acceptance bands — pass/fail (§5 of the procedure)

Fill the **band** column from the first good run; on later sessions, fill the
**observed** column and judge against the established band. Bands are
bench-measurements — there is no correct number to invent here.

| # | Band | Established band | Observed this session | Pass? |
|---|---|---|---|---|
| B-1 | `position_m` excursion, undisturbed (m) | \_\_\_\_ | \_\_\_\_ | ☐ |
| B-2 | `wheel_vel_mps` excursion, undisturbed (m/s) | \_\_\_\_ | \_\_\_\_ | ☐ |
| B-3 | `nudge_deg` excursion (°) — must stay inside ±2.0° clamp | \_\_\_\_ | \_\_\_\_ | ☐ |
| B-4 | `pitch_deg` vs `pitch_sp_deg` RMS tracking error (°) | \_\_\_\_ | \_\_\_\_ | ☐ |
| B-5 | `position_m` recovery after gentle disturbance (m within / s) | \_\_\_\_ | \_\_\_\_ | ☐ |
| B-6 | `position_m` long-run drift — no monotonic wind-up (m / min) | \_\_\_\_ | \_\_\_\_ | ☐ |
| B-7 | No unexpected `failure_reason` during RUN | zero | \_\_\_\_ | ☐ |

Band notes / anomalies:

```
________________________________________________________________
________________________________________________________________
```

---

## 6. F-3 — K_VEL bench-confirmation verdict (§6 of the procedure)

**Mandatory.** The 4M.14 derivation produces `K_VEL ≈ 11.68` on the nominal
chassis — roughly 3.9× the old 4M.13 hand-picked 3.0. This is expected
(critical damping), but the bench must confirm it damps well on hardware.

| Field | Value |
|---|---|
| Installed `K_VEL` (from §3) | \_\_\_\_\_\_ |
| Damping behaviour observed | ☐ well-damped  ☐ over-damped / chattering  ☐ under-damped / oscillating |

**K_VEL verdict** (tick exactly one):

- ☐ **Confirmed** — the ~3.9× K_VEL produces good damping; 4M.14 derivation
  vindicated on hardware.
- ☐ **Suspect** — visibly over- or under-damped; derivation `ζ_o`/bandwidth
  choice needs review. **Escalate — do not hand-edit the gain.**
- ☐ **Inconclusive** — insufficient clean data; re-run.

Evidence / reasoning:

```
________________________________________________________________
________________________________________________________________
```

---

## 7. Session verdict

| Field | Value |
|---|---|
| Bands passed | \_\_\_ of 7 |
| Derivation path | ☐ derived  ☐ fallback |
| F-3 K_VEL verdict | ☐ confirmed  ☐ suspect  ☐ inconclusive |
| Overall | ☐ PASS — cascade verified on hardware  ☐ PARTIAL  ☐ FAIL |

Follow-up actions / escalations (a failed band is a firmware/derivation issue
or an upstream calibration re-run — never an operator gain-edit):

```
________________________________________________________________
________________________________________________________________
________________________________________________________________
```

---

## 8. Notes & free-text

Anything else worth recording for the next session — bench setup quirks,
surface condition, battery state, telemetry oddities, ideas to check next time.

```
________________________________________________________________
________________________________________________________________
________________________________________________________________
________________________________________________________________
```

---

*Logbook entry complete. File alongside the telemetry log. The first
fully-passing derived-path session establishes the §5 bands for all later
sessions.*
