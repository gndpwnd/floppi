# 17 — Literature routing + the `docs/literature/literature/` dedupe hazard, resolved

## Provenance

**Written:** 2026-08-19. **Author:** subagent session, `~/floppi` working tree, branch `main`.

**What was read, in full:**

- `/home/devel/floppi/temp_reorg/11_routing_v2_2026-08-18.md` (governing routing doc, all 352 lines —
  VERIFIED `wc -l`; an earlier draft of this doc said 277, which was wrong)
- `/home/devel/floppi/temp_reorg/03_folder_recon_findings.md` §8–§11 (the prior recon this doc revises)
- `/home/devel/floppi/temp_reorg/05_research_repo_scope.md` (grepped for `literature`; §3.2, §4, §7 read)
- `/home/devel/floppi/docs/literature/scope.md`, `roadmap.md`, `findings/README.md` — read end to end
- `/home/devel/floppi/docs/literature/findings/recommended-textbooks.md` — heading structure + line count only
- `/home/devel/floppi/docs/literature/findings/serial-rich-text-formatting.md` — first 50 lines + full heading structure
- `/home/devel/floppi/literature/resources.md` (whole file), `drehmflight_README.md` (first 40 lines),
  `init_about_refactored_drehmflight.md` (first 60 lines), `drehmflight_transcripts.md` (first 40 lines)
- All 10 PDFs: `pdfinfo` metadata on every one; `pdftotext` first-page (and where needed 2nd/3rd-page,
  full-document grep, `pdfimages -list`) extraction on every one.

**Checksums:** `md5sum` on all 24 files in both trees; `sha256sum` additionally on all 9 files in
`docs/literature/`. Raw values reproduced in §1.

**Git reads only.** `git ls-files`, `git log`, `git show --stat`. No branch changes, no writes.
`~/SwarmLoc`, `~/GravityProbe`, `~/WayfindR-driver` were read only (a single `find -iname '*.pdf'` each).
**Those searches were not null — report their results, 21 PDFs total (VERIFIED):** `~/SwarmLoc` 18, of which
16 are Gerber/etch plot sheets under `DWS1000_UWB/lib/DW1000/adapterBoard/` (PCB fabrication artwork, not
literature); the two that are documents are `DWS1000_UWB/lib/DW1000/extras/doc/DW1000_Arduino_API_doc.pdf`
and `.../adapterBoard/dwm1000 arduino standalone/etch_top_sided_miror.pdf`. `~/WayfindR-driver` 3:
`docs/Indoor Mapping and Navigation System Overview.pdf`, `adeept_car_stuff/511343Tutorial.pdf`,
`adeept_car_stuff/Introduction Robot HAT.pdf`. `~/GravityProbe` 0. **None of these 21 is routed by this
document** — all three trees are legacy/read-only under R6 and out of scope here; they are listed so a
rebuilder does not re-run the search expecting a null result. The WayfindR PDFs are `12_wayfindr_reference_inventory.md`'s
business, not this doc's.

**Honesty caveat.** `~/floppi` is an archived project that nobody has re-run. Nothing here is a claim that
any firmware builds, that any hardware enumerates, or that any of this material was ever actually used.
Everything below is a statement about **bytes on disk on 2026-08-19** and about **what the documents say
about themselves**. Where a claim is the repo's own assertion rather than something I confirmed, it is
marked **DOCUMENTED**; where I confirmed it by reading the file or running a command, **VERIFIED**.

---

## 1. The dedupe hazard — VERIFIED, and the common statement of it is wrong

### 1.1 The claim under test

> "the `docs/literature/literature/` dedupe is NOT safe as commonly described" — peer session, unverified.
> The naive assumption being flagged: *`~/floppi/docs/literature/literature/` is a duplicate of
> `~/floppi/literature/`, so one can be dropped.*

**Verdict: the peer session is right to flag it, but the naive premise is not merely risky — it is false.
The two directories share zero bytes.** The real duplication is somewhere else entirely, and acting on the
naive premise in either direction destroys unique content.

### 1.2 Both trees, complete, with checksums

`~/floppi/literature/` — **15 files, 62,513,908 bytes (59.6 MiB)** (VERIFIED, `find -printf '%s'` summed):

| md5 | bytes | file |
|---|---:|---|
| `9815325ac5757c23a52b16dfd3767305` | 6,716 | `drehmflight_README.md` |
| `2dcb2039b1c33593e5660f81f4722ae1` | 66,953 | `drehmflight_transcripts.md` |
| `7e9c9fcc243324a5045e962537181d77` | 24,075,170 | `dRehmFlight VTOL Documentation.pdf` |
| `3b9dd3a40b5ee1af524beff30c1172ad` | 1,255,135 | `DWM1000 Data Sheet.pdf` |
| `93e34b0f30daed4a19f939cc550e9b25` | 192,385 | `dws1000productbriefv10.pdf` |
| `6e750908fc22e7257191f9cdb259bec1` | 5,289,668 | `ElkeJohnson…Caverly2024-…GNCTesting.pdf` |
| `e16f1bed69ca6dd765523908a36fce97` | 228,181 | `fs-ia6b-manual.pdf` |
| `6ba41a45af89554d48f1506a83b52c4e` | 1,059,782 | `gy-521_mpu-6050_…_en.pdf` |
| `5a57379929ae98de7f7e2bfb967dce24` | 6,769 | `init_about_refactored_drehmflight.md` |
| `8d10f23d0aea541d627c855e4ae0b264` | 132,805 | `Longfly dRehmFlight Purchase Lists.pdf` |
| `f7d2f9b2ac09689c5151e96383c940a1` | 41,174 | `longfly pcb.webp` |
| `51c7773075a699ca824e6293e3c2a505` | 27,449,015 | `Morphy_ A Compliant and Morphologically Aware Flying Robot.pdf` |
| `19197e255cca16567671949b3f393c97` | 1,526,545 | `MPU-6000-Datasheet1.pdf` |
| `8e1a18c9e5afc635748c83f3786dfddc` | 1,415 | `resources.md` |
| `d33e4e12ccbdcda16647416813699721` | 1,182,195 | `RFM69HCW-V1.1.pdf` |

`~/floppi/docs/literature/` — **9 files, 134,660 bytes (131.5 KiB)**, of which **5 distinct**:

