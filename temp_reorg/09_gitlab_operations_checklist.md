# 09 — GitLab Operations Checklist (human, manual)

**Status:** 2026-07-09 planning-phase artifact. Final planning doc in the `temp_reorg/` corpus.
**Purpose:** operational **human** checklist for the manual GitLab CRUD + content migration from `~/floppi/`, `~/SwarmLoc/`, `~/GravityProbe/` into the `lowprofiledronegurus` GitLab group. **User performs every step by hand.** This is a checklist, not an automation script.
**Audience:** the user (repo owner) — sits alongside a terminal + browser tab open to GitLab.
**Companions (READ-ONLY reference):**
- `00_reorg_master_plan.md` — master plan; §3 source→target mapping is the migration ground truth
- `01_target_repos_v2.md` — per-repo purpose / scope-in / scope-out / source-content cards
- `02_swarm_protocol_seed.md` — seed content for `swarm_communication_protocol`
- `03_folder_recon_findings.md` — file-by-file recon over floppi + SwarmLoc
- `04_mini_project_setup_guide.md` — bootstrap tree per mini-project (full tree per mini)
- `05_research_repo_scope.md` — internal design for the `research` repo
- `06_readme_template.md` — cross-repo README template + Category tag values
- `07_verbatim_content_registry.md` — files copied verbatim at migration
- `08_gravityprobe_recon.md` — GravityProbe routing recon (produced this wave alongside this doc)
- `~/llm-project-bootstrap/templates/{scope,roadmap,readme,findings,todo}_template.md`

---

## 0. How to use this checklist

- Work top-to-bottom. Phases are ordered by dependency — do not skip forward without reason.
- Every `- [ ]` is a discrete manual action. Tick it when done.
- **STOP markers** (§13) call out where human decision-making is required before proceeding. Do not paper over them.
- **You copy content with `cp -r`** — never `mv`. Sources at `~/floppi/`, `~/SwarmLoc/`, `~/GravityProbe/` stay on disk indefinitely as historical archives (per user directive).
- **You do all repo CRUD manually on GitLab.** No CLI scripts. No automation. No `git init` runs from tooling.
- Rollback strategy: because nothing is moved and nothing is deleted, rollback = delete the target repos on GitLab (or reset them to empty). Sources remain intact.
- Expect this to be a **multi-session effort**. Do not try to finish it in one sitting.

---

## 1. Prerequisites

- [ ] You have GitLab access with repo-creation permissions in the `lowprofiledronegurus` group.
- [ ] Six empty target repos are already cloned at `~/lowprofiledronegurus/`:
  - `auto_orientation_research/`, `drone_frame_modeling/`, `fc_tool/`, `flight_controller/`, `position_denial_research/`, `sensor_interactions/`
- [ ] Source repos on disk and accessible:
  - `~/floppi/` (working tree present)
  - `~/SwarmLoc/` (working tree present)
  - `~/GravityProbe/` (working tree present, on `data-analysis` branch)
- [ ] Bootstrap library at `~/llm-project-bootstrap/`:
  - `guides/PROJECT_SETUP.md`, `guides/DOCUMENTATION_HANDLING.md`, `guides/PROJECT_SCRIPTS.md`, `guides/SESSION_CONDUCT.md`
  - `templates/{scope,roadmap,readme,findings,todo}_template.md`
- [ ] Planning corpus at `~/floppi/temp_reorg/` reviewed end-to-end (docs 00–08 + this doc).
- [ ] Time-budget acknowledgement: this is not one afternoon. Plan for several focused sessions.
- [ ] Optional but recommended: a scratchpad (paper, Obsidian, whatever) to jot per-file routing decisions you make while working through Phase 4.

---

## 2. Phase 0 — Sanity checks

Run before touching anything.

