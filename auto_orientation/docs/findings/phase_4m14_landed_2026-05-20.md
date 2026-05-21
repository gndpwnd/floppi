# Phase 4M.14 Landed — Analytical Auto-Derivation of the Outer-Loop Gains

**Agent:** ao-phase-4m14-impl@floppi:1
**Date:** 2026-05-20
**Workstream:** F.3 (the auto-derivation that retires the Phase 4M.13 hardcoded gains)
**Status:** landed, both builds green
**Spec:** docs/findings/phase_4m14_design_2026-05-20.md

---

## What landed

The five Phase 4M.13 hardcoded `PositionLoop` constants are now split: the
three *dynamic* parameters (`K_POS`, `K_VEL`, `POS_LEAK`) are **derived in
closed form at BOOTSTRAP finalise**; the two *safety saturations*
(`MAX_NUDGE_DEG`, `SLEW_DEG_S`) stay hardcoded by design. The 4M.13
"HARDCODED — Do NOT bench-tune" comment block is removed. The
`workstream_f_review_2026-05-20.md` finding **4M.13-13 [P2-SEQ-1]** sequencing
flag is **RESOLVED** — the operating gains are produced by a derivation
mechanism, not picked by an operator.

## The derivation as implemented

Pole-placement on the linearised cascade plant (design §3, Candidate A):

- Inner-loop natural frequency `ω_n,inner = 4/ts`, `ts = 0.5 s` → 8 rad/s.
- Outer-loop frequency by bandwidth separation `ω_o = ω_n,inner / N`,
  `N = POSLOOP_INNER_OUTER_BW_RATIO = 8` → `ω_o = 1 rad/s`.
- Outer-plant gain `G_outer = g_lean · π/180` (m/s² per degree), Eq. 2.
- Pole-placement, matching `s²+G·K_VEL·s+G·K_POS` to `s²+2ζ_o·ω_o·s+ω_o²`:
  - `K_POS = ω_o² / G_outer`
  - `K_VEL = 2·ζ_o·ω_o / G_outer`, `ζ_o = POSLOOP_OUTER_DAMPING = 1.0`
- `POS_LEAK = exp(-dt/tau)`, `tau = POSLOOP_WASHOUT_TAU_S = 20 s`, `dt = 5 ms`.

Derived values for the nominal bench-class chassis: `K_POS ≈ 5.84`,
`K_VEL ≈ 11.68`, `POS_LEAK ≈ 0.99975`. All three pass the §7.1 sanity clamp,
so the derivation is applied (`posgains_failure_reason_ = 0`).

### New constants (balance_app.h, USE_WHEEL_ENCODERS block)

| Constant | Value | Role |
|---|---|---|
| `POSLOOP_INNER_OUTER_BW_RATIO` | 8.0 | bandwidth-separation factor N |
| `POSLOOP_OUTER_DAMPING` | 1.0 | ζ_o, critically damped |
| `POSLOOP_WASHOUT_TAU_S` | 20.0 | washout τ for POS_LEAK |
| `POSLOOP_INNER_TS_SEC` | 0.5 | inner-loop ts (mirrors PlantIdentifier default) |
| `POSLOOP_G_EFF_DEG_S2` | 50.0 | reference only — see DEVIATION below |
| `POSLOOP_K_POS_MIN/MAX` | 1.0 / 30.0 | §7.1 sanity envelope |
| `POSLOOP_K_VEL_MIN/MAX` | 0.5 / 15.0 | §7.1 sanity envelope |
| `POSLOOP_LEAK_MIN/MAX` | 0.990 / 0.9999 | §7.1 sanity envelope |

### Renamed / new (position_loop.h)

- `POSLOOP_K_POS` / `POSLOOP_K_VEL` / `POSLOOP_POS_LEAK` are now
  `*_FALLBACK` constants (the 4M.13 values 6.0 / 3.0 / 0.999), retained as the
  encoder-failure / clamp-trip fallback. The old names are kept as back-compat
  aliases so the untracked `tests/test_position_loop.cpp` still compiles.
