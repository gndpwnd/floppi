# 04 — Mini-Project Setup Guide (multi-project repos)

**Status:** 2026-07-09 draft. Load-bearing pattern doc for the `lowprofiledronegurus` reorg.
**Companions:** `00_reorg_master_plan.md`, `01_target_repos_v2.md`, `03_folder_recon_findings.md`.
**Bootstrap sources cited:**
- `~/llm-project-bootstrap/guides/PROJECT_SETUP.md` (repo shape)
- `~/llm-project-bootstrap/guides/DOCUMENTATION_HANDLING.md` (features vs findings)
- `~/llm-project-bootstrap/guides/PROJECT_SCRIPTS.md` (install / test / deploy)
- `~/llm-project-bootstrap/guides/SESSION_CONDUCT.md` (test lifecycle)
- `~/llm-project-bootstrap/templates/{scope,roadmap,readme,findings,todo}_template.md`

---

## 1. Purpose

This guide governs how to structure a **mini-project inside a multi-project repo** — one of `communication_hardware`, `networking_pocs`, `sensor_interactions`, `research`, `cybersecurity_demos` (deferred).

Each mini-project ships the **full bootstrap tree** (`docs/{README, scope, roadmap, todo, features/, findings/, archive/}` + `src/` + `tests/`). The parent repo has its own **thin top-level `docs/`** that indexes and orients across the mini-projects.

**User rationale (verbatim):** "standardization makes things easier and over time i can point the automated research tools to different locations."

Consequences of that rationale:
- Every mini-project MUST use the same on-disk layout, even if a subfolder starts empty. Automated tools depend on the layout being predictable.
- Deviations (e.g. no `src/` because the mini-project is docs-only) are documented explicitly in that mini-project's `docs/scope.md` — automation should not have to guess.
- Names of the seven bootstrap files are **fixed** (`README.md`, `scope.md`, `roadmap.md`, `todo.md`, `features/`, `findings/`, `archive/`). Do not rename or reorder.

---

## 2. Repo taxonomy recap

The 12–13 target repos fall into three shapes. This guide applies to shape (a) primarily; it is informative for (b) and (c).

**(a) Multi-project repos (5)** — this guide's target:
- `communication_hardware` — UWB / RF / radio hardware POCs
- `networking_pocs` — WiFi / cellular / DNS-driveby POCs
- `sensor_interactions` — non-radio, non-networking sensor POCs
- `research` — literature + mini research projects + automated-research-tool hookup
- `cybersecurity_demos` — deferred; materializes when first cyber POC lands

**(b) Single-project-with-build-variants (2)** — one project, multiple build/application targets in one tree:
- `auto_orientation_research` — rover / drone / land-vehicle variants of the same orientation stack
- `position_denial_research` — GPS-denial theory with sub-topics (multilateration, DNS-driveby math)

Build-variant projects use the **standard bootstrap tree at the repo root** (per `PROJECT_SETUP.md` §Project Structure), not per-variant mini-project trees. Variants are represented as build targets under `src/` (e.g. `src/variants/rover/`, `src/variants/drone/`) with a single `docs/features/` describing each variant, not by copying the docs tree.

**(c) Standalone single-project (6):**
- `flight_controller`, `fc_tool`, `swarm_api`, `drone_frame_modeling`, `swarm_communication_protocol`, `darpa_lift_2026`

Standalone repos use the standard bootstrap tree at the repo root and are outside this guide's scope.

---

## 3. Directory shape — the load-bearing diagram

