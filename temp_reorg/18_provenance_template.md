# 18 — `PROVENANCE.md` template + per-repo "what's worth going back for"

**Why this exists.** N-10 ruled 2026-08-19: **no git history transfer.** Operator, verbatim:

> *"why would we take the fresh import? i just want to have documentation perhaps of what we need about
> the git history, i really don't want to transfer git history at all that is way too much of a mess and
> burdensome, its not like we are deleting ~/floppi/ so we can always reference it if we are missing
> something"*

Every new repo starts at commit 1. Each carries one `PROVENANCE.md` at its root. **This is a POINTER, not
an archive** — the source repos are never deleted, so the job is to tell a future engineer *where to look
and what is worth looking for*, not to reproduce anything.

---

## 1. Verified provenance facts for the four source repos

Measured 2026-08-19 with `git rev-list`, `git log`, `git shortlog -sne`. Copy the relevant row into each
new repo's `PROVENANCE.md` rather than re-deriving it.

| Source | Remote | Branch | Commits | Date span | First → HEAD |
|---|---|---|---|---|---|
| `~/floppi` | `github.com:gndpwnd/floppi` | `main` | 126 | 2025-06-08 → 2026-07-09 | `139b0f44` → `dd39f23` |
| `~/SwarmLoc` | `github.com:msmcs-robotics/SwarmLoc` | `main` | 26 | 2025-05-23 → 2026-05-08 | `32563aae` → `6a6ee41` |
| `~/GravityProbe` | `github.com:msmcs-robotics/GravityProbe` | **`data-analysis`** | 6 | 2025-01-03 → 2025-01-29 | `c8f4965d` → `1e0c086` |
| `~/WayfindR-driver` | `github.com:msmcs-robotics/WayfindR-driver` | `main` | 93 | 2025-05-23 → 2026-04-09 | `6b5dd470` → `18bdd07` |

⚠ **GravityProbe's content is on `data-analysis`, not `main`.** `main` holds only a README. Anyone cloning
the default branch to "go back and look" will find nothing and conclude the work never existed.

### Contributors (git authors, by commit count)

| Identity | floppi | SwarmLoc | GravityProbe | WayfindR |
|---|---:|---:|---:|---:|
| `kaleldev <dev@kalel.com>` | 74 | — | — | 65 |
| `kaleldev <kaleldev@gmail.com>` | 46 | 15 | — | 4 |
| `gndpwnd <88689368+gndpwnd@users.noreply.github.com>` | 4 | 3 | 2 | 8 |
| `kaleledev <dev@kalel.com>` | 2 | — | — | 11 |
| `msc_intra <109829955+mscrobotics@users.noreply.github.com>` | — | 7 | 3 | 2 |
| `gndpwnd <gnelsondev@gmail.com>` | — | 1 | 1 | — |
| `kalel_dev <dev@kalel.com>` | — | — | — | 3 |

**Correction to an earlier claim:** `Emmett M. Gilbert` is **not a git author in any of the four repos** —
verified, zero matches across all branches. He is the author of **`ecoder_demo.c`**, attributed in an
in-file comment (*"Written By Emmett M. Gilbert @ 9:02 7/7/2026"*), which survives only inside
`temp_reorg/chat-export-chatgpt-2026-07-09.md`. That is a **third-party-authored source file with no
git provenance at all** — the one piece of attribution a history-preserving import would *not* have saved
anyway, and the one most at risk of being lost. It seeds `swarm_communication_protocol` (a command-string →
hex-token encoder for mothership→drone messaging) and **must carry its attribution comment verbatim.**

Several identities are the same person under different git configs (`kaleldev` / `kaleledev` /
`kalel_dev`, two emails). Do not "clean this up" in `PROVENANCE.md` — record what git actually says.

---

## 2. The template

Drop this at the root of each new repo, filled in. Keep it under one screen.

```markdown
# Provenance

This repository starts at commit 1. Its content was migrated from an older repository whose git history
was deliberately **not** imported (decision: no history transfer, 2026-08-19 — carrying it was judged
more burden than value, and the sources are never deleted).

**The source repositories still exist and are kept read-only. If something here looks unmotivated,
go and look.**

## Source

| | |
|---|---|
| Source repo | `<name>` — `<remote url>` |
| Branch | `<branch>` |
| Local path | `<~/path>` |
| Commit range | `<first>` … `<head>` |
| Date span | `<YYYY-MM-DD>` … `<YYYY-MM-DD>` |
| Migrated | `<YYYY-MM-DD>` |

## Contributors to the migrated work

<identity + email, one per line, as git reports them — including duplicate configs of the same person>

<Any file authored by someone who is not a git author — name them and name the file.>

## What is worth going back for

<The load-bearing section. Not a file listing — a short list of things a future engineer would
regret not knowing existed, each with the path to find it under the source repo.>

## What was deliberately left behind

<Build artifacts, generated/ output, debug-variant sprawl, superseded docs — and why. This stops
someone "restoring" what was dropped on purpose.>
```

---

## 3. Per-repo "what is worth going back for"

The section that carries real value. Drafted from the floppi tree; **each destination repo should prune
and extend its own list** — this is a starting point, not a finished answer.

