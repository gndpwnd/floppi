# GitLab reorg — master plan (working draft)

**Status:** 2026-07-09 working draft. Q-A / Q-B / Q-E / Q-F / Q-H resolved this round; all 4 placeholder repo names locked in; **GravityProbe added as third source repo** (on `data-analysis` branch); the multi-source SOURCE_MAP pattern was considered and **dropped** per user (content migrates cleanly into new repos, no per-mini-project source provenance tracking).
**Last updated:** 2026-07-09.
**Scope:** copy content from `~/floppi/`, `~/SwarmLoc/`, and `~/GravityProbe/` into repos under the `lowprofiledronegurus` GitLab group. None of the three source repos will be deleted; they stay on local disk indefinitely as historical archives.
**Owner will do all repo CRUD manually on GitLab** — this doc is the plan, not the execution. See `09_gitlab_operations_checklist.md` (in flight this wave) for the operational human checklist.

---

## 1. Repo taxonomy (the framing that emerged)

Repos fall into distinct categories with different roles. Naming and structure should follow the category.

The **B / proof-of-concept** category is now split **by hardware/technology domain** rather than being one catch-all POC repo. Each domain gets its own POC bucket so hardware experiments are easy to find and don't tangle across radio/sensor/network/cyber lines.

The **A / research** category now covers two patterns: (1) a topic-specific pure-research repo (e.g. `position_denial_research`) and (2) a centralized **research-staging** repo (`research`) that hosts literature, mini research projects, and automated-research-tool hookups — pieces graduate outward to topic-specific repos as they mature.

| Category | Purpose | Naming pattern | Structure hint |
|---|---|---|---|
| **A. Pure theory / research** | Papers, findings, protocol design, spec docs. Little/no shipping code. **Two flavors:** topic-specific research repo, AND a centralized research-staging repo for literature + mini-projects + automated-research pipelines. | `<topic>_research` OR bare `research` for the staging repo | Docs + design specs + prototypes if any |
| **B-comm. Communication hardware POCs** | UWB, RF, LoRa, radio-communication-hardware experiments. Hardware-domain POCs. | `communication_hardware` | Subfolders per module (`uwb/dw1000/`, `radio/lora/`, ...) |
| **B-network. Networking POCs** | WiFi/cellular/DNS-driveby-style networking experiments. Software-networking-domain POCs (connecting to commercial WiFi, cellular probing, DNS-based positioning, etc.). | `networking_pocs` | Subfolders per experiment |
| **B-sensor. Non-radio/non-networking sensor POCs** | Camera, environmental, lidar, ultrasonic, GPS-module, IMU-standalone rigs, etc. Whatever isn't a radio/network POC. Re-scoped narrower now. | `sensor_interactions` | Subfolders per sensor family |
| **B-cyber. Cybersecurity POCs (candidate)** | Encryption experiments, IDS, intrusion detection, security tooling. Materializes when first cyber POC lands. | `cybersecurity_demos` | Subfolders per experiment |
| **C. Implementation / production** | Shippable code intended for real drones (or land vehicles / rovers, where applicable). | Bare topic name | Standard project layout |
| **D. Design assets** | 3D models, CAD, mechanical drawings | Bare topic name | Asset folders |
| **E. Mission / competition context** | Requirements, transcripts, docs for a specific project/deadline | Named after the event | Docs-heavy |
| **F. Vendored / upstream** | Not moved. Stays as reference inside whichever consuming repo. | Kept as subfolder | N/A |

**Decision recorded:** research-vs-implementation split is **case-by-case by topic maturity** — mature topics (flight_controller) stay single-repo; exploratory topics (position_denial, swarm_comms) get research-only repos, promote to implementation later.

**Decision recorded (this round):** POC bucket is **domain-split, not monolithic**. New POCs land in the domain-appropriate B-* repo. A POC that spans two domains (e.g. DNS-driveby positioning) lives at its hardware/software home and the theory-side use case gets a link from the research-side repo.

---

## 2. Target repos in `~/lowprofiledronegurus/` (6 present, 12-13 total planned)

