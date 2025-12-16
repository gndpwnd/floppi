# FS-iA6B Receiver Binding Guide - Using Teensy Power

## 🎯 Overview

You can bind your FS-iA6B receiver to your FlySky transmitter using ONLY the Teensy's USB power. No separate battery needed for initial setup!

---

## 📋 What You Need

- ✓ FlySky FS-iA6B receiver
- ✓ FlySky FS-i6 or FS-i6X transmitter (or compatible)
- ✓ Teensy 4.0 with USB cable
- ✓ 3 jumper wires
- ✓ Computer

---

## 🔌 STEP 1: Wiring for Binding

### Connect FS-iA6B to Teensy

```
┌─────────────────────────────────────────┐
│  FS-iA6B Receiver      Teensy 4.0       │
│                                         │
│  SBUS Port (3 pins):                    │
│  ┌───┬───┬───┐                         │
│  │ G │ + │ S │                         │
│  └─┬─┴─┬─┴─┬─┘                         │
│    │   │   └──→ Pin 21 (RX5)           │
│    │   └──────→ VIN or 5V              │
│    └──────────→ GND                     │
│                                         │
│  [Bind Button] ← Hold during power-on  │
└─────────────────────────────────────────┘
```

### Wire Connections

| FS-iA6B Pin | Wire Color | Teensy Pin | Notes |
|-------------|------------|------------|-------|
| GND | Black | GND | Common ground |
| +5V | Red | **VIN** | Power (5V from USB) |
| Signal | White/Black | Pin 21 | SBUS signal |

**Important:** Connect to **VIN**, not 5V pin!
- VIN passes USB 5V directly to receiver
- 5V pin has current limit
- Receiver draws ~70mA (within USB limits)

---

## 📱 STEP 2: Transmitter Preparation

### Enable SBUS Mode (Do this FIRST)

1. **Turn ON transmitter**

2. **Enter menu:**
   - Press and hold OK/Menu button
   - Navigate with arrow buttons

3. **Find RX Setup:**
   ```
   Menu
   └─ System Setup
      └─ RX Setup
         └─ Serial Mode
   ```

4. **Set to SBUS:**
   ```
   Serial Mode: [SBUS]  ← Select this
   NOT: PPM, PWM, iBUS
   ```

5. **Save settings:**
   - Press OK
   - Exit menu
   - **Power cycle transmitter** (turn OFF then ON)

### Verify SBUS Mode

- Display should show SBUS indicator
- Some transmitters show "S-BUS" in corner

---

## 🔗 STEP 3: Binding Process

### Method A: Bind Button (Recommended)

1. **Make sure transmitter is OFF**

2. **Locate bind button on receiver:**
   - Small button usually on top
   - May be labeled "BIND" or "SET"
   - May be recessed (need paperclip)

3. **Hold bind button**

4. **While holding, plug in Teensy USB:**
   ```bash
   # On computer:
   # Plug in Teensy USB cable
   ```

5. **Watch receiver LED:**
   - Should flash RAPIDLY (2-3 times per second)
   - Keep holding bind button

6. **Turn ON transmitter**

7. **Wait for bind:**
   - LED will stop flashing
   - LED becomes SOLID
   - Release bind button

8. **Success!**
   - Solid LED = bound
   - Move transmitter sticks → LED brightness changes slightly

### Method B: Transmitter Bind Mode

Some transmitters have bind mode:

1. **Transmitter OFF**

2. **Hold bind button on receiver**

3. **Plug in Teensy USB (power receiver)**

4. **Receiver LED flashing rapidly**

5. **On transmitter:**
   ```
   Menu
   └─ System Setup
      └─ RX Bind
         └─ Start Bind
   ```

6. **Turn ON transmitter in bind mode**
   - Some transmitters: Hold bind switch while powering on

7. **Wait for solid LED**

8. **Exit bind mode on transmitter**

---

## ✅ STEP 4: Verify Binding

### Check LED Status

| LED Pattern | Meaning | Action |
|-------------|---------|--------|
| Solid ON | Bound & connected | ✓ Good! |
| Slow flash (1/sec) | Bound but no signal | Turn on transmitter |
| Fast flash (2-3/sec) | Not bound | Repeat binding |
| OFF | No power | Check wiring |

### Test Communication

1. **Upload test program:**
   ```bash
   # Use main_COMPLETE.cpp with:
   # Uncomment printRadioData() in loop()
   
   pio run -e teensy40 -t upload
   pio device monitor
   ```

2. **Move transmitter sticks:**
   ```
   Expected output:
   CH1:1500 CH2:1500 CH3:1000 CH4:1500 ...
   
   Move RIGHT stick:
   CH1:1789 CH2:1456 CH3:1000 CH4:1500 ...
   ```

3. **Test all channels:**
   - Right stick L/R → CH1 changes (Roll)
   - Right stick U/D → CH2 changes (Pitch)
   - Left stick U/D → CH3 changes (Throttle)
   - Left stick L/R → CH4 changes (Yaw)
   - Switches → CH5/CH6 change

---

## 🔧 Troubleshooting Binding

### LED Won't Flash During Binding

**Problem:** LED stays solid or off
**Causes:**
1. Bind button not pressed early enough
2. Bind button stuck
3. Wrong power sequence

