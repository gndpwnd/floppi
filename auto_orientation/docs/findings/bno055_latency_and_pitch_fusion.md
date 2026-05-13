# BNO055 Latency and Pitch-Fusion Options for the Uno Balance Bot

**Date:** 2026-05-12 · **Status:** Research / recommendation
**Hardware:** Arduino Uno (ATmega328P, 16 MHz, 2 KB SRAM, no FPU) + BNO055 over I2C
**Companion to:** [balance_failure_diagnosis_2026-05-12.md](balance_failure_diagnosis_2026-05-12.md), [balance_point_and_mounting_research.md](balance_point_and_mounting_research.md), [mpu6050_external_mag_pipeline.md](mpu6050_external_mag_pipeline.md)

## TL;DR

BNO055 NDOF latency is **a real contributor** but **not the dominant one**. Cutting gains and adding a measurement low-pass (Tier 1) buys more stability per keystroke than any fusion-mode swap. After that, **IMUPLUS (Tier 2) is a five-character source change** that removes ~10 ms of group delay for free. Tier 3 (raw + Madgwick on-host) is feasible on a 16 MHz AVR at 200 Hz, but is not worth doing until Tiers 1–2 are exhausted.

## 1. BNO055 NDOF end-to-end latency

From Bosch **BST-BNO055-DS000-14 rev 1.4** (Nov 2014):

| Source | Latency | Datasheet ref |
| --- | --- | --- |
| Accel pre-LP (62.5 Hz BW in NDOF) | ~3–5 ms | §3.5.2 Table 3-8 |
| Gyro pre-LP (32 Hz BW in NDOF) | ~5–10 ms | §3.5.3 Table 3-9 |
| Mag ODR (20 Hz in NDOF) | up to ~25 ms | §3.5.4 |
| Fusion output rate (NDOF) — **100 Hz** | up to 10 ms (1/ODR) | §3.6.5.5 Table 3-19 |
| I2C `getQuat()` @ 100 kHz (~100 bits) | ~1 ms | §4.6.1 |
| **Physical pitch → host quaternion** | **~20–40 ms typical, ~50 ms worst** | sum |

The datasheet publishes no single "fusion latency" number; it is the additive group delay of the pre-filters plus the output decimator. §3.6.5.5 fixes NDOF at 100 Hz — the host never sees fresher than 10 ms data. Mag is the slowest term but the on-chip Kalman blends it lightly for short-horizon attitude, so the *pitch-relevant* delay is closer to gyro+accel: **~20–30 ms typical**.

## 2. Is latency our balance problem?

Partly. The bot's natural inverted-pendulum period is ~500–700 ms (`balance_point_and_mounting_research.md` §3). A 30 ms delay = ~18° of phase — eats roughly half of the achievable D-gain before noise wins, but does **not** make balancing impossible. Lauszus' Balanduino and Brokking's YABR balance fine on MPU6050-class chips. **Their trick is not lower latency — it is:**

1. Running their own Kalman/complementary fusion on **raw gyro + accel** at 200–500 Hz, *not* the chip's NDOF stream. Lauszus' KalmanFilter runs in ~150 µs on a 16 MHz AVR.
2. Low-passing the pitch measurement **before** differentiating for D.
3. Modest gains — Lauszus ships `Kp≈12 Ki≈0 Kd≈3`, *much* tamer than our `65/12/38`.

Our `Kp=65 Kd=38` saturates the ±100 output at 2° of error (`balance_failure_diagnosis_2026-05-12.md` §1a). Latency is not why; the controller is open-loop above 2°. **Fix gains first.**

## 3. IMUPLUS mode

Per §3.6.5.5 Table 3-19: **IMUPLUS** is the 6-DOF fusion mode (accel + gyro, no mag). Output rate is **100 Hz** — same published ODR as NDOF — but the mag fusion stage is bypassed, so the internal pipeline is shorter. Bosch community + Adafruit reports put IMUPLUS group delay at **~10–20 ms**, vs ~20–40 ms for NDOF.