| # | Target repo | Category | Present in lowprofiledronegurus/? | Status |
|---|---|---|---|---|
| 1 | `auto_orientation_research` | A theory (+ code case-by-case) | YES | Present, empty. **Scope note:** IMU/magnetometer research is domain-agnostic — applies to rovers and other land vehicles, not drones only. Possible rename — see Q-N. |
| 2 | `drone_frame_modeling` | D design assets | YES | Present, empty |
| 3 | `fc_tool` | C implementation | YES | Present, empty |
| 4 | `flight_controller` | C implementation | YES | Present, empty |
| 5 | `position_denial_research` | A theory | YES | Present, empty. GPS-denial recovery, multilateration theory. |
| 6 | `sensor_interactions` | B-sensor POCs | YES | Present, empty. **Re-scoped narrower:** now non-radio, non-networking sensor POCs only (camera, lidar, ultrasonic, environmental, standalone IMU, possibly GPS-module). See Q-J. |
| 7 | `swarm_communication_protocol` | A theory | NO — need to create | Home for the drone-swarm hex-string language design work from `tmp.md`. |
| 8 | `swarm_api` | C implementation | NO — need to create | Python HTTP/WS API currently in `floppi/swarm_api/`. Scope open — see Q-M. |
| 9 | `darpa_lift_2026` | E mission | NO — need to create (or wiki) | Currently `floppi/darpa_lift_2026/` — mostly markdown mission-context docs. |
| 10 | `communication_hardware` | B-comm POCs | NO — need to create | **Name FINAL (Q-H resolved).** UWB/RF/LoRa/radio hardware POCs. |
| 11 | `networking_pocs` | B-network POCs | NO — need to create | **Name FINAL (Q-H resolved).** WiFi/cellular/DNS-driveby-style networking experiments. |
| 12 | `research` | A centralized research-staging | NO — need to create | **Name FINAL (Q-H resolved).** Centralized literature + mini research projects + automated-research-tool hookup. Copies out to topic repos as pieces mature. Internal structure locked in `05_research_repo_scope.md`. |
| 13 | `cybersecurity_demos` | B-cyber POCs | NO — deferred (Phase 7) | **Name FINAL (Q-H resolved).** Materializes when first cyber POC surfaces. |

**Total target:** 6 present + 5 confirmed-new (7, 8, 9, 10, 11) + 1 near-term-new (12 `research`) + 1 candidate (13 `cybersecurity_demos`) = **12-13 repos**.

---

## 3. Source → target mapping (proposed; open items called out)

**Migration philosophy (locked this round):** content migrates cleanly into the new repos. This section is a **one-time migration reference only** — there is no ongoing source-provenance tracking per mini-project (the multi-source SOURCE_MAP pattern was considered and dropped). Old source repos stay on the user's local disk indefinitely.

### Source repos (3 confirmed)

| Source repo | Local path | Branch | Notes |
|---|---|---|---|
| floppi | `~/floppi` | default (main) | Cross-cutting: flight controller, auto orientation, swarm API, darpa lift 2026 mission context, literature, research staging. |
| SwarmLoc | `~/SwarmLoc` | default (main) | GPS-denied positioning: DW1000 UWB PlatformIO project, LoRa sketches, ESP32 WiFi field node, GPS module sketches, docs/findings. |
| GravityProbe | `~/GravityProbe` | **`data-analysis` branch** (confirmed this round) | POC-heavy: display/hardware POCs, ESP32 commercial-WiFi/networking POCs, quaternion math, misc small POCs. Per-folder routing in `08_gravityprobe_recon.md`. |

### From `~/floppi/`