**Solution:**
```bash
# Try this sequence:
1. Unplug Teensy USB
2. Press and HOLD bind button on receiver
3. While holding, plug in USB
4. Keep holding for 5 seconds
5. LED should start flashing rapidly
```

### LED Flashes Then Goes Solid Immediately

**Problem:** Receiver already bound to another transmitter
**Solution:** This is OK if it's YOUR transmitter!
- Test channels with pio device monitor
- If not your transmitter, need to rebind

### Can't Find Bind Button

**Check these locations:**
- Top of receiver near antenna
- Side of receiver
- Under label (peel back carefully)
- May be tiny hole (need paperclip)

**If really can't find:**
- Check receiver manual
- Try "long press" bind: Hold power on for 10 seconds

### Transmitter Won't Enter Bind Mode

**FlySky FS-i6:**
```
Menu → System → RX Bind → Bind
```

**FlySky FS-i6X:**
```
Hold bind/menu button while powering on
OR
Menu → Function → RX Bind
```

---

## ⚡ Power Considerations

### USB Power Limits

**Teensy 4.0 via USB:**
- Max current: 500mA
- Teensy itself: ~100mA
- FS-iA6B receiver: ~70mA
- **Total: ~170mA** ✓ Well within limits

**Safe for binding:** ✓ YES
**Safe for testing receiver:** ✓ YES
**Safe for flight:** ❌ NO (need battery)

### For Flight Testing

When you add motors/servos, USB power is NOT enough:

```
Final Power Setup:
┌────────────────────────────────────┐
│ Battery (7.4V-11.1V LiPo)          │
│    ↓                               │
│ BEC/Regulator (5V output)          │
│    ↓                               │
│    ├──→ ESCs → Motors              │
│    ├──→ Teensy VIN                 │
│    └──→ Receiver VCC               │
└────────────────────────────────────┘
```

---

## 🔄 Re-binding (if needed)

If you need to bind to different transmitter:

1. **Unbind old:**
   - Not necessary, rebinding overwrites

2. **Follow binding steps above**
   - New transmitter will overwrite old binding

3. **Verify:**
   - Old transmitter won't work anymore
   - New transmitter should control receiver

---

## 📊 Binding Checklist

Before binding:
- [ ] Transmitter set to SBUS mode
- [ ] Transmitter powered OFF
- [ ] Receiver wired to Teensy correctly
- [ ] Bind button located on receiver

During binding:
- [ ] Bind button held BEFORE powering on
- [ ] Receiver LED flashing rapidly
- [ ] Transmitter powered ON
- [ ] Wait for solid LED

After binding:
- [ ] LED solid when transmitter ON
- [ ] LED flash when transmitter OFF
- [ ] Channels respond to stick movement
- [ ] All 6 channels working (test with serial monitor)

---

## 🎯 Next Steps After Binding

Once receiver is bound and working:

1. **Test all channels:**
   ```cpp
   // In main.cpp loop(), uncomment:
   printRadioData();
   ```

2. **Verify channel mapping:**
   - Right stick L/R = Roll (CH1)
   - Right stick U/D = Pitch (CH2)
   - Left stick U/D = Throttle (CH3)
   - Left stick L/R = Yaw (CH4)

3. **Set up failsafe:**
   - Move sticks to desired failsafe positions
   - Turn off transmitter
   - Hold set/bind button for 2 seconds
   - Turn transmitter back on

4. **Calibrate transmitter endpoints:**
   - Check that full stick travel gives 1000-2000μs
   - Adjust EPA if needed

---

## 💡 Tips & Tricks

### Bind Without Antennas

- **Can bind without antennas attached**
- Keep transmitter close (< 1 meter)
- Once bound, attach antennas for range

### Multiple Receivers

- **Each receiver must be bound individually**
- Cannot bind multiple receivers to one transmitter simultaneously
- Can have backup receiver already bound (hot spare)

### Bind Indicator

Some receivers have two-color LED:
- Red flashing = bind mode
- Red solid = bound but no signal
- Green solid = bound and receiving

---

## ❓ FAQ

**Q: Can I bind using Teensy USB power?**
A: Yes! 70mA receiver draw is well within USB limits.

**Q: Do I need to rebind every time?**
A: No, binding is permanent until you rebind to different transmitter.

**Q: What if I don't have a bind button?**
A: Some receivers bind by holding set/failsafe button during power-on.

**Q: Can I bind multiple times?**
A: Yes, rebinding just overwrites old binding. No limit.

**Q: Will binding erase my transmitter settings?**
A: No, transmitter settings are unaffected by binding.

**Q: Must transmitter be in SBUS mode to bind?**
A: Yes! Set SBUS mode BEFORE binding, then power cycle transmitter.

**Q: Can I change from SBUS to iBUS later?**
A: Yes, but you'll need to change wiring and code. Stick with SBUS for now.

---

## 🚨 Common Mistakes

1. **Binding before setting SBUS mode**
   - Set SBUS mode FIRST
   - Power cycle transmitter
   - THEN bind

2. **Transmitter ON before pressing bind button**
   - Transmitter must be OFF
   - Press bind button
   - THEN power on transmitter

3. **Not holding bind button long enough**
   - Hold through entire binding process
   - Wait for solid LED before releasing

4. **Wrong power pin**
   - Use VIN, not 5V pin
   - VIN handles more current

---

**Success = Solid LED + Channels responding to transmitter!**

Next: Test receiver with printRadioData(), then move on to PID tuning!