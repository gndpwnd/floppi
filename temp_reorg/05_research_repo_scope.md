# `research` repo — internal scope + layout (Q-L resolution draft)

**Status:** 2026-07-09 planning draft. Resolves Q-L from `00_reorg_master_plan.md`. Sibling to `04_mini_project_setup_guide.md` (bootstrap-tree standard for mini-projects). Planning only — no repo CRUD, no code, no migration scripts.
**Owner action:** review, confirm repo shape + naming + initial mini-project seeds. Manual GitLab CRUD after sign-off.
**References:**
- `temp_reorg/00_reorg_master_plan.md` (§2 row 12; §4 Q-L)
- `temp_reorg/01_target_repos_v2.md` (per-repo card for `research`)
- `temp_reorg/03_folder_recon_findings.md` (initial-content routing; `researchhub_client.py` finding)
- `~/llm-project-bootstrap/guides/PROJECT_SETUP.md` (bootstrap tree)
- `~/llm-project-bootstrap/guides/DOCUMENTATION_HANDLING.md` (features/ vs findings/, naming)
- `~/llm-project-bootstrap/guides/SESSION_CONDUCT.md` (test discipline)

---

## 1. Purpose

The `research` repo is the group's **centralized research-staging area**. It holds three kinds of material that don't belong in any single topic repo:

1. **Literature** — canonical store of PDFs + per-paper summary notes, organized by topic.
2. **Mini research projects** — bootstrap-shape lifecycles for exploratory investigations (surveys, deep-dives, comparative analyses, algorithm sketches) that haven't earned a standalone repo yet.
3. **Automated research tools** — the pipelines/clients that produce findings across the group (starting with `researchhub_client`), each treated as its own mini-project.

Pieces **graduate outward** into topic repos (`position_denial_research`, `auto_orientation_research`, `swarm_communication_protocol`, etc.) as they mature. `research/` stays canonical for the categorized literature store and for the automated-tool code that other repos consume.

**What this repo is not:** it is not a dumping ground for anything speculative. Content lands here only if it fits one of the three buckets above.

---

## 2. Repo shape (layout diagram)

```
research/
├── README.md                             # repo-level entry point (one screen)
├── docs/                                 # bootstrap tree (per PROJECT_SETUP.md)
│   ├── README.md                         # navigation of docs/
│   ├── scope.md                          # what belongs, what doesn't (see §8)
│   ├── roadmap.md                        # cross-mini-project research roadmap
│   ├── todo.md                           # session-level tasks
│   ├── INDEX.md                          # master index: literature areas + mini-projects + tools
│   ├── features/                         # (rare here) repo-level "capabilities" if any
│   ├── findings/                         # cross-mini findings that span topics
│   └── archive/                          # session records, superseded plans
├── literature/                           # NOT a mini-project — categorized store
│   ├── README.md                         # navigation of literature/
│   ├── INDEX.md                          # per-PDF index (author, year, topic, link, notes-file)
│   ├── uwb/                              # topic subfolders
│   │   ├── README.md
│   │   ├── INDEX.md
│   │   ├── DecawaveNNNN_dwm1000-datasheet.pdf
│   │   └── DecawaveNNNN_dwm1000-datasheet.md   # summary note (same base name)
│   ├── gps/
│   ├── imu/
│   ├── flight_dynamics/
│   ├── swarm_algorithms/
│   ├── communications_theory/
│   └── vendor_datasheets/
├── automated_research_tools/             # each tool = full mini-project (bootstrap tree)
│   ├── README.md                         # navigation + tool registry
│   ├── INDEX.md                          # per-tool index
│   ├── shared_config/                    # shared endpoints, prompt libraries (NO secrets)
│   │   ├── README.md
│   │   └── SECRETS.md                    # documents where secrets live (env vars, untracked)
│   └── researchhub_client/               # first confirmed tool (migrated from floppi/tools/)
│       ├── docs/
│       │   ├── README.md
│       │   ├── scope.md
│       │   ├── roadmap.md
│       │   ├── todo.md
│       │   ├── features/                 # documents API surface exposed by the tool
│       │   ├── findings/                 # research the tool has PRODUCED
│       │   └── archive/
│       ├── src/
│       └── tests/
│           ├── persistent/               # contract tests only (invariants of HTTP client)
│           └── (ephemeral tests deleted after use)
└── mini_projects/                        # each = full bootstrap mini-project
    ├── README.md                         # navigation
    ├── INDEX.md                          # per-mini-project index
    └── [topic_slug]/                     # e.g. ekf_theory_deepdive/, multilateration_survey/
        ├── docs/{README,scope,roadmap,todo,features,findings,archive}
        ├── src/
        └── tests/
```