### `flight_controller`
- **The consolidated bench-validation runbook** — `flight_controller/docs/findings/bench_validation_runbook_2026-05-27.md`. 17 hardware-gated items in safe-first order (Phase 1 smoke → Phase 4.5 ESC endpoints → Phase 5 failsafe → Phase 6 tethered hover). Nothing here has ever been validated end-to-end on hardware; this is the gate.
- **The bootloader findings cluster** — `floppi/docs/findings/` (7 files: DTR/RTS analysis, recovery guide + summary, quick reference, `README_BOOTLOADER.md`, `BOOTLOADER_FILES.txt`) plus `tools/recover_bootloader.sh`. Hard-won recovery knowledge for bricked Teensy boards.
- **`docs/build_matrix.md`** — what was *last actually compiled*, with no carry-forward claims. The honest record of what builds.
- **`docs/FEATURE_COMPARISON.md`** and the dRehmFlight divergence history — why this firmware differs from upstream.
- **Session records** — `flight_controller/docs/archive/session_records/`, one per multi-agent session including aborted ones.

### `auto_orientation_research`
- **The bench-validation runbook** — `auto_orientation/docs/findings/bench_validation_runbook_2026-05-27.md`, 24 items. Includes the fact that the Mega bot has **never balanced successfully**; last attempt (2026-05-18) was twitch-and-fall in ~1 s.
- **The two-tier platform-bifurcation reasoning (2026-05-19)** — why the universal/adaptive stack is Mega-only and Uno gets a manual operator-guided tier. The split is **memory-driven, not sensor-driven**, and the reasoning is easy to lose and expensive to re-derive.
- **`docs/findings/bno_cross_project_2026-05-20.md`** — the BNO055/BNO085 cross-project integration roadmap.
- **`docs/findings/calibration-test-results-2026-02-12.md`** — real measured calibration results.
- **The CRC-8-CCITT calibration HAL + photo-backup printer design** — every persisted value is printable and paste-able into `balance_constants.h` to survive an EEPROM wipe. Vendored into `flight_controller` as a sibling lib.

### `position_denial_research`
- **`~/SwarmLoc/findings/` + `~/SwarmLoc/docs/`** (176 KB) — the UWB research spine: TWR accuracy optimization, the DW1000 library bug + fix, library alternatives evaluated, calibration research, swarm ranging architecture. Distilled into this repo's theory tree; the raw source stays in SwarmLoc.
- **`~/SwarmLoc/DWS1000_UWB/tests/`** — the ~60 single-iteration debug variants. Deliberately left behind, but *the naming is the record of the debug journey*; go back if you hit a DW1000 problem that looks familiar.

### `sensor_interactions`
- **`~/WayfindR-driver/findings/lidar-data-workflow.md`** — RPLidar C1M1 record → replay → map → tune on ROS2 Humble. The record-once/replay-many practice is the single biggest time-saver in that repo.
- **`~/WayfindR-driver/ambot-slam/`** — the production ROS2 SLAM implementation (SLAM Toolbox + Nav2 + AMCL) that supersedes the four `ros2_*_attempt/` folders. Fork from here, read the attempts for the failure record.
- **`~/WayfindR-driver/old_stuff/rplidar_setup/0.1`→`0.4`** — the bare-Python sensor bring-up ladder.
- **`~/GravityProbe/`** (on `data-analysis`) — ESP32 WPA2-Enterprise / WPA3-EAP auth, SD/HW125, I²C OLED, MPU6050 sketches. 22 files total.

### `swarm_communication_protocol`
- **`floppi/tmp.md`** (43 KB) — the operator's own raw design reasoning for the hex-string protocol. The distilled version is `temp_reorg/02_swarm_protocol_seed.md`; keep both.
- **`ecoder_demo.c` by Emmett M. Gilbert** — inside `temp_reorg/chat-export-chatgpt-2026-07-09.md`. **No git provenance anywhere.** Carry the attribution comment verbatim.
- **The ChatGPT design critique** — same chat export, registered in `07_verbatim_content_registry.md`.

### `research`
- **`floppi/literature/`** (60 MB of PDFs) — routing settled in `17_literature_routing.md`.
- **`floppi/darpa_lift_2026/`** — the requirements-driven-sizing case study. The challenge itself is dropped; the *requirements → airframe* capability is the durable part.
- **`floppi/tools/researchhub_client.py`** (2943 lines) — single-file zero-dep API client.

### `fc_tool`, `swarm_api`, `drone_frame_modeling`
- `fc_tool` — its pytest suite tests **host-side parsing reference implementations** for a Tauri/Rust app. Unusual but deliberate; don't "fix" it.
- `swarm_api` — `deploy/` carries full systemd wrapping (`menu.sh`, `service.sh`), heavier than "simple API" suggests. Also read `floppi/docs/flight-computer/` — it reads like this project's origin story.
- `drone_frame_modeling` — starts genuinely empty by design (N-9). Its provenance file should say so explicitly, and point at `engineer360/docs/domains/uav-airframes/` for the structural/aero method, per R8.

---

## 4. What was deliberately left behind (state this in every `PROVENANCE.md`)

So nobody "restores" what was dropped on purpose:

- **Git history itself** — N-10.
- **`generated/` ResearchHub output and `sources/` PDFs** — regenerable derivative output; re-ingesting it is the corpus-poisoning loop. The `repo_untrack` fence carries forward.
- **20 extension-less compiled ELF test binaries** — see `11_routing_v2` §5 gate 3.
- **`~/SwarmLoc/DWS1000_UWB/tests/`'s ~60 debug variants** — superseded by `tests_new/`.
- **`darpa_lift_2026/initial_research.md` + `avionics_controls.md`** (~77 KB) — raw LLM chat transcript with inline `images.openai.com` URLs. Generated output, not authored research.
- **Superseded docs** — `floppi/docs/ARCHITECTURE.md` (a retired "Level -1" hub), `MIGRATION_SUMMARY.md`, `ORGANIZATION_SUMMARY.md`, `SCOPE_REFACTOR_COMPLETE.md`, and `drone_3d_model/`'s `ROADMAP.md`/`SCOPE.md` (which contradict each other).
