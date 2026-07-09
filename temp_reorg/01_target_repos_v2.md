# Target repos v2 — per-repo purpose, scope, source mapping

**Status:** 2026-07-09 draft. Supersedes the target-repo table in `00_reorg_master_plan.md` §2 with Q-A/Q-B/Q-E/Q-F decisions applied. Names marked LOW confidence are placeholders pending user sign-off.

## 0. Purpose

This doc defines the 12-13 repos under the flat GitLab group `lowprofiledronegurus` — one card per repo covering purpose, in-scope/out-of-scope, source-content routing from `~/floppi/` and `~/SwarmLoc/`, and naming confidence. It is the canonical answer to "which repo does X go in?" and is the reference every subsequent routing doc (per-file splits, README templates, checklist) links back to. Categories A/B/C/D/E map to the taxonomy in `00_reorg_master_plan.md` §1.

---

## 1. Repo cards

### 1.1 `auto_orientation_research`
- **Category:** A theory (with maturing implementation code co-located, per case-by-case rule)
- **Purpose:** Attitude/orientation estimation research — IMU + magnetometer fusion, EKF theory + tuning, sensor-calibration methodology. Provides ground for any vehicle that needs a stable orientation estimate.
- **Scope IN:** EKF derivations + tuning docs, IMU/mag calibration procedures, Madgwick/Mahony/complementary-filter comparison, PlatformIO test rigs for orientation algorithms, balance-robot demonstrator (already in `auto_orientation/QUICKSTART_BALANCE_ROBOT.md`), orientation-related datasheets.
- **Scope OUT:** Full flight-control loop (→ `flight_controller`), GPS-denial multilateration (→ `position_denial_research`), UWB/RF hardware POCs (→ `communication_hardware`), camera-based orientation experiments as pure-POC (→ `sensor_interactions`).
- **Source content:** `~/floppi/auto_orientation/` (whole tree); `~/floppi/docs/EKF_*.md`; `~/floppi/research/CAMERA_EXTRINSIC_CALIBRATION.md` (calibration is orientation-adjacent); `~/floppi/literature/gy-521_mpu-6050_datasheet.pdf`, `MPU-6000-Datasheet1.pdf`.
- **Naming confidence:** HIGH
- **Notes:** User comment — **not drone-specific**; applies to rovers/land vehicles too. README must be vehicle-agnostic; do not lean on drone-only framing.

### 1.2 `drone_frame_modeling`
- **Category:** D design assets
- **Purpose:** Mechanical/3D design of the airframe — CAD sources, generated STL/OBJ, reference models, wiring diagrams, printable parts.
- **Scope IN:** OpenSCAD/FreeCAD/STEP sources, generated meshes, reference third-party models, physical wiring/pin-out diagrams, mechanical BOM, print-config notes, ROADMAP/SCOPE for the mechanical design track.
- **Scope OUT:** Electrical firmware (→ `flight_controller`), aero-/simulation research (→ `research` if theoretical, `auto_orientation_research` if orientation-tied), competition-specific frame requirements (→ `darpa_lift_2026` as mission ref, mirrored here as design constraint).
- **Source content:** `~/floppi/drone_3d_model/` (whole tree — `docs/`, `generated/`, `reference_models/`, `scripts/`, `sources/`, `ROADMAP.md`, `SCOPE.md`, `README.md`); `~/floppi/docs/BEC_Wiring.md` (physical wiring diagram belongs with mechanical/electrical layout).
- **Naming confidence:** HIGH
- **Notes:** `researchhub_client.py` at repo root is tooling — carry over or reroute per fc_tool/research decision.

### 1.3 `fc_tool`
- **Category:** C implementation
- **Purpose:** Host-side operator tool (Tauri desktop app) for interacting with the flight controller — calibration, telemetry, parameter dumps, mission uploads.
- **Scope IN:** Tauri app source (`src/`, `src-tauri/`), Node package manifests, dev-setup scripts, deploy script, host-side unit tests, tool-facing docs.
- **Scope OUT:** Firmware-side handlers (→ `flight_controller`), swarm-level orchestration APIs (→ `swarm_api`), 3D visualization assets for the frame (→ `drone_frame_modeling` if static, stays here if runtime).
- **Source content:** `~/floppi/fc_tool/` (whole tree).
- **Naming confidence:** HIGH
- **Notes:** Semi-independent from firmware but shares the FC serial protocol contract — cross-link to `flight_controller` docs for the wire protocol.

