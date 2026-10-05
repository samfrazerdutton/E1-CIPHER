# E1-CIPHER — Confidential Physical AI Fabric

*Compute on sensitive physical-world intelligence without exposing the data.*

A reproducible research platform investigating whether a spatial-dataflow,
general-purpose edge processor — specifically Efficient Computer's Electron
E1 — could make privacy-preserving physical-AI systems (the flagship
scenario: autonomous drone fleets inspecting infrastructure) practical at
the energy-constrained edge.

**This is a single research session's slice of a much larger brief.** It
deliberately prioritizes the scientifically defensible core — a real CKKS
benchmark suite, a measured bottleneck decomposition, a policy-driven
crypto-placement engine, and a real plaintext-vs-encrypted fleet-aggregation
comparison — over a polished UI. The browser-based 3D fleet simulator, the
full 15-document CRA evidence set, and effcc/E1 compiler integration are
**not built** in this slice; they are explicitly out of scope here and
listed under [Not built this session](#not-built-this-session). Nothing
below is a mockup — every number comes from code in this repository that
you can re-run yourself (see [Reproduction](#reproduction)).

## Research Question

> Can a spatial-dataflow general-purpose processor make privacy-preserving
> physical-AI workloads practical at the energy-constrained edge?

### Hypotheses (status reflects what this session's artifacts actually test)

- **H1** — Spatial execution reduces data-movement cost in CKKS-related
  workloads.
  **Status: NOT TESTED.** No E1 hardware or effcc toolchain is available
  (see [Limitations](#limitations)). This repo instead produces the
  prerequisite for testing H1: a measured host bottleneck decomposition
  (`docs/bottleneck-report.md`) showing that rescale/relinearize/rotate —
  all key-switching operations with a regular, data-movement-heavy,
  NTT-butterfly shape — dominate CKKS latency (60–73% of per-op time,
  growing with N). That is the workload H1 would need to be tested against
  on real E1 hardware.
- **H2** — Whole-application execution provides a larger advantage than
  accelerating only the AI model.
  **Status: NOT TESTED on E1.** `apps/fleet/compare.ts` does measure the
  *whole* pipeline (sensor fusion → encode → encrypt → aggregate → decrypt)
  rather than a single kernel, which is the methodology H2 requires — but
  without E1 hardware there is no "whole app on E1 vs. accelerated-kernel
  on E1" comparison to report.
- **H3** — Adaptive encryption policies reduce energy and latency while
  preserving confidentiality for sensitive data.
  **Status: PARTIALLY TESTED (logic only, not energy).** `lib/policy/scheduler.ts`
  implements and explains adaptive placement decisions (battery/network/
  latency/mission-priority-aware), and its decision logic is deterministic
  and unit-testable. Energy was **not measured** (no power-measurement
  instrumentation in this environment) — see
  [Energy Analysis](#energy-analysis).
- **H4** — Encrypted edge aggregation can reduce raw-data transmission from
  autonomous systems.
  **Status: TESTED, with a nuance the brief explicitly asked for us not to
  hide.** `apps/fleet/compare.ts` shows CKKS aggregation keeps raw per-drone
  readings out of the aggregator's hands entirely — a genuine confidentiality
  win. It does **not** reduce *upload* bandwidth: a single drone's CKKS
  ciphertext is ~2,700x larger than its plaintext feature vector at the
  N4096 parameter set (see [Results](#results)). H4 is true for *what the
  aggregator learns*, false for *bytes on the wire*, and this project reports
  both rather than only the flattering half.

## Problem

Autonomous physical-AI systems (drone fleets inspecting infrastructure,
industrial sensor networks, mobile robotics) generate continuous,
sensitive, physical-world data — imagery, location, structural-health
signals — under three simultaneous constraints that rarely get solved
together: a tight energy budget, bandwidth that is often poor or metered,
and a genuine need to keep raw observations confidential from whoever
operates the central aggregation system. Today's answer is usually "send
less data" (lossy, loses information) or "trust the cloud operator"
(doesn't address a compromised-cloud or malicious-operator threat model —
see `docs/threat-model.md`). Homomorphic encryption (specifically CKKS, an
approximate-arithmetic scheme suited to real-valued sensor telemetry) is a
third option that has existed for years but has mostly stayed out of reach
at the battery-powered edge because of its compute and memory-movement
cost.

## Why Edge Confidential Computing?

Pushing confidentiality to the edge (encrypt before transmission, never
decrypt centrally) means a compromised or malicious central aggregator
simply cannot see raw sensor data, regardless of network security — see
threats T1–T3 in `docs/threat-model.md`. The alternative (TLS to a trusted
cloud) only protects data in transit, not from the operator of that cloud.

## Why CKKS?

CKKS is a leveled homomorphic encryption scheme over approximate real
numbers — a good match for physical sensor telemetry (floats with natural
noise tolerance), unlike exact-integer schemes (BFV/BGV) that are a better
fit for things like database queries. It supports addition and
multiplication directly on ciphertexts and SIMD-style batching across
thousands of "slots" in one ciphertext. **CKKS is not the product here** —
it is one cryptographic backend behind a swappable abstraction
(`lib/ckks/backend.ts`; see [Cryptographic agility](#cryptographic-agility)),
chosen because it is the best-understood approach to the actual problem:
confidential physical-AI telemetry aggregation.

## Why Drones?

A drone fleet is a clean, concrete instance of the general physical-AI
problem: battery-constrained, bandwidth-constrained, operating sensitive
physical-world sensors, and benefiting from fleet-level aggregate
intelligence that no single drone can compute alone. `lib/telemetry` and
`lib/sensor_fusion` implement a deterministic synthetic drone-fleet
telemetry generator (no paid APIs, fully offline, reproducible from a
seed) standing in for a real sensor stack.

## Why Efficient?

The measured bottleneck (`docs/bottleneck-report.md`) shows CKKS's cost is
concentrated in key-switching operations (rescale/relinearize/rotate) with
a specific, regular shape: RNS-limb decomposition → forward NTT → a fixed
pattern of modular multiply-adds → inverse NTT → recomposition. That shape
— fixed, branch-free, high-arithmetic-intensity, dominated by moving
coefficient arrays through butterfly stages rather than by control flow —
is exactly the kind of workload a spatial-dataflow architecture targets.
Section [E1 Mapping](#e1-mapping) below details the correspondence. Whether
Electron E1 actually delivers an advantage on this workload is an open,
testable question this repository sets up but — without hardware access —
cannot yet answer (H1, above).

## Architecture

```
sensor telemetry (lib/telemetry)
        │  deterministic synthetic drone fleet, offline, seeded
        ▼
sensor fusion (lib/sensor_fusion)
        │  reduces a telemetry sample to a 6-scalar feature vector;
        │  raw IMU/camera data never leaves this stage
        ▼
policy engine (lib/policy)
        │  classifies data (LOCAL_ONLY / PLAINTEXT / ENCRYPTED /
        │  AGGREGATABLE / BLOCKED) and picks a compute placement
        │  (LOCAL / EDGE / CLOUD × PLAINTEXT / ENCRYPTED), with an
        │  explanation trail — see lib/policy/scheduler.ts
        ▼
crypto backend (lib/ckks)
        │  CryptoBackend abstraction over CKKS (real, via node-seal/
        │  Microsoft SEAL), Plaintext (zero-crypto baseline), and
        │  MockEncrypted (non-cryptographic dev placeholder)
        ▼
fleet aggregation (apps/fleet)
        │  N-drone homomorphic sum vs. plaintext sum, measured
        ▼
security evidence (lib/security, tools/security)
           machine-readable manifest + SBOM generated from the
           repository's actual state (deps, params, git commit)
```

Supporting: `lib/kernels` (from-scratch modular-arithmetic and NTT
reference kernels for architecture-level reasoning, independent of SEAL's
internals) and `lib/platform` (host-info capture + an explicitly-stubbed
E1 hardware abstraction layer).

## Threat Model

See `docs/threat-model.md` for the full threat register (10 threats, what
is and is not addressed, and why). Summary: this prototype's
confidentiality guarantee is "the fleet aggregator never sees a raw,
individual drone reading for ENCRYPTED/AGGREGATABLE data classes." It does
**not** provide message authenticity, replay protection, aggregate-poisoning
robustness, firmware integrity, or side-channel hardening — those are
explicitly out of scope and listed as unaddressed threats, not silently
ignored.

## Security Model

- **Key custody:** drones hold only the CKKS public key (can encrypt, can't
  decrypt); the fleet operator holds the secret key. Modeled directly in
  `lib/ckks/ckksBackend.ts`'s separate `Encryptor`/`Decryptor` construction.
- **Data classification:** static baseline table in
  `lib/policy/classification.ts` (camera frames LOCAL_ONLY, object
  embeddings ENCRYPTED, fleet aggregates AGGREGATABLE, etc.), adjusted at
  runtime by `lib/policy/scheduler.ts` for battery/network/latency/mission
  state — every decision carries a human-readable explanation trail (see
  `PlacementDecision.explanation`).
- **Cryptographic agility:** the `CryptoBackend` interface
  (`lib/ckks/backend.ts`) is implemented three ways — `createCkksBackend`
  (real CKKS), `createPlaintextBackend` (explicit zero-crypto baseline),
  `createMockEncryptedBackend` (NOT cryptography — a structurally
  ciphertext-shaped placeholder for development, documented as unsafe to
  use for any reported number). Swapping backends requires no change to
  calling code.

## CKKS Workload

`tools/bench/ckks-bench.ts` benchmarks five parameter sets
(`lib/ckks/paramSets.ts`, N = 2^10 … 2^14) across encode, encrypt, add,
multiply, relinearize, rescale, rotate, decrypt, decode — measuring
latency, ciphertext byte size, and round-trip/multiplication accuracy.
**N=1024 cannot support key-switching at 128-bit security** (its 27-bit
modulus budget fits exactly one prime) — multiply/relinearize/rescale/
rotate are reported as explicitly unsupported for that parameter set, not
silently skipped or faked. **N=2048's naively-chosen 2-prime chain also
turned out not to work** — SEAL rejects it at multiply time, a real,
documented finding in `lib/ckks/paramSets.ts`'s N2048 entry — and the
working configuration (a 3-prime chain) was found by empirical search, also
documented there.

## E1 Mapping

Based on the measured bottleneck (`docs/bottleneck-report.md`): the three
most expensive operations at every parameter set with key-switching support
are rescale, relinearize, and rotate (together 60–73% of per-op latency,
increasing with N) — all of which decompose into the same five-stage
pipeline:

```
INPUT (RNS limbs)
   │
   ▼
FORWARD NTT  ──── per-limb, O(N log N), regular butterfly network
   │                (lib/kernels/ntt.ts measures this shape standalone)
   ▼
MODULAR MULTIPLY-ADD ── fixed pattern against a public key-switch matrix
   │                     (lib/kernels/modular.ts: naive vs. Montgomery)
   ▼
INVERSE NTT  ──── mirror of the forward pass
   │
   ▼
RECOMPOSE / MOD-SWITCH ── base conversion across the RNS chain
   │
   ▼
CIPHERTEXT OUT ──── serialized size measured per parameter set
```

Each stage is fixed-size, branch-free, and dominated by moving coefficient
arrays through regular arithmetic — the shape a spatial/dataflow fabric is
designed around (parallel PEs per NTT butterfly stage or per RNS limb,
streaming producer/consumer between stages, data reuse across the fixed
key-switch matrix). Whether Electron E1 realizes an advantage on this exact
shape is the open question this repo sets up (H1) but cannot answer without
hardware.

## Benchmark Methodology

- **Timing:** `tools/bench/util.ts::timeOp` — warm-up iterations (JIT/SEAL
  cache settling) then N timed trials via `process.hrtime.bigint()`;
  reports mean/median/stddev/min/max.
- **Reproducibility:** every artifact in `results/` embeds an experiment ID
  (UUID), ISO timestamp, git commit, Node version, and host CPU/OS info
  (`lib/platform/hostInfo.ts`) — see `tools/bench/util.ts::experimentMetadata`.
- **Result labeling:** every number is one of `HOST_REFERENCE` (measured on
  this CPU), `SIMULATED_E1` (none exist in this repo), `REAL_HARDWARE`
  (none exist in this repo), or `NOT_MEASURED` (explicit placeholder, e.g.
  `lib/platform/e1Target.ts`). See `lib/platform/e1Target.ts::ResultClass`.

## Results

Full tables: `docs/bottleneck-report.md` (CKKS + kernel benchmarks),
`results/*-latest.{json,csv}` (raw data, regenerate anytime — see
[Reproduction](#reproduction)).

**CKKS op latency, mean of 30 trials (HOST REFERENCE, this repo's dev
machine):**

| Op | N1024 | N2048 | N4096 | N8192 | N16384 |
|---|---|---|---|---|---|
| encrypt | 0.28 ms | 1.00 ms | 1.95 ms | 5.20 ms | 18.26 ms |
| rescale | N/A | 0.94 ms | 2.03 ms | 6.95 ms | 34.78 ms |
| relinearize | N/A | 0.88 ms | 1.66 ms | 5.98 ms | 29.85 ms |
| rotate | N/A | 0.68 ms | 1.43 ms | 5.01 ms | 26.22 ms |

**Fleet aggregation, plaintext vs. CKKS (`apps/fleet/compare.ts`, N4096,
median of 5 trials):**

| Fleet size | Plaintext latency | Plaintext bytes | CKKS bytes | Bandwidth ratio | Mean accuracy (maxAbsErr) |
|---|---|---|---|---|---|
| 1 | 0.004 ms | 48 B | 131,185 B | 2,733x | 7.0e-5 |
| 5 | 0.005 ms | 240 B | 655,925 B | 2,733x | 2.6e-5 |
| 10 | 0.006 ms | 480 B | 1,311,850 B | 2,733x | 3.5e-5 |
| 25 | 0.013 ms | 1,200 B | 3,279,625 B | 2,733x | 5.8e-6 |
| 50 | 0.011 ms | 2,400 B | 6,559,250 B | 2,733x | 1.0e-5 |

CKKS's win here is confidentiality of the aggregator's view (it never holds
a plaintext per-drone reading), not bandwidth — the bandwidth ratio is
flat at ~2,733x because each drone's ciphertext size is independent of
fleet size; this is reported plainly rather than cherry-picked around.

**Kernel micro-benchmark, Montgomery vs. naive modular multiplication**
(`lib/kernels/modular.ts`, host reference): Montgomery multiplication was
**19–83x slower** than naive BigInt `%` at every N tested, because V8's
native BigInt modulo is already a tuned C++ routine and the from-scratch
Montgomery implementation pays pure overhead in a managed VM with no
corresponding division cost to avoid. Reported as a loss, per
`docs/bottleneck-report.md` §4 — not omitted.

## Energy Analysis

**Not measured.** This environment has no power-measurement instrumentation
(no RAPL/perf-energy access exposed, no E1 hardware, no bench-grade power
meter). Every "energy" field that would appear in a complete version of
this project (energy/inference, energy/drone-minute, energy per useful
decision — section 17 of the original brief) is intentionally absent rather
than estimated from a proxy and presented as measured. `lib/platform/e1Target.ts`
encodes this directly: its `E1ExecutionResult.energyMicrojoules` is always
`null` with `resultClass: 'NOT_MEASURED'`.

## CRA-Aware Security Evidence

Not a compliance claim — evidence-generation tooling only.
`tools/security/generate-manifest.ts` produces `results/security-manifest.json`
from this repository's *actual* state: real `package.json` dependencies,
the real CKKS parameter sets in use, the real git commit, a real SHA-256 of
`package-lock.json`. Fields this prototype cannot honestly populate say so
explicitly instead of being filled with a plausible-looking placeholder —
e.g. `knownVulnerabilities` points at `npm run security:sbom` (which runs
the real CycloneDX generator, `@cyclonedx/cyclonedx-npm`, producing
`results/sbom.json`) and names the scanners (`npm audit`, Grype,
OSV-Scanner) that would need to run against it, rather than claiming a scan
occurred. `updateMechanism` says plainly: "None implemented." See also
`docs/threat-model.md` for the companion human-readable threat register.
The full 15-document CRA evidence set from the original brief (asset
inventory, incident-response procedure, attack-surface map, etc.) is
**not built** this session — see below.

## Limitations

- **No E1 hardware or effcc toolchain was available.** Every timing/energy
  number in this repository is a HOST REFERENCE result on ordinary x86_64
  silicon via Microsoft SEAL compiled to WASM (node-seal) — never a
  silicon measurement. `lib/platform/e1Target.ts` is a stub that always
  returns `NOT_MEASURED`.
- **No energy instrumentation was available** — see
  [Energy Analysis](#energy-analysis).
- **The NTT/modular-arithmetic bottleneck decomposition is at the
  operation level, not inside SEAL.** node-seal does not expose SEAL's
  internal NTT or modular-arithmetic routines as separately callable, so
  there is no way to time "SEAL's NTT" in isolation; `lib/kernels` is a
  from-scratch reference implementation used to reason about the
  architectural *shape* of the workload, not a drop-in replacement whose
  absolute timings are comparable to SEAL's.
- **Fleet networking is simulated in-process** (`apps/fleet/compare.ts`) —
  no sockets are opened, so reported "bandwidth" is serialized payload
  size, and reported "latency" is compute time only, not network RTT.
- **No chained multi-level multiplicative depth was tested.** Each
  parameter set is verified for exactly one multiply→relinearize→rescale→
  rotate cycle; deeper chains are not exercised or claimed.

### Not built this session

Per the original project brief, these are explicitly deferred, not
secretly faked:

- The browser-based interactive 3D fleet simulator (drones/terrain/
  comm-links/attack injection UI).
- Red-team mode (live network-intercept / compromised-cloud / compromised-
  drone demonstrations) beyond the threat register in `docs/threat-model.md`.
- The full 15-document CRA evidence set (asset inventory, incident-response
  procedure, attack-surface map, build/reproducibility record beyond what
  `lib/security/manifest.ts` already covers).
- effcc / E1 compiler integration and CPU-vs-GPU-vs-E1 application
  benchmarking (no toolchain or hardware access).
- The "Why Efficient?" per-stage interactive analyzer UI (the written
  analysis exists in [Why Efficient?](#why-efficient) and
  [E1 Mapping](#e1-mapping) above; no UI was built).

## Reproduction

```sh
npm install
npm run bench:ckks       # tools/bench/ckks-bench.ts -> results/ckks-bench-*.{json,csv}
npm run bench:kernels    # tools/bench/kernel-bench.ts -> results/kernel-bench-*.{json,csv}
npm run fleet:compare    # apps/fleet/compare.ts -> results/fleet-aggregation-compare-*.{json,csv}
npm run security:sbom    # -> results/sbom.json (CycloneDX)
npm run security:manifest # tools/security/generate-manifest.ts -> results/security-manifest.json
```

Every command above writes a timestamped artifact and a `-latest` copy
under `results/`, each self-describing (experiment ID, git commit, host
info) per [Benchmark Methodology](#benchmark-methodology).

## Future Work

- Hardware validation on actual Electron E1 silicon / the effcc toolchain,
  to actually test H1 and H2.
- A real power-measurement rig (even a USB power meter on a Raspberry-Pi-
  class host) to stop reporting "Not measured" for energy.
- Slot-packing multiple drones' readings into one ciphertext (CKKS SIMD
  batching) to explore whether upload bandwidth can be amortized across a
  fleet — the current `apps/fleet/compare.ts` deliberately does not do this
  (one ciphertext per drone) and reports the resulting bandwidth cost
  honestly rather than optimizing it away before measuring the naive case.
- Chained multi-level multiplicative depth testing per parameter set.
- The interactive 3D simulator, red-team mode, and full CRA evidence set
  listed under [Not built this session](#not-built-this-session).