- [ ] `cd ~/GravityProbe && git branch --show-current` returns `data-analysis`.
- [ ] `cd ~/floppi && git status` reports a clean working tree (or committed-known state).
- [ ] `cd ~/SwarmLoc && git status` reports a clean working tree.
- [ ] `cd ~/GravityProbe && git status` reports a clean working tree.
- [ ] Confirm each target repo at `~/lowprofiledronegurus/<repo>/` is truly empty — only `.git/` and at most a stub README. `ls -la ~/lowprofiledronegurus/<repo>/` per repo.
- [ ] Confirm you have `~/lowprofiledronegurus/` cloned locally as the working directory for the new repos (not just a folder — actual clones of the GitLab remotes).
- [ ] Confirm no in-flight branches on any source repo have unmerged work you would silently forget about (`git log --branches --oneline -20` per repo).

---

## 3. Phase 1 — Create the 7 new repos on GitLab

Six repos already exist (see §1). This phase creates the remaining **seven** target repos in the `lowprofiledronegurus` group. Perform these actions in the GitLab web UI unless you prefer a `glab` CLI equivalent.

For each new repo, do:

1. GitLab → `lowprofiledronegurus` group → **New project** → **Create blank project**.
2. Project name = the exact name listed below (lowercase, underscore-separated).
3. Project slug = same as name.
4. Visibility: match the group's default (project-owner choice).
5. **Uncheck** "Initialize repository with a README" — you will seed the README manually from `06_readme_template.md`.
6. Create the project.
7. From your local machine: `cd ~/lowprofiledronegurus && git clone <gitlab-url-of-new-repo>`.
8. Confirm `~/lowprofiledronegurus/<new-repo>/` exists and contains only `.git/`.

The seven repos:

- [ ] **`swarm_communication_protocol`** — Category A theory. Home for the hex-string swarm messaging design (seeded from `02_swarm_protocol_seed.md`).
- [ ] **`swarm_api`** — Category C implementation. Python HTTP/WS API currently at `floppi/swarm_api/`.
- [ ] **`darpa_lift_2026`** — Category E mission context. Mostly markdown mission docs (currently `floppi/darpa_lift_2026/`).
- [ ] **`communication_hardware`** — Category B-comm POC directory (multi-mini-project). UWB / RF / LoRa hardware POCs.
- [ ] **`networking_pocs`** — Category B-network POC directory (multi-mini-project). WiFi / cellular / networking POCs.
- [ ] **`research`** — Category A centralized research-staging (multi-mini-project). Literature + mini research projects + automated-research-tool hookup.
- [ ] **`cybersecurity_demos`** — Category B-cyber POC directory. **DEFERRED. Do not create yet.** Materialize when the first cyber POC lands.

Post-condition: `ls ~/lowprofiledronegurus/` shows 12 subfolders (6 pre-existing + 6 newly cloned; `cybersecurity_demos` deferred).

---

## 4. Phase 2 — Seed `docs/{README, scope, roadmap, todo}.md` in every repo

Every repo — single-project or multi-project — gets its base `docs/` tree seeded **before** any content migrates. Do this now so incoming content has a documented home to arrive into.

For each of the 12 repos being seeded this wave (13th deferred), do:

1. Read the repo's card in `01_target_repos_v2.md` §1 — this tells you category, purpose, scope-in, scope-out, source content, notes.
2. Consult `06_readme_template.md` for the README shape. Pick **Variant 1** (single-project) or **Variant 2** (multi-project) per the category tag.
3. Consult `~/llm-project-bootstrap/templates/{scope,roadmap,readme,todo}_template.md` for the shape of the other three files.
4. Create the following files at `~/lowprofiledronegurus/<repo>/`:
   - `README.md` (top-level, one-liner + link to `docs/`)
   - `docs/README.md`
   - `docs/scope.md`
   - `docs/roadmap.md`
   - `docs/todo.md`
5. For multi-project repos, additionally create `docs/INDEX.md` (empty index stub, to be filled during Phase 3).

Per-repo checklist:

