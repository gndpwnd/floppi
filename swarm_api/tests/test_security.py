"""
Security-hardening tests for swarm_api (security_audit_2026-05-22).

Covers: opt-in server auth (F-01/F-02), WS command validation (F-06), drone
metadata validators (F-07), config-secret redaction + mutation gating (F-04/F-09).

No live server / no hardware: the DroneManager is replaced with a stub and the
app lifespan is bypassed by setting app.state directly.
"""

import asyncio
import sys
from pathlib import Path
from types import SimpleNamespace

import pytest
from fastapi.testclient import TestClient

# Make `src` importable when run from anywhere.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from src.main import app  # noqa: E402
from src.config import AppConfig, ServerConfig, NetworkConfig, DroneEntry  # noqa: E402


class _StubDrone:
    def __init__(self, mac, online=True):
        self.mac = mac
        self.name = "stub"
        self.group = None
        self.tags = []
        self.state = SimpleNamespace(online=online, last_telemetry={})
        self.sent = []

    async def send_command(self, channels):
        self.sent.append(channels)
        return True

    def summary(self):
        return {"mac": self.mac, "online": self.state.online}


class _StubManager:
    def __init__(self, config):
        self.config = config
        self.drones = {"AA:BB:CC:DD:EE:FF": _StubDrone("AA:BB:CC:DD:EE:FF")}

    def get_drone(self, mac):
        return self.drones.get(mac)

    def fleet_status(self):
        return {"total": 1, "online": 1, "offline": 0, "armed": 0, "groups": {}}

    def list_drones(self):
        return [d.summary() for d in self.drones.values()]

    def get_online_drones(self):
        return [d for d in self.drones.values() if d.state.online]

    def get_drones_by_group(self, g):
        return []

    def get_drones_by_tag(self, t):
        return []

    async def send_command_to_many(self, macs, channels):
        out = {}
        for m in macs:
            d = self.drones.get(m)
            if d and d.state.online:
                out[m] = await d.send_command(channels)
            else:
                out[m] = False
        return out

    async def disarm_all(self):
        return await self.send_command_to_many(list(self.drones), {})

    async def add_drone(self, **kw):
        d = _StubDrone(kw["mac"])
        self.drones[kw["mac"]] = d
        return d


def _client(auth_token=None, mutation=True):
    config = AppConfig(
        drones=[DroneEntry(mac="AA:BB:CC:DD:EE:FF", name="stub")],
        network=NetworkConfig(),
        server=ServerConfig(auth_token=auth_token,
                            config_mutation_enabled=mutation),
    )
    app.state.config = config
    app.state.manager = _StubManager(config)
    return TestClient(app)


# ----------------------- F-01: opt-in server auth -----------------------

def test_command_open_when_auth_disabled():
    c = _client(auth_token=None)
    r = c.post("/api/drones/AA:BB:CC:DD:EE:FF/command", json={"ch3": 1200})
    assert r.status_code == 200, r.text


def test_command_rejected_without_token_when_auth_enabled():
    c = _client(auth_token="s3cret")
    r = c.post("/api/drones/AA:BB:CC:DD:EE:FF/command", json={"ch3": 1200})
    assert r.status_code == 401


def test_command_accepted_with_bearer_token():
    c = _client(auth_token="s3cret")
    r = c.post("/api/drones/AA:BB:CC:DD:EE:FF/command", json={"ch3": 1200},
               headers={"Authorization": "Bearer s3cret"})
    assert r.status_code == 200, r.text


def test_command_accepted_with_x_auth_token_header():
    c = _client(auth_token="s3cret")
    r = c.post("/api/drones/AA:BB:CC:DD:EE:FF/command", json={"ch3": 1200},
               headers={"X-Auth-Token": "s3cret"})
    assert r.status_code == 200


def test_command_rejected_with_wrong_token():
    c = _client(auth_token="s3cret")
    r = c.post("/api/drones/AA:BB:CC:DD:EE:FF/command", json={"ch3": 1200},
               headers={"Authorization": "Bearer nope"})
    assert r.status_code == 401


def test_fleet_disarm_and_batch_gated():
    c = _client(auth_token="s3cret")
    assert c.post("/api/fleet/disarm").status_code == 401
    assert c.post("/api/fleet/command", json={"ch3": 1100}).status_code == 401
    assert c.post("/api/fleet/disarm",
                  headers={"Authorization": "Bearer s3cret"}).status_code == 200


def test_read_only_endpoints_stay_open():
    c = _client(auth_token="s3cret")
    assert c.get("/api/drones").status_code == 200
    assert c.get("/health").status_code == 200
    assert c.get("/api/fleet/status").status_code == 200


# ----------------------- F-04: config mutation gating -----------------------

