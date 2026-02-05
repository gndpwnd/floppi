# Flight Controller Firmware - Documentation

Open-source VTOL flight controller firmware for Teensy 4.0/4.1, based on dRehmFlight. Designed for garage-buildable drones with auto-calibration features.

## Project Documents

| Document | Purpose |
|----------|---------|
| [scope.md](scope.md) | What this project IS and IS NOT |
| [roadmap.md](roadmap.md) | Feature checklist and milestones |
| [todo.md](todo.md) | Immediate tasks and session work |

## User Guides

| Guide | Description |
|-------|-------------|
| [0_quickstart.md](0_quickstart.md) | 60-minute path from zero to first flight |
| [1_hardware_setup.md](1_hardware_setup.md) | Complete wiring guide with diagrams |
| [2_calibration_guide.md](2_calibration_guide.md) | Manual and automatic calibration procedures |
| [3_troubleshooting.md](3_troubleshooting.md) | Problem solving reference |

## Documentation Subfolders

| Folder | Contents |
|--------|----------|
| [features/](features/) | Feature specifications and usage docs |
| [findings/](findings/) | Research, investigations, and discoveries |
| [archive/](archive/) | Session summaries and historical records |

## Quick Start

```bash
cd flight_controller
pio run -e teensy40 -t upload
```

See [0_quickstart.md](0_quickstart.md) for the full guide.

## Related Projects

- **[fc_tool](/fc_tool/)** — Desktop app for serial monitoring, IMU visualization, and calibration interface
- **[engineering360](https://github.com/yourusername/engineering360)** — Physical drone design, frame construction, component selection

---

*For the parent project overview, see the [floppi README](/README.md) and [floppi docs](/docs/).*