- [ ] `auto_orientation_research/` — Variant 1. **User note:** README must be vehicle-agnostic (applies to rovers/land vehicles, not drones only) per `01_target_repos_v2.md` §1.1 Notes.
- [ ] `drone_frame_modeling/` — Variant 1. Category D.
- [ ] `fc_tool/` — Variant 1. Category C.
- [ ] `flight_controller/` — Variant 1. Category C. **Do not disturb established layout** — this is the most mature repo (`01_target_repos_v2.md` §1.4 Notes).
- [ ] `position_denial_research/` — Variant 1. Category A theory-only.
- [ ] `sensor_interactions/` — Variant 2 (multi-mini-project). Open README with "what's here vs. what's next door" cross-ref to `communication_hardware` and `networking_pocs` per `01_target_repos_v2.md` §1.6 Notes.
- [ ] `swarm_communication_protocol/` — Variant 1. Category A pure spec.
- [ ] `swarm_api/` — Variant 1. Category C.
- [ ] `darpa_lift_2026/` — Variant 1. Category E.
- [ ] `communication_hardware/` — Variant 2.
- [ ] `networking_pocs/` — Variant 2.
- [ ] `research/` — Variant 2. Internal design per `05_research_repo_scope.md`.

Post-condition: every seeded repo has `docs/{README, scope, roadmap, todo}.md` (and `docs/INDEX.md` for multi-project ones) before Phase 4 content migration starts.

---

## 5. Phase 3 — Mini-project scaffolding for the 5 multi-project repos

Reference: `04_mini_project_setup_guide.md` §3 (directory shape) + §6 (mini-project docs file-by-file) + §11 (initial mini-project stubs per repo).

For each mini-project you scaffold, create the full bootstrap tree per `04` §3:

```
<repo>/<mini-project-path>/
├── docs/
│   ├── README.md
│   ├── scope.md
│   ├── roadmap.md
│   ├── todo.md
│   ├── features/    (empty directory — create with .gitkeep if needed)
│   ├── findings/    (empty directory — create with .gitkeep if needed)
│   └── archive/     (empty directory — create with .gitkeep if needed)
├── src/             (empty; firmware POCs get platformio.ini here at migration)
└── tests/
    └── persistent/  (empty)
```

Every mini-project's four `docs/` files pull from `~/llm-project-bootstrap/templates/`. Do NOT create empty `features/` documents; the folders exist, content lands during Phase 4.

### 5.1 `communication_hardware/` mini-projects

- [ ] `uwb/dw1000/` — stub the full tree (bootstrap arrives from `~/SwarmLoc/DWS1000_UWB/` in Phase 4).
- [ ] `radio/lora/` — stub the full tree (content from `~/SwarmLoc/lora_feather_esp32/` in Phase 4).
- [ ] Do NOT stub `radio/rfm69/`, `radio/cellular_rf/`, `radio/2p4_ghz_mesh/`, or additional UWB modules yet — future candidates per `04` §11.1. Empty mini-projects rot; wait for the first artifact.

### 5.2 `networking_pocs/` mini-projects

- [ ] `wifi_esp32_field_node/` — stub the full tree (content from `~/SwarmLoc/esp32_field_node/` in Phase 4).
- [ ] Additional GravityProbe-sourced ESP32/WiFi POCs — **STOP marker.** See §13. Do not scaffold until routing is confirmed per `08_gravityprobe_recon.md`.

### 5.3 `sensor_interactions/` mini-projects

Per `01_target_repos_v2.md` §1.6 and the user routing hint for GravityProbe (display/hardware POCs → sensor_interactions):

- [ ] `gps/` — stub if you confirm Q-K routes `~/SwarmLoc/GPS_module/` here (see §13 STOP marker).
- [ ] `arduino_diagnostics/` — candidate; scaffold if `~/floppi/research/ARDUINO_DIAGNOSTICS.md` lands here.
- [ ] Additional stubs (`camera/`, `environmental/`, `lidar/`, `ultrasonic/`) — **do NOT create empty stubs.** Wait for the first artifact per `04` §5.

### 5.4 `research/` mini-projects

Per `05_research_repo_scope.md` and `04` §11.4:

- [ ] `literature/` — folder (not a full mini-project bootstrap tree; it is a shelf).
- [ ] `automated_research_tools/researchhub_client/` — stub the full mini-project tree (source `~/floppi/tools/researchhub_client.py` lands here in Phase 4).
- [ ] Additional `mini_projects/*` seeds — **STOP marker.** See §13 (Q-L.2 open in `00_reorg_master_plan.md` §4).