| md5 | sha256 (first 16) | bytes | file |
|---|---|---:|---|
| `3a2867cc73214a1a28705887f4a0dfff` | `1c7094bf033ad63a` | 2,616 | `findings/README.md` |
| `1c57cafd283c5a574779c3c0c996e7d5` | `1663b633903a44b3` | 41,548 | `findings/recommended-textbooks.md` |
| **`bf70f96d427b74179c5b502dadff52b3`** | **`705517002e7ff341`** | **24,010** | **`findings/serial-rich-text-formatting.md`** |
| `1df0448f5273e5fa9d2202e99c855f0c` | `288b022d08f59e97` | 7,054 | `roadmap.md` |
| `1217b8bae9a52f851e453969f7da1b8f` | `ca889494ca2165ec` | 4,107 | `scope.md` |
| `3a2867cc73214a1a28705887f4a0dfff` | `1c7094bf033ad63a` | 2,616 | `literature/findings/README.md` |
| `1c57cafd283c5a574779c3c0c996e7d5` | `1663b633903a44b3` | 41,548 | `literature/findings/recommended-textbooks.md` |
| `1df0448f5273e5fa9d2202e99c855f0c` | `288b022d08f59e97` | 7,054 | `literature/roadmap.md` |
| `1217b8bae9a52f851e453969f7da1b8f` | `ca889494ca2165ec` | 4,107 | `literature/scope.md` |

No hidden files in either tree (VERIFIED: `find … -name '.*' -type f` returns nothing).

### 1.3 The three categories the task asked for

**Between `~/floppi/literature/` and `~/floppi/docs/literature/`:**

- byte-identical: **0 files.** Not one md5 in common. (VERIFIED — compare the two tables above.)
- same-name-different-content: **0 files.** No filename appears in both trees.
- unique to one side: **all 24.** The two directories are disjoint corpora that happen to share the word
  "literature" in their path. One is a 59.6 MiB PDF store; the other is 131 KiB of markdown *about*
  literature that mostly does not exist on disk (see §4.2).

**Inside `~/floppi/docs/literature/`, where the real duplication lives:**

- byte-identical (md5 **and** sha256 both match): **4 files.**
  `literature/findings/README.md`, `literature/findings/recommended-textbooks.md`,
  `literature/roadmap.md`, `literature/scope.md` — each identical to its parent-level counterpart.
- same-name-different-content: **0 files.**
- unique to the OUTER dir: **1 file** — `docs/literature/findings/serial-rich-text-formatting.md`
  (24,010 bytes, md5 `bf70f96d427b74179c5b502dadff52b3`).
- unique to the INNER dir: **0 files.**

`docs/literature/literature/` is therefore a **partial, strictly-smaller self-copy of its own parent**, not
a copy of `~/floppi/literature/` at all.

### 1.4 How it got there (VERIFIED from git)

Both levels were created in the **same commit**: `70817f4` *"ready for hardware, migrate 3D engineering to
engineer360"*, kaleldev, 2026-01-11. `git show --name-status 70817f4` adds all 8 paths as `A` in one go —
the inner copy was **born duplicated**, most likely a mis-targeted copy during that migration. The fifth
file, `serial-rich-text-formatting.md`, arrived later in `cdbeceb` and landed **only in the outer dir**.
That asymmetry is the whole hazard.

### 1.5 Is a dedupe safe? — Yes, in exactly one direction

**SAFE — these 4 files may be dropped, and nothing is lost. Named individually:**

1. `/home/devel/floppi/docs/literature/literature/findings/README.md`
2. `/home/devel/floppi/docs/literature/literature/findings/recommended-textbooks.md`
3. `/home/devel/floppi/docs/literature/literature/roadmap.md`
4. `/home/devel/floppi/docs/literature/literature/scope.md`

i.e. the entire `docs/literature/literature/` subtree. Every one has a byte-identical twin one level up,
confirmed on two independent hash functions.

**MUST BE KEPT — dropping any of these loses the only copy on this machine:**

- `/home/devel/floppi/docs/literature/findings/serial-rich-text-formatting.md` — **the file the dedupe
  will eat if run the wrong way round.** Unique to the outer dir; no copy anywhere else in floppi.
- `/home/devel/floppi/docs/literature/findings/README.md`
- `/home/devel/floppi/docs/literature/findings/recommended-textbooks.md`
- `/home/devel/floppi/docs/literature/roadmap.md`
- `/home/devel/floppi/docs/literature/scope.md`
- **All 15 files in `/home/devel/floppi/literature/`.** None of them is a duplicate of anything — VERIFIED
  by hashing every non-PDF against every other file in floppi (`.git`/`.pio` excluded): all five
  markdown/webp items return `dup_elsewhere=0`, and `find` confirms `~/floppi/literature/` is the **only**
  directory in floppi containing any `.pdf` or `.webp` at all.

### 1.6 The three ways to get this wrong

- **(a) Keep the inner, drop the outer.** Loses `serial-rich-text-formatting.md` — the one file in
  `docs/literature/` with a real downstream consumer (§4.3). Silent: no error, no broken link, the tree
  still looks plausible.
- **(b) Act on the naive premise as stated.** Anyone who believes `docs/literature/literature/` and
  `~/floppi/literature/` are interchangeable can just as easily delete `~/floppi/literature/` — **59.6 MiB
  and 15 unique files, including 4 that exist nowhere else.** This is the destructive reading of the
  hazard, and it is the one worth guarding against.
- **(c) Run a generic dedupe tool at `docs/literature/`.** Tools that keep "first path alphabetically"
  or "deepest path" pick the **inner** copy and land on (a). `docs/literature/findings/…` sorts before
  `docs/literature/literature/findings/…`, so an alphabetical *keep-first* is fine, but a *keep-last* or
  a depth-preferring rule is not. Do not delegate this to a tool; delete the four named paths.

### 1.7 Why a mistake here is permanent — git history was rewritten

Commit `6a9646c` (2026-03-30, kaleldev): *"Re-add literature PDFs after history cleanup (compressed)"*,
body: *"PDF blobs removed from git history via filter-repo to reduce repo size. Files re-added with pikepdf
compression applied."* (VERIFIED, `git log -1 --format=%b 6a9646c`.)

Two consequences that change the risk calculus:

1. **The *current* blobs ARE recoverable from git — an earlier draft of this doc said otherwise and was
   wrong.** All 15 `literature/` paths are in `HEAD` and in `6a9646c` (VERIFIED: `git ls-tree -r HEAD
   --name-only literature/` lists all 15; `git cat-file -e HEAD:'literature/DWM1000 Data Sheet.pdf'` and
   `git cat-file -e 6a9646c:'literature/Morphy_….pdf'` both succeed). So a PDF deleted from the working
   tree **is** restorable with `git show HEAD:<path>`. What is unrecoverable is the *pre-rewrite* blob —
   see consequence 2. Do not treat the working tree as the only copy of the current bytes.
