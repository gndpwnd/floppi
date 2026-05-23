# Session Record — 2026-05-22 — Security/Correctness Hardening + Documentation Wave

> Project: `flight_controller/` (Teensy + ESP32 firmware)
> Agent (this record): `doc-writer@flight_controller:session-record` (Claude Code Orchestra)
> Status: **All changes uncommitted — working tree only, awaiting operator review.**
> Sources of record: the dated findings/QA/handoff docs cited below; every code
> claim here was re-verified against the actual source on 2026-05-22.
>
> **Companion record:** the ESP32 compile-time WiFi auth-mode selector landed *after* this
> record was written — see [2026-05-22_wifi_network_modes.md](2026-05-22_wifi_network_modes.md)
> (`WIFI_AUTH_MODE_*` selector, WPA3-SAE + EAP-TTLS, static IP/hostname, liftable
> `wifi_connect` module, QA verdict GO).

---

## 1. One-paragraph summary

This was a security-and-correctness wave plus a documentation wave on the ESP32/Teensy
flight-controller firmware. A read-only external attack-surface audit found 3 P0 / 2 P1 /
3 P2 / 3 P3 issues; a coding agent then landed the statically-fixable subset as **opt-in,
default-OFF, backward-compatible** firmware changes (command-API token auth, OTA password +
build guards, dynamic WebSocket telemetry buffer, I2C command checksum, a Madgwick NaN
guard, and a GPS position-privacy gate). An independent QA review built the full matrix +
flag combinations, ran the native test suite, reviewed every change, and returned a **GO**
verdict. In parallel, the documentation set got an ASCII→Mermaid conversion pass, a new
layered architecture doc set (`docs/architecture/` Level 0/1/2), doc-drift fixes, and two
new operator-facing security docs. The W5 GPS passthrough was confirmed landed and a latent
build-breaker (missing `GPS_PIN_RX/TX` defaults) was fixed. **No git commits were made this
session** — everything is in the working tree.

---

## 2. Security / correctness CODE changes (all uncommitted, all verified against source)

All of these are gated so the committed-default build behaves exactly as before. Verified
flag names, paths, and line locations against the current source on 2026-05-22.

| Audit ID | Change | Flag / default | Where (verified) |
|---|---|---|---|
| SEC-01 / SEC-03 | Shared-token auth on the WiFi **command** surface (`POST /api/commands` + `/ws` data branch). Telemetry/read paths intentionally stay open. | `USE_API_AUTH` — **OFF by default** (`include/config.h:73`, commented out) | `src/web_server.cpp` |
| SEC-01 | Single-emission `#warning` when the command surface is open (auth off). Moved to **one** translation unit, not per-TU. | n/a | `src/web_server.cpp:35` (only firmware-source copy) |
| SEC-01 | Placeholder-token `static_assert` guards: `#error` if `FLOPPI_CMD_TOKEN` undefined with auth on; rejects the placeholder default and too-short tokens. | `FLOPPI_CMD_TOKEN` placeholder `"CHANGE-ME-floppi-token"` (`include/wifi_credentials.h:50`) | `src/web_server.cpp:47,70,75` |
| SEC-01 | Constant-time-ish `tokenMatches()` (folds length-mismatch in, no first-byte early-out); `httpAuthorized()` reads `X-Floppi-Token` header, `jsonAuthorized()` reads a `"token"` JSON field. | n/a | `src/web_server.cpp:220,248,257` |
| SEC-02 | OTA **password required**: `#error` if `USE_OTA` on with neither `OTA_PASSWORD` nor `OTA_PASSWORD_HASH`; `static_assert` on emptiness (`sizeof>1`) / hash length (`==33`). Prefers the hash form. | `OTA_PASSWORD` placeholder `"CHANGE-ME-floppi-ota"` (`include/wifi_credentials.h:61`) | `src/ota.cpp:39,47,50,68` |
| SEC-05 | WebSocket broadcast no longer truncates: fixed `char buf[512]` replaced with dynamic `String out; serializeJson(doc, out)` (matching `/api/status`), zero-length result skips the frame. | n/a | `src/web_server.cpp:454,460` |
| SEC-08 | I2C command frame now carries an XOR checksum, validated **in the ISR** (`onI2CReceive`). 13-byte frame = 12 data + XOR. Legacy 12-byte accepted only when `USE_I2C_CMD_CHECKSUM` is undefined; defining it makes the checksum mandatory. | `USE_I2C_CMD_CHECKSUM` opt-in | `lib/RadioComm/radioComm_ext.cpp` (`onI2CReceive`, `i2cApplyData`) |
| (M-1) | Madgwick6DOF **NaN guard**: gradient normalization skipped when `gradNormSq <= 1e-12` (degenerate exact-gravity case → `invSqrt(0)=inf → NaN`); final quaternion guarded with epsilon + `isnan` checks and **reset-to-identity** backstop. Nominal (non-degenerate) path bit-identical. | n/a (base tier) | `src/imu.cpp:279-281,301-306` |
| SEC-04 | GPS **position privacy gate**: when `GPS_TELEMETRY_INCLUDE_POSITION == 0` the raw `nmea` field (the only lat/lon carrier on a passthrough FC) is omitted from both serializers; only `gps.ok` + `gps.age_ms` emitted. | `GPS_TELEMETRY_INCLUDE_POSITION` **defaults to 1** (contract-preserving full passthrough) (`include/config.h:752`) | `src/web_server.cpp:176`, `src/api_client.cpp:126` |

