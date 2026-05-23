# Handoff: flight_controller → swarm_api (Command-Surface Auth Contract)

Date: 2026-05-22
From: dev@flight_controller:auth
To: a future swarm_api coding agent (cross-project — NOT touched this wave)

## Status
⚠️ Partial — firmware side landed; ground-station (swarm_api) side is still TODO.
The firmware change is **default-OFF and backward-compatible**: the existing
swarm_api contract keeps working unchanged until both sides flip the flag on.

## What this delivers (firmware side)

Statically-fixable items from `docs/findings/security_audit_2026-05-22.md`:

| Audit ID | What changed | File:line (post-edit) |
|----------|--------------|-----------------------|
| SEC-01 / SEC-03 | Shared-token auth on the WiFi command surface, gated behind `USE_API_AUTH` (default OFF) | `include/config.h` (flag + `#warning`), `src/web_server.cpp` (auth helpers + checks) |
| SEC-02 | OTA password required; `#error` if `USE_OTA` on with no credential | `src/ota.cpp`, `include/wifi_credentials.h` |
| SEC-05 | `/ws` telemetry broadcast no longer truncates (dynamic `String` serialization + skip-on-fail guard) | `src/web_server.cpp` broadcast path |
| SEC-08 | I2C command frame now carries an XOR checksum, validated in the ISR | `lib/RadioComm/radioComm_ext.cpp` |
| SEC-04 | (Referenced only) GPS lat/lon leak — being handled by flag-gating elsewhere; see `include/config.h` GPS section "SECURITY NOTE" | n/a here |
| SEC-07 | Replay/monotonic counter — **DEFERRED** this pass (see "Deferred" below) | n/a |

---

## TOKEN SCHEME (what swarm_api must implement)

### Flag & secret location (firmware)
- Build flag: `USE_API_AUTH` in `include/config.h` (commented out by default).
- Token value: `#define FLOPPI_CMD_TOKEN "..."` in `include/wifi_credentials.h`
  (placeholder `"CHANGE-ME-floppi-token"` committed; operators change it per drone).
- When `USE_API_AUTH` is **off** (the committed default), the command surface is
  open and behaves exactly as today — swarm_api needs no change to keep working.
- When `USE_API_AUTH` is **on**, every *command-bearing* request must present the
  matching token or it is rejected. **Telemetry/read paths are NOT gated**:
  `GET /api/status`, the `/ws` telemetry broadcast, and `GET /` stay open.

### Where the token goes per surface

1. **`POST /api/commands`** (preferred path for swarm_api)
   - HTTP header: `X-Floppi-Token: <token>`  ← preferred
   - OR a `"token"` field inside the JSON body (fallback, accepted too):
     ```json
     {"token":"<token>","ch1":1500,"ch2":1500,"ch3":1000,"ch4":1500,"ch5":1000,"ch6":1000}
     ```
   - On mismatch/absence: **HTTP 401** `{"error":"unauthorized"}`. (Malformed JSON
     still returns 400 as before, checked before auth.)

2. **WebSocket `/ws`** (command frames sent by a client)
   - The WS frame has no HTTP headers, so the token rides in the JSON body as a
     `"token"` field, alongside the channel keys:
     ```json
     {"token":"<token>","ch1":1500,...,"ch6":1000}
     ```
   - On mismatch/absence: the frame is **silently dropped** (no error reply — we
     do not chatter on the command channel). The WS connection stays open.
   - Note: the same `/ws` socket also *broadcasts telemetry outbound*; that
     direction is unauthenticated and unchanged.

3. **`/api/rc`** — listed in the audit task, but **no such endpoint exists** in
   the firmware (confirmed by grep). The only command surfaces are
   `POST /api/commands` and WS `/ws`. No action needed for `/api/rc`.

### Comparison semantics
- Token compare is a length-and-content check with a constant-ish-time loop
  (`tokenMatches()` in `web_server.cpp`) to avoid trivially leaking length via
  early-out. Token is a plain ASCII string; pick something long/random.

