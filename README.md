# E1-CIPHER

**Confidential physical AI for Efficient Computer's E1.**

*An open reference implementation for secure autonomous systems, written
in C/C++, designed to make E1 hardware validation straightforward.*

Performance-critical and hardware-facing code is implemented in C/C++.
TypeScript/Node remains only for development tooling (security/CRA
evidence generation) and an optional browser dashboard — see
[Architecture](#architecture) and `docs/architecture-assessment.md` for
exactly what moved and why.

## The problem

Autonomous machines — this project's reference application is a drone
fleet inspecting critical infrastructure — increasingly need to process
sensitive physical-world information locally: imagery, precise location,
structural-health signals. They need to do it under a tight energy
budget, over bandwidth that's often poor, and without handing raw
observations to whoever operates the central system that aggregates
fleet-wide intelligence. "Trust the cloud operator" doesn't address a
compromised-cloud or malicious-operator threat model (`docs/threat-model.md`,
threats T1–T2). Homomorphic encryption — CKKS specifically, an
approximate-arithmetic scheme suited to real-valued sensor telemetry — is
the cryptographic answer that's existed for years but has stayed out of
reach at the battery-powered edge because of its computational and
bandwidth cost.

**This repository does not oversell homomorphic encryption.** A single
drone's CKKS-encrypted feature vector is measured at **~2,733x** the size
of its plaintext equivalent (see [Results](#results)) — that number is
real, reproduced independently in this C++ implementation after first
appearing in this project's TypeScript-era prototype, and it is the whole
reason "just encrypt everything" is not a serious engineering proposal.
The actual contribution here is the system built around that constraint:
a policy engine that encrypts only what needs cryptographic
confidentiality, and a measured argument for where a different compute
architecture could change the equation.

## The application

**E1 Secure Infrastructure Inspection** — a fleet of autonomous drones
(camera, LiDAR, IMU, GPS, vibration, battery telemetry) inspects
infrastructure. Each drone must decide, locally and immediately, what to
do with what it just sensed:

```
SENSOR INGESTION → FEATURE EXTRACTION → DATA CLASSIFICATION →
LOCAL COMPUTATION → SECURITY POLICY → OPTIONAL ENCRYPTION →
SECURE TELEMETRY → FLEET AGGREGATION → AUDIT/EVIDENCE
```

Run it:

```sh
./build/apps/e1cipher inspect --drones 20
```

This executes the real pipeline (telemetry generation → sensor fusion →
policy classification → CKKS encryption → homomorphic fleet aggregation)
and prints a narrated report of what actually happened — see
[Build](#build) to produce `./build/apps/e1cipher`, and `docs/demo-script.md`
for a guided walkthrough.

**This project does not claim E1 already runs this workload.** It is a
high-quality C/C++ reference implementation and a clean hardware
abstraction layer (`platform/e1/`) so the same workload can be compiled
and validated against E1 the day hardware/toolchain access exists. No E1
performance, power, or architectural number is fabricated anywhere in
this repository — see [Limitations](#limitations).

## Research question

> Which portions of a confidential autonomous-system workload should
> execute locally, which should be encrypted, and which are
> architecturally suitable for acceleration on Efficient Computer's E1?

This is deliberately more actionable than "can CKKS run on E1" — it
forces a decomposition (local/encrypted/accelerate) that maps directly
onto an application, a compiler, and an architecture decision, rather
than treating the whole workload as one undifferentiated question.

## Why E1

The measured bottleneck (`docs/bottleneck-report.md`,
`docs/e1-application-mapping.md`) is specific: CKKS's cost concentrates in
key-switching operations (rescale/relinearize/rotate), which decompose
into RNS decomposition → forward NTT → a fixed pattern of modular
multiply-adds against a public key matrix → inverse NTT → recomposition.
Every stage is fixed-size, branch-free, and dominated by moving
coefficient arrays through regular arithmetic rather than by control
flow — the shape a spatial-dataflow architecture is supposed to help
with. **Whether Electron E1 actually delivers that advantage is the open
question this repository sets up and cannot answer without hardware** —
`platform/e1/e1_platform.hpp` is a clean interface, not a simulation
dressed up as a result.

## Architecture

```
src/telemetry/     deterministic synthetic drone-fleet telemetry (seeded)
src/fusion/         TelemetrySample -> FeatureVector (raw sensors never leave)
src/policy/         data classification + fail-closed adaptive scheduler
src/crypto/         CryptoBackend interface: Plaintext | CKKS (real SEAL) | Mock
src/crypto/kernels/ from-scratch modular-arithmetic + NTT reference kernels
src/diagnostics/    audit events, platform report, security gate
platform/host/      real host introspection (CPU, compiler, git commit)
platform/e1/        E1 hardware abstraction -- always NOT_MEASURED, never fabricated
apps/drone_inspection/  the flagship CLI application
apps/fleet_gateway/     plaintext-vs-CKKS fleet aggregation; gateway holds no secret key
apps/cli/               `e1cipher` -- one binary, subcommands dispatch into the above
benchmarks/crypto/      CKKS operation benchmark suite (real SEAL, this host)
benchmarks/kernels/     modular-multiply / NTT micro-benchmarks
tests/unit/, tests/security/   CTest + doctest -- correctness AND fail-closed behavior
```

Strongly-typed data model throughout (`include/e1cipher/`) —
`TelemetrySample`, `FeatureVector`, `EncryptedVector`, `PlacementDecision`,
`AuditEvent` — not generic JSON blobs; memory layout, ownership, and
copy/move behavior are documented and `static_assert`-enforced where it
matters (see `docs/application-engineering-checklist.md` "Memory").

**What moved from the TypeScript-era prototype, and why:** see
`docs/architecture-assessment.md` for the full file-by-file assessment —
written *before* this rewrite, as the project brief required. Summary:
the computational core (telemetry, fusion, policy, crypto, benchmarks,
applications) moved to C++20/CMake against real Microsoft SEAL. The
security/CRA evidence generator and the browser dashboard remain
TypeScript/Node, per the project's own thesis that tooling and
visualization may stay there — not because porting them was out of scope,
but because they were never the computational core.

## Security

- **Key custody is structural, not policy.** `apps/fleet_gateway/fleet.cpp`'s
  gateway role never constructs a `SecretKey` or `Decryptor` — reading
  the code is the proof, not a comment claiming it.
- **Fail-closed, tested.** Unknown data classification, a missing crypto
  backend for CONFIDENTIAL data, and LOCAL_ONLY data under
  best-case conditions all resolve to `Blocked`/no-transmit —
  `tests/security/test_fail_closed.cpp`, enforced, not aspirational.
- **Threat model**: `docs/threat-model.md` — 10 threats, each marked
  MITIGATED / PARTIALLY MITIGATED / NOT MITIGATED / OUT OF SCOPE. Never
  implies protection that doesn't exist.
- **Cryptographic agility**: `include/e1cipher/crypto/backend.hpp`'s
  `CryptoBackend` interface, three implementations (`Plaintext`, `Ckks`,
  `Mock` — the last explicitly NOT cryptography, documented as such).

## Performance

Real `std::chrono` measurements, warm-up + 20–30 trials, on this host —
every number is `HOST_REFERENCE`, never `REAL_HARDWARE` (see
`include/e1cipher/platform/result_class.hpp`). Full tables:
`docs/bottleneck-report.md`, `docs/e1-application-mapping.md`,
`results/cpp-ckks-bench-latest.json`.

- Key-switching ops (rescale/relinearize/rotate) are 60–73% of per-op
  latency and *grow* with N — the central bottleneck finding.
- Native C++/SEAL is measurably faster than the TS-era WASM build at
  every parameter set (e.g. N16384 rescale: ~15.0ms here vs ~34.8ms via
  node-seal/WASM — roughly 2.3x) — a real, checkable difference between
  the two implementations.
- Montgomery multiplication is **3–4x slower** than naive 128-bit
  hardware division, measured natively in C++ — confirming this was a
  genuine hardware-divide finding, not a JS-VM artifact from the
  TypeScript-era kernel benchmark. Reported because it's true, not
  because it's flattering.

## Compliance

**"CRA-aware engineering evidence," never "CRA compliant."** See
`docs/compliance/cra-mapping.md` for what the Cyber Resilience Act is,
what's relevant to a product built on this code, and — explicitly —
which controls remain unimplemented. `security-controls.yaml` is a
machine-readable control-to-test-to-evidence trace (10 controls, each
pointing at a real test file and a real way to reproduce its evidence).
Run `./build/apps/e1cipher security` for the live gate.

## Build

Requires CMake 3.20+ and a C++20 compiler. Verified on a machine with
**no Visual Studio installed** — Clang 22 + Ninja, SEAL fetched and built
from source via CMake `FetchContent` (see `docs/architecture-assessment.md`
for the toolchain verification done before any application code was
written, including the CRT-linkage and `__int128`-division wrinkles that
came up and how they were fixed, not routed around).

```sh
cmake -G Ninja -B build .          # add -DCMAKE_CXX_COMPILER=clang++ if needed
cmake --build build -j
ctest --test-dir build --output-on-failure

./build/apps/e1cipher inspect --drones 20
./build/apps/e1cipher fleet
./build/apps/e1cipher platform
./build/apps/e1cipher security
./build/benchmarks/ckks_bench
./build/benchmarks/kernel_bench
```

Sanitizer build (verified clean — all 4 test binaries and every CLI
subcommand, no ASan/UBSan findings):

```sh
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DE1CIPHER_SANITIZER=address-undefined -B build-asan .
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

E1 target (fails clearly, does not silently fall back to a host build):

```sh
cmake -DTARGET_E1=ON -B build-e1 .   # -- fails: "E1 toolchain not found." (no effcc on PATH)
```

Node tooling (security/CRA evidence, dashboard — unchanged from the
TypeScript-era prototype):

```sh
npm install
npm run security:sbom && npm run security:manifest && npm run security:evidence
cd apps/dashboard && npm install && npm run dev
```

## Results

**CKKS op latency, mean of 30 trials (HOST_REFERENCE, C++/real SEAL, this repo's dev machine):**

| Op | N1024 | N2048 | N4096 | N8192 | N16384 |
|---|---|---|---|---|---|
| encrypt | 0.17 ms | 0.61 ms | 1.23 ms | 3.12 ms | 10.14 ms |
| rescale | N/A | 0.36 ms | 0.77 ms | 2.74 ms | 14.97 ms |
| relinearize | N/A | 0.29 ms | 0.62 ms | 2.25 ms | 12.66 ms |
| rotate | N/A | 0.25 ms | 0.54 ms | 1.95 ms | 11.64 ms |

N1024 is a single-modulus chain with no key-switching support at 128-bit
security — reported as `N/A` by construction, not a crash.

**Fleet aggregation, plaintext vs. CKKS (`apps/fleet_gateway/fleet.cpp`, N4096):**

| Fleet size | Plaintext bytes | CKKS bytes | Bandwidth ratio | Mean accuracy (max abs err) |
|---|---|---|---|---|
| 1 | 48 B | 131,185 B | 2,733x | ~2–4e-5 |
| 50 | 2,400 B | 6,559,250 B | 2,733x | ~1e-5 |

CKKS's win is confidentiality of the aggregator's view, not bandwidth —
the ratio is flat because ciphertext size per drone doesn't depend on
fleet size. Reported plainly; see [The application](#the-application).

**Kernel micro-benchmark** (`benchmarks/kernels/kernel_bench.cpp`):
Montgomery multiplication measured 3–4x slower than naive
`unsigned __int128 %` at every N tested, natively, on this host's AMD
Zen 2 divide unit — see [Performance](#performance).

Raw artifacts: `results/cpp-ckks-bench-latest.json` and the TS-era
`results/*.json` (kept as the baseline findings these results were
checked against, not superseded wholesale — see
`docs/architecture-assessment.md`).

## Limitations

- **No E1 hardware or effcc toolchain is available in this environment.**
  Every number in this repository is `HOST_REFERENCE` on ordinary x86_64
  silicon. `platform/e1/e1_platform.hpp` always returns `NOT_MEASURED`;
  `cmake/FindE1Toolchain.cmake` fails `-DTARGET_E1=ON` outright rather
  than silently building a host binary under an E1 label.
- **No power/energy instrumentation is available.** No energy number
  appears anywhere in this repository.
- **No thread-scaling or SIMD-path benchmarking** has been done yet — see
  `docs/application-engineering-checklist.md`'s unchecked Performance
  items.
- **The C++ SBOM/dependency-vulnerability-scan gap**: the Node tooling
  scans its own dependency tree; SEAL/msgsl/doctest (fetched via CMake)
  are not yet scanned by any tool here — see
  `docs/compliance/cra-mapping.md`.
- **No authenticated/signed ciphertexts** — replay and tamper protection
  are explicitly NOT MITIGATED (`docs/threat-model.md` T4/T6).
- **CI is configured but not run in this environment** — `.github/workflows/ci.yml`
  targets Linux runners (this environment has no Visual Studio and no
  GitHub Actions runner access); it has not actually executed on a real
  runner as of this writing.

## Hardware validation

This project is structured so that, given E1 hardware and the effcc
toolchain:

```sh
cmake -DTARGET_E1=ON -B build-e1 .
cmake --build build-e1 -j
ctest --test-dir build-e1
./build-e1/benchmarks/ckks_bench
```

would run the same benchmark suite against the real target, producing
`REAL_HARDWARE`-labeled results directly comparable to the
`HOST_REFERENCE` numbers above — without redesigning the application,
the data model, or the policy engine. That is the point of
`platform/e1/` and `cmake/FindE1Toolchain.cmake` existing now, before
hardware access exists.

## Further reading

- `docs/architecture-assessment.md` — the pre-rewrite assessment this
  project's brief required, including the toolchain verification done
  before any C++ was written.
- `docs/bottleneck-report.md`, `docs/e1-application-mapping.md` — the
  measured bottleneck and its architectural decomposition, stage by stage.
- `docs/threat-model.md`, `docs/compliance/cra-mapping.md`,
  `security-controls.yaml` — security and compliance evidence.
- `docs/application-engineering-checklist.md` — a self-assessment against
  what an applications engineer would actually check.
- `docs/demo-script.md` — the 5-minute walkthrough this README's
  structure is drawn from.