### 5.5 `cybersecurity_demos/`

- [ ] **DEFERRED.** No scaffolding. Do not create the repo (Phase 1) and do not stub mini-projects. Revisit at Phase 9 when the first cyber POC lands.

Post-condition: multi-project repos have their confirmed mini-project shells present; deferred/unconfirmed mini-projects are documented as STOPs (§13), not empty-stubbed.

---

## 6. Phase 4 — Content migration

**Golden rules:**
- Every operation is `cp -r <source> <target>` — never `mv`.
- Confirm target directory exists (Phase 3) before copying.
- After each `cp -r`, `ls` the target to eyeball what arrived.
- Update the receiving mini-project's `docs/INDEX.md` (repo level) and `docs/README.md` (mini level) as content lands.

Reference: `00_reorg_master_plan.md` §3 (source→target mapping) + `03_folder_recon_findings.md` (per-file recon on floppi + SwarmLoc) + `08_gravityprobe_recon.md` (GravityProbe routing).

Execute in the four sub-phases below.

### 6.1 Phase 4a — Direct-mapping repos (highest confidence)

These are single-source, whole-tree copies with HIGH confidence per `00_reorg_master_plan.md` §3.

- [ ] `cp -r ~/floppi/auto_orientation/. ~/lowprofiledronegurus/auto_orientation_research/` (whole tree merges under repo root; adjust destination path if you prefer content lands under `src/` — decide per Variant-1 shape).
- [ ] `cp -r ~/floppi/flight_controller/. ~/lowprofiledronegurus/flight_controller/`.
- [ ] `cp -r ~/floppi/dRehmFlight-master ~/lowprofiledronegurus/flight_controller/vendored/dRehmFlight-master`.
- [ ] `cp -r ~/floppi/fc_tool/. ~/lowprofiledronegurus/fc_tool/`.
- [ ] `cp -r ~/floppi/drone_3d_model/. ~/lowprofiledronegurus/drone_frame_modeling/`.
- [ ] After each: read the destination `docs/README.md` and update source-repo pointers if needed.

### 6.2 Phase 4b — New single-project repos

- [ ] `swarm_communication_protocol`: seed the repo from `02_swarm_protocol_seed.md` — copy `~/floppi/tmp.md` into `swarm_communication_protocol/docs/archive/tmp_original_brainstorm.md` (per `07_verbatim_content_registry.md` "Additional files to add here" candidate — confirm with user first, see §13). Populate `docs/scope.md` from `01_target_repos_v2.md` §1.7.
- [ ] `swarm_api`: `cp -r ~/floppi/swarm_api/. ~/lowprofiledronegurus/swarm_api/`.
- [ ] `darpa_lift_2026`: `cp -r ~/floppi/darpa_lift_2026/. ~/lowprofiledronegurus/darpa_lift_2026/`.

### 6.3 Phase 4c — POC directories (multi-source multi-mini-project)

**`communication_hardware/`:**
- [ ] `cp -r ~/SwarmLoc/DWS1000_UWB/. ~/lowprofiledronegurus/communication_hardware/uwb/dw1000/`. Then: legacy `.cpp` iteration files in `DWS1000_UWB/tests/` — route to `communication_hardware/uwb/dw1000/docs/findings/legacy_iteration_history/` per `04` §14 Q-14b recommendation. Reorganize by hand.
- [ ] `cp -r ~/SwarmLoc/lora_feather_esp32/. ~/lowprofiledronegurus/communication_hardware/radio/lora/`.

**`networking_pocs/`:**
- [ ] `cp -r ~/SwarmLoc/esp32_field_node/. ~/lowprofiledronegurus/networking_pocs/wifi_esp32_field_node/`.
- [ ] GravityProbe ESP32/WiFi content — **STOP marker.** Per `08_gravityprobe_recon.md`, route each subfolder (`esp32_enterprise_wpa3_eap/`, `esp32_ent_wpa2_peap_web80/`, `esp32_ewpa2_iic_091/`, `esp32_hw125_sd/`, `teensy_esp8266/`, `teensy_esp8266_wifi_home/`) per the recon's recommendation. Confirm each routing with the user before `cp -r`.

