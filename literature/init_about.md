# Quick Setup Guide - dRehmFlight PlatformIO

## What You Asked About

### 1. **SBUS vs iBUS?**
**Answer: Use SBUS** ✅

The FS-iA6B supports both, but SBUS is better because:
- Built-in failsafe flags in the protocol
- More standardized (Futaba protocol)
- Better supported
- 16 channels vs iBUS's 14

### 2. **What happened to radioComm.ino?**
The functionality was **distributed**:
- Channel reading logic → `getCommands()` in main.cpp (line ~290)
- Failsafe logic → `failSafe()` in main.cpp (line ~332)
- Protocol-specific code → Integrated with receiver libraries

### 3. **What happened to the original dRehmFlight main program?**
It was **ported to main.cpp** with these improvements:
- Modular receiver support (easy to switch protocols)
- Cleaner structure with separated concerns
- All original functions preserved
- Better configuration management

### 4. **Where did radioComm functionality end up?**
```
radioComm.ino functions → New location:
├── getCommands() → main.cpp line ~290
├── failSafe() → main.cpp line ~332  
├── Channel reading → Receiver libraries + main.cpp
└── Protocol handling → Conditional compilation in main.cpp
```

## File Organization

```
YOUR_PROJECT/
├── platformio.ini          ← Build config
├── README.md              ← Full documentation
├── QUICK_SETUP.md         ← This file!
│
├── include/
│   ├── config.h           ← 🎯 EDIT THIS FIRST! Select receiver/IMU
│   └── pin_definitions.h  ← Pin mappings (Teensy 4.0/4.1)
│
├── lib/
│   ├── SBUS/              ← Copy from original dRehmFlight
│   ├── DSMRX/             ← Copy from original dRehmFlight
│   ├── MPU6050/           ← Copy from original dRehmFlight
│   └── MPU9250/           ← Copy from original dRehmFlight
│
└── src/
    └── main.cpp           ← Complete flight controller code
```

## 5-Minute Setup

### Step 1: Copy Libraries
```bash
# From your original dRehmFlight directory:
cp -r ~/floppi/dRehmFlight-master/code/src/SBUS lib/
cp -r ~/floppi/dRehmFlight-master/code/src/DSMRX lib/
cp -r ~/floppi/dRehmFlight-master/code/src/MPU6050 lib/
cp -r ~/floppi/dRehmFlight-master/code/src/MPU9250 lib/
```

### Step 2: Configure Hardware
Edit `include/config.h`:
```cpp
// Line 8-13: Choose ONE receiver
#define USE_SBUS_RECEIVER      // ✅ For FS-iA6B
//#define USE_IBUS_RECEIVER
//#define USE_DSM_RECEIVER

// Line 18-21: Choose ONE IMU
#define USE_MPU6050            // ✅ Most common
//#define USE_MPU9250

// Line 26-29: Choose controller mode
#define USE_RATE_CONTROLLER    // ✅ Acro mode
//#define USE_ANGLE_CONTROLLER // Stabilize mode
```

### Step 3: Wire Hardware

**FS-iA6B (SBUS):**
```
FS-iA6B → Teensy 4.0
VCC     → 5V
GND     → GND
SBUS    → Pin 21 (RX5)
```

**MPU6050:**
```
MPU6050 → Teensy 4.0
VCC     → 3.3V (⚠️ NOT 5V!)
GND     → GND
SDA     → Pin 18
SCL     → Pin 19
```

### Step 4: Build and Upload
```bash
pio run -e teensy40 -t upload
```

### Step 5: Test
Open serial monitor:
```bash
pio device monitor
```

You should see:
```
========================================
  dRehmFlight VTOL - PlatformIO Port
========================================
Receiver: SBUS
IMU: MPU6050
...
READY TO FLY!
========================================
```

## How the Modular System Works

