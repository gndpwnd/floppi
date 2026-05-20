# Architecture Plan — Auto-Orientation, 2026-05-20

**Author:** architect@floppi:1 (Plan-type sub-agent)
**Saved by:** orchestrator (Plan-type agents are harness-read-only; this file is the orchestrator's transcription of the agent's output)
**Status:** workstream-ready

---

## Reading order

This plan synthesizes five sibling audits delivered the same day:

- `audit_documentation_2026-05-20.md` (41 findings)
- `audit_code_quality_2026-05-20.md` (19 findings, 1 P0: collision regression)
- `audit_test_coverage_2026-05-20.md` (26 findings)
- `audit_security_2026-05-20.md` (32 findings, 0 P0)
- `audit_build_system_2026-05-20.md` (30 findings, 4 P0)

Plus 2026-05-19 findings that informed dependency ordering:

- `research_collision_signature_bno055.md` — the 3-gate detector spec
- `research_wheel_encoders_mega_2026-05-19.md` (RWE) — encoder pin alloc + cascade design
- `audit_code_quality_balance_stack_2026-05-19.md` (CQA) — bundled-with-collision audit punch list
- `investigation_held_state_machine_failure_2026-05-19.md` — the stale-test-binary root cause that motivates INFRA-A

---

## 1. Phase 4M.0 — Collision-detection restoration (top priority)

**Goal:** restore the three-gate collision detector reverted late 2026-05-19, with the exact signature `tests/test_balance_app_collision.cpp` (482 LOC, untracked-but-on-disk) and surviving scaffolding (`sensor_base.h:125`, `bno055.{h,cpp}:206`) already expect. **The test file is the contract. Do not invent a new design.**

### 1.1 Public API contract (matches test file verbatim)

- `bool collision_detected() const`
- `void clear_collision()`
- `BootstrapResult::failure_reason == 5` on collision during BOOTSTRAP
- RUN → HELD on collision (test 11); existing lenient HELD→RUN auto-resume preserved
- BOOTSTRAP / CHAR_ACT → IDLE on collision (tests 8, 9, 10)

### 1.2 Thresholds (anonymous namespace in `balance_app.cpp`, per CQA §6 ~40B win)

```cpp
constexpr float   COLLISION_PEAK_MPS2     = 12.0f;
constexpr float   COLLISION_SUSTAIN_MPS2  = 8.0f;
constexpr uint8_t COLLISION_SUSTAIN_TICKS = 3;
constexpr float   COLLISION_KICK_MPS2     = 6.0f;
constexpr float   COLLISION_KICK_GYRO_DPS = 200.0f;
```

### 1.3 Detector state (new volatile members)

```cpp
volatile float   linear_accel_mag_;
volatile uint8_t collision_consec_;
volatile bool    collision_latched_;
```

### 1.4 Compute site

Inside the existing `ATOMIC_BLOCK` in `read_imu_` (`balance_app.cpp:1416-1445`). Pre-filter: `isnan` + `mag_sq > 1600.0f` clamp (40² m²/s⁴). Compute |a_lin|, update consec counter, OR-fire three gates.

### 1.5 State-handler integration (3 sites)

- **BOOTSTRAP** (`step_bootstrap_`, ~line 982): on latch → set `failure_reason=5`, `converged=false`, `motors_.stop()`, `enter_state_(IDLE)`, return.
- **CHAR_ACT** (`step_char_act_`, ~line 1267): on latch → `motors_.stop()`, `enter_state_(IDLE)`, return.
- **RUN** (`step_run_`, ~line 408): on latch → `enter_state_(HELD)`. MUST PRECEDE the existing `safety_.abort_requested()` check at line 435.

### 1.6 Latch lifecycle

Clear under ATOMIC_BLOCK in `enter_state_(IDLE)`, `enter_state_(BOOTSTRAP)`, `enter_state_(CHAR_ACT)`. **Do not clear on HELD entry** — the latch is what got us there; operator must call `clear_collision()` explicitly (or the HELD→RUN auto-resume path).

### 1.7 No compile gate

Safety surface, not behaviour. Uno-minimal path uses a different source tree (`balancing_robot_uno/`) — zero flash impact there.

### 1.8 Bundle with these audit fixes (one PR)

Per CQA §9 punch list, items whose code paths interleave with collision restoration:

- **P0-ISR-1**: `pitch_deg_` torn read — ATOMIC_BLOCK in read_imu_ already lands here
- **P0-ISR-2**: `raw_gyro_dps_[3]` torn read — same ATOMIC_BLOCK
- **P1-NUM-1**: promote `ch_gyro_acc_x10_` to `uint32_t` (`balance_app.h:471`)
- **P1-NUM-2**: clamp `P_` to [1e-6, 1e6] in `plant_identifier.cpp:226`
- **P2-DOC-3**: `sensor_base.h:118-122` and `bno055.h:142` `COLLISION_SPIKE_MPS2` → `COLLISION_PEAK_MPS2`

