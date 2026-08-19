# 03 — Folder recon findings + contents-based routing recommendations

**Status:** 2026-07-09 recon pass. Read-only inspection of `~/floppi/` and `~/SwarmLoc/` uncertain source folders. Feeds the reorg into the 12-13 target repos under the flat `lowprofiledronegurus` GitLab group.
**Scope:** Deep look at 9 source areas + top-level docs categorization. Every recommendation is grounded in an observed README / filename / directory listing.
**Not in scope:** copying, moving, git operations, GitLab CRUD.

Cross-reference: `00_reorg_master_plan.md` (open questions Q-A…Q-F now closed by the user's answers). New target taxonomy this doc uses:

- `auto_orientation_research` (IMU/mag/orientation — domain-agnostic, incl. rovers)
- `flight_controller` (Teensy/ESP32 FC firmware)
- `fc_tool` (host-side Tauri tool)
- `drone_frame_modeling` (mechanical/CAD)
- `position_denial_research` (GPS-denial theory, multilateration)
- `sensor_interactions` (**re-scoped narrow**: non-radio, non-networking sensor POCs — cameras, environmental, lidar, ultrasonic)
- `swarm_communication_protocol` (hex-string swarm language design)
- `swarm_api` (Python HTTP/WS ground-station)
- `darpa_lift_2026` (mission context)
- `communication_hardware` (**NEW** — UWB/RF/radio hardware POCs)
- `networking_pocs` (**NEW** — WiFi/cellular/DNS-driveby experiments)
- `research` (**NEW** — centralized literature + mini research projects + automated research tool)
- `cybersecurity_demos` (future — materializes when first cyber POC lands)

---

## 1. `~/SwarmLoc/DWS1000_UWB/`

**Path:** `/home/devel/SwarmLoc/DWS1000_UWB/`
**Size / shape:** PlatformIO project. ~15 top-level entries. Substantial:
- `src/` (3 files: `anchor_main.cpp`, `tag_main.cpp`, `calibration_main.cpp`)
- `include/` (`config.h`, `display.h`)
- `lib/` (`DW1000/` legacy thotro, `DW1000-ng/` local modified, `U8g2/` vendored display)
- `tests/` — very large — dozens of `.cpp` iteration files (`test_rx_v8a`…`v11_xtalt_sweep`, `test_twr_anchor.cpp`, `test_twr_tag.cpp`, `test_calibration_tag.cpp`, plus `.sh` runners, `README.md`, `TESTING_GUIDE_POST_BUG_FIX.md`, `TESTING_SUITE_SUMMARY.md`, `EXECUTION_INSTRUCTIONS.md`, and subdirs `test_01_chip_id/` … `test_08_multi_node_swarm/`)
- `tests_new/` — cleaner reorganization: `test_01_chip_id/`, `test_02_connectivity/`, `test_03_04_tx_rx/`, `test_06_ranging/`
- `test_scripts/`, `scripts/` (upload / capture / calibration helpers), `tools/` (`serial_monitor.py`, `dev.sh`)
- `docs/` (`scope.md`, `roadmap.md`, `todo.md`, `bugs.md`, `known_issues.md`, `serial_commands.md`, `findings/`, `features/`, `archive/`, plus README.md)
- Top-level scripts: `monitor_ranging.sh`, `upload_anchor.sh`, `upload_tag.sh`, `upload_both_cable_swap.sh`
- `platformio.ini`, `compile_commands.json`

**What it actually is (from README + contents):**
Two-Way Ranging (TWR) system for UWB distance measurement using Arduino Uno + Qorvo PCL298336 v1.3 shields (which carry the DW1000 chip, NOT DWM3000). Achieves ±4.4 cm precision at 9.4 Hz after antenna delay calibration. Includes hardware-level workarounds (J1 jumper install, D8→D2 IRQ wire), a locally-modified DW1000-ng library, LDO tuning per-role, and calibration. The README frames it as "Part of the SwarmLoc project for GPS-denied drone swarm positioning" — so it's both a **DW1000 hardware driver POC** AND a **ranging-experiment platform** for the GPS-denial theory.

**Recommended target — SPLIT (per user's confirmed Q-A answer, communication_hardware is now a real repo):**

1. **CODE + BUILD → `communication_hardware/uwb/dw1000/`**
   - `src/`, `include/`, `lib/DW1000/`, `lib/DW1000-ng/`, `platformio.ini`
   - `tests/` (all `.cpp` iterations — this is chip-driver bring-up work), `tests_new/` (cleaner reorg)
   - `test_scripts/`, `scripts/`, `tools/`
   - Top-level upload/monitor shell scripts
   - `docs/scope.md`, `docs/roadmap.md`, `docs/todo.md`, `docs/bugs.md`, `docs/known_issues.md`, `docs/serial_commands.md` — hardware-specific engineering docs
   - `lib/U8g2/` vendored: keep alongside (or purge — it's public library)
2. **THEORY/FINDINGS DOCS → `position_denial_research/uwb_ranging/`**
   - `docs/findings/` (technical investigations, session logs, LDO tuning discoveries)
   - `docs/features/` (feature-level design)
   - `docs/archive/` — historical decisions
   - The DW1000 library bug-fix write-up at `~/SwarmLoc/docs/DW1000_LIBRARY_BUG_FIX.md` (see §7 below) is a companion doc — routes here.

**Alternative if split feels heavy:** whole tree → `communication_hardware/uwb/dw1000/`, and `position_denial_research/` cross-references it. Slightly less clean but valid — the findings docs are tightly coupled to specific chip behavior.

**Confidence:** HIGH for the code half (unambiguously chip-level driver + PlatformIO project). MED for the split boundary — `docs/findings/` has both hardware-quirk notes (belongs with code) and ranging-methodology notes (belongs with theory). Per-file sort recommended when actually moving.

**Should stay in ~/SwarmLoc:** nothing. Everything routes.

---

## 2. `~/SwarmLoc/esp32_field_node/`

**Path:** `/home/devel/SwarmLoc/esp32_field_node/`
**Size / shape:** PlatformIO project, ~7 top-level entries.
- `src/main.cpp` — single-file firmware entry
- `include/` — `wifi_credentials.h`, `wifi_credentials.local.h.example`
- `lib/` — `display/`, `imu/`, `MPU6050/`, `web/`, `wifi_field/` (per-domain modules)
- `docs/` — `scope.md`, `roadmap.md`, `todo.md`, `README.md`, `findings/`, `archive/`
- `scripts/` — `build.sh`, `capture_serial.py`, `monitor.sh`, `upload_and_capture.sh`
- `platformio.ini`, `README.md`, `.gitignore`

**What it actually is (from README + scope.md):**
ESP32-WROOM-32D field node whose **near-term goal** is to characterize and connect to a specific commercial WiFi network ("MSC guest"). Scope.md is explicit: probe WiFi (WPA2-Enterprise PEAP, captive portal, WPA2-PSK), display status on OLED, read MPU6050. Findings docs include `msc-guest-network-characterization.md`, `msc-guest-wifi-no-certs.md`, and reuse notes referencing `GravityProbe/esp32_ewpa2_iic_091/` and `floppi/flight_controller/`. Explicitly rules out UWB (that's DWS1000_UWB), LoRa (lora_feather_esp32/), GPS (GPS_module/). This is a **commercial-WiFi networking POC** with a display+IMU harness on top.

**Recommended target — `networking_pocs/esp32_wifi_field_node/`** (per Q-B: WiFi/cellular/DNS-driveby experiments repo)
- The whole project ports as-is: `src/`, `include/`, `lib/`, `docs/`, `scripts/`, `platformio.ini`, `README.md`
- **Do NOT commit** `wifi_credentials.local.h.example` filled-in variants — only the `.example` template. Verify at move-time no real creds are in git history before pushing.
- The MPU6050 code in `lib/imu/` and `lib/MPU6050/` is auxiliary and could logically drop into `sensor_interactions/imu/` — recommend leaving with the field node for cohesion (it's the harness, not the point). Only split if `sensor_interactions/` needs a bare MPU6050 example and doesn't already have one.

**Alternative:** if `cybersecurity_demos` materializes early with cyber-flavored WiFi probing work, this could route there. But per user's Q-B decision, `cybersecurity_demos` is deferred until an actual cyber POC lands; WiFi network characterization is a networking POC, not a cyber POC.

**Confidence:** HIGH. Scope explicitly names WiFi and connection to commercial WiFi as the point of the project.

**Should stay in ~/SwarmLoc:** nothing.

---

## 3. `~/SwarmLoc/GPS_module/`

**Path:** `/home/devel/SwarmLoc/GPS_module/`
**Size / shape:** 2 Arduino sketches, no PlatformIO project:
- `basic_GPS_LAT_LONG_LCD_adafruit_featherwing/basic_GPS_LAT_LONG_LCD_adafruit_featherwing.ino`
- `GPS_OLED_091/GPS_OLED_091.ino`

**What it actually is (from sketch headers):**
- Sketch 1: "adafruit feather esp32 and adafruit GPS featherwing / pretty print the GPS coordinates" — plain read-GPS-and-serial-print. Uses `Adafruit_GPS.h`, hardware serial Serial1.
- Sketch 2: "Adafruit Feather ESP32 with GPS FeatherWing and OLED Display / Display GPS coordinates in DMS format on OLED" — GPS + SSD1306 0.91" display via U8g2.

Neither sketch has any GPS-denial content. Both are pure GPS receiver bring-up. No spoofing / no denial-recovery / no multilateration.

**Recommended target — `sensor_interactions/gps/`**
- Both sketch folders port as-is under a `gps/` subdirectory (matches the re-scoped `sensor_interactions` narrower charter: non-radio, non-networking sensor POCs).
- GPS is a passive receiving sensor — fits `sensor_interactions` cleanly.

**Alternative:** if `position_denial_research/` wants pinned "baseline / working GPS" reference sketches for comparison against denied conditions, symlink or link back — but the code homes in `sensor_interactions/gps/`.

**Confidence:** HIGH. Sketches are trivially self-describing.

**Should stay in ~/SwarmLoc:** nothing.

---

## 4. `~/SwarmLoc/lora_feather_esp32/`

**Path:** `/home/devel/SwarmLoc/lora_feather_esp32/`
**Size / shape:** 5 Arduino sketches + 2 markdown notes:
- `lora_gps_node/lora_gps_node.ino` — "LoRa Node 1 - Adafruit Huzzah32 + RFM95W FeatherWing / Initiates communication, then responds when receiving messages"
- `lora_gps-d_node/lora_gps-d_node.ino` — "LoRa Node 2 … Waits for messages, then responds"
- `ESP32_I2C_Scanner/ESP32_I2C_Scanner.ino` — generic I2C address scanner (Rui Santos / random nerd tutorials)
- `adaf_esp32_BaudR_scan/adaf_esp32_BaudR_scan.ino` — LoRa module baud-rate discovery for ESP32
- `ard_BaudR_scan/ard_BaudR_scan.ino` — same, Arduino SoftwareSerial variant
- `adafruit_lora.md` — hardware setup notes (which URLs to add for board manager, CP2104 driver install)
- `notes.md` — mixed: LoRa hardware setup + ToF/positioning-accuracy math (speed-of-light math, TWR mention, DWM1000/3000 mention)

**What it actually is:**
LoRa radio hardware bring-up for RFM95W FeatherWing on Adafruit Huzzah32. Two-node ping-pong sketches, plus utility scanners (I2C, baud rate). `notes.md` mixes hardware wiring with borderline positioning theory. **This is radio-hardware POC territory, not networking (LoRa is point-to-point radio, not a network stack).**

**Recommended target — SPLIT:**

1. **LoRa sketches + LoRa-specific docs → `communication_hardware/radio/lora/`**
   - `lora_gps_node/`, `lora_gps-d_node/`, `adaf_esp32_BaudR_scan/`, `ard_BaudR_scan/`
   - `adafruit_lora.md`
   - `notes.md` (the LoRa/RFM95 wiring + board-manager URL portions)
2. **I2C scanner → `sensor_interactions/tools/i2c_scanner/`** or **stay with LoRa as bench-bringup utility**
   - It's a bare I2C probe utility, not radio-specific. Small enough that either home works.
   - Recommend `sensor_interactions/tools/` — it's a bring-up utility that helps any I2C-hardware POC.
3. **ToF/positioning math snippet in `notes.md` → `position_denial_research/theory/tof_ranging_basics.md`**
   - The speed-of-light math + rule-of-thumb table + TWR-vs-one-way ToF discussion is theoretical positioning content, not hardware. Extract as its own doc; leave the LoRa-hardware portions with the LoRa POCs.

**Alternative:** whole tree → `communication_hardware/radio/lora/` verbatim, and pull the ToF math out later. Faster to migrate but leaves theory content in the wrong home.

**Confidence:** HIGH for LoRa sketches. MED for the I2C-scanner boundary (either home defensible). HIGH that ToF math in `notes.md` is out-of-place for a LoRa hardware folder.

**Should stay in ~/SwarmLoc:** nothing.

---

## 5. `~/SwarmLoc/docs/` + `~/SwarmLoc/findings/` (SwarmLoc top-level)

**Paths:**
- `/home/devel/SwarmLoc/docs/DW1000_LIBRARY_BUG_FIX.md` (8.8 KB) + `docs/findings/` subdir
- `/home/devel/SwarmLoc/findings/README_Research_Findings.md` (12 KB), `UWB_Implementation_Code_Examples.md` (33 KB), `UWB_Swarm_Ranging_Architecture_Research.md` (47 KB)

**What it actually is:**
- `DW1000_LIBRARY_BUG_FIX.md` — hardware/library bug hunting narrative. Tightly coupled to the DW1000 chip and the local library modification.
- `findings/README_Research_Findings.md` + `UWB_Implementation_Code_Examples.md` + `UWB_Swarm_Ranging_Architecture_Research.md` — this is the theoretical/architectural spine of SwarmLoc. Swarm-scale ranging architecture, code-example reference (not project code — reference material), general findings.

**Recommended target:**
- `DW1000_LIBRARY_BUG_FIX.md` → **`communication_hardware/uwb/dw1000/docs/`** (belongs with the DW1000 driver code — it's a bug narrative on that specific chip/library).
- `findings/README_Research_Findings.md` + `UWB_Swarm_Ranging_Architecture_Research.md` → **`position_denial_research/uwb_ranging/`** (theory + architecture).
- `UWB_Implementation_Code_Examples.md` → **borderline**. If it's "here's how to write DW1000 code" → `communication_hardware/uwb/dw1000/docs/examples/`. If it's "here are algorithms/architectures for TWR-based swarm" → `position_denial_research/uwb_ranging/`. Recommend a quick per-file read at move-time.
- `docs/findings/` subdir → same split principle as above per-file.

**Confidence:** HIGH for the bug fix and architecture research. MED for Implementation_Code_Examples.

**Should stay in ~/SwarmLoc:** nothing. `~/SwarmLoc/README.md` and `todo.md` don't move — they're historical.

---

## 6. `~/floppi/swarm_api/`

**Path:** `/home/devel/floppi/swarm_api/`
**Size / shape:** Full Python project.
- `src/` — `main.py`, `manager.py`, `drone.py`, `config.py`, `__init__.py`, `api/`, `static/`
- `tests/test_security.py`
- `docs/` — `architecture.md`, `README.md`, `roadmap.md`, `scope.md`, `todo.md`, `archive/`, `features/`, `findings/`
- `deploy/` — `common.sh`, `diagnostics.sh`, `health.sh`, `menu.sh`, `service.sh`
- `deploy.sh`, `config.json`, `requirements.txt`

**What it actually is (from docs/README.md):**
"Ground-station control application for floppi ESP32 drones over WiFi." Python FastAPI app. Browser dashboard, real-time telemetry (attitude, motors, RSSI), fleet management via `config.json` (MAC/name/IP per drone), drone discovery via mDNS with IP fallback, Linux+Windows. Config shows drone entries keyed by MAC with mDNS hostnames like `floppi-EEFF`. This is **drone-swarm-specific**, tightly coupled to floppi ESP32 firmware, not a general-purpose API. Endpoints are drone-fleet-shaped (control, telemetry, discovery).

**Recommended target — `swarm_api` (dedicated new repo, confirmed in master plan)**
- Whole tree ports as-is.
- Do NOT sub-fold under any other repo — it's implementation-shippable (Category C in master plan) and semi-independent from FC firmware.
- Cross-reference in README to `flight_controller` (firmware peer) and `swarm_communication_protocol` (once the hex-string protocol lands, this is a candidate consumer).

**Confidence:** HIGH. Explicit, self-describing project with docs/scope.md and docs/architecture.md already present.

**Should stay in ~/floppi:** nothing.

---

## 7. `~/floppi/fc_tool/`

**Path:** `/home/devel/floppi/fc_tool/`
**Size / shape:** Tauri 2 (Rust + web frontend) desktop app.
- `src/` — JS frontend: `main.js`, `dashboard.js`, `plotter.js`, `connection.js`, `cursors.js`, `period-detector.js`, `ansi.js`, `styles.css`, `index.html`, `lib/`
- `src-tauri/` — Rust backend: `Cargo.toml`, `Cargo.lock`, `build.rs`, `src/`, `tauri.conf.json`, `capabilities/`, `icons/`
- `tests/` — Python-based tests: `conftest.py`, `pytest.ini`, `simulate_serial.py`, `simulator/`, `parsers.py`, `test_parse_ansi.py`, `test_parse_dashboard.py`, `test_serial_pipeline.py`, `test_headless.py`, `test_cursors.py`, `test_period_detector.py`, `test_performance.py`, `test_edge_cases.py`, `test_plotter.sh`, `test_monitor.sh`, others
- `docs/` — `architecture.md`, `scope.md`, `roadmap.md`, `todo.md`, `README.md`, `cursor-interaction-discussion.md`, `plotter_discussion.md`, `signal-analysis-discussion.md`, `features/`, `findings/`
- `dev_setup/` — `linux/`, `macos/`, `windows/`, `README.md`
- `package.json`, `deploy.sh`

**What it actually is:**
Cross-platform desktop tool for serial monitoring + dynamic multi-graph plotting of flight controller output. Tauri 2 shell (Rust) + web frontend (JS + Chart.js). Ships as single native executable per platform, no runtime deps. Parses `name@plotId:value` from serial stream, dynamic per-plot graphs. Explicitly complements PlatformIO for firmware flashing; NOT a build tool.

**Recommended target — `fc_tool` (direct-match existing empty repo)**
- Whole tree ports as-is (Category C implementation).

**Confidence:** HIGH. Direct 1:1 name and purpose match.

**Should stay in ~/floppi:** nothing.

---

## 8. `~/floppi/docs/` (top-level docs — per-file routing)

**Path:** `/home/devel/floppi/docs/`
**Size / shape:** ~25 markdown files at root + 8 subdirectories.

Per-file routing (each row grounded in the first 5-10 lines of the file):

| File | What it is | Target | Confidence |
|---|---|---|---|
| `ARCHITECTURE.md` | "Whole-Repo Architecture (Level -1)" — describes the five sub-projects of legacy floppi | **stays in `floppi/` as historical**, OR archive as a "how-it-was" doc in `flight_controller/docs/history/`. Not carried verbatim into any new repo (obsolete post-split). | HIGH |
| `EKF_API_REFERENCE.md` | API doc for `ExtendedKalmanFilter` class at `auto_orientation/src/navigation/ekf.h` | **`auto_orientation_research/theory/ekf/`** | HIGH |
| `EKF_THEORY.md` | EKF math theory reference | **`auto_orientation_research/theory/ekf/`** | HIGH |
| `EKF_TUNING_GUIDE.md` | Step-by-step tuning procedure | **`auto_orientation_research/theory/ekf/`** | HIGH |
| `QUATERNION_MASTER_INDEX.md` | Hub for the quaternion doc set | **`auto_orientation_research/theory/quaternions/`** | HIGH |
| `QUATERNION_REFERENCE.md` | Complete math reference | **`auto_orientation_research/theory/quaternions/`** | HIGH |
| `QUATERNION_IMPLEMENTATION_GUIDE.md` | Pseudocode + C++/Arduino for quaternion ops | **`auto_orientation_research/theory/quaternions/`** | HIGH |
| `QUATERNION_IN_FLOPPI_CONTEXT.md` | Applies quaternion math to floppi code locations | **`auto_orientation_research/theory/quaternions/`** with cross-ref to `flight_controller/` | HIGH |
| `IMU_GPS_SENSOR_FUSION.md` | IMU+GPS fusion theory for aerial platforms | **`auto_orientation_research/theory/sensor_fusion/`** (fusion is orientation-adjacent). Could also cross-post to `position_denial_research/` since GPS is core. | MED |
| `BEC_Wiring.md` | BEC power supply doc for UAV FC | **`flight_controller/hardware/power/`** | HIGH |
| `POWER_MODULE_WIRING.md` | GM v1.0 power module for Teensy + ESCs | **`flight_controller/hardware/power/`** | HIGH |
| `DRONE_APPLICATIONS_REFERENCE.md` | Practical workflows for autonomous landing, VO, 3D recon, obstacle avoidance | **`research/` (centralized research)** — this is broad application background, not FC-specific. Could also route to `darpa_lift_2026/context/` if it's LIFT-shaped. Read again at move-time. | MED |
| `FEATURE_COMPARISON.md` | dRehmFlight-master vs refactored PlatformIO comparison | **`flight_controller/docs/history/`** | HIGH |
| `PHASE_3_IMPLEMENTATION_SUMMARY.md` | EKF Phase 3 implementation summary | **`auto_orientation_research/docs/history/`** (session-record style) | HIGH |
| `PHASE_3_TEST_RESULTS.md` | EKF Phase 3 test validation results | **`auto_orientation_research/docs/history/`** | HIGH |
| `MIGRATION_SUMMARY.md` | 2026-01-11 scope refactor summary | **stays in `floppi/` archive** (about the OLD repo's own refactor) | HIGH |
| `ORGANIZATION_SUMMARY.md` | 2026-05-05 root-cleanup summary | **stays in `floppi/` archive** | HIGH |
| `SCOPE_REFACTOR_COMPLETE.md` | 2026-01-11 refactor complete marker | **stays in `floppi/` archive** | HIGH |
| `README.md` | Docs directory intro | **stays in `floppi/` archive** — each new repo gets its own fresh `docs/README.md` | HIGH |
| `ROADMAP.md` | Top-level floppi roadmap 2026-03-30 | **stays in `floppi/` archive** — new repos get their own roadmap | HIGH |
| `scope.md` | Top-level floppi scope 2026-03-30 | **stays in `floppi/` archive** | HIGH |

**Subdirectories:**
- `docs/flight-controller/` — scope, roadmap, system_overview, sensor_data_pipeline, math_and_algorithms, pid_control_systems, control_mixer_and_actuators → **`flight_controller/docs/`** (all HIGH)
- `docs/flight-computer/` — scope, roadmap → **`flight_controller/docs/flight-computer/`** or **`swarm_api/docs/history/`** (the "flight computer" is the ESP32 + WiFi command layer, cross-cutting both). Scope.md describes ESP32/RPi WiFi bridge to Teensy FC — could arguably route to `swarm_api`. Recommend reading full scope.md at move-time.
- `docs/findings/` — mixed bag:
  - `bno_cross_project_2026-05-20.md` (IMU driver comparison) → **`auto_orientation_research/findings/`**
  - `calibration-test-results-2026-02-12.md` (Teensy+MPU6050 calibration) → **`auto_orientation_research/findings/`**
  - `bootloader_recovery_guide.md`, `bootloader_dtr_rts_analysis.md`, `bootloader_quick_reference.md`, `BOOTLOADER_RECOVERY_SUMMARY.md`, `README_BOOTLOADER.md`, `BOOTLOADER_FILES.txt`, `DELIVERABLES_CHECKLIST.txt` (Arduino Mega bootloader recovery) → **`flight_controller/findings/bootloader/`**
  - `teensy-serial-troubleshooting.md` → **`flight_controller/findings/`** (Teensy is FC target)
  - `day_status_2026-05-20.md` (cross-project session snapshot) → **`floppi/` archive** (historical, per-project bits can be extracted)
  - `marine_core_drone.md` (one-line link to marines.mil 3D-printed drone article) → **`research/references/` or discard** — trivial
- `docs/guides/GPS_TROUBLESHOOTING.md` — NEO-M9N connectivity troubleshooting → **`sensor_interactions/gps/troubleshooting/`** or **`flight_controller/findings/gps/`** (peripheral bring-up)
- `docs/literature/` — has its own `scope.md`, `roadmap.md`, `findings/README.md`, `findings/recommended-textbooks.md`, `findings/serial-rich-text-formatting.md`, plus nested `literature/`
  - All → **`research/literature/`** (the centralized `research` repo — Q-E)
- `docs/todo/TASKS.md` — 2026-03-30 top-level todo list → **stays in `floppi/` archive**
- `docs/archive/session_2026-03-30_researchhub-bootstrap.md` → **stays in `floppi/` archive** (session record)
- `docs/_archived/` — README.md ("out of scope for the floppi project") + `reference-platform/scope.md`, `reference-platform/roadmap.md` → **stays in `floppi/` archive**

**Confidence overall for `docs/`:** MED at aggregate — most files map HIGH, but a few (DRONE_APPLICATIONS_REFERENCE, flight-computer/, IMU_GPS_SENSOR_FUSION) are cross-cutting.

**Should stay in ~/floppi:** every "SUMMARY / MIGRATION / ORGANIZATION / REFACTOR_COMPLETE / archived" doc, the top-level `docs/README.md`, `docs/ROADMAP.md`, `docs/scope.md`, `docs/todo/TASKS.md`, `docs/archive/`, `docs/_archived/`.

---

## 9. `~/floppi/research/` (top-level)

**Path:** `/home/devel/floppi/research/`
**Size / shape:** 6 files.
- `ARDUINO_DIAGNOSTICS.md` — Arduino serial port + BNO085 IMU diagnostics
- `CAMERA_EXTRINSIC_CALIBRATION.md` — Camera extrinsic calibration for aerial robotics (theory)
- `GPS_CHEAT_SHEET.txt` — GPS + coordinate systems quick reference
- `GPS_COORDINATE_QUICK_REFERENCE.md` — Day-to-day GPS reference
- `GPS_COORDINATE_SYSTEMS_INDEX.md` — Documentation index for GPS/geodetic docs
- `GPS_GEODETIC_COORDINATE_SYSTEMS.md` — Theory doc for coordinate systems

**Recommended target:**
- `ARDUINO_DIAGNOSTICS.md` — mostly BNO085 IMU-focused despite the "Arduino" title → **`auto_orientation_research/findings/`** (IMU diagnostics is orientation-domain)
- `CAMERA_EXTRINSIC_CALIBRATION.md` — camera calibration ties both to orientation (attitude ref) and general sensor calibration → **`sensor_interactions/camera/theory/`** primary, cross-ref from `auto_orientation_research/`
- All GPS_* files (4) → **`position_denial_research/theory/gps/`** (background reading for GPS-denial work — same routing as master plan Q-D)
  - Or alternately → **`research/gps/`** (the centralized research repo). Per user's Q-E answer that `research` is for centralized literature + mini-research, and GPS-denial is a specific target, recommend `position_denial_research` for these as they are the theoretical foundation for that specific research area, not general literature.

**Confidence:** MED-HIGH. GPS_* is unambiguous topically (position denial repo); the exact home vs. centralized `research/` is a judgment call.

**Should stay in ~/floppi:** none.

---

## 10. `~/floppi/literature/` (PDFs + a few markdowns)

**Path:** `/home/devel/floppi/literature/`
**Size / shape:** 15 items — mostly PDFs.

Per user's Q-E answer, the target is **centralize into `research/literature/`** (or distribute per-topic). Recommended per-file:

| File | Topic | Target |
|---|---|---|
| `drehmflight_README.md` | dRehmFlight upstream README (mirror) | **`research/literature/drehmflight/`** (centralized). Cross-link from `flight_controller/references/`. |
| `drehmflight_transcripts.md` | dRehmFlight video transcripts | **`research/literature/drehmflight/`** |
| `dRehmFlight VTOL Documentation.pdf` | dRehmFlight VTOL PDF | **`research/literature/drehmflight/`** |
| `init_about_refactored_drehmflight.md` | Setup guide for floppi's dRehmFlight refactor | **`flight_controller/docs/history/`** (this is floppi-specific, not general literature) |
| `Longfly dRehmFlight Purchase Lists.pdf` | Vendor purchase list | **`flight_controller/references/`** or `drone_frame_modeling/references/` |
| `longfly pcb.webp` | PCB image | **`flight_controller/references/hardware/`** |
| `DWM1000 Data Sheet.pdf` | Chip datasheet | **`communication_hardware/uwb/dw1000/references/`** |
| `dws1000productbriefv10.pdf` | Product brief | **`communication_hardware/uwb/dw1000/references/`** |
| `gy-521_mpu-6050_3-axis_gyroscope_and_acceleration_sensor_en.pdf` | Breakout datasheet | **`auto_orientation_research/references/`** or `flight_controller/references/imu/` |
| `MPU-6000-Datasheet1.pdf` | Chip datasheet | **`auto_orientation_research/references/`** or `flight_controller/references/imu/` |
| `fs-ia6b-manual.pdf` | RC receiver manual | **`flight_controller/references/rc/`** |
| `RFM69HCW-V1.1.pdf` | RFM69 radio module datasheet | **`communication_hardware/radio/references/`** |
| `Morphy_ A Compliant and Morphologically Aware Flying Robot.pdf` | Academic paper | **`research/papers/`** (centralized literature — general/academic) |
| `Elke...GNC_Testing.pdf` | Academic paper (GNC testing quadcopter) | **`research/papers/`** — general/academic |
| `resources.md` | Flight-controller-software resources list | **`flight_controller/references/`** |

**Note:** The user's decision was to **centralize** literature in `research/`. The above split still respects that in spirit — general/academic PDFs go to `research/`, but tightly-coupled hardware datasheets are pinned to the repo that consumes them (`communication_hardware/uwb/dw1000/references/DWM1000.pdf` is more useful next to the code that programs the chip than in a floating literature repo). Recommend `research/` also keeps an INDEX/README.md that lists the cross-repo references and where they live.

**Confidence:** HIGH per-file for topic. MED for the centralize-vs-pin split boundary — negotiable.

**Should stay in ~/floppi:** none. Everything routes.

---

## 11. `~/floppi/archived_docs/`

**Path:** `/home/devel/floppi/archived_docs/`
**Contents:** 1 file — `TASK_COMPLETION_SUMMARY.md` ("BNO085 Extensions & Comprehensive Testing" complete-marker, 2026-05-07).

**Recommended target:** **stays in `floppi/`** as historical (or extracted to `auto_orientation_research/docs/history/` since it's BNO085 IMU work). Recommend leave in place — it's a one-off session-completion marker with limited long-term value.

**Confidence:** HIGH.

---

## 12. `~/floppi/scripts/` + `~/floppi/tools/`

**Path:** `/home/devel/floppi/scripts/`
- `build_coordinate_frame_tests.sh` (20 lines) — test runner for coordinate-frame code → **`auto_orientation_research/scripts/`** (coordinate frames is orientation-domain)
- `GPS_IMPLEMENTATION_EXAMPLES.py` (730 lines) — Python code examples for GPS work → **`position_denial_research/examples/`** OR **`sensor_interactions/gps/examples/`** depending on whether it demonstrates denial or plain GPS. Head-read at move-time to decide.

**Path:** `/home/devel/floppi/tools/`
- `recover_bootloader.sh` (519 lines) — Arduino Mega bootloader recovery script → **`flight_controller/tools/bootloader_recovery/`** (matches the bootloader findings cluster in §8)
- `setup_permissions.sh` (115 lines) — serial-port permissions setup → **`flight_controller/tools/`** (generic dev-env helper) OR shared across all embedded-project repos
- `researchhub_client.py` (2943 lines) — "Drop-in API client for external projects" for ResearchHub → **`research/tools/researchhub_client/`** (this is exactly the "automated research tool hookup" mentioned in the user's Q-E answer — perfect fit for the centralized `research` repo)

**Confidence:** HIGH for the bootloader recovery and researchhub client. MED for the GPS Python examples (need per-file re-read for denial vs. plain).

---

## 13. `~/floppi/darpa_lift_2026/`

**Path:** `/home/devel/floppi/darpa_lift_2026/`
**Contents (first ~10 lines each):**
- `avionics_controls.md` — starts "how can i make a drone be controlled by avionics that only take 9v power and use 5v logic and have them controll motors that takes dozens of voltage and high amperage" → LLM-Q&A style research on ESCs, power architecture.
- `comp_requirements.md` — "DARPA Lift Challenge - Requirements & Competition Tracker" with Challenge Date Summer 2026, $6.5M prize, payload-to-weight ratio goal.
- `initial_research.md` — "how to increase lift to weight ratio on a drone" — Q&A on thrust vs weight.
- `initial_sources.md` — book list ("Aircraft Propulsion" by Farokhi, "Introduction to Flight" by Anderson, etc.)
- `notes1.md` — "now i want to research all relevant equations for the following components: propellers, arms/booms, motors, battery" — planning notes.
- `transcripts.md` — DARPA Lift Challenge page transcript ("darpa.mil/research/challenges/lift").

**What it actually is:** All six files are competition-context research and requirements docs for DARPA Lift Challenge 2026. Confirms master plan's routing: this is Category-E mission/competition context.

**Recommended target — `darpa_lift_2026` (dedicated new repo)**
- All 6 files port as-is under `docs/` or `context/`.
- Consider adding `initial_sources.md` books also into `research/literature/reading_list.md` cross-ref for the general reading list.

**Confidence:** HIGH.

**Should stay in ~/floppi:** none.

---

> **⚠ SUPERSEDED 2026-08-18 (routing conclusions only).** The 2026-08-18 operator rulings make
> SwarmLoc and GravityProbe *mined*, not migrated, drop the DARPA repo, and unsettle
> `drone_frame_modeling`. The recon below (what is on disk, and why) still stands; its **routing
> conclusions do not**. Current routing: `11_routing_v2_2026-08-18.md`. Rulings: `10_decision_queue.md`.

## 14. Summary — cross-inspection routing table

| Source area | Primary target | Split target(s) | Confidence |
|---|---|---|---|
| `~/SwarmLoc/DWS1000_UWB/` (whole) | `communication_hardware/uwb/dw1000/` | `position_denial_research/uwb_ranging/` (theory docs slice) | HIGH code / MED split boundary |
| `~/SwarmLoc/esp32_field_node/` | `networking_pocs/esp32_wifi_field_node/` | — | HIGH |
| `~/SwarmLoc/GPS_module/` | `sensor_interactions/gps/` | — | HIGH |
| `~/SwarmLoc/lora_feather_esp32/` | `communication_hardware/radio/lora/` | `sensor_interactions/tools/i2c_scanner/` + `position_denial_research/theory/tof_ranging_basics.md` (extract from notes.md) | HIGH / MED for I2C-scanner boundary |
| `~/SwarmLoc/docs/DW1000_LIBRARY_BUG_FIX.md` | `communication_hardware/uwb/dw1000/docs/` | — | HIGH |
| `~/SwarmLoc/findings/UWB_Swarm_Ranging_Architecture_Research.md` | `position_denial_research/uwb_ranging/` | — | HIGH |
| `~/SwarmLoc/findings/README_Research_Findings.md` | `position_denial_research/uwb_ranging/` | — | HIGH |
| `~/SwarmLoc/findings/UWB_Implementation_Code_Examples.md` | `position_denial_research/uwb_ranging/` OR `communication_hardware/uwb/dw1000/docs/examples/` | — | MED (per-file re-read needed) |
| `~/floppi/swarm_api/` | `swarm_api` | — | HIGH |
| `~/floppi/fc_tool/` | `fc_tool` | — | HIGH |
| `~/floppi/darpa_lift_2026/` | `darpa_lift_2026` | — | HIGH |
| `~/floppi/docs/EKF_*` (3 files) | `auto_orientation_research/theory/ekf/` | — | HIGH |
| `~/floppi/docs/QUATERNION_*` (4 files) | `auto_orientation_research/theory/quaternions/` | — | HIGH |
| `~/floppi/docs/BEC_Wiring.md`, `POWER_MODULE_WIRING.md` | `flight_controller/hardware/power/` | — | HIGH |
| `~/floppi/docs/IMU_GPS_SENSOR_FUSION.md` | `auto_orientation_research/theory/sensor_fusion/` | (cross-ref `position_denial_research/`) | MED |
| `~/floppi/docs/DRONE_APPLICATIONS_REFERENCE.md` | `research/` | `darpa_lift_2026/` (if LIFT-shaped) | MED |
| `~/floppi/docs/FEATURE_COMPARISON.md` | `flight_controller/docs/history/` | — | HIGH |
| `~/floppi/docs/PHASE_3_*` (2 files) | `auto_orientation_research/docs/history/` | — | HIGH |
| `~/floppi/docs/ARCHITECTURE.md`, `MIGRATION_SUMMARY.md`, `ORGANIZATION_SUMMARY.md`, `SCOPE_REFACTOR_COMPLETE.md`, `README.md`, `ROADMAP.md`, `scope.md` | **stays in `floppi/` archive** | — | HIGH |
| `~/floppi/docs/flight-controller/` (whole subdir) | `flight_controller/docs/` | — | HIGH |
| `~/floppi/docs/flight-computer/` | `flight_controller/docs/flight-computer/` | `swarm_api/docs/history/` | MED |
| `~/floppi/docs/findings/` (bootloader cluster, 7 files) | `flight_controller/findings/bootloader/` | — | HIGH |
| `~/floppi/docs/findings/bno_cross_project_2026-05-20.md`, `calibration-test-results-2026-02-12.md` | `auto_orientation_research/findings/` | — | HIGH |
| `~/floppi/docs/findings/teensy-serial-troubleshooting.md` | `flight_controller/findings/` | — | HIGH |
| `~/floppi/docs/findings/day_status_2026-05-20.md` | **stays in `floppi/` archive** | (extract per-project bits) | MED |
| `~/floppi/docs/findings/marine_core_drone.md` | `research/references/` or discard | — | LOW (trivial) |
| `~/floppi/docs/guides/GPS_TROUBLESHOOTING.md` | `sensor_interactions/gps/troubleshooting/` | `flight_controller/findings/gps/` | MED |
| `~/floppi/docs/literature/` (whole subdir) | `research/literature/` | — | HIGH |
| `~/floppi/docs/todo/TASKS.md`, `docs/archive/*`, `docs/_archived/*` | **stays in `floppi/` archive** | — | HIGH |
| `~/floppi/research/ARDUINO_DIAGNOSTICS.md` | `auto_orientation_research/findings/` | — | HIGH |
| `~/floppi/research/CAMERA_EXTRINSIC_CALIBRATION.md` | `sensor_interactions/camera/theory/` | (cross-ref `auto_orientation_research/`) | HIGH |
| `~/floppi/research/GPS_*` (4 files) | `position_denial_research/theory/gps/` | `research/gps/` | MED-HIGH |
| `~/floppi/literature/` PDFs — see §10 table | mixed (see §10) | — | HIGH per-file |
| `~/floppi/archived_docs/TASK_COMPLETION_SUMMARY.md` | **stays in `floppi/` archive** | `auto_orientation_research/docs/history/` | HIGH |
| `~/floppi/scripts/build_coordinate_frame_tests.sh` | `auto_orientation_research/scripts/` | — | HIGH |
| `~/floppi/scripts/GPS_IMPLEMENTATION_EXAMPLES.py` | `position_denial_research/examples/` OR `sensor_interactions/gps/examples/` | — | MED (per-file re-read) |
| `~/floppi/tools/recover_bootloader.sh` | `flight_controller/tools/bootloader_recovery/` | — | HIGH |
| `~/floppi/tools/setup_permissions.sh` | `flight_controller/tools/` (or shared) | — | MED |
| `~/floppi/tools/researchhub_client.py` | `research/tools/researchhub_client/` | — | HIGH |
| `~/floppi/tmp.md` (43KB) | `swarm_communication_protocol` (seed doc — per master plan) | — | HIGH |
| `~/floppi/dRehmFlight-master/` | `flight_controller/vendored/dRehmFlight/` | — | HIGH |
| `~/floppi/FOLDER_STRUCTURE.md`, `README.md` (top-level) | **stays in `floppi/` archive** | — | HIGH |

---

## 15. Items still needing per-file decisions (LOW/MED confidence flags)

1. **`~/SwarmLoc/DWS1000_UWB/docs/findings/` per-file split** — hardware-quirk notes vs ranging-methodology notes. Recommend a 30-min pass at move-time; sort into `communication_hardware/uwb/dw1000/docs/findings/` (hardware) vs `position_denial_research/uwb_ranging/findings/` (methodology).
2. **`~/SwarmLoc/findings/UWB_Implementation_Code_Examples.md`** — 33KB. Read at move-time to decide `communication_hardware/uwb/dw1000/docs/examples/` vs `position_denial_research/uwb_ranging/`.
3. **`~/floppi/docs/flight-computer/` subdir** — the "flight computer" concept overlaps `flight_controller` (peer) and `swarm_api` (WiFi ground-station). Read scope.md at move-time.
4. **`~/floppi/docs/DRONE_APPLICATIONS_REFERENCE.md`** — could be `research/` or `darpa_lift_2026/context/`. Read full doc at move-time.
5. **`~/floppi/docs/findings/day_status_2026-05-20.md`** — cross-project session snapshot with per-project bits. Extract at move-time or leave in archive.
6. **`~/floppi/docs/guides/GPS_TROUBLESHOOTING.md`** — sensor bring-up vs FC-adjacent. `sensor_interactions/gps/troubleshooting/` is primary recommendation.
7. **`~/floppi/scripts/GPS_IMPLEMENTATION_EXAMPLES.py`** (730 lines) — read function names / first 30 lines at move-time to decide denial vs plain GPS.
8. **`~/floppi/tools/setup_permissions.sh`** — belongs to one repo or shared across all embedded-hardware repos? Currently pinning to `flight_controller/tools/` as pragmatic default.
9. **`~/floppi/literature/` centralize-vs-pin split** — user chose "centralize into `research/`" (Q-E) but hardware datasheets are more useful pinned to the repo that consumes them. Recommend hybrid (see §10) with `research/literature/INDEX.md` maintaining cross-repo references.
10. **`~/floppi/research/GPS_*`** — `position_denial_research/theory/gps/` vs `research/gps/`. Slight preference for `position_denial_research` because GPS-denial is a specific research target, not general literature; `research/` is for centralized/general.
11. **`~/floppi/docs/findings/marine_core_drone.md`** — trivial 1-line link. Likely discard, or drop in `research/references/`.

---

## 16. Surprises + replan triggers

1. **`DWS1000_UWB/tests/` scale.** There are ~60 `.cpp` files — many are single-iteration debug variants (`test_rx_v9d_swap.cpp`, `test_rx_v8f_nocheck.cpp`, …). Whoever moves this content should decide whether to prune to a canonical set or preserve as-is (recommend preserve — the naming is the record of the debug journey). The parallel `tests_new/` folder already looks like a cleaner reorganization by the previous session — worth using as the "canonical" test set post-move.
2. **`lora_feather_esp32/notes.md` mixes hardware setup with ToF/positioning theory.** ~140 lines. Split point is obvious (positioning math is a distinct section) but requires extract-and-move. Flagged in §4 above.
3. **`esp32_field_node/` docs explicitly reference `floppi/flight_controller/` and `GravityProbe/esp32_ewpa2_iic_091/`** as reuse sources for WPA2-Enterprise. There's a `GravityProbe/` folder referenced that I did NOT see in either `~/floppi/` or `~/SwarmLoc/` — likely a **third source location** the user has elsewhere. **REPLAN TRIGGER:** ask user about `GravityProbe/` and whether it should also be reorged.
4. **`swarm_api/` has a full `deploy/` folder with menu.sh, service.sh, etc.** — heavier than a "simple API" implies. It's basically shipped with its own systemd wrapping. Might warrant an ops/README in the new repo.
5. **`fc_tool/tests/` is Python but the app is Tauri (Rust+JS).** ~20 pytest files exercise a serial simulator. Unusual choice, but consistent: they test parsing logic (`test_parse_ansi.py`, `test_parse_dashboard.py`) that has host-side reference implementations. Preserve as-is.
6. **`floppi/docs/ARCHITECTURE.md` says "Level -1"** with an intentional convention pointing down into per-project `docs/architecture/`. This is a **retired** hub — the new taxonomy replaces it. Fine, but flag: any lingering cross-links in the retained repo docs need updating during migration.
7. **`floppi/tools/researchhub_client.py` (2943 lines)** is a single-file zero-dep API client for a "ResearchHub" instance. This maps beautifully to the user's Q-E note about "centralized automated-research-tool hookup" for the `research` repo. Confirms the `research` repo is not just a lit dump — it's meant to have tooling.
8. **`floppi/dRehmFlight-master/` was NOT inspected** in this pass (master plan already routes it to `flight_controller/vendored/`). Confidence HIGH from directory name alone.
9. **`flight-computer/` scope.md** describes ESP32/RPi as a WiFi bridge to the Teensy FC — which is arguably the **origin story of what became `swarm_api`**. Could route to `swarm_api/docs/history/` instead of `flight_controller/docs/flight-computer/`. Depends on chronology; flag for the mover.
10. **Nothing found suggesting a `cybersecurity_demos` seed exists yet.** The `esp32_field_node/` is networking, not cyber. Confirms the master plan / user decision that `cybersecurity_demos` waits for a first real cyber POC to materialize.

---

## 17. What now

- Every `HIGH` row is safe to execute against once the new GitLab repos exist. Consumer of this doc: the next planning doc (per master plan §7) — `09_gitlab_operations_checklist.md` should turn the routing table into ordered CRUD steps.
- Every `MED`/`LOW` row wants a 5-30 min per-item deeper read before that step lands. Suggest a `04_per_file_decisions.md` *(never created — those per-file decisions were resolved directly in `11_routing_v2_2026-08-18.md` §2)* doc to burn those down.
- **REPLAN TRIGGER items in §16 (particularly #3 `GravityProbe/` — a possible third source repo)** should surface to the user before Phase 1 execution.