Top-level dir count: **5** (`docs/`, `literature/`, `automated_research_tools/`, `mini_projects/`, plus repo-root `README.md`).
Second-level curated subfolder count in the diagram: **~30** (literature topic dirs + tool subtree + mini-project scaffolding).

---

## 3. Per-level scope decisions

### 3.1 `docs/` (repo-level bootstrap tree)

- Standard bootstrap tree per `PROJECT_SETUP.md`.
- `docs/scope.md` is the canonical "what belongs / what doesn't" contract — mirrors §8 of this doc.
- `docs/roadmap.md` tracks the **cross-mini-project** research roadmap (not per-mini-project detail; per-mini roadmaps live in each mini-project's own `docs/roadmap.md`).
- `docs/INDEX.md` is the master index. It lists every literature topic subfolder, every automated tool, every mini-project — the single-page map of the repo.
- `docs/findings/` holds **cross-cutting** research pieces that don't fit one mini-project (e.g. "state of drone-swarm networking research 2026"). Per-mini findings stay under their mini-project's own `docs/findings/`.
- `docs/features/` is typically thin at the repo level; the repo doesn't "expose features" — it hosts sub-projects. Kept in the tree for consistency; may be empty.

### 3.2 `literature/` (categorized store — not a mini-project)

- **Not** a bootstrap mini-project. Content is a curated store, not a lifecycle. No `src/` or `tests/`.
- `literature/README.md` explains the store's organization + naming convention.
- `literature/INDEX.md` is a flat, greppable index: `Author | Year | Title | Topic | File path | Notes-file path | Tags`.
- Topic subfolders are **curated by human intent**, not automated. Each subfolder has its own `README.md` + `INDEX.md`.
- **PDF naming** (per bootstrap `DOCUMENTATION_HANDLING.md` file-naming conventions, adapted for citations): `[Author][Year]_[short-title].pdf` — e.g. `ElkeJohnson2024_gnc-testing-quadcopter.pdf`.
- **Summary note naming:** same base name + `.md` — e.g. `ElkeJohnson2024_gnc-testing-quadcopter.md`. Note holds abstract, key findings, quotes, relevance-to-project.
- **New topic subfolder threshold:** promote a `misc/` bucket to its own topic subfolder once it holds 3+ related PDFs.

### 3.3 `automated_research_tools/` (each tool = full mini-project)

- Each tool IS a bootstrap mini-project: `docs/{README,scope,roadmap,todo,features,findings,archive}`, `src/`, `tests/`.
- `automated_research_tools/README.md` = tool registry (name, purpose, status, entry-point script).
- `automated_research_tools/INDEX.md` = one-line-per-tool index.
- `automated_research_tools/shared_config/` = shared endpoints, prompt libraries, prompt-template files.
  - **Never commit secrets.** `SECRETS.md` documents WHERE secrets live (env vars, path to untracked file, keyring name) — not the secrets themselves.
- Tool-produced research (findings the tool itself generated) lives in each tool's own `docs/findings/` — that's the meta-research pipeline.

### 3.4 `mini_projects/` (exploratory lifecycles)

- Each is a full bootstrap mini-project (per `04_mini_project_setup_guide.md`).
- Slug naming: `snake_case_short_slug/` — e.g. `ekf_theory_deepdive/`, `multilateration_survey/`, `dns_wifi_positioning_survey/`.
- `mini_projects/README.md` explains when to add one (see §4 graduation).
- `mini_projects/INDEX.md` = one-line-per-mini-project: `slug | status | one-line purpose | roadmap link`.

---

## 4. Graduation pattern (in → out)

| Type | Trigger | Destination | Mechanism |
|---|---|---|---|
| Mini-project matures into shipping code | Real hardware target picked, code stabilizes | Its own standalone repo, OR merged into an existing implementation repo (`swarm_communication_protocol`, `flight_controller`) | Copy `src/` + relevant `docs/`; leave a `GRADUATED_TO.md` stub in the old location. |
| Finding stabilizes into canonical theory for a topic repo | Cited by other work; owner declares it "stable" | The topic-specific research repo (`position_denial_research`, `auto_orientation_research`) — as a copy in that repo's `docs/theory/` or `docs/findings/` | **COPY, not move.** Canonical stays in `research/`. Cross-link both directions. |
| Automated tool matures into shared infra | Multiple consumers depend on it; needs its own release cadence | Standalone repo | Copy full mini-project; leave pointer. |
| Literature entry | Rarely graduates | Stays in `research/literature/` | Individual PDFs may be **copied** into topic repos for convenience; canonical index is `research/literature/INDEX.md`. |

Rationale for **copy-not-move** on findings: the topic-repo copy is the durable teaching version; the `research/` original stays as a working-history record. Divergence is acceptable — the graduated copy is the source of truth for the topic.

---

## 5. `researchhub_client` migration detail

**Current location:** `~/floppi/tools/researchhub_client.py` (2943 lines, zero-dep stdlib-only Python client per `03_folder_recon_findings.md`).

**Target location:** `research/automated_research_tools/researchhub_client/src/researchhub_client.py`

**Migration checklist (Phase-6 execution work, not this doc):**

- Copy file into `src/`. No refactoring on move.
- Write `docs/scope.md` — HTTP/CLI surface, endpoints hit, config precedence (CLI flags > env > `.researchhub.json` > defaults per current file header).
- Write `docs/roadmap.md` — near-term extensions (retry policy? auth headers? streaming?).
- Write `docs/features/api-surface.md` — commands + subcommands the client exposes.
- Write `docs/findings/` — empty at bootstrap; populate as the tool produces meta-research.
- Write `tests/persistent/test_contract.py` — contract test on the CLI argparse surface (fast, no network). No heavy suite.
- Ephemeral integration tests (real HTTP against a running ResearchHub) written per-question and deleted after use.

**Consumers pattern:** other repos MAY call the client via `python -m researchhub_client` or by copying the single file (its zero-dep header even documents drop-in copy). Canonical home is `research/`; copies are stale.

---

## 6. Automated-research-tool hookup pattern (repeatable)

Each new tool that lands in `automated_research_tools/` follows this contract:

1. **Registration:** add a row to `automated_research_tools/INDEX.md` and `automated_research_tools/README.md`.
2. **Bootstrap tree:** full mini-project shape (`docs/` + `src/` + `tests/`).
3. **Feature docs (`docs/features/`):** documents the endpoints/APIs/CLI surface the tool exposes.
4. **Findings docs (`docs/findings/`):** captures research the tool has PRODUCED — this is the meta-research feedback loop (tool ran → produced summary → summary lives here).
5. **Config:** tool-specific config in the tool's own dir; shared credentials/prompt-libs in `automated_research_tools/shared_config/` (no secrets committed).
6. **Tests:** contract tests in `tests/persistent/`; ephemeral integration tests otherwise.
7. **Pipeline docs (optional):** if the tool has a multi-stage ingest→process→emit pipeline, `docs/features/pipeline.md` documents where inputs come from and where outputs land.

Second and later tools slot in beside `researchhub_client/` with the same shape.

---

## 7. Testing pattern (per user directive)

Per user's "do NOT chase-your-tail with tests" directive and the project-wide EPHEMERAL-default policy:

- **Ephemeral by default:** most tests answer a specific question, then get deleted. No accumulation of stale test files.
- **`tests/persistent/` floor:** each automated tool's `tests/persistent/` holds only contract tests for API clients (input/output invariants, argparse surface, config precedence) — small, fast, network-free.
- **No heavy test infra:** no CI runners specified at this stage. Onboard diagnostics + spot-checking carry the load.
- **Mini-projects:** most have no persistent tests. If a mini-project produces reusable analysis code, it MAY earn a persistent contract test; default is none.
- **Literature/:** no tests at all — it's a store.

---

## 8. What does NOT belong in `research/`

Explicit exclusions (mirror into `docs/scope.md`):

- **Shipping code** — belongs in implementation repos (`flight_controller`, `swarm_api`, `fc_tool`).
- **Hardware POCs** — go to `communication_hardware`, `networking_pocs`, or `sensor_interactions` per POC-by-domain framing (`00_reorg_master_plan.md` §1).
- **Mission context** — `darpa_lift_2026`.
- **Design assets** — `drone_frame_modeling`.
- **Topic-canonical theory** — once stable, lives in the topic research repo (`position_denial_research`, `auto_orientation_research`). Working-history stays here.
- **Cybersecurity POCs** — deferred to `cybersecurity_demos` when it materializes.
- **Secrets** — never. Documented references only.

---

## 9. Cross-repo linking convention

- **Within `research/`:** relative paths (`../literature/uwb/INDEX.md`).
- **Across GitLab repos (after migration):** absolute GitLab URLs. Consistent with `00_reorg_master_plan.md` §5 standards suggestion.
- **Graduation stubs:** when content graduates out, leave a `GRADUATED_TO.md` (or a stub `README.md`) with the absolute link to the new home. Same pattern the master plan calls out for POC graduation.

---

## 10. Initial content commitment (from `03_folder_recon_findings.md`)

At Phase-4 migration, `research/` is seeded with:

- **`literature/`** — the PDFs currently in `~/floppi/literature/`:
  - `dRehmFlight VTOL Documentation.pdf` → `literature/flight_dynamics/`
  - `DWM1000 Data Sheet.pdf`, `dws1000productbriefv10.pdf` → `literature/uwb/`
  - `ElkeJohnson…2024…GNC-Testing…quadcopter.pdf` → `literature/flight_dynamics/`
  - `fs-ia6b-manual.pdf` → `literature/vendor_datasheets/` (RC receiver)
  - `gy-521_mpu-6050…datasheet.pdf`, `MPU-6000-Datasheet1.pdf` → `literature/imu/` (also cross-index under `vendor_datasheets/`)
  - `Longfly dRehmFlight Purchase Lists.pdf`, `longfly pcb.webp` → `literature/vendor_datasheets/` (BoM assets)
  - `Morphy_A Compliant and Morphologically Aware Flying Robot.pdf` → `literature/flight_dynamics/`
  - `RFM69HCW-V1.1.pdf` → `literature/vendor_datasheets/` (radio)
  - Companion `.md` transcripts (`drehmflight_README.md`, `drehmflight_transcripts.md`, `init_about_refactored_drehmflight.md`, `resources.md`) → colocated as summary notes in the same topic subfolder.
- **`automated_research_tools/researchhub_client/`** — migrated from `~/floppi/tools/researchhub_client.py` per §5.
- **`mini_projects/`** — TBD by user (see §11 Q-L follow-ups). Candidate seeds from `~/floppi/research/` (GPS coordinate-systems docs) may become an `mini_projects/gps_coordinate_systems_survey/` — pending user call.
- **`docs/findings/`** — cross-cutting research pieces from `~/floppi/research/` that don't fit a topic repo (per `03_folder_recon_findings.md` routing recommendations).

Nothing else. Everything else waits for a decision.

---

## 11. Open Q-L follow-ups

Q-L parent question ("`research` repo internal structure") is answered by §§1–10 above. Sub-questions that surface once user reviews:

- **Q-L.1** — Repo shape sign-off: does user approve the 5-top-level layout (§2)?
- **Q-L.2** — Initial mini-projects: what mini_projects does user want seeded on day one? (candidates: `gps_coordinate_systems_survey`, `ekf_theory_deepdive`, `multilateration_survey`, `dns_wifi_positioning_survey`)
- **Q-L.3** — Secrets management for `automated_research_tools/shared_config/`: env vars per host, or a single untracked `.secrets.local` file, or per-tool config? Needs a convention before second tool lands.
- **Q-L.4** — `docs/features/` at the repo level: keep for consistency (even mostly empty) or drop? Bootstrap says keep; ask user.
- **Q-L.5** — Do we want an explicit `docs/discovery/` or `docs/pipeline/` subfolder to describe how automated tools ingest sources and where their outputs land, or is per-tool `docs/features/pipeline.md` enough (§6.7)?
- **Q-L.6** — Literature-topic subfolder list: is the 7-topic seed (`uwb`, `gps`, `imu`, `flight_dynamics`, `swarm_algorithms`, `communications_theory`, `vendor_datasheets`) the right cut? Add `power_electronics`? `rc_receivers`?
- **Q-L.7** — Cross-link with `position_denial_research`: any pre-agreed rule for which UWB literature is `research/literature/uwb/` canonical vs `position_denial_research/theory/uwb/` canonical? Default per §4: `research/` canonical; topic repo holds a curated subset.
- **Q-L.8** — Repo naming (upstream Q-H): still `research` vs alternatives? Confirm before Phase-2 repo creation.
