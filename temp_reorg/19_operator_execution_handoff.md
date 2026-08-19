# 19 — Operator execution handoff (current as of 2026-08-19)

**What this is.** Everything the operator needs to actually execute, in order, with exact commands.
It **replaces** `09_gitlab_operations_checklist.md` for the create-and-scaffold phase — `09`'s repo counts
and create-list are stale (it was written for 13 repos; the ledger is now 9). `09`'s phase discipline and
CRUD-is-human-only rule still stand.

**Everything here is human-only.** No agent may run any of it. Agents may only read.

---

## 1. Where things actually stand

**Verified against the live GitLab remote on 2026-08-19** (read-only `git ls-remote`, using
`~/.ssh/id_git`):

| Repo | GitLab state | Action |
|---|---|---|
| `auto_orientation_research` | EXISTS (`main`, boilerplate) | populate later |
| `drone_frame_modeling` | EXISTS (`main`, boilerplate) | leave empty by design (N-9) |
| `fc_tool` | EXISTS (`main`, boilerplate) | populate later |
| `flight_controller` | EXISTS (`main`, boilerplate) | populate later |
| `position_denial_research` | EXISTS (`main`, boilerplate) | populate later |
| `sensor_interactions` | EXISTS (`main`, boilerplate) | populate later |
| **`swarm_communication_protocol`** | **ABSENT** | **CREATE** |
| **`swarm_api`** | **ABSENT** | **CREATE** |
| **`research`** | **ABSENT** | **CREATE** |

**Do NOT create** — dropped by ruling, not oversight:
`darpa_lift_2026` (R2, challenge dropped) · `communication_hardware` (R1, its entire source was SwarmLoc
which is no longer migrated) · `networking_pocs` (R1, same) · `cybersecurity_demos` (deferred until a real
cyber POC exists).

**Total: 9 active repos + 1 deferred.** Nothing is unsettled. One decision remains open — **N-7**: do
`engineer360` and `hiverf` move into the group, or stay at `gitlab.com/gndpwnd/` and get cross-linked?
Both sessions recommend staying independent and being cited. **N-7 does not block anything below.**

---

## 2. Step 1 — create the three repos (GitLab web UI)

For each of `swarm_communication_protocol`, `swarm_api`, `research`:

1. gitlab.com → group **`lowprofiledronegurus`** → **New project → Create blank project**
2. Project name exactly as above (slug will match)
3. Visibility: **match the existing six** — check one of them first; do not guess
4. **Tick "Initialize repository with a README"** so the repo has a `main` branch, matching the other six
5. Leave everything else default

**Verify all three landed, read-only, before going further:**

```bash
export GIT_SSH_COMMAND="ssh -i ~/.ssh/id_git -o IdentitiesOnly=yes"
for r in swarm_communication_protocol swarm_api research; do
  printf '%-32s ' "$r"
  git ls-remote --heads "git@gitlab.com:lowprofiledronegurus/$r.git" 2>&1 | grep -q refs/heads \
    && echo OK || echo "MISSING"
done
```

All three must print `OK`. If one prints `MISSING`, it was not created — fix that before continuing.

> **SSH note:** `~/.ssh/config` gained `Host gitlab.com` and `Host github.com` blocks pointing at
> `~/.ssh/id_git` (backup: `~/.ssh/config.bak-2026-08-18`), so plain `ssh -T git@gitlab.com` should
> already work without the `GIT_SSH_COMMAND` prefix. It is included above only as a belt-and-braces.

## 3. Step 2 — clone them beside the existing six

```bash
cd ~/lowprofiledronegurus
for r in swarm_communication_protocol swarm_api research; do
  git clone "git@gitlab.com:lowprofiledronegurus/$r.git"
done
ls -1                       # expect 9 directories
```

---

## 4. Step 3 — the gates, before any content is copied in

Full text: `11_routing_v2_2026-08-18.md` §5. The three that bite silently:

**Gate 3 — compiled binaries no glob catches.** `auto_orientation` and `flight_controller` carry **20**
ELF test binaries outside `.pio` with **no file extension**, sitting beside their sources
(`tests/test_quaternion` next to `test_quaternion.cpp`, plus `test_plant_identifier`,
`test_noise_floor_estimator`, `test_balance_app_bootstrap`, `test_position_gain_derivation`,
`test_balance_app_soft_cutoff`, +14). `--exclude=*.elf` misses **all** of them. Run **after** any copy:

```bash
find <target-repo> -type f -exec file {} + | grep ELF | cut -d: -f1 | xargs -r rm
```

**Gate 4 — committing in floppi triggers ResearchHub.** `~/floppi/.git/hooks/post-commit` and
`post-merge` background `python3 /home/devel/researchhub/scripts/backup/cli.py repo`. On a
memory-constrained host that is not a free action. Check headroom first:

