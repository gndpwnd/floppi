# Research: GPS Accuracy Improvement

**Status**: Not Started  
**Priority**: Medium (nice-to-have for v1.0, focus for v1.1)  
**Last Updated**: 2026-05-05

---

## Problem Statement

Ublox NEO-M9N has nominal accuracy of ±1 meter (95% CEP). When stationary, we can improve accuracy to ~0.1m by:
- Taking multiple position samples
- Applying statistical averaging
- Using CEP (Circular Error Probability) as accuracy metric

**Goal**: Understand and implement multi-sample accuracy improvement for stationary GPS fixes.

---

## Questions to Answer

1. What is CEP (Circular Error Probability) and how is it calculated?
2. What sampling strategy gives best accuracy improvement (how many samples, how long)?
3. Does NEO-M9N firmware support averaging / static mode?
4. How do we validate that device is stationary (accelerometer + GPS)?
5. What's the optimal output rate for multi-sample averaging?
6. How does magnetic declination affect position (if at all)?

---

## Research Progress

- [ ] Understand CEP metric and statistical position averaging
- [ ] Review NEO-M9N datasheet for static/averaging modes
- [ ] Research "DGPS" and RTK techniques (if applicable)
- [ ] Design stationary detection algorithm (accel-based)
- [ ] Implement multi-sample averaging on board or in Python
- [ ] Test accuracy improvement on actual hardware
- [ ] Document results and recommended sample count

---

## Key References

- Ublox NEO-M9N Datasheet: (to be sourced)
- NIST/GPS Accuracy Resources
- Adafruit NEO-M9N Hookup Guide
- MDC's email notes on GPS accuracy

---

## Findings

### 1. GPS Accuracy Fundamentals

#### CEP (Circular Error Probability)
- **Definition**: The radius of a circle centered on true position that contains 50% of measurements
- **NEO-M9N Spec**: 1.5m CEP (40% better than M8N), real-world ~0.8-1.2m in open sky
- **Calculation**: CEP = sqrt(MSE) = sqrt(variance_range + variance_azimuth + covariance_terms + bias²)
- **Related metrics**: 
  - DRMS (Distance Root Mean Square): ~1.4x CEP, 63% confidence
  - R95: ~2.3x CEP, 95% confidence
  - HDOP (Horizontal Dilution of Precision): Unitless factor indicating horizontal accuracy

#### Multi-Constellation Advantage
- NEO-M9N simultaneously tracks GPS, GLONASS, BeiDou, Galileo
- Over-determined solution (often 20+ satellites) dramatically reduces single-constellation failure
- Reduces impact of multipath, signal loss, and bad satellites

### 2. Multi-Sample Accuracy Improvement

#### Sample Count Formula
For **N uncorrelated samples**, accuracy improvement follows: **Error_improved = Error_original / √N**

**Practical examples for ±1m CEP improvement:**
- 10 samples (10 sec @ 1Hz) → ±0.3m CEP (√10 ≈ 3.16× improvement)
- 30 samples (30 sec @ 1Hz) → ±0.18m CEP (√30 ≈ 5.48× improvement)
- 100 samples (100 sec @ 1Hz) → ±0.1m CEP (√100 = 10× improvement)

**Consensus from research**:
- Window of 10 samples minimum for noticeable improvement
- 30-100 samples typical for 0.1m accuracy target
- Diminishing returns after 100 samples (noise floor, clock drift dominate)

**Update Rate Optimization**:
- 1 Hz ideal for static positioning (longest integration time per sample)
- Higher rates (10-25 Hz) reduce accuracy per sample due to shorter signal integration
- For averaging: use 1 Hz output rate despite capability for higher rates

#### Averaging Method
1. **Simple arithmetic mean**: Best for Gaussian-distributed errors
   ```
   avg_lat = sum(samples_lat) / N
   avg_lon = sum(samples_lon) / N
   ```
2. **Median filtering**: Better rejection of outliers
3. **Weighted averaging**: Can weight recent samples higher (but minimal benefit for static)

### 3. NEO-M9N Static Mode Configuration

#### Available Firmware Modes
The NEO-M9N supports configurable dynamic platform models via **CFG-NAVSPG-DYNMODEL**:
- **Portable** (0): Default, general-purpose
- **Fixed** (2): For stationary antennas, updates only time/frequency
- **Pedestrian** (3): Walking speeds with acceleration limits
- **Automotive** (4): Typical car speeds
- **Sea** (5): Marine vessels
- **Airborne 1g/4g/6g** (6/7/8): Aircraft (g-force limitations)

**For static positioning in v1.0**: Set to **Fixed mode (2)** to prevent filter from tracking position noise as movement.

