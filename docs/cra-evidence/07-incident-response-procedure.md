# Incident Response Procedure (template)

**This is a draft/example procedure, not an operating capability.** No on-call rotation, ticketing
integration, or disclosure mailbox actually exists for this prototype. It is included so a team adopting
this codebase has a starting skeleton, not because this project runs one today.

1. **Intake** — a report arrives (e.g. a vulnerability in `node-seal` or in code in this repo).
2. **Triage** — classify severity using the same bands as `04-vulnerability-register.md`
   (info/low/moderate/high/critical, per `npm audit`'s scale where applicable).
3. **Containment** — for a dependency vulnerability: pin/patch via `package-lock.json`, rerun
   `npm run security:sbom` and `npm run security:evidence` to regenerate evidence. For a logic/crypto
   issue: identify the affected parameter set(s) in `lib/ckks/paramSets.ts` or policy rule in
   `lib/policy/scheduler.ts`.
4. **Remediation** — fix, add a regression test under `lib/**/*.test.ts`, rerun `npm test`.
5. **Disclosure** — for a real deployment: notify affected downstream consumers per
   `05-security-update-policy.md` (not yet implemented here).
6. **Postmortem** — record root cause and update `docs/threat-model.md` if a new threat class was involved.
