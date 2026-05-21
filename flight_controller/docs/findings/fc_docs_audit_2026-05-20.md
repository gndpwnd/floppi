# Flight Controller Documentation Audit — 2026-05-20

> Auditor: `fc-docs-audit@flight_controller:1`
> Scope: READ-ONLY audit of `/home/devel/floppi/flight_controller/README.md` and the entire `docs/` tree.
> Output: This single file. No source/doc/test edits. No git commits. No builds.

Companion to today's other findings docs (recon, wiring-audit, GPIO conflicts, sensor specs, swarm contract). Those documents are *content* audits of specific surfaces; this one is a meta-audit of the documentation set itself: drift, broken links, missing pieces, and prioritized cleanup.

Sibling docs that landed in parallel today and are referenced below: `barometer_integration_spec_2026-05-20.md`, `gps_passthrough_spec_2026-05-20.md`, `swarm_api_contract_2026-05-20.md`, `esp32_gpio_conflict_resolution_2026-05-20.md`. They are linked, not summarized.

---

## 1. Doc tree map

Sizes from `wc -l` (lines). Purpose is a one-liner derived from the doc's own opening paragraph.

### Root
| Path | Lines | Date in name | Purpose |
|---|---:|---|---|
| `README.md` | 324 | — | User-facing entry: hardware list, build envs, calibration workflow, file structure. |

### `docs/` (top-level user/standing docs)
| Path | Lines | Date | Purpose |
|---|---:|---|---|
| `docs/README.md` | 60 | — | Doc-tree index for contributors. |
| `docs/scope.md` | 321 | — | What the project IS and IS NOT (last self-stamp: 2026-03-30, MODIFIED in git status). |
| `docs/roadmap.md` | 739 | — | Feature checklist + 2026-05-20 status snapshot. |
| `docs/todo.md` | 188 | — | Active session list + hardware-return phases. |
| `docs/0_quickstart.md` | 279 | — | 60-minute zero-to-flight path (Teensy-only framing). |
| `docs/1_hardware_setup.md` | 617 | — | Long-form Teensy+MPU6050+FS-iA6B wiring + BOM. |
| `docs/2_calibration_guide.md` | 380 | — | Calibration procedure narrative. |
| `docs/3_troubleshooting.md` | 879 | — | Flat-list symptom→fix reference. |
| `docs/diagnose_decision_tree.md` | 225 | — | Decision-tree symptom flow, pairs with `dev.sh diagnose`. |
| `docs/esp32_wifi_onboarding.md` | 143 | — | First-time WiFi credentials setup (created 2026-05-20). |
| `docs/esp32_wiring.md` | 263 | — | Per-platform ESP32 + S3 pin reference. |
| `docs/teensy_wiring.md` | 206 | — | Per-platform Teensy pin reference. |
| `docs/pid-tuning-guide.md` | 310 | — | `g`-command workflow + conservative starts (new 2026-05-20). |

### `docs/features/`
| Path | Lines | Purpose |
|---|---:|---|
| `docs/features/build-targets.md` | 136 | PlatformIO calibration vs live env spec. |
| `docs/features/calibration-guide.md` | 335 | Stage-based calibration progression (parallel narrative to `2_calibration_guide.md`). |
| `docs/features/compile-time-architecture.md` | 195 | `#ifdef` feature-gating philosophy. |
| `docs/features/wifi-configuration.md` | 119 | `wifi_credentials.h` config (parallel to `esp32_wifi_onboarding.md`). |

### `docs/findings/` (research + 2026-05-20 session)
| Path | Lines | Date | Purpose |
|---|---:|---|---|
| `docs/findings/INDEX.md` | 39 | — | Curated index of findings docs. |
| `docs/findings/acrobatics-command-architecture.md` | 189 | — | Acro/rate-mode command architecture. |
| `docs/findings/auto-calibration-research.md` | 925 | — | Calibration strategy backgrounder. |
| `docs/findings/bare-bones-fc-research.md` | 426 | — | "Don't be Betaflight" design philosophy. |
| `docs/findings/command-arbitration-design.md` | 625 | — | Source-priority arbitration design. |
| `docs/findings/display-module-architecture.md` | 509 | — | U8g2 + SW-I2C display arch. |
| `docs/findings/display-screen-capacity.md` | 102 | — | 128×32 / 128×64 layout analysis. |
| `docs/findings/esp32-dual-core-research.md` | 286 | — | Core-0/Core-1 split rationale. |
| `docs/findings/esp32-fc-feasibility.md` | 264 | — | ESP32 platform feasibility. |
| `docs/findings/esp32-wifi-connectivity.md` | 1030 | — | WiFi STA, WPA2-EAP. |
| `docs/findings/fc-timing-requirements.md` | 219 | — | 1–2 kHz loop budget. |
| `docs/findings/oled-display-options.md` | 242 | — | OLED hardware survey. |
| `docs/findings/timing-calculator-analysis.md` | 128 | — | Notes on `tools/complexity_calculator.py`. |
| `docs/findings/project_recon_2026-05-20.md` | 779 | 2026-05-20 | Full project map. |
| `docs/findings/test_infrastructure_v2_2026-05-20.md` | 205 | 2026-05-20 | `tests/lib/harness.sh` + suites split. |
| `docs/findings/future_session_scaffolding_2026-05-20.md` | 251 | 2026-05-20 | 3-session forward agenda + 5 operator questions. |
| `docs/findings/wiring_guide_audit_2026-05-20.md` | 144 | 2026-05-20 | 5 wiring docs vs `pin_definitions*.h`. |
| `docs/findings/esp32_gpio_conflict_resolution_2026-05-20.md` | 290 | 2026-05-20 | Conflict A/B/C resolution spec. |
| `docs/findings/barometer_integration_spec_2026-05-20.md` | 328 | 2026-05-20 | `USE_BAROMETER` Core-1 telemetry spec. |
| `docs/findings/gps_passthrough_spec_2026-05-20.md` | 480 | 2026-05-20 | `USE_GPS` passthrough spec. |
| `docs/findings/swarm_api_contract_2026-05-20.md` | 383 | 2026-05-20 | WiFi/HTTP/WS contract w/ swarm_api. |
| `docs/findings/fc_docs_audit_2026-05-20.md` | — | 2026-05-20 | **This file.** |

