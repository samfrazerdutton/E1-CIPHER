# What I Would Build on E1 Hardware

A concrete plan, not a wishlist — every section below says what would
actually be measured and what would count as the hypothesis failing.
Nothing here is a result; every number under "Host baseline" is real
(`e1cipher experiment accelerator`, this repo); every number under
"Candidate E1 measurement" is `NOT MEASURED`, by design.

## Hypothesis

CKKS's key-switching family (relinearize, rescale, rotate — each built
from RNS decomposition → forward NTT → fixed modular multiply-adds
against a public key matrix → inverse NTT → recomposition) contains
sufficient regular parallelism and data reuse to justify spatial
execution on Electron E1, compared to this host's general-purpose CPU
execution of the same algorithm via Microsoft SEAL.

## Workload

The key-switching family measured in `e1cipher experiment accelerator`
and characterized structurally in `docs/adr/002-ntt-as-accelerator-candidate.md`
— specifically `relinearize`, `rescale`, and `rotate` at CKKS parameter
set N8192 (`include/e1cipher/crypto/ckks_params.hpp`).

## Kernel

The NTT forward/inverse transform (`crypto::kernels::ntt`,
`src/crypto/kernels/ntt.cpp`) is the specific, standalone, benchmarkable
unit this plan targets — not all of CKKS, and not SEAL's internal NTT
(which is not separately callable; see `docs/bottleneck-report.md`).

## Input size

N ∈ {1024, 2048, 4096, 8192, 16384} — the same five parameter sets this
repository already benchmarks, so a future E1 run is directly comparable
row-for-row against `results/cpp-ckks-bench-latest.json` and
`results/kernel-bench-latest.json` (the earlier TypeScript/node-seal run,
kept as a cross-check).

## Memory layout

Coefficient arrays of `std::uint64_t`, length N, mod `kNttPrime =
998244353` (see `crypto/kernels/ntt.hpp`). This is a reference layout
chosen for kernel-level benchmarking — not SEAL's actual RNS limb layout,
which uses the CKKS parameter set's own moduli, not this fixed prime. Any
E1 port should state explicitly which layout it measures.

## Expected parallelism

Within each of the log₂(N) butterfly stages, up to N/2 independent
butterfly operations; stages themselves are sequential (each depends on
the previous stage's full output). This is the structural claim ADR-002
is built on — it has not been tested against E1's actual execution model.

## Host baseline (measured, this repository, `e1cipher experiment accelerator`, N8192)

| op | mean latency |
|---|---|
| rescale | 2.75 ms |
| relinearize | 2.24 ms |
| rotate | 1.97 ms |
| **key-switching family, combined** | **64.6% of measured CKKS runtime** |

Full per-N table: `results/cpp-ckks-bench-latest.json`.

## Candidate E1 measurement

**NOT MEASURED.** No E1 hardware or effcc toolchain is available in this
environment. `platform::E1Platform::run_kernel` and
`runtime::E1ExecutionBackend::submit_kernel` both throw/return
`NOT_MEASURED` unconditionally — see `include/e1cipher/platform/e1_platform.hpp`,
`src/runtime/execution_backend.cpp`.

## Required E1 instrumentation

To actually run this plan: the effcc toolchain (`cmake -DTARGET_E1=ON`,
`cmake/FindE1Toolchain.cmake`), a way to time kernel execution on-device
(wall-clock at minimum; a hardware performance-counter interface if one
exists), and — to test H1 as originally framed — a way to measure energy
per kernel invocation, which this project has never had access to on any
platform (see README "Limitations").

## Success criterion

A real `E1ExecutionBackend::submit_kernel(KernelId::Ntt, ...)` showing a
statistically significant (e.g. >20%, averaged over ≥20 trials, same
warm-up discipline as `benchmarks/kernels/kernel_bench.cpp`) latency *or*
energy reduction versus this host's measured NTT baseline, at the same N.

## Failure criterion

No significant reduction, or a regression — in which case the honest
conclusion is that this specific workload's NTT does not benefit from
E1's architecture as implemented, and `docs/adr/002-ntt-as-accelerator-candidate.md`
should be revisited rather than the result hidden. Per this project's
rule: if a benchmark loses, show it.
