# Session Record — swarm_api Security Hardening

- **Date**: 2026-05-22
- **Project**: `swarm_api/` (FastAPI ground-station controlling a fleet of ESP32 drones over WiFi)
- **Mode**: Multi-agent (Claude Code Orchestra). No live server started; pytest TestClient only.
- **Commit status**: **ALL changes are uncommitted in the working tree.** No commits were made this session (per operator instruction).

This session moved swarm_api from an unauthenticated open control plane toward a hardened, opt-in-authenticated one, and added matching client-side auth plumbing for the firmware command surface. Independent QA returned **GO**.

---

## 1. Auth — client side (swarm_api → drone firmware)

Implements the FC handoff contract for the firmware's `USE_API_AUTH` command surface.

- `DroneClient` gained a `command_token` (per-drone or fleet-wide, from config).
- When set, the token is attached to **every command**:
  - HTTP `POST /api/commands` → `X-Floppi-Token` header.
  - WebSocket frames → a `"token"` field in the JSON body (WS frames carry no HTTP headers).
- When `command_token` is `None`, commands go out unauthenticated — backward-compatible with auth-OFF firmware.
- A `401` from the firmware logs a single throttled error (`_auth_warned` flag) telling the operator to check `command_token` vs. the drone's `FLOPPI_CMD_TOKEN`; the flag resets on recovery.
- Files: `src/drone.py`, `src/config.py`, `src/manager.py`.

This authenticates **swarm_api → drone**, NOT **client → swarm_api** (see §3 for the latter).

---

## 2. Server security audit

`docs/findings/security_audit_2026-05-22.md` — full read-only audit of swarm_api's own attack surface. 12 findings:

- **2 × P0**:
  - **F-01** — No authentication/authorization on any endpoint; anyone routable to `0.0.0.0:8080` can arm/throttle/steer/disarm the whole fleet and rewrite persisted config.
  - **F-02** — Command WebSocket (`/ws/dashboard`) had no `Origin` check and no auth → cross-site WebSocket hijacking → drive-by fleet takeover.
- **3 × P1**: F-03 (no CORS posture), F-04 (`PUT /api/system/config/network` mutates+persists config unauthenticated), F-05 (SSRF/pivot via attacker-registered drone IP/mDNS hostname).
- **4 × P2**: F-06 (WS command path bypasses REST channel validation), F-07 (mac/name/group/tags unvalidated → stored-XSS / config bloat), F-08 (no rate limiting), F-09 (`config.json` world-readable, `command_token` leaked via `GET /api/system/config`).
- **3 × P3**: F-10 (drone telemetry trusted/reflected unbounded; `net.ip` adopted unvalidated), F-11 (`0.0.0.0` bind with no documented isolation), F-12 (over-broad `except Exception`).

Confirmed non-issues: no `subprocess`/`eval`/`shell=True`, no path traversal in config I/O (fixed `CONFIG_PATH`), no `allow_origins=["*"]` footgun, REST command values already clamped 1000–2000.

---

## 3. Server hardening (opt-in auth layer + input validation)

New module `src/api/auth.py` plus enforcement across the routers.

- **Opt-in server auth** (closes F-01): `server.auth_token` config field, default `None`.
  - Accepts `Authorization: Bearer <token>` **or** `X-Auth-Token: <token>`.
  - Constant-time compare via `hmac.compare_digest` — no plain `==` against any token anywhere in `src/`.
  - **Fails closed** when a token is set; **opens + warns** (prominent startup log) when unset → fully backward-compatible.
  - `Depends(require_auth)` on every state-changing route: drone command, drone PUT/POST/DELETE, fleet command, fleet disarm, config-network PUT. All GET/telemetry endpoints intentionally stay open (read-only viewer vs. operator role separation).
- **WS Origin + token handshake** (closes F-02): `dashboard_ws` validates `Origin` against an allowlist (same-origin allowed; `null`/mismatch/substring-trick rejected) and a `?token=` (constant-time) **before** `ws.accept()`; failure closes with code `1008`. Missing-Origin allowed for non-browser CLI clients (token is their real control; browsers cannot suppress Origin).
- **Input validators** (close F-05/F-07 — XSS/SSRF): Pydantic validators for `mac` (regex, uppercased), `name`/`group` (non-empty, ≤64 chars, no control chars, no ASCII `<`/`>`), `tags` (≤16 entries, per-tag `^[A-Za-z0-9 _-]{1,32}$`), and `mdns_hostname` (bare DNS label only — rejects dots/schemes/ports/paths/IPs, closing the SSRF concatenation surface).
- **Config-mutation gate** (F-04): `server.config_mutation_enabled` flag; PUT returns `503` when disabled, in addition to the auth gate.
- **Secret hardening** (F-09): `_redact_config` strips `command_token` (network + every drone) and `auth_token` (server) from all config-returning responses; `config.json` written `chmod 0600`; tokens never interpolated into logs; `.gitignore` updated.
- **WS command schema** (F-06): WS command frames parsed through `CommandPayload`; malformed frames dropped (`continue`), not fatal.
- **Narrowed exceptions** (F-12): `poll_status` now catches `(httpx.RequestError, json.JSONDecodeError, ValueError)`; `broadcast_telemetry` catches `(WebSocketDisconnect, RuntimeError, OSError)` and prunes dead clients — replacing bare `except Exception`.
- Files: `src/api/auth.py` (new), `src/api/drones.py`, `src/api/fleet.py`, `src/api/system.py`, `src/api/ws.py`, `src/config.py`, `src/main.py`, `src/manager.py`, `.gitignore`, `config.json`.

