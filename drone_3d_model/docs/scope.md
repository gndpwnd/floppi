# Project Scope: drone_3d_model

> Last updated: 2026-03-30
> Status: New — bootstrapping

## Mission Statement

Build a knowledge repository of VTOL vehicle theory, drone chassis design math, and 3D-printable frame designs for various configurations. Provide editable designs (STEP + STL) so the community can iterate and improve — not just print. Long-term: inform CAD automation and optimization for drone frames via an external project.

## Objectives

- Build a theory knowledge base for VTOL vehicle design (aerodynamics, structural, propulsion math)
- Design modular drone frames for quadcopter, hexacopter, and custom VTOL configurations
- Publish designs as STEP files (editable, community-iterable) + STL files (print-ready)
- Plan sensor mount locations (IMU, GPS, camera, LiDAR) with vibration isolation considerations
- Create component layout templates for different flight controller boards (Teensy, ESP32)
- Maintain a library of reference designs and community models
- Document CAD design tips, tricks, and optimization patterns for drone frames
- Support rapid prototyping via 3D printing (FDM/resin)

## In Scope

- VTOL theory and design math (thrust-to-weight, moment arms, prop wash, drag models)
- Frame geometry and structural design (arms, center plates, motor mounts)
- Sensor placement and mounting solutions
- Component layout planning (FC board, ESCs, battery, receiver, antennas)
- Weight and balance calculations
- Vibration dampening mount designs
- 3D print settings and material recommendations
- Reference model collection and analysis
- Aerodynamic considerations for frame design
- Multi-rotor configurations (quad-X, quad-+, hex, Y6, octo)
- CAD design patterns and optimization tips for drone frames
- STEP file distribution for community collaboration (not just STL)

## Out of Scope

- Electronic circuit design or PCB layout (separate concern)
- Motor/ESC/propeller selection (engineering360 territory, but mounting geometry is in scope)
- Flight controller firmware (flight_controller sub-project)
- Structural FEA simulation (future consideration)
- CNC machining or carbon fiber layup processes
- CAD automation/optimization tooling itself (external project — but theory and patterns documented here feed into it)

## Constraints

| Constraint | Reason | Flexibility |
|-----------|--------|-------------|
| 3D printing primary | Accessible to hobbyists and researchers | Medium — can add CNC later |
| Open-source designs | Project philosophy | Fixed |
| Parametric when possible | Support multiple configurations | High |

## Technical Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| CAD tool | TBD | Need to evaluate OpenSCAD vs FreeCAD vs Fusion 360 |
| File formats | STEP (editable) + STL (print-ready) | STEP enables community iteration, STL for direct printing |
| Design approach | Modular/parametric | Support multiple drone sizes and configs |

## Integration Points

- **flight_controller** — Board dimensions, sensor positions, wiring clearances
- **engineering360** — Component specifications, structural requirements
- **External CAD automation project** (future) — Automated CAD optimization for drone frames, informed by theory built here

## Open Questions

- Which CAD tool to standardize on? (OpenSCAD for parametric, FreeCAD for visual)
- Standard drone sizes to support? (micro 65mm, mini 125mm, 250mm racing, 450mm+)
- Should designs include propeller guards for indoor/research use?

---

*Part of the floppi drone platform.*
