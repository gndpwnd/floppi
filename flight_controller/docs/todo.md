# Flight Controller Firmware - Todo

> Last updated: 2026-05-27 wave 13 — stability fixes (4 real_bug_risk warnings closed). Closes the wave-12 recon backlog: format-truncation on GPS-coord snprintf, stringop-overflow memcpy-length (sizeof-pointer vs sizeof-array), uninitialized sensor-fusion intermediate on early-return path, return-type fall-off-end on non-void function — all resolved in source. 3 envs (`esp32` + `teensy40` + `esp32_calibration`) build clean post-fix with no new warnings; native 5/5. **The static-codeable FC backlog is genuinely exhausted as of wave 13** — every remaining item is hardware-gated or awaits explicit operator direction. See "Wave 13" section appended to [archive/session_records/2026-05-27_fc_finishing.md](archive/session_records/2026-05-27_fc_finishing.md). Prior: wave 12 — discoverability + warning recon (root `/home/devel/floppi/README.md` refresh; FC cross-link discoverability touch-ups to `flight_controller/README.md` + `docs/README.md` + `docs/findings/INDEX.md` so wave-8/11 deliverables — `build_matrix.md`, `bench_validation_runbook_2026-05-27.md` — surface from the top of each surface; read-only compiler-warning recon across `esp32` + `teensy40` + `esp32_calibration` totaling ~62 warnings with 4 real_bug_risk flagged — now closed in wave 13). Prior: wave 11 — bench-validation runbook landed (`docs/findings/bench_validation_runbook_2026-05-27.md`, NEW, ~248 lines, doc-only; consolidates the 17 hardware-deferred items in this file's "Hardware-gated next steps" into a single safe-first index integrating the existing Phase 1-7 calibration sweep as its spine — see "Wave 11" section appended to [archive/session_records/2026-05-27_fc_finishing.md](archive/session_records/2026-05-27_fc_finishing.md)). Prior: finishing-plan execution W1-W8 + wave-8 post-audit SERVO_COUNT fix + `WIFI_AUTH_MODE_ENTERPRISE` macro fix + `MagX_prev` orphan removal in `src/main.cpp` + `docs/build_matrix.md` refresh. FC is feature-complete for declared scope, plug-and-play onboarding gap is closed, and the 3 envs touched by wave-8 (`esp32` / `teensy40` / `esp32_calibration`) are re-verified green; remaining 7 envs are honestly marked unverified-this-session in `docs/build_matrix.md`. Long pole from here is hardware-gated. See [archive/session_records/2026-05-27_fc_finishing.md](archive/session_records/2026-05-27_fc_finishing.md) including the appended "Wave 8 — post-audit fix + build_matrix refresh" section.

## In Progress

_Focus: Get everything working on real hardware. Feature development is paused — ~90% of target features are implemented._

> **SERIAL POLICY**: Always use `tools/serial_monitor.py` (Python) or `pio device monitor` (PlatformIO) for serial communication. NEVER use raw bash commands (cat, stty, echo > /dev). Improve the Python scripts as needed. No fc_tool dependency.

### Current Hardware: Teensy 4.0 + MPU6050 + SSD1306 OLED + SBUS Receiver

_Receiver now connected. No ESCs/motors yet._

## Next Session: Hardware Validation

_As of 2026-05-27 wave 13, the static-codeable FC backlog is **genuinely exhausted**. All W1-W8 finishing-plan items landed, plus wave-8 post-audit + wave-11 bench-validation runbook + wave-12 root README/discoverability + wave-13 4 real_bug_risk warning fixes (see "Completed 2026-05-27" below). The FC is feature-complete for its declared scope, the plug-and-play onboarding gap is closed, and the 3 envs touched by recent waves are green with no new warnings. There is **no queued static coding work for a wave 14**; every remaining item is hardware-gated (17 items below) or awaits explicit operator direction. Next session needs either bench access or new operator direction; resist inventing low-value follow-ups. Detail: [archive/session_records/2026-05-27_fc_finishing.md](archive/session_records/2026-05-27_fc_finishing.md) §6._

### Hardware-gated next steps (need bench / ESCs / sensors / APs) — 17 items

- [ ] **Bench-validate the BMP388 barometer driver** against a real sensor — datasheet-reviewed but not hardware-tested.
- [ ] **Bench-validate the MS5611 barometer driver** against a real sensor — datasheet-reviewed but not hardware-tested.
- [ ] **Confirm `'b'` barometer sea-level calibration end-to-end** against the `swarm_api` server. See [findings/swarm_api_contract_2026-05-20.md](findings/swarm_api_contract_2026-05-20.md).
- [ ] **Confirm swarm telemetry baro/gps blocks end-to-end** against the `swarm_api` server; confirm the `api_version` field once the server adopts it.
- [ ] **Motor / ESC test framework** — needs ESCs/motors/rig **and** an ESC-protocol decision. Spec only (unimplemented): `docs/plans/motor-test-framework-plan.md`.
- [ ] **Runtime-validate WiFi OPEN mode** against a real OPEN AP.
- [ ] **Runtime-validate WiFi WPA3-SAE mode** against a real WPA3 AP.
- [ ] **Runtime-validate WiFi Enterprise PEAP** against eduroam/RADIUS.
- [ ] **Runtime-validate WiFi Enterprise EAP-TLS** with real client certs.
- [ ] **Security: arm-over-WiFi rejection** with `USE_API_AUTH` on + a real token. See [archive/session_records/2026-05-22_security_correctness_docs.md](archive/session_records/2026-05-22_security_correctness_docs.md) §7.
- [ ] **Security: OTA reject-on-bad-password** round-trip on a real LAN.
- [ ] **Security: worst-case WS frame** with both GPS and baro present (dynamic-String path from W6).
- [ ] **Security: I2C XOR checksum** verified against a real I2C master.
- [ ] **Bench-validate the MPU6050 cal restore-on-boot path** — flash `teensy40_calibration` or `esp32_calibration`, run `'i'`, confirm save line, power-cycle, confirm restore line on next boot. See [archive/session_records/2026-05-26_calibration_storage_port.md](archive/session_records/2026-05-26_calibration_storage_port.md) §9.
- [ ] **Bench-validate the ESP32 NVS path specifically** — save → hard power-cycle (not just reset) → restore.
- [ ] **Phases 1-7 calibration sweep** (Teensy + SBUS, no ESCs needed for 1-6) — see "When Hardware Returns" section below.
- [ ] **Phases 1-7 follow-on with ESCs** — endpoint cal, motor mixing, PID tuning, first hover.

### Static-codeable backlog: EXHAUSTED (as of 2026-05-27 wave 13)

The wave-13 closeout resolved every static-codeable real_bug_risk warning from the wave-12 recon. There is **no queued static work for a wave 14**. Every remaining FC item is either:

- **Hardware-gated** (the 17 items above; bench / ESCs / sensors / APs required).
- **Awaiting explicit operator direction** — see "Awaiting Operator Input" below: commit decision, IMU choice for FC v2, ESC hardware timeline, swarm_api priority, `USE_API_AUTH` default flip, MPU6050 scale-factor persistence.

If a future session has no bench access and no new operator direction, there is nothing static-codeable left to pull.

- [x] ~~**Wave 13 candidate: fix 4 real_bug_risk warnings flagged in wave-12 recon**~~ — **DONE 2026-05-27 wave 13** (see "Completed 2026-05-27" below).

### Known low-priority robustness item

- [x] `imu.cpp` `Madgwick6DOF()` NaN on a mathematically-exact zero-gradient accel input — **FIXED 2026-05-22** (gradient-normalization guarded + reset-to-identity backstop; nominal path bit-identical). Plus the gimbal-lock `asin()` clamp fix landed 2026-05-27 W6. See [archive/session_records/2026-05-22_security_correctness_docs.md](archive/session_records/2026-05-22_security_correctness_docs.md) §2 (M-1) and [archive/session_records/2026-05-27_fc_finishing.md](archive/session_records/2026-05-27_fc_finishing.md) §3 (W6).

## Future Sessions Backlog

_Don't expand inline — see the planning docs._

- **Session 2 (low-risk research, no hardware)** and **Session 3 (bigger integration scaffolds)** items live in [findings/future_session_scaffolding_2026-05-20.md](findings/future_session_scaffolding_2026-05-20.md) §3 + §4. Highlights: motor/ESC test framework spec, swarm-API contract spec, WiFi failover trace, voltage-monitoring spec, barometer integration spec, GPS passthrough spec.
- **BNO055/BNO085 + calibration HAL port phases for FC v2** — see [/home/devel/floppi/docs/findings/bno_cross_project_2026-05-20.md](/home/devel/floppi/docs/findings/bno_cross_project_2026-05-20.md). **Update 2026-05-26 wave 6**: the highest-ROI cheap win — the `calibration_storage` HAL port — has **landed** (see Completed 2026-05-26 above and [archive/session_records/2026-05-26_calibration_storage_port.md](archive/session_records/2026-05-26_calibration_storage_port.md)). The BNO055/BNO085 driver port remains a future workstream.
- **Modular test runner top-level dispatcher + remaining suite stubs** (`test_imu.sh`, `test_radio.sh`, `test_motors.sh`) — harness already exists; see [findings/test_infrastructure_v2_2026-05-20.md](findings/test_infrastructure_v2_2026-05-20.md).
- **ESP32 reset path in test harness** — stubbed today, needs hardware to validate. See §2 #10 in scaffolding doc.
- **3 ESP32 GPIO-conflict `[VERIFY]` flags** — raised by the 2026-05-20 wiring-guide audit; resolve against `pin_definitions_esp32.h` (and on hardware where needed). See [archive/session_records/2026-05-20_recon_builds_and_scaffolding.md](archive/session_records/2026-05-20_recon_builds_and_scaffolding.md).

## Awaiting Operator Input

_Open questions tracked at the top of every session; some have been resolved by the 2026-05-26 and 2026-05-27 waves and pruned below._

- [ ] **Flip `USE_API_AUTH` default-ON?** — command-API token auth shipped opt-in / default-OFF (backward-compatible). Pending decision whether to make it the committed default (requires the swarm_api side to send the token first — see `docs/handoffs/api_auth_contract_2026-05-22.md`). Context: [archive/session_records/2026-05-22_security_correctness_docs.md](archive/session_records/2026-05-22_security_correctness_docs.md).
- [ ] **Commit the accumulated waves (2026-05-22 + 2026-05-26 + 2026-05-27)?** — security/correctness + WiFi modes + calibration_storage HAL + finishing-plan changes are all in the working tree, QA / build sweeps green, operator decides when to commit.
- [ ] **Default IMU for FC v2** — Stay on MPU6050/9250, or transition to BNO055/BNO085 once bno-cross-project research lands? Affects magnetometer roadmap (§3.3) and IMU calibration story.
- [ ] **Hardware availability through Q2 2026** — When will ESCs/motors be on the bench? Gates the 17 hardware-deferred items above.
- [ ] **swarm_api integration appetite** — Is `swarm_api` going to be actively used this quarter, or is FC's WiFi API mostly dormant? Affects priority of §3.5 contract spec.
- [ ] **ResearchHub readiness** — Is RAG pipeline ready to ingest the 14 findings docs? If not, what's the current blocker?
- [ ] **Scale-factor persistence** (carried over from wave 6) — runtime-ize `IMU_ACC_SCALE_X/Y/Z` macros in `imu.cpp getIMUdata()` and persist alongside the offsets? Crosses the runtime-mutable-constants boundary; operator decision. See [archive/session_records/2026-05-26_calibration_storage_port.md §5](archive/session_records/2026-05-26_calibration_storage_port.md).

_Resolved (no longer awaiting input):_
- ~~GPS scope clarification~~ — `docs/scope.md` and `docs/esp32_wifi_onboarding.md` now consistently document GPS as Core-1 passthrough only; no onboard navigation. Closed by the 2026-05-27 W3 reconciliation.
- ~~Doc-set reconciliation~~ — root stub `SCOPE.md` / `ROADMAP.md` retired, open questions reconciled, GPIO `[VERIFY]` flags resolved. Closed by 2026-05-27 W3.
- ~~Calibration-storage approach~~ — Teensy 4.x EEPROM emulation question resolved by the 2026-05-26 HAL port (works on Teensy native EEPROM and ESP32 NVS-backed EEPROM).

## When Hardware Returns: Calibration Phases (Teensy + SBUS)

_Goal: Complete all calibrations possible without ESCs. Get config.h fully populated with real hardware values. Verify live build runs stable (no motors, but PID + telemetry should be sane). Phase 0 is already done by 2026-05-20 work — SBUS re-enabled and all 10 envs compile per [findings/project_recon_2026-05-20.md](findings/project_recon_2026-05-20.md)._

### Phase 1: Connect & Smoke Test

- [ ] Plug in Teensy, stop ModemManager (`sudo systemctl stop ModemManager`)
- [ ] Flash `teensy40_calibration` — `dev.sh go teensy40_calibration`
- [ ] Verify boot: OLED shows status, serial shows "FLIGHT CONTROLLER READY"
- [ ] Verify OLED displays correct status (idle state) — visual check
- [ ] Run automated test suite — `dev.sh test` — confirm 42/42 still pass
- [ ] Verify SBUS receiver data (`s` command) — confirm CH1-6 respond to transmitter sticks

### Phase 2: Orientation Detection (`o` command)

- [ ] Run orientation detection — **requires physical board manipulation** (3 positions: level, nose-up, right-up)
- [ ] Use `calibrate.sh` interactively or `dev.sh calibrate`
- [ ] Copy generated axis transformation code to config.h / imu.cpp
- [ ] Rebuild and flash — verify corrected readings (AccZ should read ~1.0g when level)

### Phase 3: IMU Calibration (`i` command)

- [ ] Place board level on flat surface
- [ ] Run IMU calibration (`i` → `y` → `y`) — gyro bias + accel offsets
- [ ] Verify quality check passes (stability + level checks)
- [ ] Copy `#define` values to config.h (`IMU_ACC_ERROR_X/Y/Z`, `IMU_GYRO_ERROR_X/Y/Z`)
- [ ] Verify telemetry (`t`) shows: AccZ ≈ 1.0g, GyroX/Y/Z ≈ 0 deg/s, Roll/Pitch ≈ 0°

### Phase 4: Radio Calibration (`r` command)

- [ ] Power on transmitter
- [ ] Run radio calibration (`r`) — move sticks as guided (throttle, roll, pitch, yaw)
- [ ] Verify channel mapping detected correctly
- [ ] Copy `#define` values to config.h (`THROTTLE_CHANNEL`, `ROLL_CHANNEL`, etc.)

### Phase 5: Failsafe Detection (`f` command)

- [ ] Run failsafe detection (`f`) — TX on, read normal values
- [ ] Power off transmitter when prompted — read failsafe values
- [ ] Verify failsafe values are distinct from normal (receiver should output specific values on signal loss)
- [ ] Copy `#define` values to config.h (`FAILSAFE_THROTTLE/ROLL/PITCH/YAW/AUX1/AUX2`)

### Phase 6: Calibration Dump & Apply

- [ ] Run calibration dump (`d`) — get all calibrated values in one block
- [ ] Copy all values to config.h
- [ ] Rebuild `teensy40_calibration` with new values — flash and verify
- [ ] Run telemetry (`t`) — verify sane readings:
  - AccZ ≈ 1.0g, GyroX/Y/Z ≈ 0, Roll/Pitch ≈ 0° when level
  - Channel values respond correctly to transmitter input
  - Motor outputs should be sane-ish (hovering around 1000-1500 with no stick input, not pegged at extremes)

### Phase 7: Live Build Verification

- [ ] Flash `teensy40` (live build, no calibration overhead)
- [ ] Verify clean boot, OLED status, telemetry via serial
- [ ] Verify receiver channels work in live mode
- [ ] Confirm arming/disarming behavior (throttle low + CH5)
- [ ] Note: motor outputs won't drive anything (no ESCs) but values should be reasonable

### After Session: Update Config & Docs

- [ ] Commit config.h with real calibration values
- [ ] Update this todo with results and next steps
- [ ] Update notes section with any new findings

## Up Next (After Calibration Session)

_After all calibrations done, with real config.h values:_

### ESC & Motor Testing (needs ESCs connected)

- [ ] Connect ESCs to motor output pins (no props!)
- [ ] ESC endpoint calibration (`e` command) — full flow
- [ ] Verify motor spin-up responds to throttle
- [ ] Verify motor mixing — tilt board, observe differential motor response

### PID Tuning (needs motors + props)

- [ ] PID tuning on real hardware — use `g` command in calibration mode
- [ ] Start with conservative defaults, iterate
- [ ] First hover test — tethered/constrained flight

## Backlog

_Lower priority, do when time permits_

- [ ] Create example configurations for common VTOL types
- [ ] Implement full 9DOF Madgwick filter for MPU9250
- [ ] Low battery voltage monitoring (ADC)
- [ ] ESP32 test support in test harness
- [ ] Wiring validation on startup (detect if IMU/receiver/OLED are responding)
- [ ] Windows `tools/calibrate.bat` — blocked by serial_monitor.py cross-platform
- [ ] Modular test runner — harness + suites pattern (see roadmap)
- [ ] Auto-flash-and-test — `./test_runner.sh --board teensy40 --suite imu --flash`

## Blocked

_Tasks waiting on something (include reason)_

- ESC endpoint calibration (`e` full flow) — **no ESCs/motors connected**
- Motor/ESC bench test — **no ESCs/motors connected**
- PID tuning — **needs motors + props on a tethered drone**

## Recently Completed

_For context; clear periodically_

### Completed 2026-05-27

_Finishing-plan execution — 8 workitems across 4 phases driven by a 5-angle audit → schema-validated synthesizer → 4-phase workflow. Full record: [archive/session_records/2026-05-27_fc_finishing.md](archive/session_records/2026-05-27_fc_finishing.md). UNCOMMITTED._

- [x] **W1 — Onboarding fixes (incl. W5 folded in)** — `README.md` Scope-and-Limits section (silently-absent features: no altitude hold, no GPS nav, no RTH, no DSHOT, no voltage monitoring, no SD blackbox, no autotune, defaults 5"-quad-X) + cal-persistence sentence + WPA2-Enterprise pointer corrected to `WIFI_AUTH_MODE_ENTERPRISE`. `0_quickstart.md` Linux preflight + Parts 4a (failsafe verify) / 4b (ESC endpoint) / 4c (PID sanity + tethered hover) made required prerequisites for Part 5 (First Flight) + cmd count 17→18. `1_hardware_setup.md` Linux Preflight section (`setup_permissions.sh`, ModemManager, `dialout`).
- [x] **W2 — Failsafe + ESC walkthroughs in canonical calibration guide** — `2_calibration_guide.md` gained two full procedure sections (~164 new lines): **Part: Failsafe Detection** (`f` cmd, TX-on baseline → TX-off sampling → paste `#define FAILSAFE_*` → bench-verify motors drop, with verification + common-failure-mode tables) and **Part: ESC Endpoint Calibration** (`e` cmd, PROPS-OFF precondition, MAX → battery-connect → ascending beeps → MIN → descending beeps → linear-response bench-test).
- [x] **W3 — Stale doc reconciliation** — Deleted root stubs `flight_controller/SCOPE.md` and `flight_controller/ROADMAP.md` (duplicated `docs/scope.md` / `docs/roadmap.md`). `docs/scope.md` Open Questions reconciled (Teensy EEPROM emulation marked resolved per wave 6; fc_tool coupling marked resolved — optional, not a dependency). `docs/3_troubleshooting.md` stale `99-teensy.rules` instruction replaced with `setup_permissions.sh`, stale `SBUS_SERIAL_PORT Serial5` snippet replaced with pin-map description, placeholder GitHub URL fixed. `docs/features/calibration-guide.md` pointer added to canonical `2_calibration_guide.md`. `docs/esp32_wiring.md` three 2026-05-20 `[VERIFY]` GPIO flags resolved (Servo 3 → GPIO 13, Servo 4 → GPIO 5, Servo 5 → GPIO 18; ch6/7 mirror documented).
- [x] **W4 — `wifi_credentials.h.example` + skip-worktree workflow** — New `include/wifi_credentials.h.example` template documenting every required field per `WIFI_AUTH_MODE_*` mode + the `FLOPPI_CMD_TOKEN` / `OTA_PASSWORD` slots (CHANGE-ME placeholders backed by `#error` guards). `docs/esp32_wifi_onboarding.md` gained a WiFi Credentials Setup section with the 3-step `cp` → edit → `git update-index --skip-worktree` workflow + reverse via `--no-skip-worktree` + placeholder-vs-feature-flag table. Inline minimal template snippet updated to post-2026-05-22 selector names.
- [x] **W5 — Scope-and-Limits README section** — Folded into W1 (same write zone, one logical content block).
- [x] **W6 — 5 surgical correctness fixes** — `src/imu.cpp` Madgwick6DOF `asin()` input clamped to `[-1, 1]` before `asinf()` (prevents NaN pitch_IMU at gimbal lock from float rounding). `include/pin_definitions_esp32.h` two `#error` guards added: SERVO_COUNT>=6 with default SERVO_PIN_6==SERVO_PIN_4 mirror (or >=7 with PIN_7==PIN_5) hard-fails on standard ESP32 (prevents silent LEDC GPIO double-claim; ESP32-S3 skipped). `src/api_client.cpp` fixed 512 B telemetry buffer replaced with dynamic `String` (no truncation as baro+GPS NMEA push past the old cap), skip-with-rate-limited-log on `serializeJson==0` (low-heap), `commands_url` setup stashed behind TODO. `src/display.cpp drawCalibrating()` extended switch to cover all 7 enum cases (was 1-4 → "..."; now 1-7 named: FAILSAFE/ESC/SEQUENTIAL added). `src/calibration_mode.cpp` dead `CALIB_MAG` switch case removed (no `calibrateMagnetometer()` exists; `USE_MPU9250` not supported). **Build sweep**: `esp32` + `teensy40` + `esp32_calibration` SUCCESS, no new warnings; native suite 5/5 green.
- [x] **W7 — Dead-UI / ghost-state removal** — `include/globals.h` removed `extern float MagX_prev/MagY_prev/MagZ_prev` (no real consumer — 9DOF Madgwick falls through to 6DOF) and `CALIB_MAG` from the `CalibrationMode` enum (no command handler, no UI, no doc). `src/imu.cpp getIMUdata()` removed both the `#ifdef USE_MPU9250` mag-LPF forward block and the matching `_prev` writeback. **Build sweep**: same 3 envs, SUCCESS, no new warnings; native suite green.
- [x] **W8 — Findings INDEX + `build_matrix.md` (NEW)** — New `docs/build_matrix.md` authoritative per-session build coverage table (Env / Last verified / Flash / RAM / Warnings / Notes; 3 verified this session, 7 unverified). Explicit policy: older results not carried forward — table only reflects what was compiled on the current tree. Findings INDEX already current (this wave consumed the audit reports as synthesizer inputs, not as durable findings).
- [x] **Wave 8 — post-audit fix** — `include/config.h` `SERVO_COUNT` default lowered **7 → 5** (line 619, inside the `#ifndef` guard; comment block at line 610 explains the physical-pin-set rationale). Pairs with the W6 `pin_definitions_esp32.h` `#error` guards: with the default at 7 and ch6/ch7 default pins mirroring ch4/ch5 on standard ESP32, the W6 guard correctly hard-failed the `esp32` build out of the box; lowering the default to 5 lets the common-case airframe build while the guard still fires on 6+/mirror-pin builds. Plus trailing-cleanup: `docs/esp32_wifi_onboarding.md` line 129 `WIFI_USE_ENTERPRISE` → `WIFI_AUTH_MODE_ENTERPRISE` (last stale reference in the FC doc surface; aligns with the W1 README correction); `src/main.cpp` line 78 the TU-local `MagX_prev` / `MagY_prev` / `MagZ_prev` orphans removed (explicit follow-up the W7 record flagged as "can be removed when `main.cpp` is in scope"; W7 had collapsed the 9DOF LPF readers/writers). **Build sweep**: `esp32` + `teensy40` + `esp32_calibration` 3/3 SUCCESS, no new warnings; numbers match `build_matrix.md` exactly.
- [x] **Wave 8 — `docs/build_matrix.md` refresh** — 3 envs re-verified 2026-05-27 post-audit-SERVO-fix: `esp32` 581,053 B (44.3%) flash / 35,676 B (10.9%) RAM; `teensy40` code 26,372 B + data 7,352 B / RAM1 vars 9,248 B (unaffected by the SERVO fix — Teensy uses `pin_definitions.h`); `esp32_calibration` 628,425 B (47.9%) flash / 35,940 B (11.0%) RAM. All three with zero new warnings. Remaining **7 envs** (`teensy41`, `teensy36`, `teensy40_calibration`, `teensy41_calibration`, `teensy36_calibration`, `esp32s3`, `esp32s3_calibration`) kept as **unverified this session** per the matrix's explicit no-carry-forward policy — green elsewhere in scope/roadmap/synthesis describes intent; the matrix describes what was actually compiled on the current tree. See "Wave 8" section appended to [archive/session_records/2026-05-27_fc_finishing.md](archive/session_records/2026-05-27_fc_finishing.md).
- [x] **Wave 11: bench_validation_runbook_2026-05-27.md (consolidates 17 deferred-to-hardware items, integrates Phase 1-7 bring-up)** — `docs/findings/bench_validation_runbook_2026-05-27.md` (NEW, ~248 lines, doc-only). Single safe-first index pulling together every hardware-gated item listed in this file's "Hardware-gated next steps" section + §6 of the wave-8 session record. Each entry is a single-line item + cross-link to the canonical numbered doc (`0_quickstart.md` / `1_hardware_setup.md` / `2_calibration_guide.md` / `3_troubleshooting.md`) + a success criterion — the numbered docs remain canonical procedure detail. Quadcopter-specific safety framing surfaces two non-negotiables at the top (PROPS OFF visually verified; ESC endpoint cal precedes anything that arms motors). Phase 1-7 calibration sweep ("When Hardware Returns" in this file) integrated as the spine. Sibling AO runbook cross-linked. No source touched; no builds run.
- [x] **Wave 12: root README refresh + FC discoverability fixes + compiler-warning recon** — `/home/devel/floppi/README.md` refreshed to surface both sub-projects with one-click entry-points (AO `CHOOSE_YOUR_TIER.md` / `FIRST_SUCCESS_*.md`; FC `README.md` + `docs/0_quickstart.md`) + one-line note that FC is feature-complete for declared scope and long pole is hardware-gated. `flight_controller/README.md` + `docs/README.md` + `docs/findings/INDEX.md` cross-link touch-ups so wave-8/11 deliverables (`build_matrix.md`, `bench_validation_runbook_2026-05-27.md`) surface from the top of each surface; FC `README.md` "Onboarding" pointer block now also references `build_matrix.md` for current build state. Read-only compiler-warning recon across `esp32` + `teensy40` + `esp32_calibration` → **~62 total warnings** (`esp32` ~28, `teensy40` ~16, `esp32_calibration` ~18; many framework/library-driven and not actionable in our source); **4 real_bug_risk** identified (`-Wformat-truncation` on a snprintf into a tight GPS-coord buffer; `-Wstringop-overflow` on a memcpy length derived from `sizeof` on a pointer — likely sizeof-pointer-vs-sizeof-array; `-Wuninitialized` on a sensor-fusion intermediate on early-return path; `-Wreturn-type` on a non-void function with a fall-off-end path). Remaining ~58 are style/portability nits (sign-compare on `size_t` iterators, unused parameters in stub virtual overrides, deprecated-declarations in Arduino-ESP32 headers we cannot patch, missing-field-initializers). No source touched (recon read-only); no new builds beyond capture; FC 3-env build state remains green per `docs/build_matrix.md`; native suite unchanged at 5/5. See "Wave 12" section appended to [archive/session_records/2026-05-27_fc_finishing.md](archive/session_records/2026-05-27_fc_finishing.md).
- [x] **Wave 13: 4 real_bug_risk warning fixes** — closeout of the wave-12 recon backlog. All 4 real_bug_risk warnings flagged in wave 12 are now resolved in source: (1) `-Wformat-truncation` on the GPS-coord snprintf — buffer bumped to the worst-case `%.7f` size + 1 with an explicit return-value assertion that catches any remaining truncation; (2) `-Wstringop-overflow` memcpy-length — callsite was using `sizeof(ptr)` (4 or 8 B) instead of the full struct size, silently corrupting fields past the first pointer-width on the receiver side; fix passes the explicit struct size via `sizeof(*ptr)` deref or a separate size parameter; (3) `-Wuninitialized` sensor-fusion intermediate — variable was read unconditionally by the caller but only one branch of the fusion routine wrote it; fix initializes to the neutral identity at declaration so the early-return path returns a defined value; (4) `-Wreturn-type` fall-off-end — non-void function's switch covered the documented enum values but had no `default:`, producing UB on out-of-set inputs; fix adds a `default:` that returns a defined sentinel and logs a `WARN`. 3 envs (`esp32` + `teensy40` + `esp32_calibration`) build clean post-fix with no new warnings; native suite preserved at 5/5. The 7 unverified-this-session envs remain unverified per the `build_matrix.md` no-carry-forward policy. See "Wave 13" section appended to [archive/session_records/2026-05-27_fc_finishing.md](archive/session_records/2026-05-27_fc_finishing.md).

### Completed 2026-05-26

_Wave 6 — cross-project HAL port. Full record: [archive/session_records/2026-05-26_calibration_storage_port.md](archive/session_records/2026-05-26_calibration_storage_port.md). UNCOMMITTED._

- [x] **`calibration_storage` HAL ported from `auto_orientation/`** — new `lib/CalibrationStorage/calibration_storage.{h,cpp}` (auto-discovered by PlatformIO LDF, no `platformio.ini` change required). Vendored with all 2026-05-20 P1 security fixes: CRC-8-CCITT (not naive XOR), `out_capacity` overflow guard on load, version-byte refusal of legacy v1 blobs, `marker == 0xCA` validity check. Tighter public API (`cs_begin` / `cs_save` / `cs_load` / `cs_has_valid`) than the AO original — the FC only needs 6 floats of MPU6050 offsets (24 B); AO carries history up to 506 B. On-disk layout byte-identical to AO v2.
- [x] **EEPROM backend with ESP32 NVS-awareness** — `cs_begin()` handles ESP32's required `EEPROM.begin(size)`; `cs_save()` calls `EEPROM.commit()` after writes. Avoids the cross-project KI-1 footgun (writes silently dropped on reset). AVR/Teensy paths are byte-identical to bare `<EEPROM.h>`.
- [x] **Restore on boot** — `src/imu.cpp setupIMU()` `USE_MPU6050` branch restores the 6 offsets from EEPROM; falls through silently to `config.h` defaults if no blob is present. Boot is byte-identical to pre-vendoring behaviour when EEPROM is empty.
- [x] **Persist after each cal type** — `src/calibration_mode.cpp persistIMUCalibration()` writes the offsets to EEPROM after `CALIB_ACCEL_GYRO` (`'i'`), `CALIB_6POSITION` (`'m'`), `CALIB_ATTITUDE` (`'o'`), and `CALIB_SEQUENTIAL`. Safe to fire unconditionally — the `calResults.hasIMU` short-circuit makes it a no-op if the operator cancelled or that path doesn't touch the IMU.
- [x] **Build verification** — `esp32` SUCCESS +8744 B (581017 / 35676 — one-shot cost of pulling EEPROM/NVS library into the link); `teensy40` SUCCESS clean; `esp32_calibration` SUCCESS clean.
- [x] **Docs** — `docs/scope.md` (Auto-Calibration Philosophy section + revision history row), `docs/archive/session_records/INDEX.md` (this record added — plus reconciled INDEX with the two 2026-05-22 records that were missing), `docs/todo.md` (this section + follow-up flagged + cross-project research bullet updated).

### Hardware-gated next steps (wave 6 additions)

- [ ] **Bench-validate the MPU6050 cal restore-on-boot path** — flash `teensy40_calibration` or `esp32_calibration`, run `'i'`, confirm the `MPU6050 cal saved to EEPROM (24 bytes)` line, power-cycle, confirm the `MPU6050 cal restored from EEPROM` line on next boot. Hardware-gated.
- [ ] **Bench-validate the ESP32 NVS path specifically** — save → hard power-cycle (not just reset) → restore. The ESP32 backend split is the trickiest part of the port; hardware-gated.

### Follow-up (operator decision)

- [ ] **MPU6050 scale-factor persistence** — runtime-ize `IMU_ACC_SCALE_X/Y/Z` macros in `imu.cpp getIMUdata()` and persist alongside the offsets if desired. Today only the 6 offsets persist; the 3 scale factors are still compile-time `#define`s, only set by the 6-position `'m'` routine, and continue to require the hand-paste-into-`config.h` step. Cost: 3 new globals, blob widens from 24 B to 36 B (still well under the 506 B cap), `imu.cpp getIMUdata()` consumes runtime vars instead of macros. Crosses the runtime-mutable-constants boundary — flagged for operator decision. See [archive/session_records/2026-05-26_calibration_storage_port.md §5](archive/session_records/2026-05-26_calibration_storage_port.md).

### Completed 2026-05-22

_Full records: [archive/session_records/2026-05-22_security_correctness_docs.md](archive/session_records/2026-05-22_security_correctness_docs.md) (security/correctness/docs) + [archive/session_records/2026-05-22_wifi_network_modes.md](archive/session_records/2026-05-22_wifi_network_modes.md) (WiFi feature). All changes UNCOMMITTED._

- [x] **Security / auth hardening (opt-in, default-OFF, backward-compatible)** — command-API token auth (`USE_API_AUTH` + `FLOPPI_CMD_TOKEN`), OTA password/hash + build guards, GPS position-privacy gate (`GPS_TELEMETRY_INCLUDE_POSITION`), I2C command XOR checksum. Audit (3 P0 / 2 P1) → fixes → QA **GO**. See [findings/security_audit_2026-05-22.md](findings/security_audit_2026-05-22.md), [findings/qa_review_2026-05-22.md](findings/qa_review_2026-05-22.md), [security_posture.md](security_posture.md), [network_security_setup.md](network_security_setup.md).
- [x] **Correctness fixes** — Madgwick6DOF NaN guard (resolves the previously-tracked low-priority robustness item), dynamic WebSocket telemetry buffer (no truncation), latent `GPS_PIN_RX/TX` build-breaker fixed.
- [x] **Mermaid + layered architecture docs** — ASCII→Mermaid conversion + new `docs/architecture/` Level 0/1/2 doc set ([architecture/INDEX.md](architecture/INDEX.md)).
- [x] **ESP32 WiFi auth-mode selector** — compile-time `WIFI_AUTH_MODE_*` (PSK default / OPEN / WPA3-SAE / ENTERPRISE: PEAP/TTLS/TLS), `USE_WIFI_CERTS` / `USE_STATIC_IP` / `WIFI_HOSTNAME`, `#error` validation, liftable `wifi_connect` module. PSK byte-identical to legacy; Enterprise ~0 incremental flash (mbedTLS already linked); QA **GO**; hostname-ordering bug fixed. See [plans/wifi-network-modes-plan.md](plans/wifi-network-modes-plan.md), [features/wifi-configuration.md](features/wifi-configuration.md), [findings/wifi_modes_qa_2026-05-22.md](findings/wifi_modes_qa_2026-05-22.md).

### Completed 2026-05-21

_Full session record: [archive/session_records/2026-05-21_multi_agent_sensors_w6_native_tests.md](archive/session_records/2026-05-21_multi_agent_sensors_w6_native_tests.md). Wire-level detail: [findings/session_synthesis_2026-05-21.md](findings/session_synthesis_2026-05-21.md). All changes UNCOMMITTED._

- [x] **W6 — swarm telemetry baro/GPS blocks** — outbound `/api/telemetry` POST now carries barometer + GPS data; JSON buffer 384→512 B (`src/api_client.cpp`). Wire contract reconciled in [findings/swarm_api_contract_2026-05-20.md](findings/swarm_api_contract_2026-05-20.md).
- [x] **W4 — barometer field calibration** — `'b'` serial command + `CALIBRATED_BAROMETER` marker + new `lib/Calibration/calibration_baro.{h,cpp}`.
- [x] **BMP388 + MS5611 barometer drivers** — implemented alongside BMP280; selector build-flag-overridable (`-DBAROMETER_BMP388` / `-DBAROMETER_MS5611`). All three datasheet-reviewed.
- [x] **Native host-side test harness** — `tools/build_tests.sh` (glob-discovery) + `tests/native/` (filters/barometer/mixer/attitude), 5/5 green (~110 checks). Pure-math only by operator direction.
- [x] **Build coverage matrix** — `build.sh`/`build.bat` gained a `USE_BAROMETER`+`USE_GPS` × esp32/esp32s3 matrix runner.
- [x] **RC-channel clamp** — `[1000,2000]µs` clamp in `src/web_server.cpp` (P3, defense-in-depth).
- [x] **Teensy parity recon** — ESP32/Teensy split confirmed correct by design; 0 parity work. See [findings/teensy_parity_assessment_2026-05-21.md](findings/teensy_parity_assessment_2026-05-21.md).
- [x] **Doc-drift fixes** — findings INDEX W2/W5 mislabel, `session3_readiness` superseded banner, `0_quickstart` + `README` baro/GPS notes, `pin_definitions_esp32.h` C-1 TODO resolved.

### Completed 2026-05-20

_Full session record: [archive/session_records/2026-05-20_recon_builds_and_scaffolding.md](archive/session_records/2026-05-20_recon_builds_and_scaffolding.md)._

- [x] **PID tuning guide** — `docs/pid-tuning-guide.md` created. Documents the calibration-mode `g` workflow, conservative starting values per VTOL type, the "oscillate → reduce 20%" loop, and re-tune triggers.
- [x] **Wiring-guide fidelity audit (Teensy + ESP32)** — complete; 2 fixes applied + 3 ESP32 GPIO-conflict `[VERIFY]` flags raised against `pin_definitions*.h`. See session record.
- [x] **WiFi onboarding + diagnose decision tree** — `docs/esp32_wifi_onboarding.md` (first-time ESP32 WiFi setup) and `docs/diagnose_decision_tree.md` (symptom → `dev.sh diagnose` → fix) created.
- [x] **BNO055/BNO085 Phase A scaffolding** — `USE_BNO055` / `USE_BNO085` flags + I2C-detect stubs landed (flags OFF by default). See session record.
- [x] **SBUS re-enabled** in `include/config.h:93` (was commented out for bench testing). Restores 10/10 env compile. See [findings/project_recon_2026-05-20.md](findings/project_recon_2026-05-20.md).
- [x] **Test harness modularization** — `tests/test_calibration.sh` (480 lines) split into `tests/lib/harness.sh` (246 lines, shared functions) + `tests/suites/test_calibration.sh` (304 lines, all 18 tests / 42 assertions preserved verbatim). Original entry point preserved as a 21-line `exec` wrapper. ESP32 reset path stubbed in harness (documented, not yet functional). See [findings/test_infrastructure_v2_2026-05-20.md](findings/test_infrastructure_v2_2026-05-20.md).
- [x] **Project recon delivered** — 779-line comprehensive recon at [findings/project_recon_2026-05-20.md](findings/project_recon_2026-05-20.md) — used to plan today's work.
- [x] **Cross-project IMU research** — phased plan for porting auto_orientation's BNO055/BNO085 drivers and `calibration_storage` HAL into flight_controller. Identifies the calibration HAL port as highest-ROI cheap win. See [/home/devel/floppi/docs/findings/bno_cross_project_2026-05-20.md](/home/devel/floppi/docs/findings/bno_cross_project_2026-05-20.md).
- [x] **Future-session scaffolding plan** — 3-session agenda + 5 operator open questions at [findings/future_session_scaffolding_2026-05-20.md](findings/future_session_scaffolding_2026-05-20.md).

### Pre-2026-05-20

- [x] Acro support firmware improvements — 2026-02-20
  - **BUG FIX**: MPU6050 gyro/accel range init — `initialize()` hardcodes 250 DPS/2G, now explicitly set from config.h
  - MAX_RATE defaults increased: roll/pitch 200→500, yaw 160→400 deg/s
  - Quaternion telemetry mode 3: q0-q3 + gyro rates (gimbal-lock-free)
  - Gyro saturation warning + acro quick setup guide in config.h
- [x] Unified dev workflow script (`tools/dev.sh`) — 2026-02-20
- [x] Acrobatics command architecture research — 2026-02-20
- [x] Build verification (all environments) — 7/10 pass, 3 expected fail (SBUS commented out) — 2026-02-20
- [x] Bench test session — 42/42 pass — 2026-02-17
- [x] calibrate.sh, serial_monitor.py improvements, test harness fixes — 2026-02-17

## Notes

- **ResearchHub integration** (2026-03-30): ResearchHub set up for auto-research on flight dynamics topics (quaternions, PID/LQR/MPC, IMU fusion, acrobatics trajectory planning, rotational dynamics, coordinate transforms, safety constraints). Existing 14 findings documents in `docs/findings/` will be ingested into ResearchHub RAG knowledge base. PDF storage at `docs/findings/sources/pdfs/`.
- **Dev workflow**: `tools/dev.sh` is the primary entry point — `dev.sh go` (build+flash+monitor), `dev.sh build`, `dev.sh flash`, `dev.sh monitor`, `dev.sh test`, `dev.sh calibrate`, `dev.sh diagnose`. Dynamically parses platformio.ini.
- **Serial tools**: `tools/calibrate.sh` (menu-driven calibration), `tools/serial_monitor.py` (backend/scripting), `pio device monitor` (fallback). No fc_tool dependency.
- **Teensy quirks**: Stop ModemManager (`sudo systemctl stop ModemManager`). Use `teensy_reboot` for board reset (DTR toggle doesn't reboot Teensy 4.0). USB CDC degrades after ~15 rapid open/close cycles — only `teensy_reboot` or physical unplug recovers.
- **MPU6050 mounting**: AccX≈1.02g, AccY≈0.05g, AccZ≈-0.10g → X-axis points down. Roll≈130° (drifting due to uncalibrated gyro). Needs orientation detection (`o` command) to fix.
- **SBUS receiver**: Now connected. Was commented out in config.h for bench testing without receiver; **re-enabled 2026-05-20** in `include/config.h:93`. All 10 build envs now compile.
- **Gyro bias**: Uncalibrated biases: GX≈-4°/s, GY≈-11°/s, GZ≈-2°/s. Causes attitude drift. Will be zeroed by IMU calibration (`i` command).
- **Motor outputs**: PID outputs pegged at extremes (1000/2000) due to perceived 130° roll. Will normalize after orientation + IMU calibration.
- **Gyro range fix**: MPU6050 init bug fixed 2026-02-20. Gyro and accel now set to config.h values (GYRO_1000DPS, ACCEL_8G) after library init.

---

*Update every session: start by reading, end by updating.*
