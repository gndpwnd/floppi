# Verbatim content registry

**Status:** 2026-07-09 working draft.
**Purpose:** register specific files that must be COPIED VERBATIM (not summarized, not adapted) into target repos at migration time. Preserves human/AI-authored artifacts as historical record.
**Rule:** contents of registered files are NEVER paraphrased when the corresponding target repo is populated. Migration script does `cp` (or `rsync -a`) into the specified `docs/archive/` location, with the original file mtime preserved if possible.

---

## Registered files

### 1. ChatGPT design critique of the swarm communication protocol

- **Source:** `/home/devel/floppi/temp_reorg/chat-export-chatgpt-2026-07-09.md`
- **Origin:** ChatGPT conversation exported 2026-07-09. Substantive design critique of the `tmp.md` swarm-communications brainstorm. Includes:
  - Analysis of an early C encoder prototype (`ecoder_demo.c`) that converts readable command strings into hex-encoded payloads.
  - Research-backed recommendations pulling from MAVLink 2, WiSwarm, TARRAQ, 2026 FANET routing work.
  - Design shifts recommended: packed binary (not readable hex) as the payload; binary-tree as delegation-and-accountability overlay only (not sole routing truth); one protocol family with multiple traffic classes and possibly multiple bearers (UWB + WiFi + local-store-and-forward); deadline/freshness semantics per message class (sequence number + deadline or max-age, not always both send+expiry timestamps); mission-transfer sub-protocol with mission-ID + chunk sequencing + missing-item re-request + completion ACK; reserve space and semantics for signed packets now even without shipping crypto in v1 (MAVLink 2 pattern).
- **Target — primary:** `swarm_communication_protocol/docs/archive/chat-export-chatgpt-2026-07-09.md`
- **Target — secondary (candidate):** if `research/mini_projects/swarm_protocol_design/` mini-project materializes, ALSO here: `research/mini_projects/swarm_protocol_design/docs/archive/chat-export-chatgpt-2026-07-09.md`
- **Migration directive:** copy verbatim. Do NOT rewrite as a design decision doc. The synthesis into design decisions belongs in the seed doc `02_swarm_protocol_seed.md`'s "open design questions" section (as a cross-reference to this archive file), not in the archive copy itself.

---

## Additional files to add here as they're identified

Reserved for future entries. Candidate additions:
- The `tmp.md` swarm-comms brainstorm itself — should the raw brainstorm ALSO be preserved verbatim in `swarm_communication_protocol/docs/archive/`? (Currently: extracted structure lives in `02_swarm_protocol_seed.md`; raw brainstorm may deserve archive preservation for provenance.) **Open — user decision.**
- The `ecoder_demo.c` snippet from the ChatGPT export (embedded in the export) — arguably belongs in `swarm_communication_protocol/prototypes/ecoder_demo.c` as an early prototype artifact. Its author line (`Emmett M. Gilbert @ 9:02 7/7/2026`) suggests it's already a distinct authored artifact.

---

## Cross-references

- `00_reorg_master_plan.md` §7 (Follow-on docs list) — this file should be added there
- `02_swarm_protocol_seed.md` §11 (Open design questions) — cross-reference the ChatGPT critique as evidence-backed alternatives to consider
- `04_mini_project_setup_guide.md` — the `docs/archive/` pattern per mini-project is where these files land
- `05_research_repo_scope.md` — if the design work also mini-projects into `research/`, this file has a secondary home there

---

## Governance

- New entries added by the reorg orchestrator OR by the user directly
- Each entry MUST specify: source path, target path (primary + optional secondary), migration directive (verbatim vs adapt), origin/context (why it matters)
- Migration script (Phase 5) reads this registry and preserves listed files as literal copies
- If a source file changes between registration and migration, the newer file wins — verbatim means "preserve current content," not "freeze at registration time"
