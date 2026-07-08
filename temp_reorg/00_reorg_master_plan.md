# GitLab reorg — master plan (working draft)

**Status:** 2026-07-08 working draft. NOT yet actionable — captures categorization decisions + open questions before any repo CRUD.
**Scope:** copy content from `~/floppi/` and `~/SwarmLoc/` into repos under the `lowprofiledronegurus` GitLab group. Neither source repo will be deleted; nothing moved in-place.
**Owner will do all repo CRUD manually on GitLab** — this doc is the plan, not the execution.

---

## 1. Repo taxonomy (the framing that emerged)

Repos fall into distinct categories with different roles. Naming and structure should follow the category.

| Category | Purpose | Naming pattern | Structure hint |
|---|---|---|---|
| **A. Pure theory / research** | Papers, findings, protocol design, spec docs. Little/no shipping code. | `<topic>_research` | Docs + design specs + prototypes if any |
| **B. Proofs-of-concept** | Hardware experimentation, sensor validation, test rigs, throwaway sketches. Standardized enough that you can find them later. | `<domain>_interactions` | Subfolders per module/experiment |
| **C. Implementation / production** | Shippable code intended for real drones. | Bare topic name | Standard project layout |
| **D. Design assets** | 3D models, CAD, mechanical drawings | Bare topic name | Asset folders |
| **E. Mission / competition context** | Requirements, transcripts, docs for a specific project/deadline | Named after the event | Docs-heavy |
| **F. Vendored / upstream** | Not moved. Stays as reference inside whichever consuming repo. | Kept as subfolder | N/A |

**Decision recorded:** research-vs-implementation split is **case-by-case by topic maturity** — mature topics (flight_controller) stay single-repo; exploratory topics (position_denial, swarm_comms) get research-only repos, promote to implementation later.

---

## 2. Target repos in `~/lowprofiledronegurus/` (currently 6 cloned, more may be needed)

| Target repo | Category | Present in lowprofiledronegurus/? | Status |
|---|---|---|---|
| `auto_orientation_research` | A theory (+ code case-by-case) | ✅ | Present, empty |
| `drone_frame_modeling` | D design assets | ✅ | Present, empty |
| `fc_tool` | C implementation | ✅ | Present, empty |
| `flight_controller` | C implementation | ✅ | Present, empty |
| `position_denial_research` | A theory | ✅ | Present, empty |
| `sensor_interactions` | B POC directory | ✅ | Present, empty |
| **`swarm_communication_protocol`** | A theory | ❌ | **Need to create on GitLab.** Home for the drone-swarm hex-string language design work from `tmp.md`. |
| **`swarm_api`** | C implementation | ❌ | **Need to create on GitLab.** Python HTTP/WS API currently in `floppi/swarm_api/`. |
| **`darpa_lift_2026`** | E mission | ❌ | **Need to create on GitLab** (or wiki). Currently `floppi/darpa_lift_2026/` — mostly markdown mission-context docs. |
| **`cyber_interactions`** (candidate) | B POC directory | ❌ | Discussed — home for cybersecurity POCs (e.g. commercial-wifi probing, intrusion detection experiments). Not yet decided. |

**Total target: 6 present + at least 3 confirmed-new (`swarm_communication_protocol`, `swarm_api`, `darpa_lift_2026`) + potentially `cyber_interactions` = 9-10 repos.**

---

## 3. Source → target mapping (proposed; open items called out)

### From `~/floppi/`

| Source (in `~/floppi/`) | Proposed target | Confidence | Notes |
|---|---|---|---|
| `auto_orientation/` | `auto_orientation_research` | HIGH | Direct match. Mature enough that implementation code lives with research per case-by-case rule. |
| `flight_controller/` | `flight_controller` | HIGH | Direct match. Mature implementation repo. |
| `fc_tool/` | `fc_tool` | HIGH | Direct match. Host-side tool for interacting with FC. |
| `drone_3d_model/` | `drone_frame_modeling` | HIGH | Direct match. Design assets. |
| `swarm_api/` | `swarm_api` (new repo) | HIGH | Python API. Full project (src/tests/deploy/docs). Own repo — semi-independent from FC. |
| `darpa_lift_2026/` | `darpa_lift_2026` (new repo) | HIGH | Mission docs (avionics_controls.md, comp_requirements.md, initial_research.md, initial_sources.md, notes1.md, transcripts.md). |
| `dRehmFlight-master/` | (stays in `flight_controller/vendored/` or similar) | HIGH | Vendored upstream reference — NOT its own new repo. |
| `docs/` (root-level) | **Per-doc split** | MED | See §4 below — needs per-doc decision. |
| `research/` (root-level) | **Per-doc split** | MED | See §4 below. |
| `literature/` (root-level PDFs) | **Per-topic distribution or group-level literature** | LOW | Datasheets + papers. See §4. |
| `archived_docs/` (root-level) | Historical — keep in `floppi/` or discard | MED | Archived task summaries. May not need to move. |
| `scripts/`, `tools/` | Per-tool routing | LOW | Depends on what's in them (need recon per tool). |
| `FOLDER_STRUCTURE.md`, `README.md`, `tmp.md` | Repo-level docs — not carried into new repos | MED | The `tmp.md` swarm-protocol brainstorm content → `swarm_communication_protocol` seed. |
| `temp_reorg/` | This planning folder — stays in `floppi` | HIGH | Not moved. |