### `docs/plans/`
| Path | Lines | Purpose |
|---|---:|---|
| `docs/plans/calibrate-sh-plan.md` | 121 | Pre-implementation plan for `tools/calibrate.sh` (already shipped 2026-02-17). |
| `docs/plans/motor-test-framework-plan.md` | 375 | Spec for safety-gated motor test framework (no impl yet). |

### `docs/wiring_diagrams/`
| Path | Lines | Purpose |
|---|---:|---|
| `docs/wiring_diagrams/teensy_wiring_fsia6b_drone.md` | 295 | Build-specific Teensy + FS-iA6B drone wiring. |
| `docs/wiring_diagrams/esp32_wiring_fsia6b_drone.md` | 331 | Build-specific ESP32 + FS-iA6B drone wiring. |
| `docs/wiring_diagrams/esp32_wiring_web_api_drone.md` | 202 | ESP32 + WiFi-only (no receiver) wiring. |

### `docs/archive/`
| Path | Lines | Purpose |
|---|---:|---|
| `docs/archive/4_readme_original.md` | 389 | **Historical** copy of an older Teensy-only README — links inside point at non-existent `HARDWARE_SETUP.md`, `QUICKSTART.md`, etc. (see §2). |
| `docs/archive/bench-test-2026-02-17.md` | 91 | 42/42 pass session record. |
| `docs/archive/wifi_ap_mode_implementation.md` | 101 | Old WiFi-AP design (project chose STA — superseded). |
| `docs/archive/session_records/INDEX.md` | 11 | Newest-first session-record index. |
| `docs/archive/session_records/2026-05-20_recon_builds_and_scaffolding.md` | 154 | 2026-05-20 multi-agent session record. |

### Non-markdown docs (flagged separately)
| Path | Notes |
|---|---|
| `lib/ArduinoJson/LICENSE.txt` | Third-party vendored library license — not project doc. |
| `lib/MPU6050/keywords.txt`, `lib/U8g2/keywords.txt`, `lib/PWMServo/keywords.txt` | Arduino-IDE keyword files — not docs. |
| `sources/pdfs/*` (5 PDFs under `Complementary_and_Mahony_filters/`, `LQR_state-space_control_design/`, `Quaternion-based_attitude_representation/`) | Academic references; sized large; not exposed in any `findings/` link. |
| `generated/*.md` (≈80 files) | Auto-generated RAG knowledge / synthesis files (dates 2026-04-03 → 2026-05-05). **Not** indexed by `findings/INDEX.md`. Effectively orphaned (intentional, but a contributor entering through `findings/INDEX.md` will not find them). |

There are **no** swarm_api docs inside `flight_controller/swarm_api/` — that directory does **not exist** in this project. The repo-root `swarm_api/` is its own sibling project and out of audit scope. Reference at `README.md:318` and `docs/README.md:55` to `../swarm_api/` / `/swarm_api/` is therefore an inter-project link.

---

## 2. README audit