```
<repo>/                                    # e.g. communication_hardware/
├── README.md                              # Repo top-level: 1-liner + link to docs/
├── docs/                                  # REPO-LEVEL docs (thin)
│   ├── README.md                          # Overview + navigation to mini-projects
│   ├── scope.md                           # Repo boundary (what belongs, what doesn't)
│   ├── roadmap.md                         # Cross-mini roadmap
│   ├── todo.md                            # Cross-mini current tasks
│   ├── INDEX.md                           # Explicit index of every mini-project + status
│   ├── findings/                          # Repo-wide research (spans mini-projects)
│   └── archive/                           # Session summaries at repo scope
├── <mini-project-1>/                      # e.g. uwb/dw1000/
│   ├── docs/
│   │   ├── README.md
│   │   ├── scope.md
│   │   ├── roadmap.md
│   │   ├── todo.md
│   │   ├── features/
│   │   ├── findings/
│   │   └── archive/
│   ├── src/                               # or platformio.ini + src/include/lib/ for firmware
│   └── tests/
│       ├── persistent/                    # small protected floor
│       └── test_*.py                      # ephemeral, deleted after use
├── <mini-project-2>/                      # e.g. radio/lora/
│   └── ...same shape...
└── util/                                  # OPTIONAL — shared utilities across mini-projects
    └── docs/README.md                     # explains what belongs here
```

Key invariants:
- Every mini-project owns its own `docs/`, `src/`, `tests/` triple.
- Repo-level `docs/` is **navigational** (INDEX, cross-mini roadmap, cross-mini findings) — it does not duplicate mini-project content.
- Mini-project folders may nest (e.g. `communication_hardware/uwb/dw1000/`) — the leaf that owns the bootstrap tree is the mini-project; the intermediate folders are just organizational buckets.

---

## 4. When to create a mini-project

Create a new mini-project when **any two** of the following are true:

- **Distinct hardware/software domain** — different chip family, different network protocol, different sensor family (DW1000 UWB vs. RFM95 LoRa vs. RFM69 sub-GHz)
- **Independent lifecycle** — has its own roadmap, its own scope boundary, its own "done" definition
- **Different intended consumers or contributors** — a firmware POC lead and a Python-networking lead should not fight over one `docs/`
- **Would benefit from independent findings/features documentation** — the findings are chip-specific or protocol-specific and not useful to sibling work
- **Own PlatformIO / build system** — a mini-project with its own `platformio.ini` is almost always its own mini-project

Ranking hint: hardware boundary > protocol boundary > sensor boundary > "vibes."

---

## 5. When NOT to create a mini-project

Do not create a mini-project when:

- It's a **one-file sketch or experiment** — put it inline in a sibling mini-project's `tests/` (ephemeral) or `findings/` if the learning matters
- It's a **shared utility across mini-projects** — put it at repo level as `util/` (see §3)
- It's **cross-cutting research spanning multiple mini-projects** — put it in repo-level `docs/findings/`
- It's a **datasheet or reference PDF** — attach to the mini-project that consumes it, under `<mini>/docs/findings/references/` or similar; don't spawn a folder for a PDF
- **The candidate is empty** — you are seeded from a naming decision but have no artifacts. Wait for the first artifact, then stub. Empty mini-projects rot faster than empty repos.

---

## 6. Mini-project docs — file-by-file

All seven names are fixed. Templates cited from `~/llm-project-bootstrap/templates/`.

### 6.1 `<mini>/docs/README.md`
- Source template: `templates/readme_template.md`
- **Contents:** one-line description of what this mini-project is, quick-start (how to build/run/flash), link to `docs/scope.md` for boundaries, link to parent repo's `docs/INDEX.md`
- **Style:** brief, user-facing. Not an essay.
- **Rule:** if you need more than ~40 lines, move detail into `features/` or `findings/` and link out.

### 6.2 `<mini>/docs/scope.md`
- Source template: `templates/scope_template.md`
- **Required** — this is the boundary contract
- **Contents:**
  - Overview (2–3 sentences)
  - Objectives (what success looks like)
  - In-scope / out-of-scope lists (explicit exclusions)
  - Technical decisions table
  - Applicable platforms (for platform-agnostic work — e.g. rover/drone applicability)
  - Cross-refs to sibling mini-projects and to related theory repos
- **Voice:** boundary-setting, not aspirational. "This mini-project does X, does not do Y."