**`sensor_interactions/`:**
- [ ] `~/SwarmLoc/GPS_module/` — pending Q-K resolution (see §13). If GPS-as-sensor: `cp -r ~/SwarmLoc/GPS_module/. ~/lowprofiledronegurus/sensor_interactions/gps/`. If GPS-denial-experiment: routes to `position_denial_research/gps_experiments/` instead.
- [ ] GravityProbe display/hardware POCs — per user routing hint: display/hardware POCs → `sensor_interactions`. Candidates from `~/GravityProbe/`: `teensy_i2c_oled/`, `teensy_mpu6050/`, `teensy_mpu6050_zero/`, `teensy_sdcard/`, `i2c_scanner_whoami/`. Route per `08_gravityprobe_recon.md`. Confirm each with user before `cp -r`.
- [ ] `~/floppi/research/ARDUINO_DIAGNOSTICS.md` — `cp` into `sensor_interactions/arduino_diagnostics/docs/findings/` (create the mini-project stub inline if not done in Phase 3).

**GravityProbe quaternion / math code:**
- [ ] Per user routing hint: quaternion/math → `auto_orientation_research`. `~/GravityProbe/matlab_scripts/` and `~/GravityProbe/matlab.md` — route to `auto_orientation_research/docs/findings/quaternion_math_from_gravityprobe/` (or similar; confirm per `08_gravityprobe_recon.md`).

**GravityProbe "other" POCs:**
- [ ] Route by best-fit per `08_gravityprobe_recon.md` recon judgment. Anything without a clear home stays uncopied for now — do NOT force-place.

### 6.4 Phase 4d — `research/` repo

Per `05_research_repo_scope.md` and `03_folder_recon_findings.md`:

- [ ] `cp -r ~/floppi/literature/. ~/lowprofiledronegurus/research/literature/`. Trim any datasheets that clearly belong to a specific topic repo — those get copied out to `<topic>/docs/references/` per `04` §10 (Datasheet dual-homing rule).
- [ ] `cp ~/floppi/tools/researchhub_client.py ~/lowprofiledronegurus/research/automated_research_tools/researchhub_client/src/researchhub_client.py`.
- [ ] `research/mini_projects/*` initial seeds — **STOP marker.** See §13 (Q-L.2 open).
- [ ] `~/floppi/docs/` root-level files — **STOP marker per Q-C** (see `00_reorg_master_plan.md` §4 Q-C). Route per-file. EKF_*.md → `auto_orientation_research/theory/`; BEC_Wiring.md → `flight_controller/hardware/` or `drone_frame_modeling/wiring/` (user picks); DRONE_APPLICATIONS_REFERENCE.md → `darpa_lift_2026/` or `research/` (user picks); archive/, _archived/ → stay in `floppi/`.
- [ ] `~/floppi/research/` root-level docs — **STOP marker per Q-D.** GPS_*.md → `position_denial_research/theory/gps/` or `research/gps/` (user picks); CAMERA_EXTRINSIC_CALIBRATION.md → `sensor_interactions/camera/` or `auto_orientation_research/theory/` (user picks).

Post-condition: source-→target mapping in `00_reorg_master_plan.md` §3 is fully executed **except** for items still gated by STOP markers.

---

## 7. Phase 5 — Verbatim content copies

Per `07_verbatim_content_registry.md` — these files are copied byte-for-byte, not paraphrased.

- [ ] `cp /home/devel/floppi/temp_reorg/chat-export-chatgpt-2026-07-09.md ~/lowprofiledronegurus/swarm_communication_protocol/docs/archive/chat-export-chatgpt-2026-07-09.md`.
- [ ] Secondary target (candidate — only if `research/mini_projects/swarm_protocol_design/` materializes): also copy to `research/mini_projects/swarm_protocol_design/docs/archive/chat-export-chatgpt-2026-07-09.md`. Skip if the mini-project is not stubbed.
- [ ] Additional verbatim entries — none registered at time of writing. If new entries are added to `07_verbatim_content_registry.md`, re-visit this phase.

---

## 8. Phase 6 — README standardization pass

