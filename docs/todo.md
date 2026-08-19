# floppi — TODO (SSOT)

mode: BLOCKED:operator-decisions  <!-- Migration execution is gated on the STOP-marker rulings below. Non-blocked threads (inventory, per-file routing, corpus corrections, coord relay) proceed on defensible defaults. -->

> Last updated: 2026-08-18. Supersedes `docs/todo/TASKS.md` (2026-03-30, now historical).

---

## ⭐ CURRENT STATE (verified on disk 2026-08-18, not carried forward)

**floppi is a migration SOURCE ARCHIVE.** Its content is being distributed into 12-13 repos under the
GitLab group `lowprofiledronegurus`. floppi, `~/SwarmLoc`, and `~/GravityProbe` all stay on local disk
indefinitely as historical archives — never deleted.

**Planning: RE-DERIVED 2026-08-18.** The 2026-07-09 corpus in [`temp_reorg/`](../temp_reorg/) still holds
the taxonomy, repo cards, mini-project pattern, `research` layout, README template and the GitLab CRUD
checklist — but a set of operator rulings on 2026-08-18 **invalidated its routing conclusions**.
`00` §3 and `03` §14 are marked SUPERSEDED in place. **Current routing:
[`temp_reorg/11_routing_v2_2026-08-18.md`](../temp_reorg/11_routing_v2_2026-08-18.md).
Decisions + rulings: [`temp_reorg/10_decision_queue.md`](../temp_reorg/10_decision_queue.md).**

**The rulings, in force:**
- **R1 — SwarmLoc + GravityProbe are MINED, not migrated**, and are eventually deleted. This guts the
  source basis of four planned repos.
- **R2 — DARPA Lift is dropped.** No repo. The durable capability is *requirements-driven design sizing*;
  DARPA Lift becomes one worked case study inside `research`.
- **R3 — org purpose:** `lowprofiledronegurus` is for **drone / UAV / UAS / cUAS prototyping** — working
  airframes first, then working swarms/flocks. EW/SIGINT is a sibling (`hiverf`), integrated later.
- **R4 — `engineer360` keeps everything hitting its scope**; `drone_frame_modeling` may reduce to
  structural/aero reference or fold into `engineer360`. **UNSETTLED — do not populate it.**
- **R5 — ResearchHub syncs DISTILLED research only** — no `generated/`, no `sources/`. (ResearchHub
  replied: current sync *already* honours this; the on-disk bloat is legacy residue from an older config,
  and the 2026-07-11 floppi fence was their own `repo_untrack.py` tooling.)
- **R6 — LEGACY/SPRINGBOARD class, READ-ONLY:** `~/SwarmLoc`, `~/GravityProbe`, `~/WayfindR-driver`
  (+`WayfindR-android`). **Do not delete, do not modify** — read from them, write documentation when
  needed. ACTIVE peers: `~/floppi`, `~/lowprofiledronegurus`, `~/engineer360`, `~/hiverf`.
- **R7 — `sensor_interactions` is the SUCCESSOR repo**, inheriting WayfindR's lidar corpus and
  GravityProbe's sensor bring-up. Not thin.
- **Q-N — `auto_orientation_research` keeps its name.**
- **WiFi security PARKED** by the operator — recorded in `11_routing_v2` §4, gating nothing.

**Repo list: 13 → 9 active + 1 deferred. Nothing is unsettled.** DROPPED: `darpa_lift_2026`,
`communication_hardware`, `networking_pocs`. All 6 repos that already exist on GitLab survive:
`sensor_interactions` resolved by R7 (successor repo), `drone_frame_modeling` by N-9 (re-scoped to
CAD/mesh/BOM/print-config for airframes actually built). No repo deletion is proposed.

- **R8 — engineer360 owns the METHOD, the fleet owns BUILDS.** Boundary test agreed by all three sessions:
  *a number, a curve, a geometry, or a piece of knowledge → engineer360; a thing that flies or talks to a
  thing that flies → the fleet.*
- **N-10 — NO git history transfer.** New repos start at commit 1; a `PROVENANCE.md` per repo carries the
  pointer back. Operator: *"i really don't want to transfer git history at all… its not like we are
  deleting ~/floppi/ so we can always reference it."* Template + verified provenance facts for all four
  sources + per-repo "what's worth going back for" →
  [temp_reorg/18_provenance_template.md](../temp_reorg/18_provenance_template.md).

**Execution: NOT STARTED.** Zero content migrated. All sessions holding.

