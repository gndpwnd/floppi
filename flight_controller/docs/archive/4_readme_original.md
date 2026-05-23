# ⚠️ ARCHIVED — HISTORICAL COPY (links below are dead)

> **DO NOT FOLLOW THE LINKS BELOW.**
>
> This is a snapshot of an older, Teensy-only README kept for historical
> reference only. The project has since moved to lowercase, numbered docs
> (`docs/0_quickstart.md`, `docs/1_hardware_setup.md`, `docs/2_calibration_guide.md`,
> `docs/3_troubleshooting.md`, …). **Every `docs/UPPERCASE.md` link in this file is
> dead** (those files no longer exist).
>
> For current documentation start at **[../../README.md](../../README.md)** (project
> README) or **[../README.md](../README.md)** (docs index).

## 🚁 dRehmFlight VTOL - Flight Controller for Teensy 4.0/4.1 (archived original)

**Full-featured flight controller with automatic calibration for Teensy 4.0/4.1 + MPU6050 + FlySky FS-iA6B**

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Ready-orange)](https://platformio.org/)
[![Teensy 4.0/4.1](https://img.shields.io/badge/Teensy-4.0%2F4.1-blue)](https://www.pjrc.com/teensy/)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

---

## ⚡ Quick Start

**Get flying in 60 minutes:**

1. **[Hardware Setup](docs/HARDWARE_SETUP.md)** - Wire Teensy + MPU6050 + FS-iA6B
2. **[Quickstart Guide](docs/QUICKSTART.md)** - Upload code, calibrate, fly
3. **[Calibration](docs/CALIBRATION_GUIDE.md)** - Auto-calibration procedures
4. **[Troubleshooting](docs/TROUBLESHOOTING.md)** - Fix common issues

---

## 📚 Documentation

### Getting Started (Read in Order)

| Guide | Description | Time Required |
|-------|-------------|---------------|
| **[QUICKSTART.md](docs/QUICKSTART.md)** | 60-minute path from zero to first flight | 60 min |
| **[HARDWARE_SETUP.md](docs/HARDWARE_SETUP.md)** | Complete wiring guide with diagrams | 30 min |
| **[CALIBRATION_GUIDE.md](docs/CALIBRATION_GUIDE.md)** | Manual & automatic calibration | 15 min |
| **[PID_TUNING_GUIDE.md](docs/PID_TUNING_GUIDE.md)** | Tune for stable flight | 30 min |
| **[TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)** | Problem solving reference | As needed |

### Reference Documentation

- **[RECEIVER_BINDING.md](docs/RECEIVER_BINDING.md)** - FS-iA6B binding procedures
- **[PIN_DEFINITIONS.md](docs/PIN_DEFINITIONS.md)** - Pin mapping reference
- **[CONFIG_OPTIONS.md](docs/CONFIG_OPTIONS.md)** - All configurable parameters

---

## 🔧 Hardware Requirements

### Required Components

| Component | Specifications | Notes |
|-----------|---------------|-------|
| **Microcontroller** | Teensy 4.0 or 4.1 | 600MHz ARM Cortex-M7 |
| **IMU** | MPU6050 (GY-521 breakout) | 6-axis gyro + accel |
| **Receiver** | FlySky FS-iA6B | SBUS protocol |
| **Transmitter** | FlySky FS-i6/i6X or compatible | 6+ channels |
| **ESCs** | 4-6x 20A+ BLHeli compatible | OneShot125 or standard PWM |
| **Motors** | 4-6x Brushless | 2300-2700 KV typical |
| **Battery** | 3S-4S LiPo | 1300-2200 mAh |
| **BEC** | 5V 3A step-down regulator | For Teensy + receiver |

### Optional Components

- Servos (for VTOL/plane control surfaces)
- Buzzer (audio feedback)
- GPS module (future feature)
- Barometer (altitude hold - future feature)

---

## ⚙️ Features

### Core Functionality

✅ **6-axis IMU support** (MPU6050)  
✅ **SBUS receiver** (FlySky FS-iA6B)  
✅ **Rate and Angle flight modes**  
✅ **PID control** (independent roll/pitch/yaw)  
✅ **Motor mixing** (Quad X configuration)  
✅ **Servo control** (7 channels for VTOL/planes)  
✅ **Arming safety** (dual-condition arming)  
✅ **Throttle cut** (instant motor shutdown)  
✅ **Failsafe** (automatic on signal loss)

### Advanced Features

✅ **Automatic IMU calibration** (via CH6 switch)  
✅ **Madgwick attitude filter** (quaternion-based)  
✅ **Configurable loop rate** (up to 2000Hz)  
✅ **OneShot125 ESC support**  
✅ **Serial debugging** (real-time monitoring)  
⚠️ **Auto radio calibration** (in development)  
⚠️ **IMU orientation detection** (in development)

---

## 🚀 Installation

### 1. Install PlatformIO

```bash
# VS Code Extension (recommended)
# Install from VS Code Extensions marketplace

# Or via pip
pip install platformio

# Or via Homebrew (Mac)
brew install platformio
```

### 2. Clone Repository

```bash
cd ~/your-projects-folder
git clone https://github.com/your-username/flight_controller.git
cd flight_controller
```

### 3. Open in VS Code

```bash
code .
```

### 4. Configure Hardware

Edit `include/config.h`:

```cpp
// Select your IMU
#define USE_MPU6050       // ✓ Most common

// Select receiver protocol
#define USE_SBUS_RECEIVER // ✓ For FS-iA6B

// Select control mode
#define USE_ANGLE_CONTROLLER // ✓ Beginner-friendly
```

### 5. Upload Code

```bash
# Connect Teensy via USB
pio run -e teensy40 -t upload

# Or click "Upload" button in VS Code PlatformIO toolbar
```

---

## 🎛️ Configuration

### Basic Configuration (config.h)

```cpp
//=============================================================================
// IMU Calibration Values (from auto-calibration)
//=============================================================================
#define IMU_ACC_ERROR_X 0.012345
#define IMU_ACC_ERROR_Y -0.008765
#define IMU_ACC_ERROR_Z 0.023456
#define IMU_GYRO_ERROR_X 0.456789
#define IMU_GYRO_ERROR_Y -0.234567
#define IMU_GYRO_ERROR_Z 0.123456

//=============================================================================
// PID Gains (tune for your aircraft)
//=============================================================================
#define KP_ROLL_RATE 0.15f
#define KI_ROLL_RATE 0.15f
#define KD_ROLL_RATE 0.0004f

#define KP_PITCH_RATE 0.15f
#define KI_PITCH_RATE 0.15f
#define KD_PITCH_RATE 0.0004f

#define KP_YAW_RATE 0.30f
#define KI_YAW_RATE 0.05f
#define KD_YAW_RATE 0.00015f

//=============================================================================
// Filter Coefficients
//=============================================================================
#define B_ACCEL 0.14        // Accelerometer filter
#define B_GYRO 0.10         // Gyroscope filter
#define MADGWICK_BETA 0.04  // Attitude filter
```

---

## 📊 Default Channel Mapping

**Mode 2 (most common):**

| Channel | Control | Typical Use |
|---------|---------|-------------|
| CH1 | Right stick L/R | Roll |
| CH2 | Right stick U/D | Pitch |
| CH3 | Left stick U/D | Throttle |
| CH4 | Left stick L/R | Yaw |
| CH5 | 2-position switch | Arm/Disarm (throttle cut) |
| CH6 | 3-position switch | Flight mode / Auto-calibration |

**To change mapping:** Edit `THROTTLE_CHANNEL`, `ROLL_CHANNEL`, etc. in `config.h`

---

## 🎯 Calibration Procedures

### Automatic IMU Calibration

**Method 1: On Startup**
1. Place aircraft flat and level
2. Hold CH6 switch HIGH
3. Power on Teensy
4. Wait 5 seconds
5. Copy calibration values from Serial Monitor
6. Paste into `config.h`

**Method 2: In-Flight Trigger**
1. Disarm aircraft
2. Throttle minimum
3. Hold CH6 MID for 3 seconds
4. Calibration runs automatically
5. Copy values (if desired for permanent storage)

**See [CALIBRATION_GUIDE.md](docs/CALIBRATION_GUIDE.md) for full details.**

---

## 🛫 First Flight Checklist

### Pre-Flight

- [ ] All wiring verified against diagrams
- [ ] IMU calibrated (flat surface, no vibration)
- [ ] Receiver bound to transmitter
- [ ] Motor directions tested (props OFF!)
- [ ] Propeller directions verified
- [ ] Arming/disarming tested
- [ ] Battery voltage >11.1V (3S) or >14.8V (4S)
- [ ] Clear, open area (10m x 10m minimum)

### Hover Test

1. Arm system (throttle low + CH5 low)
2. Slowly raise throttle to 40-50%
3. Hover 15cm for 5 seconds
4. Watch for oscillations
5. Land gently
6. Disarm

**If stable → Proceed to first flight**  
**If oscillations → See [PID_TUNING_GUIDE.md](docs/PID_TUNING_GUIDE.md)**

---

## 🔧 Debugging

### Enable Debug Output

In `src/main.cpp`, uncomment debug functions in `loop()`:

```cpp
void loop() {
    // ... core flight controller code ...
    
    // Uncomment as needed:
    //printRadioData();      // Show receiver channels
    //printGyroData();       // Show gyro X/Y/Z
    //printAccelData();      // Show accel X/Y/Z
    //printRollPitchYaw();   // Show attitude angles
    //printPIDoutput();      // Show PID controller outputs
    //printMotorCommands();  // Show motor PWM values
    //printLoopRate();       // Show loop timing
}
```

**Upload and open Serial Monitor** (115200 baud)

**Use Serial Plotter** for graphs (Tools → Serial Plotter)

---

## 🏗️ Project Structure

```
flight_controller/
├── docs/
│   ├── QUICKSTART.md
│   ├── HARDWARE_SETUP.md
│   ├── CALIBRATION_GUIDE.md
│   ├── PID_TUNING_GUIDE.md
│   ├── TROUBLESHOOTING.md
│   └── RECEIVER_BINDING.md
├── include/
│   ├── config.h             ← Main configuration
│   ├── pin_definitions.h    ← Pin assignments
│   └── calibration.h        ← Auto-calibration functions
├── lib/
│   ├── RadioComm/           ← Receiver communication
│   │   ├── radioComm.cpp
│   │   └── radioComm.h
│   ├── Calibration/         ← Auto-calibration implementation
│   │   └── calibration.cpp
│   └── MPU6050/             ← IMU library
├── src/
│   └── main.cpp             ← Main flight controller code
├── platformio.ini           ← Build configuration
└── README.md                ← This file
```

---

## 🤝 Contributing

Contributions welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit changes (`git commit -m 'Add amazing feature'`)
4. Push to branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

---

## 📝 License

This project is licensed under the MIT License - see [LICENSE](LICENSE) file for details.

---

## 🙏 Acknowledgments

**Original dRehmFlight by Nicholas Rehm:**
- [dRehmFlight GitHub](https://github.com/nickrehm/dRehmFlight)
- [YouTube Channel](https://www.youtube.com/@NicholasRehm)

**Libraries used:**
- MPU6050 by ElectronicCats/Jeff Rowberg
- SBUS by bmellink/bolderflight
- PWMServo by Paul Stoffregen
- Wire (Arduino I2C)

---

## 📞 Support

**Having issues?**

1. Check [TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)
2. Search [existing issues](https://github.com/your-username/flight_controller/issues)
3. Open a new issue with:
   - Hardware details
   - Full error message
   - What you've tried
   - Serial Monitor output

**Questions?**
- GitHub Discussions
- dRehmFlight Google Group
- RCGroups Forums

---

## 🗺️ Roadmap

### Current Version (v1.0)
✅ Basic flight controller  
✅ SBUS receiver support  
✅ Manual IMU calibration  
✅ Automatic IMU calibration  
✅ PID control (rate + angle modes)  

### Future Features (v2.0)
⚠️ Automatic radio channel mapping  
⚠️ IMU orientation auto-detection  
⚠️ GPS support (position hold)  
⚠️ Barometer (altitude hold)  
⚠️ Logging to SD card  
⚠️ OSD (on-screen display)  
⚠️ Blackbox recorder  

---

## 📸 Gallery

**Your build photos here!** Submit photos via Pull Request.

---

**Happy Flying! 🚁**

**For questions or issues, see [TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) or open a GitHub issue.**