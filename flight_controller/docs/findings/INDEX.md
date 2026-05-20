# Findings — Index

Research notes, audits, recon reports, and design documents for the flight_controller project. Dated files use `YYYY-MM-DD` suffixes; undated files are standing reference research.

## 2026-05-20 session

| Document | Summary |
|---|---|
| [project_recon_2026-05-20.md](./project_recon_2026-05-20.md) | First comprehensive map of the FC project (779 lines); build matrix — 7/10 passing pre-session, 10/10 after SBUS re-enable. |
| [test_infrastructure_v2_2026-05-20.md](./test_infrastructure_v2_2026-05-20.md) | Test harness modularization design — `test_calibration.sh` split into `lib/harness.sh` + `suites/test_calibration.sh`; 18 tests / 42 assertions preserved. |
| [future_session_scaffolding_2026-05-20.md](./future_session_scaffolding_2026-05-20.md) | 3-session forward agenda + 5 operator open questions. |
| [wiring_guide_audit_2026-05-20.md](./wiring_guide_audit_2026-05-20.md) | Fidelity audit of 5 wiring docs vs pin_definitions headers — 2 hard fixes, 3 ESP32 GPIO `[VERIFY]` flags. |

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
