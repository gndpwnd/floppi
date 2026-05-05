# Scope Refactor Complete - Summary

**Date:** 2026-01-11
**Status:** ✅ Complete

---

## What Was Done

The floppi project has been refocused to be exclusively a **flight controller firmware and flight computer integration** project. All physical design, mechanical optimization, and calculation work has been clearly separated and designated for the **engineering360** companion project.

---

## Files Updated

### Core Documentation

1. **`/docs/scope.md`** (v2.0)
   - Completely rewritten to focus on firmware and flight computer integration
   - Clear "In Scope" vs "Out of Scope" sections
   - Added "Relationship to engineering360" section
   - Removed all references to physical design, calculators, and literature RAG

2. **`/docs/ROADMAP.md`** (v2.0)
   - Removed VTOL calculator phase
   - Removed reference platform physical design phase
   - Removed literature/RAG development phases
   - Renumbered to 6 firmware-focused phases:
     - Phase 1-2: Flight Controller Core & Advanced Control
     - Phase 3: ESP32 Flight Computer Integration
     - Phase 4: Raspberry Pi Flight Computer Integration
     - Phase 5: Swarm Coordination
     - Phase 6: Advanced Features & Optimization

3. **`/README.md`** (main project README)
   - Updated to reflect firmware-only focus
   - Added clear project description
   - Added quick start guide
   - Added reference to engineering360
   - Listed hardware requirements

### New Documentation

4. **`/docs/README.md`** (new)
   - Navigation guide for documentation
   - Clear scope statement
   - Directory structure explanation
   - Links to key documents

5. **`/docs/MIGRATION_SUMMARY.md`** (new)
   - Detailed explanation of what changed and why
   - Before/after scope comparison
   - Project relationship diagram
   - Guidance for users and contributors

6. **`/docs/_archived/README.md`** (new)
   - Explains what was archived
   - Directs users to engineering360 for archived topics

---

## Directory Structure Changes

### Archived Directories (moved to `/docs/_archived/`)

```
/docs/_archived/
├── README.md (explains what's archived and why)
├── findings/ (RAG system implementation guides)
│   ├── rag-architecture.md
│   ├── pgvector-setup.md
│   ├── implementation-roadmap.md
│   ├── pdf-processing-guide.md
│   ├── hpc-ollama-setup.md
│   ├── ollama-embeddings.md
│   └── fastmcp-guide.md
├── literature/ (literature collection and management)
│   ├── scope.md
│   ├── roadmap.md
│   └── findings/
└── reference-platform/ (physical platform design)
    ├── scope.md
    └── roadmap.md
```

### Active Directories (retained)

```
/docs/
├── README.md (navigation guide)
├── scope.md (v2.0 - firmware focus)
├── ROADMAP.md (v2.0 - firmware phases)
├── MIGRATION_SUMMARY.md (explains changes)
├── BEC_Wiring.md (power system wiring)
├── POWER_MODULE_WIRING.md (power module setup)
├── FEATURE_COMPARISON.md (flight controller features)
├── flight-controller/ (firmware documentation)
│   ├── scope.md
│   └── roadmap.md
└── flight-computer/ (flight computer integration)
    ├── scope.md
    └── roadmap.md
```

---

## Scope Clarity

### floppi (This Project) - FIRMWARE FOCUS

**In Scope:**
- ✅ Flight controller firmware (dRehmFlight-based)
- ✅ PID control loops and stabilization
- ✅ Sensor integration (IMU, GPS, barometer, magnetometer)
- ✅ Receiver protocols (SBUS, iBUS, DSM)
- ✅ Flight modes (Rate, Angle, Altitude Hold, Position Hold)
- ✅ Flight computer integration (ESP32, Raspberry Pi)
- ✅ WiFi telemetry and communication
- ✅ Autonomous navigation software
- ✅ Vision processing integration
- ✅ Swarm coordination (future)
- ✅ Testing and validation procedures
- ✅ Firmware documentation and guides

**Out of Scope (moved to engineering360):**
- ❌ Frame design and CAD modeling
- ❌ 3D printable components
- ❌ VTOL performance calculators
- ❌ Propulsion system optimization
- ❌ Structural analysis
- ❌ Material selection
- ❌ Lift-to-weight ratio calculations
- ❌ Component selection tools
- ❌ Literature RAG system
- ❌ Research paper compilation

### engineering360 (Companion Project) - PHYSICAL DESIGN FOCUS

**Handles:**
- Physical drone design and CAD
- Performance calculators and optimization
- Structural analysis and material selection
- Component selection and specifications
- Literature and research compilation
- Design iteration based on flight test data