### 6.3 `<mini>/docs/roadmap.md`
- Source template: `templates/roadmap_template.md`
- **Contents:** feature checklist per bootstrap convention (`- [ ]` / `- [x]`)
- **No time estimates** (per `PROJECT_SETUP.md` §Key Principles)
- **Rule:** if an entry needs more than 2 lines of explanation, split it into `features/<name>.md` or `findings/<topic>.md` and link (per `DOCUMENTATION_HANDLING.md` §Keeping the Roadmap Lean)

### 6.4 `<mini>/docs/todo.md`
- Source template: `templates/todo_template.md`
- **Contents:** session-level tasks — In Progress / Blocked / Up Next / Backlog / Recently Completed
- **Update frequency:** every session
- **Differs from `roadmap.md`:** more granular, shorter timeframe

### 6.5 `<mini>/docs/features/`
- **Purpose:** what the mini-project does — specs, API contracts, usage examples
- **Naming:** lowercase-hyphenated, descriptive (`twr-ranging.md`, `serial-command-catalog.md`)
- **Promotion rule:** start as a single file; promote to subfolder when >50KB or when multiple logical sections need separate files (per `DOCUMENTATION_HANDLING.md`)
- **Do not create during unstable early exploration** — use `findings/` instead. Only formalize a `features/` doc once the surface is stable.

### 6.6 `<mini>/docs/findings/`
- **Purpose:** mini-project-scoped research, investigations, non-obvious discoveries
- **Naming:** descriptive, grep-friendly (`ldo-tuning-per-role.md`, `d8-to-d2-irq-wire-workaround.md`)
- **Cross-refs:** if a finding informs a parent-level or sibling decision, cross-link bidirectionally
- **When to write:** non-obvious solutions, failed approaches worth remembering, comparative research
- **When NOT to write:** trivial fixes, things already in official docs

### 6.7 `<mini>/docs/archive/`
- **Purpose:** session summaries + historical decisions
- **Contents:** dated session logs, superseded scope drafts, discussion documents
- **Rule:** never delete from archive; move findings here when they age out of active `findings/`

---

## 7. Repo-level docs — file-by-file

Repo-level `docs/` is **thin and navigational**. It does not duplicate mini-project content.

### 7.1 `<repo>/README.md`
- 1-line description of the repo + link to `docs/README.md` + link to `docs/INDEX.md`
- Do NOT enumerate mini-projects here — that's what INDEX.md is for

### 7.2 `<repo>/docs/README.md`
- Repo purpose + link to `docs/INDEX.md` + link to `docs/roadmap.md`
- Include a "graduation" note if applicable (e.g. `communication_hardware`: "when a POC matures, code moves to `flight_controller/lib/`")

### 7.3 `<repo>/docs/scope.md`
- Repo-scope boundary contract — meta level
- Example (`communication_hardware`): "Hosts POCs for radio / UWB / RF hardware bring-up. Excludes multilateration theory (`position_denial_research`), swarm protocol design (`swarm_communication_protocol`), production integration (`flight_controller/lib/`)."
- Cross-links to sibling repos with absolute GitLab URLs

### 7.4 `<repo>/docs/roadmap.md`
- Cross-mini roadmap — features that span mini-projects, or the sequence of which mini-projects deliver which capability
- **Do not duplicate** per-mini roadmap items; link to them: `- [ ] TWR ranging POC — see [uwb/dw1000/docs/roadmap.md](../uwb/dw1000/docs/roadmap.md)`

### 7.5 `<repo>/docs/todo.md`
- Cross-mini tasks — repo-level infra work, index maintenance, cross-cutting fixes
- Do not duplicate per-mini todos

### 7.6 `<repo>/docs/INDEX.md`
- **The load-bearing file for automated tools.** Enumerates every mini-project.
- **Row per mini-project:**
  - Name (relative path)
  - Status: `active` / `stub` / `dormant` / `archived` / `graduated`
  - 1-line purpose
  - Link to its `docs/README.md` and `docs/scope.md`
- Update whenever a mini-project is created, promoted, demoted, or archived.

