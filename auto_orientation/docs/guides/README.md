# User Guides: Getting Started with Auto Orientation

Welcome! This directory contains comprehensive guides to help you get started with Auto Orientation. Whether you're setting up for the first time or deploying in the field, there's a guide for you.

---

## Quick Navigation

### I'm New to This

Start here:
1. **[Quick Start: 5-Minute Setup Guide](QUICK_START.md)** — Get hardware connected and firmware running
2. **[First Calibration Guide](FIRST_CALIBRATION.md)** — Calibrate your sensors (required once)
3. **[Real-Time Monitoring Guide](MONITORING_REAL_TIME_DATA.md)** — See your data in action

### I Have Questions

- **[FAQs](../getting_started/FAQS.md)** — 50+ common questions answered (accuracy, GPS, power, troubleshooting, etc.)
- **[Troubleshooting section](QUICK_START.md#troubleshooting-quick-reference)** — Fix common problems fast

### I'm Deploying to the Field

- **[Field Deployment Guide](FIELD_DEPLOYMENT.md)** — Checklists, location selection, power management, troubleshooting

### I'm Troubleshooting Hardware

- **[Hardware Setup Guide](HARDWARE_SETUP.md)** — Wiring diagrams, connections, assembly checklist, multimeter tests

### I Want to Understand Calibration Deeply

- **[Calibration Guide](../calibration/CALIBRATION_GUIDE.md)** — Detailed magnetometer calibration reference, common mistakes, recalibration scenarios

### I Want to Add a New Sensor

- **[Adding New Sensors Guide](ADDING_NEW_SENSORS.md)** — How to integrate additional sensors (temperature, pressure, other IMUs)

---

## All Guides at a Glance

| Guide | Purpose | Time | Difficulty | Status |
|-------|---------|------|------------|--------|
| **[Quick Start](QUICK_START.md)** | 5-min setup, hardware + firmware + verification | 5-10 min | Beginner | ✓ Complete |
| **[First Calibration](FIRST_CALIBRATION.md)** | First-time magnetometer calibration | 5-10 min | Beginner | ✓ Complete |
| **[Real-Time Monitoring](MONITORING_REAL_TIME_DATA.md)** | Monitor, log, and analyze data | 5-30 min | Intermediate | ✓ Complete |
| **[Field Deployment](FIELD_DEPLOYMENT.md)** | Checklists, location selection, troubleshooting | 30+ min | Intermediate | ✓ Complete |
| **[Hardware Setup](HARDWARE_SETUP.md)** | Wiring, connections, assembly, testing | 30 min | Intermediate | ✓ Complete |
| **[Calibration Guide](../calibration/CALIBRATION_GUIDE.md)** | Detailed calibration reference | 15-30 min | Intermediate | ✓ Complete |
| **[Adding Sensors](ADDING_NEW_SENSORS.md)** | Integrate new sensors | Varies | Advanced | ✓ Complete |

---

## Recommended Reading Order

### Path 1: I Just Got Everything (Fastest)
1. [Quick Start](QUICK_START.md) — Get it running (5 min)
2. [First Calibration](FIRST_CALIBRATION.md) — Calibrate (5 min)
3. [Real-Time Monitoring](MONITORING_REAL_TIME_DATA.md) — See the data (5 min)
4. **Done!** You're ready to use it

### Path 2: I Want to Do It Right (Thorough)
1. [Hardware Setup](HARDWARE_SETUP.md) — Understand the connections
2. [Quick Start](QUICK_START.md) — Get it running
3. [First Calibration](FIRST_CALIBRATION.md) — Calibrate properly
4. [Calibration Guide](../calibration/CALIBRATION_GUIDE.md) — Understand calibration
5. [Real-Time Monitoring](MONITORING_REAL_TIME_DATA.md) — Master data collection
6. [Field Deployment](FIELD_DEPLOYMENT.md) — Deploy successfully

### Path 3: I'm Hitting Issues (Troubleshooting)
1. [Quick Start - Troubleshooting section](QUICK_START.md#troubleshooting-quick-reference) — Common quick fixes
2. [FAQs](../getting_started/FAQS.md) — Search for your specific issue
3. [Hardware Setup - Troubleshooting section](HARDWARE_SETUP.md#troubleshooting-guide) — Deep hardware diagnosis

### Path 4: I'm Deploying to Field (Professional)
1. [Quick Start](QUICK_START.md) — Verify system works
2. [First Calibration](FIRST_CALIBRATION.md) — Complete calibration indoors
3. [Real-Time Monitoring](MONITORING_REAL_TIME_DATA.md) — Test data logging
4. [Field Deployment](FIELD_DEPLOYMENT.md) — Detailed pre-deployment checklist

---

## Key Concepts at a Glance

### Orientation (Pitch, Roll, Yaw)

- **Pitch**: Forward/backward tilt (-90° to +90°)
- **Roll**: Left/right tilt (-180° to +180°)
- **Yaw**: Direction/heading (0° to 360°, where 0° = north)

Accuracy: ±2-5° typical, ±1-2° with good calibration

### Position (Latitude, Longitude, Altitude)

- **Latitude**: North-South position (±0.0001° ≈ 10 meters)
- **Longitude**: East-West position (±0.0001° ≈ 10 meters)
- **Altitude**: Height above sea level (±5-10 meters)
- **CEP**: Accuracy metric (1-3m typical)

Requires: GPS, clear sky view (5+ degrees above horizon)

### Calibration

- **What it is**: Teaches magnetometer to compensate for local magnetic interference
- **How long**: 30-60 seconds after power-on
- **How often**: Once per location (or less), then persists forever
- **Result**: Yaw/heading becomes accurate (±2-5° instead of ±20-50°)

---

## Features by Guide

### Quick Start
- [ ] 5-minute setup overview
- [ ] Prerequisites checklist
- [ ] 3-step installation (hardware, build/flash, verify)
- [ ] Quick troubleshooting reference

### First Calibration
- [ ] What calibration does
- [ ] Expected timeline (1-2 minutes)
- [ ] Step-by-step instructions
- [ ] What "done" looks like
- [ ] Common mistakes and how to avoid them
- [ ] If calibration gets stuck (solutions)
- [ ] FAQ section (specific to calibration)

### Real-Time Monitoring
- [ ] How to launch the monitor
- [ ] Understanding the display (orientation, position, status)
- [ ] Finding your serial port
- [ ] Recording data (logging)
- [ ] Analyzing recorded data (Python examples, plotting)
- [ ] Monitoring specific aspects
- [ ] Data rate and performance metrics
- [ ] Troubleshooting monitor issues
- [ ] Advanced monitoring techniques

### Field Deployment
- [ ] Pre-deployment checklist
- [ ] Packing checklist (hardware, software, tools, documentation)
- [ ] Location selection (GPS, magnetic conditions)
- [ ] Initial setup at site
- [ ] Waiting for GPS lock
- [ ] Testing the site
- [ ] Power management
- [ ] Field troubleshooting (GPS, yaw, connections, logging)
- [ ] Data recovery and validation
- [ ] Safety considerations

### Hardware Setup
- [ ] Complete system overview
- [ ] BNO085 UART connection details
- [ ] GPS (Neo-M9N) connection details
- [ ] Power distribution and requirements
- [ ] Complete system wiring diagram
- [ ] Assembly checklist
- [ ] Testing procedures
- [ ] Troubleshooting guide
- [ ] Safety considerations

### Calibration Guide
- [ ] Quick start (2 minutes)
- [ ] Detailed calibration process
- [ ] Optimal figure-8 motion techniques
- [ ] Calibration status explained
- [ ] What good calibration looks like
- [ ] Common mistakes to avoid
- [ ] Recalibration when needed
- [ ] FAQs about calibration

### Adding New Sensors
- [ ] Overview and use cases
- [ ] Step-by-step integration guide
- [ ] Code examples
- [ ] Testing the new sensor
- [ ] Common sensors (BME280, QMC5883L, etc.)
- [ ] Troubleshooting sensor integration

---

## Document Statistics

- **Total guides**: 7 comprehensive guides
- **Total lines**: 2,700+ lines of documentation
- **Total size**: ~75 KB
- **Code examples**: 30+
- **Diagrams**: 20+
- **Checklists**: 15+
- **FAQ questions**: 50+

---

## Before You Ask Support

Most questions are answered in these guides! Please check:

1. **For setup issues**: [Quick Start](QUICK_START.md) + [Hardware Setup](HARDWARE_SETUP.md)
2. **For data/analysis**: [Real-Time Monitoring](MONITORING_REAL_TIME_DATA.md)
3. **For field problems**: [Field Deployment](FIELD_DEPLOYMENT.md)
4. **For any question**: [FAQs](../getting_started/FAQS.md) (search with Ctrl+F)
5. **For sensor integration**: [Adding Sensors](ADDING_NEW_SENSORS.md)

---

## Getting Help

If you can't find an answer:

1. **Search these guides** (Ctrl+F for keywords)
2. **Check [FAQs](../getting_started/FAQS.md)** (most common questions answered)
3. **Review [Architecture](../getting_started/ARCHITECTURE.md)** (system design diagrams)
4. **Check [findings](../findings/)** (research notes on specific topics)
5. **Check troubleshooting sections** in relevant guide

---

## Quick Links to Specific Topics

### Setup & Installation
- [Quick Start Guide](QUICK_START.md) — Complete setup
- [Hardware Setup Guide](HARDWARE_SETUP.md) — Detailed wiring
- [platformio.ini](../../platformio.ini) — Build configuration

### Calibration
- [First Calibration Guide](FIRST_CALIBRATION.md) — First time
- [Calibration Guide](../calibration/CALIBRATION_GUIDE.md) — Detailed reference

### Data & Monitoring
- [Real-Time Monitoring Guide](MONITORING_REAL_TIME_DATA.md) — Display and logging
- [tools/real_time_monitor.py](../../tools/real_time_monitor.py) — Monitor script
- [tools/MONITOR_README.md](../../tools/MONITOR_README.md) — Monitor detailed docs

### Deployment
- [Field Deployment Guide](FIELD_DEPLOYMENT.md) — Complete checklist
- [FAQs](../getting_started/FAQS.md) — Accuracy, GPS, troubleshooting

### Advanced
- [Adding New Sensors](ADDING_NEW_SENSORS.md) — Sensor integration
- [Architecture](../getting_started/ARCHITECTURE.md) — System design
- [findings/](../findings/) — Research notes

---

## Contributing to Guides

If you find:
- **Missing information** — File an issue
- **Errors or typos** — Submit a correction
- **Better explanations** — Suggest improvements
- **New FAQ questions** — Add to FAQs

All guides use clear, user-friendly language with practical examples.

---

**Last Updated**: 2025-05  
**Total Documentation**: 2,700+ lines  
**Skill Levels**: Beginner to Advanced  
**Format**: Markdown with checklists, tables, and examples
