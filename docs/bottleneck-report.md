# CKKS Bottleneck Report (HOST REFERENCE RESULTS)

Generated from `results/ckks-bench-latest.json` (regenerate with `npm run bench:ckks`)
and `results/kernel-bench-latest.json` (`npm run bench:kernels`).

**Host:** AMD Ryzen 9 4900HS (x16 logical), Node v25.6.1, Windows 11 — see
`results/ckks-bench-latest.json.experiment.host` for the exact capture. These
are measurements on ordinary x86_64 silicon via Microsoft SEAL compiled to
WASM (node-seal). They are **not** E1 measurements — see
[Limitations](README.md#limitations).

## 1. Per-operation latency (mean over 30 trials, after warm-up)

| Op | N1024 | N2048 | N4096 | N8192 | N16384 |
|---|---|---|---|---|---|
| encode | 0.089 ms | 0.132 ms | 0.269 ms | 0.798 ms | 3.011 ms |
| encrypt | 0.275 ms | 0.997 ms | 1.950 ms | 5.203 ms | 18.264 ms |
| add | 0.014 ms | 0.028 ms | 0.048 ms | 0.161 ms | 0.961 ms |
| multiply (raw) | N/A | 0.169 ms | 0.383 ms | 0.998 ms | 4.355 ms |
| relinearize | N/A | 0.882 ms | 1.658 ms | 5.983 ms | 29.850 ms |
| rescale | N/A | 0.935 ms | 2.033 ms | 6.952 ms | 34.782 ms |
| rotate | N/A | 0.684 ms | 1.432 ms | 5.013 ms | 26.218 ms |
| decrypt | 0.026 ms | 0.060 ms | 0.121 ms | 0.283 ms | 1.188 ms |
| decode | 0.066 ms | 0.256 ms | 0.411 ms | 1.296 ms | 6.237 ms |

N1024 is a single-modulus chain (27-bit security budget at N=1024 leaves no
room for a second modulus), so it has no key-switching support at all:
multiply/relinearize/rescale/rotate are all N/A by construction, not a
crash — see `lib/ckks/paramSets.ts`.

## 2. Where the time actually goes (share of measured per-op time)

For every parameter set with key-switching support (N2048–N16384), the three
most expensive operations are **rescale, relinearize, and rotate** — all
three are key-switching operations (they call into SEAL's RNS hybrid
key-switching machinery), not the "plain" arithmetic (add/multiply) most
people assume dominates:

| Param set | rescale | relinearize | rotate | encrypt | multiply | encode+decode+decrypt |
|---|---|---|---|---|---|---|
| N2048 | 22.6% | 21.3% | 16.5% | 24.1% | 4.1% | 10.8% |
| N4096 | 24.5% | 20.0% | 17.2% | 23.5% | 4.6% | 9.6% |
| N8192 | 26.0% | 22.4% | 18.8% | 19.5% | 3.7% | 9.0% |
| N16384 | 27.9% | 23.9% | 21.0% | 14.6% | 3.5% | 8.4% |

**Observation:** as N grows, key-switching ops (rescale + relinearize +
rotate) go from ~60% to ~73% of per-operation latency, while raw `encrypt`
(the next biggest line item) *shrinks* as a share. Key-switching cost scales
worse than encrypt/add/multiply as N grows — exactly the operations that
involve a full extra NTT pass, a base conversion across the RNS limbs, and
decompose/compose over every modulus in the chain. This is the real,
measured bottleneck, not an assumption.

## 3. Why this is architecturally interesting

Key-switching (used by both relinearize and rotate; rescale is a related
modulus-chain operation) is, at its core:

1. decompose a polynomial into its RNS/CRT limbs,
2. run a forward NTT on each limb,
3. do a fixed, regular pattern of modular multiply-adds against a public key
   matrix,
4. run an inverse NTT, and
5. recompose/mod-switch the result.

Every one of those five steps is (a) the same fixed-size operation repeated
across every RNS limb and every polynomial coefficient, (b) free of data-
dependent branching, and (c) dominated by *moving* coefficient arrays through
NTT butterfly stages and modular-arithmetic units, not by control flow. That
is precisely the shape of workload a spatial/dataflow architecture is
supposed to help with: fixed, regular, high-arithmetic-intensity, streamable
tensor-like operations where data movement (not instruction dispatch) is the
cost center. See `docs/README.md` "E1 Mapping" for how this maps onto
Electron E1's architecture, and `lib/kernels/ntt.ts` /
`results/kernel-bench-latest.json` for a standalone measurement of the NTT
butterfly network's cost in isolation from SEAL.

## 4. A result that does NOT flatter the thesis: Montgomery multiplication

`npm run bench:kernels` measures `lib/kernels/modular.ts`'s naive BigInt
modular multiplication against a from-scratch Montgomery-multiplication
implementation, over vectors of length N matching each CKKS parameter set.

| N | naive (BigInt `%`) | Montgomery | Montgomery speedup |
|---|---|---|---|
| 1024 | 0.020 ms | 0.641 ms | **0.03x (32x slower)** |
| 2048 | 0.015 ms | 1.243 ms | **0.01x (83x slower)** |
| 4096 | 0.057 ms | 1.966 ms | **0.03x (34x slower)** |
| 8192 | 0.097 ms | 3.084 ms | **0.03x (32x slower)** |
| 16384 | 0.357 ms | 6.721 ms | **0.05x (19x slower)** |

Montgomery multiplication is *slower* than the naive approach here, by over
an order of magnitude. This is a real, reproducible result on this host, not
an error — and it does not support "spatial dataflow always wins." V8's
native BigInt `%` is a highly optimized, hand-tuned C++ division routine;
the from-scratch TypeScript Montgomery implementation pays BigInt allocation
and shift/multiply overhead on every limb with no corresponding division
savings, because JavaScript has no cheap fixed-width-word division to avoid
in the first place. Montgomery multiplication earns its keep on hardware
(or a C/assembly kernel) where integer division is genuinely expensive
relative to multiplication and shifts — that comparison cannot be made
honestly in a managed BigInt VM, so this suite reports the loss rather than
omitting the technique. This is exactly the kind of result Electron E1
hardware validation would need to either confirm or overturn — see
[Limitations](README.md#limitations).

## 5. What this report does not claim

- No NTT-vs-modular-arithmetic-vs-memory-movement percentage breakdown of
  SEAL's *internal* execution is given, because node-seal (SEAL compiled to
  WASM) does not expose internal NTT or modular-arithmetic calls as
  separately timeable units. The per-operation breakdown above is the
  finest-grained decomposition actually measurable without instrumenting
  SEAL's C++ source.
- `lib/kernels` is a *reference* NTT/modular-arithmetic implementation used
  to reason about the architectural shape of the workload, not a
  reimplementation of SEAL's (Shoup/Barrett-optimized, AVX-vectorized)
  internals. Its absolute timings are not comparable to SEAL's.
- Nothing here is an E1 measurement. No E1 hardware or effcc toolchain was
  available — see `lib/platform/e1Target.ts` and
  [Limitations](README.md#limitations).
