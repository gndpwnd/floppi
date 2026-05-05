# Library Management Strategy

**Updated**: 2026-05-05  
**Philosophy**: Keep external libraries clean and local; reference shared libraries from other projects

---

## Current Setup

### ✅ Adafruit BNO08x Library (Local Copy)
**Location**: `lib/Adafruit_BNO08x_Arduino/`  
**Size**: 228 KB (pruned of examples, docs, git artifacts)  
**Status**: ✅ Ready for v1.0

**Why Local Copy?**
- Core dependency for BNO085 sensor driver
- No git submodules (clean repository)
- Can modify if needed for calibration persistence (SH-2 protocol)
- Pruned of unnecessary files (.github, doxyfile, examples)

**Files Included**:
```
src/
├── Adafruit_BNO08x.cpp/h    (Main library)
├── sh2.c/h                  (SH-2 protocol implementation)
├── sh2_SensorValue.c/h      (Sensor data structures)
├── sh2_util.c/h, shtp.c/h   (Utilities)
├── sh2_err.h, sh2_hal.h     (Error & HAL)
└── library.properties, license.txt
```

**Pruned Away** (not needed for compilation):
- `.github/`, `.gitignore`, `.gitmodules` — Git artifacts
- `Doxyfile` — Documentation generator config
- `examples/` — Example sketches
- `docs/` — API documentation
- `README.md`, `CHANGELOG.md`, etc. — Documentation

---

## Referenced Libraries (Shared Across Projects)

### MPU6050 Library (From flight_controller)
**Location in flight_controller**: `/home/devel/floppi/flight_controller/lib/MPU6050/`  
**Status**: Available for v1.1 MPU6050 support  
**Strategy**: TBD (see decision matrix below)

---

## Library Decision Matrix

| Library | v1.0 | Location | Strategy |
|---------|------|----------|----------|
| **Adafruit_BNO08x** | ✅ Required | `auto_orientation/lib/` | Local copy (pruned) |
| **MPU6050** | 📋 v1.1 | `flight_controller/lib/` | DECISION NEEDED |
| **ArduinoJSON** | ❌ Optional | Not added yet | Use only if needed |
| **GPS NMEA** | ❌ Not used | In-house | Custom implementation |

---

## MPU6050 Strategy: THREE OPTIONS

### Option A: Copy to local/lib/ (RECOMMENDED)
**Pros**:
- Self-contained, doesn't depend on flight_controller
- Can modify for auto_orientation-specific needs
- Cleaner dependency management

**Cons**:
- Duplicates code across projects
- More maintenance burden

**Use when**: Auto_orientation is independent product

### Option B: Reference via platformio.ini
**Pros**:
- Single source of truth
- No duplication

**Cons**:
- Hard-coded path dependency on flight_controller
- Breaks if flight_controller is deleted/moved
- Harder to version independently

**Use when**: Projects are tightly coupled

### Option C: Symlink (NOT RECOMMENDED)
**Pros**:
- Single source, no duplication

**Cons**:
- Git can't track symlinks properly
- Breaks on Windows
- Problematic in CI/CD

**Use when**: Never

---

## DECISION: MPU6050 Library

**Recommended**: **Option A - Copy to local lib/**

**Rationale**:
- auto_orientation is a standalone toolkit
- flight_controller may evolve independently
- MPU6050 driver may be customized for auto_orientation
- Keeps dependencies explicit and local
- Avoids cross-project fragility

**Implementation** (when v1.1 starts):
```bash
# Copy MPU6050 from flight_controller
cp -r ../flight_controller/lib/MPU6050 lib/

# Prune like we did with Adafruit
cd lib/MPU6050
rm -rf .git .github examples/ docs/ *.md Doxyfile
```

---

## `.gitignore` Strategy

**Current approach**: Track library *code* but ignore *build artifacts*

```gitignore
# Track library source files (they're needed for building)
lib/Adafruit_BNO08x_Arduino/
lib/MPU6050/  (when added in v1.1)

# Ignore build artifacts
.pio/
.vscode/
build/
*.o
*.a
```

**Why track library code?**
- Needed for reproducible builds
- Ensures field deployment works without network
- Others can clone and build immediately

**Why ignore build artifacts?**
- Binary files, not source
- Regenerated on each build
- Can be large

---

## Adding New Libraries

**Process**:
1. Download/clone library
2. Remove: `.git/`, `.github/`, `examples/`, `docs/`, `*.md`, `Doxyfile`
3. Keep: Source code (`.h`, `.c`, `.cpp`), `library.properties`, `LICENSE`
4. Place in: `lib/<LibraryName>/`
5. Document in: `LIBRARY_STRATEGY.md`

**Size guideline**: Keep final size <500 KB per library (prune aggressively)

---

## Dependency List (Maintained)

This is the **definitive source** for what libraries auto_orientation needs:

### v1.0 Dependencies
- [x] Adafruit_BNO08x (228 KB, local)
- [ ] ArduinoJSON (optional, not added yet)
- [ ] NMEA parser (in-house, no external library)

### v1.1+ Dependencies (Planned)
- [ ] MPU6050 (to be copied from flight_controller)
- [ ] SD card library (optional, for high-frequency logging)
- [ ] Madgwick filter (if custom sensor fusion needed)

---

## Cross-Project Library Reuse

**If another project needs auto_orientation's libraries**:

Copy the library to their project:
```bash
cp -r auto_orientation/lib/<LibName> other_project/lib/
```

**Don't create symlinks or shared library directories** - each project owns its dependencies.

---

## References

- `LIBRARIES.md` — External library manifest
- `platformio.ini` — Build configuration
- `lib/` — All local library code
