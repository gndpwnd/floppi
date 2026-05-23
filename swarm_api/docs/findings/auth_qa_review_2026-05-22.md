# Adversarial QA Review — swarm_api Auth Layer

- **Date**: 2026-05-22
- **Reviewer**: security-reviewer (Claude Code Orchestra), instance `reviewer@swarm_api:qa`
- **Mode**: Read-only review + read-only `pytest` (TestClient). No source changed, no server started, no commits.
- **Scope**: the opt-in server auth added this session — `src/api/auth.py`, enforcement in `drones.py`/`fleet.py`/`system.py`/`ws.py`, config flags in `config.py`, dashboard wiring in `src/static/index.html`, and `tests/test_security.py`. Companion: `docs/findings/security_audit_2026-05-22.md`.
- **Verdict (TL;DR)**: **GO**. No blocking bypass found. The auth layer is correctly enforced on every state-changing route, uses constant-time comparison, fails closed when a token is set, and stays fully backward-compatible (open + warned) when unset. A handful of nits and one out-of-scope residual (telemetry-driven `ip` XSS, the audit's F-10, never claimed fixed) are noted below.

---

## Per-area verdict

| Area | Verdict |
|------|---------|
| 1a. Route coverage (every mutating route gated) | **sound** |
| 1b. Constant-time / correct token compare | **sound** |
| 1c. WS Origin + token handshake | **sound** (1 defensive nit) |
| 1d. Backward-compat (token unset opens all + warns) | **sound** |
| 2.  Secret redaction (config + auth_token) | **sound** |
| 2.  Dashboard token handling (DOM/URL leakage) | **sound** (WS `?token=` exposure is accepted, documented) |
| 3.  Validator soundness (mac/name/group/tags/mdns) | **sound** (2 nits, neither exploitable) |
| 4.  Regression / narrowed exceptions | **sound** — 18/18 pytest pass |

---

## 1. Auth-bypass hunting

### 1a. Route coverage — SOUND
Enumerated every state-changing / websocket route in `src/`:

| Route | File:line | Gate |
|-------|-----------|------|
| `POST /api/drones/{mac}/command` | drones.py:118 | `Depends(require_auth)` |
| `PUT  /api/drones/{mac}` | drones.py:134 | `Depends(require_auth)` |
| `POST /api/drones` | drones.py:150 | `Depends(require_auth)` |
| `DELETE /api/drones/{mac}` | drones.py:164 | `Depends(require_auth)` |
| `POST /api/fleet/command` | fleet.py:58 | `Depends(require_auth)` |
| `POST /api/fleet/disarm` | fleet.py:93 | `Depends(require_auth)` |
| `PUT  /api/system/config/network` | system.py:84 | `Depends(require_auth)` + `config_mutation_enabled` gate |
| `WS  /ws/dashboard` | ws.py:37 | inline Origin + `ws_token_valid` before `accept()` |

No mutating route is missing the dependency. All GET endpoints (`/api/drones`, `/api/fleet/status`, `/api/system/config`, `/health`, etc.) are intentionally left open for telemetry viewing — matches the audit's documented role-separation decision. No author-missed route.

### 1b. Token comparison — SOUND
- Both paths use `hmac.compare_digest` (`auth.py:69`, `auth.py:83`). `grep` for `==` against any token name across `src/` returns nothing — no plain-equality compare anywhere.
- Type-confusion / None-empty handled: `require_auth` returns early when `_configured_token` is falsy; otherwise rejects unless `presented` is truthy **and** `compare_digest` matches. `ws_token_valid` rejects on `not presented` before the digest call. Both sides are always `str` (config value vs. header/urllib value), so `compare_digest`'s str/bytes `TypeError` cannot fire.
- Empirically probed via TestClient (auth enabled, token `s3cret`): `no header` 401, `Bearer ` (empty) 401, `Basic s3cret` 401, `X-Auth-Token: ""` 401, `X-Auth-Token: nope` 401, `X-Auth-Token: s3cret` 200, `Bearer s3cret` 200, mixed-case scheme `bEaReR s3cret` 200 (correct per RFC 7235), and `?token=s3cret` on the HTTP path **401** (query-param token is WS-only and correctly does not bypass HTTP).

### 1c. WS handshake — SOUND (one defensive nit)
Order in `dashboard_ws` (ws.py:54-101): read config → if `auth_token` set, check `ws_origin_allowed` then `ws_token_valid`, `ws.close(1008)` + `return` on failure → only then `await ws.accept()` → only then enter `receive_text()` loop. **No command frame can be received before the handshake completes** — `receive_text()` is unreachable until after `accept()`, which is unreachable if either check fails.

Origin logic probed directly (`ws_origin_allowed`):
- missing Origin → allowed (intended: non-browser CLI clients have no Origin; the token is their real control). **This is not a browser bypass** — browsers always send an `Origin` on WS handshakes and JS cannot suppress it, so a cross-site page cannot present "no Origin"; it presents its own origin, which fails same-origin/allowlist.
- `null` literal origin → **rejected** (sandboxed/`file://` pages).
- exact host mismatch (`http://evil.com`) → rejected.
- substring trick (`http://host:8080.evil.com`) → rejected (uses `urlparse(...).netloc` exact compare, not substring).
- case-variant host / case-variant allowlist entry → rejected (exact match only).

Token: `?token=` validated with `compare_digest`; missing/empty/wrong all rejected with `close(1008)`.

**Nit (non-blocking):** allowlist match is exact and case-sensitive (`origin in allowlist`), while the same-origin branch compares only `netloc` (ignoring scheme). Minor inconsistency; operators must list the exact `scheme://host:port`. Document the expected format. Not a bypass.

### 1d. Backward-compat — SOUND
`auth_token` defaults to `None` (config.py:137). `_configured_token` and `require_auth`/`ws_token_valid` treat `None` **and** empty-string as "auth disabled" (open). Startup warning fires whenever `not config.server.auth_token` (main.py:57-64), so empty-string also warns. Confirmed by `test_command_open_when_auth_disabled` and `test_ws_open_when_auth_disabled_and_validates_frames` (both pass).

---

## 2. Secret leakage — SOUND

- `_redact_config` (system.py:22-34) strips `command_token` from `network`, from **every** drone entry, and `auth_token` from `server`. `GET /api/system/config` (system.py:73) and `GET /api/system/config/network` (system.py:81/107/115) all route through redaction. The PUT response also re-pops `command_token`. `test_config_get_redacts_tokens` asserts all three (network token, server auth_token, per-drone token) are absent — passes.
- `GET /api/system/info` (system.py:46-66) returns only host/port/rate/poll — no token field. Clean.
- Logs/errors: the 401 detail is the static string `"Missing or invalid auth token"` — never echoes the presented or expected token. WS rejections log `Rejecting WS: ... %r` on **Origin** (not token) and a tokenless `missing/invalid token` message. The downstream firmware-auth 401 log (drone.py:162-165) names the drone, not the token value. No token interpolated into any log line.
- Dashboard: token stored in `localStorage` (`floppi_auth_token`), attached to HTTP as `Authorization: Bearer` **header** (index.html:238), never written into the DOM or an HTTP URL. The password input is `type="password"` + `autocomplete="off"` and is cleared after save. The indicator shows only `Token set` / `No token`, never the value.
- **Accepted exposure (documented):** the WS token rides in the URL as `?token=` (index.html:253). On a wss:// connection the query string is encrypted in transit, but it can land in server access logs / browser history. This is the standard browser-WS limitation (browsers can't set WS request headers from JS) and the code comments call it out. Acceptable for a LAN ground station; if proxied, ensure the proxy does not log query strings. Non-blocking.

---

## 3. Validator soundness — SOUND (two nits, neither exploitable)

Probed `_validate_name`, `_validate_tags`, `MAC_RE`, `MDNS_RE` directly:
- MAC: regex-anchored hex+colons, uppercased; `not-a-mac` rejected (`test_add_drone_rejects_bad_mac`).
- Name/group: rejects empty/whitespace-only, `>64` chars, any control char (`ord<32`, incl. NUL), and ASCII `<`/`>`. The audit's `<script>alert(1)</script>` is rejected (`test_add_drone_rejects_xss_name`).
- mDNS: bare DNS label only — `evil.internal`, `a.b`, `host:80`, `../x`, `127.0.0.1`, `http://evil.internal:80` all rejected; `floppi-5566` accepted. Closes the F-05 SSRF concatenation surface (`test_add_drone_rejects_bad_mdns_hostname`).
- Tags: list cap 16, per-tag `^[A-Za-z0-9 _-]{1,32}$`; oversized tag rejected (`test_update_drone_rejects_oversized_tag`).

**Nit (non-blocking):** the name validator blocks only ASCII `<`/`>`. Fullwidth `＜script＞` (U+FF1C/U+FF1E) is accepted. This is **not** XSS via the dashboard: `innerHTML` does not NFKC-normalize, so it renders as literal fullwidth glyphs, not a tag. Defense-in-depth only.

**Nit (non-blocking):** strings like `javascript:alert(1)` or `img onerror=...` pass the name validator (no `<>`), but they are only ever rendered as text content inside the card name; with no `<>` they cannot break out of the text node. Harmless.

---

## 4. Regression / narrowed-exception check — SOUND

- **pytest: 18 passed, 0 failed, 0 skipped** (`tests/test_security.py` is the only project test file; `--collect-only` confirms all 18 are the new security tests). Run from `swarm_api/` with the project venv.
- Backward-compat regression: `test_command_open_when_auth_disabled`, `test_read_only_endpoints_stay_open`, and `test_ws_open_when_auth_disabled_and_validates_frames` confirm the no-token path serves dashboard reads, commands, and WS frames exactly as before.
- Narrowed exception handlers reviewed for newly-surfaced crashes:
  - `drone.poll_status` now catches `(httpx.RequestError, json.JSONDecodeError, ValueError)` (drone.py:113) instead of bare `Exception`. JSON parse / bad-telemetry errors are still caught; a genuinely unexpected error would now surface to logs (intended, audit F-12). No request path regresses — `poll_status` runs in a background task, not a request handler.
  - `broadcast_telemetry` catches `(WebSocketDisconnect, RuntimeError, OSError)` (ws.py:31) and prunes dead clients — covers the realistic send-failure modes.
  - WS command parse wraps `CommandPayload(**...)` in `except (ValidationError, TypeError)` (ws.py:94) and `continue`s — a malformed frame is dropped, not fatal (`test_ws_open_when_auth_disabled_and_validates_frames` proves the bad frame is dropped and only the valid one reaches `send_command`).
  No previously-swallowed error now crashes a request path.

---

## Residual items (NOT part of the auth layer; out of scope, none blocking)

- **F-10 telemetry-driven `ip` XSS (P3, never claimed fixed):** `drone.poll_status` adopts `net.ip` from drone telemetry with **no validation** (drone.py:104-107; no `ipaddress` import anywhere in `src/`), and `summary()['ip']` is interpolated into the dashboard via `innerHTML` (index.html:297). A compromised/spoofed drone could inject markup through `ip`. `mac` (regex) and `name` (validated) in the same template are safe; `ip` is the gap. This is a telemetry-trust issue, independent of the client→server auth under review. Recommend: validate `net.ip` with `ipaddress` before adoption, and/or escape values in `renderFleet`. Tracked as audit F-10.
- **No rate limiting on the WS command channel / dashboard client count** (audit F-08) — unchanged this session.
- **CORS still not declared** (audit F-03) — the auth layer is the primary control; CORS remains defense-in-depth, not addressed here.

---

## Final go/no-go

**GO** for the swarm_api server auth layer. When `server.auth_token` is set, every command, batch-command, disarm, drone-registration/update/delete, config-mutation, and the dashboard WebSocket are gated with a constant-time token check that fails closed; secrets are redacted from all config-returning responses and never logged; input validators reject the audit's XSS/SSRF payloads. When the token is unset, behavior is identical to pre-auth and a prominent startup warning fires. 18/18 security tests pass and no narrowed exception handler regresses a request path. The remaining items are pre-existing, out-of-scope, non-blocking hardening (notably F-10 telemetry `ip` validation, recommended as the next follow-up).
