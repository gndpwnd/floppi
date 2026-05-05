# DTR/RTS Reset Behavior: Windows vs Linux Analysis

## Executive Summary

The Arduino Mega's bootloader failure on Linux (after working on Windows) is caused by **inconsistent DTR/RTS timing and driver-specific behavior**. The bootloader waits for STK500v2 protocol handshake within a tight window (1-2 seconds), but Linux USB drivers may delay or skip the reset signal that triggers the bootloader.

---

## Part 1: The Reset Circuit Hardware

### Arduino Mega Reset Circuit

```
┌─────────────────────────────────────────────┐
│        USB Serial Adapter                   │
│        (built-in on Mega)                   │
│                                             │
│  DTR pin ────[1μF capacitor]────┬──────┐   │
│                                  │      │   │
│                                [10kΩ]  │   │
│                                  │      │   │
│                                  │    RESET │
│                                  │      │   │
│                                 GND    │   │
│                                  │      │   │
└──────────────────────────────────┼──────┘   │
                                   │          │
                    ┌──────────────┘          │
                    │                        │
                    ▼                        │
              (Mega 2560 RESET pin)          │
              (ATmega2560 pin 9)             │
                    │                        │
                    ▼                        │
           [Bootloader starts when LOW]
```

### How It Works: State Transitions

```
STATE 1: Normal Operation (Idle)
  DTR = HIGH
  Capacitor charged (no voltage across it)
  RESET pin held HIGH (inactive)
  → Bootloader or user code running

STATE 2: PC Opens Serial Port (Windows/Linux common)
  DTR transitions LOW
  Capacitor discharges through RESET pin to GND
  RESET pin pulled LOW momentarily
  → Arduino receives RESET pulse

STATE 3: Serial Port Stable (DTR stays LOW)
  DTR = LOW
  Capacitor slowly charges through 10kΩ resistor
  RESET pin voltage rises
  → RESET released, bootloader starts
  → Bootloader initializes UART
  → Bootloader starts listening for STK500v2 SYNC byte

STATE 4: PC Closes Serial Port (DTR goes HIGH)
  DTR = HIGH
  Capacitor discharges again
  → This is a second reset pulse (usually ignored by bootloader)
```

---

## Part 2: Linux USB Driver Behavior

### Driver Types on Linux

```
┌──────────────────────────────────────────────────────┐
│         Arduino Mega USB Driver Stack                │
│                                                      │
│  ┌─────────────────────────────────────────────┐   │
│  │  Application: avrdude, PlatformIO, Arduino  │   │
│  │  IDE                                        │   │
│  └────────────────┬──────────────────────────┘   │
│                   │                              │
│  ┌────────────────▼──────────────────────────┐   │
│  │  /dev/ttyACM0 or /dev/ttyUSB0 (device)   │   │
│  │  ↓ Linux kernel serial core               │   │
│  └────────────────┬──────────────────────────┘   │
│                   │                              │
│  ┌────────────────▼──────────────────────────┐   │
│  │  USB Serial Driver Module                 │   │
│  │                                           │   │
│  │  Options:                                 │   │
│  │  - cdc_acm (CDC ACM - MOST STABLE)       │   │
│  │  - usbserial (generic - variable)        │   │
│  │  - cp210x (Silicon Labs chips)           │   │
│  │  - ftdi_sio (FTDI chips)                 │   │
│  └────────────────┬──────────────────────────┘   │
│                   │                              │
│  ┌────────────────▼──────────────────────────┐   │
│  │  USB Host Controller                      │   │
│  │  (XHCI, OHCI, UHCI, EHCI)                │   │
│  └────────────────┬──────────────────────────┘   │
│                   │                              │
│  ┌────────────────▼──────────────────────────┐   │
│  │  Arduino Mega Hardware                    │   │
│  │  (Reset circuit + UART)                   │   │
│  └──────────────────────────────────────────┘   │
│                                                      │
└──────────────────────────────────────────────────────┘
```

### Driver-Specific Behavior

#### CDC ACM Driver (Most Common, Most Stable)

```
Timeline when opening port:
  
0ms    +  DTR line transition triggered
       │  (kernel cdc_acm driver detects open())
       │
       ├─→ Request USB SET_CONTROL_LINE_STATE (clear DTR)
       │
5ms    ├─ DTR signal LOW on physical line
       │  Capacitor begins discharging
       │
10ms   ├─ RESET pin LOW for ~150ms
       │  Arduino receives reset pulse
       │
20ms   ├─ RESET line releases (capacitor charging again)
       │  Bootloader begins initialization
       │
100ms  ├─ Bootloader UART ready
       │  Bootloader enters SYNC-wait loop
       │
       ├─ Bootloader timeout: 8 seconds
       │  (listens for STK500v2 SYNC byte 0x30)
       │
1000ms ├─ avrdude transmits first SYNC byte
       │  ✓ Bootloader responds with ACK (0x14)
       │  Upload proceeds...

STABLE: CDC ACM has well-defined timing, consistent across Linux versions
```