```bash
free -h        # want several GB available and swap not near-full
```

**Gate 5 — no git history travels (N-10).** New repos start at commit 1. Each gets a `PROVENANCE.md`;
template and per-repo "what's worth going back for" lists are in `18_provenance_template.md`.

---

## 5. ⚠ Step 4 — the two things that would destroy data

### 5.1 NEVER `filter-repo` `~/floppi`

`repo_untrack.py` is **index-only** — it stages `git rm --cached` and adds a `.gitignore` fence, but never
commits. So floppi's "deleted" research corpus **is still at plain `HEAD`**:
`git show HEAD:drone_3d_model/generated/<file>` resolves today. `engineer360` has been given `HEAD`
(`dd39f23`) as their recovery ref for pulling 33 research topics.

A history purge would delete one of only **two** copies and silently break that pointer. floppi needs no
purge anyway: `size-pack: 146.64 MiB`, `garbage: 0`. The same exclusion applies to `~/SwarmLoc`,
`~/GravityProbe`, `~/WayfindR-driver` — all read-only archives under R6. All four peer sessions have
hardened their tooling against this.

### 5.2 The literature dedupe — delete these four paths and nothing else

The widely-repeated description of this dedupe is **inverted**. `~/floppi/literature/` and
`~/floppi/docs/literature/literature/` share **zero** byte-identical files — verified on md5 and sha256.
Acting on the common description could delete `~/floppi/literature/`: **59.6 MiB, 15 unique files, 4
existing nowhere else on this machine.**

**Safe to delete — exactly these, verified byte-identical to twins one level up:**

```
~/floppi/docs/literature/literature/findings/README.md
~/floppi/docs/literature/literature/findings/recommended-textbooks.md
~/floppi/docs/literature/literature/roadmap.md
~/floppi/docs/literature/literature/scope.md
```

**Must be kept:** `~/floppi/docs/literature/findings/serial-rich-text-formatting.md` — unique, no copy
anywhere, and it is what a wrong-way dedupe eats. Do **not** point a generic dedupe tool at
`docs/literature/`: keep-last and depth-preferring rules pick the wrong copy. Full analysis:
`17_literature_routing.md` §1.

---

## 6. Optional — floppi git tidy (low priority, after any content copy)

Stale `filter-branch` backup ref plus 617 loose objects. Litter from a **completed, pushed** operation —
floppi is not mid-rewrite (`HEAD` == `origin/main` == `dd39f23`). Hold until content copying is done so
nothing shifts mid-migration.

```bash
git -C ~/floppi update-ref -d refs/original/refs/remotes/origin/main
git -C ~/floppi gc --prune=now
git -C ~/floppi count-objects -vH    # verify
```

---

## 7. What happens after the repos exist

Content migration is **not** scheduled and should not start on the same pass as creation.

1. **Per-repo bootstrap** — `~/llm-project-bootstrap/PROMPTS.md` § "Bootstrap Existing", per repo. This is
   the step deferred until the project/repo/scope inventory was settled. It now is.
2. **Content copy**, per `11_routing_v2` §2, gates applied.
3. **ResearchHub configuration — owned by the `lowprofiledronegurus` session, per operator assignment
   2026-08-19** (*"this should get addressed by them not you"*). They register and configure every
   `lowprofiledronegurus` repo. None is registered today, which is correct — repos get registered **as
   content lands**, not before. The contract they were handed: current sync is already distilled-only
   fleet-wide; point **both** `folders.generated` and `distill.output` at one dedicated fenced directory
   (`engineer360`'s `docs/research/distilled` is the reference shape); **fence that path in `.gitignore`
   at registration time**; and ResearchHub repoints *before* any folder is moved, never after — reverse it
   and the distiller recreates the old path within one 5-minute cycle.
   **The one exception that stays with `floppi`:** `floppi_flight_controller` **repoints in place**
   (`/home/devel/floppi/flight_controller` → `/home/devel/lowprofiledronegurus/flight_controller`, entry
   and `workspace_name` preserved) when that content moves — heads-up → they repoint → confirm the path
   exists. It must **not** also be registered fresh, or one project ends up with two entries.
4. **`PROVENANCE.md`** in each repo, from `18_provenance_template.md`.

## 8. Where to read further

`INDEX.md` — what is live vs superseded · `11_routing_v2_2026-08-18.md` — routing authority + gates ·
`10_decision_queue.md` — every decision with its ruling · `12_wayfindr_reference_inventory.md` — the
WayfindR springboard · `14`–`17` — the mining extracts · `18_provenance_template.md` — provenance.
