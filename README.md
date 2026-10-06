# E1-CIPHER

## Confidential Edge Compute Runtime

A C++ reference architecture for deciding where sensitive physical-AI
workloads should execute: **locally, remotely, over encrypted data, or
on a spatial accelerator candidate.**

> A runtime that determines which portions of a physical-AI workload
> should execute locally, which should execute over encrypted data, and
> which cryptographic kernels are candidates for spatial acceleration.

Cryptography (CKKS, via real Microsoft SEAL) is **one execution backend**
this runtime can route an operation to — not the whole application. The
flagship reference workload is autonomous infrastructure inspection, but
the same runtime also drives a robotics/physical-AI workload and a
confidential-edge-AI workload (`include/e1cipher/runtime/workload.hpp`)
— demonstrating a reusable abstraction, not three separate demos.

## Problem

Autonomous machines inspecting infrastructure, navigating environments,
or running local inference need to process sensitive physical-world data
under three constraints that rarely get solved together: a tight energy
budget, limited bandwidth, and a genuine need to keep observations
confidential from whoever operates the central system that aggregates
fleet-wide intelligence. "Trust the cloud operator" doesn't address a
compromised-cloud threat model (`docs/threat-model.md`). Homomorphic
encryption (CKKS) is the cryptographic answer — but it is measurably
expensive, and "encrypt everything" is not a serious engineering
proposal (see Results).

## Architecture

```sh
APPLICATION  ->  RUNTIME  ->  POLICY/SECURITY  ->  COST MODEL + PLACEMENT
                                                          |
                              LOCAL | ENCRYPTED | ACCELERATED | REMOTE | BLOCKED
```

Full diagrams (application→runtime→placement, and the CKKS crypto path
stage-by-stage): `docs/architecture-diagram.md`. Design rationale, as
four ADRs: `docs/adr/001-selective-confidential-execution.md`,
`002-ntt-as-accelerator-candidate.md`, `003-feature-level-encryption.md`,
`004-hardware-agnostic-runtime.md`. Why this moved from TypeScript to
C++20/CMake, and what carried over: `docs/architecture-assessment.md`.

```
include/e1cipher/runtime/   Operation, Placement, CostModel, ExecutionBackend, Runtime
include/e1cipher/crypto/    CryptoBackend (Plaintext | CKKS, real SEAL | Mock), kernels (NTT, modular)
include/e1cipher/{telemetry,fusion,policy,diagnostics,platform}/   data model + legacy drone-specific policy
apps/runtime_demo/          the flagship `e1cipher runtime-demo`
apps/experiments/           selective execution, accelerator opportunity, scaling
apps/{drone_inspection,fleet_gateway}/   the original reference application + fleet aggregation
benchmarks/{crypto,kernels}/   real SEAL + reference-kernel benchmarks
tests/{unit,security}/      CTest + doctest, including fail-closed regression tests
```

## Demo

```sh
cmake -G Ninja -B build .
cmake --build build -j
./build/apps/e1cipher runtime-demo
```

See `docs/recruiter-demo.md` for the 60-second walkthrough and
`docs/demo-script.md` for a longer, narrated version.

## Results

**The central experiment** (`e1cipher experiment selective` —
`docs/adr/001-selective-confidential-execution.md`): the same workload,
three ways.

| Mode | Latency | Bandwidth | Confidential bytes exposed |
|---|---|---|---|
| A: Everything plaintext | 0.15 ms | 184 B | 160 B |
| B: Everything encrypted | 8.80 ms | 655,925 B | 0 B |
| C: Selective (this runtime) | 1.77 ms | 393,563 B | 0 B |

Mode C matches Mode B's zero exposure at 60% of its bandwidth and a
fraction of its latency, by routing only the operations that actually
need confidentiality through CKKS/the accelerator candidate, and letting
public/restricted data skip encryption entirely. That gap is the whole
thesis — not a crypto optimization, a placement one.

**Where CKKS's cost actually goes** (`e1cipher experiment accelerator`,
N8192): relinearize + rescale + rotate (the key-switching family) are
**64.6%** of measured per-operation latency — more than encrypt, add,
multiply, encode, and decode combined. See
`docs/adr/002-ntt-as-accelerator-candidate.md` for why this, and not
whichever op measures highest individually (encrypt does, at 28.8% —
deliberately not picked, see that ADR), is the accelerator candidate.

**The honest cost of confidentiality**: a 48-byte feature vector becomes
a 131,185-byte ciphertext at N4096 — **2,733x** expansion. Reproduced
independently in this C++ implementation after first appearing in this
project's TypeScript-era prototype (`docs/architecture-assessment.md`).
This number is not hidden; it's the reason selective placement exists.

**Scaling** (`e1cipher experiment scaling`, 1/10/100/1000 devices):
full-CKKS bandwidth and encryption workload scale linearly with device
count *and* with how many fields per device are encrypted; selective
placement halves both in this workload without giving up confidentiality
for the field that needs it.

Every number above is `HOST_REFERENCE` — measured on ordinary x86_64
silicon via real Microsoft SEAL — never `REAL_HARDWARE`. See
`include/e1cipher/platform/result_class.hpp`.

## E1 hardware hypothesis

> CKKS's key-switching family contains sufficient regular parallelism
> and data reuse to justify spatial execution on Electron E1, compared
> to this host's general-purpose CPU execution via Microsoft SEAL.