Apply `06_readme_template.md` uniformly across every repo. Do this after content migration so any inline README bits salvaged from source repos have been reconciled.

For each of the 12 seeded repos:

- [ ] Category tag line is present, first line, drawn from `06_readme_template.md` fixed list.
- [ ] `Status:` line present with one of `early | active | stable | archived`.
- [ ] One-line description matches the repo's `01_target_repos_v2.md` §1 purpose.
- [ ] `## Overview` present, 2–3 sentences.
- [ ] `## Quick Start` present (even if `TODO: fill in when scripts land`).
- [ ] Cross-repo links stubbed (see `04` §10 rule: absolute GitLab URLs across repos, or flagged local placeholders during pre-migration).
- [ ] `docs/` tree links resolve — no dead links to files that never made it over.
- [ ] For multi-project repos: `docs/INDEX.md` link present + resolves, and INDEX enumerates every scaffolded mini-project with `status: active | stub | dormant | archived | graduated`.

Repo checklist:

- [ ] `auto_orientation_research/README.md` standardized.
- [ ] `drone_frame_modeling/README.md` standardized.
- [ ] `fc_tool/README.md` standardized.
- [ ] `flight_controller/README.md` standardized.
- [ ] `position_denial_research/README.md` standardized.
- [ ] `sensor_interactions/README.md` standardized.
- [ ] `swarm_communication_protocol/README.md` standardized.
- [ ] `swarm_api/README.md` standardized.
- [ ] `darpa_lift_2026/README.md` standardized.
- [ ] `communication_hardware/README.md` standardized.
- [ ] `networking_pocs/README.md` standardized.
- [ ] `research/README.md` standardized.

---

## 9. Phase 7 — Verification pass

Walk every repo once more with fresh eyes and tick every item.

- [ ] Every repo has `docs/{README.md, scope.md, roadmap.md, todo.md}`.
- [ ] Every multi-project repo has `docs/INDEX.md` and it enumerates every mini-project with a status.
- [ ] Every scaffolded mini-project has its own `docs/{README.md, scope.md, roadmap.md, todo.md}` + `docs/features/` + `docs/findings/` + `docs/archive/` + `src/` + `tests/persistent/` per `04` §3.
- [ ] Cross-repo links resolve. Any that cannot (target file missing) are flagged with `TODO: verify post-migration` inline rather than left silently broken.
- [ ] No dead references to `~/floppi/...` or `~/SwarmLoc/...` or `~/GravityProbe/...` **inside the new repos**. Every such reference now points at the corresponding new-repo location.
- [ ] Bootstrap conventions honored:
  - No time estimates in any `roadmap.md` (per `04` §6.3 + `PROJECT_SETUP.md`).
  - No emojis in README unless the user has explicitly asked for them.
  - Category tag on every README.
  - Mini-project layout uses the fixed seven filenames (`README.md`, `scope.md`, `roadmap.md`, `todo.md`, `features/`, `findings/`, `archive/`) — no renames.
- [ ] `00_reorg_master_plan.md` §3 source→target mapping is fully realized (or every gap is a documented STOP marker in §13).
- [ ] Test-discipline check: no repo prescribes a heavy test suite. Persistent floor is small (per `04` §8). Ephemeral tests default. Onboard diagnostics carry the health-check load.

---

## 10. Phase 8 — Source repo pointer notes

Source repos remain on disk indefinitely as historical archives — nothing is deleted. Add a self-documenting pointer so the source repos advertise the migration.

For each source repo, do **one** of:

- **Option A (recommended):** create a top-level `POINTER.md` file containing a short "content migrated to `lowprofiledronegurus/<repos>`" note. Preserves existing `README.md` untouched.
- **Option B:** update the top of the existing `README.md` with a "**MIGRATED — see `lowprofiledronegurus`**" banner.

Per source repo:

- [ ] `~/floppi/POINTER.md` (or `README.md` banner) — points at the ~10 target repos that received floppi content.
- [ ] `~/SwarmLoc/POINTER.md` (or `README.md` banner) — points at `position_denial_research`, `communication_hardware`, `networking_pocs`, `sensor_interactions`.
- [ ] `~/GravityProbe/POINTER.md` (or `README.md` banner) — points at `sensor_interactions`, `networking_pocs`, `auto_orientation_research` per user routing hint.