2. **The PDFs on disk are recompressed derivatives, and the pre-`filter-repo` originals are gone from
   floppi's history.** They open fine (VERIFIED: `pdfinfo` succeeds on all 10; `pdftotext` returns
   non-whitespace text from all 10, but only **9 have a usable body-text layer** — item 14 returns 5,832
   characters that are nothing but the Wiley download watermark repeated across 19 pages, see §3.6). They
   are not bit-equal to whatever was fetched from Wiley / ResearchGate / Decawave, and no earlier revision
   in this repo holds the pristine bytes. Anything that needs the original must re-download from the
   citation, not from this tree and not from git history.

**Git tracking status (VERIFIED):** all 24 files across both trees are tracked; `git check-ignore` matches
none of them. So they will all travel on a fresh import unless deliberately excluded.

---

## 2. Verdict on the prior HYBRID recommendation

The prior recon (`03_folder_recon_findings.md` §10) proposed: **centralise in `research/literature/`, but
pin hardware datasheets to the repo that consumes them, with `research/literature/INDEX.md` holding the
cross-repo references.**

**Endorsed in shape. Three rows are dead and the pin rule needs a stated test.**

### 2.1 What is broken in it

**(i) Three rows point at a repo that will not exist.** `03` §10 routes `DWM1000 Data Sheet.pdf` and
`dws1000productbriefv10.pdf` → `communication_hardware/uwb/dw1000/references/`, and `RFM69HCW-V1.1.pdf` →
`communication_hardware/radio/references/`. `11_routing_v2` §1 row 10 marks `communication_hardware` **DO
NOT CREATE** under R1. Those three files have no destination in the current plan.

**(ii) The pin rule's premise fails for exactly those three — there is no consumer.** VERIFIED by
case-insensitive grep, `.git/`, `.pio/` and `literature/` excluded. **Read the two columns separately — an
earlier draft of this table claimed to be an all-files grep while actually reporting only code directories,
which is how it came to state two things that are false.** The *code* column is `--include=*.{h,cpp,c,ino,py}`;
the *prose* column is every other tracked file type (`.md`, `README`s, `.vscode`, …). The pin rule keys off
the **code** column only (corollary 1); the prose column is shown because it is where the earlier draft's
errors hid.

| Part string | Consuming subprojects — CODE | Also appears in PROSE (not a consumer) |
|---|---|---|
| `RFM69` | **none** | **`temp_reorg/*.md` planning prose only — 5 files.** NOT `docs/todo.md`: `grep -c RFM69 docs/todo.md` = **0**. The earlier draft named `docs/todo.md` here and that was false. |
| `DW1000` / `DWM1000` | **none** | `temp_reorg/*.md` only — 11 files. Again **not** `docs/todo.md` (`grep -c` = 0). |
| `MPU6050` | `flight_controller/{include,lib,src,tools}`, `auto_orientation/{src,tools}`, `dRehmFlight-master/code` | Heavily, incl. 36 files in `flight_controller/docs` and 31 in `auto_orientation/docs` |
| `FS-iA6B` / `iA6B` | `flight_controller/include/pin_definitions.h` — **the only code hit in floppi** | **21 further files**, incl. 17 under `flight_controller/docs/`, plus `flight_controller/README.md`, `docs/ROADMAP.md`, `tmp.md`, and `auto_orientation/docs/findings/research_wheel_encoders_mega_2026-05-19.md` |
| `SBUS` / `iBUS` | `flight_controller/{include,lib,src}` (+ vendored dRehmFlight) | 42 files in `flight_controller/docs`, plus `fc_tool/docs`, `auto_orientation/docs`, `docs/*` |
| `Longfly` | **none** | `temp_reorg/*.md` only — 4 files |

The UWB and RFM69 datasheets were consumed by **SwarmLoc**, which under R1/R6 is legacy, read-only, and not
migrating. So those three land in the central store **by default, not by preference**.

**(iii) The pin rule also fails in the opposite direction for the MPU pair — there are *two* consumers.**
`MPU6050` appears in both `flight_controller/` and `auto_orientation/`. "Pin to the repo that consumes it"
has no answer here; it forces either a duplicated blob or an arbitrary winner.

**(iv) It misses that `drehmflight_README.md` fills a hole.** VERIFIED: `~/floppi/dRehmFlight-master/`
contains `code/`, `COPYING.txt`, `LICENSE`, `dRehmFlight Logo.png` — **and no README of any kind**. The
copy in `literature/` is the upstream README. `03` §10 calls it a "mirror" to centralise; it is better
described as the missing piece of the vendored tree that `11_routing_v2` §2 is about to create at
`flight_controller/vendored/dRehmFlight/`.

**(v) It routes nothing to `engineer360`,** because §10 predates N-2/R4. The Morphy paper is squarely
engineer360's under the task's own test.

### 2.2 The amended rule (replaces "pin datasheets to the consumer")

> **Pin a document into a repo only when the consumer count is exactly 1 and that consumer is an ACTIVE
> repo. Otherwise it is canonical in `research/literature/` and every interested repo gets an INDEX line,
> never a copy.**
>
> Corollaries:
> 1. **Count consumers by grep over code, not by topic intuition.** A datasheet's topic says nothing about
>    whether any surviving repo actually programs the part.
> 2. **A legacy-repo consumer counts as zero.** SwarmLoc/GravityProbe/WayfindR are read-only under R6.
> 3. **Never pin into a repo whose scope forbids the file's layer.** Dropping `RFM69HCW-V1.1.pdf` into
>    `swarm_communication_protocol` would drag that repo across the hiverf boundary declared in
>    `11_routing_v2` §2 ("if this repo starts modelling contention, interference or throughput collapse, it
>    has crossed into hiverf"). A radio PHY datasheet is an invitation to do exactly that.
> 4. **`research/literature/INDEX.md` is mandatory, not optional** — it is the only thing that makes the
>    **9-pinned / 11-central** split navigable (8 pinned into `flight_controller`, 1 into `engineer360`,
>    11 canonical in `research/literature/`, across both trees' 20 distinct items; the per-tree breakdown
>    is in the paragraph below and again in §6 step 3). Keep the prior recon's format
>    (`Author | Year | Title | Topic | File path | Notes path | Tags`, `05_research_repo_scope.md` §3.2)
>    and add a `Pinned-to` column.

Applying the rule to `~/floppi/literature/`'s 15 files: **7 pin to `flight_controller`** (items 1, 2, 4,
5, 8, 9, 10 — the dRehmFlight vendored-tree cluster plus the single-consumer RC and build material),
**1 goes to `engineer360`** (item 14), and **7 are canonical in `research/literature/`** (items 3, 6, 7,
11, 12, 13, 15). From `docs/literature/`: **4 to `research/literature/`** and **1 more to
`flight_controller`** (item 20), with 4 deleted as duplicates. Two of the pinned files (items 4 and 20)
are not literature at all — they are misfiled project documents that leave the store entirely.
Nothing routes to `sensor_interactions/`, `auto_orientation_research/`, or `position_denial_research/`
as a *home* — those three get INDEX cross-references only, for the reasons given per row.

