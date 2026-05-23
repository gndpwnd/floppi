"""
Opt-in server-side authentication for swarm_api (F-01 / F-02).

This is the CLIENT -> swarm_api credential. It is DEFAULT OFF: when
`server.auth_token` is unset/empty the server behaves exactly as before and a
warning is logged at startup. When set, the token is required (constant-time
compared) on command-bearing and mutating endpoints via the `require_auth`
FastAPI dependency, and on the /ws/dashboard handshake via `ws_authorized`.

Token transport (HTTP): either an `Authorization: Bearer <token>` header or an
`X-Auth-Token: <token>` header. Read-only/health endpoints are intentionally
left open (telemetry viewing) — see the audit's role-separation note; only
state-changing surfaces are gated.
"""

import hmac
import logging
from typing import Optional
from urllib.parse import urlparse

from fastapi import Depends, HTTPException, Request, status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer

logger = logging.getLogger("swarm_api.auth")

# auto_error=False so we can also accept X-Auth-Token and produce our own 401.
_bearer = HTTPBearer(auto_error=False)


def _configured_token(request: Request) -> Optional[str]:
    """Return the configured auth token, or None if auth is disabled."""
    config = getattr(request.app.state, "config", None)
    if config is None:
        return None
    token = config.server.auth_token
    if token:
        return token
    return None


def _present_token(
    request: Request,
    creds: Optional[HTTPAuthorizationCredentials],
) -> Optional[str]:
    """Extract the token the caller presented (Bearer or X-Auth-Token)."""
    if creds and creds.scheme.lower() == "bearer" and creds.credentials:
        return creds.credentials
    header = request.headers.get("x-auth-token")
    if header:
        return header
    return None


async def require_auth(
    request: Request,
    creds: Optional[HTTPAuthorizationCredentials] = Depends(_bearer),
) -> None:
    """
    FastAPI dependency that enforces the server auth token.

    Backward-compatible: if no token is configured, this is a no-op and the
    request proceeds unauthenticated (matching legacy behaviour).
    """
    expected = _configured_token(request)
    if not expected:
        return  # auth disabled — open surface, as before

    presented = _present_token(request, creds)
    if not presented or not hmac.compare_digest(presented, expected):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Missing or invalid auth token",
            headers={"WWW-Authenticate": "Bearer"},
        )


def ws_token_valid(configured: Optional[str], presented: Optional[str]) -> bool:
    """Constant-time check used on the WebSocket handshake."""
    if not configured:
        return True  # auth disabled
    if not presented:
        return False
    return hmac.compare_digest(presented, configured)


def ws_origin_allowed(origin: Optional[str], host_header: Optional[str],
                      allowlist: list[str]) -> bool:
    """
    Validate the WebSocket Origin header (F-02 — cross-site WS hijacking).

    Only meaningful when auth is enabled (callers should gate on that). Allows:
      - a missing Origin (non-browser clients / scripts have no Origin),
      - an Origin whose host matches the request Host header (same-origin),
      - an Origin in the configured allowlist (exact match).
    Rejects everything else.
    """
    if not origin:
        # Non-browser clients (CLI scripts, the manager) send no Origin; the
        # token requirement is the real control for those.
        return True
    if origin in allowlist:
        return True
    if host_header:
        try:
            origin_host = urlparse(origin).netloc
        except ValueError:
            return False
        # Host header may include a port; compare both forms.
        if origin_host == host_header:
            return True
    return False


def auth_enabled(config) -> bool:
    return bool(config.server.auth_token)