### 7.7 `<repo>/docs/findings/`
- Research that spans mini-projects — comparative studies, shared theory notes
- Example: `communication_hardware/docs/findings/uwb-vs-lora-ranging-comparison.md`

### 7.8 `<repo>/docs/archive/`
- Repo-scope session summaries, historical scope drafts, superseded INDEX snapshots

---

## 8. Testing pattern

Follows `PROJECT_SCRIPTS.md` §Testing Scripts and `SESSION_CONDUCT.md` §Testing Rules — **do not restate them here; the summary below is the operational floor for this reorg.**

**Rule (per user directive):** tests default to **EPHEMERAL** — they answer a question, then get deleted. Do NOT chase-your-tail with tests.

### 8.1 Three test buckets per mini-project

| Bucket | Lives in | Lifecycle | What it is |
|--------|----------|-----------|------------|
| **Permanent floor** | `<mini>/tests/persistent/` | Kept forever; protected | 3–7 smoke tests + property/invariant tests on load-bearing logic + contract tests at genuine boundaries + operator-designated security tests. **Behavior-level only.** |
| **Throwaway (default)** | `<mini>/tests/` top level | Deleted after use | Spike / diagnostic tests. Product is information, not lasting verification. |
| **Out-of-loop** | run on request | Never auto-run | Throughput / stress / load / broad E2E. Human-triggered only. |

### 8.2 Onboard diagnostics carry most of the load

Per user directive, onboard diagnostics are the primary health-check mechanism, not standing test suites.

- **Firmware POCs (PlatformIO / Arduino):** add a diagnostics command to the firmware itself (e.g. `diag` over serial, or a boot-time self-check that asserts KNOWN-CORRECT outputs). A liveness/HTTP-200-style check is not enough.
- **Python mini-projects:** add a `--diagnostics` / health endpoint that asserts internal state, not just responsiveness.
- **Documentation-only mini-projects:** no tests needed.

### 8.3 External testing framework

- **pytest** is the default external testing framework — case-by-case per repo maturity.
- **Host-side firmware tests:** pytest driving `pyserial` for serial-protocol verification. Ephemeral by default; one small assertion promoted to `tests/persistent/` if it protects against a real regression.
- **On-target tests (Unity / CppUTest / PlatformIO test runner):** only if genuinely needed (register-level checks, interrupt-timing) — most POCs do not.
- **Do NOT prescribe heavy test suites.** The user's directive is explicit: "don't get stuck in running so many tests you chase your tail."

### 8.4 Test lifecycle (concrete)

- Every spike test lives at `<mini>/tests/` top level.
- After it answers its question, **delete it**. Do not "just leave it around."
- If it revealed a behavior that must not regress, promote **one** small assertion into `<mini>/tests/persistent/` and delete the rest.
- The permanent floor is small by construction — full run is cheap.
- `<mini>/tests/results/` and `<mini>/tests/outputs/` are `.gitignored`.

### 8.5 Greenfield note

None of these repos have tests yet after the reorg. First testing artifact per mini-project is the **persistent floor** (1–2 smoke tests, 0–2 invariants), created before the first ephemeral test. Do not front-load — this is a floor-installation exercise, not bloat-pruning.

---

## 9. Mini-project promotion / demotion

### 9.1 Promote to standalone repo

Trigger when a mini-project outgrows its parent:
- Independent CI/CD pipeline
- External consumers or downstream integrations
- Distinct release cadence
- Regularly diverges from sibling mini-projects on tooling, style, or lifecycle
- Consistently exceeds ~50% of the parent repo's `docs/` and `src/` mass

**Process (user-driven, manual):**
1. Add an open question to `<repo>/docs/todo.md` recommending promotion
2. User confirms + does the GitLab CRUD manually
3. Original mini-project folder in the parent repo stays as a **STATUS stub** — `<mini>/STATUS.md` says "graduated to `<repo>@<sha>`" so history and cross-refs still resolve
4. Update `<repo>/docs/INDEX.md` to mark status `graduated` with the new URL