#### Configuration Interface
- **Tool**: u-center software (UbloxCenter) for configuration
- **Protocol**: UBX proprietary binary protocol (preferred) or NMEA protocol
- **Persistence**: Settings saved to flash memory via CFG-CFG command
- **Alternative**: Can configure via UART at boot time using AT commands or Python

**Recommended settings for static accuracy**:
```
- Dynamic Model: Fixed (2)
- Output Rate: 1 Hz (1000ms between fixes)
- SBAS Enabled: Yes (augments with WAAS/EGNOS if available)
- Multi-constellation: Yes (GPS + GLONASS + BeiDou + Galileo)
```

### 4. Stationary Detection Algorithm

#### Accelerometer-Based Detection (Recommended for auto_orientation)
**Thresholds** (from vehicle/activity detection research):
- **Stationary state**: Acceleration < 0.2 m/s² across all axes
- **Transition to moving**: Acceleration > 0.2 m/s² for >0.5 sec
- **Debounce window**: Require 2-3 consecutive measurements above/below threshold

**Implementation**:
1. Read accelerometer (likely MPU6050/BNO085 already available)
2. Calculate magnitude: `accel_mag = sqrt(ax² + ay² + az²) - 1g`
3. Compare to threshold: `is_stationary = accel_mag < 0.2`
4. Apply exponential moving average: `ema = 0.9 * ema + 0.1 * accel_mag`

#### GPS-Based Detection
- **Velocity threshold**: GPS speed < 0.1 m/s for >20 seconds
- **Advantage**: Works in GPS-only mode, doesn't require IMU
- **Disadvantage**: Lag during GPS outages, slower lock-on

#### Combined Approach (Optimal)
- Use **accelerometer as primary** (always available, real-time)
- Use **GPS velocity as secondary** confirmation (validates stationary state from satellite perspective)
- **Hysteresis**: 
  - Moving→Stationary: Both accel < 0.15 m/s² AND GPS speed < 0.2 m/s²
  - Stationary→Moving: Either accel > 0.25 m/s² OR GPS speed > 0.5 m/s²

### 5. RTK vs DGPS vs Simple Averaging

#### Quick Comparison Table
| Method | Accuracy | Cost | Equipment | Complexity |
|--------|----------|------|-----------|-----------|
| Standalone + averaging | ±0.3m CEP | $ | GPS module | Low |
| DGPS (code-based) | ±1-3m | $$ | GPS + corrections | Medium |
| RTK (carrier-phase) | ±1-2cm | $$$ | Base + rover + radio | High |

**For v1.0 (stationary, no infrastructure)**:
- **Simple multi-sample averaging is SUFFICIENT** (±0.1m achievable without external corrections)
- DGPS/RTK require base station infrastructure or NTRIP service subscription
- Not practical for mobile auto_orientation use case in v1.0

### 6. NMEA Sentence Parsing

#### Standard NEO-M9N NMEA Output
The NEO-M9N outputs multiple NMEA 0183 sentence types. For basic positioning, focus on:

**GPGGA (Global Positioning System Fix Data)**
```
$GPGGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x,xx,x.x,x.x,M,x.x,M,x.x,xxxx*hh
         1        2      3  4       5 6  7 8   9   10 11  12 13  14   15
```

| Field | Description | Example |
|-------|-------------|---------|
| 1 | UTC Time | 184353.07 |
| 2 | Latitude | 1929.045 |
| 3 | N/S | S |
| 4 | Longitude | 02410.506 |
| 5 | E/W | E |
| 6 | Fix Quality | 1=GPS, 2=DGPS, 4=RTK, 5=RTK Float |
| 7 | Satellites | 4-21 |
| 8 | HDOP | 2.6 (lower better) |
| 9 | Altitude | 100.00 |
| 10 | Alt Units | M |
| 11 | Geoidal Sep | -33.9 |
| 12 | Geo Units | M |
| 13 | DGPS Age | (seconds, 0 if none) |
| 14 | DGPS ID | (base station ID) |
| 15 | Checksum | *6D |

**GPRMC (Recommended Minimum Navigation Info)**
```
$GPRMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy,x.x,a*hh
```
Provides: time, status, lat/lon, speed (knots), course, date, magnetic variation

**GPGSA (DOP and Active Satellites)**
```
$GPGSA,M,3,01,02,03,04,05,06,07,08,09,10,11,12,1.44,0.89,1.0*30
```
Provides: fix type (1=none, 2=2D, 3=3D), active satellite IDs, DOP values