### 1.9 Acceptance for 4M.0

1. `tests/test_balance_app_collision` binary prints `Tests run: 27, passed: 27`
2. `pio run -e mega_balance` succeeds
3. `pio run -e uno_balance` succeeds (must not regress)
4. `test_balance_app`, `test_balance_app_bootstrap`, `test_balance_app_soft_cutoff`, `test_held_state_machine` all still pass
5. Bench-validate (deferred if no hardware access this session): bot enters BOOTSTRAP, intentionally bumped sideways at 12+ m/s², exits to IDLE with failure_reason=5

---

## 2. Workstream partition (THE conflict-prevention table)

Convention: **OWNS** = exclusive write. **READS** = read-only. Other workstreams must not write to OWNS.

### Workstream A — Phase 4M.0 collision restore + bundled audit fixes

- **OWNS:**
  - `src/applications/balancing_robot/balance_app.h`
  - `src/applications/balancing_robot/balance_app.cpp`
  - `src/control/plant_identifier.cpp` (P_ clamp at line 226 only)
  - `src/sensors/sensor_base.h` (doc-comment fix lines 118-122)
  - `src/sensors/bno055.h` (doc-comment fix at line 142)
  - `src/main.cpp` (add periodic RUN telemetry dump — INFRA-5 bundled here)
- **READS:**
  - `tests/test_balance_app_collision.cpp` (contract; do not modify)
  - `docs/findings/research_collision_signature_bno055.md`
  - `docs/findings/audit_code_quality_balance_stack_2026-05-19.md`
- **DEPENDS ON:** INFRA-A (build_tests.sh) ideally landed first; otherwise rebuild test binaries by hand using each test's docblock compile line.
- **COMPLEXITY:** M

### Workstream B — Phase 4M.k audit follow-through

- **OWNS:**
  - `src/applications/balancing_robot/balance_app.h` (delete `tune_result_` + AUTO_TUNE-dead members; add `held_entry_reason_`; update state-diagram comment lines 7-13)
  - `src/applications/balancing_robot/balance_app.cpp` (delete `step_tune_()` and AUTO_TUNE switch case; update default_config doc-comment lines 166-168)
  - `tests/test_balance_app.cpp` (one new test for STUCK detector — P1-COV-3)
  - new file `tests/test_bootstrap_k_preservation.cpp` (P1-COV-1)
- **DEPENDS ON:** A (file conflict on balance_app.{h,cpp}). **Serial after A.**
- **COMPLEXITY:** M

### Workstream C — Phase 4M.1 wheel encoder driver

- **OWNS (all NEW):**
  - `src/sensors/wheel_encoder.h`
  - `src/sensors/wheel_encoder.cpp`
  - `tests/test_wheel_encoder.cpp`
- **READS:**
  - `docs/findings/research_wheel_encoders_mega_2026-05-19.md` §3, §4
  - `src/sensors/sensor_base.h` (pattern only)
  - `src/actuators/l298n_motor_driver.h` (pin reference only)
- **Pin allocation (RWE §2):** Left A/B = Mega 18/19 (INT5/INT4); Right A/B = 2/3 (INT0/INT1). Pins 20/21 are I²C → BNO055, MUST NOT repurpose. Compile-time assert that `USE_WHEEL_ENCODERS` + `USE_GPS` are not both defined.
- **DEPENDS ON:** none — fully parallel with A.
- **COMPLEXITY:** M

### Workstream D — Phase 4M.11 `e` command + EEPROM calibration

- **OWNS:** `src/main.cpp` (serial-command switch + EEPROM read/write helpers for slot `0x220`)
- **READS:** `src/sensors/wheel_encoder.h` (must exist on main)
- **DEPENDS ON:** C (encoder driver), and A (main.cpp conflict — sequence A first).
- **COMPLEXITY:** S

### Workstream E — Phase 4M.12 PWM range auto-discovery

- **OWNS:** `src/applications/balancing_robot/balance_app.{h,cpp}` (CHAR_PWM_RANGE state + handler), `src/main.cpp` (`p` serial command)
- **DEPENDS ON:** A, B, C, D — serialize.
- **COMPLEXITY:** M

### Workstream F — Phase 4M.2 K cross-check + 4M.13 velocity outer loop