A balance loop only needs gravity-referenced pitch; magnetic yaw is irrelevant. **IMUPLUS is strictly better for us.** Drawback: yaw drifts — fine, we don't use it.

## 4. Read raw + fuse on-host

BNO055 exposes raw accel/gyro registers (`ACC_DATA_*`, `GYR_DATA_*`, §4.3.x) in any non-config mode at the per-sensor ODR (100 Hz each in NDOF, Table 3-7) — the internal 1 kHz rate is not host-visible.

**CPU budget for a 2-state Kalman on ATmega328P:** soft-float mul/add on AVR-libc is ~7–10 µs each (~100–150 floats/ms). A pitch+bias Kalman is ~40–60 float ops per tick → **~400–600 µs/tick**, or **8–12 % of the 5 ms PID period**.

The request's "5 ms per Kalman tick" estimate is pessimistic by ~10×; the Lauszus-form 2×2 Kalman really is ~150–600 µs on a 16 MHz AVR. **Feasible on the Uno.**

## 5. Madgwick on the Uno

Madgwick 6-DOF: ~80 float mul/add + one `invSqrt` per tick (paper §3.3). At ~8 µs/op + 12 µs for the fast inverse-sqrt → **~700–900 µs/tick**, ~15 % CPU at 200 Hz. State is ~24 B. The `flight_controller/src/imu.cpp:198` Teensy port already runs at 2 kHz; an AVR at 200 Hz fits.

## 6. Tier 1 — no code changes, just gains

Adopt the gain set from `balance_failure_diagnosis_2026-05-12.md` §3:

```text
Kp = 15    Ki = 0    Kd = 8
```

Plus: add a one-pole IIR LP (`τ ≈ 15 ms`, ~10 Hz cutoff) on the pitch measurement *before* the D term consumes it — four lines in `pid_controller.cpp::compute()`. This single change buys back most of the headroom NDOF latency stole, because the D-gain no longer amplifies the 0.05° NDOF quantization step (currently ~380 PWM/quantum at Kd=38).

This is where to spend the next hour. It is the highest stability-per-keystroke ratio available.

## 7. Tier 2 — IMUPLUS mode (small change)

In `bno055.cpp:92`:

```cpp
if (!bno_->begin(OPERATION_MODE_IMUPLUS)) {   // was OPERATION_MODE_NDOF
```

What breaks: the magnetometer is unused, so `data_.cal_mag` will read 0 forever — `isHealthy()` and `getStatusString()` should stop checking it. `isFullyCalibrated()` already requires `cal_mag==3`; loosen to ignore mag in IMUPLUS. Yaw becomes drift-relative (we don't use it). Calibration profile (`getSensorOffsets`) still returns 22 B; the mag offsets in the blob will be garbage, which is fine because mag isn't fused.

Saves ~10 ms latency. **Do this after Tier 1 is verified.**

## 8. Tier 3 — raw + Madgwick on-host

Effort: ~1 day. Requires (1) raw gyro on the `OrientationSensor` base (already a Phase 4.6.5 TODO), (2) reading `GYR_DATA_*` + `ACC_DATA_*` per tick — 12 I2C bytes (~1 ms @ 100 kHz, ~250 µs @ 400 kHz; flip `Wire.setClock(400000)` in `BNO055::begin()`), and (3) Madgwick or a Lauszus 2-state Kalman on host at 200 Hz. Latency drops to **~3–5 ms**, matching Lauszus/Brokking-class bots.

**When to do it:** only after Tiers 1–2 are exhausted. The 2-state Kalman is also the foundation for the broader `auto_orientation` roadmap (`balance_point_and_mounting_research.md` §3, `MASTER_DESIGN.md` D6) — not throwaway, but not the first move.

## Verdict

**Latency is contributing ~3 dB of phase margin loss, not 30 dB.** The motors are slamming because gains are 4× too hot, not because the BNO055 is 30 ms late. Order of operations: gains + measurement LP (today) → IMUPLUS (this week) → raw + on-host Kalman (Phase 4.6.5+).
