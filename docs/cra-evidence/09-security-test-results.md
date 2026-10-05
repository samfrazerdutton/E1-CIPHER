# Security Test Results

_Generated 2026-10-05T01:42:41.448Z by actually running `npm test` — not a recorded/assumed pass._

**Result: PASS**

```
ℹ pass 3
```

Scope note: this repo's test suite (`lib/kernels/*.test.ts`) covers cryptographic-kernel *correctness*
(NTT round-trip, convolution-vs-naive match, Montgomery-vs-naive modular multiplication match) — it is not
a penetration test, a fuzzing campaign, or a formal security audit. See `docs/threat-model.md` for what has
and has not been assessed.
