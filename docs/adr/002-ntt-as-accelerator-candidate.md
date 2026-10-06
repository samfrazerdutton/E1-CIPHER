# ADR-002: NTT as Accelerator Candidate

## Status

Accepted. Implemented in `src/runtime/execution_backend.cpp`
(`HostExecutionBackend`) and measured in `e1cipher experiment accelerator`.

## Context

CKKS's cost is not evenly distributed across its sub-operations.
Measured on this host (N8192, `e1cipher experiment accelerator`):

| op | % of measured runtime | structural judgment |
|---|---|---|
| rescale | 25.6% | HIGH VALUE — regular, high arithmetic intensity, high data movement |
| encrypt | 28.8% | MEDIUM VALUE — includes RLWE error sampling, less uniformly regular |
| relinearize | 20.8% | HIGH VALUE — same NTT/modmul shape as rescale |
| rotate | 18.3% | HIGH VALUE — same shape via Galois automorphism |
| encode / add / multiply / decrypt | <4% each | LOW VALUE |

`encrypt` measures the single highest individual share, but it is NOT
the accelerator candidate this ADR picks — see the "why not encrypt"
note below. Relinearize, rescale, and rotate together are 64.6% of
measured runtime and share one underlying shape: RNS decomposition →
forward NTT → a fixed pattern of modular multiply-adds against a public
key matrix → inverse NTT → recomposition.

## Decision

Treat the NTT (and, more narrowly, the key-switching family it's part
of) as the primary accelerator candidate, not CKKS as a whole and not
whichever operation happens to measure the highest raw percentage.

## Why not just pick the highest % share (encrypt)

Encrypt's cost is dominated by RLWE noise sampling, which is less
amenable to the "fixed, regular, data-parallel" argument this project is
making for spatial dataflow. Picking it anyway because it measured
highest would be optimizing the headline number instead of the
architectural argument — see the explicit guard against this in
`apps/experiments/accelerator_opportunity.cpp`.

## Consequences

`HostExecutionBackend::submit_kernel(KernelId::Ntt, ...)` runs the real
reference NTT kernel (`crypto::kernels::ntt`) today, on host, as the
proxy this ADR argues for. It is explicitly NOT a claim that E1
accelerates it — `platform/e1/e1_platform.hpp` and
`E1ExecutionBackend` both fail clearly rather than fabricate a result.
See `docs/e1-hardware-validation-plan.md` for what would actually need
to be measured on real hardware to test this ADR's claim.
