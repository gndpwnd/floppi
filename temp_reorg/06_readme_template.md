# 06 — Cross-repo README Template

**Purpose.** Single starting template that every repo in the `lowprofiledronegurus` GitLab group uses for its top-level `README.md`. Adapts `~/llm-project-bootstrap/templates/readme_template.md` to the reorg's taxonomy so a reader lands on any repo and immediately knows: what category it is, how it's shaped, and where to look next.

Reference companions in this planning corpus:
- `01_target_repos_v2.md` — per-repo category boundaries
- `04_mini_project_setup_guide.md` — full mini-project layout (for B-* and research)
- `~/llm-project-bootstrap/templates/readme_template.md` — generic base this refines
- `~/llm-project-bootstrap/guides/PROJECT_SETUP.md` — repo/project structure conventions

Two template variants follow. Every repo picks exactly one.

---

## Category tag (required top-matter, both variants)

Every README begins with a `Category:` line drawn from this fixed list:

| Tag | Meaning | Repos |
|---|---|---|
| **A. Pure theory / research** | Single-project research, may have build variants | `auto_orientation_research`, `position_denial_research`, `swarm_communication_protocol` |
| **A. Centralized research-staging (multi-mini-project)** | Umbrella for one-off research pokes | `research` |
| **B-comm. Communication hardware POCs (multi-mini-project)** | Radio/RF/link-layer hardware probes | `communication_hardware` |
| **B-network. Networking POCs (multi-mini-project)** | Above-link networking, mesh, routing | `networking_pocs` |
| **B-sensor. Sensor POCs (multi-mini-project)** | Non-radio, non-networking sensor interactions | `sensor_interactions` |
| **B-cyber. Cybersecurity POCs (multi-mini-project)** | Attack/defense demonstrations | `cybersecurity_demos` |
| **C. Implementation / production** | Real system, users depend on it | `flight_controller`, `fc_tool`, `swarm_api` |
| **D. Design assets** | CAD, meshes, drawings | `drone_frame_modeling` |
| **E. Mission / competition context** | Documentation of an external goal | `darpa_lift_2026` |

`Status:` line values: `early` | `active` | `stable` | `archived`.

---

## Variant 1 — Single-project repo README

Use for categories **A (single)**, **C**, **D**, **E**.

```markdown
# <Repo Name>

**Category:** <A / C / D / E — pick from the table>
**Status:** <early | active | stable | archived>

<One-line description — what this repo is, in one sentence>

## Overview

<2-3 sentences: purpose, what problem it solves, who or what depends on it.>

## Quick Start

\`\`\`bash
# Clone
git clone <gitlab-url>
cd <repo-name>

# Install / setup
./scripts/install.sh

# Run / build
./scripts/run.sh          # or make, or platform-specific command
\`\`\`

## Project Structure

\`\`\`
<repo-name>/
├── docs/
│   ├── scope.md          # boundary contract
│   ├── roadmap.md        # feature plan
│   ├── todo.md           # current tasks
│   ├── features/         # feature specifications
│   ├── findings/         # research + discoveries
│   └── archive/          # superseded docs
├── src/                  # source code
├── tests/
│   ├── persistent/       # smoke, invariants, contracts
│   └── (ephemeral tests live here transiently; delete after use)
├── scripts/              # install / test / deploy entry points
└── LICENSE
\`\`\`

## Documentation

- [scope.md](docs/scope.md) — boundary contract (what belongs, what does not)
- [roadmap.md](docs/roadmap.md) — feature plan
- [todo.md](docs/todo.md) — current tasks
- [features/](docs/features/) — feature specifications
- [findings/](docs/findings/) — research and discoveries

## Cross-repo links

- `<sibling repo>` — <one-line relationship>
- `<sibling repo>` — <one-line relationship>

(Pre-migration: use relative paths from the group root. Post-migration: use GitLab URLs.)

## Requirements

- <hardware>
- <software / OS>
- <language runtime + version>
- <external services or accounts>

## License

See LICENSE file.

---
*For detailed scope and technical decisions, see [docs/scope.md](docs/scope.md).*
```

---

## Variant 2 — Multi-mini-project repo README

Use for **B-comm**, **B-network**, **B-sensor**, **B-cyber**, and the **A centralized research-staging** repo (`research`).