Do not delete the graduated stub — cross-refs from other repos need it.

### 9.2 Demote to sketch / finding

Trigger when a mini-project has been dormant for months and nothing has landed:
- Move meaningful notes to parent's `docs/findings/<topic>.md`
- Move superseded scope/roadmap drafts to parent's `docs/archive/`
- Delete the mini-project folder
- Update `<repo>/docs/INDEX.md` to remove the entry (or mark `archived` with a link to the archived doc)

Neither promotion nor demotion is automatic — both require user judgment. Automation should surface candidates, not execute the change.

---

## 10. Cross-mini-project references

Rules for linking between docs, taken from `DOCUMENTATION_HANDLING.md` §Cross-References and extended for the multi-repo case.

- **Within the same mini-project:** relative paths (`./features/twr-ranging.md`)
- **Across mini-projects in the same repo:** relative paths (`../radio/lora/docs/scope.md`)
- **Across repos:** absolute GitLab URLs. During the pre-migration planning phase, absolute local paths (`~/lowprofiledronegurus/<repo>/...`) are acceptable as a placeholder — flag every one for URL conversion before the repo goes public.
- **Bidirectional linking:** if A references B, consider whether B should reference A. Typical case: hardware POC (`communication_hardware/uwb/dw1000/`) references theory (`position_denial_research/uwb_ranging/`), and theory references the POC.
- **Datasheet dual-homing:** datasheets used by exactly one mini-project live in that mini's `docs/findings/references/` (or similar). Broader survey papers stay in `research`. When in doubt, keep in `research` — distribution is cheaper than reunion.

---

## 11. Initial mini-project stubs per repo

Per `01_target_repos_v2.md` and `03_folder_recon_findings.md`. Names in `<>` are candidates; user confirms.

### 11.1 `communication_hardware`
- `uwb/dw1000/` — DW1000 UWB PlatformIO project (from `~/SwarmLoc/DWS1000_UWB/`). Use `tests_new/` as the primary tests layout; `tests/` legacy `.cpp` iterations move to `findings/legacy_iteration_history/` or are archived.
- `radio/lora/` — RFM95 / Adafruit LoRa Feather sketches (from `~/SwarmLoc/lora_feather_esp32/`)
- `radio/rfm69/` *(candidate)* — RFM69HCW sketches if they materialize separately from the FC repo
- **Future stubs:** `radio/cellular_rf/`, `radio/2p4_ghz_mesh/`, additional UWB modules (e.g. DWM3000 when it lands)

### 11.2 `networking_pocs`
- `wifi_esp32_field_node/` — ESP32 WiFi + MPU6050 harness (from `~/SwarmLoc/esp32_field_node/`)
- **Future stubs:** `dns_driveby_positioning/` (cross-linked to `position_denial_research`), `cellular_modem/`, `wifi_survey/`, `mesh_olsr/`

### 11.3 `sensor_interactions`
- `gps/` *(candidate)* — GPS-module sketches from `~/SwarmLoc/GPS_module/` (GPS-as-sensor, per v2 scope note)
- **Candidate future stubs (need user confirmation):** `camera/`, `environmental/`, `lidar/`, `ultrasonic/`, `arduino_diagnostics/` (from `~/floppi/research/ARDUINO_DIAGNOSTICS.md`)

### 11.4 `research`
- `literature/` — root-level PDFs from `~/floppi/literature/` not clearly owned by a topic repo
- `automated_research_tools/` — hookup for `~/floppi/tools/researchhub_client.py` (2943 lines, zero-dep — perfect fit per `03_folder_recon_findings.md`)
- **Future mini research projects:** TBD by user; each new inquiry that outgrows a single note becomes a mini-project here.

### 11.5 `cybersecurity_demos`
- **Empty until first cyber POC lands.** Do not stub. Until then, cyber-adjacent work sits under `networking_pocs/<name>/` with a `cybersecurity/` subfolder if truly needed.