**Peer sessions on the bus (4):** `lowprofiledronegurus` (target side), `hiverf` (RF/EW — boundary agreed:
they own non-cooperative other-localization, `position_denial_research` owns cooperative
self-localization-under-denial and cites them), `engineer360` (computational design; owes a scope boundary
vs `drone_frame_modeling`), `researchhub` (owes `repositories.json` repointing + distilled-only contract).

### Verified facts (re-checked today, superseding the 40-day-old plan where they differ)

- **GitLab remote state — OBSERVED, not inferred.** 6 repos exist, each with a `main` branch and nothing
  but stock GitLab boilerplate (1 commit): `auto_orientation_research`, `drone_frame_modeling`, `fc_tool`,
  `flight_controller`, `position_denial_research`, `sensor_interactions`. The 7 planned repos are
  **confirmed absent** (`swarm_communication_protocol`, `swarm_api`, `darpa_lift_2026`,
  `communication_hardware`, `networking_pocs`, `research`, `cybersecurity_demos`).
- **GitLab SSH auth — diagnosed and FIXED (2026-08-18).** Symptom was `Permission denied (publickey)`
  on every gitlab.com call; root cause was NOT a missing GitLab key. `~/.ssh/config` had no
  `Host gitlab.com`/`Host github.com` block, so ssh only offered default-named identities and never tried
  `~/.ssh/id_git`; the shell's `SSH_AUTH_SOCK` also pointed at a dead agent. `id_git` authenticates to
  **both** forges (`Welcome to GitLab, @gndpwnd!` / `Hi gndpwnd!`). Operator appended the two Host blocks
  (backup at `~/.ssh/config.bak-2026-08-18`); verified with plain `ssh -T`, no `-i`, no agent.
- **`drone_3d_model/generated/` + `sources/` staged deletions are DELIBERATE, not an accident.**
  139 staged deletions (~46 MB, recoverable from HEAD) are a ResearchHub `repo_untrack` fence added
  2026-07-11: `drone_3d_model/.gitignore` and `flight_controller/.gitignore` both gained
  `docs/findings/sources/`, `generated/`, `sources/`. These are RAG-generated synthesis/knowledge
  artifacts and downloaded source PDFs. **No index reset is warranted** — committing the index does
  exactly what was intended. Open question is only whether that content migrates (see D-6).
- **Three source repos** — `~/floppi` (github `gndpwnd/floppi`, main), `~/SwarmLoc` (github
  `msmcs-robotics/SwarmLoc`, main), `~/GravityProbe` (github `msmcs-robotics/GravityProbe`,
  **`data-analysis` branch**). Operator has signalled this set may be **incomplete** — treat as a floor.
- **Coordination bus is live and bidirectional.** This session = node `floppi` (persistent Monitor
  registered); target session = node `lowprofiledronegurus` (registered, acked). Both added to the
  roster in `/home/devel/claude_coordination/coord.sh`.

---

## 🔴 NEXT ACTION

1. **Peer dependencies CLEARED.** `engineer360` replied (N-9 closed, R8 agreed); `researchhub` accepted
   the path map and the pre-move sequencing (N-6 settled — one staged repoint,
   `floppi/flight_controller` → `lowprofiledronegurus/flight_controller`, applied in-place on my
   heads-up). All four peer sessions are live with armed watches.
2. **Operator ruling still outstanding — N-7 only:** do `engineer360` / `hiverf` move into the
   `lowprofiledronegurus` group, or stay at `gndpwnd/` and get cross-linked? (`hiverf` recommends staying
   independent and being cited; they are escalating it themselves — do not double-ask.) **This is the last
   open decision in the ledger.**
3. **ResearchHub config for the `lowprofiledronegurus` repos = the `lowprofiledronegurus` session's job**,
   assigned by the operator 2026-08-19. Not mine. My only entry is the `floppi_flight_controller`
   in-place repoint. Contract handed over; see `19_operator_execution_handoff.md` §7.
4. **Mining extracts — LANDED, verification in flight.** `14_swarmloc_uwb_extract.md`,
   `15_swarmloc_capability_inventory.md`, `16_gravityprobe_capability_inventory.md`,
   `17_literature_routing.md`. Adversarial verifiers are checking each for unsupported claims, cited paths
   that do not resolve, credential leaks and overclaiming. **Treat none as settled until verify reports.**
4. **Literature dedupe — SETTLED** (`17_literature_routing.md` §1). The common premise was *inverted*:
   `~/floppi/literature/` and `docs/literature/literature/` share **zero** byte-identical files. The real
   duplicate is `docs/literature/literature/` vs `docs/literature/` — 4 files, safe to drop, named
   individually. **`docs/literature/findings/serial-rich-text-formatting.md` is the landmine** — unique,
   no copy anywhere, and a wrong-way dedupe eats it.

