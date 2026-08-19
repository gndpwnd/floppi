# floppi — operating rules for AI assistants (Claude & sub-agents)

## ‼️ RULE #1 — GIT IS HUMAN-ONLY. NO EXCEPTIONS.
Claude and **every sub-agent it spawns** MUST NEVER run any git write/state operation —
NOT `commit` · `add` · `push` · `pull` · `merge` · `rebase` · `reset` · `checkout` · `restore` ·
`stash` · `tag` · `clean` · `rm` · `revert` · `cherry-pick`. Not "with permission," not "just this
once," not at a checkpoint. **Only the human operator runs git writes.**

If git work is needed, **hand the operator the exact commands and stop.** Do not offer to run them.
Reads are fine (`status` / `diff` / `log` / `show`). You **MAY edit `.gitignore`** (a file edit, not a git op).

## ‼️ RULE #2 — NO REPO CRUD, NO GITLAB CRUD.
No agent creates, renames, deletes, or re-scopes a GitLab repo; no `git init`; no branch-switching of
any source repo. All repo CRUD is **manual, by the operator**, driven by
[temp_reorg/19_operator_execution_handoff.md](temp_reorg/19_operator_execution_handoff.md)
(`09_gitlab_operations_checklist.md` is superseded in part — its repo counts are stale).
This is both a bootstrap non-negotiable and this project's own operating principle
([temp_reorg/00_reorg_master_plan.md](temp_reorg/00_reorg_master_plan.md) §5).

## What this repo IS right now

**floppi is a migration SOURCE ARCHIVE, not a destination.** Its content is being distributed into
**9 active repos + 1 deferred** under the GitLab group **`lowprofiledronegurus`** (down from a planned
13 — three were dropped by ruling). floppi itself is **never deleted, and never modified from outside**:
it stays on local disk indefinitely as the archive of record. Current ledger + routing:
`temp_reorg/11_routing_v2_2026-08-18.md`. Every decision and its ruling: `temp_reorg/10_decision_queue.md`.
What the operator actually runs: `temp_reorg/19_operator_execution_handoff.md`. Start at
`temp_reorg/INDEX.md`.

**⚠ NEVER `filter-repo` or history-purge `~/floppi`.** The ResearchHub `repo_untrack` fence is
*index-only* — it staged `git rm --cached` and never committed — so the untracked research corpus is
**still at plain `HEAD`**, and `engineer360` holds `dd39f23` as a live recovery ref. A purge would destroy
one of only two copies and silently break that pointer. floppi is not a bloat case anyway
(`size-pack: 146.64 MiB`, `garbage: 0`).

### Project classes — this governs what you may do to each tree

| Class | Trees | Rule |
|---|---|---|
| **ACTIVE** | `~/floppi`, `~/lowprofiledronegurus`, `~/engineer360`, `~/hiverf` | Coordinate as peers over the bus |
| **LEGACY / SPRINGBOARD** | `~/SwarmLoc` (`msmcs-robotics/SwarmLoc`, `main`) · `~/GravityProbe` (`msmcs-robotics/GravityProbe`, **`data-analysis`** — not `main`) · `~/WayfindR-driver` (+`WayfindR-android`) | **DIRECTIONAL, see below** |

**The legacy rule is directional, and both halves are binding.** Writing INTO a legacy repo is
**forbidden** — no delete, no modify, no branch work, no cleanup commits, no `.gitignore` edits. Taking
findings and useful items **OUT is allowed and expected** — selectively, with attribution, never a
wholesale `cp -r`. They are **mined, not migrated**.

**The planning corpus is DONE — do not re-derive it.** `temp_reorg/` holds 22 documents. Read it, extend
it, correct it in place. **Start at `temp_reorg/INDEX.md`**, which marks what is live vs superseded —
`00` §3, `01`, `03` §14 and `09` all carry stale routing behind supersession headers, and two sessions
have already filed findings against superseded sections.

## Sub-projects (each becomes its own GitLab repo)

