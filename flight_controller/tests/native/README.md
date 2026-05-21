# Native unit-test harness — `flight_controller/tests/native/`

A host-side, plain-`g++` unit-test harness for `flight_controller`. It mirrors
the proven `auto_orientation` pattern: a printf-based assertion harness with
per-test `[build]`/`[PASS]`/`[FAIL]` reporting and a total/passed/failed
summary. No PlatformIO, no Docker, no gtest, no Unity.

## How to run

```bash
cd flight_controller
timeout 200 bash tools/build_tests.sh
```

The runner exits `0` when every test passes, non-zero if any test fails to
build or fails an assertion.

## How to add a test

1. Drop a new file named `tests/native/test_*.cpp` into this directory.
2. `#include "test_helpers.h"` at the top.
3. Write your checks. That's it — **no other wiring needed.**

`tools/build_tests.sh` auto-discovers every `tests/native/test_*.cpp` via a
glob loop, so a new file is picked up with **zero edits to the build script**
and **zero collisions** with other test files. Each test is compiled and run
as an independent standalone binary, so two test files can never clash.

### Minimal example

```cpp
#include "test_helpers.h"

TEST_MAIN("my feature") {
    CHECK(2 + 2 == 4, "arithmetic works");
    CHECK_EQ(answer(), 42, "answer is 42");
    CHECK_NEAR(std::sqrt(2.0), 1.41421356, 1e-6, "sqrt(2) approx");
}
```

Or use the explicit form if you need setup before the banner:

```cpp
#include "test_helpers.h"

int main() {
    TEST_BEGIN("my feature");
    CHECK(true, "always passes");
    TEST_END();   // prints summary, returns the exit code
}
```

## Available macros (`test_helpers.h`)

| Macro | Purpose |
|-------|---------|
| `CHECK(cond, msg)` | Assert a boolean condition holds |
| `CHECK_TRUE(cond, msg)` / `CHECK_FALSE(cond, msg)` | Readable boolean aliases |
| `CHECK_EQ(a, b, msg)` | Assert `a == b` |
| `CHECK_NE(a, b, msg)` | Assert `a != b` |
| `CHECK_NEAR(a, b, eps, msg)` | Assert `|a - b| <= eps` (absolute tolerance) |
| `CHECK_NEAR_REL(a, b, rel, msg)` | Assert relative-tolerance closeness |
| `CHECK_FINITE(x, msg)` | Assert `x` is neither NaN nor Inf |
| `TEST_BEGIN(name)` / `TEST_END()` | Banner / summary+return |
| `TEST_MAIN(name) { ... }` | Declares `main()` + banner + summary for you |

Helper functions in `namespace fc_test`: `near_abs`, `near_rel`, `is_finite`.

`test_helpers.h` depends only on `<cstdio>`, `<cmath>`, `<cstdlib>`.

## Standalone / self-contained convention

Native tests are **pure-logic** tests. Every test file must be **standalone**:

- Compiled with the uniform line
  `g++ -std=c++11 -O2 -DUNIT_TEST -Itests/native -o <bin> <file>`.
- **No `src/` is linked.** Copy the small pure function under test into the
  test file, or `#include` a header that is itself free of hardware deps.
- **No Arduino / Wire / WiFi / `<Arduino.h>` headers**, no gtest, no Unity.

Hardware-coupled code (drivers touching I2C/SPI/GPIO, anything needing the
Arduino runtime) is **out of scope** for this harness — test that on hardware
or via the PlatformIO `native_test` environment.

## Canary

`test_harness_selfcheck.cpp` is an always-present smoke test that proves the
harness works end-to-end. If it ever goes red, the harness itself is broken,
independent of any feature test — fix the harness first.