### 2.3 This document OVERRIDES the governing routing doc — flagged, not smuggled

**`11_routing_v2` and this doc disagree, and the disagreement must be surfaced before anyone executes
either.** `11_routing_v2` §2's `→ research` table (line 183) routes **`literature/` (60 MB PDFs) +
`docs/literature/` → `literature/`** — the whole of both trees, wholesale, into `research/literature/`.
This document sends **9 of those 20 distinct items somewhere else**: 8 into `flight_controller` (items 1,
2, 4, 5, 8, 9, 10, 20) and 1 into `engineer360` (item 14), leaving 11 for the central store.

That is a **substantive divergence, not an elaboration**, and earlier drafts of this doc leaned on
`11_routing_v2` §2 to justify moving the dRehmFlight manual while never admitting it was contradicting the
same section's own table row. The case for overriding is the pin rule in §2.2 plus the vendored-tree
argument in §2.1(iv) — but the override is **not this document's to ratify**. `11_routing_v2` is the
governing doc; either it is amended to match §2.2, or §2.2 is wrong. **Operator decision, and a blocker for
§6.** Until it is resolved, treat §6's execution order as a proposal, not an instruction.

---

## 3. Routing table — `~/floppi/literature/` (15 items)

Every "what it is" below is from `pdfinfo` metadata plus extracted page text, not from the filename.

### 3.1 dRehmFlight cluster (5 items, 24.15 MB)

| # | File | Size | What it actually is (VERIFIED) | Destination | Why |
|---|---|---:|---|---|---|
| 1 | `dRehmFlight VTOL Documentation.pdf` | 24,075,170 B (23.0 MiB) | **72 pp**, LaTeX/pdfTeX-1.40.20, created 2022-07-29. Title page: *"dRehmFlight VTOL — Teensy/Arduino Flight Controller and Stabilization, nicholas rehm"*. ToC: Hardware Requirements (Teensy 4.0/4.1, MPU6050, MPU9250), Hardware Setup, Software Setup, Conventions (IMU & vehicle orientation, radio channel mapping), The Code (`setup()`/`loop()`), Functions List, §12 License Information. 96 embedded images. | **`flight_controller/vendored/dRehmFlight/docs/`** | `11_routing_v2` §2 already sends `dRehmFlight-master/` → `flight_controller/vendored/dRehmFlight/`. Splitting vendored code from its own 72-page manual across two repos is the failure this whole exercise exists to prevent. INDEX entry in `research/literature/`. |
| 2 | `drehmflight_README.md` | 6,716 B | The **upstream dRehmFlight GitHub README** (logo link, intro video, RCGroups thread, Beta 1.3 changelog, Teensy 4.0 + GY-521 hardware list with Amazon affiliate links). | **`flight_controller/vendored/dRehmFlight/README.md`** | VERIFIED: `dRehmFlight-master/` has **no README on disk**. This restores it. Upgrade from `03` §10's "mirror → centralise". |
| 3 | `drehmflight_transcripts.md` | 66,953 B | Auto-generated **YouTube transcript** of Nicholas Rehm's dRehmFlight intro video (`watch?v=tlD0C5CrWcA`) and others — lowercase, unpunctuated ASR text with HTML entities (`&#39;`). | **`research/literature/drehmflight/_raw/`** | Not documentation; a scraped transcript with murky provenance. Keep it out of the vendored tree and out of anything indexed as authoritative. Carry a provenance header per the `_raw/` convention `11_routing_v2` §2 uses for the DARPA-Lift transcripts. |
| 4 | `init_about_refactored_drehmflight.md` | 6,769 B | **Floppi-authored**, not literature. "Quick Setup Guide — dRehmFlight PlatformIO": SBUS-vs-iBUS decision for the FS-iA6B, where `radioComm.ino`'s functions went in floppi's own refactor (`getCommands()` → `main.cpp` ~line 290, `failSafe()` → ~line 332), the `include/config.h` + `include/pin_definitions.h` layout, 5-minute setup. | **`flight_controller/docs/history/`** | It documents *floppi's* port, not the upstream project. It should never have been filed under `literature/`. Agrees with `03` §10. |
| 5 | `resources.md` | 1,415 B | Link list: 3 dRehmFlight forks (nickrehm, dgm3333 ESP32-webserver variant, Qlongl LongFly), a **LongFly PCB spec block** (30.5×30.5 mm mount, 52×50×10 mm, 2-8S, 240 A cont./300 A burst PDB, 5 V4 A BEC, ~9 g, 6 motor pads, SBUS/PPM/PWM, Oneshot125, MPU6050), 5 Thingiverse micro-quad frames, and a "Future Developments → Morphy" pointer with the Wiley DOI. | **`flight_controller/docs/references/`** | Two-thirds of it is FC firmware/PCB material with a single consumer. Do not split a 1.4 KB file — but note it is the **provenance record for item 14**, and its 5 Thingiverse frame links are the one engineer360-flavoured line: *cite* them into engineer360, do not move the file. |

### 3.2 IMU (2 items, 2.59 MB) — the two-consumer case

