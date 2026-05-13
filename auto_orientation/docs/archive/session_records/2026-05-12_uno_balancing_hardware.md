# Session Record — 2026-05-12 — Uno + BNO055 Hardware Bring-up

> See also: [../../MINIMIZE_ACCELERATIONS_PHILOSOPHY.md](../../MINIMIZE_ACCELERATIONS_PHILOSOPHY.md) — the design direction that grew out of this session.

**Type**: Hardware test + iterative firmware tuning. Living doc — issues and solutions captured in real time as we hit them.
**Hardware**: Arduino Uno + BNO055 (I2C 0x28, ADR=GND) + L298N motor driver + DC motors + battery pack.
**Build env**: `arduino_uno_balancing`.

---

## Final architecture (what the bot is doing as of session end)

**Persistence policy** (user clarified preference):

| Item | Persisted? | Why |
|------|------------|-----|
| BNO055 sensor offsets (22 B) | ✅ EEPROM @ 0x000 | Hardware property — takes minutes to redo |
| Mounting offset (8 B) | ✅ EEPROM @ 0x200 | Physical mounting geometry — doesn't change unless IMU is moved |
| PID gains | ❌ Hardcoded defaults | User preference: gains should be *dynamic* (adapted online) — saving them defeats the point. Defaults `Kp=65 Ki=12 Kd=38` from .ino reference. |

**Boot flow** ("prop-and-go" UX the user wants):

1. Power on
2. Restore BNO055 cal from EEPROM (automatic, ~50 ms)
3. Restore mounting offset from EEPROM (automatic, ~5 ms)
4. PID gains set to .ino defaults
5. If mount offset was restored → **auto-enter RUN after 2 s grace period**
6. If no mount offset → IDLE, wait for `c` (mounting capture) command

**First-time procedure** (only needed once per bot — runs once, saved to EEPROM, never repeated):

1. Flash firmware → BNO055 wizard prompts → rotate device through poses until `gyro=3 accel=3` → saves
2. Hold the bot at its natural balance point → send `c` → mount offset captured + saved
3. Power cycle → bot now auto-RUNs on every boot

**Runtime safety** (live):

- **Tilt limit 10°** → SAFE_FALL (motors stop immediately)
- **Tilt recovery 4°** → auto-resume RUN
- **PID output capped at ±150 during RUN** (full ±255 reserved for future driving/remote-control modes)
- **Dynamic slew rate limit**: 12 / 40 / 100 PWM per tick depending on |pitch| — smooth near balance, fast catch when needed
- **Saturation timeout**: if |motor_pwm| ≥ 200 for > 3 s → SAFE_FALL
- **Online mounting estimator**: slowly tracks balance-point drift over runtime (5-min time constant)

---

## Issues hit + solutions

### 1. Wizard pre-load: silent EEPROM corruption from a stale port holder
- **Issue**: Python serial monitor session held the port open in the background; new pyserial reads failed with "device disconnected".
- **Solution**: `fuser /dev/ttyACM0` to see holders; `fuser -k /dev/ttyACM0` to release; ModemManager note from MEMORY.md (`sudo systemctl stop ModemManager` if reads are flaky).