---

## Project Relationship

### Clear Division of Responsibilities

```
┌──────────────────────────────────────────────────────────┐
│                    engineering360                        │
│                                                          │
│  • Designs physical drone (frame, structure)            │
│  • Selects components (motors, props, battery)          │
│  • Calculates performance (thrust, endurance)           │
│  • Optimizes for lift-to-weight ratio                   │
│  • Provides specifications to floppi                     │
│                                                          │
└──────────────────┬───────────────────────────────────────┘
                   │
                   │ Outputs: CAD files, component specs,
                   │          performance requirements
                   ▼
┌──────────────────────────────────────────────────────────┐
│                        floppi                            │
│                                                          │
│  • Implements firmware for specified hardware           │
│  • Configures PID tuning for frame characteristics      │
│  • Integrates sensors and flight computer               │
│  • Develops autonomous navigation software              │
│  • Tests and validates flight performance               │
│  • Provides flight data back to engineering360          │
│                                                          │
└──────────────────┬───────────────────────────────────────┘
                   │
                   │ Outputs: Flight controller firmware,
                   │          flight test data,
                   │          performance metrics
                   ▼
         ┌─────────────────────┐
         │  Iterative Design   │
         │     Refinement      │
         └─────────────────────┘
```

---

## Benefits of This Refactor

### Before
- ❌ Unclear project scope ("everything drone")
- ❌ Mixed firmware and physical design
- ❌ Difficult to navigate and contribute
- ❌ Hard to find relevant documentation
- ❌ Scope creep potential

### After
- ✅ Crystal clear scope (firmware and software only)
- ✅ Easy to understand what belongs where
- ✅ Contributors know exactly where to contribute
- ✅ Better project organization
- ✅ Reduced scope creep
- ✅ Complementary projects (floppi + engineering360)

---

## For Users and Contributors

### Working on Firmware/Software?
👉 **Stay in floppi**
- Flight controller firmware development
- Sensor drivers and integration
- Flight computer communication
- Autonomous navigation software
- Testing and validation

### Working on Physical Design?
👉 **Go to engineering360**
- Frame design and CAD
- Performance calculations
- Component selection
- Structural analysis
- Optimization tools

### Need Both?
👉 **Use both projects together**
1. Design drone in engineering360
2. Get component specs and requirements
3. Implement firmware in floppi
4. Fly and test
5. Share flight data with engineering360
6. Iterate and improve

---

## Next Steps

### Immediate (floppi)
1. ✅ Scope and roadmap updated
2. ✅ Documentation restructured
3. ✅ Out-of-scope content archived
4. Continue Phase 1 flight controller development
5. Hardware bench testing
6. First flight tests

### Future (engineering360)
1. Set up engineering360 repository
2. Migrate calculator code from floppi
3. Migrate 3D files and CAD models
4. Set up literature RAG system
5. Create engineering360 scope and roadmap
6. Establish workflow between projects

---

## Questions?

- **Firmware/software:** floppi GitHub Issues
- **Physical design:** engineering360 GitHub Issues
- **General:** Check scope documents in each project

---

## Files for Review

Key documents to read:

1. **[/README.md](/home/devel/Desktop/floppi/README.md)** - Main project README
2. **[/docs/scope.md](/home/devel/Desktop/floppi/docs/scope.md)** - Project scope (v2.0)
3. **[/docs/ROADMAP.md](/home/devel/Desktop/floppi/docs/ROADMAP.md)** - Development roadmap (v2.0)
4. **[/docs/MIGRATION_SUMMARY.md](/home/devel/Desktop/floppi/docs/MIGRATION_SUMMARY.md)** - Detailed migration explanation
5. **[/docs/README.md](/home/devel/Desktop/floppi/docs/README.md)** - Documentation navigation

---

## Verification Checklist

- [x] scope.md updated to v2.0 (firmware focus)
- [x] ROADMAP.md updated to v2.0 (6 firmware-focused phases)
- [x] Main README.md updated with clear focus statement
- [x] docs/README.md created (navigation guide)
- [x] MIGRATION_SUMMARY.md created (explains changes)
- [x] Out-of-scope directories archived to _archived/
- [x] _archived/README.md created (explains what's archived)
- [x] All references to engineering360 added
- [x] Clear scope boundaries established
- [x] Project relationship documented

---

**Refactor Status:** ✅ **COMPLETE**

**Project Focus:** Flight controller firmware and flight computer integration ONLY

**Companion Project:** engineering360 (for physical design, calculators, optimization)

---

**Document Version:** 1.0
**Completed:** 2026-01-11
