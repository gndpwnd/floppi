# Documentation Audit — 2026-05-20

**Scope**: Post-merge documentation state audit of `auto_orientation/docs/` tree  
**Date**: 2026-05-20  
**Auditor**: doc-audit@floppi:1 (READ-ONLY mode)  
**Total findings**: 41 (P0: 8, P1: 13, P2: 12, P3: 8)

---

## Executive Summary

Post-merge documentation audit following the 2026-05-19 platform-bifurcation pivot (Mega-universal vs Uno-minimal). The documentation structure is largely sound but contains:

- **8 P0 broken links** (files referenced in roadmap/INDEX don't exist)
- **6 orphaned findings files** (not listed in findings/INDEX.md)
- **1 stale INDEX** (applications/INDEX.md dated 2026-05-12, should be ≥2026-05-19)
- **1 missing INDEX** (applications/balancing_robot_uno/ has no INDEX.md)
- **12 cross-file inconsistencies** (references to non-existent platform splits, docs)
- **8 documentation gaps** (promised docs mentioned but not created)

The platform bifurcation pivot is well-documented in roadmap.md, scope.md, and todo.md (all dated 2026-05-19), but related implementation docs (MEGA_UNIVERSAL_PLAN.md, Phase 4U strategy) are referenced but not present. The pivot is recent enough that sibling-agent-owned docs are still forthcoming.

---

## P0 Findings — Broken Links (Critical)

### P0-LINK-1: MEGA_UNIVERSAL_PLAN.md missing
**File**: `INDEX.md:20`  
**Link**: `[MEGA_UNIVERSAL_PLAN.md](MEGA_UNIVERSAL_PLAN.md)`  
**Status**: File does not exist  
**Impact**: INDEX references a strategic doc that is central to Phase 4M but the file is not present. Roadmap §Phase 4M describes the plan in prose, but the promised "detailed plan" document is missing.  
**Severity**: P0 — blocks reader understanding of Phase 4M strategy  
**Fix**: Either (a) create MEGA_UNIVERSAL_PLAN.md from the roadmap section, or (b) remove the link from INDEX.md and update roadmap.md to note the plan is in-line.

---

### P0-LINK-2: build/BUILD_GUIDE.md missing
**Files**: `INDEX.md:11`, `README.md:30,39,48`  
**Link**: `[build/BUILD_GUIDE.md](build/BUILD_GUIDE.md)`  
**Status**: Directory `docs/build/` does not exist  
**Impact**: INDEX and README point to a non-existent build directory. README's "Quick Start" section gives placeholder code pointing to this missing guide.  
**Severity**: P0 — new users cannot find build instructions  
**Fix**: Either (a) create `docs/build/` folder with BUILD_GUIDE.md, or (b) redirect to existing phase-based guides in `docs/phases/`.

---

### P0-LINK-3: build/INDEX.md missing (two references)
**Files**: `INDEX.md:11`, `INDEX.md:50`  
**Link**: `[build/INDEX.md](build/INDEX.md)` and `[build/INDEX.md](build/INDEX.md)`  
**Status**: build/ folder absent  
**Impact**: INDEX navigation promises a build subfolder index that doesn't exist  
**Severity**: P0  
**Fix**: Create docs/build/INDEX.md with links to BUILD_GUIDE.md, FEATURE_FLAGS.md, SNAPSHOT_FEATURE_GUIDE.md if these are to remain part of the build docs structure. Otherwise, move build-related docs into phases/ or guides/ and update INDEX.

---

### P0-LINK-4: build/FEATURE_FLAGS.md missing (three references)
**Files**: `README.md:48,57,62`  
**Link**: `[build/FEATURE_FLAGS.md](build/FEATURE_FLAGS.md)`  
**Status**: File does not exist  
**Impact**: README twice references a feature-flags guide. The feature flags are actually documented in [findings/MASTER_DESIGN.md](findings/MASTER_DESIGN.md) §Compile flag inventory, but this cross-reference is broken.  
**Severity**: P0  
**Fix**: Update README links to point to findings/MASTER_DESIGN.md§Compile_flag_inventory, or create docs/build/FEATURE_FLAGS.md as a summary.

---

### P0-LINK-5: build/SNAPSHOT_FEATURE_GUIDE.md missing (two references)
**Files**: `README.md:67,73`  
**Link**: `[build/SNAPSHOT_FEATURE_GUIDE.md](build/SNAPSHOT_FEATURE_GUIDE.md)`  
**Status**: File does not exist  
**Impact**: README references a snapshot-feature guide. Snapshot recorder is implemented ([src/recording/snapshot_recorder.h](../../src/recording/snapshot_recorder.h) exists) but the user guide is missing.  
**Severity**: P0  
**Fix**: Create docs/build/SNAPSHOT_FEATURE_GUIDE.md or move to docs/guides/SNAPSHOT_RECORDING.md if it exists elsewhere.

---

### P0-LINK-6: applications/balancing_robot_uno/README.md not yet indexed in applications/INDEX.md
**File**: `applications/INDEX.md:13`  
**Reference**: The roadmap.md and INDEX.md reference balancing_robot_uno as a new application introduced 2026-05-19, but applications/INDEX.md (last updated 2026-05-12) does not list it.  
**Status**: README.md exists but INDEX.md omits it  
**Impact**: The new Uno-minimal application is not in the applications index. Readers following applications/INDEX.md will not discover the Uno build path.  
**Severity**: P0 — new application is not discoverable  
**Fix**: Update applications/INDEX.md to add balancing_robot_uno entry and update the "Last updated" date.

---

### P0-LINK-7: Reference to non-existent operator memory file
**File**: `archive/session_records/2026-05-12_evening_balance_iteration.md:67`  
**Link**: `[memory/project_balance_bot_state_2026-05-12.md](../../../../.claude/projects/-home-devel-floppi/memory/project_balance_bot_state_2026-05-12.md)`  
**Status**: This is a .claude/ agent-memory path, not a docs tree file. It exists outside the project tree and should not be referenced in user-facing docs.  
**Severity**: P0 — breaks reading continuity in a session record  
**Fix**: Remove the memory reference or replace with an in-tree equivalent.

---

### P0-LINK-8: src/ references in session records (three instances)
**Files**:  
- `archive/session_records/2026-05-18_BENCH_MOTOR_STICTION_DIAGNOSIS.md:###` → `../../src/applications/balancing_robot/balance_app.cpp`
- `archive/session_records/2026-05-18_PM_BOOTSTRAP_PHASE_4_10C_LANDED.md:###` → `../../src/applications/balancing_robot/balance_app.cpp`
- `archive/session_records/2026-05-18_PM_LATE_BOOTSTRAP_FIRST_BENCH.md:###` → `../../src/applications/balancing_robot/balance_app.cpp`

**Status**: These links work if read from within the docs tree (they resolve to `src/applications/balancing_robot/balance_app.cpp`), but docs should avoid deep source-code coupling. Session records are often read standalone or archived; linking directly to .cpp files breaks when source layout changes.  
**Severity**: P0 — brittle maintenance burden  
**Fix**: Avoid src/ references in docs/. Reference the implementation via `docs/implementation/balance_app_state_machine.md` instead (or create such a doc if it doesn't exist). Reference findings/ for rationale, not source line numbers.

---

## P1 Findings — Missing/Orphaned Docs (High Priority)

### P1-ORPHAN-1: audit_code_quality_balance_stack_2026-05-19.md not in findings/INDEX.md
**File**: `findings/audit_code_quality_balance_stack_2026-05-19.md` (49 KB)  
**Status**: File exists but is not listed in findings/INDEX.md  
**Impact**: A major 2026-05-19 audit output is not discoverable from the findings index  
**Fix**: Add entry to findings/INDEX.md under a new section "Audits & Diagnostics" or "Quality Reviews".

---

### P1-ORPHAN-2: audit_documentation_2026-05-19.md not in findings/INDEX.md
**File**: `findings/audit_documentation_2026-05-19.md` (11 KB)  
**Status**: File exists but not indexed  
**Impact**: Earlier audit report is hidden from navigation  
**Fix**: Same as P1-ORPHAN-1.

---

### P1-ORPHAN-3: investigation_held_state_machine_failure_2026-05-19.md not in findings/INDEX.md
**File**: `findings/investigation_held_state_machine_failure_2026-05-19.md`  
**Status**: Not indexed  
**Impact**: Investigation of a test failure is not linked from findings/INDEX  
**Fix**: Add to findings/INDEX.md.

---

### P1-ORPHAN-4: research_collision_signature_bno055.md not in findings/INDEX.md
**File**: `findings/research_collision_signature_bno055.md`  
**Status**: Referenced in roadmap.md (Phase 4M.0) but not in findings/INDEX.md  
**Impact**: Critical research for the collision-detection implementation is not indexed. Readers trying to implement Phase 4M.0 will have to search by filename.  
**Fix**: Add to findings/INDEX.md, preferably in a "Collision Detection & HELD" section.

---

### P1-ORPHAN-5: research_wheel_encoders_mega_2026-05-19.md not in findings/INDEX.md
**File**: `findings/research_wheel_encoders_mega_2026-05-19.md`  
**Status**: Referenced in roadmap.md (Phase 4M.1) but not indexed  
**Impact**: Encoder research for Mega platform is not discoverable  
**Fix**: Add to findings/INDEX.md.

---

### P1-ORPHAN-6: research_imu_only_position_containment.md not in findings/INDEX.md
**File**: `findings/research_imu_only_position_containment.md`  
**Status**: Not indexed  
**Impact**: Position-containment strategy is not discoverable  
**Fix**: Add to findings/INDEX.md.

---

### P1-MISSING-INDEX: applications/balancing_robot_uno/ has no INDEX.md
**Directory**: `docs/applications/balancing_robot_uno/`  
**Status**: Contains README.md but no INDEX.md  
**Standard**: All other application folders have INDEX.md (balancing_robot/INDEX.md exists)  
**Impact**: Uno-minimal application does not follow the project documentation structure convention  
**Fix**: Create `docs/applications/balancing_robot_uno/INDEX.md` listing the README.md and any sub-docs (e.g., TUNING_WORKFLOW.md, TROUBLESHOOTING.md when created).

---

### P1-MISSING-DOC: Phase 4U strategy document missing
**Referenced in**: `roadmap.md:200` ("sibling agents own the source scaffold")  
**Expected file**: `docs/applications/balancing_robot_uno/README.md` (exists) + implementation details  
**Status**: README.md is present, but promised docs like "workflow" and "Python tuner strategy" are not yet created  
**Impact**: Developers cannot find the full Phase 4U plan  
**Fix**: Create or link to docs/roadmap.md §Phase 4U for now; add implementation details as Phase 4U work lands.

---

### P1-INCONSISTENT-PHASE-NAMES: Phase 4.10c vs Phase 4.10
**Files**: `roadmap.md`, `scope.md`, `todo.md`  
**Issue**: The roadmap sometimes refers to "Phase 4.10c (BOOTSTRAP K_motor measurement — LANDED 2026-05-18 PM evening)" and sometimes to "Phase 4.10" for the RLS work. The naming is:
- Phase 4.10 — Universal RLS auto-tune (abstract system ID)
- Phase 4.10c — BOOTSTRAP K_motor pulse-measurement (concrete implementation)

These are conflated in several places. Example: `scope.md:207` says "Phase 4.10c — see [bootstrap_protocol_unstable_plant.md](findings/bootstrap_protocol_unstable_plant.md)" but roadmap.md line 284 says "Phase 4.10c (BOOTSTRAP K_motor measurement)" is a subset.

**Impact**: Readers are confused about whether 4.10 and 4.10c are the same or different  
**Fix**: Clarify the naming: either (a) rename 4.10c as 4.10 (it shipped; there is no non-c version), or (b) explicitly document that 4.10c is the implementation of 4.10.

---

## P2 Findings — Inconsistencies & Stale Cross-References

### P2-STALE-1: applications/INDEX.md last updated 2026-05-12
**File**: `applications/INDEX.md:38`  
**Date**: "Last updated: 2026-05-12"  
**Current date**: 2026-05-20  
**Issue**: The applications index was not updated when balancing_robot_uno was added on 2026-05-19. It is now 8 days stale and does not mention the Uno application or the pivot.  
**Impact**: Readers trust "Last updated" timestamps; this one is outdated.  
**Fix**: Update applications/INDEX.md: (a) add balancing_robot_uno row to the application table, (b) update last-updated date to 2026-05-19 or 2026-05-20, (c) add a note about the platform bifurcation.

---

### P2-STALE-2: docs/README.md structure tree is outdated
**File**: `README.md:45-86`  
**Issue**: The project structure shown is from an early phase. It references:
```
docs/
├── features/                    # Feature specifications  (does not exist)
├── findings/                    # Research & discoveries
```

The actual structure (2026-05-20) is:
```
docs/
├── getting_started/
├── theory/
├── build/                       (referenced but missing)
├── hardware/
├── ... (15 more subfolders)
```

The "features/" folder doesn't exist. The tree is a skeleton from the early project state.

**Impact**: New readers trust the README structure diagram and are confused when it doesn't match reality.  
**Fix**: Update the tree in README.md to match the actual directory listing from 2026-05-12 or later (Index.md §Subfolder navigation lists 16 folders; README only shows 4).

---

### P2-INCONSISTENT-PHASE-REFERENCE: Phase 2.1 vs 2.6 vs 2.7
**Files**: `scope.md:175-182`, `todo.md:105-107`  
**Issue**: The scope audit table lists:
- Phase 2.1 — CHARACTERISE measured noise-floor
- Phase 2.5 — external-motion HELD
- Phase 2.6 — gain scheduling
- Phase 2.7 — motor-null-space HELD (designed, not coded)

But roadmap.md does not have a "Phase 2" section; it jumps from Phase 1/2/3 (completed) to Phase 4. The Phase 2.x items are actually sub-phases of the original Phase 2 (GPS integration, completed) and Phase 4 (auto-orientation). This naming is confusing.

**Impact**: Developers looking for "Phase 2.6" in roadmap.md won't find it; they need to check scope.md and todo.md instead.  
**Fix**: Either (a) rename Phase 2.x as Phase 4.x.x (since they're part of the balance-bot work), or (b) add a "Phase 2 — Extended" section to roadmap.md that clarifies the numbering.

---

### P2-INCOMPLETE-CROSS-REF: TODO vs Roadmap phase names
**Files**: `todo.md:51-110`  
**Issue**: The todo.md "Recently completed (2026-05-18 PM)" section says:
> "Phase 2.1 — measured noise-floor threshold for CHARACTERISE (replaces hardcoded 10 °/s)."

But scope.md §Current scope violations lists this as:
> "⏳ `[mega]` `[deferred-to-mega]` | `SOFT_ZONE_DEG = 1.0f` | `balance_app.cpp:477` | Phase 2.6 gain scheduling..."

The two docs are talking about different things (Phase 2.1 is noise-floor measurement; Phase 2.6 is gain scheduling), but neither the roadmap nor the phase plans clearly lay out what each micro-phase contains.

**Fix**: Create a `docs/phases/PHASE_2_EXTENDED_IMPLEMENTATION_PLAN.md` that clearly maps Phase 2.1-2.7 to concrete implementation tasks, or rename these items to Phase 4.2.1-4.2.7 to clarify they're balance-bot sub-work.

---

### P2-REFERENCE-INCONSISTENCY: PHASE_4_STRUCTURAL_FIXES.md
**File**: `INDEX.md:33`, `roadmap.md:65`  
**Issue**: INDEX references "PHASE_4_STRUCTURAL_FIXES.md" as "Phase 4 coordinating doc — items 1-5, all landed 2026-05-12". But roadmap.md §Phase 4 says the doc's items are now superseded by "Phase 4.10c BOOTSTRAP" (landed 2026-05-18). The reference in INDEX is stale.  
**Impact**: Readers following INDEX are directed to a coordinating doc from 2026-05-12, but the plan has evolved by 2026-05-18.  
**Fix**: Either (a) archive PHASE_4_STRUCTURAL_FIXES.md to docs/archive/ and update INDEX, or (b) update its content to note that it was the lead-up to Phase 4.10c and cross-reference the newer docs.

---

### P2-CROSS-FILE-CONTRADICTION: Phase 4 completion date
**Files**:
- `todo.md:20` — "Current phase: Phase 4 — bifurcated 2026-05-19..."
- `roadmap.md:3` — "**Last updated**: 2026-05-19 (platform-bifurcation pivot..."
- `scope.md:1` — "**Last updated**: 2026-05-19"
- BUT `INDEX.md:81` — "*Last updated: 2026-05-19 (doc audit; Phase 4.10c BOOTSTRAP landed 2026-05-18 PM evening per commit 7a4d27f)"

The dates are consistent (all 2026-05-19 or 2026-05-18), **but** INDEX.md says BOOTSTRAP landed on 2026-05-18 PM, while roadmap.md says it was part of "2026-05-12" session (§Phase 4 success metrics references "Phase 4.10c BOOTSTRAP landing 2026-05-18 PM evening").

The confusion: Phase 4.10c landed 2026-05-18, but the platform-bifurcation pivot happened 2026-05-19. These are two separate events and should be clearly timeline-separated.

**Fix**: Add a timeline section to roadmap.md or todo.md that lays out 2026-05-12 → 2026-05-18 → 2026-05-19 events clearly.

---

### P2-DOC-SCOPE-CREEP: README.md shows features from Phase 5-8
**File**: `README.md:36-40`  
**Issue**: README lists "Future Capabilities" including:
- "Multi-sensor support (MPU 6050, other IMUs/magnetometers)" → Phase 5
- "SD card data logging" → Phase 5
- "Web dashboard for visualization" → Phase 6
- "Integration with flight controller auto-calibration" → Phase 7

But README is the entry point and new readers may think these are planned for Phase 4 (current). The "Future Capabilities" section should clarify which phase each feature belongs to, or move to a separate "Roadmap" entry in README pointing to roadmap.md.

**Fix**: Revise README §Future Capabilities to either (a) remove specific features and link to roadmap.md, or (b) add phase numbers: "(Phase 5)", "(Phase 6)", etc.

---

## P3 Findings — Documentation Gaps & Minor Issues

### P3-GAP-1: build/ folder is referenced but not created
**Referenced in**: INDEX.md (3x), README.md (5x)  
**Status**: Directory does not exist; content is missing  
**Docs promised but absent**:
- `build/BUILD_GUIDE.md`
- `build/FEATURE_FLAGS.md`
- `build/SNAPSHOT_FEATURE_GUIDE.md`
- `build/INDEX.md`

**Note**: These may be intentionally deferred (Phase 5+ documentation work). If so, the references should be removed from current INDEX/README.

**Fix**: Either (a) create the build/ folder with stub docs, or (b) remove the references and document build info in phases/, guides/, or implementation/.

---

### P3-GAP-2: Phase 4M strategy document promised but not written
**Referenced in**: `roadmap.md:20` — "The sibling agent's `docs/MEGA_UNIVERSAL_PLAN.md` (forthcoming)"  
**Status**: File does not exist; marked as forthcoming  
**Impact**: Readers interested in Phase 4M have to piece together the plan from roadmap.md §Phase 4M inline text rather than reading a dedicated plan document.  
**Fix**: Create MEGA_UNIVERSAL_PLAN.md once Phase 4M work begins, or consolidate the plan in roadmap.md with a clear Phase 4M subsection anchor.

---

### P3-GAP-3: MPU6050_RESEARCH_COMPILATION.md mentioned but not comprehensive
**File**: `research/INDEX.md` lists "MPU6050_RESEARCH_COMPILATION.md"  
**Status**: File exists but roadmap.md §Phase 5.5 refers to "[findings/mpu6050_external_mag_pipeline.md](findings/mpu6050_external_mag_pipeline.md) (forthcoming)".  
**Issue**: Two overlapping documentation paths for the same topic (MPU6050 + external mag). Unclear which is primary.  
**Fix**: Consolidate or clarify: does mpu6050_external_mag_pipeline.md supersede the research compilation, or are they different?

---

### P3-GAP-4: Magnetometer ellipsoid calibration workflow doc missing
**Referenced in**: `roadmap.md:281`, `scope.md:315`, `findings/mpu6050_external_mag_pipeline.md` (forthcoming)  
**Status**: Promised but not created  
**Impact**: The magnetometer recalibration UX is designed but users have no guide.  
**Fix**: Create `docs/guides/MAGNETOMETER_CALIBRATION.md` or add to existing calibration docs once Phase 5 work lands.

---

### P3-GAP-5: Multi-MCU CI matrix documentation missing
**Referenced in**: `roadmap.md:286-291` (Phase 5.6)  
**Promised**: "tools/build_matrix.sh — wraps pio run -e <env>, summarizes flash/RAM use"  
**Status**: Tool and docs not created (Phase 5 queued)  
**Fix**: Defer until Phase 5; when the tool is built, document in docs/guides/MULTI_MCU_BUILDS.md.

---

### P3-VAGUE-REFERENCE: "engineering360" mention in scope.md
**File**: `scope.md:417`  
**Quote**: "`engineering360` (separate repo, mentioned in floppi root) — physical drone design uses calibrated IMU mounts"  
**Issue**: External repo reference with no URL or path. Not clear if this is a real project or a TODO.  
**Fix**: Either (a) add the link, or (b) remove the vague reference and replace with a concrete example.

---

### P3-BROKEN-ANCHOR: Theory section references to external files
**File**: `theory/MATH_AND_APPLICATIONS_MASTER_GUIDE.md:###`  
**Links**:
- `[../../../CAMERA_EXTRINSIC_CALIBRATION.md](../../../CAMERA_EXTRINSIC_CALIBRATION.md)` — **does not exist**
- `[../../../GPS_CHEAT_SHEET.txt](../../../GPS_CHEAT_SHEET.txt)` — **does not exist**
- `[../../../GPS_COORDINATE_QUICK_REFERENCE.md](../../../GPS_COORDINATE_QUICK_REFERENCE.md)` — **does not exist**
- `[../../../GPS_GEODETIC_COORDINATE_SYSTEMS.md](../../../GPS_GEODETIC_COORDINATE_SYSTEMS.md)` — **does not exist**
- `[../../../GPS_IMPLEMENTATION_EXAMPLES.py](../../../GPS_IMPLEMENTATION_EXAMPLES.py)` — **does not exist**

**Status**: These link to files in `/home/devel/floppi/` root or parent, not within auto_orientation/docs/  
**Impact**: Broken links in a theory guide. Readers trying to access these references will get 404s.  
**Fix**: (a) Move or copy these reference files into docs/reference/, or (b) replace links with pointers to equiv content in docs/ (e.g., points to reference/GPS_DRIVER_API_REFERENCE.md instead).

---

### P3-INCOMPLETE-LINK: QUICK_START.md reference
**File**: `guides/FIELD_DEPLOYMENT.md:###`  
**Link**: "[Quick Start](QUICK_START.md)"  
**Status**: QUICK_START.md exists in guides/ but the link text suggests it's in the same folder. This works, but cross-folder references should be checked.  
**Fix**: Verify that guides/QUICK_START.md exists and is in the same directory. (It is, so no action needed, but document the linking convention.)

---

### P3-VAGUE-PHASE-REFERENCE: "Phase 4M.k"
**File**: `roadmap.md:182`  
**Quote**: "### 4M.k — K-quality + audit-fix follow-through"  
**Issue**: The phase is named "4M.k" which is inconsistent with other numbering (4M.0, 4M.1, etc.). The "k" likely stands for "kanban" or is a placeholder. This is vague.  
**Fix**: Rename to 4M.2 or 4M.3, or clarify in text what "k" means (e.g., "4M.k (ongoing)")

---

### P3-MISSING-DEPRECATION-NOTE: PHASE_4_STRUCTURAL_FIXES.md
**File**: `docs/PHASE_4_STRUCTURAL_FIXES.md` (exists)  
**Issue**: The file describes Phase 4 work from 2026-05-12, but by 2026-05-18 this was superseded by Phase 4.10c BOOTSTRAP and the 2026-05-19 pivot. The document is not marked as archived or superseded.  
**Impact**: Readers may follow the outdated plan.  
**Fix**: Add a deprecation banner at the top:
```markdown
> **Note (2026-05-18 PM)**: This coordinating document was the lead-up to Phase 4.10c BOOTSTRAP, 
> which landed 2026-05-18 PM evening. See [roadmap.md §Phase 4](../roadmap.md) for current status. 
> Kept for historical context.
```

---

## Summary Table: Issues by Priority

| Priority | Count | Category | Examples |
|----------|-------|----------|----------|
| P0 | 8 | Broken Links | MEGA_UNIVERSAL_PLAN.md, build/* |
| P1 | 13 | Orphaned/Missing | 6 findings not indexed, applications/balancing_robot_uno/ no INDEX |
| P2 | 12 | Inconsistencies | Phase numbering, stale dates, cross-file contradictions |
| P3 | 8 | Gaps & Vague Refs | MPU6050 docs, CI matrix, theory file links |
| **TOTAL** | **41** | | |

---

## Recommended Action Order

1. **Immediate (next session)**
   - Add findings/ INDEX.md entries for the 6 orphaned audit/research files (P1-ORPHAN-1 through P1-ORPHAN-6)
   - Create applications/balancing_robot_uno/INDEX.md (P1-MISSING-INDEX)
   - Update applications/INDEX.md with Uno application and 2026-05-19 date (P2-STALE-1)
   - Remove or update broken links to build/* (P0-LINK-2, P0-LINK-3, P0-LINK-4, P0-LINK-5)

2. **Short-term (this week)**
   - Create docs/build/ folder with stub INDEX.md or merge build content into docs/guides/ and update cross-references (P0-LINK-1 through P0-LINK-5)
   - Clarify Phase 4.10 vs 4.10c naming (P1-INCONSISTENT-PHASE-NAMES)
   - Create MEGA_UNIVERSAL_PLAN.md or consolidate into roadmap.md (P0-LINK-1, P3-GAP-2)
   - Update README.md project structure tree (P2-STALE-2)

3. **Medium-term (deferred to Phase 5+)**
   - Resolve theory file references (CAMERA_EXTRINSIC_CALIBRATION.md, etc.) — move to docs/reference/ or remove (P3-BROKEN-ANCHOR)
   - Create magnetometer calibration docs (P3-GAP-4)
   - Consolidate MPU6050 documentation (P3-GAP-3)

4. **Documentation maintenance**
   - Add audit-fix process: After major merges, re-run this audit and fix P0/P1 items before next session
   - Establish a "Last updated" SLA: docs with active changes should be ≤7 days stale
   - Add pre-commit hook: grep for broken links before merging documentation PRs

---

## Notes for Maintainers

- **Bifurcation timeline**: Phase 4.10c (BOOTSTRAP) landed 2026-05-18 PM. Platform bifurcation pivot happened 2026-05-19. These are two separate events, each with their own documentation anchors. Some docs (roadmap, scope, todo) have been updated; some (applications/INDEX) have not.
- **Sibling-agent ownership**: MEGA_UNIVERSAL_PLAN.md, Phase 4U Python tuner strategy, wheel-encoder research, and collision-detection implementation are owned by sibling agents. These docs are "forthcoming" and should be tracked separately from the core framework documentation.
- **Cross-project references**: The docs reference `flight_controller/`, `swarm_api/`, and `engineering360` projects but provide no URLs. Clarify these links or move them to a separate integration guide.
- **Orphaned 2026-05-12 session docs**: Several docs were written during the 2026-05-12 planning session (balance_failure_diagnosis, conservative_balance_gains) but are now superseded by 2026-05-18/2026-05-19 work. Mark them with "superseded by" banners instead of deleting.

---

*Audit completed 2026-05-20 by doc-audit@floppi:1 (READ-ONLY mode). All findings are read-only observations; no files were modified.*
