# Security Update Policy

**Current state: none implemented.** This is a research prototype distributed as source via git, not a
shipped product. There is no signed-update mechanism, no OTA channel, and no version-pinning guidance
issued to downstream consumers.

What a real product built on this codebase would need before any security-update-policy claim could be
supported:

- A signed release/build pipeline (this repo has none — see `08-build-reproducibility-record.md`).
- A defined patch cadence and severity-to-SLA mapping (e.g. critical within 7 days).
- A notification channel for downstream consumers (mailing list, security advisory feed, etc).

This document exists so that gap is visible, not hidden.
