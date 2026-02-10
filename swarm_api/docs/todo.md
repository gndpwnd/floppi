# Swarm API - Todo

> Last updated: 2026-02-10

## In Progress

_(none)_

## Blocked

_(none)_

## Up Next

- [ ] Test with real ESP32 drone on WiFi (telemetry display, command sending)
- [ ] Dashboard responsive layout polish for tablet/phone
- [ ] Network interface selection support
- [ ] Gamepad/joystick input mapping (browser Gamepad API)

## Backlog

- [ ] Telemetry recording/playback
- [ ] Drone groups/tags
- [ ] Config editor in dashboard UI
- [ ] Notification system (drone disconnected, low signal)

## Recently Completed

- [x] Project scope, roadmap, docs — 2026-02-10
- [x] Config management (Pydantic validation, load/save) — 2026-02-10
- [x] DroneClient (HTTP polling + WebSocket + command sending) — 2026-02-10
- [x] DroneManager (fleet lifecycle, mDNS + IP discovery, health monitoring) — 2026-02-10
- [x] REST API (CRUD drones, telemetry, commands) — 2026-02-10
- [x] Dashboard WebSocket bridge (drone telemetry → browser, browser commands → drone) — 2026-02-10
- [x] Web dashboard (fleet panel, drone control view, sliders, arm/disarm) — 2026-02-10
- [x] Scope updated with actual firmware API contract from code review — 2026-02-10

---

## Notes

- ESP32 `/api/commands` IS implemented in firmware — both POST and WebSocket commands work
- First real test needs an ESP32 running floppi firmware with WiFi on the same network
- Command rate is 10Hz when actively controlling — drone failsafe at 500ms timeout

---

*Update every session: start by reading, end by updating.*
