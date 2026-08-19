# `temp_reorg/` — index

The planning corpus for consolidating the drone work into the **`lowprofiledronegurus`** GitLab group.

**If you read only one thing:** [`11_routing_v2_2026-08-18.md`](11_routing_v2_2026-08-18.md) — the routing
authority. **If you are about to ask the operator something:** check
[`10_decision_queue.md`](10_decision_queue.md) first; it may already be ruled, or owned by another session.

⚠ **Two documents have been superseded in part.** `00` §3 and `03` §14 carry the original source→target
routing, derived when SwarmLoc and GravityProbe were still treated as migration sources. **They are still
valid as recon — what is on disk and why — but their routing conclusions are obsolete.** Both carry an
in-place supersession pointer. Two peer sessions have now filed findings against these superseded
sections; check `11_routing_v2` before concluding anything is unrouted.

---

## Live authorities

| Doc | What it is |
|---|---|
| [`11_routing_v2_2026-08-18.md`](11_routing_v2_2026-08-18.md) | **Routing authority.** The 9-repo ledger, what moves where, the mining pass, and §5 the hazard gates (ELF binaries, git hooks, history, R6 directionality) |
| [`10_decision_queue.md`](10_decision_queue.md) | **Ruling log + open decisions.** Every item with a recommendation, cost-of-defaulting, and dated ruling. **Jointly owned by all sessions — edit it directly** |
| [`12_wayfindr_reference_inventory.md`](12_wayfindr_reference_inventory.md) | What `~/WayfindR-driver` offers as a springboard — lidar/SLAM, FS-iA6B, ESP32 dual-core, Pi fleet |
| [`13_operator_charter_notes_2026-08-18.md`](13_operator_charter_notes_2026-08-18.md) | **Verbatim, do not edit.** The operator's own charter note — the source of rulings R1, R4, R6, R7 |
| [`18_provenance_template.md`](18_provenance_template.md) | `PROVENANCE.md` template + verified git facts for all four sources + per-repo "what's worth going back for" |
| [`19_operator_execution_handoff.md`](19_operator_execution_handoff.md) | **What the operator actually runs.** The 3 repos to create, exact commands, the gates, and the two things that would destroy data. Replaces `09`'s create-and-scaffold phase |

## Mining extracts

Produced by the read-only mining pass over the legacy repos, each adversarially verified.

| Doc | Covers |
|---|---|
| [`14_swarmloc_uwb_extract.md`](14_swarmloc_uwb_extract.md) | SwarmLoc's UWB/TWR research spine, distilled → seeds `position_denial_research/theory/uwb_ranging/` |
| [`15_swarmloc_capability_inventory.md`](15_swarmloc_capability_inventory.md) | SwarmLoc's non-theory folders — what was proven, what it took, what to carry forward |
| [`16_gravityprobe_capability_inventory.md`](16_gravityprobe_capability_inventory.md) | GravityProbe's 22 files — every folder read, folder-name-vs-reality mismatches called out |
| [`17_literature_routing.md`](17_literature_routing.md) | `floppi/literature/` routing + the `docs/literature/literature/` dedupe hazard, settled by checksum |

## Original planning corpus (2026-07-09, waves 1-4)

Still authoritative except where noted.

| Doc | Status |
|---|---|
| [`00_reorg_master_plan.md`](00_reorg_master_plan.md) | Taxonomy + phases stand. **§3 source→target map SUPERSEDED** |
| [`01_target_repos_v2.md`](01_target_repos_v2.md) | Per-repo cards. Repo *list* superseded by `11_routing_v2` §1; the per-repo purpose/README-seed detail still useful |
| [`02_swarm_protocol_seed.md`](02_swarm_protocol_seed.md) | Live. Seeds `swarm_communication_protocol` |
| [`03_folder_recon_findings.md`](03_folder_recon_findings.md) | Recon stands. **§14 routing table SUPERSEDED** |
| [`04_mini_project_setup_guide.md`](04_mini_project_setup_guide.md) | Live. The mini-project pattern for multi-project repos |
| [`05_research_repo_scope.md`](05_research_repo_scope.md) | Live. Internal layout of the `research` repo |
| [`06_readme_template.md`](06_readme_template.md) | Live. Cross-repo README template, 2 variants |
| [`07_verbatim_content_registry.md`](07_verbatim_content_registry.md) | Live. Files copied verbatim + why |
| [`08_gravityprobe_recon.md`](08_gravityprobe_recon.md) | Recon stands; routing superseded by R1/R6 |
| [`09_gitlab_operations_checklist.md`](09_gitlab_operations_checklist.md) | **SUPERSEDED IN PART.** Phase discipline + CRUD-is-human-only still govern; repo counts and create-list are stale. Use `19_operator_execution_handoff.md` for the create-and-scaffold phase |
| [`chat-export-chatgpt-2026-07-09.md`](chat-export-chatgpt-2026-07-09.md) | Source material. Contains `ecoder_demo.c` by Emmett M. Gilbert — **the only copy, no git provenance** |

---

## Standing rules

- **No git mutations, no GitLab/repo CRUD by any session.** Operator-only, both forges.
- **The three legacy repos** (`~/SwarmLoc`, `~/GravityProbe`, `~/WayfindR-driver`) are **directional**:
  writing INTO them is forbidden; taking findings and useful items OUT is allowed and expected —
  selectively, with attribution, never a wholesale `cp -r`.
- **No history transfer** (N-10). New repos start at commit 1 + a `PROVENANCE.md`.
- **Verify on the real path.** A script printing "done" is not proof the edit landed — grep the anchor back
  out of the file. This corpus has already lost one edit to a silent no-op.