`/home/devel/floppi/flight_controller/README.md` (324 lines, last modified per git on 2026-05-04, pre-dating today's Session 2 work).

### Up-to-date with what shipped Session 2 (2026-05-20)?
**Partial.** Verdict per claim:

| Claim in README | Status against 2026-05-20 code | Evidence |
|---|---|---|
| 10 PlatformIO envs listed | OK; matches `platformio.ini:33–185`. | — |
| Calibration values "baked in" to config.h | OK; reaffirmed by today's roadmap. | — |
| Test suite "19 tests" (line 284, 311) | **STALE.** Today's harness has **18 tests / 42 assertions** post-modularization. | `docs/findings/test_infrastructure_v2_2026-05-20.md:9`, roadmap.md:20 |
| `serial_monitor.py` and `calibrate.sh` workflow | OK; matches today's tooling. | — |
| `USE_ANGLE_CONTROLLER` / `USE_RATE_CONTROLLER` (line 224–225) | OK; matches `include/config.h:309–310`. | — |
| No mention of BNO055 / BNO085 Phase A scaffolding | **MISSING.** Flags landed today per roadmap §2026-05-20 status. README's "Select your IMU" block (line 214–216) shows only MPU6050/MPU9250. | `include/config.h:96–97`, roadmap.md:25 |
| No mention of new findings (baro, GPS, swarm contract, GPIO conflicts, recon, session record) | **MISSING.** Documentation table at line 293–300 lists only `0_quickstart`, `2_calibration_guide`, `teensy_wiring`, `esp32_wiring`, `scope`, `roadmap`. Does not reference `pid-tuning-guide.md`, `diagnose_decision_tree.md`, or `esp32_wifi_onboarding.md` — all three created 2026-05-20. | README.md:293–300 vs roadmap.md:24 |
| 60-minute quickstart promised | Still Teensy-only framed; ESP32 quickstart implicit only. |  — |

### Build instructions vs `platformio.ini`
Env names in README table (line 53–60) all exist in `platformio.ini`:
- `teensy40`, `teensy40_calibration`, `teensy41`, `teensy41_calibration`, `esp32`, `esp32_calibration`, `esp32s3`, `esp32s3_calibration`.
- README does **not** list `teensy36` / `teensy36_calibration` although `platformio.ini:64,104` defines them (only listed as "Legacy" in hardware table). Defensible omission, but the Build Environments table is presented as complete.
- Quick-Start block (README.md:66–75) and live/calib workflow are accurate for the 2026-05-20 build status (10/10 envs compile per roadmap.md:19).

### Pin/wiring guidance vs `pin_definitions_esp32.h`
README does not contain pin tables — defers to `docs/teensy_wiring.md` and `docs/esp32_wiring.md`. Today's wiring audit (`wiring_guide_audit_2026-05-20.md`) and GPIO-conflict spec (`esp32_gpio_conflict_resolution_2026-05-20.md`) confirm those guides match `pin_definitions*.h`. README has **no** call-out for the 3 ESP32 GPIO `[VERIFY]` flags raised today — a new ESP32 contributor reading only the README will not know GPIO 4/16/17 are contested.

### Missing sections a new contributor would expect
- **Quickstart for ESP32**: only Teensy quickstart exists. ESP32 WiFi onboarding lives in `docs/esp32_wifi_onboarding.md` — README does not link it.
- **Hardware list comprehensiveness**: README:25–30 lists Teensy hardware only; ESP32 hardware (board variant, USB-UART chip) not enumerated.
- **RC-protocol selection guide**: README touches RC at line 218–221 with a 3-line code block; no decision matrix (FlySky → iBUS, FrSky → SBUS, Spektrum → DSM). `scope.md:130–139` has the canonical table; README does not link it.
- **OTA flash instructions**: not in README. The mechanism exists (`src/ota.cpp`, `include/ota.h`) and is referenced in `docs/findings/swarm_api_contract_2026-05-20.md:310–315`, but the README's flash instructions are PlatformIO upload only.
- **Swarm-API quickstart / contract pointer**: README:318 mentions `../swarm_api/` but does not link `findings/swarm_api_contract_2026-05-20.md` (the canonical contract).
- **Test-running guide**: README:268 mentions the test suite; no link to how to run a single test, where output goes, or what `tests/lib/harness.sh` does.

### Broken / stale links and file references
| Location | Refers to | Issue |
|---|---|---|
| `README.md:284` | "Automated test suite (19 tests)" | Wrong count; today is **18 / 42 assertions** (roadmap.md:20). |
| `README.md:311` | `tests/test_calibration.sh` "(19 tests)" | Same drift as above. |
| `README.md:295–300` | Documentation table | Does not list `pid-tuning-guide.md`, `diagnose_decision_tree.md`, `esp32_wifi_onboarding.md`, `1_hardware_setup.md`, `3_troubleshooting.md`. All exist on disk. |
| `README.md:317` | `[fc_tool](../fc_tool/)` | Path exists; OK. |
| `README.md:318` | `[swarm_api](../swarm_api/)` | Path exists at `/home/devel/floppi/swarm_api/`; OK. README does not link the canonical wire contract `findings/swarm_api_contract_2026-05-20.md`. |
| `docs/README.md:54` | `[fc_tool](/fc_tool/)` | Absolute path — works only if rendered at repo root. From `flight_controller/docs/README.md` viewed in GitHub, `/fc_tool/` resolves at repo root which is correct; on a local Markdown viewer the link is brittle. |
| `docs/README.md:55` | `[swarm_api](/swarm_api/)` | Same caveat as above. |
| `docs/archive/4_readme_original.md:15–37` | `docs/HARDWARE_SETUP.md`, `docs/QUICKSTART.md`, `docs/CALIBRATION_GUIDE.md`, `docs/PID_TUNING_GUIDE.md`, `docs/TROUBLESHOOTING.md`, `docs/RECEIVER_BINDING.md`, `docs/PIN_DEFINITIONS.md`, `docs/CONFIG_OPTIONS.md` | **All 8 broken** — files do not exist (renamed to lowercase numbered scheme). Archive is historical, but is still a discoverable `*.md` with live-looking links. |
| `docs/1_hardware_setup.md:618` | `./CALIBRATION_GUIDE.md` | **Broken.** Should be `./2_calibration_guide.md`. |

### README verdict: **PARTIAL — current**.
Bones are right; the user-visible drift is the stale test count (3 occurrences), the missing Documentation-table entries for the 3 docs created today, and silence on the 3 ESP32 GPIO `[VERIFY]` flags. None of those drifts will break a build; they will mislead a new contributor.

---

## 3. `docs/scope.md` audit

Self-stamps "Last updated: 2026-03-30" (scope.md:3) and is **MODIFIED** in git status — meaning the on-disk content is newer than the last commit. Read of HEAD shows the file already contains current language (RadioComm universal entry, modular hardware vision, calibration coverage table) and the most recent Revision-History entry is 2026-02-10. So the "Last updated" header is **drifted** relative to the actual content (the 2026-02-10 revision is the most recent of the eleven in §Revision History). Whatever modification git is tracking has not bumped either the header date or the Revision History.

### Does it bound the project correctly?
- "Flight stabilizer, not autopilot" framing (scope.md:10) is correct and consistent with `bare-bones-fc-research.md`.
- "GPS, barometer, magnetometer — flight computer territory" (scope.md:238) is the **load-bearing exclusion** that today's two new sensor specs explicitly cite (`barometer_integration_spec_2026-05-20.md:13`, `gps_passthrough_spec_2026-05-20.md:17`). Both new specs scope themselves under the Hardware Architecture Vision's "future sensors added modularly on Core 1" carve-out (scope.md:176).

### Sensor expansion acknowledgement?
**Drift.** scope.md was written before today's two sensor specs landed. Boundaries section still says baro/GPS are flight-computer territory (scope.md:238) and does not surface that there is now a defined telemetry-only path for both on Core 1. The two sensor specs are internally consistent with scope.md's architecture vision, but a reader of scope.md alone will conclude "no baro, no GPS" and miss the carved-out path.

### What changed (git-modified) vs framing in other docs?
Without `git diff` (read-only), the modification is opaque; the file as-read matches the framing other docs lean on. The MODIFIED marker plus stale header date is itself a process drift — header dates and the Revision-History table should bump in lockstep with any edit.

### Other scope drift
- "Last updated: 2026-03-30" is wrong against last actual revision entry (2026-02-10) and against today's MODIFIED-but-undated edit.
- `scope.md:291` ("14 findings documents in `docs/findings/` will be ingested into a RAG knowledge base") — at 2026-05-20 the actual count of `findings/*.md` (excluding INDEX.md and today's 7 dated docs) is **12 standing reference docs**. Today's 7 dated 2026-05-20 docs bring the inventory to 19 (plus this audit). "14" is now stale.
- `scope.md:284` ("Open Question: Best approach for Teensy 4.x EEPROM emulation") still open, never moved. With calibration values now permanently hard-coded into `config.h` and the BNO055/BNO085 calibration-storage HAL coming from auto_orientation per `bno_cross_project_2026-05-20.md`, this question is effectively answered ("don't use EEPROM"); the doc just hasn't closed it.
- `scope.md:287` ("Open Question: fc_tool integration coupling") is essentially dead — `scope.md:259` already records the testing-approach decision that drops the fc_tool dependency. The Open Question line should be retired.

---

## 4. `docs/findings/` inventory (today's session)

Status from a self-read of each opening block.

| Doc | Lines | Stamp/Status | Cross-refs out? |
|---|---:|---|---|
| `project_recon_2026-05-20.md` | 779 | "Status: READY for parallel implementation workstreams" — complete. | — |
| `test_infrastructure_v2_2026-05-20.md` | 205 | Implementation-landed report; complete. | scope §6 Pattern; harness already shipped. |
| `future_session_scaffolding_2026-05-20.md` | 251 | "Status: planning-ready" — complete. | All other 2026-05-20 docs descend from §3. |
| `wiring_guide_audit_2026-05-20.md` | 144 | Complete; 2 hard fixes + 3 `[VERIFY]` flags applied to wiring docs. | — |
| `esp32_gpio_conflict_resolution_2026-05-20.md` | 290 | "Status: SPEC ONLY" — complete spec, no impl. | wiring audit. |
| `barometer_integration_spec_2026-05-20.md` | 328 | "Status: Scaffolding spec — NO CODE" — complete. | scaffolding §3.2, swarm contract, scope. |
| `gps_passthrough_spec_2026-05-20.md` | 480 | "Status: Scaffolding spec — NO CODE" — complete. | scaffolding §3.4, swarm contract, GPIO spec, baro spec. |
| `swarm_api_contract_2026-05-20.md` | 383 | "Verified 2026-05-20 against … @HEAD (9dd60ca)" — complete; SHA-stamped. | scaffolding §3.5. |
| `fc_docs_audit_2026-05-20.md` | — | This file; complete on save. | All of the above. |

### Orphan check
- `findings/INDEX.md` lists 4 of today's docs under the "2026-05-20 session" heading: `project_recon`, `test_infrastructure_v2`, `future_session_scaffolding`, `wiring_guide_audit`.
- **NOT YET INDEXED** (orphan from `INDEX.md` perspective): `esp32_gpio_conflict_resolution_2026-05-20.md`, `barometer_integration_spec_2026-05-20.md`, `gps_passthrough_spec_2026-05-20.md`, `swarm_api_contract_2026-05-20.md`, and this file. All five exist on disk and are linked by other docs but not by the canonical findings index.
- The `archive/session_records/2026-05-20_recon_builds_and_scaffolding.md` record exists (154 lines), is indexed in `archive/session_records/INDEX.md`, and per its title is **content-scoped to the morning's recon+scaffolding work only** — it predates the four spec docs (GPIO, baro, GPS, swarm). So the canonical "what landed today" coverage requires reading both the session record and the four uncited specs. A contributor entering through `findings/INDEX.md` will miss the four orphans.

---

## 5. `docs/plans/` inventory

| Plan | Status | Promoted? |
|---|---|---|
| `plans/calibrate-sh-plan.md` | Self-stamps "Status: Planned, Priority: High, Created: 2026-02-13" | **Shipped 2026-02-17**: `tools/calibrate.sh` and roadmap.md:409 mark it `[x]`. The plan doc is stale-as-plan but useful as a design record. Should be archived or its header changed to "Status: Implemented (see roadmap §Calibration Wrapper)". |
| `plans/motor-test-framework-plan.md` | "Status: Spec only — no implementation, Priority: Medium (gated on hardware)" — current. | Not promoted to `findings/`; framed as plans-tier. Cross-refs scaffolding §3.1. Should remain in `plans/` until WS-1 lands. |

**Dangling plans:** none. The calibrate-sh plan is the only stale-as-plan doc; the motor-test plan is correctly dangling because it depends on hardware that hasn't returned.

---

## 6. Drift between docs and code

For each major surface, one concrete drift example (file:line both sides).

### a) `include/config.h` flag → docs mismatch
- `include/config.h:96–97`: `USE_BNO055` / `USE_BNO085` scaffolding flags exist (commented, OFF).
- `README.md:214–216` ("Select your IMU"): lists only `USE_MPU6050` and `USE_MPU9250`. **Drift.** Roadmap.md:25 records the Phase-A scaffolding landing today; the README missed the update.

### b) `pin_definitions_esp32.h` GPIO defaults → wiring docs
- `pin_definitions_esp32.h:151` `SBUS_RX_PIN = 16`; `pin_definitions_esp32.h:116` `SERVO_PIN_4 = 16`.
- `docs/esp32_wiring.md` lists both **and** has `[VERIFY]` flags annotated by today's wiring audit (`wiring_guide_audit_2026-05-20.md:42–46`). Wiring doc matches headers; what is missing is a README-level call-out. **Drift = absent README warning**, not a doc/code mismatch.

### c) `src/imu.cpp` IMU selection logic vs README/docs
- `src/imu.cpp:21–28`: `#ifdef USE_MPU6050` → MPU6050 instance; `#elif defined(USE_MPU9250)` → MPU9250 SPI on CS=36; `src/imu.cpp:85–96` BNO055 detect-only stub; `src/imu.cpp:98+` BNO085 detect-only stub.
- `README.md:214–216`: shows MPU6050/MPU9250 only. **Drift** identical to (a).
- `docs/1_hardware_setup.md:1–82` is MPU6050-exclusive; no mention of MPU9250 (which has SPI not I2C wiring) despite that being a documented `USE_*` option.

### d) `swarm_api/drone.py` (sibling project) → `swarm_api_contract_2026-05-20.md`
- The contract self-stamps "Verified 2026-05-20 against … `swarm_api/src/drone.py@HEAD`" (`swarm_api_contract_2026-05-20.md:8–13`). No measurable drift today — the doc is the canonical surface.
- **Latent drift risk**: the contract's §7 records that no `api_version` field exists in any JSON payload, so any future schema change (e.g., adding the `baro` block from `barometer_integration_spec_2026-05-20.md:271–278` or the `gps` block from `gps_passthrough_spec_2026-05-20.md:234–243`) will silently break clients. Two specs already announce they will do exactly that.

### e) `platformio.ini` env list → docs reference correct env names?
- `platformio.ini:33,49,64,84,94,104,152,160,169,178` define 10 envs.
- `README.md:53–60` lists 8 (omits the two `teensy36` legacy variants — defensible).
- `docs/0_quickstart.md:86–88`: `teensy40_calibration` ✓, `teensy40` ✓. OK.
- `docs/features/build-targets.md:26–37`: shows `teensy40/41/36_calibration`. OK.

### f) Test count drift (already flagged in §2)
- `tests/lib/harness.sh` + `tests/suites/test_calibration.sh` host **18 tests / 42 assertions** (`test_infrastructure_v2_2026-05-20.md:9`).
- README.md:284, README.md:311, `docs/README.md:50`, `docs/0_quickstart.md:268` all say **"19 tests"**. Four user-facing locations carry the wrong number.

### g) "Last updated" headers
- `scope.md:3`: "2026-03-30" — actual last revision-table entry is 2026-02-10; file is MODIFIED-but-undated 2026-05-20.
- `roadmap.md:3`: "2026-05-20" — current ✓.
- `todo.md:3`: "2026-05-20" — current ✓.

**Drift findings count: 7.**

---

## 7. Cross-reference graph

Forward-link matrix for today's 8 findings docs + the 1 archive session record. `+` = explicit text link or filename reference, `.` = no reference.

| from \\ to | recon | tests-v2 | scaffolding | wiring | gpio | baro | gps | swarm | session-rec |
|---|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
| recon | — | . | . | . | . | . | . | . | . |
| tests-v2 | . | — | . | . | . | . | . | . | . |
| scaffolding | + | + | — | . | . | . | . | . | . |
| wiring | . | . | . | — | . | . | . | . | . |
| gpio | . | . | . | + | — | . | . | . | . |
| baro | . | . | + | . | . | — | . | + | . |
| gps | . | . | + | . | + | + | — | + | . |
| swarm | . | . | + | . | . | . | . | — | . |
| session-rec | + | + | + | + | . | . | . | . | — |

### Missing cross-references that should exist
- **`barometer_integration_spec_2026-05-20.md` → `esp32_gpio_conflict_resolution_2026-05-20.md`**: baro spec puts the barometer on `Wire1` (GPIO 25/26 = `I2C_CMD_SDA/SCL`). Those pins are not contested today, but the GPIO-conflict spec is the canonical "which pins are safe on ESP32" reference and is **not linked** from the baro spec. GPS spec does link it (gps:15) for the analogous reason. Baro should follow.
- **`swarm_api_contract_2026-05-20.md` → `barometer_integration_spec_2026-05-20.md` / `gps_passthrough_spec_2026-05-20.md`**: both sensor specs declare schema additions to `/api/status` and `/api/telemetry` and explicitly call for re-stamping the swarm contract's "Verified … @SHA". The swarm contract itself **does not** acknowledge that two pending additions are queued. It should at minimum reference them as "known pending schema extensions".
- **`scope.md` ↔ today's specs**: every sensor/swarm spec quotes `scope.md`'s "Out of Scope" language. `scope.md` does **not** reverse-link any of today's specs. A reader of scope.md alone (the canonical "what is this project") will not learn that telemetry-only baro/GPS paths now have approved spec docs.
- **`findings/INDEX.md` → 5 today's docs**: gpio, baro, gps, swarm, and this audit are not indexed (see §4 orphans).
- **`docs/README.md` → 2026-05-20 docs**: the docs index lists only the standing user guides (scope, roadmap, todo, 0_quickstart, etc.). It does not reference `pid-tuning-guide.md`, `diagnose_decision_tree.md`, `esp32_wifi_onboarding.md` (all created today and listed in roadmap.md:24).
- **Scope-boundary partner check**: of the 4 dated specs, only baro and gps explicitly cite the scope.md "Out of Scope" line they sit adjacent to. GPIO and swarm specs do not — defensible (GPIO is implementation-internal, swarm contract is documenting existing code), but worth a single sentence each.

---

## 8. Missing docs that ought to exist

- **Wiring diagrams (image / svg / true graphic):** every wiring doc is ASCII-text or Mermaid. `docs/1_hardware_setup.md:54–82` is ASCII-art; `docs/wiring_diagrams/*` are tables + Mermaid. No image/SVG anywhere. No doc *points* at a missing-image filename (so no broken-image link); the gap is "no operator-friendly visual exists at all."
- **Test-running guide for a new contributor**: `test_infrastructure_v2_2026-05-20.md` documents the *split*; no doc walks a newcomer through "install pyserial, ensure ModemManager is off, run `./tests/test_calibration.sh /dev/ttyACM0`, read `tests/results/`, interpret pass/fail output". `tests/suites/test_calibration.sh` is referenced from roadmap and todo but not from any user-tier doc.
- **OTA-flash operator runbook**: `src/ota.cpp` ships and `swarm_api_contract_2026-05-20.md:310–315` warns "Anyone on the LAN can fly or OTA-flash a drone." No operator-tier doc explains:
  - how to enable/disable OTA at build time,
  - how to scope OTA to a trusted SSID,
  - what a stuck-armed drone failure mode looks like (OTA gated by `!armedFly`).
- **Build-matrix doc**: env-flag combinations that are supported vs tested vs broken. `platformio.ini` defines 10 envs; `config.h` defines ~12 orthogonal `USE_*` flags (`USE_MPU6050`, `USE_MPU9250`, `USE_SBUS_RECEIVER`, …, `USE_OPTIMIZATION`, `USE_RACING`, …, `USE_BAROMETER` future, `USE_GPS` future). Combinatorial space is large; today there is no canonical "we test these N combinations" matrix. `docs/features/build-targets.md` documents the live-vs-calibration split only.
- **PID-tuning hardware results doc**: `docs/pid-tuning-guide.md` is procedural; no doc captures any real-flight PID values for known frames. `roadmap.md:174–198` lists every step `[ ]` (blocked on hardware). When values land, where do they go? No template.
- **BNO055/BNO085 Phase-A operator note**: the flags exist in config.h, the cross-project plan (`/home/devel/floppi/docs/findings/bno_cross_project_2026-05-20.md`) describes Phase B/B.5/C, but no doc tells an operator-on-the-bench how to flip on the Phase-A flags to see the boot detection messages.
- **`swarm_api/` cross-project README pointer from inside flight_controller**: `flight_controller/swarm_api/` does not exist; the project lives at repo root. A new contributor cloning only `flight_controller/` and reading README.md:318 will follow `../swarm_api/` which **does** resolve in the parent repo but not in a `flight_controller/`-only checkout.

---

## 9. Tone / writing-quality spot check

### Unclear, ambiguous, or contradictory
- **`docs/scope.md:238`** — "GPS, barometer, magnetometer — flight computer territory" — read in isolation contradicts today's `barometer_integration_spec_2026-05-20.md` and `gps_passthrough_spec_2026-05-20.md`. The specs reconcile via the Hardware Architecture Vision (scope.md:176), but a casual reader sees a flat contradiction. Recommend an inline footnote on line 238 referencing the carve-out.
- **`docs/3_troubleshooting.md` vs `docs/diagnose_decision_tree.md`** — both cover the same symptoms (ModemManager, USB CDC, port-not-detected). 3_troubleshooting.md is 879 lines flat-list; diagnose_decision_tree.md is 225 lines decision-tree. Neither doc tells the reader **which one to read first**. The decision-tree doc opens with "Pair this with…" (decision_tree:3) but doesn't say "use me first; fall back to 3_troubleshooting.md for depth." Risk: a contributor reads both.
- **`docs/features/calibration-guide.md` (335 lines) vs `docs/2_calibration_guide.md` (380 lines)** — two parallel calibration narratives. Roadmap link goes to the `features/` one (roadmap.md:380); README link goes to the numbered one (README.md:296). No doc explicitly says "these are different audiences, read X for Y." Reader has to diff them to find out.
- **`docs/0_quickstart.md:139`** — "If your IMU is mounted at an angle (e.g., sideways, rotated), use calibrate.sh option **9** or type `o`." The number "9" is brittle — it depends on `tools/calibrate.sh` keeping the menu order stable. If the menu is reordered the doc silently misleads. Better: name the option ("IMU + orientation").
- **`docs/archive/4_readme_original.md`** — entire file's link block (lines 15–37, 8 references) is broken because the project moved to lowercase numbered docs. Archive is allowed to be historical but the broken links are crawler-discoverable and confuse contributors using GitHub's "find file by name" search.

### Well-written passages worth cloning the style of
- **`docs/findings/swarm_api_contract_2026-05-20.md:8–17`** — "Verified 2026-05-20 against …@SHA" block + the explicit "When either side changes, re-stamp the Verified block" instruction. Self-validating doc. Adopt this pattern for any doc that mirrors code.
- **`docs/findings/gps_passthrough_spec_2026-05-20.md:23–55`** — "Framing and the hard scope boundary" section. Restates the relevant `scope.md` constraint, names the scope tension explicitly, scopes the feature inside the carve-out. Should be the template for every future sensor/feature spec.
- **`docs/findings/esp32_gpio_conflict_resolution_2026-05-20.md:104–112`** — Summary table (REAL today / latent / N/A) with one-line verdicts per conflict. Compact, decision-ready. Adopt for any "is this a real bug" disposition doc.
- **`docs/scope.md:147–177`** — Hardware Architecture Vision diagram + the "key principles" bullet list. Sets the load-bearing rules every later spec leans on. Both new sensor specs cite this verbatim — proof the section is doing its job.

---

## 10. Recommendations (prioritized)

LOC estimates are doc-LOC (markdown lines added/changed), not source.

### P0 — Trust-breaking drift; fix this session
1. **Fix test-count drift in 4 places** — change "19 tests" → "18 tests / 42 assertions" in `README.md:284`, `README.md:311`, `docs/README.md:50`, `docs/0_quickstart.md:268`. *LOC: 4. Why: user-visible, immediately checkable, and the wrong number is in the entry-point doc.*
2. **Index today's 5 orphan findings** in `docs/findings/INDEX.md` under the existing "2026-05-20 session" heading — gpio resolution, baro spec, gps spec, swarm contract, this audit. *LOC: ~10 (one row each). Why: without this, half of today's deliverables are not discoverable via the canonical index.*
3. **Fix broken link in `docs/1_hardware_setup.md:618`** — `./CALIBRATION_GUIDE.md` does not exist; should be `./2_calibration_guide.md`. *LOC: 1.*
4. **Update README Documentation table (README.md:293–300)** to list `pid-tuning-guide.md`, `diagnose_decision_tree.md`, `esp32_wifi_onboarding.md`, `1_hardware_setup.md`, `3_troubleshooting.md`. *LOC: ~6.*

### P1 — Drift that misleads a new contributor
5. **Bump `scope.md` header date and add a revision-history entry** for today's MODIFIED edit (whatever it is). Also retire the "fc_tool integration coupling" open question (scope.md:287) — already decided in the Testing Approach row of Technical Decisions (scope.md:259). *LOC: 4–6.*
6. **Add baro/GPS carve-out footnote to `scope.md:238`** ("GPS, barometer, magnetometer — flight computer territory"): one line pointing at `findings/barometer_integration_spec_2026-05-20.md` and `findings/gps_passthrough_spec_2026-05-20.md` as the approved telemetry-only paths. *LOC: 2.*
7. **Add BNO055/BNO085 row to README "Select your IMU" block (README.md:214–216)** mirroring config.h:87–97 — explain Phase A is detect-only. *LOC: 4.*
8. **Reverse-link from `swarm_api_contract_2026-05-20.md` to baro and gps specs** as pending schema additions; add an explicit "Known pending schema additions" subsection in §3 or §7. *LOC: ~10.*
9. **Reverse-link from `barometer_integration_spec_2026-05-20.md` to `esp32_gpio_conflict_resolution_2026-05-20.md`** in the Cross-references block at top (line 9–14). *LOC: 1.*

### P2 — Quality / discoverability
10. **Triage `docs/archive/4_readme_original.md`** — either prepend a "ARCHIVE — broken links inside, see ../README.md" banner, or move into `docs/archive/_quarantine/` so the broken-link surface is gated. *LOC: 3.*
11. **Resolve the calibration-guide duplication** — `docs/features/calibration-guide.md` (335 lines) vs `docs/2_calibration_guide.md` (380 lines). Either explicitly delegate (one is the canonical user guide, the other is the feature spec), or merge. *LOC: small (header rewrite), or large (merge); choose header path first. ~10 LOC.*
12. **Add ESP32-quickstart sibling** to `docs/0_quickstart.md` (or split it into `0_quickstart_teensy.md` + `0_quickstart_esp32.md`) — the current 60-minute quickstart is Teensy-only. ESP32 has the extra WiFi credentials step that today's `docs/esp32_wifi_onboarding.md` covers; quickstart should link it from the prerequisites. *LOC: ~40 (sibling doc) or 5 (link from existing).*
13. **Write an OTA operator runbook** (new `docs/ota_runbook.md`, ~80 LOC): how `USE_OTA` is enabled, how `floppi-XXXX.local --upload-port` works, the security warning from swarm contract §8 item 1, what to do if a drone is stuck armed (you cannot OTA-flash). *LOC: ~80.*
14. **Promote `plans/calibrate-sh-plan.md`** — change its Status header from "Planned" to "Implemented" with a link to roadmap.md:409. *LOC: 3.*
15. **Add a test-running primer** — small (40–60 LOC) doc, either as `docs/testing.md` or as an addition at the top of `tests/README.md` (no such file yet). Cover prerequisites, how to run a single test, how to read `tests/results/`. *LOC: ~50.*

### P3 — Long-tail / discretionary
16. **Add `docs/build_matrix.md`** documenting which `USE_*` flag combinations are routinely built/tested. *LOC: ~30.*
17. **Cross-link `scope.md:291` ("14 findings documents") to the live count** or remove the count entirely. *LOC: 1.*
18. **Add a `findings/` orphan-doc subsection to `findings/INDEX.md`** for the auto-generated `generated/*.md` if those are meant to be discoverable, or explicitly say "auto-generated, see `.researchhub/`". *LOC: ~5.*
19. **Adopt the `swarm_api_contract`-style "Verified <date> against <file>@<sha>" stamp** in any spec doc that mirrors source — baro, gps, future sensor specs. Pattern from `swarm_api_contract_2026-05-20.md:8–13`. *LOC: ~5 per doc.*
20. **Standardize "Last updated" header policy** — current docs are split between header-date and Revision-History-table; scope.md has both and they drifted. Pick one (recommend Revision History only, regenerated from git mtime by a CI job later). *LOC: ~3 per affected doc.*

---

## Notes on this audit's scope

- All claims above are anchored to a `file:line` cite or to git status; nothing was inferred from intuition alone.
- The four 2026-05-20 specs that are not in `findings/INDEX.md` were each read in full to confirm cross-reference and status. They are complete, not stub-sized.
- No file in the audited tree was modified. No source code, no other docs, no tests, no commits.
- The auto-generated `generated/*.md` corpus and the `sources/pdfs/*.pdf` corpus were scope-noted in §1 but not content-audited — they are ResearchHub artifacts, not project docs.
- This document targets the 300–500 line range and the table-heavy / no-long-code-blocks style requested.
