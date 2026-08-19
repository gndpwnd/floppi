# 10 — Consolidated operator decision queue

**Status:** open, built 2026-08-18. **Owner of the queue:** jointly maintained by the `floppi`
(source-side) and `lowprofiledronegurus` (target-side) Claude sessions so the operator is asked **once**.
**Rule:** every item carries a recommended default and the cost of defaulting it. Items marked
**HUMAN-ONLY** must not be defaulted. Record the ruling inline, dated, and supersede in place.

Sources: `09_gitlab_operations_checklist.md` §13 (STOP markers S-*), `00_reorg_master_plan.md` §4
(open questions Q-*), plus items N-* surfaced by the 2026-08-18 re-verification against live disk.

---

## Tier 1 — blocks everything (HUMAN-ONLY)

### D-0. Which session is the operator's front door?
The operator told **both** sessions, in near-identical words, that decisions route back through *that*
session. Both of us declined to resolve it by assertion.
**RULED 2026-08-18 — no fixed front door.** Operator: *"don't worry about it, just ask me questions as
needed and it will get sorted out, just stay in communication with other sessions and try not to ask the
same questions as other sessions, know what you are trying to get done."*
**Binding consequence:** either session may ask the operator directly. The obligation is **de-duplication,
not routing** — before asking, check this queue and the bus for whether the other session already owns
that question. Every ruling received in one session is mirrored into this file's ruling log and pushed
over the bus the same turn.