### Confidentiality caveat (important for swarm_api docs)
- Traffic is plaintext HTTP/WS (SEC-06). The token is a **control-plane gate,
  not a confidentiality control** — a passive LAN sniffer can read it. Run the
  fleet on an isolated SSID. A future hardening step is HMAC-with-counter
  (defeats both sniffing-reuse and replay); see Deferred.

### swarm_api implementation checklist
1. Add a per-drone `token` to the drone registry in `config.json` (or a global
   default), matching each drone's `FLOPPI_CMD_TOKEN`.
2. When posting commands: add header `X-Floppi-Token` (or include `"token"` in
   the JSON body). Same for any WS command frames the dashboard sends.
3. Do NOT add the token to telemetry GETs — they are not gated and adding it is
   harmless but unnecessary.
4. Coordinate the flip: enable `USE_API_AUTH` in firmware **only after** the
   ground station sends the token, or commands will start returning 401.

---

## OTA password (SEC-02) — operator note, not a swarm_api API change
- `include/wifi_credentials.h` now defines `OTA_PASSWORD` (placeholder
  `"CHANGE-ME-floppi-ota"`). `USE_OTA` builds **will not compile** without a
  non-empty `OTA_PASSWORD` (or a 32-char `OTA_PASSWORD_HASH`). Enforced by an
  `#error` (missing) + `static_assert` (empty) in `src/ota.cpp`.
- `pio run -t upload --upload-port floppi-XXXX.local` must now pass
  `--auth=<password>` (PlatformIO: `upload_flags = --auth=...`, or interactive
  prompt). Document this for whoever does OTA.
- Hash form preferred for field builds (keeps plaintext out of flash strings):
  set `OTA_PASSWORD_HASH` to the lowercase MD5 of the password, leave
  `OTA_PASSWORD` empty.

## I2C command frame change (SEC-08) — note for any I2C master author
- New contract: master writes **13 bytes** = 12 channel-data bytes
  (6× uint16 LE, 1000–2000us) + 1 XOR-of-the-12-data-bytes checksum.
- Backward-compat: a legacy 12-byte (unchecksummed) write is still accepted
  **unless** the firmware build defines `USE_I2C_CMD_CHECKSUM`, which makes the
  13-byte checksummed frame mandatory. Frames with a bad checksum are dropped
  in the ISR (last good command is held).

---

## Deferred / not done this pass
- **SEC-07 (replay guard / monotonic counter):** intentionally skipped to avoid
  complicating the command contract before swarm_api implements the basic token.
  Recommended follow-up: add a strictly-increasing `seq` field to command
  payloads, reject stale/duplicate counters, and tighten the 500 ms WiFi
  failsafe. This is best done together with an HMAC migration (SEC-06) so the
  counter + MAC are authenticated, not just present. Both touch swarm_api and
  warrant their own wave.
- **SEC-06 (transport encryption / HTTPS for the swarm POST):** cross-project,
  heavy (WiFiClientSecure + CA on Core 1, TLS termination on swarm_api). Not
  attempted. Flagged for the swarm_api maintainer.
- **SEC-04 (GPS telemetry flag-gating):** owned elsewhere this wave per task
  brief; only referenced here.

## Build verification (this wave)
- `pio run -e esp32` default (USE_API_AUTH off): SUCCESS — RAM 10.9% (35644 B),
  Flash 43.6% (572117 B). `#warning` fires noting the open command surface.
- `pio run -e esp32` with USE_API_AUTH on: SUCCESS — Flash 43.7% (572453 B),
  RAM unchanged (+336 B flash for the auth code).
- `pio run -e esp32s3` with USE_API_AUTH on: SUCCESS — RAM 9.7%, Flash 17.0%.
- config.h / wifi_credentials.h reverted to safe committed defaults afterward;
  final default build re-confirmed clean (byte-identical 572117 B).

## Core-0 impact
- Zero. All auth helpers compile out entirely when `USE_API_AUTH` is off, and
  even when on they run only inside the Core-1 async web-server callbacks. The
  Core-0 1 kHz flight loop reads the spinlock buffer exactly as before.