def test_config_update_requires_auth(monkeypatch):
    # Don't touch the real config.json on disk during the mutation path.
    monkeypatch.setattr("src.api.system.save_config", lambda cfg: None)
    c = _client(auth_token="s3cret")
    r = c.put("/api/system/config/network", json={"command_rate_hz": 20})
    assert r.status_code == 401
    r2 = c.put("/api/system/config/network", json={"command_rate_hz": 20},
               headers={"Authorization": "Bearer s3cret"})
    assert r2.status_code == 200


def test_config_update_disabled_returns_503():
    c = _client(auth_token="s3cret", mutation=False)
    r = c.put("/api/system/config/network", json={"command_rate_hz": 20},
              headers={"Authorization": "Bearer s3cret"})
    assert r.status_code == 503


# ----------------------- F-09: secret redaction -----------------------

def test_config_get_redacts_tokens():
    config = AppConfig(
        drones=[DroneEntry(mac="AA:BB:CC:DD:EE:FF", name="stub",
                           command_token="dronesecret")],
        network=NetworkConfig(command_token="fleetsecret"),
        server=ServerConfig(auth_token="serversecret"),
    )
    app.state.config = config
    app.state.manager = _StubManager(config)
    c = TestClient(app)
    body = c.get("/api/system/config").json()
    assert "command_token" not in body["network"]
    assert "auth_token" not in body["server"]
    assert all("command_token" not in d for d in body["drones"])
    netbody = c.get("/api/system/config/network").json()
    assert "command_token" not in netbody


# ----------------------- F-07: drone metadata validators -----------------------

def test_add_drone_rejects_bad_mac():
    c = _client(auth_token=None)
    r = c.post("/api/drones", json={"mac": "not-a-mac", "name": "x"})
    assert r.status_code == 422


def test_add_drone_rejects_xss_name():
    c = _client(auth_token=None)
    r = c.post("/api/drones",
               json={"mac": "11:22:33:44:55:66", "name": "<script>alert(1)</script>"})
    assert r.status_code == 422


def test_add_drone_rejects_bad_mdns_hostname():
    c = _client(auth_token=None)
    r = c.post("/api/drones",
               json={"mac": "11:22:33:44:55:66", "name": "ok",
                     "mdns_hostname": "http://evil.internal:80"})
    assert r.status_code == 422


def test_add_drone_accepts_valid():
    c = _client(auth_token=None)
    r = c.post("/api/drones",
               json={"mac": "11:22:33:44:55:66", "name": "good-drone",
                     "mdns_hostname": "floppi-5566", "tags": ["alpha", "beta"]})
    assert r.status_code == 200, r.text


def test_update_drone_rejects_oversized_tag():
    c = _client(auth_token=None)
    r = c.put("/api/drones/AA:BB:CC:DD:EE:FF",
              json={"tags": ["x" * 50]})
    assert r.status_code == 422


# ----------------------- F-06: WS command validation + auth -----------------------

def test_ws_open_when_auth_disabled_and_validates_frames():
    c = _client(auth_token=None)
    with c.websocket_connect("/ws/dashboard") as ws:
        # malformed frame (non-numeric channel) must be dropped, not crash
        ws.send_json({"type": "command", "mac": "AA:BB:CC:DD:EE:FF",
                      "channels": {"ch1": "boom"}})
        # valid frame
        ws.send_json({"type": "command", "mac": "AA:BB:CC:DD:EE:FF",
                      "channels": {"ch3": 1200}})
    drone = app.state.manager.drones["AA:BB:CC:DD:EE:FF"]
    # only the valid frame should have produced a clamped command
    assert drone.sent == [{"ch1": 1500, "ch2": 1500, "ch3": 1200,
                           "ch4": 1500, "ch5": 1000, "ch6": 1000}]


def test_ws_rejects_without_token_when_auth_enabled():
    c = _client(auth_token="s3cret")
    from starlette.websockets import WebSocketDisconnect as WSD
    with pytest.raises(WSD):
        with c.websocket_connect("/ws/dashboard") as ws:
            ws.receive_text()


def test_ws_accepts_with_token_query_param():
    c = _client(auth_token="s3cret")
    # TestClient sets Origin to the testserver host -> same-origin allowed.
    with c.websocket_connect("/ws/dashboard?token=s3cret") as ws:
        ws.send_json({"type": "command", "mac": "AA:BB:CC:DD:EE:FF",
                      "channels": {"ch3": 1300}})
    drone = app.state.manager.drones["AA:BB:CC:DD:EE:FF"]
    assert drone.sent[-1]["ch3"] == 1300


# ----------------------- F-10: telemetry-trust XSS (dashboard DOM sinks) -----------------------

def _dashboard_html() -> str:
    c = _client(auth_token=None)
    r = c.get("/")
    assert r.status_code == 200
    return r.text