Full plan — workload, kernel, input size, memory layout, required
instrumentation, success/failure criteria: `docs/e1-hardware-validation-plan.md`.
**Not measured.** No E1 hardware or effcc toolchain is available in this
environment. `include/e1cipher/runtime/execution_backend.hpp`'s
`E1ExecutionBackend` fails clearly (throws) rather than silently
executing on host under an E1 label; `cmake/FindE1Toolchain.cmake` does
the same at configure time (`-DTARGET_E1=ON` without `effcc` on `PATH`
is a hard configuration error).

## Security

Fail-closed, tested: unknown classification, no available crypto
backend for CONFIDENTIAL+ data, and SECRET data under any
battery/network/priority conditions all resolve to `Blocked`/`Local`
respectively — never a permissive default
(`tests/security/test_fail_closed.cpp`, `tests/unit/test_runtime.cpp`).
Key custody is structural: `apps/fleet_gateway/fleet.cpp`'s gateway role
never constructs a `SecretKey`/`Decryptor`. Full threat register (10
threats, each MITIGATED/PARTIALLY MITIGATED/NOT MITIGATED/OUT OF SCOPE):
`docs/threat-model.md`. CRA-readiness evidence (never "CRA compliant"):
`docs/compliance/cra-mapping.md`, `security-controls.yaml`.

## Build

Requires CMake 3.20+, a C++20 compiler. Verified with **no Visual Studio
installed** — Clang 22 + Ninja, Microsoft SEAL fetched and built from
source via CMake `FetchContent` (see `docs/architecture-assessment.md`
for the toolchain issues hit and fixed, not routed around, including a
genuine Windows-macro name collision — `DeviceCapabilities` vs.
`winspool.h`'s `DeviceCapabilities`/`DeviceCapabilitiesA` — caught by a
real linker error during this session, not invented for this README).

```sh
cmake -G Ninja -B build .          # add -DCMAKE_CXX_COMPILER=clang++ if needed
cmake --build build -j

./build/apps/e1cipher runtime-demo
./build/apps/e1cipher experiment selective
./build/apps/e1cipher experiment accelerator
./build/apps/e1cipher experiment scaling
./build/apps/e1cipher inspect --drones 20
./build/apps/e1cipher fleet
./build/apps/e1cipher platform
./build/apps/e1cipher security
./build/benchmarks/ckks_bench
./build/benchmarks/kernel_bench
```

E1 target (fails clearly, never silently falls back to host):

```sh
cmake -DTARGET_E1=ON -B build-e1 .   # -- fails: "E1 toolchain not found." (no effcc on PATH)
```

## Tests

```sh
ctest --test-dir build --output-on-failure
```

6 suites (kernels, policy, crypto round-trip, fail-closed security,
runtime placement, execution backend), all passing in Release and under
AddressSanitizer+UndefinedBehaviorSanitizer:

```sh
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DE1CIPHER_SANITIZER=address-undefined -B build-asan .
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

## Benchmark methodology

Real `std::chrono::steady_clock` measurements, warm-up + 15-30 trials,
median/mean/stddev reported, on this host. Every number is tagged with
its real provenance — `HOST_REFERENCE`, `ESTIMATED` (formula-driven,
e.g. network bandwidth-time), `SIMULATED` (none exist in this repo — no
verified E1 specs to build a cost model from), or `NOT_MEASURED` — see
`include/e1cipher/platform/result_class.hpp` and
`include/e1cipher/runtime/cost_model.hpp`. CPU, compiler, compiler
version, OS, and git commit are embedded in every benchmark artifact
(`src/platform/host/host_platform.cpp`, `CMakeLists.txt`'s
`E1CIPHER_GIT_COMMIT`). Raw artifacts: `results/cpp-*.json`.

## Limitations

- No E1 hardware or effcc toolchain is available — see "E1 hardware
  hypothesis" above.
- No power/energy instrumentation — no energy number appears anywhere
  in this repository.
- No C++-side SBOM/dependency-vulnerability scan — the Node tooling
  layer (`tools/security/generate-evidence.ts`) scans its own
  dependency tree only; SEAL/msgsl/doctest are unscanned
  (`docs/compliance/cra-mapping.md`).
- No thread-scaling or SIMD-path benchmarking.
- CI (`.github/workflows/ci.yml`) targets Linux runners and is written,
  but this environment has no GitHub Actions runner access to actually
  execute it — clang-format, the C++ build, and the test suite have all
  been verified locally (the commands above), not on a hosted runner.
- `runtime::ExecutionBackend`'s accelerator path measures a real NTT
  kernel's shape on host, as a proxy — it is not, and does not claim to
  be, an E1 measurement or a speedup claim.

## Further reading

- `docs/architecture-assessment.md` — the pre-rewrite audit this
  project's brief required.
- `docs/architecture-diagram.md`, `docs/technical-deep-dive.md` — how
  the pieces fit together and why each stage costs what it does.
- `docs/adr/` — four architecture decision records.
- `docs/e1-hardware-validation-plan.md` — the falsifiable E1 hypothesis.
- `docs/threat-model.md`, `docs/compliance/cra-mapping.md`,
  `security-controls.yaml` — security and compliance evidence.
- `docs/application-engineering-checklist.md` — a self-assessment.
- `docs/recruiter-demo.md`, `docs/demo-script.md` — guided walkthroughs.