### 1.4 `flight_controller`
- **Category:** C implementation
- **Purpose:** Onboard firmware for the flight controller (Teensy 4.x + ESP32 co-processor) — control loops, sensor drivers, radio input, motor mixing, HAL.
- **Scope IN:** PlatformIO project (`src/`, `include/`, `lib/`, `lib_esp32/`, `platformio.ini`), build scripts (`build.sh`, `build.bat`), hardware-in-the-loop tests, session records, FC architecture docs, vendored `dRehmFlight-master/` reference (as `vendored/dRehmFlight-master/`), radio + receiver datasheets, FC-specific ARCHITECTURE.md.
- **Scope OUT:** Host-side operator tool (→ `fc_tool`), orientation-estimation research (→ `auto_orientation_research`), GPS-denial recovery theory (→ `position_denial_research`), swarm messaging protocol (→ `swarm_communication_protocol`).
- **Source content:** `~/floppi/flight_controller/` (whole tree); `~/floppi/dRehmFlight-master/` → `vendored/dRehmFlight-master/`; `~/floppi/docs/ARCHITECTURE.md` (FC-scope architecture); `~/floppi/literature/dRehmFlight VTOL Documentation.pdf`, `drehmflight_README.md`, `drehmflight_transcripts.md`, `fs-ia6b-manual.pdf`, `RFM69HCW-V1.1.pdf`, `Longfly dRehmFlight Purchase Lists.pdf`, `longfly pcb.webp`, `init_about_refactored_drehmflight.md`.
- **Naming confidence:** HIGH
- **Notes:** Most mature repo — established layout must not be disturbed by reorg.

### 1.5 `position_denial_research`
- **Category:** A theory
- **Purpose:** Theoretical spine for operating without GPS — multilateration math, UWB ranging theory, GPS-degradation modeling, DNS-driveby-style positioning, fallback-mode design.
- **Scope IN:** Multilateration/TDoA/TWR derivations, UWB architecture research, algorithm write-ups, GPS coordinate-systems reference material, findings/lessons-learned docs, DNS-driveby positioning theory (cross-cut with `networking_pocs`).
- **Scope OUT:** UWB hardware bring-up + PlatformIO code (→ `communication_hardware`), WiFi-network probing hardware POCs (→ `networking_pocs`), swarm-level protocol design (→ `swarm_communication_protocol`), IMU dead-reckoning-only work (→ `auto_orientation_research`).
- **Source content:** `~/SwarmLoc/docs/` (UWB architecture research, `README_Research_Findings.md`, `UWB_Implementation_Code_Examples.md`); `~/SwarmLoc/findings/`; `~/SwarmLoc/DWS1000_UWB/docs/` (theory subset); `~/floppi/research/GPS_*.md` + `GPS_CHEAT_SHEET.txt`.
- **Naming confidence:** HIGH
- **Notes:** Repo is theory only — every POC lives elsewhere with a bidirectional cross-link back here. DNS-driveby is cross-cutting with `networking_pocs`.

### 1.6 `sensor_interactions`
- **Category:** B POC-by-domain (narrowed)
- **Purpose:** POC directory for **non-radio, non-networking** sensor bring-up — environmental, optical, ranging, imaging. One subfolder per sensor family.
- **Scope IN:** Camera POCs (image capture, extrinsic-calibration rigs), LiDAR bring-up, ultrasonic ranging sketches, barometer/temperature/humidity experiments, GPS-receiver hardware POCs (GPS is a sensor, not a radio-comm hardware family), general-purpose Arduino/ESP32 diagnostic sketches, I2C/SPI scanner utilities.
- **Scope OUT:** UWB/DW1000 + LoRa + RFM69 (→ `communication_hardware`), WiFi/cellular field nodes (→ `networking_pocs`), IMU-only rigs (→ `auto_orientation_research` — orientation is its own track), production sensor drivers integrated into FC (→ `flight_controller/lib/`).
- **Source content:** `~/SwarmLoc/GPS_module/` (both Arduino sketches); `~/floppi/research/ARDUINO_DIAGNOSTICS.md` (general diagnostic reference). Camera POCs will land here as they materialize.
- **Naming confidence:** MED
- **Notes:** **Rescoped in v2** — original catchment (radio, network, camera, everything) is now split three ways. Repo README must open with an explicit "what's here vs. what's next door" cross-ref block to `communication_hardware` and `networking_pocs`.