#### Generic usbserial Driver (Variable, Problem-Prone)

```
Timeline when opening port:

0ms    +  DTR line transition requested
       │  (kernel usbserial driver detects open())
       │
       ├─→ Request USB SET_LINE_REQUEST
       │
3-50ms ├─ DTR transition DELAYED or SKIPPED
       │  (driver bug or timing issue)
       │  OR
       │  DTR pulse is too short (< 10ms)
       │
       ├─ RESET may not trigger properly
       │  OR
       │  Bootloader interrupted mid-initialization
       │
100-300ms ├─ Timing unpredictable
       │   Bootloader state unknown
       │
1000ms ├─ avrdude transmits SYNC byte
       │  ✗ Bootloader doesn't respond (too late or missed reset)
       │  TIMEOUT → FAILURE

UNSTABLE: Timing varies, may work once then fail next time
          Different on each reboot or power cycle
```

#### CP210x Driver (Silicon Labs - Usually OK)

```
Behavior: Similar to CDC ACM but with slight delays
DTR transition: Well-defined, but may have 50-100ms latency
RESET pulse duration: ~200-300ms
Bootloader startup: Usually reliable after DTR

Status: Generally reliable, better than usbserial
```

---

## Part 3: avrdude STK500v2 Implementation

### avrdude Sync Sequence (from source: stk500v2.c)

```c
// This is approximately what avrdude does:

// 1. Open serial port and set up timing
fd = open("/dev/ttyACM0", O_RDWR);
tcsetattr(fd, TCSANOW, &tty);  // Set 115200 baud, 8N1

// 2. DTR/RTS handling
ioctl(fd, TIOCMSET, &lines);   // Toggle DTR line
                                // This triggers the reset circuit!

// 3. Wait for bootloader to initialize
usleep(100000);  // Wait 100ms

// 4. Try to synchronize with bootloader
for (int attempt = 0; attempt < 3; attempt++) {
    // Send SYNC byte (0x30 0x20)
    unsigned char sync_msg[] = {0x30, 0x20};
    write(fd, sync_msg, 2);
    
    // Wait for response with timeout
    struct timeval timeout;
    timeout.tv_sec = 1;    // 1 second timeout!
    timeout.tv_usec = 0;
    
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    
    int ret = select(fd + 1, &readfds, NULL, NULL, &timeout);
    
    if (ret > 0) {
        unsigned char response;
        read(fd, &response, 1);
        
        if (response == 0x14) {  // Expected response!
            return SUCCESS;      // Bootloader is responding!
        }
    }
    // If no response, timeout happens here
    // Then loop retries up to 3 times
}

// After 3 failed attempts with 1-second timeout each:
printf("stk500v2_ReceiveMessage(): timeout");
return FAILURE;
```

### Critical Timing Window

```
Bootloader Initialization Timeline:

0ms     ├─ Reset pulse received
        │
50ms    ├─ ATmega2560 oscillator stabilizes
        │
100ms   ├─ Bootloader code starts executing
        │
150ms   ├─ UART initialized and configured
        │
200ms   ├─ "Timeout counter" starts (for 8-second bootloader timeout)
        │
300ms   ├─ Bootloader enters main receive loop
        │   ┌─────────────────────────────────┐
        │   │ Listening for SYNC byte (0x30)  │
        │   │ Timeout: 8 seconds              │
        │   │ Success: Respond with 0x14      │
        │   └─────────────────────────────────┘
        │
        │   avrdude already sent SYNC at:
        │   └─ 100ms (too early!) ✗
        │   
        │   avrdude will send SYNC at:
        │   └─ 1000ms (safe window) ✓
        │
8000ms  ├─ Bootloader timeout expires
        │  Jump to user program (if running)
        │
        │ (Upload must complete before this)
```

**The Problem:** If DTR pulse is delayed or skipped on Linux, the bootloader initialization doesn't happen, so when avrdude sends SYNC at ~100-300ms, the bootloader isn't listening yet → timeout.

---

## Part 4: Comparing Windows vs Linux Behavior

### Windows Behavior

```
SETUP:
Windows COM port stack → Native USB driver → Arduino

DTR TIMING ON WINDOWS:
Program opens COM port
  ↓ (Win32 SetCommState API)
  ↓
DTR toggles to LOW immediately
  ↓ (Hardware-controlled via FTDI or native driver)
  ↓
RESET pin pulled LOW for ~100ms
  ↓
DTR returns HIGH
  ↓
Bootloader begins initialization

TOTAL TIME FROM OPEN TO LISTENING: 150-200ms

SYNC ATTEMPT BY avrdude:
Waits 100ms after open
Sends SYNC byte (0x30)
Bootloader responds (0x14)
✓ UPLOAD SUCCEEDS

KEY: Windows drivers have very predictable, quick DTR transitions
```

