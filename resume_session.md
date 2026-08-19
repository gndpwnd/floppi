# RESUME SESSION — floppi, 2026-08-19

**Read this first, then `docs/todo.md` (SSOT), then `temp_reorg/INDEX.md`.**

---

## 1. What this work is

floppi is a **migration SOURCE ARCHIVE**. Its content is being distributed into repos under the GitLab
group **`lowprofiledronegurus`**. This session's arc: inventoried every drone project, settled the repo
list and every scope decision, and produced a runnable operator handoff. **Zero content has been migrated.
No repo has been created. No git write has been run by any agent.**

## 2. First 5 minutes of the next session

```bash
# 1. Re-arm the coordination inbox — it does NOT survive a session end
bash /home/devel/claude_coordination/coord.sh watch floppi
#    then register the printed one-liner with the Monitor tool, persistent:true

# 2. Verify it is actually live (registration != running)
pgrep -af "claude_coordination/inbox/floppi/new"

# 3. Drain anything queued
bash /home/devel/claude_coordination/coord.sh list floppi
```
**Gotcha:** never pipe `coord.sh read` into `head` — SIGPIPE kills it *before* the `new/ → cur/` move, so
processed mail still shows unread. Read the file directly.

## 3. The four peer sessions — this is a multi-session effort

| Node | Owns | Status at handoff |
|---|---|---|
| `lowprofiledronegurus` | Target side: repo skeletons, README standardisation, per-repo bootstrap, **and ResearchHub config for all lpdg repos** (operator assignment) | Active, sharp, caught two of my errors |
| `engineer360` | The **method**: computational/parametric sizing, aero, structures, CAD, engineering corpora | Active. Was the "dead node" until pinged — its watch wasn't armed |
| `hiverf` | RF/EW: non-cooperative emitter localisation | Active. Fixed a real `coord.sh` bug that silently hid mail |
| `researchhub` | Research sync, `repositories.json`, the distilled-only contract | **Checkpointed out.** Items logged as OWED, resuming next session |

**No fixed front door** (ruled): any session may ask the operator. The duty is **de-duplication** — check
`temp_reorg/10_decision_queue.md` and the bus before asking, and mirror every ruling into that file *and*
onto the bus the same turn.

## 4. The ledger — 9 active repos + 1 deferred (was 13)

**Exist on GitLab** (boilerplate only): `auto_orientation_research` · `drone_frame_modeling` · `fc_tool` ·
`flight_controller` · `position_denial_research` · `sensor_interactions`

**To create:** `swarm_communication_protocol` · `swarm_api` · `research`

**DROPPED — do not create:** `darpa_lift_2026` · `communication_hardware` · `networking_pocs`
**Deferred:** `cybersecurity_demos`

## 5. Operator rulings in force

| Ref | Ruling |
|---|---|
| **R1/R6** | `~/SwarmLoc`, `~/GravityProbe`, `~/WayfindR-driver` are **LEGACY/SPRINGBOARD**, and the rule is **directional**: writing INTO them is forbidden (no delete, modify, commit, `.gitignore` edit); taking findings and useful items **OUT** is allowed and expected — selectively, attributed, never a wholesale `cp -r`. **Mined, not migrated.** |
| **R2** | DARPA Lift dropped. The durable capability is *requirements-driven design sizing*; DARPA is one worked case study in `research/`. |
| **R3** | Org purpose: **drone / UAV / UAS / cUAS prototyping** — working airframes first, then swarms/flocks. EW/SIGINT is a sibling (`hiverf`), integrated later. |
| **R5** | ResearchHub syncs **distilled research only**. (RH confirmed current sync already honours this; on-disk bloat is legacy residue.) |
| **R7** | `sensor_interactions` is the **SUCCESSOR** to GravityProbe sensors + WayfindR lidar. It inherits; it is not thin. |
| **R8** | **engineer360 owns the METHOD, the fleet owns BUILDS.** *A number, a curve, a geometry, or a piece of knowledge → engineer360. A thing that flies, or talks to a thing that flies → the fleet.* |
| **N-9** | `drone_frame_modeling` **survives, re-scoped** to CAD/mesh/BOM/print-config for airframes actually built. Empty by design until the first build. |
| **N-10** | **No git history transfer.** New repos start at commit 1 + a `PROVENANCE.md`. |
| **Q-N** | `auto_orientation_research` keeps its name. |
| — | **WiFi security is PARKED** by the operator. Recorded, gating nothing. |

**ACTIVE projects:** `~/floppi`, `~/lowprofiledronegurus`, `~/engineer360`, `~/hiverf`.

## 6. ⚠ Three things that would destroy data