### 1.7 `swarm_communication_protocol`
- **Category:** A theory
- **Purpose:** Design of the inter-drone/hex-string message language used by swarm members to exchange state, commands, and negotiation packets. Spec, message-shape catalog, versioning strategy.
- **Scope IN:** Protocol specification, message-format catalog (hex encoding), state-machine + handshake docs, reference message dumps, encoding/decoding pseudocode, wire-compatibility rules, versioning policy, discussion of collision/loss handling.
- **Scope OUT:** Python HTTP/WS server implementation (→ `swarm_api`), radio hardware bring-up used to carry the protocol (→ `communication_hardware`), competition-specific message profiles (→ `darpa_lift_2026` cross-refs).
- **Source content:** `~/floppi/tmp.md` (protocol brainstorm — canonical seed). Future protocol drafts + revisions.
- **Naming confidence:** HIGH
- **Notes:** Pure spec repo; reference implementations link back here for the contract.

### 1.8 `swarm_api`
- **Category:** C implementation
- **Purpose:** Host-side Python service (HTTP + WebSocket) that mediates between operators and the swarm — issues commands, aggregates telemetry, exposes swarm state.
- **Scope IN:** `src/` server + handlers, `tests/`, deployment scripts + `deploy/`, `requirements.txt`, `config.json`, `docs/` (API reference), integration harness against protocol reference messages.
- **Scope OUT:** Protocol design itself (→ `swarm_communication_protocol` — dependency), operator desktop UI (→ `fc_tool` if that grows to swarm-scale, else new repo later), FC firmware (→ `flight_controller`).
- **Source content:** `~/floppi/swarm_api/` (whole tree).
- **Naming confidence:** HIGH
- **Notes:** Depends on `swarm_communication_protocol` — pin the protocol version in `config.json` or equivalent.

### 1.9 `darpa_lift_2026`
- **Category:** E mission / competition context
- **Purpose:** Single source of truth for the DARPA LIFT 2026 competition — requirements, avionics-controls constraints, source material, meeting transcripts, deadlines.
- **Scope IN:** `avionics_controls.md`, `comp_requirements.md`, `initial_research.md`, `initial_sources.md`, `notes1.md`, `transcripts.md`, future meeting notes + task lists tied to the competition, cross-refs from implementation repos ("this requirement drove this code").
- **Scope OUT:** General drone reference material not tied to the competition (→ `research`), implementation code (→ `flight_controller` / `swarm_api` / etc.), mechanical design constraints beyond mission text (→ `drone_frame_modeling` mirrored constraint doc).
- **Source content:** `~/floppi/darpa_lift_2026/` (whole tree, all 6 markdown docs); `~/floppi/docs/DRONE_APPLICATIONS_REFERENCE.md` if it turns out to be mission-scoped.
- **Naming confidence:** HIGH
- **Notes:** Could be a GitLab wiki instead of a repo; user preference deferred. Treat as repo for now for parity with other targets.