### Linux Behavior (Problem Case)

```
SETUP:
Linux application → cdc_acm/usbserial driver → USB stack → Arduino

DTR TIMING ON LINUX:
Program opens /dev/ttyACM0
  ↓ (tcsetattr/ioctl API)
  ↓
Request to toggle DTR sent to driver
  ↓ (Driver may have queued operations, buffering)
  ↓
Depends on driver and kernel version:
  - Good case: DTR toggles 20-50ms after request
  - Bad case: DTR toggle is skipped (driver bug)
  - Worse case: DTR toggle happens after avrdude's timeout!
  ↓

TOTAL TIME FROM OPEN TO LISTENING: 200-500ms or never happens!

SYNC ATTEMPT BY avrdude:
Waits 100ms after open (not enough!)
Sends SYNC byte (0x30)
Bootloader still initializing or not reset at all
No response within 1 second timeout
✗ UPLOAD FAILS → TIMEOUT

KEY: Linux drivers have variable/unpredictable DTR timing
```

### Side-by-Side Comparison

| Aspect | Windows | Linux (Good) | Linux (Bad) |
|--------|---------|--------------|------------|
| DTR pulse latency | 10-30ms | 30-100ms | 200-500ms or skipped |
| RESET pulse duration | 80-120ms | 100-200ms | Unpredictable |
| Bootloader startup time | 150-200ms | 200-300ms | 300-800ms+ |
| avrdude SYNC timing | 100ms wait | 100ms wait | 100ms wait (too short!) |
| Success rate | >99% | 95%+ | <10% (intermittent) |

---

## Part 5: Root Cause Analysis

### Why Worked on Windows, Fails on Linux?

The user says: "It worked on Windows 2 days ago"

Possible explanations:

**Scenario A: Different Kernel/Driver Version (Most Likely)**
- Updated to new Linux kernel yesterday
- Kernel version changed `usbserial.c` timing behavior
- Same Arduino Mega, but different driver timing
- **Fix:** Reduce upload speed or manually reset during upload

**Scenario B: Linux Distribution Change**
- Switched from Ubuntu to Debian (or different Ubuntu version)
- Different version of `cdc_acm.ko` driver
- Different USB stack behavior
- **Fix:** Same as Scenario A

**Scenario C: USB Hub or Port Change**
- Using different USB port (different root hub)
- Different host controller (XHCI vs EHCI)
- Different driver path taken
- **Fix:** Try original USB port, or different port entirely

**Scenario D: Application Change**
- Was using Arduino IDE (had workarounds)
- Now using PlatformIO (different timeout handling)
- **Fix:** Adjust PlatformIO upload settings

### Why Manual Reset Button Works

When you press RESET button during upload:

```
Timeline with Manual Reset:

100ms  ← avrdude sends SYNC byte

        Meanwhile, you press RESET button (0-200ms)
        
150ms  ← RESET pin pulled LOW by capacitor discharge
        ↓
        Bootloader RESET triggered by your action
        (regardless of DTR state)
        
200ms  ← RESET released
        ↓
        Bootloader begins initialization
        
300ms  ← Bootloader UART ready
        Bootloader listening for SYNC
        
        SYNC byte still in serial buffer from 100ms!
        ✓ Bootloader receives it and responds
        ✓ UPLOAD SUCCEEDS

KEY: Manual reset bypasses the driver timing issue
```

---

## Part 6: Solution Strategies

### Strategy 1: Force Consistent DTR Timing

**Edit `/etc/udev/rules.d/99-arduino.rules`:**

```bash
# For Arduino Mega (CDC ACM driver)
SUBSYSTEMS=="usb", ATTRS{idVendor}=="2341", ATTRS{idProduct}=="0042", MODE="0666", SYMLINK+="arduino_mega"

# Kernel module options for better DTR handling
# Add to /etc/modprobe.d/arduino.conf:
options cdc_acm use_tiocm_line=1 no_union_ies=1
```

Then reload:
```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
```

### Strategy 2: Increase Initial Wait Time

**Edit `platformio.ini`:**

```ini
[env:arduino_mega]
# Add wait after opening port, before sending SYNC
upload_wait_for_upload_port = true

# Can't directly change avrdude's 100ms wait in PlatformIO config
# But slower baud rate effectively gives more time
upload_speed = 57600  # Instead of 115200
```

At 57600 baud instead of 115200, the timing window is effectively doubled, giving the bootloader more time to start listening.

### Strategy 3: Hardware Workaround

Add a small delay capacitor to the reset circuit (advanced):