```markdown
# <Repo Name>

**Category:** <B-comm | B-network | B-sensor | B-cyber | A centralized research-staging> (multi-mini-project)
**Status:** <early | active | stable>

<One-line description of the umbrella domain this repo hosts>

## Overview

<2-3 sentences: what mini-projects live here, why they are grouped together, what the umbrella does NOT cover (points at sibling repos).>

## Mini-projects

Authoritative list: [docs/INDEX.md](docs/INDEX.md). Highlights:

- **<mini-1>** — <one-line purpose>
- **<mini-2>** — <one-line purpose>
- **<mini-3>** — <one-line purpose>

## Mini-project shape

Every mini-project in this repo follows the shared mini-project layout
(`docs/`, `src/`, `tests/`, `scripts/`). See the mini-project setup guide
for the exact tree and required doc files. During planning the guide lives
at `temp_reorg/04_mini_project_setup_guide.md`; each repo copies the
relevant portion into its own `docs/MINI_PROJECT_GUIDE.md` after migration.

## Repo-level Documentation

- [scope.md](docs/scope.md) — what belongs here vs. sibling repos
- [roadmap.md](docs/roadmap.md) — cross-mini roadmap
- [todo.md](docs/todo.md) — cross-mini tasks
- [INDEX.md](docs/INDEX.md) — every mini-project enumerated
- [findings/](docs/findings/) — cross-mini research

## Cross-repo links

- `<sibling repo>` — <one-line relationship>
- `<sibling repo>` — <one-line relationship>

## Contributing new mini-projects

Follow the mini-project setup guide. Ping the human contact for a scope
review before merging a new mini-project stub, so it does not drift into a
sibling repo's territory.

## License

See LICENSE file.

---
*For per-mini-project details, navigate to that mini-project's own `docs/`.*
```

---

## Required top-matter (both variants)

Non-negotiable. A README that omits any of these fails review:

1. **Category:** line — must match the taxonomy table above verbatim.
2. **Status:** line — one of `early`, `active`, `stable`, `archived`.
3. **One-line description** — a single sentence beneath the top-matter.
4. **Cross-repo links** — at minimum a stub section listing one sibling; empty is a red flag that this repo is not really part of the group.

---

## Style rules

- No emojis. Not in headings, not in bullets, not as decoration.
- No time estimates ("~2 weeks", "Q3"). Roadmap docs carry those; the README does not.
- No marketing language. Technical readers only.
- Cross-links inside the repo use paths relative to the README (`docs/scope.md`), not absolute.
- Cross-repo links use GitLab group-relative URLs post-migration. Pre-migration, use paths relative to the group root.
- Fence code blocks explicitly with `bash`, `python`, or the appropriate language tag.
- Prefer prose in complete sentences over telegraph-style fragments.

---

## Category-specific customization notes

Extend, do not remove, template fields. Each category may add sections; none may drop required top-matter.

### A. Pure theory / research (single, with build variants)
`auto_orientation_research`, `position_denial_research`, `swarm_communication_protocol`
- Add a **Build variants** section listing the target platforms (rover / drone / land-vehicle for auto_orientation; sub-topics for position_denial).
- Each variant gets a bullet with its `scripts/run_<variant>.sh` entry point.
- Quick Start shows one representative variant; the rest live under Build variants.

### A. Centralized research-staging (`research`)
- Same shape as other multi-mini-project repos.
- Add a short **Graduation policy** section: when a mini-project outgrows the staging umbrella, it moves to a permanent home. Cite the master plan section.
- Note that `tools/researchhub_client.py` (relocated from `floppi/tools/`) is the automated-research-tool hookup point (per `03_folder_recon_findings.md`).

### B-comm / B-network / B-sensor / B-cyber
- Standard multi-mini-project variant. No special sections.
- `B-cyber` (`cybersecurity_demos`): README exists from day one; `docs/INDEX.md` may start empty. Add a **Materialization** note: "First mini-project stub lands here once a concrete POC is proposed."

### C. Implementation / production
`flight_controller`, `fc_tool`, `swarm_api`
- Quick Start must be runnable by a new contributor with no prior context.
- `fc_tool` (Tauri): add a **Platform-specific setup** subsection under Quick Start (Linux / macOS / Windows). Extend, do not replace.
- `flight_controller`: add a **Hardware targets** section listing supported boards.
- `swarm_api`: add an **API surface** section pointing at the OpenAPI/schema file.

### D. Design assets (`drone_frame_modeling`)
- Quick Start header stays; body reads `N/A (asset repo — see docs/scope.md for viewer/tool recommendations)`.
- Add a **File formats** section (STEP, STL, F3D, etc.) so readers know what tooling they need.
- `tests/` directory is likely absent; the Project Structure block should omit it rather than list an empty dir.

### E. Mission / competition context (`darpa_lift_2026`)
- Replace the Quick Start section body with a **How to read this repo** paragraph. Keep the section header identical.
- Add a **Timeline anchors** section (key competition dates) but keep it fact-only, no speculation.
- Cross-repo links here are especially important — this repo pulls from many siblings.

---

## When to deviate at all

The template covers 95% of cases. Legitimate reasons to deviate:

1. A repo has a hard external constraint (e.g., a sponsor requires a specific badge).
2. A repo genuinely has nothing to run (design assets — handled above).
3. A repo is archived and the README should say so at the top and stop.

Illegitimate reasons: personal preference, "it looked cleaner", "the section felt redundant". Redundancy is deliberate; readers land on random READMEs and need the same anchor points every time.

---

## Open questions (defer to execution phase)

- Should each repo carry a permanent copy of `MINI_PROJECT_GUIDE.md` inside `docs/`, or link to a canonical copy in the `research` repo? (Current recommendation: permanent copy per repo — repos should be self-describing.)
- Badge policy (CI status, license badge)? Defer until CI is set up.
- Should the top-matter include a `Contact:` line? Defer until the group has a stable maintainer table.