- **OWNS:** `src/applications/balancing_robot/balance_app.{h,cpp}` (BOOTSTRAP K cross-check, step_run_ cascade), `src/control/position_loop.{h,cpp}` (NEW for 4M.13)
- **DEPENDS ON:** all of A, B, C, D, E.
- **COMPLEXITY:** L. Splittable into 4M.2 (S-M) + 4M.13 (M).
- **Sequencing-discipline flag**: 4M.13 will land with hardcoded `K_POS`, `K_VEL`, `MAX_NUDGE_DEG`, `POS_LEAK`, `SLEW_DEG_S` per RWE §6.1 — auto-derivation deferred to Phase 4M.14. **Acceptable only if 4M.14 is on roadmap AND nobody bench-tunes these in the same session that lands 4M.13.**

### Workstream UNO-A — Tuner convergence run + Uno bench validation

- **OWNS:**
  - `src/applications/balancing_robot_uno/balance_constants.h` (tuner output)
  - `tools/sim/brute_tune.py` (small tweaks if convergence stalls)
- **READS:** `tools/sim/balance_bot_sim.py`, `src/applications/balancing_robot_uno/{main.cpp, uno_balance_app.{h,cpp}}`
- **DEPENDS ON:** none — fully orthogonal to all Mega workstreams.
- **COMPLEXITY:** S compute + S bench (bench-blocked if no hardware access).
- **Command:** `python3 tools/sim/brute_tune.py --mode evolutionary --budget 5000 --preset uno_small --output src/applications/balancing_robot_uno/balance_constants.h`

### Workstream UNO-B — Disturbance + plant-uncertainty sweep hardening

- **OWNS:** `tools/sim/brute_tune.py` only
- **DEPENDS ON:** UNO-A baseline must land first.
- **COMPLEXITY:** M

### Workstream UNO-C — Uno re-tune workflow doc

- **OWNS:** `docs/applications/balancing_robot_uno/README.md` (NEW; path from roadmap.md line 240)
- **DEPENDS ON:** UNO-A
- **COMPLEXITY:** S

### Workstream INFRA-A — `tools/build_tests.sh` rewrite

- **OWNS:**
  - `tools/build_tests.sh`
  - `tests/test_held_state_machine.cpp` (one-line `#if !defined(USE_BALANCE_HELD_DETECTION) #error` at top)
- **DEPENDS ON:** none. **Should land first.** Drives correct binaries for all Mega workstreams.
- **COMPLEXITY:** S

### Workstream INFRA-B — `.gitignore` for test binaries

- **OWNS:** `auto_orientation/.gitignore`
- **DEPENDS ON:** none.
- **COMPLEXITY:** S

### Workstream DOC-A — Roadmap / todo / scope drift fixes

- **OWNS:** `docs/roadmap.md`, `docs/todo.md`, `docs/scope.md`
- **DEPENDS ON:** A, B, C, D, E, F, UNO-A, UNO-B, INFRA-A, INFRA-B (lands LAST; reconciles prose with shipped reality).
- **COMPLEXITY:** S-M

---

## 3. Dependency graph

```
INFRA-A ── INFRA-B
   │
   ├── A (4M.0 collision) ────► B (4M.k audit follow-up) ──┐
   │       │                                                │
   │       └─► (main.cpp conflict with D)                   │
   │                                                        ▼
   │   C (4M.1 encoder driver) ────► D (4M.11 e cmd) ──► E (4M.12 PWM range)
   │          │                              │                  │
   │          └──────────────────────────────┼──────────────────┼──► F (4M.2 + 4M.13)
   │                                         │                  │
   ├── UNO-A (tuner run) ── UNO-B (sweep) ── UNO-C (workflow doc)
   │
   └── DOC-A (reconcile docs) — lands LAST
```

---

## 4. Parallelization for this session

**Wave 3 dispatch (4 parallel agents, non-overlapping WRITE zones):**

| Agent | Workstream | Files owned (write-zone, exclusive) |
|---|---|---|
| `infra-agent@floppi:1` | INFRA-A + INFRA-B | `tools/build_tests.sh`, `tests/test_held_state_machine.cpp` (one-line edit), `auto_orientation/.gitignore` |
| `collision-restorer@floppi:1` | A (4M.0 + bundled fixes) | `src/applications/balancing_robot/balance_app.{h,cpp}`, `src/control/plant_identifier.cpp`, `src/sensors/sensor_base.h`, `src/sensors/bno055.h`, `src/main.cpp` |
| `encoder-driver@floppi:1` | C (4M.1) | `src/sensors/wheel_encoder.{h,cpp}` (NEW), `tests/test_wheel_encoder.cpp` (NEW) |
| `tuner-runner@floppi:1` | UNO-A | `src/applications/balancing_robot_uno/balance_constants.h`, `tools/sim/brute_tune.py` |