### N-1. The repo list itself — 13 repos: approve, add, delete, or re-scope?
The operator explicitly raised this ("perhaps some repos need to be deleted, some might need to be added,
some might need to have their scope changed"). Live state **observed** 2026-08-18: 6 exist on GitLab with
`main` + stock boilerplate only; 7 absent.

| # | Repo | Cat | State | Note |
|---|---|---|---|---|
| 1 | `auto_orientation_research` | A | exists | rename? → Q-N |
| 2 | `drone_frame_modeling` | D | exists | ← `floppi/drone_3d_model/` |
| 3 | `fc_tool` | C | exists | ← `floppi/fc_tool/` |
| 4 | `flight_controller` | C | exists | ← `floppi/flight_controller/` + vendored dRehmFlight |
| 5 | `position_denial_research` | A | exists | ← SwarmLoc docs/findings + GPS theory |
| 6 | `sensor_interactions` | B-sensor | exists | scope → Q-J |
| 7 | `swarm_communication_protocol` | A | **create** | ← `floppi/tmp.md` + `02_swarm_protocol_seed.md` |
| 8 | `swarm_api` | C | **create** | scope → Q-M |
| 9 | `darpa_lift_2026` | E | **create** | 6 markdown files only — thin; fold instead? |
| 10 | `communication_hardware` | B-comm | **create** | ← SwarmLoc UWB + LoRa |
| 11 | `networking_pocs` | B-network | **create** | ← SwarmLoc field node + GravityProbe ESP32 WiFi |
| 12 | `research` | A | **create** | literature + mini-projects + researchhub_client |
| 13 | `cybersecurity_demos` | B-cyber | **deferred** | no seed content exists yet |

**PARTIALLY ANSWERED 2026-08-18 — the question got bigger, not smaller.** Operator did not approve the
13; they named the real problem instead: *"there is ~/floppi/ ~/lowprofiledronegurus/ ~/engineer360 and
also ~/hiverf/ that will all have overlapping scopes so you might need to coordinate and message the
developers of all of them."*

So **`engineer360` and `hiverf` are IN SCOPE**, both already have GitLab remotes under `gndpwnd/` (not
the `lowprofiledronegurus` group), and both have scopes that **overlap** planned repos:
- `engineer360` (`gitlab.com:gndpwnd/engineer360`) overlaps `drone_frame_modeling` (structural/aero
  sizing) and is the **existing implementation of the parametric sizing/scoring tool** that
  `darpa_lift_2026/notes1.md` specifies as unbuilt. It is also now the destination for the frame-design
  research content per the N-2 ruling.
- `hiverf` (`gitlab.com:gndpwnd/hiverf`) overlaps `position_denial_research` and SwarmLoc hard —
  multilateration/trilateration 2D+3D, DoA, Kalman tracking, multipath, fingerprinting, ~1,996 tests.
  If it lands, `position_denial_research` stops being a theory repo and arrives with a mature simulation
  platform instead.

**Recommendation now:** do not close the repo list. Resolve the overlaps first (N-7), then approve.
Sub-recommendation unchanged: keep `cybersecurity_demos` deferred; `darpa_lift_2026` is contested — see
N-5, where the `lowprofiledronegurus` session filed a reasoned dissent against folding it.

### N-3. Are there source projects beyond the three?
Verified source set today: `~/floppi`, `~/SwarmLoc`, `~/GravityProbe` (`data-analysis` branch). The
operator has said there may be more. `lowprofiledronegurus` is running a home-directory sweep for
drone-adjacent candidates.
**ANSWERED 2026-08-18 — NO, the source set is NOT closed. Hold stands.** Home-directory sweep by the
`lowprofiledronegurus` session (114 entries + one level down + a `find -maxdepth 3 -name .git` backstop):

**Structural finding:** the **`msmcs-robotics` GitHub org is the robotics body of work and was only
partially enumerated.** SwarmLoc and GravityProbe both live there — and so do `WayfindR-driver` and
`WayfindR-android`. The source set was never "floppi + two repos"; it is "floppi + an org."

**Operator-confirmed in scope:** `engineer360`, `hiverf` (see N-1).

**Sweep candidates, not yet ruled:**
| Candidate | Remote | Call |
|---|---|---|
| `~/WayfindR-driver` (63M) | `msmcs-robotics` | Tier 1 — ground-rover stack (ESP32, RPLidar, ROS2 SLAM, FS-iA6B, Pi fleet mgr). No target repo covers it. Shares the FS-iA6B receiver with `flight_controller`. |
| `~/vizor` | `gndpwnd/vizor` | Tier 2 — VR/FPV telepresence, head-tracked gimbal. Covers the pilot interface, which nothing else touches. Pre-code. 2.6G is corpus, must not migrate. |
| `~/WayfindR-android` | `msmcs-robotics` | Tier 3 — Kotlin companion to WayfindR-driver; the call is coupling, not content. Stalest in sweep (2025-12-25). |
| `~/skytracker_data_analysis` | `comperem` org | Tier 3 — manned-aircraft ADS-B/YOLO. Extract `camera_calibration/` at most. |
| `~/outsource_to_hpc/cad_automation_optimization_stuff` | subdir of live HPC repo | Tier 3 — duplicates engineer360's toolchain; likely merge there, not `drone_frame_modeling`. Subtree extraction is hard to reverse. |
| `~/edgeai_docs` | — | Tier 3 — swarm/GPS-denial/counter-UAS content, but declared sibling of `~/denoiseai`. Splitting the pair is operator's call. |
| `~/Downloads/BNO085_I2C_Adafruit.ino` + Adafruit lib | — | → `sensor_interactions`. An accompanying `.eml` may be a brief or personal correspondence — not opened. |

**Leave alone:** `~/barrecuda` (automotive STM32 ECU), `~/byotn/habitat_iot_system` (greenhouse),
`~/skytracker-algorithm` (different org), ~40 others dismissed (keyword hits were the word "drone" in
prose, not hardware).

### N-5. Is the DARPA Lift Challenge still live, and does its deadline drive this program? (HUMAN-ONLY)
*Filed by the `lowprofiledronegurus` session, 2026-08-18.* `darpa_lift_2026/comp_requirements.md` says
**"Competition Date: Summer 2026."** It is now 2026-08-18 — that window is open or just closed. Every
file in the folder is dated 2026-05-04 and untouched for three months, which is equally consistent with
"abandoned" and with "requirements locked, work moved elsewhere." **Not inferable from disk.**
- **If LIVE:** `darpa_lift_2026` gets its own repo (category E); the sizing/scoring tool becomes a real
  backlog item (and `engineer360` already substantially implements it — see N-7); the heavy-lift
  requirements propagate into `drone_frame_modeling` and `flight_controller` scope; and the whole
  migration inherits an external deadline, which **reorders the execution plan**.
- **If PASSED/ABANDONED:** fold to `research/missions/darpa_lift_2026/`, archive the raw dumps.

**Dissent on record (the `lowprofiledronegurus` session, against folding):** it is a *mission* with hard
binding external requirements (55 lb max airframe, 110 lb min payload, 2:1 qualifying / 4:1 target ratio,
5 nm course, 350 ft AGL, <30 min, FAA Part 107 + Experimental Airworthiness Certificate; $6.5M prize) —
category E exists for exactly this; it contains an **unbuilt software deliverable** specified by the
operator in `notes1.md`; and decisively, it is a **different vehicle class** (55 lb airframe lifting
110-220 lb) than anything else in the corpus, so no existing repo's scope covers it and folding it hides
the constraint from the two repos it actually binds. **This session accepts the dissent** — the fold
recommendation is withdrawn pending N-5.

**Content split (agreed by both sessions, applies whichever way N-5 rules):** of the 104 KB, ~77 KB is
raw LLM chat transcript (`initial_research.md` 55 KB, `avionics_controls.md` 22 KB — inline
`images.openai.com` URLs). That is generated output in the same category as the N-2 fence and must not
migrate as authored research; quarantine as `_raw/` with a provenance header, or leave in floppi.
Operator-authored primary material is ~27 KB: `comp_requirements.md`, `notes1.md`, `initial_sources.md`
(a real bibliography), `transcripts.md`.

### N-6. ResearchHub `repositories.json` reconciliation (follow-on from the N-2 ruling)
`/home/devel/researchhub/repositories.json` has **40 tracked entries**, keyed by **local filesystem
path** — and several of those paths move or change policy under this migration. Verified 2026-08-18:

| Entry | `path` | `sync_docs` | `sync_sources` | Issue |
|---|---|---|---|---|
| `floppi_flight_controller` | `/home/devel/floppi/flight_controller` | `false` | `false` | path moves → `lowprofiledronegurus/flight_controller`; also lacks a `folders.generated` mapping, so it has no distilled-research channel |
| `floppi_drone_3d_model` | `/home/devel/floppi/drone_3d_model` | `false` | `false` | path moves → `drone_frame_modeling`; same missing `folders.generated` |
| `engineer360` | `/home/devel/engineer360` | **`true`** | `false` | already on the modern pattern (`folders.generated: docs/research`) — this is the **distilled-only** config the operator described |
| `hiverf_signal_prop_sim` / `_signal_id_sim` / `_comm_saturation` | `/home/devel/hiverf/*` | `false` | `false` | three entries; paths move if hiverf migrates |
| `vizor` | `/home/devel/vizor` | `true` | `false` | modern pattern; moves only if vizor migrates |

**Recommendation:** ask the ResearchHub session to (a) confirm `folders.generated: docs/research` +
`sync_docs: true` + `sync_sources: false` is the canonical **distilled-only** contract, (b) adopt it for
the two `floppi_*` entries, and (c) agree a **repointing protocol** so entries are updated at migration
time rather than silently syncing into a frozen archive. **Do not edit their file** — it is their zone.
*Message sent 2026-08-18.*

### N-7. Org boundaries and scope overlap: `engineer360`, `hiverf` — and `msmcs-robotics` (HUMAN-ONLY)
Operator named four trees with **overlapping scopes**: `~/floppi`, `~/lowprofiledronegurus`,
`~/engineer360`, `~/hiverf`. Verified remotes: `engineer360` → `gitlab.com:gndpwnd/engineer360`,
`hiverf` → `gitlab.com:gndpwnd/hiverf`, `vizor` → `gitlab.com:gndpwnd/vizor`. So there are **three
distinct forge homes** in play: the `lowprofiledronegurus` GitLab **group**, the `gndpwnd` GitLab
**personal namespace**, and the `msmcs-robotics` GitHub **org**.

Three questions, none defaultable:
1. **Do `engineer360` / `hiverf` MOVE into the `lowprofiledronegurus` group, or stay at `gndpwnd/` and
   just get cross-linked?** They are mature repos with real history — a move is a transfer, not a copy.
2. **Where does each overlap resolve?** `engineer360` ∩ `drone_frame_modeling` (structural/aero sizing,
   and engineer360 holds the DARPA solver); `hiverf` ∩ `position_denial_research` ∩ SwarmLoc
   (multilateration/DoA/Kalman — if hiverf lands, `position_denial_research` stops being a theory repo).
   Note `hiverf` deliberately keeps its "hive" generic (sensors "could be drones, BCI electrodes, field
   monitors") and is sibling to `~/bcicycle`, so any fold is lossy.
3. **Does the `msmcs-robotics` GitHub org come across wholesale** (SwarmLoc, GravityProbe,
   WayfindR-driver, WayfindR-android), or only the drone-relevant parts?

**Recommendation:** cross-link rather than move for `engineer360` and `hiverf` — they have independent
lifecycles and non-drone consumers — and treat `msmcs-robotics` as source-only. But this is the
operator's structural call. **Coordination underway:** messages sent to the `engineer360`, `hiverf`, and
`researchhub` sessions 2026-08-18.


### N-8. Operator charter change: SwarmLoc + GravityProbe are MINE-ONLY, and eventually DELETED (HUMAN-ONLY)
**This supersedes the 2026-07-09 plan and is the single biggest change to it.** Operator, written
2026-08-18 21:47 in `/home/devel/lowprofiledronegurus/tmp.md` (verified verbatim; surfaced by the `hiverf`
session):

> *"~/floppi/ should have sorted out integration of swarmloc and gravityprobe like only looking at the
> important stuff. we do'nt really want to integrate those repos at all, just looking at them for examples
> of capabilities and ideally they would get deleted in the long run because again not everything from
> them is usefule."*

**What it overturns.** `00_reorg_master_plan.md` §3 says content "migrates cleanly into the new repos" and
Q-G resolved to "kept on local disk indefinitely as historical archive, never deleted." Both are now
wrong: SwarmLoc and GravityProbe are a **capability-mining source**, not a migration source, and their
long-run fate is deletion.

**Blast radius — four of the thirteen repos draw their content ENTIRELY or MOSTLY from these two, and the
measured content does not justify them:**

| Repo | Planned source | Measured reality (2026-08-18) |
|---|---|---|
| `communication_hardware` | 100% SwarmLoc (`DWS1000_UWB/` 53M, `lora_feather_esp32/` 64K) | The 53M is dominated by ~60 single-iteration debug test variants (`test_rx_v9d_swap.cpp`, `test_rx_v8f_nocheck.cpp`, …) already flagged in `03` §16.1. Strip those and little remains. |
| `networking_pocs` | SwarmLoc `esp32_field_node/` (712K) + GravityProbe `esp32_*` (4 folders, **8-12K each**) | Near-empty. |
| `sensor_interactions` | GravityProbe displays/SD/I²C/MPU6050 + SwarmLoc `GPS_module/` (28K) | **GravityProbe is 22 files TOTAL**, every folder 8-32K — one or two sketches apiece. |
| `position_denial_research` | SwarmLoc `docs/` (80K) + `findings/` (96K) | **This is the one with real value** — 176K of genuine research spine. But `hiverf` overlaps it with a mature implementation (N-7). |

**Recommendation:** do not create `communication_hardware` or `networking_pocs`. On measured content they
would be repos built to hold a few kilobytes of Arduino sketches — the exact "repo that never fills"
failure. Mine both source repos for the ~176 KB of SwarmLoc research (→ `position_denial_research`) and a
short capability-inventory finding naming what the sketches demonstrate; leave the sketches themselves in
the archives. **If defaulted wrong:** we skip two repos the operator wanted — cheap to add later, whereas
creating and populating them is the expensive direction.

**RULED 2026-08-18 — CLOSED. Q-G resolved: READ-ONLY, no deletion, no modification.** Operator:
*"SwarmLoc is an outdated repo to be treated the same as gravity probe and wayfindr, don't just delete
them or modify them, just read from them and make documentation when needed."* All three legacy repos
(`SwarmLoc`, `GravityProbe`, `WayfindR-driver`+`WayfindR-android`) are read-only springboards. There is
no deletion gate because there is no deletion. Also **parked:** the WiFi-credential finding — operator
*"don't worry about security with wifi right now that is not the priority."*

### N-9. Does `drone_frame_modeling` survive as a repo, or dissolve into `engineer360`? (HUMAN-ONLY)
Same operator note, same timestamp:

> *"i think i want to keep everything in engeering360 that hits their scope, but who knows maybe i need to
> review it, perhaps the drone frame modelling will end up just becoming a group of structural and
> aerodynamics textbooks or something in the engineering360 repo."*

Consistent with the **N-2 ruling** (the frame-design research content's home is `engineer360`). If the
research goes there and the computational sizing already lives there, what remains for
`drone_frame_modeling` is CAD assets and printed-frame geometry — possibly not a repo's worth.
`drone_frame_modeling` **already exists on GitLab** (empty), so dissolving it means a repo deletion.
**RULED 2026-08-19 — CLOSED. `drone_frame_modeling` LIVES, re-scoped.** The `engineer360` session replied
and its argument beat both prior recommendations (this session and `lowprofiledronegurus` had each
recommended dropping it as near-empty):

> *the moment you build a real airframe its CAD needs a home that is not engineer360 — otherwise a
> vehicle's build artifacts end up inside a domain-neutral method repo and we have recreated this exact
> conversation in six months*

We were optimising for "no empty repos"; they optimised for "does the boundary still hold once the org
succeeds at its purpose." The org exists to build airframes, so the repo is empty because the work has not
happened — **deleting it prices in failure.** Both recommendations reversed.

**New scope:** CAD sources, meshes, BOM and print configs for airframes this group actually builds;
structural/aero theory and analysis method live in `engineer360`; a `REFERENCES.md` carries the pointer
(Q-I convention — link, not copy).

**R8, the governing boundary test** (agreed by all three sessions): *if the artifact is a number, a curve,
a geometry, or a piece of knowledge — it's engineer360's. If it's a thing that flies, or talks to a thing
that flies — it's the fleet's.* **CLOSED.**


---

## Tier 2 — shape decisions (defaults exist, but cheap to get right now)

### Q-N. Rename `auto_orientation_research`?
IMU/magnetometer/orientation research is **not drone-specific** — it applies to rovers and other land
vehicles. Candidates: `orientation_research`, `imu_research`, `attitude_estimation_research`, or leave.
**RULED 2026-08-18 — LEAVE AS-IS (`auto_orientation_research`).** Operator selected the recommendation.
No rename. Corpus cross-references stand unchanged. **CLOSED.**

### Q-M. `swarm_api` scope — drone-specific or general coordination layer?
Currently a Python FastAPI ground station for ESP32 drones, with a full `deploy/` (systemd wrapping).
**Recommendation: drone-specific**, framed as the ground-station/API for this fleet. Generalizing is a
speculative abstraction with no second consumer today. **If defaulted:** README framing is narrower than
the operator intended; a later re-frame is a docs edit, not a migration.

### Q-J. `sensor_interactions` scope — which sensor families?
Re-scoped narrower once comm-hardware and networking POCs moved out.
**Recommendation:** cameras (RGB/thermal/event), environmental (temp/pressure/humidity), lidar /
ultrasonic / ToF, standalone IMU rigs not owned by `auto_orientation_research`, displays/OLED (from
GravityProbe), I²C scanners and bring-up tooling, **and GPS modules** (see Q-K). Explicitly NOT: radio
(→`communication_hardware`), WiFi/cellular/DNS (→`networking_pocs`).

### Q-K. `SwarmLoc/GPS_module/` home (= STOP S-3)
**Recommendation: `sensor_interactions/gps/`.** These are plain module bring-up sketches, not
denial-characterization experiments. GPS *theory* still goes to `position_denial_research/theory/gps/`.
Split by artifact type: sketches→sensor, theory→denial-research. **If defaulted:** a later move of two
sketch folders.

### Q-I. Cross-cutting POC routing convention (the DNS-driveby case)
**Recommendation: Option 1** — canonical home is the **hardware/software domain** (`networking_pocs`);
the use-case repo (`position_denial_research`) carries a link, not a copy. Plus the corpus's
`GRADUATED_TO.md` pointer convention when a POC matures. Rationale: code has one home, cite-not-copy.

### Q-G. What happens to `~/floppi`, `~/SwarmLoc`, `~/GravityProbe` when done?
Already effectively decided ("kept on local disk indefinitely, never deleted").
**Recommendation:** kept **and frozen in place** with a top-level `POINTER.md` naming the new GitLab
homes; no end-of-life date. This is only still open on the *wording* → S-7.

---

## Tier 3 — per-file routing (recommendations exist; low risk to default)

### Q-C / S-4. `floppi/docs/` per-file split
**Recommendation:** take `03_folder_recon_findings.md` §14 verbatim — EKF_*→`auto_orientation_research/theory/ekf/`,
QUATERNION_*→`.../theory/quaternions/`, BEC_Wiring + POWER_MODULE_WIRING→`flight_controller/hardware/power/`,
`docs/flight-controller/`→`flight_controller/docs/`, bootloader findings cluster (7 files)→
`flight_controller/findings/bootloader/`, `docs/literature/`→`research/literature/`; ARCHITECTURE.md,
MIGRATION_SUMMARY.md, ORGANIZATION_SUMMARY.md, SCOPE_REFACTOR_COMPLETE.md, `docs/archive/`, `docs/_archived/`
**stay in floppi as archive**. Two genuinely ambiguous files remain: `DRONE_APPLICATIONS_REFERENCE.md`
(→`research/` vs `darpa_lift_2026/context/`) and `docs/flight-computer/` (→`flight_controller/` vs
`swarm_api/docs/history/` — it reads like the origin story of swarm_api).

### Q-D / S-5. `floppi/research/` per-file split
**Recommendation:** `GPS_*` (4 files)→`position_denial_research/theory/gps/`;
`CAMERA_EXTRINSIC_CALIBRATION.md`→`sensor_interactions/camera/theory/`;
`ARDUINO_DIAGNOSTICS.md`→`auto_orientation_research/findings/`.

### S-1. GravityProbe per-folder routing (~15 subfolders)
**Recommendation:** apply `08_gravityprobe_recon.md` §5 as written — ESP32 WiFi/enterprise-auth folders→
`networking_pocs`, display/OLED/SD/I²C-scanner→`sensor_interactions`, MPU6050 + quaternion/matlab math→
`auto_orientation_research`. **HUMAN-ONLY sub-item:** `teensy_esp8266/` contains **hardcoded credentials**
in-tree (`08` §4.4) — those must not be copied into a new repo as-is.

### S-2. `floppi/tmp.md` verbatim into `swarm_communication_protocol/docs/archive/`
**Recommendation: yes, verbatim.** 43 KB of the operator's own design reasoning; `02_swarm_protocol_seed.md`
is the distilled version and the raw source is worth keeping beside it. Register in
`07_verbatim_content_registry.md`.

### S-6. `research/mini_projects/` initial seeds
**Recommendation:** seed **only** what has real content today — do not create empty stubs (the checklist
says so explicitly). On current inventory that means `literature/` + `tools/researchhub_client/` and
nothing else; mini-projects get created when a real inquiry starts.

### S-7. `POINTER.md` wording for the three frozen source repos
**Recommendation:** I draft one paragraph, reused verbatim across all three, operator approves at Phase 8.
Not blocking anything now.

### N-2. Does the untracked `generated/` + `sources/` content migrate? (~46 MB)
`drone_3d_model/generated/` (RAG synthesis + rag_knowledge markdown) and `sources/pdfs/` were deliberately
untracked on 2026-07-11 by a ResearchHub `repo_untrack` fence; 139 staged deletions, content recoverable
from HEAD, absent from the worktree. Same fence in `flight_controller`.
**RULED 2026-08-18 — does not go to `drone_frame_modeling`; the content's home is `engineer360`.**
Operator: *"seems like it should get put into the engineer360, who i have told that i don't want
'generated documents' or any 'sources' from researchhub getting synced… now we only care about distilled
research from researchhub."*

Two separable rulings, both binding beyond this one folder:
1. **Content placement** — the frame-design research subject matter belongs with `engineer360`
   (computational design automation for VTOL), not duplicated into `drone_frame_modeling`.
2. **Standing sync policy** — **only DISTILLED research from ResearchHub syncs into any repo.** No
   `generated/` documents, no `sources/`. The 2026-07-11 fence was correct and reflects an *older*
   ResearchHub version/configuration; the current expectation is distilled-only.

**Follow-on (new, N-6):** ResearchHub's `/home/devel/researchhub/repositories.json` must be reconciled —
see N-6 below. **CLOSED as a placement decision; the config work is N-6.**

### N-4. Does `floppi` itself get a full bootstrap docs tree?
**Recommendation: NO — archive-only.** floppi is being frozen; scaffolding eight empty bootstrap folders
into a repo that is about to become a historical archive is bloat. It has `CLAUDE.md` + `docs/todo.md`
(SSOT) + `docs/scope.md`, which is what a live session needs. Full `PROMPTS.md § Bootstrap Existing`
treatment belongs to the **destination** repos, after the scope decisions above land. *(Defaulted
2026-08-18; say if wrong.)*

---

## Ruling log

_(record each ruling here, dated, and update the item above in place)_

- **2026-08-18** — **D-0 RULED:** no fixed front door; either session may ask the operator; the duty is
  de-duplication, and every ruling mirrors into this file + the bus the same turn.
- **2026-08-18** — **Q-N RULED:** leave `auto_orientation_research` as-is. No rename. CLOSED.
- **2026-08-18** — **N-2 RULED:** the frame-design research content's home is `engineer360`, not
  `drone_frame_modeling`. Standing policy: **only DISTILLED ResearchHub research syncs into any repo** —
  no `generated/` docs, no `sources/`. The 2026-07-11 fence reflected an older ResearchHub configuration
  and was correct. Config follow-on → N-6.
- **2026-08-18** — **N-1 REOPENED/ENLARGED:** `engineer360` + `hiverf` confirmed in scope with
  overlapping scopes; repo list must not close until N-7 resolves.
- **2026-08-18** — **N-3 ANSWERED:** source set is NOT closed; `msmcs-robotics` is a partially-enumerated
  org, not two repos. Phase 2 hold stands.
- **2026-08-18** — N-4 defaulted to archive-only by the `floppi` session; reversible.
- **2026-08-19** — **N-10 CLOSED:** **no git history transfer.** New repos start at commit 1; a
  `PROVENANCE.md` per repo carries the pointer back to the source. Operator: *"i really don't want to
  transfer git history at all that is way too much of a mess and burdensome, its not like we are deleting
  ~/floppi/ so we can always reference it if we are missing something."* Template →
  `18_provenance_template.md`.
- **2026-08-19** — **N-9 CLOSED / R8 AGREED:** `drone_frame_modeling` survives, re-scoped to CAD/mesh/BOM/
  print-config; engineer360 owns the method. Ledger 8 → **9 active**, nothing unsettled.
- **2026-08-18** — **N-8 CLOSED / Q-G RESOLVED:** the three legacy repos are **READ-ONLY** — not deleted,
  not modified; read and document only. WiFi-credential finding **parked** by the operator.
- **2026-08-18** — **R7:** `sensor_interactions` is the **successor** to GravityProbe sensor work +
  WayfindR lidar work; its thinness problem is resolved by inheritance. KEEP.
- **2026-08-18** — **N-8 FILED (supersedes plan §3 + Q-G):** SwarmLoc + GravityProbe are mine-only and
  eventually deleted, per operator note of the same day. Recommend NOT creating `communication_hardware`
  or `networking_pocs`; measured content does not justify them.
- **2026-08-18** — **N-9 FILED:** `drone_frame_modeling` may dissolve into `engineer360`; do not populate
  it pending the engineer360 scope-boundary reply.
- **2026-08-18** — fold recommendation for `darpa_lift_2026` **withdrawn** by the `floppi` session in
  favour of the `lowprofiledronegurus` dissent; now gated on N-5.