| Source (in `~/floppi/`) | Proposed target | Confidence | Notes |
|---|---|---|---|
| `auto_orientation/` | `auto_orientation_research` | HIGH | Direct match. Mature enough that implementation code lives with research per case-by-case rule. |
| `flight_controller/` | `flight_controller` | HIGH | Direct match. Mature implementation repo. |
| `fc_tool/` | `fc_tool` | HIGH | Direct match. Host-side tool for interacting with FC. |
| `drone_3d_model/` | `drone_frame_modeling` | HIGH | Direct match. Design assets. |
| `swarm_api/` | `swarm_api` (new repo) | HIGH | Python API. Full project (src/tests/deploy/docs). Own repo — semi-independent from FC. Scope open — see Q-M. |
| `darpa_lift_2026/` | `darpa_lift_2026` (new repo) | HIGH | Mission docs (avionics_controls.md, comp_requirements.md, initial_research.md, initial_sources.md, notes1.md, transcripts.md). |
| `dRehmFlight-master/` | (stays in `flight_controller/vendored/` or similar) | HIGH | Vendored upstream reference — NOT its own new repo. |
| `literature/` (root-level PDFs) | `research/literature/` | **HIGH (resolved Q-E)** | Centralized under new `research` repo. Individual PDFs may be copied out to topic repos as needed, but canonical home is `research/literature/`. |
| `docs/` (root-level) | **Per-doc split** | MED | See Q-C below — per-file recon still pending. Some pieces to `research`, some to topic repos. |
| `research/` (root-level) | **Per-doc split** | MED | See Q-D below — some pieces to `research` repo (centralized), some to `position_denial_research` (GPS-heavy), some to `sensor_interactions` or `auto_orientation_research`. |
| `archived_docs/` (root-level) | Historical — keep in `floppi/` or discard | MED | Archived task summaries. May not need to move. |
| `scripts/`, `tools/` | Per-tool routing | LOW | Depends on what's in them (need recon per tool). |
| `FOLDER_STRUCTURE.md`, `README.md`, `tmp.md` | Repo-level docs — not carried into new repos | MED | The `tmp.md` swarm-protocol brainstorm content → `swarm_communication_protocol` seed. |
| `temp_reorg/` | This planning folder — stays in `floppi` | HIGH | Not moved. |

### From `~/SwarmLoc/` (README says "positioning system for GPS-denied areas")

| Source (in `~/SwarmLoc/`) | Proposed target | Confidence | Notes |
|---|---|---|---|
| `docs/` (root-level: UWB architecture research, DW1000 library bug fix notes, README_Research_Findings.md, UWB_Implementation_Code_Examples.md) | `position_denial_research` | HIGH | This is the theory/research spine of SwarmLoc — belongs in the position-denial research repo. |
| `findings/` (root-level) | `position_denial_research/findings/` | HIGH | Findings docs — theory. |
| `DWS1000_UWB/` (whole subfolder) | `communication_hardware/uwb/dw1000/` | **HIGH (resolved Q-A)** | POC-by-domain: UWB is a communication-hardware POC, so the whole PlatformIO project lands under the new `communication_hardware` repo. Pure-theory docs inside it may be duplicated/linked into `position_denial_research`. |
| `GPS_module/` | `sensor_interactions/gps/` OR `position_denial_research/gps_experiments/` | MED | Two Arduino sketches. GPS-module POC — is this "sensor" or "position-denial experiment"? **See Q-K.** |
| `lora_feather_esp32/` | `communication_hardware/radio/lora/` | HIGH (resolved via Q-A framing) | LoRa is radio-communication-hardware. Sketches + notes (ESP32_I2C_Scanner, lora_gps-d_node, ard_BaudR_scan, notes.md). |
| `esp32_field_node/` | `networking_pocs/wifi_esp32_field_node/` | **HIGH (resolved Q-B)** | Commercial-WiFi field-node experiment — networking POC by domain. Lands in the new `networking_pocs` repo. NOT cybersecurity (yet); cyber gets its own repo when a cyber POC lands. |
| `README.md`, `todo.md` | Repo-level docs — new repos get fresh READMEs | HIGH | Not moved verbatim. |

### From `~/GravityProbe/` (data-analysis branch)

Per-folder routing detail lives in `08_gravityprobe_recon.md` (in flight this wave). Summary routing hints per user guidance:

| GravityProbe content family | Proposed target | Notes |
|---|---|---|
| Display / hardware POCs (small POCs, sensor interactions) | `sensor_interactions` | User quote: "perhaps different displays will be different hardware/sensor interactions but the POCs are so small we can just throw it in with the sensor interactions". |
| ESP32 commercial WiFi / networking POCs | `networking_pocs` | Networking-domain POCs. User quote: "the esp32 stuff network stuff might get harder to classify but i'll let you deal with all that". |
| Quaternion / attitude / orientation math | `auto_orientation_research` | Math for orientation belongs with the orientation-research repo. |
| Other POCs | Best-fit by recon agent's judgment | Full per-folder table in `08_gravityprobe_recon.md`. |