### From `~/SwarmLoc/` (README says "positioning system for GPS-denied areas")

| Source (in `~/SwarmLoc/`) | Proposed target | Confidence | Notes |
|---|---|---|---|
| `docs/` (root-level: UWB architecture research, DW1000 library bug fix notes, README_Research_Findings.md, UWB_Implementation_Code_Examples.md, findings/) | `position_denial_research` | HIGH | This is the theory/research spine of SwarmLoc — belongs in the position-denial research repo. |
| `findings/` (root-level) | `position_denial_research` | HIGH | Findings docs — theory. |
| `DWS1000_UWB/` | **Split**: theory docs → `position_denial_research`; code (src/tests/lib/scripts/tools/platformio.ini/.sh) → `sensor_interactions/uwb/dw1000/` | MED | This is the biggest single directory in SwarmLoc — full PlatformIO project. Both theory and POC. Ideal is a split; alt is keep whole thing under `sensor_interactions` and only move the pure-docs to `position_denial_research`. **Open question.** |
| `GPS_module/` | `sensor_interactions/gps/` | HIGH | Two Arduino sketches (basic_GPS_LAT_LONG_LCD_adafruit_featherwing, GPS_OLED_091). Pure POC. |
| `lora_feather_esp32/` | `sensor_interactions/lora/` (or `sensor_interactions/radio/lora/`) | HIGH | Sketches + notes (ESP32_I2C_Scanner, lora_gps-d_node, ard_BaudR_scan, notes.md). Pure POC. |
| `esp32_field_node/` | **Open** — candidate for `cyber_interactions/` OR `sensor_interactions/wifi_probing/` | LOW | User uncertain. It's about commercial-wifi experimentation. **Open question below.** |
| `README.md`, `todo.md` | Repo-level docs — new repos get fresh READMEs | HIGH | Not moved verbatim. |

---

## 4. Open questions that still need your call

### Q-A. `SwarmLoc/DWS1000_UWB/` — split or keep whole?
It's a full PlatformIO project (~15+ folders) with both substantial theory docs AND POC code.
- **Option 1 (split):** theory docs → `position_denial_research`; code + platformio.ini → `sensor_interactions/uwb/dw1000/`
- **Option 2 (whole):** all of it → `sensor_interactions/uwb/dw1000/`, and `position_denial_research` links back
- **Option 3 (whole, other direction):** all of it → `position_denial_research/experiments/dw1000/`, and `sensor_interactions` links back
Recommend Option 1 for cleanest separation of theory-vs-POC.

### Q-B. `SwarmLoc/esp32_field_node/` — where does this go?
User's own words: "perhaps we need to rename that to something else — or rather I don't have enough context of how exactly to categorize it. Perhaps we want to have multiple types of proofs of concepts, so like cybersecurity protocols or practices, that might become a 'cyber_interactions' repo."
- **Option 1:** Create `cyber_interactions` repo now, put `esp32_field_node` there under a sub-name like `wifi_field_node/` or `cellular_probing/`.
- **Option 2:** Put in `sensor_interactions/wifi/` for now; promote to `cyber_interactions` later if that repo materializes.
- **Option 3:** Leave in SwarmLoc for now, decide after next round of cyber-related POCs surfaces.
Needs a read of what's actually inside to recommend.

### Q-C. `floppi/docs/` root-level docs — per-doc distribution
Sample contents observed: `ARCHITECTURE.md`, `BEC_Wiring.md`, `DRONE_APPLICATIONS_REFERENCE.md`, `EKF_API_REFERENCE.md`, `EKF_THEORY.md`, `EKF_TUNING_GUIDE.md`, plus `archive/` and `_archived/`.
Proposed:
- EKF_* → `auto_orientation_research/theory/` (EKF is orientation-adjacent)
- BEC_Wiring → `flight_controller/hardware/` or `drone_frame_modeling/wiring/`
- ARCHITECTURE.md → per-topic — probably `flight_controller` (whichever project it describes)
- DRONE_APPLICATIONS_REFERENCE.md → likely `darpa_lift_2026` or group-level
- archive/, _archived/ → keep in `floppi/` (historical)
Needs your per-file confirmation for the load-bearing ones.