## 🟡 BLOCKED

- **`drone_frame_modeling` population** — blocked on R4/N-9 pending the `engineer360` reply.
- **GitLab repo creation** — human-only CRUD. Now only 2 repos to create (`swarm_communication_protocol`,
  `swarm_api`) plus `research`, not 6.
- **Source-repo deletion** — **not happening at all (R6).** The three legacy repos are read-only: not
  deleted, not modified, no cleanup commits, no `.gitignore` edits, no branch work.
- **Repo deletion on GitLab** (`darpa_lift_2026` never created; `drone_frame_modeling`/`sensor_interactions`
  questionable) — human-only CRUD, flagged only.
- **⚠ `~/floppi` is EXCLUDED from the fleet history purge.** Its untracked corpus is *staged but never
  committed*, so it is still at plain `HEAD` — `git show HEAD:drone_3d_model/generated/<file>` resolves,
  and `engineer360` has been given `HEAD` (`dd39f23`) as their recovery ref. A `filter-repo` pass would
  destroy one of only two copies and silently break that pointer. floppi needs no purge anyway
  (`size-pack: 146.64 MiB`, `garbage: 0`). Broadcast to all four peers; `11_routing_v2` §5 Gate 6.
- **All hardware-gated firmware work** — 41 items across the two bench runbooks
  (`auto_orientation/docs/findings/bench_validation_runbook_2026-05-27.md`, 24 items;
  `flight_controller/docs/findings/bench_validation_runbook_2026-05-27.md`, 17 items). Waits for a bench
  session. Neither firmware project has ever been validated end-to-end on real hardware.

## 🟢 UP NEXT (unblocked — proceed on defensible defaults)

- [ ] Burn down the 11 LOW/MED per-file routing flags in `temp_reorg/03_folder_recon_findings.md` §15
      (each is a 5-30 min read-and-decide; they are recommendations, not human-only calls).
- [ ] Reconcile the `temp_reorg/` corpus against today's disk — record verified deltas in place, dated.
- [ ] Relay rulings to `lowprofiledronegurus` over the bus as they land; keep the division of labor
      (source side here, target side there).

## 📋 BACKLOG

- [ ] Per-repo bootstrap (`~/llm-project-bootstrap/PROMPTS.md` § "Bootstrap Existing") for each new repo
      — explicitly deferred by the operator until the project/repo/scope inventory is settled.
- [ ] docs-rag deployment per repo — operator deferred ("how about not just yet").
- [ ] Phase 6 freeze: pointer READMEs in all three source repos (wording is STOP marker #7).

## ✅ RECENTLY COMPLETED

- [x] Coordination inbox stood up; `floppi` + `lowprofiledronegurus` nodes added to the bus; handshake
      exchanged and acked both directions — 2026-08-18
- [x] `CLAUDE.md` authored at repo root (was absent entirely) — 2026-08-18
- [x] GitLab remote state and SSH auth diagnosed and verified read-only — 2026-08-18
- [x] `drone_3d_model` staged-deletion hazard characterized as a deliberate untrack fence — 2026-08-18
- [x] Routing re-derived under the rulings → `temp_reorg/11_routing_v2_2026-08-18.md`; `00` §3 and `03`
      §14 superseded in place — 2026-08-18
- [x] Decision queue built + maintained jointly → `temp_reorg/10_decision_queue.md` — 2026-08-18
- [x] WayfindR reference inventory written for the `lowprofiledronegurus` startup →
      `temp_reorg/12_wayfindr_reference_inventory.md` — 2026-08-18
- [x] Coordinated with 4 peer sessions (`lowprofiledronegurus`, `hiverf`, `engineer360`, `researchhub`);
      hiverf boundary agreed; ResearchHub path map delivered — 2026-08-18
- [x] GitLab/GitHub SSH auth root-caused (missing `~/.ssh/config` Host blocks + dead agent socket) and
      fixed by the operator; both forges verified — 2026-08-18
- [x] Live GitLab org state OBSERVED (not inferred): 6 repos exist w/ `main`, 7 absent; all 6 local
      clones in sync with remote, genuinely pristine — 2026-08-18

---

## Notes

- `docs/todo/TASKS.md` (2026-03-30) is superseded by this file and kept as history.
- Sub-projects each keep their own `docs/todo.md`; this file is the repo-level SSOT and the migration
  state of record. When touching a sub-project's code, read its own todo too.
