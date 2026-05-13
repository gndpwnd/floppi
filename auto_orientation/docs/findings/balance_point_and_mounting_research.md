# Balance-Point Capture and Mounting-Angle Estimation

Research notes for the hands-off self-balancing robot layer in
`auto_orientation/`. Target: Arduino Mega 2560, BNO085 (or BNO055), L298N
drive, ~8 KB SRAM / 256 KB flash.

---

## Recommendation summary

- **Capture method:** hybrid *user-assisted gravity vector + gyro-stillness
  gate*. User holds the bot at its visual balance point and presses a button;
  firmware confirms `|gyro| < 0.5 deg/s` for 500 ms, averages ~200 accel
  samples, commits a quaternion offset. ~40 B RAM, no motor-current sensing.
- **Persist as a quaternion delta** (4 floats = 16 B) plus version + CRC8 in
  the existing 256 B EEPROM calibration slot. Replaces the hand-tuned
  `PITCH_OFFSET=-8.6` scalar with a proper 3D rotation so any chassis pose
  works.
- **Use a lean controller**, not the 16-state EKF. 2-state Kalman (pitch +
  gyro bias) feeding a cascaded PID (inner angle, outer wheel velocity). The
  16-state EKF in `src/navigation/ekf.h` belongs to the GPS-fusion layer and
  is too heavy for the 200 Hz balance loop on AVR.

---

## 1. Balance-point capture methods

The true balance point is the chassis attitude where the centre of mass (CoM)
sits over the wheel axle. Reading "level by IMU" is wrong: the IMU is mounted
at some arbitrary angle, and battery / cable / SD-card placement shift the
CoM. We must *measure* the pose, not assume it.

- **(a) Accel-only gravity vector while held still.** Average ~200 accel
  samples (~1 s at 200 Hz); gravity dominates and the normalised vector
  gives pitch in the IMU frame. Trivial, ~12 B RAM. Downside: trusts the
  user not to wobble. Accuracy ~0.3 deg with a steady hand.
- **(b) Gyro zero-rate gate.** Only sample when `|gyro|` stays below ~0.5
  deg/s for N frames. ~8 B RAM. Alone it gives no pitch — it's a *gate*,
  not a measurement.
- **(c) Static torque balance via motor current.** Search for the pose where
  required current crosses zero. Measures *true* mechanical balance,
  including CoM offset. Rejected: L298N current sense is coarse, and the
  robot moves during calibration — bad for a first-time user UX.
- **(d) Hybrid (recommended).** Combine (a)+(b) into a one-button gesture:

```c
// pseudocode — ~40 B locals
while (millis() - last_motion < 500) {
    read_gyro(g);
    if (norm(g) > 0.5 /*deg/s*/) last_motion = millis();
}
vec3 sum = {0,0,0};
for (i = 0; i < 200; i++) { read_accel(a); sum += a; delay_us(5000); }
vec3 g_body = normalize(sum / 200);
quat q_mount = quat_from_gravity(g_body);   // see Section 2
save_to_eeprom(q_mount);
```

Conceptually this is what Bosch's BNO055 accelerometer self-cal does
internally, exposed as a 2-second user gesture. No extra hardware.

## 2. Mounting-angle estimation and storage

`g_body = [gx, gy, gz]` (unit) says where "down" is in IMU frame. Robot body
frame defines "down" as `[0, 0, -1]`. The mount rotation is the *shortest-arc
quaternion* rotating one onto the other (Melax, *Game Programming Gems 1*,
2000):

```
v       = cross(g_body, [0,0,-1])
s       = sqrt((1 + dot(g_body, [0,0,-1])) * 2)
q_mount = { s*0.5, v.x/s, v.y/s, v.z/s }    // w, x, y, z
```

Pitch is then read from `q_mount^-1 * q_imu` converted to Euler. By
construction it is exactly 0 at the captured pose. This handles arbitrary 3D
mounting in one shot, unlike a single Euler offset.

### EEPROM record (fits the 256 B slot)