| # | File | Size | What it actually is (VERIFIED) | Destination | Why |
|---|---|---:|---|---|---|
| 6 | `MPU-6000-Datasheet1.pdf` | 1,526,545 B | **52 pp**, InvenSense Inc., *"MPU-6000 and MPU-6050 Product Specification Revision 3.4"*, doc no. **PS-MPU-6000A-00**, released 2013-08-19. The authoritative silicon datasheet. | **`research/literature/imu/`** (canonical) + INDEX pointers from `flight_controller` **and** `auto_orientation_research` | Consumer count = **2** (VERIFIED). The pin rule has no answer; central + two pointers is the only non-duplicating one. The part actually in use is the MPU-6050, which this same document specifies. Be precise about the filename, though: the *unhyphenated* `MPU6000` appears in **0** lines of floppi code, but the **hyphenated** `MPU-6000` — the form this datasheet's own title uses — appears in **41** code lines across `flight_controller/lib/MPU6050/src/*.{cpp,h}` and `dRehmFlight-master/code/src/MPU6050/*.{cpp,h}` (e.g. `MPU6050.cpp:162` "the MPU-6000, which does not have a VLOGIC pin", and the `RM-MPU-6000A-00` register-map citation in eight file headers). An earlier draft's "`MPU6000` never appears in floppi code" was true only of the unhyphenated spelling and misleading as stated. |
| 7 | `gy-521_mpu-6050_3-axis_gyroscope_and_acceleration_sensor_en.pdf` | 1,059,782 B | **24 pp**, LibreOffice 7.0, 2021-01-06. **Not a datasheet** — an **AZ-Delivery reseller eBook**: *"Thank you for purchasing our AZ-Delivery GY-521 MPU-6050…"*, spec table (operating current, ranges, dimensions) plus Arduino Uno and Raspberry Pi quick-start wiring tables by wire colour. | **`research/literature/imu/`** (canonical), same two INDEX pointers | Same two consumers, and it is the *lower-authority* of the pair — a breakout-board user guide, superseded for every electrical question by item 6. Freely re-downloadable from AZ-Delivery; a strong candidate for **cite-don't-carry** (§5). |

### 3.3 RC link (1 item, 228 KB) — the clean pin

| # | File | Size | What it actually is (VERIFIED) | Destination | Why |
|---|---|---:|---|---|---|
| 8 | `fs-ia6b-manual.pdf` | 228,181 B | **5 pp**, wkhtmltopdf 0.12.6, 2023-05-24. Title: *"FLYSKY FS-iA6B Receiver 2.4G 6CH with Double Antenna User Guide"*. **It is a scrape of `manuals.plus`**, not the Flysky original — page 1 header reads *"Manuals+ — User Manuals Simplified … Home » Flysky » …"*. Covers binding, firmware update, failsafe, PWM/PPM/i-BUS/S.BUS output. | **`flight_controller/hardware/rc/`** | Consumer count = **1 in code**, VERIFIED: the only *code* hit for `FS-iA6B`/`iA6B` anywhere in floppi is `flight_controller/include/pin_definitions.h`. **State the qualifier — the bare string is not rare.** It also appears in 21 non-code files (17 under `flight_controller/docs/`, plus `flight_controller/README.md`, `docs/ROADMAP.md`, `tmp.md`, and one `auto_orientation` findings doc); an earlier draft said it "appears only in `flight_controller/include`", which is false of the tree as a whole. The routing conclusion survives on the code-only reading — every prose hit but the `auto_orientation` one is already inside `flight_controller` — but the pin rests on a code grep, not on scarcity. Clean pin, and the failsafe/binding pages are exactly what item 4's SBUS decision depends on. |

### 3.4 Build / procurement (2 items, 174 KB)

| # | File | Size | What it actually is (VERIFIED) | Destination | Why |
|---|---|---:|---|---|---|
| 9 | `Longfly dRehmFlight Purchase Lists.pdf` | 132,805 B | **3 pp**, Google Docs export (Skia/PDF m139), no author metadata. Community BoM: vendor advice (Getfpv, RacedayQuads vs Amazon, "20-30% less"), battery safety-procedure video, **3D-print guidance — PETG @ 25% infill, or PLA @ 50%; motors running hot loosen nuts/bolts**, then generic parts (M2/M3 screw kits, battery straps). | **`flight_controller/hardware/`** | It is the BoM for the FC build; its consumer is whoever assembles that airframe. The **print-settings line is engineer360-shaped** — quote it into engineer360's material/process notes (cite-not-copy), don't move the file. |
| 10 | `longfly pcb.webp` | 41,174 B | RIFF/WebP image of the LongFly PCB. | **`flight_controller/hardware/`** | It is the illustration for item 5's spec block and item 9's BoM. Keep the three together or the spec block loses its picture. |

### 3.5 UWB + sub-GHz radio (3 items, 2.63 MB) — orphaned by R1

| # | File | Size | What it actually is (VERIFIED) | Destination | Why |
|---|---|---:|---|---|---|
| 11 | `DWM1000 Data Sheet.pdf` | 1,255,135 B | **33 pp**, A4, author **DecaWave**, 2020-09-01. **Metadata/filename mismatch worth recording:** the PDF's own `Title` is *"DW1000 Datasheet"* and `Subject` is *"Datasheet"*, but page 1 is the **DWM1000 module** overview — *"based on Decawave's DW1000 UWB transceiver IC. It integrates antenna, all RF circuitry, power management and clock circuitry in one module… 2-way ranging or TDOA… precision of 10 cm… up to 6.8 Mbps"*. So: module datasheet, IC metadata. | **`research/literature/uwb/`** | Consumer count = **0** in floppi (VERIFIED). Its consumer was SwarmLoc `DWS1000_UWB/`, legacy/read-only under R1+R6. `position_denial_research` is theory-only and explicitly cites hiverf rather than implementing — a silicon/module datasheet is the wrong layer for it. Central store, INDEX cross-ref from `position_denial_research/theory/uwb_ranging/`. **Any index built from PDF metadata will mislabel this file** — set the title by hand. |
| 12 | `dws1000productbriefv10.pdf` | 192,385 B | **2 pp**, author Decawave, 2020-07-10. *"PRODUCT BRIEF: DWS1000"* — an Arduino form-factor **shield** carrying the DWM1000 module; a marketing one-pager, not a technical reference. | **`research/literature/uwb/`** | Same zero-consumer reasoning. Lowest-value item in the set: 2 pages of marketing. Reasonable to drop entirely in favour of an INDEX line + vendor URL. |
| 13 | `RFM69HCW-V1.1.pdf` | 1,182,195 B | **79 pp**, A4, *"Microsoft Word - RFM69HCW-V1.1.doc"*, Acrobat Distiller, 2013-11-18. HopeRF **RFM69HCW ISM transceiver module** spec v1.1 — 315/433/868/915 MHz license-free bands, programmable narrow/wide-band modes, and (per its own intro) a large slice of the underlying **RF69H** chip's parameters. | **`research/literature/radio/`** | Consumer count = **0** (VERIFIED). Its consumer was SwarmLoc `lora_feather_esp32/`, legacy. **Explicitly do not pin to `swarm_communication_protocol`** — that repo declares message sizes, fan-out and duty cycle and hands them to hiverf; putting a 79-page PHY spec in it is the first step across the boundary `11_routing_v2` §2 draws. |

