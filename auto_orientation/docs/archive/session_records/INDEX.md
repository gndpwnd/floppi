# Session Records — Index

Dated, comprehensive logs of work sessions. Pattern: `YYYY-MM-DD_topic-slug.md`. Each session record contains:

- What was done (concrete changes, file paths)
- Decisions made + rationale
- Blockers / open questions
- Next steps for the following session
- Cross-references to new findings / design docs / commits

| Date | Slug | Summary |
|------|------|---------|
| 2026-05-12 | [framework-planning](2026-05-12_framework-planning.md) | Phase 4 re-scope + 12 parallel research agents producing the design corpus. |
| 2026-05-12 | [uno_balancing_hardware](2026-05-12_uno_balancing_hardware.md) | First Uno + BNO055 + L298N bench bring-up — calibration wizard, mount capture, motor test. |
| 2026-05-12 | [evening_balance_iteration](2026-05-12_evening_balance_iteration.md) | Tier 1+2 iteration: conservative gains, HELD/FALLEN state machine, lenient resume. |
| 2026-05-12 | [evening_phase4_landing](2026-05-12_evening_phase4_landing.md) | Phase 4 structural fixes (raw-gyro D-term, MsTimer2 ISR, soft-cutoff) + Phase 4.10 universal RLS auto-tune landed. Builds @ 99.9% flash. |
| 2026-05-12 | [drone_vs_bot_cross_project_research](2026-05-12_drone_vs_bot_cross_project_research.md) | Cross-project synthesis: why drones are simpler than balance bots, BNO055 cal audit, FC PID architecture lessons. Three findings docs landed. |
| 2026-05-12 | [BENCH_BNO055_FROZEN_DIAGNOSIS](2026-05-12_BENCH_BNO055_FROZEN_DIAGNOSIS.md) | Live bench: pitch frozen at 1.46° for 30 s during tilt test. Fixed with `-D BNO055_NO_EXT_CRYSTAL`. Sensor confirmed alive after fix. |
| 2026-05-18 | [BENCH_MOTOR_STICTION_DIAGNOSIS](2026-05-18_BENCH_MOTOR_STICTION_DIAGNOSIS.md) | Continuation: motors didn't spin at PWM 90 but did at 200. Real blocker = `stiction_min_pwm=0` masking a ~100 PWM dead-band. Explains the whole tuning frustration. |
| 2026-05-18 | [PHASE2_FLASH_TRIMS_AND_HEURISTICS](2026-05-18_PHASE2_FLASH_TRIMS_AND_HEURISTICS.md) | Flash bottleneck root-caused (`snprintf` chain in `getStatusString` = 1.3 KB). Freed 1.7 KB. Landed Phase 2.1 (measured CHARACTERISE threshold), 2.5 (external-motion HELD), 2.6 (gain scheduling). Cleaned platformio.ini. |
| 2026-05-18 PM | [BENCH_VALIDATION_AND_VIOLATIONS_AUDIT](2026-05-18_PM_BENCH_VALIDATION_AND_VIOLATIONS_AUDIT.md) | 6 rounds of bench testing. Removed lateral-gyro HELD trigger (was firing on recovery transients). Tightened soft zone 2°→1°. Sped online-est 20s→8s. **Architectural fix**: mount estimator now tracks pitch_deg directly (was I-term based, structurally couldn't converge). **Realisation**: every hardcoded gain/threshold is a scope violation. scope.md now has a 19-row audit listing each violation with its replacement plan. **Next**: BOOTSTRAP state (Phase 4.10c) — the unblocking work. |
| 2026-05-18 PM evening | [BOOTSTRAP_PHASE_4_10C_LANDED](2026-05-18_PM_BOOTSTRAP_PHASE_4_10C_LANDED.md) | **Phase 4.10c shipped**: BOOTSTRAP state measures K_motor from ±PWM pulses, derives Kp/Kd/Ki via pole-placement (Kp=ω_n²/K, Kd=2ζω_n/K), pushes to PID before RUN. Hardcoded Kp=50/Ki=1/Kd=10 + `R` command + relay tuner all REMOVED. Boot auto-bootstraps with stale-mount sanity check. Uno flash 95.6%→92.2% (relay tuner deletion saved 1.3 KB; BOOTSTRAP cost 200 B; net +1.1 KB headroom). 123/123 native tests pass (test_balance_app_bootstrap NEW with 27 assertions + seed_k_motor test). 7/21 scope violations retired. |
| 2026-05-18 PM late | [BOOTSTRAP_FIRST_BENCH](2026-05-18_PM_LATE_BOOTSTRAP_FIRST_BENCH.md) | **First on-hardware run of BOOTSTRAP.** Found + fixed two pre-bench bugs: (a) noise-threshold time-scale mismatch (per-tick |α| baseline vs 100ms-averaged pulse — over-threshold by ~5–20×); (b) pulse magnitudes too low for real motors (100/150 PWM → raised to 180/240). Added per-pulse Serial telemetry to BOOTSTRAP + CHARACTERISE — single-line records (`bs#0 pwm=180 g0=-0.1 m=9.5 thr=1.1 ok=1`) closed the "motors not moving" mystery in one flash cycle. Bot transitioned IDLE→BOOTSTRAP→RUN with K≈0.38, Kp≈168 — **twitched but didn't balance**. Three open problems: K spread 0.09–0.74 across pulses (cooldown too short, bot tumbling), operator-motion poisons baseline (touched bot → thr jumps to 50 dps → all pulses fail), aggressive pole target may exceed BNO055 NDOF phase budget. Uno flash 92.2%→94.9%. 27/27 BOOTSTRAP native tests still pass. |
| 2026-05-19 PM | [MULTI_AGENT_LANDING_WAVE](2026-05-19_PM_MULTI_AGENT_LANDING_WAVE.md) | ~30-agent landing wave + strategic pivot to Mega/Uno bifurcation. Collision detection, wheel encoder driver, Phase 4M.12 PWM auto-discovery, Uno-minimal program, Python brute-force tuner all landed; full green build matrix; 16 findings/design docs. |
| 2026-05-20 | [multi_agent_sync_audits_and_fixes](2026-05-20_multi_agent_sync_audits_and_fixes.md) | ~20-agent sync day: err0r two-clone divergence merged (`ec4ef53`); 5 audits + state reconciliation (148 findings) — discovered Phases 4M.0/4M.1/4M.12 already landed and the audits were stale; F2 RAM fix, 4 P1 calibration security fixes + `CAL_FORMAT_VERSION` bump, 36 new tests, INFRA build-script rewrite. |

**Pre-existing session summaries** still live one level up in [`../`](../INDEX.md) as a flat archive — they were created before this subfolder.

*Last updated: 2026-05-20.*
