# Documentation Migration Summary

**Date:** 2026-01-11
**Scope Refactor:** Focusing floppi on firmware and flight computer integration only

---

## What Changed

The floppi project scope has been **clarified and narrowed** to focus exclusively on:

- Flight controller firmware development (dRehmFlight-based)
- Flight computer integration (ESP32, Raspberry Pi)
- Sensor integration and calibration
- Testing and validation procedures

All physical design, mechanical optimization, and calculation tools have been **moved to engineering360**.

---

## Documentation Changes

### Updated Files

#### `/docs/scope.md` (Version 2.0)
**Changes:**
- Redefined mission statement to focus on firmware and flight computer integration
- Added clear "In Scope" vs "Out of Scope" sections
- Added explicit relationship to engineering360 project
- Removed references to:
  - Optimization tools and calculators
  - 3D modeling and CAD design
  - Literature RAG system
  - Physical design and mechanical engineering

**Key Addition:**
- "Relationship to engineering360" section explaining the division of responsibilities

#### `/docs/ROADMAP.md` (Version 2.0)
**Changes:**
- Removed Phase 3: "VTOL Optimization Calculator"
- Removed Phase 5: "Reference Platform Drone" (physical design aspects)
- Removed literature/RAG development phases
- Renumbered phases to focus on:
  - Phase 1-2: Flight Controller Core & Advanced Control
  - Phase 3: ESP32 Flight Computer Integration
  - Phase 4: Raspberry Pi Flight Computer Integration
  - Phase 5: Swarm Coordination
  - Phase 6: Advanced Features & Optimization
- Added "Relationship to engineering360" section
- Updated all references to remove calculator and physical design milestones

### New Files

#### `/docs/README.md`
- Navigation guide for documentation
- Clear scope statement
- Links to key documents
- Out-of-scope topics listed with reference to engineering360

#### `/docs/_archived/README.md`
- Explains what was archived and why
- Directs readers to engineering360 for archived topics

### Archived Directories

Moved to `/docs/_archived/`:

1. **`findings/`** - RAG system implementation guides
   - `rag-architecture.md`
   - `pgvector-setup.md`
   - `implementation-roadmap.md`
   - `pdf-processing-guide.md`
   - `hpc-ollama-setup.md`
   - `ollama-embeddings.md`
   - `fastmcp-guide.md`

2. **`literature/`** - Literature collection and management
   - Scope and roadmap for literature RAG system
   - Findings directory (textbook recommendations)

3. **`reference-platform/`** - Physical platform design
   - Scope and roadmap for reference drone builds

**Reason for archiving:** These topics are now handled by the **engineering360** project.

### Retained Directories

#### `/docs/flight-controller/`
**Status:** Active - core to floppi mission
**Contents:**
- Firmware setup guides
- Configuration documentation
- Calibration procedures
- Scope and roadmap specific to flight controller firmware

#### `/docs/flight-computer/`
**Status:** Active - core to floppi mission
**Contents:**
- ESP32 integration guides
- Raspberry Pi setup
- Communication protocols
- Autonomous navigation

---

## Project Relationship

### floppi (This Project)
**Repository:** `/home/devel/Desktop/floppi`
**Focus:** Firmware and software
**Deliverables:**
- Flight controller firmware (C/C++)
- Flight computer integration software (Python, C++)
- Communication protocols
- Sensor drivers and calibration tools
- Testing frameworks

### engineering360 (Companion Project)
**Repository:** `/home/devel/Desktop/engineering360` (or separate GitHub repo)
**Focus:** Physical design and optimization
**Deliverables:**
- CAD models and 3D printable files
- VTOL performance calculators
- Propulsion system optimization tools
- Structural analysis
- Component selection guides
- Literature RAG system for textbooks and research papers

### Workflow Between Projects

```
┌─────────────────────────────────────────────────────────────┐
│                     engineering360                          │
│  - Designs drone frame                                      │
│  - Selects motors, props, battery                           │
│  - Calculates expected performance                          │
│  - Provides specifications                                  │
└────────────────────┬────────────────────────────────────────┘
                     │
                     │ Specs: weight, dimensions,
                     │        motor/prop config,
                     │        expected thrust
                     ▼
┌─────────────────────────────────────────────────────────────┐
│                         floppi                              │
│  - Implements firmware for specified hardware               │
│  - Configures PID tuning for frame characteristics         │
│  - Integrates sensors and flight computer                   │
│  - Tests and validates flight performance                   │
└────────────────────┬────────────────────────────────────────┘
                     │
                     │ Flight test data:
                     │   actual performance,
                     │   handling characteristics,
                     │   power consumption
                     ▼
┌─────────────────────────────────────────────────────────────┐
│                     engineering360                          │
│  - Uses flight data to refine design                        │
│  - Iterates on frame, propulsion, structure                 │
│  - Updates calculations and recommendations                 │
└─────────────────────────────────────────────────────────────┘
```

---

## Why This Change?

### Before (Unclear Scope)
- floppi tried to be "everything drone"
- Mixed firmware, physical design, calculators, and literature
- Difficult to navigate and contribute to
- Unclear what belonged where

### After (Clear Scope)
- **floppi = firmware and software**
- **engineering360 = physical design and calculations**
- Clear separation of concerns
- Easier for contributors to know where to contribute
- Better project focus and maintainability

---

## For Users and Contributors

### If You're Working on Firmware
✅ **Stay in floppi**
- Flight controller firmware development
- Sensor integration
- Flight computer communication
- PID tuning and control algorithms
- Testing procedures

### If You're Working on Physical Design
➡️ **Move to engineering360**
- Frame design and CAD
- Propulsion calculations
- Component selection tools
- Structural analysis
- VTOL performance optimization
- Literature and research compilation

### If You Need Both
🔄 **Use both projects together**
- Start with engineering360 to design your drone
- Use floppi firmware to fly it
- Share flight data back to engineering360 to refine design

---

## Impact on Existing Work

### What's Still Valid in floppi
- All `/flight_controller/` firmware code (100% in scope)
- Power system wiring guides (needed for firmware integration)
- Feature comparisons (firmware-focused)
- Flight computer integration plans (100% in scope)

### What Moved to engineering360
- `/darpa_lift_2026/` calculator work
- `/optimization_tools/` Python calculators
- `/3D_files/` mechanical designs
- `/literature/` textbook collection
- RAG system for research papers

### What's Archived (Reference Only)
- Documentation in `/docs/_archived/`
- Kept for historical reference
- Not actively maintained in floppi
- May be migrated to engineering360 if needed

---

## Next Steps

### For floppi
1. Continue Phase 1 flight controller development
2. Complete hardware bench testing
3. Begin ESP32 flight computer integration planning
4. Expand flight-controller and flight-computer documentation

### For engineering360
1. Set up new repository structure
2. Migrate calculator code and documentation
3. Migrate 3D files and CAD models
4. Set up literature RAG system
5. Create clear scope and roadmap for physical design focus

---

## Questions?

- **Firmware/software questions:** floppi GitHub Issues
- **Physical design questions:** engineering360 GitHub Issues
- **Not sure which project?** Check the scope documents:
  - [floppi scope.md](./scope.md)
  - engineering360 scope.md (in engineering360 repo)

---

**Document Version:** 1.0
**Last Updated:** 2026-01-11