| Path | Becomes | Notes |
|---|---|---|
| `auto_orientation/` | `auto_orientation_research` | Balance-robot + orientation/calibration framework (AVR/ESP32/Teensy, BNO055/BNO085). Mature. |
| `flight_controller/` | `flight_controller` | dRehmFlight-derived VTOL firmware (Teensy 4.x + ESP32). Mature. |
| `fc_tool/` | `fc_tool` | Tauri 2 desktop serial monitor/plotter (Rust + JS, pytest harness). |
| `swarm_api/` | `swarm_api` (new repo) | Python FastAPI ground station + systemd deploy wrapping. |
| `drone_3d_model/` | **split** | Structural/aero *research* → `engineer360/docs/domains/uav-airframes/` (via the distilled channel, not a `cp -r`). The 5 reference `.zip` models → same place. `drone_frame_modeling` is re-scoped to **CAD/mesh/BOM/print-config for airframes actually built** — correctly empty until the first build. |
| `darpa_lift_2026/` | **DROPPED — no repo** | The challenge is dropped. The durable capability is *requirements-driven design sizing*; the ~27 KB of operator-authored material becomes a worked case study in `research/`. The ~77 KB of raw LLM transcript does not migrate. |
| `dRehmFlight-master/` | `flight_controller/vendored/` | Vendored upstream — NOT its own repo. |
| `literature/`, `research/`, `docs/`, `scripts/`, `tools/` | per-file split | Routing table: `temp_reorg/11_routing_v2_2026-08-18.md` §2 (**not** `03` §14 — superseded). Literature specifically: `17_literature_routing.md`. |
| `temp_reorg/` | **stays here** | The planning corpus. Not migrated. |

Each sub-project already carries its own `docs/{scope,roadmap,todo}.md` — read the sub-project's
before touching its code. Full status narrative: [README.md](README.md).

## Cross-session coordination (Claude ↔ Claude)

This session is node **`floppi`** on the file bus at `/home/devel/claude_coordination/`
(helper `coord.sh`, contract `PROTOCOL.md`). **Four peer nodes:** `lowprofiledronegurus` (target side),
`engineer360` (computational design — owns the *method*), `hiverf` (RF/EW), `researchhub` (research sync).

- Register the watch on session start if it isn't live:
  `bash /home/devel/claude_coordination/coord.sh watch floppi` → register the printed one-liner with
  the **Monitor** tool, `persistent: true`.
- `coord.sh list floppi` → `coord.sh read floppi` → act → `coord.sh ack <msg_id> "<outcome>"`.
  **Always close the loop** — silence is indistinguishable from a crashed watch.
- **Division of labor:** `floppi` owns the source side (inventory, per-file routing, planning corpus,
  staging). `lowprofiledronegurus` owns the target side (repo skeletons, README standardization,
  per-repo bootstrap).
- **No fixed front door** (ruled 2026-08-18). Any session may ask the operator directly. The obligation
  is **de-duplication, not routing** — check `temp_reorg/10_decision_queue.md` and the bus for whether a
  peer already owns a question before asking, and mirror every ruling you receive into that file **and**
  onto the bus the same turn.
- **Gotcha:** piping `coord.sh read` into `head` kills it with SIGPIPE *before* the `new/ → cur/` move, so
  processed messages still show as unread. Read the file directly instead.

## Other hard constraints

- **Hardware-first testing.** Most tests belong on real hardware, not host/native suites. Do not build
  out native harnesses for coverage's sake; a small native harness for pure math is fine but never a
  priority. Hardware-gated work waits for a bench session.
- **Nothing here is flight-validated.** Both firmware projects are code-complete on the no-hardware
  axis only; 41 items are hardware-deferred across the two bench runbooks. Never describe them as
  production-ready. Read the relevant `docs/findings/bench_validation_runbook_2026-05-27.md` before
  any claim about real-world behavior.
- **Simplicity over cleverness.** Favor the simplest clear implementation; flag complexity rather than
  layering abstraction. Keep INDEX.md files current.
- **Verify the real end-state.** Green / exit-0 / "complete" is a claim, not proof — re-check on the
  real path.
- **Delegate heavy/multi-step work to sub-agents** so the main context stays lean. Every agent gets an
  exclusive WRITE_ZONE (explicit file list); no two concurrent agents write the same file; freeze any
  shared interface verbatim in both prompts. **Cap concurrent PlatformIO builds at ONE per project per
  wave** (`.pio` SCons cache races). Gate every spawn on host RAM/swap.
- **Project knowledge lives in `docs/`**, never only in `~/.claude/.../memory/` (machine-local, not
  indexable, recall-layer only).

## Read first, each session

[docs/todo.md](docs/todo.md) (CURRENT STATE + NEXT ACTION — SSOT) → [docs/scope.md](docs/scope.md) →
[temp_reorg/INDEX.md](temp_reorg/INDEX.md) (what is live vs superseded) →
[temp_reorg/11_routing_v2_2026-08-18.md](temp_reorg/11_routing_v2_2026-08-18.md) (routing + hazard gates)
→ the sub-project's own `docs/todo.md` if you're touching its code.

Standards hub: `~/llm-project-bootstrap/` (`PROMPTS.md`, `guides/`, `directives/`,
`docs/lessons_learned/lessons/`). Orchestration protocols:
`/home/devel/palletai/claude_code_orchestra/CONTEXT_FOR_NEW_CONVERSATIONS.md`.