### 1.10 `communication_hardware` *(placeholder name)*
- **Category:** B POC-by-domain (**new** in v2 per Q-A resolution)
- **Purpose:** POCs for radio + RF + UWB communication **hardware** — bring-up, ranging, air-link characterization. One subfolder per module family.
- **Scope IN:** DW1000 / DWS1000 UWB PlatformIO project + tests + tools + platformio.ini + upload scripts, LoRa (RFM95 / feather) sketches + tuning notes, RFM69 sketches, future 2.4GHz mesh POCs, hardware-side ranging measurement rigs, air-link datasheets.
- **Scope OUT:** Multilateration/positioning theory (→ `position_denial_research`), swarm-message protocol design (→ `swarm_communication_protocol`), WiFi/cellular networking POCs (→ `networking_pocs`), production integration of the ranging module into FC (→ `flight_controller/lib/`).
- **Source content:** `~/SwarmLoc/DWS1000_UWB/` (whole PlatformIO project minus `docs/` theory subset — code, tests, scripts, tools, upload scripts, platformio.ini, README, archive); `~/SwarmLoc/lora_feather_esp32/` (all 5 sketches + `adafruit_lora.md`, `notes.md`); `~/floppi/literature/DWM1000 Data Sheet.pdf`, `dws1000productbriefv10.pdf`, `RFM69HCW-V1.1.pdf` (if not already routed to `flight_controller`).
- **Naming confidence:** LOW (candidates: `communication_hardware` / `communication_hardware_testing` / `comms_hw`)
- **Notes:** POC layer under the theory in `position_denial_research`. Cross-link every subfolder README back to relevant theory doc. Repo README must include a "graduation" note — when a module matures into production, code moves to `flight_controller/lib/`.

### 1.11 `networking_pocs` *(placeholder name)*
- **Category:** B POC-by-domain (**new** in v2 per Q-B resolution)
- **Purpose:** POCs for network-layer experimentation — commercial WiFi, cellular, DNS-driveby-style positioning, mesh networking, opportunistic connectivity. Distinct from radio-hardware POCs by operating **at the network stack**, not the PHY.
- **Scope IN:** ESP32 WiFi field-node bring-up (`esp32_field_node`), cellular-modem POCs, DNS-driveby positioning proof-of-concept, WiFi survey/scan sketches, captive-portal experiments, opportunistic uplink prototypes, mesh/OLSR/BATMAN experiments.
- **Scope OUT:** Cybersecurity offensive/defensive tooling (→ `cybersecurity_demos` when it materializes), physical radio bring-up like LoRa/UWB (→ `communication_hardware`), swarm-message protocol on top of any transport (→ `swarm_communication_protocol`), positioning theory (→ `position_denial_research`).
- **Source content:** `~/SwarmLoc/esp32_field_node/` (whole tree).
- **Naming confidence:** LOW (candidates: `networking_pocs` / `networking_proof_of_concepts`)
- **Notes:** DNS-driveby is **cross-cutting** with `position_denial_research` — theory lives there, hardware POC lives here. Cybersecurity-adjacent POCs stay here until cyber concerns become primary, then graduate to `cybersecurity_demos`.