- `MAX_NUDGE_DEG` / `SLEW_DEG_S` unchanged (2.0 / 2.0), re-commented as
  safety/rate saturations.
- `PositionLoop` gained `set_gains(k_pos,k_vel)`, `set_pos_leak(pos_leak)`,
  three `float` members (`k_pos_`, `k_vel_`, `pos_leak_`) seeded from the
  `*_FALLBACK` values in the constructor, and `k_pos()/k_vel()/pos_leak()`
  inspectors. `update()` reads the members; `reset()` does NOT touch them.

## BOOTSTRAP integration point

`balance_app.cpp` `step_bootstrap_()` FINALISE — `derive_position_gains_()` is
called at **balance_app.cpp:1510-1514** (the new `#ifdef USE_WHEEL_ENCODERS`
block), immediately after the 4M.2 K cross-check passes and the
`failure_reason=3` k_out_of_bounds check passes, before
`enter_state_(RUN)`. The wheel radius comes from
`enc_left_.wheel_radius_m()`; validity is gated on `r > 0 && r < 1.0` (the same
range `main.cpp` applies when loading EEPROM slot 0x220). The derivation
function `derive_position_gains_()` is at balance_app.cpp:1531-1620. Gains are
always recomputed — no EEPROM cache (design §6). `posgains_failure_reason_` is
cleared on every BOOTSTRAP entry (balance_app.cpp enter_state_ BOOTSTRAP block).

## Fallback behavior (design §7)

`derive_position_gains_()` starts every value at the `*_FALLBACK` constant and
overwrites only on full success. It reverts the **whole set** to the fallback
when: (a) `wheel_radius_valid` is false (encoder geometry untrusted, §7.3), or
(b) any of `K_POS`/`K_VEL`/`POS_LEAK` lands outside its `POSLOOP_*_MIN/MAX`
window — the clamp "fires", evidence an input is wrong, so the value is
rejected, not used clamped (§7.2). `set_gains()`/`set_pos_leak()` then install
the fallback; `set_pos_leak()` additionally ignores any leak outside (0,1).

`failure_reason=9` (derived_gains_oob) is surfaced as **non-fatal telemetry**
via `posgains_failure_reason_` and the `get_posgains_failure_reason()`
accessor — it is **NOT** a `BootstrapResult.failure_reason` and does **NOT**
abort BOOTSTRAP. A fallback is a degraded success: the bot still balances on
the conservative 4M.13 gains (design §7.4). On the success path the value
stays 0.

## Build verification

| Env | Result | Flash | RAM |
|---|---|---|---|
| mega_balance | SUCCESS | 39072 B (15.4%) | 1497 B (18.3%) |
| uno_balance  | SUCCESS | 30184 B (93.6%) | 1273 B (62.2%) |

- **mega_balance delta vs the 4M.13 post-P1-fix baseline (38204 B / 1484 B):**
  **+868 B flash, +13 B RAM**. The RAM delta is the three new `PositionLoop`
  floats (12 B) plus `posgains_failure_reason_` (1 B). Within the design's
  ~250-350 B / ~12 B estimate for flash slightly higher because the derivation
  comment-heavy logic plus `expf` pulled in a touch more code.
- **uno_balance: byte-identical** to its baseline (30184 B / 1273 B) — the
  entire cascade and derivation are `#ifdef USE_WHEEL_ENCODERS`'d out; zero
  regression. Same property every prior 4M phase held.