#### Checksum Format & Validation
- **Format**: `*` followed by 2 hex characters (XOR of all chars between `$` and `*`)
- **Validation**:
  ```python
  def validate_checksum(sentence):
      if '*' not in sentence:
          return False
      data, checksum = sentence.rsplit('*', 1)
      calculated = 0
      for char in data[1:]:  # Skip $
          calculated ^= ord(char)
      return f"{calculated:02X}" == checksum
  ```
- **Error handling**: Discard sentences with invalid checksums (corrupted transmission)

#### Coordinate Format Conversion
NMEA uses degrees + minutes format, GPS uses decimal degrees:
```
NMEA: 1929.045,S  →  Decimal: -19.484083°
      DDMM.MMMM       = -(19 + 29.045/60) = -19.484083

Algorithm:
  degrees = int(coord_str / 100)
  minutes = coord_str % 100
  decimal = degrees + (minutes / 60)
  if direction == 'S' or 'W':
    decimal = -decimal
```

### 7. NMEA Parsing Libraries vs In-House Implementation

#### Option A: In-House Implementation

**Pros**:
- Zero dependencies (pure Python)
- Complete control over error handling
- Lightweight and transparent
- Easy to extend with custom logic (accuracy filtering, etc.)
- No version compatibility issues
- Suitable for embedded Python (Raspberry Pi, etc.)

**Cons**:
- Must handle all NMEA edge cases (incomplete sentences, timeouts, corruption)
- No checksum auto-validation (must implement)
- More code to maintain and test
- Risk of subtle parsing bugs

**Effort**: ~200-300 lines of Python for robust implementation

**Recommended for auto_orientation v1.0**: **YES** - Keep it simple, focus on GPGGA + GPRMC only

#### Option B: pynmea2 Library

**Pros**:
- Well-tested, standard library
- Automatic checksum validation
- Coordinate format conversion built-in
- Handles all NMEA sentence types
- Clean object-oriented API

**Cons**:
- External dependency (pip install pynmea2)
- Overkill if only parsing GGA/RMC
- Slight overhead vs custom parser
- May include features you don't need

**Dependencies**: Minimal, pure Python (compatible with Python 2.7+, 3.4+)

**Use case**: If parsing many sentence types or uncertain about requirements

#### Option C: NeoGPS (Arduino/C++)

**Pros**:
- Extremely efficient (10 bytes RAM, 866 bytes PROGMEM)
- Multiple input streams (Serial, I2C, etc.)
- Recognizes multiple talker IDs (GPS, GLONASS, BeiDou, etc.)
- 37-72% faster than alternatives

**Cons**:
- C++ only (auto_orientation is Python-based)
- Arduino-specific ecosystem
- Not applicable for this project

**Verdict**: Not suitable for Python-based auto_orientation

#### Recommendation for auto_orientation v1.0
**Option A (In-House)** is preferred because:
1. Focus on 2-3 core sentences (GPGGA, GPRMC) only
2. Simplifies dependency management
3. Easier to integrate accuracy filtering/multi-sample logic
4. Natural fit with Python ecosystem
5. Can always migrate to pynmea2 in v1.1 if scope expands

---

## Recommendations

### For v1.0 (MVP - Minimum Viable Product)

#### 1. Accuracy Improvement Strategy
**Use multi-sample averaging with stationary detection:**
- Collect 30 samples @ 1 Hz while stationary detected
- Average latitude, longitude independently
- Expected improvement: ±1m → ±0.2m CEP (~5-6x reduction)
- Implement accelerometer-based stationary detection (already have MPU6050/BNO085)
- Threshold: accel_mag < 0.2 m/s² for 3+ consecutive measurements

**Algorithm pseudocode**:
```python
def get_accurate_position():
    samples = []
    stationary_count = 0
    
    while stationary_count < 30:
        accel = read_accelerometer()
        gps = read_gps_sentence()
        
        if magnitude(accel) < 0.2:
            samples.append(gps.position)
            stationary_count += 1
        else:
            samples.clear()
            stationary_count = 0
    
    avg_lat = mean([s.lat for s in samples])
    avg_lon = mean([s.lon for s in samples])
    return (avg_lat, avg_lon)
```

#### 2. NMEA Parsing Approach
**Implement lightweight in-house parser targeting GPGGA + GPRMC:**

**Minimum required extraction**:
- Sentence type (GGA, RMC)
- Time (UTC)
- Latitude, Longitude (DDMM.MMMM format)
- Fix quality (0-5)
- Satellite count
- HDOP
- Altitude (from GGA)
- Speed in knots (from RMC)
- Date (from RMC)
- Checksum validation