Exact wording of POINTER.md — **STOP marker.** See §13.

**Do NOT delete anything from source repos.** Do NOT rename source folders. Do NOT force-push. The source repos are read-only from this point forward, by convention.

---

## 11. Phase 9 — Post-migration

Housekeeping after the main migration lands.

- [ ] Address remaining open questions in `00_reorg_master_plan.md` §4 case-by-case as they surface during actual work in the new repos:
  - Q-C: `floppi/docs/` per-file routing (partial resolutions logged during Phase 4d STOP)
  - Q-D: `floppi/research/` per-file routing (partial resolutions logged during Phase 4d STOP)
  - Q-G: source-repo lifecycle (POINTER.md handles the "frozen archive" case)
  - Q-I: cross-cutting POC routing convention (DNS-driveby example — pick canonical home + cross-link back)
  - Q-J: sensor_interactions final scope confirmation
  - Q-K: GPS_module routing (sensor vs. GPS-denial)
  - Q-L: `research/` internal structure refinement, `research/mini_projects/` seeds
  - Q-M: `swarm_api` scope (drone-specific vs. general multi-agent)
  - Q-N: `auto_orientation_research` renaming decision
- [ ] Materialize `cybersecurity_demos` when the first cyber POC lands (Phase 1 deferred, Phase 3 deferred, Phase 4 deferred).
- [ ] Establish cross-repo linking convention (relative vs. absolute GitLab URL) — pick one and enforce (per `00_reorg_master_plan.md` §5).
- [ ] Establish POC graduation convention — `GRADUATED_TO.md` pointer at POC folder root when a POC matures (per `00_reorg_master_plan.md` §5).

---

## 12. Cross-reference: which planning doc governs which phase

| Phase | Governing planning docs |
|---|---|
| Phase 0 (sanity) | This checklist §2 |
| Phase 1 (create repos) | `00_reorg_master_plan.md` §2, `01_target_repos_v2.md`, `06_readme_template.md` |
| Phase 2 (seed docs) | `01_target_repos_v2.md`, `06_readme_template.md`, `~/llm-project-bootstrap/templates/*` |
| Phase 3 (mini-project scaffolding) | `04_mini_project_setup_guide.md` §3, §6, §11 |
| Phase 4a (direct-mapping) | `00_reorg_master_plan.md` §3, `01_target_repos_v2.md` |
| Phase 4b (new single-project) | `01_target_repos_v2.md`, `02_swarm_protocol_seed.md` |
| Phase 4c (POC directories) | `03_folder_recon_findings.md`, `08_gravityprobe_recon.md`, `04` §11 |
| Phase 4d (`research/`) | `05_research_repo_scope.md`, `03_folder_recon_findings.md`, `00` §4 Q-C/Q-D |
| Phase 5 (verbatim copies) | `07_verbatim_content_registry.md` |
| Phase 6 (README standardization) | `06_readme_template.md` |
| Phase 7 (verification) | Every prior doc (verification cross-check) |
| Phase 8 (source pointers) | `00_reorg_master_plan.md` §4 Q-G |
| Phase 9 (post-migration) | `00_reorg_master_plan.md` §4 open questions |

---

## 13. STOP markers — human decisions required before proceeding

Ordered by phase. Each is a place where the checklist cannot proceed until you decide.