### Cross-cutting: DNS-driveby-style WiFi positioning

The Hackster article the user cited (DNS-based WiFi positioning) is a **cross-cutting POC**: the hardware/software work lives in `networking_pocs`, but the theoretical use case (positioning without GPS) is `position_denial_research` material. See Q-I for the routing convention.

---

## 4. Open questions

### Resolved this round

- **Q-A. `SwarmLoc/DWS1000_UWB/` — split or keep whole?** RESOLVED. Whole subfolder lands in **new `communication_hardware` repo** under `uwb/dw1000/`. POC-by-hardware-domain — UWB is a communication-hardware POC. Any pure-theory pieces inside can be linked/duplicated from `position_denial_research` if useful.
- **Q-B. `SwarmLoc/esp32_field_node/`.** RESOLVED. Goes to **new `networking_pocs` repo** under `wifi_esp32_field_node/`. Networking (WiFi) POC by domain. Cybersecurity gets its own `cybersecurity_demos` repo when a cyber POC materializes — not this one.
- **Q-E. `floppi/literature/` PDFs.** RESOLVED. Centralize into a new **`research` repo** (`research/literature/`). This repo is more than PDFs — it also hosts mini research projects and the centralized automated-research-tool hookup. Individual PDFs may be copied out to topic repos as convenient, but `research/` is the canonical staging area.
- **Q-F. Group-level GitLab structure.** RESOLVED. **FLAT.** GitLab group cannot host sub-groups in the way we'd want (per user); all repos sit at the same level under `lowprofiledronegurus`.
- **Q-H. Naming for the four new placeholder repos.** RESOLVED this round. All 4 names locked in as final:
  - `communication_hardware` (final)
  - `networking_pocs` (final)
  - `research` (final)
  - `cybersecurity_demos` (final, Phase 7 deferred)

### Also resolved this round (no prior Q-number)

- **Multi-source SOURCE_MAP pattern.** Considered and **DROPPED.** User quote: "no don't worry about sources, i don't want to have to deal with old repositories, i just want all content migrated into the new repos to like start fresh and organized and standardized, but don't delete the old repos from my local disk." Content migrates cleanly with no per-mini-project source provenance tracking. Old repos remain on local disk indefinitely as historical archive.
- **GravityProbe confirmed as third source repo.** On `data-analysis` branch. Per-folder routing captured in `08_gravityprobe_recon.md`. Summary routing hints already applied in §3 above.

### Still open

#### Q-C. `floppi/docs/` root-level docs — per-doc distribution
Sample contents observed: `ARCHITECTURE.md`, `BEC_Wiring.md`, `DRONE_APPLICATIONS_REFERENCE.md`, `EKF_API_REFERENCE.md`, `EKF_THEORY.md`, `EKF_TUNING_GUIDE.md`, plus `archive/` and `_archived/`.
Proposed:
- EKF_* → `auto_orientation_research/theory/` (EKF is orientation-adjacent)
- BEC_Wiring → `flight_controller/hardware/` or `drone_frame_modeling/wiring/`
- ARCHITECTURE.md → per-topic — probably `flight_controller`
- DRONE_APPLICATIONS_REFERENCE.md → likely `darpa_lift_2026` or `research`
- archive/, _archived/ → keep in `floppi/` (historical)
Needs per-file confirmation. Full recon → `03_folder_recon_findings.md`.

#### Q-D. `floppi/research/` root-level docs (GPS-heavy)
Sample: `ARDUINO_DIAGNOSTICS.md`, `CAMERA_EXTRINSIC_CALIBRATION.md`, `GPS_CHEAT_SHEET.txt`, `GPS_COORDINATE_QUICK_REFERENCE.md`, `GPS_COORDINATE_SYSTEMS_INDEX.md`, `GPS_GEODETIC_COORDINATE_SYSTEMS.md`.
Proposed:
- GPS_* → `position_denial_research/theory/gps/` (background for GPS-denial work) — or `research/gps/` if we want centralized staging. See Q-K/Q-L.
- CAMERA_EXTRINSIC_CALIBRATION → `sensor_interactions/camera/` or `auto_orientation_research/theory/`
- ARDUINO_DIAGNOSTICS → `sensor_interactions/` general reference OR `flight_controller/diagnostics/`

