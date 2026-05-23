# Findings — Index

Research notes, audits, recon reports, and design documents for the flight_controller project. Dated files use `YYYY-MM-DD` suffixes; undated files are standing reference research.

## 2026-05-20 session

| Document | Summary |
|---|---|
| [project_recon_2026-05-20.md](./project_recon_2026-05-20.md) | First comprehensive map of the FC project (779 lines); build matrix — 7/10 passing pre-session, 10/10 after SBUS re-enable. |
| [test_infrastructure_v2_2026-05-20.md](./test_infrastructure_v2_2026-05-20.md) | Test harness modularization design — `test_calibration.sh` split into `lib/harness.sh` + `suites/test_calibration.sh`; 18 tests / 42 assertions preserved. |
| [future_session_scaffolding_2026-05-20.md](./future_session_scaffolding_2026-05-20.md) | 3-session forward agenda + 5 operator open questions. |
| [wiring_guide_audit_2026-05-20.md](./wiring_guide_audit_2026-05-20.md) | Fidelity audit of 5 wiring docs vs pin_definitions headers — 2 hard fixes, 3 ESP32 GPIO `[VERIFY]` flags. |
| [esp32_gpio_conflict_resolution_2026-05-20.md](./esp32_gpio_conflict_resolution_2026-05-20.md) | Resolution spec for ESP32 GPIO conflicts A/B/C — servo pins moved off receiver pins; compile-time `#error` guards. |
| [barometer_integration_spec_2026-05-20.md](./barometer_integration_spec_2026-05-20.md) | `USE_BAROMETER` Core-1 telemetry-only integration spec. |
| [gps_passthrough_spec_2026-05-20.md](./gps_passthrough_spec_2026-05-20.md) | `USE_GPS` passthrough-only spec — GPS bytes relayed to the flight computer, no onboard navigation. |
| [swarm_api_contract_2026-05-20.md](./swarm_api_contract_2026-05-20.md) | WiFi/HTTP/WebSocket wire contract between FC firmware and the swarm_api server; SHA-stamped. |
| [fc_docs_audit_2026-05-20.md](./fc_docs_audit_2026-05-20.md) | Meta-audit of the FC documentation set — drift findings, broken links, and prioritized cleanup recommendations. |
| [session3_readiness_2026-05-20.md](./session3_readiness_2026-05-20.md) | Session-3 readiness gate — collapses the 5 Session-2 specs into one executable plan and flags inter-spec contradictions. |
| [fc_core1_budget_2026-05-20.md](./fc_core1_budget_2026-05-20.md) | ESP32 Core-1 scheduling budget analysis for `USE_BAROMETER` + `USE_GPS` — placement guidance for the W2/W5 coding agents. |
| [phase_w2_barometer_landed_2026-05-20.md](./phase_w2_barometer_landed_2026-05-20.md) | W2 landing report — `USE_BAROMETER` code shipped (`src/barometer.cpp`, `include/barometer.h`, web_server baro block). |
| [phase_w5_gps_landed_2026-05-20.md](./phase_w5_gps_landed_2026-05-20.md) | W5 landing report — `USE_GPS` passthrough code shipped (`src/gps.cpp`, `include/gps.h`, web_server gps block). |

## 2026-05-21 session

| Document | Summary |
|---|---|
| [session_synthesis_2026-05-21.md](./session_synthesis_2026-05-21.md) | Per-area wire-level detail for the 2026-05-21 multi-agent session: W6 baro/GPS telemetry blocks added to the swarm POST, W4 `'b'` barometer calibration command + `calibration_baro` module, BMP388/MS5611 drivers (selector build-flag-overridable), native host-side test harness, `build.sh` coverage-matrix runner, doc-drift fixes. Session-level summary: [archive/session_records/2026-05-21_multi_agent_sensors_w6_native_tests.md](../archive/session_records/2026-05-21_multi_agent_sensors_w6_native_tests.md). Uncommitted. |
| [teensy_parity_assessment_2026-05-21.md](./teensy_parity_assessment_2026-05-21.md) | Teensy-parity recon — concludes the ESP32/Teensy split is correct by design; no parity work needed. |
| [phase_w5_gps_landed_2026-05-21.md](./phase_w5_gps_landed_2026-05-21.md) | W5 GPS LANDED + pin-default fix — `USE_GPS` passthrough confirmed shipped (`src/gps.cpp`, Core-1 task, `/api/telemetry` + `/api/status` + `/ws` gps blocks); missing `GPS_PIN_RX`/`GPS_PIN_TX` defaults fixed in `include/config.h`; build numbers + GPS-vs-SBUS UART separation. |

## 2026-05-22 session