**Note on the per-TU `#warning` claim:** the QA review (§3a nit 2) flagged the SEC-01
`#warning` as living in `config.h` and emitting ~14× per ESP32 build. The current source
has it scoped to a **single** translation unit (`src/web_server.cpp:35`); `config.h` only
*documents* this (`include/config.h:76-80`). The improvement landed after the QA nit.

**Core-0 impact: zero.** All auth helpers compile out when `USE_API_AUTH` is off; even on,
they run only in Core-1 async web-server callbacks. The WS buffer, GPS gate, and OTA are all
Core 1. The I2C checksum is in an ISR feeding the existing command buffer. The Madgwick guard
adds two cheap branches on the nominal path. The 1 kHz flight loop is untouched.

---

## 3. GPS (W5) — landed + latent build-breaker fixed

- `USE_GPS` is confirmed **LANDED** (committed in `3f57a6c`): telemetry-only, passthrough-only
  raw-NMEA feed on ESP32/ESP32-S3 Core-1 (`src/gps.cpp`, `include/gps.h`, task spawned from
  `src/main.cpp`). RX-only UART1 framer + spinlock snapshot. `/api/status`, `/ws`, and the
  swarm `/api/telemetry` POST all carry a `gps` block. The FC parses nothing beyond `$…\r\n`
  framing + a liveness bit; no flight-loop coupling. Flag defaults OFF, zero bytes when off.
- **Latent build-breaker fixed (2026-05-21, `include/config.h`):** the driver shipped without
  resolvable `GPS_PIN_RX` / `GPS_PIN_TX` defaults — a `-D USE_GPS` build would not compile
  because `Gps::begin()` referenced undefined pin macros. `#ifndef`-guarded defaults were added
  in two idempotent places (the `USE_GPS` feature section and the PIN OVERRIDES block):
  `GPS_PIN_RX` = `4` (ESP32) / `16` (ESP32-S3), `GPS_PIN_TX` = `-1` (RX-only). Verified present
  at `include/config.h:246-254` and `:722-730`. No SBUS/UART collision; `pin_definitions_esp32.h`
  not edited, so no GPIO `#error`-guard regression.
- Reference: `docs/findings/phase_w5_gps_landed_2026-05-21.md`.

---

## 4. Documentation changes

- **ASCII→Mermaid conversion** across docs — diagrams now render as Mermaid (e.g. the trust-
  boundary diagram in `security_posture.md`).
- **New layered architecture doc set** in `docs/architecture/` — Level 0 (one whole-system
  diagram), Level 1 (per-subsystem: flight loop+PID, command sources+arbitration, ESP32
  dual-core, sensor/telemetry pipeline), Level 2 (component detail for arbitration buffers +
  baro Core-1 task). All 7 docs are Mermaid-grounded; `docs/architecture/INDEX.md` is the entry.
- **Doc-drift P0/P1/P2/P3 fixes** applied across the doc set.
- **`docs/security_posture.md`** — durable, operator-facing threat-model + posture synthesis of
  the audit (trust-boundary Mermaid, attack surface table, severity table, operator guidance).
- **`docs/network_security_setup.md`** — operator how-to for enabling `USE_API_AUTH` +
  `FLOPPI_CMD_TOKEN`, the OTA password/hash, and the `GPS_TELEMETRY_INCLUDE_POSITION` OPSEC
  switch. Defers the "why" to security_posture.md.
- **`docs/handoffs/api_auth_contract_2026-05-22.md`** — wire-level command-auth contract for a
  future `swarm_api` agent (header `X-Floppi-Token` / JSON `"token"`, 401 on POST mismatch,
  silent drop on WS mismatch, plaintext-token caveat, integration checklist). Cross-project —
  swarm_api side is still TODO; firmware side is default-OFF so the existing contract keeps
  working until both sides flip the flag.

---

## 5. Verification

- **Native tests (host-side, `bash tools/build_tests.sh`):** 5/5 test files PASS, 0 fail.
  The verification doc recorded 114 assertions pre-change; the QA gate recorded the suite at a
  higher count after `test_attitude.cpp` gained the M-1 regression case (exact-level gravity that
  would NaN on the old source, passes on the new guard). All 5 files green either way. The native
  harness *copies* the Madgwick logic rather than linking `src/imu.cpp`; the identical guard is
  confirmed in `src/imu.cpp` by source diff.
