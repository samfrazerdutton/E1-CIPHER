# Technical Deep Dive

## CKKS

CKKS (Cheon-Kim-Kim-Song) is a leveled homomorphic encryption scheme
over approximate real/complex numbers, built on the Ring Learning With
Errors (RLWE) problem. Plaintexts are vectors of real numbers, encoded
into a polynomial, then encrypted into a ciphertext pair of polynomials.
Addition and multiplication on ciphertexts correspond to addition and
multiplication on the encoded plaintext vectors — approximately:
every operation introduces a small amount of noise, and every
multiplication requires "spending" one level of a finite modulus chain
to control that noise (`rescale`). This project uses Microsoft SEAL's
CKKS implementation (`crypto::CkksBackend`, `include/e1cipher/crypto/ckks_backend.hpp`),
not a from-scratch implementation — see `docs/architecture-assessment.md`'s
"do not implement cryptography from scratch merely to claim you did."

## RNS (Residue Number System)

A ciphertext's modulus is a product of several smaller "RNS limb" primes
rather than one huge integer, so arithmetic can be done limb-by-limb in
machine-word-sized pieces instead of giant multi-precision integers.
`lib/ckks/paramSets.ts`'s (TS-era) and `crypto::ckks_param_sets()`'s
(current) `coeff_modulus_bits` arrays are literally this list of limb
sizes — e.g. N8192's `{60, 40, 40, 60}`.

## NTT (Number-Theoretic Transform)

The finite-field analogue of the FFT: multiplying two degree-N
polynomials naively costs O(N²); transforming both into the NTT
"evaluation" domain, multiplying pointwise (O(N)), and transforming back
costs O(N log N). SEAL uses this for every polynomial multiplication
internally. This project's `crypto::kernels::ntt` (`src/crypto/kernels/ntt.cpp`)
is a from-scratch reference implementation of the same algorithmic shape
— used for architectural reasoning and as the `ACCELERATED` placement's
candidate-kernel proxy, **not** a reimplementation of SEAL's own
(Shoup/Barrett-optimized, likely AVX-vectorized) internal NTT, and its
absolute timings are not directly comparable to SEAL's.

## Key switching

Several CKKS operations (relinearization after a multiply, Galois
rotation) require transforming a ciphertext's secret-key basis, which is
done via a public "key-switching" matrix: decompose the ciphertext across
RNS limbs, NTT each limb, multiply-add against the key-switching matrix,
inverse-NTT, recompose. Measured in this repository
(`e1cipher experiment accelerator`, N8192): `relinearize` + `rescale` +
`rotate` together are **64.6%** of per-operation latency — the single
largest cost center in CKKS, by a wide margin, once encode/encrypt/
decrypt/decode and the raw add/multiply are accounted for separately.

## Why encrypted computation is expensive

Two measured reasons, both in this repository:

1. **Ciphertext expansion**: a 48-byte plaintext feature vector becomes a
   131,185-byte ciphertext at N4096 — **2,733x**. This is a property of
   the scheme's security parameters (the ciphertext must be large enough
   to carry RLWE noise safely), not an implementation inefficiency.
2. **Key-switching cost**: see above — 64.6% of per-op time in one
   structurally regular but computationally heavy operation family.

## Why feature-level encryption matters

See `docs/adr/003-feature-level-encryption.md`. Applying the measured
2,733x expansion to a 2MB raw camera frame instead of a 48-byte feature
vector would be a different order of magnitude of infeasibility, not a
proportional cost increase this architecture could absorb. Reducing raw
sensor data to a compact feature vector *before* any encryption decision
is what makes CKKS viable at all in this workload.

## Why spatial architectures are interesting

The key-switching family's internal shape — fixed-size, branch-free,
dominated by moving coefficient arrays through regular NTT butterfly
stages and modular multiply-adds — is exactly the kind of workload a
spatial-dataflow architecture is supposed to help with: parallel
processing elements per butterfly stage or per RNS limb, streaming
producer/consumer between stages, data reuse across the fixed
key-switch matrix. This is an architectural *hypothesis*
(`docs/e1-hardware-validation-plan.md`), motivated by a real measurement,
not a verified hardware fact.

## What E1 would need to accelerate

Per `docs/adr/002-ntt-as-accelerator-candidate.md` and
`include/e1cipher/runtime/execution_backend.hpp`'s `KernelId` enum: the
NTT/inverse-NTT transform, specifically. Modular multiply and feature
aggregation are also modeled as candidate kernels (both measured LOW
VALUE in this workload's actual cost breakdown — see the experiment
output — included for completeness of the `ExecutionBackend` contract,
not because they're the interesting case). RNS decomposition and the
key-switch inner loop are explicitly NOT implemented as standalone host
kernels here, because SEAL performs them internally and this project
will not claim a capability it hasn't built.

## What remains unverified

Everything about actual E1 behavior. No E1 hardware or effcc toolchain
was available while building this project. Every number in this
document and in this repository's benchmarks is `HOST_REFERENCE` —
measured on ordinary x86_64 silicon — never `REAL_HARDWARE`. The
specific, falsifiable claim this project is set up to test, once
hardware access exists, is written out in full in
`docs/e1-hardware-validation-plan.md`, including what would count as the
hypothesis failing.
