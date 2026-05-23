# Index: swarm_api Findings

Research, audits, and analysis for the swarm_api ground station.

## Quick Links

- [security_audit_2026-05-22.md](security_audit_2026-05-22.md) — Read-only security audit of swarm_api's own attack surface. 12 findings (2 P0: no server auth, WS hijack; 3 P1; 4 P2; 3 P3) with remediation order.
- [auth_qa_review_2026-05-22.md](auth_qa_review_2026-05-22.md) — Independent adversarial QA of the opt-in server auth layer. Verdict: **GO**. Per-route gate verification, constant-time compare, WS handshake, secret redaction, validator probing.

## Organization

Documents organized by topic, dated where session-bound.

## Latest Work

- **2026-05-22** — Security hardening: audit → opt-in auth layer → QA GO. The two findings above are the audit input and the QA sign-off. Implementation + test summary lives in the session record:
  - [docs/archive/session_records/2026-05-22_security_hardening.md](../archive/session_records/2026-05-22_security_hardening.md)