1. **Phase 3.2 / Phase 4c — GravityProbe subfolder routing.** `~/GravityProbe/` contains ~15 subfolders. Per user hint: display/hardware → `sensor_interactions`, ESP32/WiFi → `networking_pocs`, quaternion/math → `auto_orientation_research`, other → best-fit. Confirm each subfolder's destination per `08_gravityprobe_recon.md` before `cp -r`.
2. **Phase 4b — `~/floppi/tmp.md` verbatim preservation into `swarm_communication_protocol/docs/archive/`.** Currently a candidate under `07_verbatim_content_registry.md` "Additional files to add here." Confirm before the copy.
3. **Phase 4c — `~/SwarmLoc/GPS_module/` routing (Q-K).** `sensor_interactions/gps/` or `position_denial_research/gps_experiments/` or split? Decide.
4. **Phase 4d — `~/floppi/docs/` per-file split (Q-C).** EKF_*.md, BEC_Wiring.md, ARCHITECTURE.md, DRONE_APPLICATIONS_REFERENCE.md, archive/, _archived/ — confirm each destination.
5. **Phase 4d — `~/floppi/research/` per-file split (Q-D).** GPS_*.md, CAMERA_EXTRINSIC_CALIBRATION.md, ARDUINO_DIAGNOSTICS.md — confirm each destination.
6. **Phase 4d / Phase 3.4 — `research/mini_projects/` initial seeds (Q-L.2).** Which exploratory research inquiries seed the `research` repo now vs. later? Do not empty-stub.
7. **Phase 8 — POINTER.md wording.** Exact text of the "content migrated to lowprofiledronegurus" pointer notes. Decide once, reuse across all three source repos.

Do NOT paper over these with default choices — every STOP marker exists because the user has held sign-off on it.

---

## 14. Rollback / undo

Because every operation in Phases 4–5 is a copy (not a move) and no source file is ever deleted, rollback is bounded:

- [ ] To undo a mis-copied file: delete it from the target repo. Source is untouched.
- [ ] To undo a mis-scaffolded mini-project: delete the mini-project folder from the target repo. Source is untouched.
- [ ] To undo a whole target repo: delete the repo on GitLab (or `git reset --hard` to the pre-migration commit + force-push, if you want the repo shell to survive). Local clone at `~/lowprofiledronegurus/<repo>/` can be deleted separately.
- [ ] To undo the migration entirely: delete every target repo on GitLab; remove `~/lowprofiledronegurus/`. `~/floppi/`, `~/SwarmLoc/`, `~/GravityProbe/` are unaffected.

**Do not** try to "undo" by editing source repos. Sources are read-only from the moment Phase 8 pointers land.

---

## 15. Cross-refs table — planning corpus at a glance

| Doc | Path | Role |
|---|---|---|
| 00 master plan | `~/floppi/temp_reorg/00_reorg_master_plan.md` | Category taxonomy + source→target mapping + open questions |
| 01 target repos v2 | `~/floppi/temp_reorg/01_target_repos_v2.md` | Per-repo cards (purpose, scope-in, scope-out, source) |
| 02 swarm protocol seed | `~/floppi/temp_reorg/02_swarm_protocol_seed.md` | Seed content for `swarm_communication_protocol` |
| 03 folder recon | `~/floppi/temp_reorg/03_folder_recon_findings.md` | File-by-file recon over floppi + SwarmLoc |
| 04 mini-project guide | `~/floppi/temp_reorg/04_mini_project_setup_guide.md` | Bootstrap tree per mini-project |
| 05 research repo scope | `~/floppi/temp_reorg/05_research_repo_scope.md` | Internal design for the `research` repo |
| 06 README template | `~/floppi/temp_reorg/06_readme_template.md` | Cross-repo README shape (two variants) |
| 07 verbatim registry | `~/floppi/temp_reorg/07_verbatim_content_registry.md` | Files copied byte-for-byte at migration |
| 08 GravityProbe recon | `~/floppi/temp_reorg/08_gravityprobe_recon.md` | GravityProbe routing recon (per user hint) |
| 09 this checklist | `~/floppi/temp_reorg/09_gitlab_operations_checklist.md` | Operational human checklist (this doc) |
| Bootstrap templates | `~/llm-project-bootstrap/templates/*.md` | scope, roadmap, readme, findings, todo |
| Bootstrap guides | `~/llm-project-bootstrap/guides/*.md` | PROJECT_SETUP, DOCUMENTATION_HANDLING, PROJECT_SCRIPTS, SESSION_CONDUCT |

---

*This checklist is a planning-phase artifact. It prescribes what a human does by hand; it does not automate anything. No migration scripts, no repo CRUD, no `git commit` runs are performed by tooling. Every action is a user decision, executed manually.*