- **Builds clean:** `teensy40`, `esp32`, `esp32s3` all SUCCESS at default flags. esp32 ≈ 43.7%
  flash / 10.9% RAM; esp32s3 ≈ 17.0% flash / 9.7% RAM; teensy40 ≈ 2% flash. Default esp32 build
  emits the intended SEC-01 `#warning` (auth off). Flag-enabled combinations also built clean
  read-only (`USE_API_AUTH+USE_GPS`; `USE_GPS+USE_BAROMETER+GPS_TELEMETRY_INCLUDE_POSITION=0`;
  `USE_I2C_COMMANDS+USE_I2C_CMD_CHECKSUM+USE_COMMAND_ARBITRATION`). OTA guards verified out-of-tree
  with standalone `g++` (empty password → compile error; valid → compiles; wrong-length hash → error).
- **Independent QA verdict: GO — safe to commit.** No blocking issues; 4 non-blocking nits
  (placeholder-token offers no protection if unchanged with auth on; `tokenMatches` is constant-
  time-ish not strictly constant-time; OPSEC GPS path still serializes NMEA into a throwaway
  buffer for the age). Reference: `docs/findings/qa_review_2026-05-22.md`.

---

## 6. Build / focus status (the 4 focus builds)

| Env | Compile | Functional flight validation |
|---|---|---|
| `teensy40` | ✅ clean (no network code in image; builds to confirm shared `imu.cpp` didn't regress) | ⚠️ hardware-gated |
| `teensy41` | ✅ (same family as teensy40) | ⚠️ hardware-gated |
| `esp32` | ✅ clean (WiFi/web/api/ota compiled + linked) | ⚠️ hardware-gated |
| `esp32s3` | ✅ clean | ⚠️ hardware-gated |

**Functional flight work remains hardware-gated** — the bench has only OLED + MPU6050 (no
ESCs/motors, no barometer, no GPS module). ESC calibration, baro/GPS telemetry, arm-over-WiFi
rejection, OTA reject-on-bad-password, and failsafe timing cannot be validated this session.
Prior bench runs (2026-02-17) validated the 9 IMU/radio calibration commands at 42/42 checks.

---

## 7. Next session / remaining

**Hardware-gated bench validation (props off, motors removed):**
- SEC-01 end-to-end arm-over-WiFi rejection with `USE_API_AUTH` on + a real token.
- SEC-02 OTA reject-on-bad-password round-trip (`pio run -t upload --auth=...`).
- SEC-05 worst-case WS frame size with GPS + barometer both enabled.
- SEC-08 I2C checksum against the real I2C master.
- Baro (`b` cal + telemetry) and GPS passthrough once a breakout/module is on the bench.
- ESC calibration (`e`) once ESCs/motors are present.

**Deferred security items (statically or cross-project, not done this wave):**
- **SEC-07** (replay guard / monotonic command counter + tighter WiFi failsafe) — DEFERRED;
  best done alongside an HMAC migration so the counter is authenticated.
- **SEC-06** (transport encryption / HTTPS for the swarm POST) — cross-project, heavy
  (`WiFiClientSecure` + CA on Core 1, TLS termination on `swarm_api`).
- **SEC-03/04** swarm-side: `swarm_api` must send the command token and (eventually) support
  HTTPS — see `docs/handoffs/api_auth_contract_2026-05-22.md`.
- **SEC-09** (trim verbose info fields) and **SEC-10** (PPM/PWM range-sanity validity check) —
  low priority; SEC-10 needs a real receiver to validate.
- QA nits (placeholder-token `#warning`, OPSEC throwaway-NMEA tidy) — optional follow-ups.

**Operator action before any of the hardening takes effect:** none is enabled by default.
Enable `USE_API_AUTH`, set a strong `FLOPPI_CMD_TOKEN` and `OTA_PASSWORD` (prefer the hash),
and set `GPS_TELEMETRY_INCLUDE_POSITION 0` where broadcasting location is unacceptable. See
`docs/network_security_setup.md`.

---

## 8. Commit status

**NO commits were made this session.** All firmware changes (web_server.cpp, ota.cpp,
radioComm_ext.cpp, imu.cpp, config.h, wifi_credentials.h, api_client.cpp) and all new/edited
docs are in the **working tree only**, awaiting operator review. The QA gate is GO; the
operator decides when to commit.

---

*Durable handoff record. Code/flag/path claims verified against the actual source on
2026-05-22. Sources: `docs/findings/security_audit_2026-05-22.md`,
`docs/findings/qa_review_2026-05-22.md`,
`docs/findings/calibration_telemetry_verification_2026-05-22.md`,
`docs/findings/phase_w5_gps_landed_2026-05-21.md`, `docs/security_posture.md`,
`docs/network_security_setup.md`, `docs/handoffs/api_auth_contract_2026-05-22.md`.*