```c
struct AutoOrientRecord {       // 24 bytes
    uint8_t  magic;             // 0xA0
    uint8_t  version;           // 0x01
    uint16_t pad;
    float    q_mount[4];        // 16 B  shortest-arc mount quaternion
    uint16_t accel_samples;     // QC: samples averaged
    uint8_t  gyro_max_dps_x10;  // QC: max gyro during capture * 10
    uint8_t  crc8;
};
```

Concatenate behind the BNO085 profile, or use the documented backup slot
at `0x100`. Reuse `calculateCRC8()`. If the record fails CRC on boot, fall
back to identity quaternion and prompt re-capture.

## 3. Inverted-pendulum control essentials

Minimum useful state for a two-wheel balancer:

1. **Pitch** `theta` (rad) — fused accel + gyro.
2. **Pitch rate** `theta_dot` (rad/s) — gyro Y minus bias.
3. **Wheel velocity** `v_wheel` — encoders if present, else PWM proxy.
4. *Optional* **position** `x` — only for station-keeping; not needed to stay
   upright.

Roll, yaw, accel bias, GPS — irrelevant for the balance loop.

### Is the 16-state EKF appropriate?

No, not for the inner loop. Propagating a 16x16 covariance is ~1 KB SRAM plus
heavy matmul per tick — designed for ~50 Hz GPS fusion, not a 200 Hz balance
tick on AVR. Keep that EKF for navigation. For balancing use the standard
2-state Kalman (Lauszus / TKJ Electronics, 2012):

```
State:   x = [theta, b_gyro]^T
Predict: theta  <- theta + (gyro_y - b_gyro) * dt
         b_gyro stays
Measure: theta_acc = atan2(-ax, sqrt(ay^2 + az^2))
Update:  2x2 Kalman gain
```

~40 B RAM, ~150 us per tick on AVR. The bias state matters: cheap MEMS bias
drifts visibly over a 5-minute run, which a complementary filter cannot
track.

### Controller and auto-tune

Cascaded PID: inner loop on `(theta - 0)` (zero because `q_mount` cancelled
the offset); outer loop on wheel-velocity error nudges the setpoint a few
tenths of a degree to prevent drift — same structure as Segway / LegWay.
Auto-tune the inner loop with relay feedback (Åström–Hägglund, 1984):
inject a bang-bang oscillation, measure period and amplitude, derive
Ziegler–Nichols gains in ~10 s while the user keeps a hand near the bot.

## 4. Standard references

- **Joop Brokking — YABR (2017).** Arduino Uno + MPU6050 + steppers,
  complementary filter, single-loop PID, hand-tuned. *Borrow:* 5 ms tick,
  motor deadband. *Reject:* hand-coded gains, raw register reads (we have a
  HAL).
- **Kristian Lauszus — Balanduino (2012).** ~60 LOC 2-state Kalman, the
  de-facto Arduino reference. *Borrow* verbatim.
- **Anouar Achghaf — self-balancing-robot (GitHub, 2020).** ESP32 +
  MPU6050, cascaded PID + encoders. *Borrow:* cascaded topology, encoder
  drift correction. *Reject:* FreeRTOS task layout (Mega is bare-metal).
- **David Anderson — nBot (2003) / MIT 6.270.** Historical reference for
  the linearised pendulum model — useful only to sanity-check gain ranges.
  *Reject:* hand-derived LQR; relay-feedback tune ships faster.

---

## References

- Melax, S. "The Shortest Arc Quaternion." *Game Programming Gems 1*, 2000.
- Lauszus, K. "A practical approach to Kalman filter." TKJ Electronics /
  Balanduino source, 2012.
- Åström, K. J., Hägglund, T. "Automatic tuning of simple regulators."
  *Automatica* 20(5), 1984.
- Brokking, J. "YABR — Yet Another Balancing Robot," 2017.
- Anderson, D. "nBot Balancing Robot," 2003.
  <http://www.geology.smu.edu/~dpa-www/robo/nbot/>