**6.1 NEVER `filter-repo` / history-purge `~/floppi`.** ResearchHub's `repo_untrack.py` is *index-only*
(staged `git rm --cached`, never committed), so the untracked research corpus is **still at `HEAD`** and
`engineer360` was given that ref to pull 33 research topics from. A purge deletes one of only two copies.
floppi is not a bloat case anyway (`size-pack: 146.64 MiB`, `garbage: 0`). Same for all three legacy repos.
**All four peer sessions have hardened their tooling against this.**

**6.2 The literature dedupe is INVERTED from how it is commonly described.** `~/floppi/literature/` and
`~/floppi/docs/literature/literature/` share **zero** byte-identical files. Acting on the common
description could delete `~/floppi/literature/` — 59.6 MiB, 15 unique files, 4 existing nowhere else.
Only these four are safe to delete, and nothing else:
`docs/literature/literature/{findings/README.md, findings/recommended-textbooks.md, roadmap.md, scope.md}`.
**`docs/literature/findings/serial-rich-text-formatting.md` is unique — a wrong-way dedupe eats it.**
Full analysis: `temp_reorg/17_literature_routing.md` §1.

**6.3 20 extension-less ELF binaries** in `auto_orientation` and `flight_controller` that no glob catches.
Post-copy: `find <target> -type f -exec file {} + | grep ELF | cut -d: -f1 | xargs -r rm`.

## 7. What is DONE

- `CLAUDE.md` (repo had none), `docs/todo.md` (SSOT — repo had none), memory rewritten to standard
- `temp_reorg/` corpus: **22 documents** with `INDEX.md` marking live vs superseded. `00` §3, `01`, `03`
  §14 and `09` carry supersession headers — **two sessions have already filed findings against superseded
  sections**, so always check `11_routing_v2` first
- `11_routing_v2_2026-08-18.md` — routing authority + 7 hazard gates
- `10_decision_queue.md` — every decision with its dated ruling. **Jointly owned; peers edit it directly**
- `19_operator_execution_handoff.md` — what the operator actually runs
- `18_provenance_template.md` — verified git facts for all four sources + per-repo "what's worth going back for"
- `12_wayfindr_reference_inventory.md` — the WayfindR springboard (lidar/SLAM, FS-iA6B, ESP32 dual-core)
- `14`–`17` — mining extracts from the legacy repos
- Credential leak in `08_gravityprobe_recon.md` **redacted** (real SSID + password were in cleartext ×3)
- All corpus cross-references repaired and verified

## 8. What is NEXT

1. **⚠ FINISH THE EXTRACT CORRECTIONS.** Adversarial verifiers returned **`NEEDS_FIXES` on all four**
   extracts (`14`–`17`). A correction workflow was running when this session ended and **did not finish**
   — `16_gravityprobe_capability_inventory.md` had not been rewritten yet. **Treat `14`–`17` as DRAFTS.**
   Verdict files: `/tmp/claude-1000/-home-devel-floppi/58b85709-.../scratchpad/verdict_*.md` (may be
   cleared — regenerate by re-running verification if so). Known real errors: a merged measurement in
   `14` §10, a wrong library date in `14` §5.1, cooperative-TDOA over-ceded to hiverf, institutional
   SSIDs/MAC/campus-IP reproduced in `15`.
2. **Operator creates the 3 repos** → `19_operator_execution_handoff.md`.
3. **Per-repo bootstrap** — `~/llm-project-bootstrap/PROMPTS.md` § "Bootstrap Existing". This was
   deferred until the inventory was settled. **It now is.**
4. **Content copy** per `11_routing_v2` §2, gates applied.
5. **ResearchHub repoint** — heads-up before moving `floppi/flight_controller`, they apply the staged
   `repositories.json` edit, confirm the path exists.

## 9. Only open decision: N-7

Do `engineer360` and `hiverf` **move** into the `lowprofiledronegurus` group, or **stay** at
`gitlab.com/gndpwnd/` and get cross-linked? Both sessions recommend staying independent and being cited
(`hiverf` has a shipped non-drone consumer). **`hiverf` is escalating it — do not double-ask.**

## 10. Hard-won lessons — do not relearn these

- **A script printing "done" is not proof.** A `str.replace()` that matched nothing still printed success,
  and I reported three hazard gates as written when they were not. `lowprofiledronegurus` caught it by
  checking four ways. **Every edit must abort on a failed match, and you must grep the anchor back out.**
- **A peer's self-report is not the trust anchor** — nor is your own. Verify on the real path.
- **When a tool does half a job, look in its own directory for the other half.** Three sessions
  independently started rebuilding a history-purge tool that ResearchHub had owned since 2026-07-11,
  sitting next to the `repo_untrack.py` we had all read.
- **`git ls-tree` C-quotes non-ASCII paths; `git rev-list --objects` does not.** A `\.pdf$` anchor on
  `ls-tree` silently skips them and reports a false pass. Use `\.pdf"?$` everywhere.
- **`du -sh .git` cannot distinguish real history from wreckage.** Run `git count-objects -vH`.