### Q-D. `floppi/research/` root-level docs (GPS-heavy)
Sample: `ARDUINO_DIAGNOSTICS.md`, `CAMERA_EXTRINSIC_CALIBRATION.md`, `GPS_CHEAT_SHEET.txt`, `GPS_COORDINATE_QUICK_REFERENCE.md`, `GPS_COORDINATE_SYSTEMS_INDEX.md`, `GPS_GEODETIC_COORDINATE_SYSTEMS.md`.
Proposed:
- GPS_* → `position_denial_research/theory/gps/` (background reading for GPS-denial work)
- CAMERA_EXTRINSIC_CALIBRATION → `sensor_interactions/camera/` or `auto_orientation_research/theory/` (calibration ties to orientation)
- ARDUINO_DIAGNOSTICS → `sensor_interactions/` general reference OR `flight_controller/diagnostics/`

### Q-E. `floppi/literature/` PDFs
Sample observed: `drehmflight_README.md`, `drehmflight_transcripts.md`, `dRehmFlight VTOL Documentation.pdf`, `DWM1000 Data Sheet.pdf`, `dws1000productbriefv10.pdf`, `Elke...GNC_Testing.pdf`, `fs-ia6b-manual.pdf`, `gy-521_mpu-6050_datasheet.pdf`.
- **Option 1 (distribute by topic):** dRehmFlight PDFs → `flight_controller/references/`; DWM/DWS1000 → `sensor_interactions/uwb/references/`; MPU-6050 → `flight_controller/references/`; FS-iA6B → `flight_controller/references/`; ElkeJohnson paper → `flight_controller/references/` or `drone_frame_modeling/references/`.
- **Option 2 (group-level references):** Put ALL literature in a group-level `references/` repo. Cleaner curation, but adds another repo.
Which do you prefer?

### Q-F. Group-level GitLab structure
Does `lowprofiledronegurus` have sub-groups (folders of repos)? For example:
- `lowprofiledronegurus/research/` (containing `auto_orientation_research`, `position_denial_research`, `swarm_communication_protocol`)
- `lowprofiledronegurus/implementation/` (containing `flight_controller`, `fc_tool`, `swarm_api`)
- `lowprofiledronegurus/pocs/` (containing `sensor_interactions`, `cyber_interactions`)
- `lowprofiledronegurus/context/` (containing `darpa_lift_2026`)
Or is it flat (all repos at same level)?
Sub-groups aid navigation but add discovery friction. Recommend: **flat unless you already have >12 repos**, then reconsider.

### Q-G. What happens to `~/floppi/` and `~/SwarmLoc/` when we're done?
- Kept in place indefinitely as historical archive (your stated preference — "I don't want to delete floppi/SwarmLoc")
- Frozen in place with a top-level `README.md` pointer to the new GitLab locations
- Deprecated with a specific end-of-life date
This affects whether new work should go to the new repos immediately or continue in the source repos for now.

---

## 5. Standards / conventions to establish once (not per repo)

Suggested cross-repo standards to formalize while reorganizing:

- **README shape** — all repos start with a standard header (purpose / status / entry point / how-to-run / cross-refs)
- **INDEX pattern** — for research/POC repos, a `docs/INDEX.md` listing every doc in the repo
- **Session records** — the pattern already used in `floppi/*/docs/archive/session_records/YYYY-MM-DD_*.md` (very useful; keep across all repos)
- **findings/ folder** — for research repos: dated finding docs with a INDEX
- **Category-tag in README** — first line: `**Category:** A / B / C / D / E` so a new visitor knows the repo's role
- **Cross-repo linking convention** — always relative or always absolute GitLab URLs — pick one

Not urgent — can crystallize after first repo migration.

---

## 6. Ordered execution plan (once open questions are resolved)

Phase 0 — decisions (this doc)
Phase 1 — direct-mapping repos (highest confidence): copy floppi content into `auto_orientation_research`, `flight_controller`, `fc_tool`, `drone_frame_modeling`
Phase 2 — new-repo creation on GitLab: `swarm_communication_protocol`, `swarm_api`, `darpa_lift_2026` (+ `cyber_interactions` if Q-B lands there)
Phase 3 — SwarmLoc distribution: apply Q-A and Q-B decisions
Phase 4 — floppi cross-cutting content: apply Q-C, Q-D, Q-E decisions
Phase 5 — README standardization pass across all new repos
Phase 6 — freeze `floppi/` + `SwarmLoc/` with pointer READMEs (Q-G)

You do all repo CRUD manually on GitLab; this doc becomes the checklist.

---

## 7. Where to keep expanding this plan

`temp_reorg/` is the working folder. Additional docs as needed:
- `01_swarm_comms_protocol_seed.md` — extract the tmp.md brainstorm into a seed doc for the new repo
- `02_swarmloc_split_detail.md` — file-by-file Q-A + Q-B decisions
- `03_floppi_docs_routing.md` — per-file Q-C + Q-D + Q-E decisions
- `04_readme_template.md` — the cross-repo README shape
- `05_gitlab_operations_checklist.md` — the manual CRUD steps once decisions are final

Only creating this master file for now; will spawn the others as decisions land.