### 3.6 Academic papers (2 items, 32.7 MB — 52% of the tree)

| # | File | Size | What it actually is (VERIFIED) | Destination | Why |
|---|---|---:|---|---|---|
| 14 | `Morphy_ A Compliant and Morphologically Aware Flying Robot.pdf` | 27,449,015 B (26.2 MiB) | **19 pp.** Title metadata: *"Morphy: A Compliant and Morphologically Aware Flying Robot"*. Wiley **Advanced Intelligent Systems**, DOI **10.1002/aisy.202400493**. Printed to PDF from Chrome 137 on 2025-06-14 (Skia/PDF m137). **Fully rasterized** — `pdfimages` shows one 2480×3260 ICC image per page at 296 ppi; the *entire* text layer of the 26 MiB file is **2 distinct lines**, both the Wiley download watermark. Cross-referenced from item 5 under "Future Developments". | **`engineer360/`** (structural/aero knowledge) + INDEX entry in `research/literature/` | The task's own test decides it: a compliant, morphologically-aware airframe is *"a geometry and a piece of knowledge"*, not *"a thing that flies"* that any floppi repo builds. It is aspirational future-direction reading (item 5 files it that way), not a reference any current code consumes. **Three hazards, see §5.** |
| 15 | `ElkeJohnsonRoslanskyGebre-EgziabherandCaverly2024-DesignFabricationandFlightoftheCost-andRisk-ReducingQuadcopterSystemforGNCTesting.pdf` | 5,289,668 B | **15 pp**, LaTeX/pdfTeX-1.40.24, 2024-02-26. Page 1 is a **ResearchGate cover sheet** ("Conference Paper · February 2024", 5 authors). Page 2: **AAS 24-P15**, *"Design, Fabrication, and Flight of the Cost- and Risk-Reducing Quadcopter System for GNC Testing"* — Elke, Johnson, Roslansky, Gebre-Egziabher, Caverly (Univ. of Minnesota AEM; first author now NASA). A quadrotor testbed with an **inverted pendulum and a hanging pendulum** to mimic launch-vehicle and landing-system dynamics for maturing GNC algorithms through TRL 4-6. | **`research/literature/flight_dynamics/`** (canonical) + INDEX cross-ref from `auto_orientation_research/` | It is a **methodology** paper — how to build a cheap testbed for attitude/estimation algorithms — which is the closest thing in this corpus to `auto_orientation_research`'s problem. But it is general academic literature, not a floppi artefact, so it stays canonical in the central store. **Highest redistribution risk in the set** (§5). |

---

## 4. Routing table — `~/floppi/docs/literature/` (5 distinct items)

| # | File | Size | What it actually is (VERIFIED) | Destination | Why |
|---|---|---:|---|---|---|
| 16 | `findings/recommended-textbooks.md` | 41,548 B | **1141 lines**, compiled 2026-01-11. A structured textbook reading list across 5 disciplines with per-book notes: Aerospace (Anderson, Etkin & Reid, Stevens & Lewis, Raymer…), Mechanical (materials, machine design, structural analysis, vibrations), Electrical (power electronics, motor drives, embedded, sensors, control), Battery & Energy Storage (electrochemistry → BMS), Rotary-Wing (Leishman, Seddon & Newman, Prouty; multi-rotor; NASA papers), then "Quick Reference: Top Picks by Category" and "Universities Referenced". | **`research/literature/`** — *with a provenance header prepended* | The **longest and most immediately useful** document in either tree: self-contained, readable without this machine, and the only thing approaching a reading list. **But it is LLM-generated and must be migrated marked as such, not "as-is".** VERIFIED: its own line 4 reads `**Compiled by:** Literature Review Agent`; it contains **zero ISBNs** (`grep -ci isbn` = 0), cites no source for any claim, and carries unattributed pull-quotes (line 49: *"The best intro to Aero book on the planet"*). Its assertions about university adoption and citation frequency are therefore **DOCUMENTED, not VERIFIED** — nothing in the file supports them and nothing here checked them. This is the same generated-output class as item 3's `_raw/` quarantine and the N-2 fence in `11_routing_v2`; the only reason it is not quarantined is that a textbook list is cheap to spot-check against a library catalogue, which the next reader should do before ordering anything. Titles/authors are plausible; treat every *evaluative* line as unsourced. |
| 17 | `findings/README.md` | 2,616 B | Index for #16. **DOCUMENTED but false:** it advertises 5 sibling reports (`aerospace-textbooks.md`, `mechanical-textbooks.md`, `electrical-textbooks.md`, `battery-textbooks.md`, `rotary-wing-textbooks.md`) and a reorganised `books/` tree with 8 subject folders. **VERIFIED: none of the 5 files exists, and no `books/` directory exists anywhere in floppi** (`ls books docs/books docs/literature/books` → all "No such file"). | **`research/literature/`** — *only after stripping the dangling references* | Migrating it unedited ships a broken index into a brand-new repo. The file listing and the `books/` tree must go. Its research-methodology paragraph (university adoption, AIAA/IEEE, citation counts) reads well but is **the same unsourced LLM-generated provenance as item 16** — keep it only as a description of what the author *intended*, never as evidence that the method was followed. |
| 18 | `roadmap.md` | 7,054 B | A 7-phase literature **acquisition** plan (Core Aerospace → Rotary-Wing → VTOL/Drone → Battery/Power → Mechanical → Electrical Components → Organization & Knowledge Management), naming **11 specific textbook references** — 10 quoted titles (Anderson *Fundamentals of Aerodynamics*, Katz & Plotkin *Low-Speed Aerodynamics*, Etkin & Reid, Nelson, Stevens & Lewis, Raymer, Leishman, Seddon & Newman, Prouty, plus one more) and "Roskam's aircraft design series" at line 27 — VERIFIED by `grep -o '"[^"]*"'`. The remaining phases name **generic categories** ("Collect control systems theory references"), not titles; an earlier draft's "~30 specific textbooks" was roughly triple the real count. | **`research/literature/docs/history/`** | **Aspirational; the acquisition half was not executed.** VERIFIED narrowly: **no `books/` directory exists anywhere in floppi** (`find -type d -name books` → 0) **and no textbook PDF exists** — the only 10 PDFs in the tree are the ones catalogued in §3. Stop short of "zero of it was executed": Phase 3's *"Small UAS/Drone Literature"* is arguably met by items 14 and 15 already sitting in `literature/`, and Phase 7's cataloguing task is partly met by item 16 itself. Migrate as a record of intent, clearly marked, not as a live plan. |
| 19 | `scope.md` | 4,107 B | In/out scope for the literature collection: IN = aerospace, aerodynamics, rotary-wing, VTOL, mechanical, electrical, battery chemistry. OUT = *"general electronics tutorials"*, *"hobbyist drone build guides"*, *"consumer drone operation manuals"*. Quality standards demand *"academic textbooks from recognized publishers, peer-reviewed journal articles…"*. | **`research/literature/docs/history/`** | **It contradicts the collection it governs.** By its own out-of-scope list, items 1, 2, 3, 4, 5, 7, 8, 9, 10 (**9** of 15 — count the list, an earlier draft said 10) do not belong: the AZ-Delivery guide is a hobbyist tutorial, the Manuals+ scrape is a consumer manual, the Longfly list is a build guide. Only items 14-15 meet its "peer-reviewed" bar. Migrate as history; **do not migrate it as if it governs `research/literature/`** — write a fresh scope that describes the actual store. |
| 20 | `findings/serial-rich-text-formatting.md` | 24,010 B | **Not literature at all.** Dated 2026-02-10. ANSI/VT100 (ECMA-48) escape-code reference: full SGR parameter table with reset codes, 8/bright/256/24-bit colour tables, cursor control "useful for live telemetry displays", C/C++ embedded usage examples, a **survey of which serial monitors render ANSI**, a graceful-degradation strategy, markdown-over-serial analysis, a terminal-emulator feature matrix, "The PlatformIO Factor", a **byte-overhead and 115200-baud bandwidth budget** with a performance verdict, and §6 *"Recommendations for Floppi"* with a suggested colour scheme and byte budget. | **`flight_controller/findings/`** + INDEX cross-ref from `fc_tool` | Misfiled. It is firmware-serial-output engineering with two real consumers — the firmware that emits the escape codes (`flight_controller`) and the host monitor that renders them (`fc_tool`, whose autoscroll work appears in the same commit `cdbeceb` that added this file). **This is the file the §1 dedupe hazard would have destroyed.** |
| — | `literature/` (4 files) | 55,325 B | Byte-identical partial self-copy of the above. | **DELETE — see §1.5** | Nothing unique. Four named paths, zero loss. |