### 1.12 `research` *(placeholder name)*
- **Category:** A theory (broad literature + mini research projects)
- **Purpose:** Centralized staging area for reference literature, mini research projects, and automated research-tool hookups. **Not just PDFs** — also active research experiments that don't yet belong to a topic-specific research repo.
- **Scope IN:** Papers/PDFs not clearly owned by a topic repo, mini research projects (~1-2 file explorations), automated-research-tool integration (e.g. researchhub client wiring), literature indexes, aspirational reading lists, cross-topic surveys.
- **Scope OUT:** Topic-scoped theory that has a dedicated repo (EKF → `auto_orientation_research`, GPS theory → `position_denial_research`, protocol design → `swarm_communication_protocol`), mission context (→ `darpa_lift_2026`), PDFs that are datasheets for a specific POC (→ that POC's repo `references/`).
- **Source content:** `~/floppi/literature/` items not clearly topic-owned — e.g. `Morphy_ A Compliant and Morphologically Aware Flying Robot.pdf`, `ElkeJohnsonRoslanskyGebre-Egziabinry...GNC_Testing.pdf`, `resources.md` (index); `~/floppi/research/` general items (calibration surveys, etc.); future automated-research-tool outputs.
- **Naming confidence:** LOW (candidates: `research` / `research_hub` / `research_staging`)
- **Notes:** User comment — content **can be copied out** to other repos when a piece finds a specific home. This is the canonical staging area; distribution to topic repos is a downstream operation. Consider a `docs/INDEX.md` that lists every reference with its "eventual home" tag.

### 1.13 `cybersecurity_demos` *(candidate future, placeholder name)*
- **Category:** B POC-by-domain (**not created yet**)
- **Purpose:** POCs for encryption, intrusion detection, jamming resilience, spoofing-defense, and offensive/defensive cyber concerns specific to drone systems.
- **Scope IN:** IDS-on-CAN experiments, GPS-spoofing detection sketches, radio-jamming characterization, encrypted-comms overlays for the swarm protocol, side-channel-analysis test rigs.
- **Scope OUT:** General WiFi/network POCs without a security angle (→ `networking_pocs`), pure protocol design (→ `swarm_communication_protocol`), radio hardware bring-up (→ `communication_hardware`).
- **Source content:** None yet. Materializes when the first cyber POC lands (candidate: security-hardened variant of `esp32_field_node` work).
- **Naming confidence:** LOW (candidates: `cybersecurity_demos` / `cyber_interactions` / `security_pocs`)
- **Notes:** Do **not** create on GitLab until first POC is ready. Until then, cyber-adjacent work lives in `networking_pocs` with a `cybersecurity/` subfolder if needed.

---

## 2. Cross-cutting concerns

- **Cross-domain POCs.** Some experiments straddle two repos by design. Rule: the **primary artifact** (hardware POC vs. theory) determines home; the other repo gets a `SEE_ALSO.md` (or README cross-ref block) pointing at it. Concrete examples:
  - **DNS-driveby positioning** — hardware POC in `networking_pocs`, theory + math in `position_denial_research`.
  - **UWB ranging** — hardware POC in `communication_hardware`, multilateration theory + findings in `position_denial_research`.
  - **Camera extrinsic calibration** — calibration POC in `sensor_interactions/camera/`, theory tie-in in `auto_orientation_research`.
- **POC → dedicated-repo graduation.** When a POC in a `B` repo matures into shippable code, it moves to the relevant `C` repo (typically `flight_controller/lib/` or `swarm_api/`). Original POC folder stays in place with a `STATUS.md` noting "graduated to <repo>@<sha>" so history is preserved. Do not delete graduated POC folders.
- **Cross-linking convention.** Use **absolute GitLab URLs** for links across repos (survives clones, permalinks). Use **relative paths** only within the same repo. Formalize in the shared README template (`04_readme_template.md` when drafted).
- **Literature dual-homing.** Datasheets used by exactly one POC live in that POC repo's `references/`. Broader survey papers stay in `research`. If in doubt, keep in `research` — distribution is cheaper than reunion.
- **Vendored code.** `dRehmFlight-master` stays inside `flight_controller/vendored/`. No standalone repo. Apply the same rule to any future upstream vendored code (keep near consumer, do not multiply repos).
- **`auto_orientation_research` vehicle-agnostic framing.** Because orientation research applies to rovers/land vehicles too, README + doc voice must avoid drone-only assumptions. Add a "Applicable platforms" callout.

---

## 3. Naming decisions still needed

Names below are placeholders — user has not confirmed the final slug. Recommend confirming these before any GitLab repo creation.

| # | Placeholder in this doc | Candidate options |
|---|---|---|
| 10 | `communication_hardware` | `communication_hardware` / `communication_hardware_testing` / `comms_hw` / `radio_hardware_pocs` |
| 11 | `networking_pocs` | `networking_pocs` / `networking_proof_of_concepts` / `network_pocs` |
| 12 | `research` | `research` / `research_hub` / `research_staging` / `literature_and_research` |
| 13 | `cybersecurity_demos` | `cybersecurity_demos` / `cyber_interactions` / `security_pocs` (deferred — do not create yet) |

Six existing repos in `~/lowprofiledronegurus/` are **already-named on GitLab** — assumed frozen: `auto_orientation_research`, `drone_frame_modeling`, `fc_tool`, `flight_controller`, `position_denial_research`, `sensor_interactions`. Three confirmed-new (`swarm_communication_protocol`, `swarm_api`, `darpa_lift_2026`) are HIGH confidence.

**Total: 4 names to confirm** (repos 10, 11, 12, 13) — of which 3 need creation now (10, 11, 12) and 1 is deferred (13).