### 2. BNO055 calibration ALL stuck at 0 for 2 minutes
- **First diagnosis**: looked like external-crystal mismatch or I2C clock stretching.
- **Actual cause**: false alarm — pitch/roll/yaw values were updating fine (chip was fusing). The wizard's `gyro=3 accel=3` thresholds were correct, the user just needed clearer motions.
- **Solution**: added live raw-accel readout (`ax/ay/az` in m/s²) so the operator can SEE which axis is "down" at any moment. Goal: hit `accel=3` quickly via discrete axis-aligned poses with hard 2 s pauses.
- **Threshold relaxation**: changed from "all 4 = 3 (gold)" to "gyro=3 AND accel=3" (mag and sys don't care). Balance bot only cares about pitch — mag affects yaw only.

### 3. Cal "back to zero" mid-session
- **Issue**: User saw accel go from 0→1→0.
- **Explanation**: BNO055 continuously reassesses its calibration. Values CAN drop when the chip detects motion that doesn't match its current model. This is normal. The 22-byte blob gets saved the moment we see `gyro=3 AND accel=3` simultaneously — even briefly. Once saved, value transients afterwards don't matter.

### 4. Motors not spinning after boot, then went full blast when battery connected
- **Issue**: User triggered auto-tune before connecting motor power, then connected battery during tune → motors slammed.
- **Cause**: The relay-feedback tuner drives motors at ±150 (now ±50 after this session) continuously until oscillation is detected. With no power, no motion, no oscillation — tuner holds output at one extreme indefinitely until timeout.
- **Solution**: decoupled CAPTURE from AUTO_TUNE — `c` no longer auto-transitions into tune. Tune now only fires when explicitly triggered (`t` over serial / long-press button). Motor power asserted before `t` is the operator's responsibility.

### 5. Wheel direction inverted ("forward" rolled bot backward)
- **Issue**: Default L298N pin mapping from the .ino had IN1/IN2 wired such that positive PWM rolled the bot backward. The balance loop assumes positive PWM = forward.
- **Solution**: swapped IN1↔IN2 and IN3↔IN4 in the `L298NPins` struct in `main.cpp`. Functionally identical to swapping motor leads at the L298N output, but no rewiring needed.

### 6. Right wheel reverse sluggish
- **Issue**: Right motor turned shorter/slower in reverse than forward.
- **Cause**: stiction asymmetry in that motor (common in cheap DC motors with brushes/gearboxes — friction differs by direction).
- **Mitigation**: bumped test PWM from 50→90 so even the higher-stiction direction breaks free cleanly. PID integral term absorbs steady-state asymmetry during balance.

### 7. Motor command "uncontrolled speeds"
- **Issue**: PID slammed motors from 0→max in a single 5 ms tick when seeing a step in pitch. Bot jerked violently and overshot.
- **Solution**: **dynamic slew rate limiter** on motor command. The rate of change of `motor_pwm` is capped per cycle, with the cap growing as tilt grows:
  - `|pitch| < 3°`: ≤ 12 PWM/cycle (~2400 PWM/s — smooth steady-state)
  - `|pitch| < 8°`: ≤ 40 PWM/cycle
  - `|pitch| < 15°`: ≤ 100 PWM/cycle
  - `|pitch| ≥ 15°`: ≤ 255 PWM/cycle (essentially unrestricted)
- Allows full ±255 range to remain available; smoothness near balance plus responsiveness during recovery.

### 8. Motors still full-blast during a fall
- **Issue**: Even with slew limit, between e.g. 8° (when PID demands high output) and 10° (SAFE_FALL), motors are pegged. User wanted them to never blast.
- **Solution**: **cap PID output limits to ±150 during RUN state** (set via `pid_.set_output_limits(-150, 150)` on entering RUN). The full ±255 range stays available for non-balance modes (motor test today, future driving / remote-control). Combined with the 10° SAFE_FALL threshold this means motors never run beyond ±150 for more than a few hundred ms.

### 9. State transitions invisible
- **Issue**: Hard to tell when the bot moved between IDLE / CAPTURE_MOUNTING / AUTO_TUNE / RUN / SAFE_FALL.
- **Solution**: added `[state] -> <STATE>` log line in `enter_state_()`. Visible on every transition.

### 10. State lost on Arduino reset
- **Issue**: Every time Python opens the serial port, the Uno's USB CDC toggles DTR and the chip resets. In-RAM state (tuned gains, captured offset) vanishes.
- **User preference clarified**: PID gains should be *dynamic* — not saved. Mount offset should be saved (physical property).
- **Solution**:
  - Saved: BNO055 cal (slot 0x000), mount offset (slot 0x200, 8 bytes magic+ver+float+crc)
  - Not saved: PID gains (hardcoded defaults on every boot)
  - Auto-RUN on boot if mount offset is restored

---

## User preferences captured in this session

These are durable preferences worth keeping in mind for future work:

1. **Prop-and-go UX**: user shouldn't have to do any per-boot setup. Power on → bot balances.
2. **Dynamic tuning preferred over saved gains**: PID values should adapt online, not be persisted.
3. **Full motor range reserved for non-balance modes**: balance is bounded; driving/remote should be uncapped.
4. **Tight safety**: 10° tilt limit; motors must never run "full blast for more than 3 seconds during balance".
5. **No motors during capture**: `c` should be safe regardless of whether motors are powered.
6. **Visible state transitions**: print every state change to serial for debugging.
7. **Auto-tune is on-demand**, not on-boot — `t` only fires when operator says.
8. **Calibration values that are lost on reboot are unacceptable** — anything that's expensive to redo must persist.

---

## Final serial command reference (after this session)

| Cmd | Action | Motors |
|-----|--------|--------|
| `c` | Mounting capture (records balance angle, saves to EEPROM, returns to IDLE) | off |
| `t` | Auto-tune (relay-feedback at ±50 PWM, ~30 s) | active during |
| `R` | Reset PID to .ino defaults + enter RUN (session-only, not saved) | active |
| `a` | Abort current state, motors stop, return to IDLE | off |
| `s` | Print status: state, pitch, mount offset, output, current Kp/Ki/Kd | no change |
| `r` | Clear BNO055 cal from EEPROM, restart wizard | off |
| `m` | Motor wiring/direction test (each motor + both together) | active during |
| `k` (during cal wizard) | Skip BNO055 calibration | off |
| `p` (during cal wizard) | Force-save current cal levels (warn if gyro/accel < 3) | off |

---

## What's NOT yet done

- Online mounting estimator's freeze gates don't get real gyro_rate / windup signals (passed as 0/false in the skeleton). Phase 4.6.5 deliverable: add `RawIMUAccess` to the sensor base class and wire those signals through.
- No remote control / driving mode yet — but the framework is set up to support it (the `enter_run_with_current_gains` path is balance-only; adding `enter_driving_mode` is straightforward).
- No I2C-bridge to flight_controller — that's Phase 7.
- No WiFi telemetry — that's Phase 6 (ESP32 only anyway, not Uno).

---

## What to try next session

1. **Hands-off boot test**: power-cycle the Uno without any serial connection. Bot should boot, see the saved mount offset, and auto-RUN with default gains. This is the canonical "prop-and-go" UX moment.
2. **Tune iteration**: if .ino defaults don't balance well on this specific bot, run auto-tune (`t`) and let the relay-feedback discover better gains for the session.
3. **Long-running test**: leave the bot running for 5+ minutes. The online mounting estimator should slowly refine the mount offset via the PID integral term. Periodic save kicks in every 60 s of stable RUN.
4. **Drive mode**: design a `D` command that puts the bot into a "driving" state where the operator can command forward/back via serial while balance is maintained. Reuses full ±255 range.

---

*Status: bot mechanics validated (motors spin both directions, IMU streams orientation, EEPROM persists cal+mount). Balance behavior under iteration as gain values and slew limits are tuned to this specific bot. Session ongoing.*