**Nothing from either tree routes to `sensor_interactions/`** — but **not** for the reason an earlier draft
gave. That draft justified it as "there is no lidar, camera, ranging-sensor, or multi-sensor-interaction
document in this corpus", **which the corpus contradicts**: item 11 is a *ranging* datasheet by its own
page 1 (*"2-way ranging or TDOA location systems… precision of 10 cm"*, quoted at §3.5), and items 6 and 7
are IMU sensor documents. The corrected justification is a **layer** argument, not an absence argument:
`sensor_interactions` is about how multiple sensors interfere with and complement *each other*, and every
sensing document here is a **single-part silicon/module spec** — none of them discusses a second sensor at
all. There is no lidar or camera document (that part of the old claim holds). So `sensor_interactions` gets
INDEX cross-references to items 6, 7 and 11 and no files; its actual inheritance comes from WayfindR
(`12_wayfindr_reference_inventory.md`), not here.

**Nothing stays in the floppi archive.** All 20 distinct items route out; 4 are deleted as duplicates.

---

## 5. Copyright — 12 of 15 items in `literature/` are third-party, 2 are floppi's, 1 is unresolved

This matters because `research` is being created as a new GitLab repo in a group. **Correct the headline
number: third-party material is 62,464,550 of 62,513,908 bytes = 99.92% of the tree**, not the 82.4% an
earlier draft claimed. 82.4% is what you get from items 14 and 1 alone (51,524,185 B) — the two largest
PDFs, not the third-party set. The floppi-native remainder is **49,358 bytes**, under 0.1%. Effectively the
whole byte-weight of `literature/` is somebody else's.

The 15 items classify as **12 third-party** (10 below + items 1 and 2), **2 floppi-native** (items 4 and 5),
and **1 unresolved**: item 10, `longfly pcb.webp`. It is a photograph of a third-party PCB with no EXIF
attribution and no recorded source — it may be an operator photo or lifted from a vendor listing, and
nothing on disk decides which. **It is deliberately left unclassified; resolve it before any public push.**

**Third-party, redistribution NOT established** (**10** items — count the rows — 38,382,664 B / ~38.4 MB):

| Item | Owner | Note |
|---|---|---|
| 15 · Elke et al. 2024 | American Astronautical Society (AAS 24-P15) | Obtained via **ResearchGate** — page 1 is RG's cover sheet. **The riskiest item to republish**: an author-uploaded copy of a society conference paper. No copyright or CC statement found anywhere in the text (VERIFIED: full-document `pdftotext \| grep -inE 'copyright\|Creative Commons\|all rights'` returns **zero** hits). **This is the paper of the American Astronautical Society (AAS 24-P15), not AIAA** — an earlier draft's parenthetical about "AIAA hits" was a non-sequitur on two counts: `AIAA` was never in that grep pattern, and the paper is not an AIAA paper. AIAA does appear 8 times in the file, all inside the reference list. Absence of a notice is **not** a grant of permission. |
| 14 · Morphy | Wiley (*Advanced Intelligent Systems*) | **License is not determinable from the file.** The only text in it is Wiley's generic download stamp, which says *"OA articles are governed by the applicable Creative Commons License"* — a conditional that does not tell you whether *this* article is OA. It also names the **downloading subscriber account and the download date** in that stamp (account name redacted here as `<institutional Wiley account>`; the literal string is in the PDF's text layer, recoverable with `pdftotext` by anyone who needs it), i.e. the file carries an institutional-access fingerprint identifying who pulled it. **Do not assume OA.** Resolve via the DOI before it goes anywhere public. |
| 6 · MPU-6000 Product Spec | InvenSense (now TDK) | Vendor datasheet; freely downloadable, redistribution not licensed. |
| 7 · GY-521 guide | AZ-Delivery | Reseller product eBook. |
| 8 · FS-iA6B manual | `manuals.plus` scrape of a Flysky manual | Two layers of third-party: Flysky's content, republished by Manuals+. |
| 11 · DWM1000 datasheet | DecaWave (now Qorvo) | Vendor datasheet. |
| 12 · DWS1000 brief | DecaWave (now Qorvo) | Vendor marketing brief. |
| 13 · RFM69HCW spec | HopeRF | Vendor datasheet. |
| 9 · Longfly Purchase Lists | unattributed community Google Doc | No author metadata at all; provenance unknown. |
| 3 · dRehmFlight transcripts | YouTube ASR of Nicholas Rehm's videos | Scraped transcript; not a licensed artefact. |

