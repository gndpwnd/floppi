# Repository Organization Summary

**Date:** 2026-05-05  
**Task:** Organize stray files at repository root into logical locations

---

## Organization Decisions & Rationale

### Files Moved to `docs/findings/`

These files contain research findings, documentation, and analysis related to specific technical investigations:

| File | Reason |
|------|--------|
| `BOOTLOADER_RECOVERY_SUMMARY.md` | Bootloader investigation overview and summary |
| `BOOTLOADER_FILES.txt` | Index/quick reference for bootloader recovery docs |
| `DELIVERABLES_CHECKLIST.txt` | Project deliverables tracking for bootloader work |
| `marine_core_drone.md` | Research reference about Marine 3D-printed drones |

**Location:** `/docs/findings/` contains research findings, technical investigations, and discovery documentation.

### Files Moved to `tools/`

These are utility scripts and reusable tools that support project development:

| File | Purpose |
|------|---------|
| `researchhub_client.py` | ResearchHub API client - reusable Python client for research integration |
| `setup_permissions.sh` | System permissions setup script for udev rules and USB device access |

**Location:** `/tools/` contains scripts and utilities that support development workflows.

### Files Moved to `docs/todo/`

Task and progress tracking documentation:

| File | New Name | Purpose |
|------|----------|---------|
| `todo.md` | `TASKS.md` | Project task list with work-in-progress items |

**Location:** `/docs/todo/` is dedicated to task lists and progress tracking.

**Naming convention:** Use clear names like `TASKS.md` rather than generic `todo.md` for discoverability.

### Files Moved to `docs/`

High-level documentation that applies to the entire project:

| File | Purpose |
|------|---------|
| `SCOPE_REFACTOR_COMPLETE.md` | Project scope refactor summary and status |

**Location:** `/docs/` contains top-level project documentation.

### Files Kept at Root

| File | Reason |
|------|--------|
| `README.md` | Main project overview - belongs at repository root |

**Criterion:** Root README should contain only high-level project overview, quick start, and links to detailed documentation. Current README.md meets this criterion and has been reviewed.

### Files Deleted

| File | Reason |
|------|--------|
| `connections.md` | Internal setup notes (SSH/rsync commands) - not project documentation |

**Criterion:** Internal developer notes about local machine connections don't belong in version control.

---

## Directory Structure Reference

```
/home/devel/floppi/
├── README.md                          (root level: main project overview)
│
├── docs/
│   ├── ORGANIZATION_SUMMARY.md        (this file - organization guide)
│   ├── SCOPE_REFACTOR_COMPLETE.md     (project scope documentation)
│   ├── scope.md                       (project scope v2.0)
│   ├── ROADMAP.md                     (development roadmap)
│   ├── MIGRATION_SUMMARY.md           (scope refactor details)
│   ├── README.md                      (docs navigation guide)
│   │
│   ├── findings/                      (research and technical investigations)
│   │   ├── BOOTLOADER_RECOVERY_SUMMARY.md
│   │   ├── BOOTLOADER_FILES.txt
│   │   ├── DELIVERABLES_CHECKLIST.txt
│   │   ├── marine_core_drone.md
│   │   ├── README_BOOTLOADER.md
│   │   ├── bootloader_recovery_guide.md
│   │   ├── bootloader_quick_reference.md
│   │   ├── bootloader_dtr_rts_analysis.md
│   │   └── [other research findings]
│   │
│   ├── todo/                          (task tracking)
│   │   └── TASKS.md                   (project tasks and progress)
│   │
│   ├── flight-controller/
│   └── flight-computer/
│
├── tools/                             (development utilities and scripts)
│   ├── recover_bootloader.sh          (bootloader recovery automation)
│   ├── researchhub_client.py          (ResearchHub API client)
│   ├── setup_permissions.sh           (system permissions setup)
│   └── [other tools]
│
├── flight_controller/                 (firmware and flight controller code)
├── flight_computer/                   (flight computer integration)
├── auto_orientation/                  (sub-project)
├── drone_3d_model/                    (3D model reference sub-project)
└── [other project directories]
```

---

## Organization Principles

### 1. Bootloader/Hardware Findings → `docs/findings/`

Technical investigations, research findings, and discovery documentation go in `/docs/findings/`. This includes:
- Hardware analysis and troubleshooting guides
- Technical investigations (DTR/RTS timing, USB driver behavior, etc.)
- Research references and related resources
- Checklists tracking investigation deliverables

### 2. Project Deliverables → `docs/` or `docs/findings/`

Project completion tracking and deliverables:
- Strategic project docs → `/docs/`
- Technical deliverables → `/docs/findings/`
- Task tracking → `/docs/todo/`

### 3. Tools/Scripts → `tools/`

Utility scripts and reusable tools:
- Automation scripts (recovery, setup, etc.)
- Reusable client libraries (ResearchHub client)
- Development helper utilities
- **All must have clear, descriptive names**

### 4. READMEs at Root

Only the main project README.md should be at repository root. It should contain:
- High-level project description
- Quick start guide
- Links to detailed documentation
- Hardware requirements

**It should NOT contain:**
- Internal setup notes or personal commands
- Temporary task lists (use `/docs/todo/`)
- Detailed technical investigations (use `/docs/findings/`)

### 5. Task/Todo Lists → `docs/todo/`

Work-in-progress and task tracking:
- Use clear names: `TASKS.md`, `IN_PROGRESS.md`, `BACKLOG.md`
- Avoid generic `todo.md` - it's not discoverable
- Include section headers for organization
- Date stamp for reference

---

## Files Updated for New Locations

When moving files with internal cross-references:
- ✅ `BOOTLOADER_RECOVERY_SUMMARY.md` - Updated file structure diagram to reflect new location
- ✅ `BOOTLOADER_FILES.txt` - Updated main documentation path reference

**Verify:** All path references in moved files point to correct locations after moving.

---

## Future Organization Guidelines

When adding new files to the root directory, ask:

1. **Is this a README?**
   - YES → Keep at root (but keep it high-level)
   - NO → Move to appropriate subdirectory

2. **Is this a technical investigation/finding?**
   - YES → `/docs/findings/`
   - NO → Continue

3. **Is this a utility script or tool?**
   - YES → `/tools/`
   - NO → Continue

4. **Is this a task list or progress tracking?**
   - YES → `/docs/todo/` (with clear naming)
   - NO → Continue

5. **Is this project scope, roadmap, or strategic docs?**
   - YES → `/docs/`
   - NO → Continue

6. **Is this internal personal setup notes?**
   - YES → Delete or keep in `.gitignore`
   - NO → Place in appropriate subdirectory

---

## Summary Statistics

**Files Organized:** 10 files
- Moved: 8 files
- Deleted: 1 file
- Kept at root: 1 file

**Directories Created/Used:**
- `docs/findings/` - Bootloader documentation and research
- `docs/todo/` - Task tracking
- `tools/` - Utility scripts

**Key Updates:**
- File structure diagrams in bootloader docs updated
- Main path reference in BOOTLOADER_FILES.txt updated
- All absolute paths verified to be correct
- Executable bit preserved on shell scripts

---

**Status:** ✅ Complete  
**Date:** 2026-05-05  
**Next Review:** Before adding more root-level files