---

## 12. Operating principles reference

Short reminder — these govern every wave of reorg work, not just this doc.

- **Planning before dev.** This is a planning wave. No migration scripts, no scaffolding code, no repo CRUD. Open questions and phase-6 execution items only.
- **Ephemeral tests + onboard diagnostics** are the default. Persistent floor is small and protected.
- **Documentation discipline.** Write only in your write-zone. If a task genuinely requires editing another file, stop and report.
- **No repo CRUD without user approval.** User does all GitLab operations manually.
- **Cross-refs before merges.** When you split content across repos, check bidirectional links exist before considering it done.
- **Standardization enables automation.** Layout invariants (§3) are the contract with future automated research tools — do not break them casually.

---

## 13. Cross-refs to bootstrap guides used

Every claim in this doc is grounded in one of these. Cite them from a mini-project's docs when you need the authoritative source rather than re-explaining.

| Concern | Bootstrap source |
|---|---|
| Bootstrap tree shape | `~/llm-project-bootstrap/guides/PROJECT_SETUP.md` §Project Structure |
| features/ vs findings/ decisions | `~/llm-project-bootstrap/guides/DOCUMENTATION_HANDLING.md` |
| Roadmap-lean rule (>2 lines → separate doc) | `DOCUMENTATION_HANDLING.md` §Keeping the Roadmap Lean |
| Cross-reference conventions | `DOCUMENTATION_HANDLING.md` §Cross-References |
| Install / test / deploy trio | `~/llm-project-bootstrap/guides/PROJECT_SCRIPTS.md` |
| Test lifecycle (persistent vs ephemeral vs out-of-loop) | `~/llm-project-bootstrap/guides/SESSION_CONDUCT.md` §Testing Rules |
| Onboard diagnostics vs external pytest | `PROJECT_SCRIPTS.md` §External Testing vs Onboard Diagnostics |
| Live vs calibration/debug firmware builds | `PROJECT_SCRIPTS.md` §Live vs Calibration/Debug Builds |
| scope.md structure | `~/llm-project-bootstrap/templates/scope_template.md` |
| roadmap.md structure | `~/llm-project-bootstrap/templates/roadmap_template.md` |
| README.md structure | `~/llm-project-bootstrap/templates/readme_template.md` |
| todo.md structure | `~/llm-project-bootstrap/templates/todo_template.md` |
| findings/*.md structure | `~/llm-project-bootstrap/templates/findings_template.md` |

---

## 14. Open questions (for user)

- **Q-14a Mini-project naming inside repos.** For `communication_hardware`, use `uwb/dw1000/` (two-level) or flatter `uwb_dw1000/`? Two-level assumed for this draft — it lets `uwb/` grow to `uwb/dwm3000/` etc. cleanly.
- **Q-14b Legacy `.cpp` iteration files in DWS1000_UWB `tests/`.** Route to `communication_hardware/uwb/dw1000/docs/findings/legacy_iteration_history/` (preserve as findings), or archive without importing (drop them)? Recommendation: archive as findings — the iteration story has value.
- **Q-14c `research/automated_research_tools/` shape.** Is the researchhub client a mini-project of its own, or a `util/` at repo level? Recommendation: mini-project — it has its own scope and lifecycle.
- **Q-14d `sensor_interactions` stub list.** User to confirm which of `camera/`, `environmental/`, `lidar/`, `ultrasonic/`, `gps/` land now vs. later. Do not create empty stubs.
- **Q-14e `GravityProbe/` reference.** `esp32_field_node` docs reference it but it is not in known source repos (per `03_folder_recon_findings.md`). Where does it live?
- **Q-14f Cross-repo URL convention.** Confirm absolute GitLab URLs are the target for cross-repo links post-migration. Local paths as placeholder pre-migration — OK?

---

*This doc is a planning-phase artifact. No scaffolding, no code, no repo CRUD. All initial mini-project stubs listed in §11 are proposals — user confirms before any folder or repo is created.*