**Third-party but redistributable** (2 items, 24.1 MB): items **1** and **2**. VERIFIED:
`~/floppi/dRehmFlight-master/LICENSE` and `COPYING.txt` are both **GNU GPL v3, 29 June 2007**, and the PDF
itself carries §12 *"License Information"*. **Quoted verbatim** (VERIFIED, `pdftotext` output line 3188 —
an earlier draft rendered "provided source" for "provided code", moved the ellipsis, and silently dropped
three of the six granted rights):

> "This work includes a GNU General Public License. This allows commercial use, distribution,
> modification, patent use, and private use of the provided code on the condition that the original
> source is disclosed, the same license is used on modified work, and all changes are clearly stated."

Carry `LICENSE` with them into `flight_controller/vendored/dRehmFlight/`.

**Floppi-native** (**2** items in `literature/`: 4 and 5 — an earlier draft said 4 items and then named
only 2 — **plus** all 5 distinct docs in `docs/literature/`, for 7 floppi-native items across both trees).
Item 10 is *not* counted here; see the unresolved note above.

### Recommendation

1. **`research/literature/` should be a private repo, or citation-only.** If it is ever public, 9 vendor
   and academic PDFs become a redistribution question that nobody in this migration has authority to
   answer. The cheap fix is a `SOURCES.md` carrying DOI / part number / vendor URL for each, with the
   blobs kept out of git. **Six** of them are vendor documents re-downloadable from a part number (items 6,
   7, 8, 11, 12, 13) — an earlier draft said 8. The other three need a different key each: item 15 by DOI or
   the AAS proceedings citation, item 14 by DOI `10.1002/aisy.202400493`, and **item 9 by nothing at all** —
   it is the unattributed community Google Doc described three rows up, with no part number, no author
   (VERIFIED: `pdfinfo` gives Title "Purchase Lists" and no Author) and no known canonical URL. **Item 9 is
   the one blob that cannot be re-fetched if it is dropped.** Either carry it or accept losing it.
2. **Drop item 14's blob and keep the citation.** 26.2 MiB, 43.9% of the whole tree, **no searchable body
   text** — precisely: `pdftotext` yields 5,832 characters and they are *all* the 2-line Wiley download
   watermark repeated across 19 pages (§3.6), so the searchable *content* is nil while the file is not
   literally text-free; rasterized, and useless to any RAG index or `grep` without an OCR pass — and an unresolved licence
   with an institutional-access fingerprint baked in. A DOI line plus a distilled paragraph in
   `engineer360` recovers 100% of the durable value at 0.001% of the size. If the operator wants the
   paper, re-fetch it from `10.1002/aisy.202400493`.
3. **Same logic, weaker case, for items 7, 8, 12** — a reseller eBook, a manual scrape, and a 2-page
   marketing brief, together 1.48 MB, all trivially re-downloadable.
4. **Items 1 and 15 are the ones worth actually carrying.** Item 1 is GPL and belongs beside the code it
   documents; item 15 is a 15-page paper with a real text layer and a methodology a future engineer would
   otherwise have to reconstruct.

**Corrected arithmetic — an earlier draft's "59.6 MiB → ~7.4 MiB" does not reproduce and neither figure is
right.** `research/literature/` never holds 59.6 MiB: that is the size of the whole `literature/` tree, and
under §2.2's routing items 1, 2, 4, 5, 8, 9, 10 go to `flight_controller` and item 14 to `engineer360`
before this store is built. What `research/literature/` actually holds:

| Stage | Contents | Bytes | MiB |
|---|---|---:|---:|
| As routed | items 3, 6, 7, 11, 12, 13, 15 + the 4 `docs/literature/` docs | 10,627,988 | **10.14** |
| After dropping items 7 and 12 (rec 3) | items 3, 6, 11, 13, 15 + the 4 docs | 9,375,821 | **8.94** |

So the saving from recs (2) and (3) *inside `research/literature/`* is **1.19 MiB**, not 52 MiB — because
the two big PDFs were never going there. Rec 2's real effect is on `engineer360` (−26.2 MiB) and rec 3's
item 8 on `flight_controller` (−0.22 MiB). Across the **whole migration**, dropping items 14, 1, 7, 8 and
12 takes the 62,513,908 B corpus to 9,509,375 B = **9.07 MiB**. The 23.0 MiB dRehmFlight manual (item 1)
lands in `flight_controller` if it is kept, where a clone is expected to be heavy anyway.

---

## 6. Execution order (human-only — no git writes proposed here)

1. **Delete the 4 duplicate paths first** (§1.5), before any copy — otherwise the duplicate travels and
   has to be found again on the far side.
2. **Copy `serial-rich-text-formatting.md` out to `flight_controller/findings/` explicitly and by name**,
   not as part of a `docs/literature/` sweep. It is the file most likely to be lost.
3. **Then the pins** into `flight_controller` — items 1, 2, 4, 5, 8, 9, 10 (7 files across
   `vendored/dRehmFlight/{,docs/}`, `docs/history/`, `docs/references/`, `hardware/{,rc/}`) — then
   **item 14** to `engineer360`, and the remaining **11 items** (7 from `literature/`, 4 from
   `docs/literature/`) to `research/literature/`.
4. **Write `research/literature/INDEX.md` in the same wave**, with the `Pinned-to` column. Per the
   docs-and-planning rule, every docs subdir gets an INDEX in the wave that creates it — and here the
   INDEX is load-bearing, not decorative: it is the only record that a `flight_controller` datasheet is
   part of the literature corpus.
5. **Do not carry `scope.md` forward as the governing scope** (§4, item 19). Write a new one describing
   the store that actually exists.

Gate 3 of `11_routing_v2` (the ELF-binary content filter) does not apply to this content — no executables
in either tree. Gate 4 (floppi's `post-commit` hook firing ResearchHub) **does**: any operator commit in
floppi, including one that deletes the four duplicate paths, backgrounds
`python3 /home/devel/researchhub/scripts/backup/cli.py repo` on a memory-constrained host. Check free
memory first.