### Before (Original dRehmFlight):
```cpp
// radioComm.ino - Everything in one file
void getCommands() {
    #if defined PPM_PINS
        // PPM code
    #elif defined PWM_PINS
        // PWM code
    #elif defined SBUS_MODE
        // SBUS code
    #endif
}
```

### After (This Port):
```cpp
// main.cpp - Clean modular approach
#ifdef USE_SBUS_RECEIVER
    #include "SBUS.h"
    SBUS sbus(SBUS_SERIAL_PORT);
#elif defined(USE_IBUS_RECEIVER)
    #include "IBUSReceiver.h"
    IBUSReceiver receiver(&IBUS_SERIAL_PORT);
#endif

void getCommands() {
    #ifdef USE_SBUS_RECEIVER
        if (sbus.read(&sbusChannels[0], &sbusFailSafe, &sbusLostFrame)) {
            channel_1_pwm = map(sbusChannels[0], 172, 1811, 1000, 2000);
            // ... etc
        }
    #elif defined(USE_IBUS_RECEIVER)
        receiver.update();
        channel_1_pwm = receiver.getChannel(1);
        // ... etc
    #endif
}
```

**Benefits:**
- ✅ Easy to add new receivers
- ✅ Change protocols by editing one #define
- ✅ Cleaner, more maintainable code
- ✅ No duplicate code

## Main Functions Map

| Original Function | New Location | Line # |
|-------------------|--------------|--------|
| `setup()` | main.cpp | ~129 |
| `loop()` | main.cpp | ~209 |
| `getCommands()` | main.cpp | ~290 |
| `failSafe()` | main.cpp | ~332 |
| `getIMUdata()` | main.cpp | ~354 |
| `calculate_IMU_error()` | main.cpp | ~394 |
| `Madgwick()` | main.cpp | ~442 |
| `getDesState()` | main.cpp | ~521 |
| `controlRATE()` | main.cpp | ~548 |
| `controlANGLE()` | main.cpp | ~572 |
| `controlMixer()` | main.cpp | ~600 |
| `scaleCommands()` | main.cpp | ~634 |
| `throttleCut()` | main.cpp | ~664 |
| `commandMotors()` | main.cpp | ~713 |

## Common Questions

**Q: Can I switch between SBUS and iBUS easily?**
A: Yes! Just change one line in `config.h`:
```cpp
//#define USE_SBUS_RECEIVER  // Comment out SBUS
#define USE_IBUS_RECEIVER   // Uncomment iBUS
```
Then rebuild and upload.

**Q: Where's the control mixing for my aircraft?**
A: In `controlMixer()` function (main.cpp, line ~600). The default is quadcopter X. Customize for your aircraft.

**Q: How do I tune PID gains?**
A: Edit the gains in `config.h` (lines 85-120), then rebuild and upload. Start with conservative values!

**Q: Why SBUS over iBUS?**
A: SBUS has failsafe flag bits built into the protocol, making failsafe detection more reliable.

**Q: Can I use this with other receivers?**
A: Yes! The modular structure makes it easy to add new receiver types. Just create a new receiver class following the pattern.

## Troubleshooting Quick Fixes

| Problem | Quick Fix |
|---------|-----------|
| IMU not detected | Check 3.3V (not 5V!), verify pins 18/19 |
| Receiver not responding | Verify SBUS mode in transmitter, check pin 21 |
| Won't compile | Copy libraries from original dRehmFlight |
| Motors won't arm | Check ESC calibration, verify min throttle |
| Oscillations | Reduce P gain in config.h |

## Next Steps

1. ✅ Copy libraries from original project
2. ✅ Edit `config.h` for your hardware
3. ✅ Wire receiver and IMU
4. ✅ Build and upload
5. ✅ Calibrate IMU (board flat and still)
6. ✅ Test receiver with serial monitor
7. 📝 Customize `controlMixer()` for your aircraft
8. 📝 Tune PID gains
9. 📝 Test motors (no props!)
10. 🚀 First flight!

---

**Need more help?** See README.md for full documentation!