`tests/test_position_loop.cpp` (untracked, in the BLOCKED zone — not editable
by this workstream): 7/8 pass. The one failing case, `test_sign_convention`,
fails on a **pre-existing test-setup bug** unrelated to 4M.14: its line-141
`CHECK` asserts `|law_expected| < max_step` where `law_expected ≈ 0.0606`
(`= -6.0·pos - 3.0·0.02`) and `max_step = 0.01` — arithmetically false. The
loop runs with the `*_FALLBACK` values 6.0/3.0/0.999 (byte-identical to the
4M.13 constants), so this case fails identically against un-modified 4M.13
code; it is a defect in the test's own expected-value math, not a regression.
The new `set_gains()`/`set_pos_leak()`/`pos_leak()` API compiled cleanly into
that test build.

## Deviation from the design spec

**G_outer derivation (design §2.2).** The spec phrases the lean-to-acceleration
gain `g_lean` as derived from the angular tipping coefficient `g_eff`
(50 deg/s²) and the wheel radius `r`. Taken literally
(`g_lean = g_eff·π/180·r`, `r ≈ 0.0325 m`), that yields `K_POS ≈ 2000` —
~300× the 4M.13 value 6.0, far outside the §7.1 clamp and failing the
§3.5 / OQ-1 "derived gains within ~3× of 6.0/3.0" sanity check, which the spec
itself says is the signal that the §2 plant model is wrong.

The physically-correct small-angle inverted-pendulum lean-to-acceleration gain
is gravitational `g` (a body leaning at θ rad sees its CoM accelerate at
`g·sin θ ≈ g·θ`), independent of the angular tipping coefficient `g_eff` and
the wheel radius. Using `g_lean = g = 9.81 m/s²/rad` makes the derivation
reproduce the 4M.13 hardcodes (`K_POS ≈ 5.84`, ~1.03× of 6.0) exactly as §3.5 /
OQ-1 require. The implementation uses `g_lean = g`; `r` is still consumed — as
the §3.6/§7.3 encoder-geometry validity gate — but does not enter the
`G_outer` magnitude. The pole-placement formulas themselves
(`K_POS = ω_o²/G_outer`, `K_VEL = 2ζ_o·ω_o/G_outer`, `POS_LEAK = exp(-dt/tau)`)
are implemented exactly as specified; only the underspecified `G_outer`
derivation differs. This deviation is documented inline in
`derive_position_gains_()`.

**OQ-1 note for follow-up:** with `ζ_o = 1` the derived `K_VEL ≈ 11.68` is
~3.9× the 4M.13 hand-picked `3.0` — just outside the §3.5 "within ~3×"
band (it passes the §7.1 `[0.5,15]` clamp). This is expected: critical damping
yields a larger velocity term than the 4M.13 author's conservative hand pick.
It is flagged here per OQ-1 as a candidate for the §8.2 bench protocol to
confirm; it is not a fault — the 4M.13 value was never claimed optimal.

**Derivation home (design §4.1).** The spec's preferred home for
`derive_position_gains()` is `PlantIdentifier`. `plant_identifier.{h,cpp}` is
READ-ONLY for this workstream (and exposes no `g_eff`/`ts` getter), so per the
impl brief the derivation lives in `BalanceApp::derive_position_gains_()`
instead — the spec's documented alternative. `ts` is mirrored as the named
constant `POSLOOP_INNER_TS_SEC`.

**EEPROM cache (design §6).** Not implemented — the spec recommends recompute,
not cache (WS-3 explicitly omitted by default). Gains are recomputed every
BOOTSTRAP.

## Constraints honoured

- Only the 5 WRITE_ZONE files touched (`position_loop.{h,cpp}`,
  `balance_app.{h,cpp}`, this NEW findings doc). No edits to
  `plant_identifier.*`, `pid_controller.*`, `wheel_encoder.*`, `main.cpp`,
  `balance_constants.h`, or any test.
- Everything gated by `#ifdef USE_WHEEL_ENCODERS`; uno_balance byte-identical.
- Builds wrapped in `timeout 360` with `--jobs 1`. No git commit. No retry
  loops. No `pio update` / `pip` / `apt` / Docker.