**Error handling**:
1. Verify sentence starts with `$` and contains `*`
2. Validate checksum (XOR of chars between $ and *)
3. Discard on mismatch
4. Handle incomplete sentences (state machine or line buffering)
5. Timeout: if no complete sentence in 5 seconds, reset buffer
6. Skip sentences with fix_quality = 0 (invalid fix)

**Parser signature**:
```python
class NMEAParser:
    def parse_gga(sentence: str) -> dict:
        # Returns: {time, lat, lon, fix_quality, satellites, hdop, altitude}
    
    def parse_rmc(sentence: str) -> dict:
        # Returns: {time, lat, lon, speed_knots, course, date}
    
    def validate_checksum(sentence: str) -> bool:
        # Returns: True if valid checksum
    
    def dms_to_decimal(coord_str: str, direction: str) -> float:
        # Converts NMEA DDMM.MMMM to decimal degrees
```

#### 3. NEO-M9N Configuration
- Configure to Fixed mode (dynamic model 2) via u-center or UART
- Set output rate to 1 Hz for best averaging
- Enable multi-constellation (GPS + GLONASS + BeiDou + Galileo)
- Enable SBAS (WAAS/EGNOS) if available in region

#### 4. Stationary Detection
- Use existing accelerometer readings from BNO085/MPU6050
- Calculate magnitude: `accel_mag = sqrt(ax² + ay² + az²) - 1g`
- Apply exponential moving average for smoothing
- Threshold: < 0.2 m/s² sustained for 2-3 sec
- Only run position averaging when stationary

### For v1.1+ (Enhancements)

1. **RTK Integration**: If infrastructure/NTRIP available in region
2. **Kalman Filtering**: Fuse GPS with IMU velocity estimates
3. **Library Migration**: Switch to pynmea2 if parsing scope expands
4. **Rate Adaptation**: Auto-adjust sample count based on satellite count/HDOP
5. **Magnetic Declination**: Incorporate local declination for yaw correction validation

### Testing Recommendations

1. **Static test**: Place antenna on fixed surface, collect 100 samples, verify 0.2m CEP
2. **Movement test**: Walk with device, verify stationary detection responds correctly
3. **Multipath test**: Test near buildings/trees to observe accuracy degradation
4. **Checksum test**: Inject corrupted sentences, verify parser rejects
5. **Timeout test**: Simulate GPS loss, verify graceful handling

---

## Implementation Notes

- **Altitude extraction**: Include from GPGGA for 3D positioning (important for drones)
- **Speed filtering**: Use GPS speed as secondary check for stationary state (speed < 0.1 m/s)
- **HDOP monitoring**: Log HDOP values; if > 5, consider increasing sample count
- **Satellite count**: 8+ satellites recommended; < 4 = no valid fix (skip)
- **Magnetic declination**: Doesn't directly affect GPS position, but important for:
  - Yaw estimation from GPS course (GPRMC field)
  - Compass heading validation in auto_orientation
  - Apply correction: `true_heading = magnetic_heading + local_declination`
  - Source: NOAA magnetic declination database by lat/lon

---

## Key References & Sources

- [NEO-M9N Datasheet](https://content.u-blox.com/sites/default/files/NEO-M9N-00B_DataSheet_UBX-19014285.pdf)
- [NEO-M9N Integration Manual](https://content.u-blox.com/sites/default/files/NEO-M9N_Integrationmanual_UBX-19014286.pdf)
- [SparkFun GPS NEO-M9N Hookup Guide](https://learn.sparkfun.com/tutorials/sparkfun-gps-neo-m9n-hookup-guide/all)
- [GPS Accuracy Comparison: NEO-6M vs NEO-M8N vs NEO-M9N - Zbotic](https://zbotic.in/gps-accuracy-comparison-neo-6m-vs-neo-m8n-vs-neo-m9n/)
- [NMEA Sentence Information - APRS](https://aprs.gids.nl/nmea/)
- [NMEA GGA Sentences - Lefebure](https://lefebure.com/articles/nmea-gga/)
- [Real-time averaging of position data from multiple GPS receivers - Purdue](https://web.ics.purdue.edu/~minb/pub/measurement2016.pdf)
- [An Effective Approach to Improving Low-Cost GPS Positioning Accuracy - NIH](https://pmc.ncbi.nlm.nih.gov/articles/PMC4099514/)
- [Circular Error Probable - Wikipedia](https://en.wikipedia.org/wiki/Circular_error_probable)
- [NeoGPS Arduino Library - GitHub](https://github.com/SlashDevin/NeoGPS)
- [pynmea2 Python Library - GitHub](https://github.com/Knio/pynmea2)