def test_dashboard_renderfleet_has_no_innerhtml_sink():
    """The fleet renderer must not push drone-derived strings (name/mac/ip)
    into innerHTML. A spoofed/compromised drone controls telemetry net.ip
    (adopted verbatim) and could otherwise inject markup into the operator DOM.
    """
    import re
    html = _dashboard_html()
    # Isolate the renderFleet function body.
    start = html.index("function renderFleet")
    end = html.index("function updateFleetCard")
    body = html[start:end]
    # No HTML-sink ASSIGNMENT in the renderer (a bare mention in a comment is
    # fine; an `.innerHTML = <data>` assignment is the actual sink).
    assert re.search(r"\.(?:inner|outer)HTML\s*=", body) is None
    assert "insertAdjacentHTML" not in body
    # Drone-derived values must reach the DOM via text nodes / textContent.
    assert "createTextNode" in body or "textContent" in body
    # The old vulnerable template-literal interpolation of d.name/d.ip is gone.
    assert "${d.name}" not in body
    assert "${d.ip}" not in body


def test_dashboard_has_no_html_interpolation_sinks_anywhere():
    """Defense-in-depth: no innerHTML/outerHTML/insertAdjacentHTML assignment
    carries an interpolated value in the whole dashboard script.
    """
    html = _dashboard_html()
    # The only remaining occurrences are in comments / a literal clear ('').
    # Assert there is no `innerHTML = ` assignment with a template literal or
    # concatenation (i.e. an actual sink carrying data).
    import re
    # match `<x>innerHTML = <something other than empty string>`
    sinks = re.findall(r"\.(?:inner|outer)HTML\s*=\s*(?![\"']{2}\s*;)[^;\n]+", html)
    assert sinks == [], f"unexpected HTML sink(s): {sinks}"


# ----------------------- F-10: telemetry-driven IP validation (server-side) -----------------------

def test_telemetry_ip_validator_accepts_valid_ips():
    """_valid_ip returns the address for well-formed IPv4/IPv6, else None."""
    from src.drone import _valid_ip
    assert _valid_ip("192.168.1.50") == "192.168.1.50"
    assert _valid_ip("10.0.0.1") == "10.0.0.1"
    assert _valid_ip("::1") == "::1"
    assert _valid_ip("fe80::1") == "fe80::1"


def test_telemetry_ip_validator_rejects_malformed():
    """Malformed / non-IP / injection strings are dropped (return None)."""
    from src.drone import _valid_ip
    assert _valid_ip("not-an-ip") is None
    assert _valid_ip("192.168.1.999") is None
    assert _valid_ip('<img src=x onerror=alert(1)>') is None
    assert _valid_ip("evil.internal") is None
    assert _valid_ip("") is None
    assert _valid_ip(None) is None
    assert _valid_ip(12345) is None


class _FakeResp:
    status_code = 200

    def __init__(self, payload):
        self._payload = payload

    def json(self):
        return self._payload


def _poll_with_telemetry(reported_ip):
    """Drive DroneClient.poll_status once with a stubbed HTTP response that
    reports `reported_ip` in net.ip. Returns the client after the poll.

    Run synchronously via asyncio.run to match the existing TestClient-based
    suite (the project venv has no pytest-asyncio).
    """
    from src import drone as drone_mod

    client = drone_mod.DroneClient(mac="AA:BB:CC:DD:EE:FF", name="t",
                                   ip="192.168.1.10")

    async def _fake_get(url):
        return _FakeResp({"net": {"ip": reported_ip}})

    client._http.get = _fake_get  # type: ignore[assignment]

    async def _run():
        data = await client.poll_status()
        await client.close()
        return data

    data = asyncio.run(_run())
    assert data is not None
    return client


def test_poll_status_drops_malformed_telemetry_ip():
    """poll_status must not adopt a malformed drone-reported net.ip.

    A spoofed drone returns a markup payload as its IP; the server must keep
    its original known IP rather than ingest the attacker-controlled string.
    """
    client = _poll_with_telemetry("<script>alert(1)</script>")
    assert client.ip == "192.168.1.10"
    assert client.state.ip == "192.168.1.10"


def test_poll_status_adopts_valid_telemetry_ip():
    """A valid drone-reported net.ip is adopted as before."""
    client = _poll_with_telemetry("192.168.1.42")
    assert client.ip == "192.168.1.42"
    assert client.state.ip == "192.168.1.42"


# ----------------------- F-11: configurable bind host -----------------------

def test_server_host_defaults_to_all_interfaces():
    """Backward-compat: default bind host is 0.0.0.0 (unchanged behaviour)."""
    assert ServerConfig().host == "0.0.0.0"


def test_server_host_is_configurable():
    cfg = AppConfig(server=ServerConfig(host="127.0.0.1"))
    assert cfg.server.host == "127.0.0.1"


def test_config_json_on_disk_parses_with_server_host():
    """load_config() parses the committed config.json including server.host."""
    from src.config import load_config, CONFIG_PATH
    cfg = load_config(CONFIG_PATH)
    assert isinstance(cfg.server.host, str)
    assert cfg.server.host  # non-empty
