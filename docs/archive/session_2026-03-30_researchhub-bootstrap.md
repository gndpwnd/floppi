# Session Summary: 2026-03-30 — ResearchHub Bootstrap & drone_3d_model

## What Changed

### Project Repositioning
- floppi repositioned from "bare-bones flight controller" to **open-source high-performance drone research platform**
- Updated scope.md with new mission statement and expanded sub-projects table

### New Sub-Project: drone_3d_model/
- Created `drone_3d_model/` sub-project for 3D frame design, VTOL theory, and STEP+STL distribution
- Bootstrapped with standard structure: `docs/features/`, `docs/findings/`, `docs/archive/`, `docs/scope.md`, `docs/roadmap.md`, `docs/todo.md`
- Moved contents of `3D_files/` into `drone_3d_model/reference_models/`

### Sub-Projects Clarified
Four embedded sub-projects now formalized:
1. **flight_controller/** — PlatformIO firmware (Teensy + ESP32)
2. **fc_tool/** — Tauri desktop app (Rust + JS)
3. **drone_3d_model/** — 3D frame design (new this session)
4. **swarm_api/** — Python FastAPI server

### ResearchHub Integration
- Two research projects prepared: `flight_controller` and `drone_3d_model`
- ResearchHub will auto-research topics and build RAG knowledge base for each
- PDF source directories created at `docs/findings/sources/pdfs/` in both research projects
- Existing findings in `flight_controller/docs/findings/` identified for RAG ingestion
- **Status**: Structure prepared, waiting on `.researchhub.json` config files to activate

## Files Modified
- `/home/devel/floppi/docs/scope.md` — new mission, drone_3d_model added to sub-projects table
- `/home/devel/floppi/docs/ROADMAP.md` — drone_3d_model entry, ResearchHub section, updated overview
- `/home/devel/floppi/todo.md` — marked done items, added pending ResearchHub tasks
- `/home/devel/floppi/drone_3d_model/` — new sub-project directory tree created
