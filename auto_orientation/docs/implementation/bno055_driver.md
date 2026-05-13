# BNO055 Driver

**Source**: `src/sensors/bno055.{h,cpp}`
**Phase**: 4.6 — BNO055 driver (also fixes Known Issue **KI-2**: Euler-angle 90° pitch discontinuity).
**Decision rows**: [D2, D3, D4](../findings/MASTER_DESIGN.md) in `findings/MASTER_DESIGN.md`.

## Purpose

Concrete `OrientationSensor` implementation backed by the Adafruit_BNO055 library. Provides absolute-orientation quaternions from the chip's onboard 9-DoF fusion (NDOF mode, magnetometer included) and exposes the 22-byte calibration offset/radius blob so it can be persisted via the storage HAL. Sits next to the existing `BNO085` driver in the sensor abstraction — runtime-polymorphic on Teensy/ESP32 (vtable cost is free; base class is already virtual), compile-time-selected on AVR.

## Data flow

```
I2C bus (0x28 default, ADR=GND)
        │
        ▼
   Adafruit_BNO055 library (lib_deps; .cpp-only include)
        │  getQuat()  → imu::Quaternion(w,x,y,z)
        │  getCalibration(&sys,&gyro,&accel,&mag)
        ▼
   BNO055::read()
        ├─ store w,x,y,z into OrientationData.quaternion
        ├─ quaternion_to_euler_degrees() → roll/pitch/yaw_deg
        ├─ copy four 0-3 accuracies into OrientationData
        └─ stamp last_read_ms_; set new_data_ = true
        │
        ▼
   OrientationData (via getOrientation())
        │
        ▼
   consumer: SensorOutputManager / EKF / balance-loop
```

## Core algorithm

```text
begin():
    bno_ = new Adafruit_BNO055(55, i2c_address_, &Wire)
    if !bno_->begin(OPERATION_MODE_NDOF): return false
    if use_ext_crystal_: bno_->setExtCrystalUse(true)
    initialized_ = true; return true

read():
    q = bno_->getQuat()
    data_.quaternion = {q.w(), q.x(), q.y(), q.z()}
    quaternion_to_euler_degrees(data_.quaternion → roll/pitch/yaw)
    bno_->getCalibration(&sys, &gyro, &accel, &mag) → data_.calibration_*
    new_data_ = true; last_read_ms_ = millis(); return true

getCalibrationProfile(buf, len_out):
    if !bno_->isFullyCalibrated(): return false  # Adafruit refuses partial reads
    bno_->getSensorOffsets(cal_blob_)            # 22 B
    cal_blob_valid_ = true; memcpy buf; *len_out = 22; return true

setCalibrationProfile(buf, 22):
    bno_->setSensorOffsets(buf); return true
```

**Critical design choice (D4)**: we read the *quaternion* via `getQuat()` and derive Euler through `math/quaternion_conversions.h`. We deliberately do NOT call `getEvent(&e, VECTOR_EULER)` — the BNO055's native Euler output has a documented 90°-pitch discontinuity (Adafruit datasheet errata).

The Adafruit_BNO055 headers stay scoped to the `.cpp`; `bno055.h` uses a forward declaration so callers don't transitively drag in `imu::Quaternion`, `Adafruit_Sensor.h`, or `utility/imumaths.h`.

## Buffer / RAM costs

- `BNO055` instance: ~80 B (Adafruit pointer + OrientationData + 22 B calibration blob + flags).
- Heap allocation: 1× `Adafruit_BNO055` (~200 B per the library) at `begin()`.
- Static (Adafruit lib): TwoWire buffer (~32 B).

Compiles on AVR (Arduino Mega 2560) when `USE_BNO055` is defined; the build env pulls in `Adafruit_BNO055` and `Adafruit_Sensor` via `lib_deps`. Without `USE_BNO055`, the `.cpp` body is `#ifdef`-blank so non-BNO055 envs don't link the library.

## Integration points

- **Called by**: any app instantiating the sensor abstraction (balancing robot uses it as the canonical IMU). Calibration profile flows out to the HAL via `persistent_storage` + tagged-blob layer (D2).
- **Gating**: `-D USE_BNO055` in the build env (e.g., `arduino_mega_bno055`, `arduino_mega_balancing`).
- **Body frame**: chip-native (+X forward, +Y left, +Z up). Unifying with BNO085's frame is tracked as Phase 4.6.5 — for now consumers that mix sensors must remap themselves.
- **Cross-link**: design rationale in [`findings/bno055_driver_and_multi_imu_strategy.md`](../findings/bno055_driver_and_multi_imu_strategy.md).

## Tests

- `tests/test_bno055.cpp` — split harness:
  - **Native (host)**: googletest. Exercises the `quaternion → Euler` pipeline the driver delegates to. Built when `BUILD_NATIVE_TESTS` or `GTEST_API_` is defined.
  - **Hardware bring-up**: gated by `#ifdef ARDUINO_HW_TEST`. Exercises `begin()`, `read()`, `getCalibrationProfile()`, `setCalibrationProfile()` against a real BNO055 on Wire. Manual bench run; not in CI.
- Run native: `pio test -e native_test -f test_bno055` (or compile with `g++ -DBUILD_NATIVE_TESTS`).
- Mocking Adafruit_BNO055 wholesale was considered and rejected — it would shadow ~24 library symbols for negligible value over the existing math-pipeline coverage.
