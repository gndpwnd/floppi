# Roadmap: drone_3d_model

> No time estimates — items are ordered by priority.

## Overview

Theory-first approach: build knowledge of VTOL design math and aerodynamics, then apply to frame design. STEP + STL distribution for community iteration.

## Core Features

### VTOL Theory & Design Knowledge
- [ ] Thrust-to-weight ratio calculations for multi-rotor configurations
- [ ] Moment arm geometry and motor placement math
- [ ] Propeller aerodynamics (disc loading, prop wash, ground effect)
- [ ] Drag models for different frame geometries
- [ ] Vibration modes and resonance in multi-rotor frames
- [ ] Weight distribution and center of gravity optimization

### Frame Design Foundation
- [ ] Evaluate and select CAD toolchain (OpenSCAD / FreeCAD / Fusion 360)
- [ ] Catalog reference models with dimensions and configurations
- [ ] Define standard mounting patterns (M2, M3 hole spacing for FC boards)
- [ ] First custom quadcopter frame design (250mm class, STEP + STL)
- [ ] Document CAD tips and optimization patterns for drone frames

### Sensor Integration
- [ ] IMU mounting with vibration isolation (soft mounts, dampening)
- [ ] GPS antenna mount (clear sky view, away from ESC noise)
- [ ] Camera mount options (FPV, downward-facing)
- [ ] Modular sensor bay design

### Multi-Configuration Support
- [ ] Quadcopter X-frame
- [ ] Hexacopter frame
- [ ] Micro/mini frames (sub-250g)
- [ ] Payload/research frames (sensor arrays, compute modules)

## Infrastructure
- [ ] 3D printing profile templates (PLA, PETG, TPU for dampeners)
- [ ] Weight tracking spreadsheet/tool
- [ ] Assembly documentation template

## Nice to Have
- [ ] Parametric design generator (input wheelbase → output frame)
- [ ] FEA stress analysis for arm designs
- [ ] Aerodynamic shroud/duct designs

## Completed
_(none yet)_