---

## 4. Dashboard auth wiring

`src/static/index.html`:

- Token UI (password input, `autocomplete="off"`, cleared after save), persisted to `localStorage` (`floppi_auth_token`).
- HTTP requests send `Authorization: Bearer <token>`; the WS connects with `?token=<token>` (the standard browser-WS limitation — JS can't set WS request headers; documented as accepted, encrypted under `wss://`).
- Handles `401` (HTTP) and `1008` (WS close) by prompting for / flagging the token.
- Indicator shows only `Token set` / `No token`, never the value.

---

## 5. Round-2 follow-ups

- **F-10 telemetry XSS (DOM sink)** — `renderFleet` rewritten to push drone-derived strings (`name`/`mac`/`ip`) into the DOM via `textContent` / text nodes instead of `innerHTML` template-literal interpolation. Verified by `test_dashboard_renderfleet_has_no_innerhtml_sink` and `test_dashboard_has_no_html_interpolation_sinks_anywhere`.
- **F-10 server-side `net.ip` validation (this round-2 task)** — `src/drone.py` now validates drone-reported `net.ip` with Python's `ipaddress` module before adopting it (defense-in-depth behind the already-fixed DOM sink). New helper `_valid_ip(value)` returns the address only if it parses as a valid IPv4/IPv6 string; anything else (markup, hostname, malformed octet, non-string, empty) is dropped and the previously-known IP is retained. `poll_status` no longer trusts `net.ip` verbatim. Does not crash on bad input. Minimal/consistent with surrounding code.
- **F-11 configurable bind host** — `server.host` config field; defaults to `0.0.0.0` (unchanged behaviour), now operator-settable (e.g. `127.0.0.1`). Parsed from `config.json`.
- **F-08 rate limiting** — **declined this session with rationale**: needs a design decision on mechanism/limits (slowapi vs. reverse-proxy) and would add a dependency; the opt-in auth layer is the primary control. Tracked as a follow-up.

---

## 6. Independent QA

`docs/findings/auth_qa_review_2026-05-22.md` — adversarial read-only review by `reviewer@swarm_api:qa`.

- **Verdict: GO.** No blocking auth bypass found. Every mutating route gated, constant-time compare, fails-closed-when-set, backward-compatible-when-unset, secrets redacted everywhere and never logged, validators reject the audit's XSS/SSRF payloads.
- Non-blocking nits noted: fullwidth `＜＞` accepted by name validator (renders as literal glyphs, not XSS); WS `?token=` URL exposure (accepted/documented); allowlist exact/case-sensitive match (document the `scheme://host:port` format).
- Test result at QA time: 18/18 (auth-focused subset). Suite has since grown.

---

## 7. Test status (end of session)

- `tests/test_security.py` (new this session) — full suite: **27 passed, 0 failed** (system pytest 9.0.2 + app deps).
  - Covers: opt-in auth (F-01/F-02), config-mutation gating + 503 (F-04), secret redaction (F-09), drone metadata validators incl. XSS name + bad mDNS (F-05/F-07), WS command validation + token handshake (F-06), dashboard DOM-sink absence (F-10 client), bind-host config (F-11), and the new **server-side `net.ip` validation** (F-10 server): valid IPv4/IPv6 accepted, malformed/markup/hostname/None/int dropped, and `poll_status` retains its prior IP on bad telemetry.
- The `net.ip` tests run synchronously via `asyncio.run` (the project venv has no `pytest-asyncio`), matching the existing TestClient-based suite.

---

## 8. State for the next session

- **Everything above is uncommitted** in the swarm_api working tree (`src/api/auth.py`, `tests/`, the audit + QA findings are untracked; `src/drone.py`, `src/main.py`, routers, `config.py`, `config.json`, `.gitignore`, `index.html` are modified). Commit when the operator approves.
- **Open / deferred**: F-03 (explicit CORS allowlist), F-08 (rate limiting + WS client cap), F-12 remaining broad catches outside the touched paths. None blocking; auth is the primary control.
- **Operational note**: server still defaults to `0.0.0.0`; document/enforce the trusted-network assumption (F-11) and set `server.auth_token` for any non-isolated deployment.

### What's next (explicit follow-ups)

- **TLS / HTTPS — deferred.** All traffic is still plaintext HTTP/WS; the bearer/WS token is a control-plane gate, not a confidentiality control. Front the server with TLS (reverse proxy: `wss://` + `https://`) before any non-isolated deployment. Not implemented this session.
- **Rate limiting at the proxy — deferred (F-08).** Declined in-app this session (would add a dependency and needs a mechanism/limits design decision). Recommended approach: enforce request/connection limits at the reverse proxy rather than in the app. Pair with a WS-client cap.
- **Flip auth default to ON — operator decision.** Auth currently defaults OFF (backward-compatible, opens + warns). Whether to make `server.auth_token` mandatory/default-ON is an operator/deployment policy call, not a code default to change unilaterally.