Conflict-prevention notes:
- `collision-restorer@floppi:1` and `encoder-driver@floppi:1` both compile against `mega_balance` env but write disjoint files
- `tuner-runner@floppi:1` is fully orthogonal (Uno path)
- `infra-agent@floppi:1` touches only build infrastructure
- All 4 agents share read access to docs/findings/ and src/ but only one writes to any given file

**Wave 4 (after Wave 3 lands):**
- B (depends on A): cleanup + new tests
- D (depends on C): `e` command + EEPROM
- UNO-B (depends on UNO-A): sweep hardening
- UNO-C (depends on UNO-A): workflow doc

**Wave 5:** E (depends on A, B, C, D)

**Wave 6:** F (depends on A, B, C, D, E)

**Wave 7:** DOC-A (lands LAST)

---

## 5. Risk register

1. **Mega RAM overflow** (HIGH) — architect could not find `mega_orientation_ram_overflow_diagnosis_2026-05-19.md`. Encoder driver + collision state add ~90B RAM. Mitigation: run `pio run -e mega_balance` before merging C; if tight, retire AUTO_TUNE-dead `tune_result_` (P2 audit gives +28B).
2. **Sequencing-discipline drift** (HIGH) — recurring failure mode: agent edits a constant instead of building the measurement that retires it. Every workstream above is structured as a **mechanism**, not a **value**. Single flag: workstream F's velocity-loop gains land hardcoded; auto-derive deferred to Phase 4M.14. Acceptable only if 4M.14 stays on roadmap.
3. **Test-binary contamination** (MEDIUM) — until INFRA-A lands, every workstream touching `tests/` must rebuild affected binaries using documented per-test compile lines.
4. **BNO055 LIA glitch** (MEDIUM, mitigated) — register-tear can produce ±2.55 m/s² noise or full-scale spikes. Pre-filter `isnan` + ±40 m/s² clamp in §1.4 belt-and-suspenders.
5. **Operator memory-file drift** (LOW) — `audit_documentation_2026-05-19.md` left 4 broken links; not blocking.
6. **Bench-access dependency** (LOW-MED) — workstreams A, C, D, E, F, UNO-A reach bench gates. If no bench, code lands green on native tests; bench-validate gates defer.

---

## 6. Open questions (to surface to operator before Wave 6)

1. **Mega RAM overflow finding** — confirm RAM headroom number for `mega_balance` before dispatching Workstream C or any Mega-stack additions.
2. **Audit findings referenced in brief but not on disk**: `audit_uno_minimal_2026-05-19.md`, `phase_4_11a_design_2026-05-19.md`, `brute_tune_simplification_design_2026-05-19.md`. Confirm whether these landed and where.
3. **Bench access this week?** Determines whether bench-validate gates apply or defer.
4. **`tests/test_balance_app_collision.cpp` tracked state** — confirm it's now in the merged tree; Workstream A includes committing it alongside the restoration hunk if not.

---

## 7. Sequencing-discipline checklist

Every workstream's deliverable is a **mechanism**, not a value:

| Workstream | Builds measurement? | Bench-iterates a constant? |
|---|---|---|
| A (4M.0 collision) | 3-gate LIA detector | No — thresholds from research doc |
| B (4M.k audit) | STUCK + K-preservation tests | No |
| C (4M.1 encoder driver) | Encoder velocity | No |
| D (4M.11 e cmd) | CPM persistence | No |
| E (4M.12 PWM range) | PWM_MIN/MAX per wheel | No |
| F.1 (4M.2 K cross-check) | Independent K estimate | No |
| F.2 (4M.13 velocity loop) | Cascade mechanism | **Flag — K_POS/K_VEL hardcoded until 4M.14** |
| UNO-A (tuner run) | Tuner IS the measurement (offline) | No — sim-iterates by design |
| UNO-B (sweep) | Robustness envelope | No |

---

## 8. Cumulative findings inventory (across 5 audits, 2026-05-20)

| Audit | Findings | P0 | P1 | P2 | P3+ |
|---|---|---|---|---|---|
| Documentation | 41 | 8 | 13 | 12 | 8 |
| Code Quality | 19 | 1 (collision regression) | 3 | 3 | 12 |
| Test Coverage | 26 | 2 | 7 | 8 | 9 |
| Security | 32 | 0 | 4 | 19 | 9 |
| Build System | 30 | 4 | 5 | 10 | 11 |
| **TOTAL** | **148** | **15** | **32** | **52** | **49** |

Wave 3 absorbs all P0 items except the build-system ones (which INFRA-A doesn't fully cover — see Wave 4 for residual platformio.ini hygiene). Wave 4-6 absorb most P1 items.

---

*Plan complete. Wave 3 dispatch follows immediately.*
