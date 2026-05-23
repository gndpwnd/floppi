# Flight Controller Firmware - Documentation

Open-source VTOL flight controller firmware for Teensy 4.0/4.1, based on dRehmFlight. Designed for garage-buildable drones with auto-calibration features.

## Project Documents

| Document | Purpose |
|----------|---------|
| [scope.md](scope.md) | What this project IS and IS NOT |
| [roadmap.md](roadmap.md) | Feature checklist and milestones |
| [todo.md](todo.md) | Immediate tasks and session work |
| [architecture/INDEX.md](architecture/INDEX.md) | Layered architecture docs (Mermaid) — start at Level 0 system overview, drill down per subsystem |

## User Guides

| Guide | Description |
|-------|-------------|
| [0_quickstart.md](0_quickstart.md) | 60-minute path from zero to first flight |
| [1_hardware_setup.md](1_hardware_setup.md) | Complete wiring guide with diagrams |
| [2_calibration_guide.md](2_calibration_guide.md) | Manual and automatic calibration procedures |
| [3_troubleshooting.md](3_troubleshooting.md) | Problem solving reference |
| [diagnose_decision_tree.md](diagnose_decision_tree.md) | Decision-tree symptom flows (pairs with `dev.sh diagnose`) |
| [pid-tuning-guide.md](pid-tuning-guide.md) | PID tuning workflow (`g` command, conservative starts) |
| [testing.md](testing.md) | How to run the native unit tests and the calibration suite |
| [ota_runbook.md](ota_runbook.md) | Operator runbook for ESP32 over-the-air firmware flashing |
| [esp32_wifi_onboarding.md](esp32_wifi_onboarding.md) | First-time ESP32 WiFi credentials setup |
| [security_posture.md](security_posture.md) | Threat model + security posture for WiFi builds — read before flying on any network |
| [network_security_setup.md](network_security_setup.md) | Operator how-to: enable command-API auth, set the OTA password, build a GPS position-privacy variant |

## Optional ESP32 Sensors

ESP32 builds support two optional, telemetry-only sensors (default OFF, gated by
`#define` flags in `include/config.h`). They add data to the WiFi telemetry feed
only — neither touches the flight loop.

| Sensor | Flag | Setup Doc |
|--------|------|-----------|
| Barometer (pressure/temp/relative altitude — telemetry-only) | `USE_BAROMETER` | [phase_w2_barometer_landed_2026-05-20.md](findings/phase_w2_barometer_landed_2026-05-20.md) |
| GPS (raw NMEA passthrough — no parsing, no navigation) | `USE_GPS` | [phase_w5_gps_landed_2026-05-20.md](findings/phase_w5_gps_landed_2026-05-20.md) |

## Documentation Subfolders

| Folder | Contents |
|--------|----------|
| [architecture/](architecture/) | Layered architecture diagrams (Level 0 overview → Level 1 subsystems → Level 2 detail) |
| [features/](features/) | Feature specifications and usage docs |
| [findings/](findings/) | Research, investigations, and discoveries |
| [archive/](archive/) | Session summaries and historical records |

## Quick Start

```bash
cd flight_controller
pio run -e teensy40_calibration -t upload   # Flash calibration firmware
./tools/calibrate.sh                        # Launch calibration menu
# Copy values to config.h, then:
pio run -e teensy40 -t upload               # Flash live firmware
```

See [0_quickstart.md](0_quickstart.md) for the full guide.

## Tools

| Tool | Description |
|------|-------------|
| [calibrate.sh](../tools/calibrate.sh) | Menu-driven calibration wrapper (primary) |
| [serial_monitor.py](../tools/serial_monitor.py) | Raw serial monitor (scripting backend) |
| [flash_and_run.sh](../tools/flash_and_run.sh) | Build + flash + serial monitor |
| [calibration_reset.py](../tools/calibration_reset.py) | Reset config.h to factory defaults |
| [test_calibration.sh](../tests/test_calibration.sh) | Automated calibration test suite (18 tests / 42 assertions) |
| [build_tests.sh](../tools/build_tests.sh) | Native host-side (`g++`) unit-test runner — pure-math tests in `tests/native/` |

## Related Projects

- **[fc_tool](/fc_tool/)** — Desktop app for serial monitoring and data visualization
- **[swarm_api](/swarm_api/)** — Python FastAPI server for ESP32 fleet control over WiFi
- **engineering360** (separate repo) — Physical drone design, frame construction, component selection

---

*For the parent project overview, see the [floppi README](/README.md) and [floppi docs](/docs/).*