#### Q-G. What happens to `~/floppi/` and `~/SwarmLoc/` when we're done?
- Kept in place indefinitely as historical archive (user's stated preference)
- Frozen in place with top-level `README.md` pointer to new GitLab locations
- Deprecated with a specific end-of-life date

### Still open (per-file / migration-time decisions)

#### Q-I. Cross-cutting POC routing convention (DNS-driveby example)
DNS-driveby WiFi positioning spans **networking_pocs** (implementation domain) and **position_denial_research** (theory/use-case domain). Convention needed:
- Option 1: canonical home is the hardware/software domain (`networking_pocs`); theory-side repo links back
- Option 2: canonical home is the use-case domain (`position_denial_research`); hardware repo links back
- Option 3: dual-home — copy relevant pieces both ways
POCs graduate into production/research as they mature — so wherever the POC starts, it needs an obvious "graduated to" pointer.

#### Q-J. `sensor_interactions` re-scoping — what non-radio/non-networking sensors go there?
Now that comm-hardware and networking POCs are moved out, `sensor_interactions` is narrower. Confirm the covered sensor families:
- Cameras (RGB, thermal, event-based)
- Environmental (temperature, pressure, humidity)
- Lidar / ultrasonic / TOF
- Standalone IMU rigs (that aren't `auto_orientation_research`)
- GPS-module? (also see Q-K)
- Anything else?

#### Q-K. `SwarmLoc/GPS_module/` home
GPS-module Arduino sketches — is this:
- `sensor_interactions/gps/` (GPS as a sensor, generic)
- `position_denial_research/gps_experiments/` (GPS characterization for GPS-denial work)
- Both, split?

#### Q-L. `research` repo internal structure
Open design work — what does the `research` repo actually contain?
- `literature/` — PDFs (from `floppi/literature/`)
- `mini_projects/` — one folder per exploratory research project
- `automated_tools/` — hookup point for the centralized automated-research pipeline
- `INDEX.md` at root?
- Per-topic subfolders (`orientation/`, `positioning/`, `swarm/`) or flat?
Full internal design → `04_research_repo_scope.md` (next round).

#### Q-M. `swarm_api` scope — drone-specific or general?
Is the swarm API strictly for the drone project, or does it aim to be a general multi-agent HTTP/WS coordination layer usable outside drones? Impacts naming + README framing.

#### Q-N. `auto_orientation_research` renaming?
User noted IMU/magnetometer research is NOT drone-specific — rovers and other land vehicles use it too. Consider a domain-neutral name:
- `orientation_research`
- `imu_research`
- `attitude_estimation_research`
Or leave as-is (`auto_orientation_research`) since "auto_orientation" is already fairly generic.

---

## 5. Standards / conventions to establish once (not per repo)

Suggested cross-repo standards to formalize while reorganizing:

- **README shape** — all repos start with a standard header (purpose / status / entry point / how-to-run / cross-refs)
- **INDEX pattern** — for research/POC repos, a `docs/INDEX.md` listing every doc in the repo
- **Session records** — the pattern already used in `floppi/*/docs/archive/session_records/YYYY-MM-DD_*.md` (very useful; keep across all repos)
- **findings/ folder** — for research repos: dated finding docs with an INDEX
- **Category-tag in README** — first line: `**Category:** A / B-comm / B-network / B-sensor / B-cyber / C / D / E` so a new visitor knows the repo's role
- **Cross-repo linking convention** — always relative or always absolute GitLab URLs — pick one
- **POC graduation convention** — POCs that mature into production/research get a `GRADUATED_TO.md` pointer at the POC folder root (see Q-I)
- **Project-wide operating principles (locked this round)** — apply across every repo, every wave:
  - **Planning wave before dev wave.** Planning docs finalize decisions; no code, migration scripts, or scaffolds start until the planning corpus is complete and the user green-lights execution.
  - **Test discipline.** Tests default to **ephemeral**; onboard diagnostics carry most of the health-check load. Do not prescribe heavy host/native test suites in any repo's docs. (Most tests belong on hardware, not host.)
  - **Documentation discipline.** Every agent/session writes only inside its declared WRITE_ZONE. No wandering into unrelated docs. Structured sections, explicit headings, source citations by path.
  - **No repo CRUD without user approval.** No agent creates/renames/deletes GitLab repos, git-inits, commits, or branch-switches source repos. All CRUD is manual by the user, driven by `09_gitlab_operations_checklist.md`.

Not urgent — can crystallize after first repo migration, except the operating-principles bullet which is already in force.

---

## 6. Ordered execution plan (once open questions are resolved)

Repo count: **12-13 repos total** (6 present + 5 confirmed-new + 1 near-term-new `research` + 1 candidate `cybersecurity_demos`).

- **Phase 0** — decisions (this doc + follow-on planning docs). **Complete after wave 4.**
- **Phase 1** — direct-mapping repos (highest confidence): copy floppi content into `auto_orientation_research`, `flight_controller`, `fc_tool`, `drone_frame_modeling`.
- **Phase 2** — new-repo creation on GitLab (**7 repos this phase**): `swarm_communication_protocol`, `swarm_api`, `darpa_lift_2026`, `communication_hardware`, `networking_pocs`, `research`. (`cybersecurity_demos` is deferred to Phase 7.) **Operational document: `09_gitlab_operations_checklist.md`** — human step-by-step for repo creation + settings + initial README seed.
- **Phase 3** — **SwarmLoc + GravityProbe distribution.** SwarmLoc: apply resolved Q-A + Q-B (whole `DWS1000_UWB/` → `communication_hardware/uwb/dw1000/`; `esp32_field_node/` → `networking_pocs/wifi_esp32_field_node/`; `lora_feather_esp32/` → `communication_hardware/radio/lora/`; SwarmLoc docs+findings → `position_denial_research`); resolve Q-K for `GPS_module/`. GravityProbe: apply per-folder routing from `08_gravityprobe_recon.md` (display/hardware POCs → `sensor_interactions`; ESP32 WiFi → `networking_pocs`; quaternion math → `auto_orientation_research`; other POCs by best-fit).
- **Phase 4** — floppi cross-cutting content: apply Q-C, Q-D decisions; migrate `floppi/literature/` → `research/literature/` per resolved Q-E. GravityProbe cross-cutting spillover (if any) also handled here.
- **Phase 5** — README standardization pass across all new repos (uses `06_readme_template.md`).
- **Phase 6** — freeze `floppi/`, `SwarmLoc/`, and `GravityProbe/` with pointer READMEs (Q-G). Sources stay on disk indefinitely.
- **Phase 7** (deferred) — create `cybersecurity_demos` when the first cyber POC surfaces.

You do all repo CRUD manually on GitLab; `09_gitlab_operations_checklist.md` is the checklist.

---

## 7. Where to keep expanding this plan

`temp_reorg/` is the working folder. Planning corpus status:

- `00_reorg_master_plan.md` — **this doc** (updated wave 4).
- `01_target_repos_v2.md` — target list detail (per-repo purpose, category, README-seed, folder-structure sketch). ✅ landed.
- `02_swarm_protocol_seed.md` — `tmp.md` brainstorm seed for `swarm_communication_protocol`. ✅ landed.
- `03_folder_recon_findings.md` — floppi + SwarmLoc contents-based routing recon. ✅ landed.
- `04_mini_project_setup_guide.md` — mini-project bootstrap pattern (full tree per mini). ✅ landed.
- `05_research_repo_scope.md` — internal design for the `research` repo (Q-L). ✅ landed.
- `06_readme_template.md` — cross-repo README template. ✅ landed.
- `07_verbatim_content_registry.md` — files copied verbatim at migration. ✅ landed.
- `08_gravityprobe_recon.md` — GravityProbe per-folder routing recon. **In progress this wave.**
- `09_gitlab_operations_checklist.md` — final HUMAN manual CRUD checklist for Phase 2 GitLab repo creation. **In progress this wave.**

**After wave 4 the planning corpus is COMPLETE** and the user is ready to begin GitLab CRUD (Phase 2) following `09_gitlab_operations_checklist.md`.
