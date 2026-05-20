# Future Session Scaffolding for flight_controller (2026-05-20)

**Agent**: fc-scaffolding-planner@flight_controller:1
**Saved by**: orchestrator (Plan-agent harness is read-only; this is the orchestrator's transcription)
**Status**: planning-ready

**Cross-references** (do not duplicate):
- `project_recon_2026-05-20.md` (fc-recon)
- `test_infrastructure_v2_2026-05-20.md` (fc-test-modularizer)
- `/home/devel/floppi/docs/findings/bno_cross_project_2026-05-20.md` (bno-cross-project-researcher) — covers IMU/calibration synergies, NOT duplicated below

---

## 1. Current "what we have" — completed scope inventory

Baseline as of 2026-05-20, after fc-sbus-fixer re-enabled `USE_SBUS_RECEIVER` and fc-test-modularizer split the test harness into `tests/lib/` + `tests/suites/`.

### Build environments (10/10 should now compile)

| Env | Type | Notes |
|---|---|---|
| `teensy40` | Live | Now passes — SBUS re-enabled |
| `teensy40_calibration` | Calibration | 42/42 assertions pass on bench |
| `teensy41` / `teensy41_calibration` | Live / Calib | Same code, more pins |
| `teensy36` / `teensy36_calibration` | Legacy | Older MCU, no FPU |
| `esp32` / `esp32_calibration` | Live / Calib | Dual-core, WiFi+web+API+OTA |
| `esp32s3` / `esp32s3_calibration` | Live / Calib | USB native variant |

### Working firmware features

- Madgwick 6DOF attitude filter (single beta parameter)
- PID loops in rate + angle mode with D-on-measurement and PT1/biquad D-term LPF
- Motor mixing (quad X) + 7 servo channels (up to m1-m6 motors)
- Arming/disarming (throttle-low + CH5) + receiver-loss failsafe
- 5 RC protocols (SBUS, iBUS, DSM, PPM, PWM) + 3 override sources (serial, I2C, WiFi)
- Command source arbitration (`USE_COMMAND_ARBITRATION`)
- OLED display module (SSD1306 128x32/128x64, SH1106) with 2-second screen rotation on 128x32
- ESP32 dual-core (Core 0 = FC at 1 kHz, Core 1 = display+WiFi+web+API+OTA)
- WiFi STA mode incl. WPA2-Enterprise; web server with JSON + WebSocket + mDNS; HTTP POST API client; OTA gated by `armedFly`
- `USE_OPTIMIZATION` (biquad gyro/D-term, gyro notch, accel 2nd LP) and `USE_RACING` (FF, TPA, expo, air mode, setpoint smoothing) feature tiers

### Calibration system (~10 routines, all auto-validated)

`i`, `m`, `o`, `r`, `f`, `e`, mag, `g` (PID tune), `p` (filter tune), `d` (dump), `a` (sequential), `c` (status), `s` (channels), `t` (telemetry), `n` (network).

### Test suite (post fc-test-modularizer)

- `tests/lib/harness.sh` (246 lines) — port mgmt, serial wrapper, assertions, CDC recovery, boot drain
- `tests/suites/test_calibration.sh` (304 lines) — 18 functions, 42 assertions
- ESP32 reset path: stubbed (documented), not yet functional
- Entry point `tests/test_calibration.sh` preserved as thin wrapper

### Documentation

- User: `README.md`, `0_quickstart.md`, `1_hardware_setup.md`, `2_calibration_guide.md`, `3_troubleshooting.md`
- Architecture/scope: `scope.md`, `roadmap.md`, `todo.md`
- Features: `features/build-targets.md`, `features/calibration-guide.md`, `features/compile-time-architecture.md`, `features/wifi-configuration.md`
- Findings: 14 docs incl. recon and test-modularizer outputs from today
- Wiring: 2 platform refs + 3 build-specific drone diagrams

### Tooling

- `tools/dev.sh` (unified entry point), `tools/calibrate.sh` (menu), `tools/serial_monitor.py` (raw termios), `tools/flash_and_run.sh`, `tools/complexity_calculator.py`, `tools/calibration_reset.py`

**The above is the operator's "completed" baseline.** Everything else in this document is scaffolding for what comes next.

---

## 2. "What we have" gaps — final polish for current scope

These items close out the CURRENT scope (no new features). Each is sized ≤4 hours. Maximum 10.

| # | Title | Scope | Files | Exit criteria |
|---|---|---|---|---|
| 1 | **PID tuning guide using `g` command** | S (2h research + 2h doc) | NEW `docs/pid-tuning-guide.md`; references `lib/Calibration/calibration_imu.cpp`, `src/main.cpp` serial parser, `include/config.h` PID section | Document the calibration-mode `g` workflow with: conservative starting values per VTOL type, step-by-step "oscillate → reduce 20%" loop, log capture via `serial_monitor.py --output`, when to re-tune after frame/prop changes. Linked from `roadmap.md` and `2_calibration_guide.md`. |
| 2 | **ESP32 wiring guide fidelity audit** | S (2-3h, no hardware) | `docs/esp32_wiring.md`, `docs/wiring_diagrams/esp32_wiring_fsia6b_drone.md`, `docs/wiring_diagrams/esp32_wiring_web_api_drone.md` | Cross-check every pin against `include/pin_definitions_esp32.h` defaults and config.h PIN OVERRIDES; flag any drift between guides and code; add a "verified 2026-05-XX against pin_definitions_esp32.h@<sha>" stamp. |
| 3 | **Teensy wiring guide fidelity audit** | S (2h, no hardware) | `docs/teensy_wiring.md`, `docs/wiring_diagrams/teensy_wiring_fsia6b_drone.md` | Same as #2 but against `include/pin_definitions.h`. |
| 4 | **ResearchHub ingestion of 14 findings docs** | S (3h) | `docs/findings/*.md`, RH config | Ingest existing 14 findings into RAG; verify search returns relevant chunks for topics in scope.md (quaternions, PID, fusion, etc.). Documented in `docs/findings/researchhub-ingestion-notes.md`. (Item is in roadmap "Nice to Have".) |
| 5 | **Roadmap/todo refresh post-SBUS / post-test-modularizer** | S (1h) | `docs/roadmap.md`, `docs/todo.md` | Mark "SBUS re-enabled / 10-of-10 builds" complete; mark "modular test runner architecture (refactor stage)" done; reset Next Session goal to calibration phases 1-7. |
| 6 | **Live build smoke-test for ESP32 (no motors)** | M (3-4h, NEEDS hardware) | `docs/findings/esp32-bench-smoke-2026-XX.md` (new) | Run boot → `dev.sh test` → web UI ping → WebSocket echo → OTA round-trip on an ESP32 dev board (no ESCs). Capture timings (Core 0 loop µs, Core 1 web latency). NOTE: only if hardware is available — otherwise defer. |
| 7 | **WiFi credentials onboarding doc** | S (1h) | `docs/findings/esp32-wifi-onboarding.md` or section in `1_hardware_setup.md` | Describe first-time ESP32 WiFi setup (creating `include/wifi_credentials.h`, gitignore confirmation, fallback hostname behavior). Existing `features/wifi-configuration.md` covers configuration; this fills the onboarding gap. |
| 8 | **Diagnose flow documentation** | S (1h) | `docs/3_troubleshooting.md` (extend), references `tools/dev.sh diagnose` | Add a "when something is wrong" decision tree mapping symptoms → `dev.sh diagnose` outputs → corrective action. Pull from existing notes in `todo.md` (ModemManager, CDC recovery, gyro bias, MPU mounting). |
| 9 | **Add 3 telemetry-mode assertions to test suite** | S (2h) | `tests/suites/test_calibration.sh` | Add tests that verify `t 1` (IMU mode), `t 2` (full), `t 3` (quaternion) emit expected line formats. Bench-verified, ≤3 new assertions, total assertion count remains under 50. |
| 10 | **Stub ESP32 reset implementation in harness** | M (3-4h) | `tests/lib/harness.sh` (extend the stubbed reset path), `tools/serial_monitor.py` (RTS/DTR toggle if needed) | Implement the documented ESP32 reset path so suites can target ESP32 builds. Requires either ESP32 hardware OR a thorough mock; if hardware-blocked, defer to §4 Session 2. |

**Selection note**: items 1-5 are zero-hardware, immediately doable. Items 6, 9, 10 want hardware. Item 8 can be done now but is lower urgency.

---

## 3. Scaffolding for FUTURE sessions (research/plan, no implementation)

For each topic: framing, what doc would unblock the next implementation session, complexity rating, and an "over-complicates current scope?" verdict per the operator's "don't over-complicate" rule.

### 3.1 Motor / ESC test framework
**Framing**: Recon Workstream 4 item 1. Once ESCs are connected, FC needs a safety-gated test harness that can drive PWM ranges, verify failsafe cuts, and validate mixer response — all with props removed and explicit user confirmation. Today, only the `e` command exists for endpoint calibration; nothing exercises mixing in a controlled way.
**Scaffolding deliverable**: `docs/plans/motor-test-framework-plan.md` covering safety state machine (props-off attestation, battery-out attestation, kill switch), six test phases, integration with `tests/lib/harness.sh` (new suite `tests/suites/test_motors.sh` — stub only), what needs to land in firmware (likely: "test mode" command in calibration build that maps inputs to single motors).
**Complexity**: M (4-6h research/spec, zero hardware). **OVER-COMPLICATES?**: NO — finishing existing calibration story.

### 3.2 Barometer integration spec (`USE_BAROMETER`)
**Framing**: Recon §9 lists this as next-most-modular sensor add. scope.md currently calls baro "flight computer territory", which conflicts with recon's roadmap. Operator should choose, and a spec doc forces the choice. Any baro on FC is altitude-hold / vertical-rate territory — actual altitude PID belongs on flight computer per scope.
**Scaffolding deliverable**: `docs/findings/barometer-integration-spec.md` covering sensor candidates (BMP280, BMP388, MS5611), where it runs (Core 1 telemetry-only vs Core 0 vertical-rate term), pin allocation, config flag layout, calibration routine outline, **explicit scope decision** (telemetry-only or feedback-into-loop).
**Complexity**: M (4-6h research). **OVER-COMPLICATES?**: CONDITIONAL — telemetry-only is fine; vertical-rate feedback drifts into autopilot territory.

### 3.3 Magnetometer integration spec (`USE_COMPASS`)
**Framing**: Sphere-cal for MPU9250 already exists. Dedicated compass spec would cover external compass modules (HMC5883L, QMC5883L, LIS3MDL) for users on MPU6050 who want yaw stabilization.
**Scaffolding deliverable**: `docs/findings/magnetometer-integration-spec.md` — driver options, I2C addresses, integration into Madgwick (6DOF → 9DOF promotion path — already in roadmap "Nice to Have"), and explicit recommendation: defer external mag to bno-cross-project sibling if BNO055/85 makes mag a non-issue.
**Complexity**: S (3h research) — most heavy lifting overlaps with BNO sibling. **OVER-COMPLICATES?**: CONDITIONAL.

### 3.4 GPS integration spec (`USE_GPS`)
**Framing**: scope.md explicitly excludes GPS as "flight computer territory". auto_orientation has `GPS_QUICK_START.md`. Cross-project synergy might mean GPS belongs on Core 1 of ESP32 as a passthrough to flight computer, NOT in flight loop.
**Scaffolding deliverable**: `docs/findings/gps-passthrough-spec.md` covering: GPS module UART → Core 1 → API/WebSocket relay → flight computer. Explicitly NOT a "GPS-guided flight" feature.
**Complexity**: S (2-3h). **OVER-COMPLICATES?**: CONDITIONAL — passthrough mode is fine; active GPS use (waypoints, RTH) is OVER-COMPLICATES: YES.

### 3.5 Swarm coordination / `swarm_api` integration spec
**Framing**: `/api/commands` and `/api/status` exist; `swarm_api/` is a sibling Python project that talks to FC over WiFi. Contract between them is implicit, scattered across `web_server.cpp`, `command-arbitration-design.md`, and swarm_api code.
**Scaffolding deliverable**: `docs/findings/swarm-api-contract.md` — formal protocol spec (endpoints, payload schemas, failsafe semantics, timing guarantees, version negotiation). Lifted from existing code, not new design. Once written, swarm_api side can also reference it.
**Complexity**: S (3h) — reading existing implementations. **OVER-COMPLICATES?**: NO — documents what already exists.

### 3.6 Telemetry storage / SD-card flight log
**Framing**: scope.md hard-prohibits SD cards in live firmware. auto_orientation has a "snapshot recorder" pattern. Recon mentions blackbox is explicitly out of scope.
**Scaffolding deliverable**: NONE. Document the decision in `docs/findings/telemetry-storage-decision.md` (1 page) closing the question: "no SD card; for flight log, ESP32 streams over WiFi to a host running swarm_api / logger script; Teensy users get serial logging via `serial_monitor.py --output`."
**Complexity**: S (1h). **OVER-COMPLICATES?**: NO if decision-doc; YES if anyone tries to implement SD logging.

### 3.7 PID autotuning
**Framing**: Roadmap "Nice to Have" item. Manual `g` covers 90%. Autotuning is large.
**Scaffolding deliverable**: DEFER ENTIRELY. bno-cross-project sibling may touch BOOTSTRAP/RLS techniques from auto_orientation. Wait for that output before doing FC-side autotune scoping.
**Complexity**: L (multi-session). **OVER-COMPLICATES?**: YES → defer.

### 3.8 OLED dashboard expansion
**Framing**: `display.cpp` is mature. 128x32 cycles screens every 2s. 128x64 shows all info. Operator may want flight-stats screens (max gyro, peak motor, last-error code).
**Scaffolding deliverable**: `docs/findings/oled-dashboard-expansion-ideas.md` (1-page) — list of screen ideas, prioritized; ROI vs adding to telemetry instead.
**Complexity**: S (1-2h) — capped to ideas list. **OVER-COMPLICATES?**: CONDITIONAL — only worth doing once live flight reveals what info operator wants on-board. Defer until first hover.

### 3.9 WiFi failover behavior
**Framing**: WiFi drops mid-flight on ESP32 — what happens? Today: api_client retries, web_server keeps trying, OTA disabled when armed. Failsafe path through RadioComm handles command-source loss.
**Scaffolding deliverable**: `docs/findings/wifi-failover-behavior.md` — trace each subsystem's behavior on disconnect. Confirm RadioComm arbitration correctly falls back to RC receiver on WiFi command-source timeout (already designed; just verify on paper).
**Complexity**: S (2h). **OVER-COMPLICATES?**: NO — documenting existing behavior.

### 3.10 Voltage monitoring / low-battery warning
**Framing**: Roadmap "Nice to Have". Simple ADC read, LED/buzzer/OLED warning. Useful before any meaningful flight.
**Scaffolding deliverable**: `docs/findings/vbat-monitoring-spec.md` — ADC pin choice (Teensy & ESP32), divider math, warning thresholds, integration into display & telemetry only (no loop coupling).
**Complexity**: S (2h). **OVER-COMPLICATES?**: NO — small, additive, safety-relevant.

### 3.11 Ground-station integration
**Framing**: §3.5 covers wire protocol; this would cover human-facing "what does the dashboard show / control" UX — entirely owned by swarm_api project, not FC.
**Scaffolding deliverable**: NONE on FC side. Cross-reference swarm_api docs.
**OVER-COMPLICATES?**: YES on FC side → defer to swarm_api project entirely.

---

## 4. Sequencing — proposed agenda for the next 3 sessions

Each session = 6-8h of agent work budget total. All session items are zero-hardware unless flagged.

### Session 1 — Close out CURRENT scope (post-current)
| Deliverable | Source | Size |
|---|---|---|
| PID tuning guide (`docs/pid-tuning-guide.md`) | §2 #1 | M (3-4h) |
| Wiring-guide fidelity audit (Teensy + ESP32, both deep & build-specific) | §2 #2 + §2 #3 | M (4h) |
| Roadmap/todo refresh post-SBUS + post-test-modularizer | §2 #5 | S (1h) |
| WiFi onboarding doc + diagnose decision tree (combine §2 #7 + #8) | §2 #7, §2 #8 | S (2h) |
| ResearchHub ingestion of 14 findings (if RH is ready) | §2 #4 | S (2-3h) |

Total: ~12-14h — likely needs splitting. Pick 3-4 to fit ~6-8h.

### Session 2 — Low-risk research deliverables (no hardware needed)
| Deliverable | Source | Size |
|---|---|---|
| Motor / ESC test framework spec | §3.1 | M (4-6h) |
| Swarm-API contract spec (documents existing code) | §3.5 | S (3h) |
| Telemetry-storage decision doc | §3.6 | S (1h) |
| WiFi failover behavior trace | §3.9 | S (2h) |
| Voltage-monitoring spec | §3.10 | S (2h) |

Total: ~12-14h. Recommendation: items #1, #2, #4.

### Session 3 — Bigger integration scaffolds (need §2 prior work)
| Deliverable | Source | Size |
|---|---|---|
| Barometer integration spec | §3.2 | M (4-6h) |
| Magnetometer spec (post-BNO research) | §3.3 | S (2-3h) |
| GPS passthrough spec | §3.4 | S (2-3h) |
| OLED dashboard expansion ideas | §3.8 | S (1-2h) |
| Cross-reference doc to bno-cross-project output | §6 | S (1-2h) |

Total: ~10-16h. Recommendation: items #1, #3, #5.

---

## 5. Risk register

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| BNO sibling agent picks an IMU that obsoletes parts of FC IMU spec | M | M | Sequence Session 3 baro/mag specs AFTER bno-cross-project output lands. Reference, don't duplicate. |
| Hardware unavailable blocks every "verify on hardware" item | H | M | All Session 1/2/3 items chosen above are zero-hardware. Hardware-dependent items explicitly flagged. |
| Operator scope creep — adding "while we're at it" features | M | M | This document's "OVER-COMPLICATES?" verdicts are the firewall. If a future agent wants to expand scope, must first justify against scope.md and bare-bones philosophy. |
| Test suite drift — adding ESP32 reset partially could break Teensy flow | L | H | Stubbed reset path in `tests/lib/harness.sh` is the safe surface. Don't enable ESP32 path until hardware-validated. |
| ResearchHub config / ingestion not actually ready | M | L | §2 #4 is conditional. Move to backlog if not ready. |
| Token budget on parallel agents | M | L | Future agent dispatches should each have a single deliverable from §4 to avoid overrun. |
| `swarm_api` evolves independently and contract drifts | M | M | §3.5 contract doc + a "verified <date> against swarm_api@<sha>" stamp pattern. |

---

## 6. Cross-project synergies (deferred to sibling)

`bno-cross-project-researcher` produced `/home/devel/floppi/docs/findings/bno_cross_project_2026-05-20.md` covering IMU and calibration synergies between `auto_orientation` and `flight_controller`. That doc is the authoritative source for:

- IMU choice for FC v2 (MPU6050/9250 vs BNO055 vs BNO085)
- Calibration-pattern reuse (sphere cal, 6-position, orientation auto-detection)
- Madgwick beta vs BNO internal fusion trade-offs
- Coordinate-frame manager reuse
- Possibly autotuning patterns (BOOTSTRAP/RLS work)

**This plan's dependencies on bno-cross-project output**:
- §3.3 (magnetometer spec) — may collapse to "use BNO integrated mag" if BNO is chosen.
- §3.7 (PID autotuning) — deferred entirely pending RLS notes.
- §3.2 (barometer) — independent; no dependency.
- §2 calibration items — independent; no dependency.

When future implementation begins, Session 3's first task should be a 1-2h cross-reference doc that updates §3.3 and any obsolete recommendations.

---

## 7. Open questions for the operator

1. **Default IMU for FC v2** — Stay on MPU6050/9250, or transition to BNO055/BNO085 (with auto_orientation's mature integration) once bno-cross-project research lands? Decision affects §3.3 and magnetometer roadmap.
2. **GPS scope clarification** — scope.md says GPS is flight-computer territory; auto_orientation has `GPS_QUICK_START.md`. Is operator's intent that FC should be a passthrough for GPS data, or stays fully out? §3.4 hinges on this.
3. **Hardware availability through Q2 2026** — Are ESCs/motors expected on bench before next session, or should §4 stay zero-hardware? Specifically when can §2 #6 (ESP32 smoke test) and motor-test framework execution actually run?
4. **swarm_api integration appetite** — Is operator planning to actively use `swarm_api` this quarter, or is FC's WiFi API mostly dormant? Affects priority of §3.5 contract spec.
5. **ResearchHub readiness** — Is RAG pipeline ready to ingest the 14 findings docs (§2 #4)? If not, what's current blocker?

---

## Top 3 deliverables for next session
1. PID tuning guide using existing `g` command — closes most-cited current-scope gap
2. Wiring-guide fidelity audit (Teensy + ESP32) — keeps user-facing docs trustworthy
3. Roadmap/todo refresh + WiFi onboarding + diagnose decision tree — small but high-leverage cleanups

## Top 3 deferred-as-too-complex items (OVER-COMPLICATES: YES)
1. PID autotuning (§3.7) — pending BNO sibling output and not needed while manual `g` works
2. SD-card flight logging (§3.6) — scope-violating; document the decision instead
3. Active GPS-guided flight features (§3.4 advanced mode) — passthrough-only is at-most extent compatible with scope

## Top 3 operator questions
1. Default IMU for FC v2 (depends on BNO sibling output) — affects §3.3 and §3.2
2. GPS scope: passthrough-only on FC, or no driver code at all? — affects §3.4
3. Hardware timeline (ESP32 + ESCs on bench) — gates §2 #6, §2 #10, motor-test execution

---

*Plan complete. Operator can use this and bno_cross_project_2026-05-20.md as the joint scaffolding for the next 2-4 sessions.*