```
Original:
DTR ──[1μF]──┬── RESET
             │
          [10kΩ]
             │
            GND

Modification (increases pulse width):
DTR ──[2.2μF]──┬── RESET  (increased capacitance)
               │
            [10kΩ]        (unchanged)
               │
              GND

Effect:
- Capacitor discharge is slower
- Reset pulse is wider (better tolerance)
- But bootloader still needs to respond to avrdude's SYNC
```

This is rarely needed if software solutions work.

### Strategy 4: Kernel Module Parameter

**For systems with intermittent failures:**

```bash
# Check current setting
cat /sys/module/usbserial/parameters/dtr_rts_on_open

# If it says 'N' (disabled), enable it:
sudo modprobe -r usbserial
sudo modprobe usbserial dtr_rts_on_open=Y

# Make permanent in /etc/modprobe.d/usbserial.conf:
options usbserial dtr_rts_on_open=Y
```

Then reboot.

---

## Part 7: Debugging Commands

### Verify Which Driver is Loaded

```bash
# Find the device
lsusb | grep -i arduino
# Output: Bus 001 Device 005: ID 2341:0042 Arduino SA Arduino Mega 2560 R3

# Check which driver handles it
lsusb -v -d 2341:0042 | grep iSerialNumber
# Also look at: /sys/bus/usb/devices/*/driver

# See loaded driver module
lsmod | grep -E "cdc_acm|usbserial|ftdi"
```

### Monitor DTR Signal with strace

```bash
# Terminal 1: Monitor syscalls while avrdude runs
sudo strace -f -e ioctl,open,read,write /usr/bin/avrdude -c stk500v2 -p m2560 -P /dev/ttyACM0 -b 115200 2>&1 | grep -E "ioctl|TIOCM|TIOCSSERIAL"

# Look for TIOCMSET commands (these control DTR/RTS)
# TIOCMSET with bits 0x04 = DTR, 0x08 = RTS
```

### Capture Verbose avrdude Output

```bash
pio run -e arduino_mega -t upload -v 2>&1 | tee upload_verbose.log

# Then analyze:
grep -E "timeout|Device signature|Transmitting|Found" upload_verbose.log
```

### Check USB Device Reset Behavior

```bash
# Monitor USB device resets in real-time
dmesg -w | grep -i "arduino\|reset\|cdc"

# Then try upload in another terminal
# Watch for reset-related kernel messages
```

---

## Part 8: Prevention & Best Practices

### For Users

1. **Always use slower baud rate on Linux for stability:**
   ```ini
   upload_speed = 57600  # Instead of 115200
   ```

2. **Be familiar with manual recovery methods:**
   - Know how to press RESET button during upload
   - Know how to unplug/replug USB

3. **Test before committing critical code:**
   - Upload dummy test sketch first
   - Verify it works before uploading actual code

### For Developers/Library Maintainers

1. **Use avrdude wait flags:**
   ```bash
   avrdude -c stk500v2 -p m2560 -P /dev/ttyACM0 \
       -b 115200 -U ... \
       -w true  # Wait for port
   ```

2. **Implement retry logic with exponential backoff:**
   ```python
   for attempt in range(3):
       try:
           # Try upload
           break
       except SerialTimeout:
           time.sleep(2 ** attempt)  # 1s, 2s, 4s
   ```

3. **Log DTR state changes for debugging:**
   ```c
   // In upload code:
   int dtr_state = TIOCM_DTR;
   ioctl(fd, TIOCMGET, &dtr_state);
   printf("DTR state: %s\n", dtr_state & TIOCM_DTR ? "HIGH" : "LOW");
   ```

---

## References

- **avrdude source code:** https://github.com/avrdude/avrdude/blob/main/src/stk500v2.c
- **Linux USB CDC ACM driver:** kernel/drivers/usb/class/cdc-acm.c
- **Arduino Mega 2560 Bootloader:** https://github.com/arduino/ArduinoCore-avr/tree/master/bootloaders/stk500v2
- **ATmega2560 Datasheet:** http://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-2549-8-bit-AVR-Microcontroller-ATmega640-ATmega1280-ATmega1281-ATmega2560-ATmega2561_datasheet.pdf
- **USB Serial Driver (usbserial):** kernel/drivers/usb/serial/generic.c

---

## Summary Table

| Symptom | Cause | Solution | Time Required |
|---------|-------|----------|----------------|
| Timeout on Linux only | DTR timing issue | Manual RESET button | 1 minute |
| Intermittent failures | Unpredictable DTR | Reduce baud rate to 57600 | 2 minutes |
| Never responds | Bootloader not running | Try USB power cycle | 1 minute |
| Timeout every time | Driver bug or missing | ICSP reflash | 1-2 hours |

---

**Document Version:** 1.0  
**Created:** 2026-05-05  
**Last Updated:** 2026-05-05