| Document | Summary |
|---|---|
| [security_audit_2026-05-22.md](./security_audit_2026-05-22.md) | External attack-surface audit of the FC firmware: 3 P0 (unauth flight control, unauth OTA/RCE, no auth on any network surface), 2 P1 (GPS/identity telemetry leak, WS JSON truncation), P2/P3 items. RC clamp verified complete; no memory overflow; secrets clean. |
| Operator synthesis: [../security_posture.md](../security_posture.md) | Durable threat-model + posture + operator-guidance doc derived from the audit (trust boundary Mermaid, severity table). |
| Operator how-to: [../network_security_setup.md](../network_security_setup.md) | Step-by-step guide to enabling the audit's hardening: `USE_API_AUTH` + `FLOPPI_CMD_TOKEN`, OTA password/hash, and the `GPS_TELEMETRY_INCLUDE_POSITION` OPSEC build switch. Defers threat model to security_posture.md. |
| [qa_review_2026-05-22.md](./qa_review_2026-05-22.md) | QA gate on this session's security/correctness changes (USE_API_AUTH, OTA guards, WS buffer, I2C checksum, Madgwick NaN guard, GPS position gate). Build matrix 3/3 + flag combos green, native tests 5/5, per-change verdict all SOUND, 4 nits. **GO — safe to commit.** |
| [calibration_telemetry_verification_2026-05-22.md](./calibration_telemetry_verification_2026-05-22.md) | Read-only verification of the calibration subsystem + sensor-telemetry pipeline (9 cmds, IMU/baro/GPS, Madgwick, 3 baro drivers). Compiles + logic-sound at levels (a)+(b); level (c) bench-gated. Flagged the Madgwick NaN edge (M-1) since fixed. |
| [wifi_modes_qa_2026-05-22.md](./wifi_modes_qa_2026-05-22.md) | QA verdict (**GO**) on the compile-time ESP32 WiFi auth-mode selector (`WIFI_AUTH_MODE_*`: PSK/OPEN/WPA3-SAE/ENTERPRISE PEAP/TTLS/TLS, `USE_WIFI_CERTS`/`USE_STATIC_IP`/`WIFI_HOSTNAME`, liftable `wifi_connect` module). All modes build, all four `#error` guards fire, PSK byte-identical to legacy, Enterprise ~0 incremental flash (mbedTLS already linked); one minor HOSTNAME-ordering bug (BUG-1) since fixed in code. |
| Plan: [../plans/wifi-network-modes-plan.md](../plans/wifi-network-modes-plan.md) | Design plan for the WiFi auth-mode selector (architect, design-only) — selector schema, per-mode `WiFi.begin()` calls, `#error` validation, backward-compat aliases, per-mode memory budget, liftable-module recommendation. |
| Feature doc: [../features/wifi-configuration.md](../features/wifi-configuration.md) | Operator-facing WiFi configuration guide — the `WIFI_AUTH_MODE_*` selector, all four modes, per-mode setup, certs, static IP, hostname, the IDF enum name-collision note. |
| Session record: [../archive/session_records/2026-05-22_security_correctness_docs.md](../archive/session_records/2026-05-22_security_correctness_docs.md) | Session-level summary of the security/correctness/docs work — security/correctness code (USE_API_AUTH, OTA guards, WS buffer, I2C checksum, Madgwick NaN guard, GPS privacy gate), W5 GPS pin-default fix, ASCII→Mermaid + layered architecture docs, verification, and remaining hardware-gated/deferred work. Uncommitted. |
| Session record: [../archive/session_records/2026-05-22_wifi_network_modes.md](../archive/session_records/2026-05-22_wifi_network_modes.md) | Session-level summary of the ESP32 WiFi auth-mode feature — selector + modes, decisions (IDF enum collision, Arduino-overload-only, deferrals), the "Enterprise effectively free" memory finding, QA GO + the fixed hostname-ordering bug, next steps (runtime AP validation = operator). Uncommitted. |

## Standing reference research

| Document | Summary |
|---|---|
| [acrobatics-command-architecture.md](./acrobatics-command-architecture.md) | Acrobatics command architecture. |
| [auto-calibration-research.md](./auto-calibration-research.md) | Auto-calibration research. |
| [bare-bones-fc-research.md](./bare-bones-fc-research.md) | Bare-bones flight-controller research. |
| [command-arbitration-design.md](./command-arbitration-design.md) | Command arbitration design. |
| [display-module-architecture.md](./display-module-architecture.md) | Display module architecture. |
| [display-screen-capacity.md](./display-screen-capacity.md) | Display screen capacity analysis. |
| [esp32-dual-core-research.md](./esp32-dual-core-research.md) | ESP32 dual-core research. |
| [esp32-fc-feasibility.md](./esp32-fc-feasibility.md) | ESP32 flight-controller feasibility. |
| [esp32-wifi-connectivity.md](./esp32-wifi-connectivity.md) | ESP32 WiFi connectivity research. |
| [fc-timing-requirements.md](./fc-timing-requirements.md) | Flight-controller timing requirements. |
| [oled-display-options.md](./oled-display-options.md) | OLED display options. |
| [timing-calculator-analysis.md](./timing-calculator-analysis.md) | Timing calculator analysis. |

## Cross-project

| Document | Summary |
|---|---|
| `/home/devel/floppi/docs/findings/bno_cross_project_2026-05-20.md` | Joint IMU integration strategy — phased plan to port auto_orientation BNO drivers + calibration_storage HAL into FC. |

---

*Add new dated findings under the most recent session section; promote evergreen research to "Standing reference research".*
