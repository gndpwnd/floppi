# FIRST SUCCESS — Mega Balance Robot

**Goal**: brand-new operator, hardware in hand, reaches a first BOOTSTRAP+RUN success on the `mega_balance` build.

> Honest framing: the Mega bot has **not** yet stably balanced on a bench. This checklist is the path the firmware is built to walk; if any step diverges from the expected serial line, jump to the failure-mode pointer instead of pushing forward. Re-read [`USER_GUIDE.md`](USER_GUIDE.md) §STATUS BANNER if you have any doubt about what "success" means here.

Pre-flight: 18650 + boost rail delivers ≥ 4.7 V at the L298N logic pin under stationary load; serial monitor open at **115200 baud**.

---

1. **Verify wiring against `HARDWARE_SETUP.md`.**
   - BNO055 on I²C: SDA=20, SCL=21, addr 0x28. L298N: ENA=5, IN1=7, IN2=6, ENB=10, IN3=8, IN4=9.
   - Encoders: L A/B = 18/19; R A/B = 2/3. Button on D4 → GND.
   - Expected (visual only): no shorted rails, no smoking.
   - Failure-mode: [`HARDWARE_SETUP.md`](HARDWARE_SETUP.md) wiring table — every cell.

2. **Flash `mega_balance`.**
   ```bash
   pio run -e mega_balance -t upload
   ```
   - Expected serial: `B` (boot banner, single character, ~1 s after reset).
   - Failure-mode: [`TROUBLESHOOTING.md` §1 "Boot prints `BF`"](TROUBLESHOOTING.md#1-boot-and-imu) — IMU init failed.

3. **Open serial monitor, expect ready banner.**
   - Expected serial: `READY` after the boot banner and EEPROM-restore lines (`ld enc …`, `ld pd …` if encoder/PWM-disc slots populated).
   - If prop-and-go fires (saved mount + |pitch − mount| < 5°), `[state] -> BOOT` follows within 2 s. Send `a` once to abort to IDLE before continuing.
   - Failure-mode: [`TROUBLESHOOTING.md` §1 "Boot prints `B` then nothing"](TROUBLESHOOTING.md#1-boot-and-imu).

4. **Capture mounting offset — `c`.**
   - Prop the bot perfectly upright on a level surface, fingertips holding it still. Send `c`.
   - Expected serial: `[state] -> CAP` immediately, then `[state] -> BOOT` after 2 s if σ_pitch ≤ 0.5°, then `sv m=<value>` on the next BOOTSTRAP→IDLE transition (failure path) or on a fresh capture.
   - Failure-mode: [`TROUBLESHOOTING.md` §6 "[state] -> CAP then [state] -> IDLE without auto-chaining"](TROUBLESHOOTING.md#6-mount-capture-and-recapture-criteria).

5. **Encoder calibration — `e` (Mega only).**
   - Bot in IDLE on the floor. Send `e`.
   - Expected serial: `enc_cal: roll bot 1.000 m, press btn`, then once-per-second `L_ticks=… R_ticks=…`. Hand-roll the bot exactly 1.000 m forward along a tape measure, then press the D4 button.
   - On save: `enc_cal: L=<cpm> R=<cpm> r=<radius> saved`.
   - Failure-mode: [`TROUBLESHOOTING.md` §7 "`enc_cal: no ticks - not saved`"](TROUBLESHOOTING.md#7-encoder-issues) — dead encoder or swapped A/B leads.

6. **PWM discovery — `p` (Mega only).**
   - **Lift the bot off the ground.** Both wheels must spin freely. Send `p`.
   - Expected serial: `[state] -> PWMD` immediately, then per-step `pd#<idx> pwm=<cmd> g0=<L_vel> m=<R_vel> thr=<delta> ok=<flag>` lines, then on success `sv pd min=<min> max=<max>` and `[state] -> IDLE`.
   - Failure-mode: [`TROUBLESHOOTING.md` §8 "PWM discovery failures"](TROUBLESHOOTING.md#8-pwm-discovery-failures) — `pd fail r=<reason>` reasons 4/8/9.

7. **BOOTSTRAP — `b`.**
   - Prop the bot upright, fingertips off **before** sending. Send `b`.
   - Expected serial: `[state] -> BOOT`, then four `bs#<idx> pwm=±180/±240 g0=… m=… thr=… ok=<0|1>` lines over ~2.5 s.
   - Failure-mode: [`TROUBLESHOOTING.md` §2 "BOOTSTRAP failure_reasons"](TROUBLESHOOTING.md#2-bootstrap-failure_reasons) — reasons 1–7.

8. **Verify BOOTSTRAP success.**
   - Send `s`. Expected serial: `RUN <pitch> <mount> <output> <stiction>`.
   - State `RUN` is the gate: `BootstrapResult.failure_reason == 0`, gains derived, transitioned to RUN. Any other state in field 1 means BOOTSTRAP failed — read the prior `bs#…` lines for which pulse drove it out.
   - Failure-mode: [`USER_GUIDE.md` §5.1 failure_reason table](USER_GUIDE.md#51-bootstrap-failure_reason-codes) — every reason 0–8 has a name and a remedy.

9. **Bench-release upright.**
   - Once `s` reports `RUN`, slowly lower the bot to release. Watch for sustained balance vs. immediate tip.
   - Expected serial: no state transition lines for at least 5 s (the bootstrap-freeze window). After that the continuous RLS adapts gains live.
   - Failure-mode: [`TROUBLESHOOTING.md` §4 "Bot enters RUN, immediately tips"](TROUBLESHOOTING.md#4-run-state-hangs-and-stuck-motor-cut) — usually `K_motor` mis-measurement or stale mount.

10. **Check telemetry via `g`.**
    - Send `g`. Expected serial: a single flat line `G,<millis>,<pitch_deg>,<pitch_sp_deg>,<wheel_vel_mps>,<position_m>,<nudge_deg>,<k_pos>,<k_vel>,<pos_leak>`.
    - Field 2 (pitch) should oscillate within ±2° of field 3 (setpoint). Non-zero `k_pos`/`k_vel`/`pos_leak` confirms the Phase 4M.14 outer-loop derivation ran (not the `*_FALLBACK` constants).
    - Failure-mode: [`KNOWN_ISSUES.md`](../../KNOWN_ISSUES.md) KI-2 through KI-10 — the unresolved-on-hardware cascade.

---

After a clean first-success run: power-cycle to confirm prop-and-go fires automatically (`READY` → 2 s grace → `[state] -> BOOT` → `[state] -> RUN`). Photograph the EEPROM state with `B` (the FIN-05 photo-backup printer) so you can rebuild without re-walking steps 4–7. The Workstream G verification protocol — [`../../findings/workstream_g_bench_protocol_2026-05-21.md`](../../findings/workstream_g_bench_protocol_2026-05-21.md) — is the next layer once a single RUN session lasts ≥ 30 s.
