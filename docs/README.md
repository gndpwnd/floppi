# floppi Documentation

This directory contains all documentation for the floppi flight controller firmware and flight computer integration project.

## Project Focus

**floppi focuses exclusively on:**
- Flight controller firmware development (dRehmFlight-based)
- Flight computer integration (ESP32, Raspberry Pi)
- Sensor integration and calibration
- Communication protocols and telemetry
- Testing and validation procedures

**For physical design, mechanical optimization, and aerodynamic calculations, see [engineering360](https://github.com/yourusername/engineering360).**

---

## Documentation Structure

### Core Documentation

- **[scope.md](./scope.md)** - Project scope, focus areas, and boundaries
- **[ROADMAP.md](./ROADMAP.md)** - Development roadmap and phases

### Hardware & Wiring

- **[BEC_Wiring.md](./BEC_Wiring.md)** - Power system wiring guide
- **[POWER_MODULE_WIRING.md](./POWER_MODULE_WIRING.md)** - Power module setup
- **[FEATURE_COMPARISON.md](./FEATURE_COMPARISON.md)** - Flight controller feature analysis

### Flight Controller Documentation

- **[flight-controller/](./flight-controller/)** - Firmware documentation
  - Setup guides
  - Configuration instructions
  - Calibration procedures
  - PID tuning guides
  - Troubleshooting

### Flight Computer Documentation

- **[flight-computer/](./flight-computer/)** - Flight computer integration
  - ESP32 integration guides
  - Raspberry Pi setup
  - Communication protocols
  - Telemetry streaming
  - Autonomous navigation

---

## Out of Scope (Moved to engineering360)

The following topics have been moved to the **engineering360** project:

### Physical Design & Mechanical Engineering
- Frame design and CAD modeling
- 3D printable components
- Structural analysis
- Material selection

### Performance Optimization & Calculators
- VTOL performance calculators
- Propulsion system optimization
- Lift-to-weight ratio calculations
- Endurance modeling
- Component selection tools

### Literature & Research
- Textbook collection and RAG system
- Research paper compilation
- Aerodynamics references
- Control theory references

### Reference Platform Design
- Physical drone platform design
- Mechanical assembly guides
- Component selection for complete builds

---

## Quick Start

1. **New to floppi?** Start with [scope.md](./scope.md) to understand the project
2. **Ready to build?** Check [flight-controller/](./flight-controller/) for firmware setup
3. **Planning integration?** See [flight-computer/](./flight-computer/) for high-level autonomy
4. **Need wiring help?** Review [BEC_Wiring.md](./BEC_Wiring.md) and [POWER_MODULE_WIRING.md](./POWER_MODULE_WIRING.md)
5. **Looking for roadmap?** Read [ROADMAP.md](./ROADMAP.md) for development phases

---

## Related Projects

- **[engineering360](https://github.com/yourusername/engineering360)** - Physical design, calculators, optimization
- **[dRehmFlight](https://github.com/nickrehm/dRehmFlight)** - Original flight controller firmware (base system)

---

## Contributing

Contributions are welcome! Areas of focus:

- Firmware bug fixes and features
- Sensor driver development
- Flight computer integration
- Documentation improvements
- Testing and validation

See the main repository README for contribution guidelines.

---

**Last Updated:** 2026-01-11
