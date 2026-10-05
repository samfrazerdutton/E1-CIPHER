# Demo Script (5 minutes)

A technical presentation script for walking an Efficient Computer engineer
through this repository. Every command below is real and actually runs —
try it before presenting it.

## Minute 1 — Problem

> Autonomous physical-AI systems — a drone fleet inspecting infrastructure,
> in this project's reference scenario — generate continuous, sensitive,
> physical-world data under three constraints that rarely get solved
> together: a tight energy budget, bandwidth that's often poor, and a real
> need to keep raw observations confidential from whoever operates the
> central aggregation system. "Just use TLS to a trusted cloud" doesn't
> address a compromised-cloud or malicious-operator threat model — see
> `docs/threat-model.md`. Homomorphic encryption (CKKS) is the option that
> addresses it, but it's never been cheap enough to take seriously at the
> battery-powered edge.

## Minute 2 — Architecture

```sh
./build/apps/e1cipher inspect --drones 20
```

Walk through the printed pipeline live: sensor ingestion -> fusion ->
policy classification -> selective CKKS encryption -> fleet aggregation ->
decision. Point out that `camera_frame` never leaves the device
(`LOCAL_ONLY`), `object_embedding` is encrypted (`CONFIDENTIAL`), and
`battery_telemetry` goes out in the clear (`PLAINTEXT`) — this is the
intellectual contribution: **encrypt only what needs cryptographic
confidentiality**, not everything. See `docs/architecture-assessment.md`
for why this moved from TypeScript to C++20/CMake, and
`include/e1cipher/` for the data model (`TelemetrySample`, `FeatureVector`,
`PlacementDecision` — all documented structs with `static_assert`ed
memory layout, not generic JSON blobs).

## Minute 3 — C++ implementation

```sh
./build/benchmarks/ckks_bench
./build/benchmarks/kernel_bench
```

This is real Microsoft SEAL, built from source via CMake `FetchContent`,
linked with Clang+Ninja (no Visual Studio needed — a real toolchain
constraint this project solved, not assumed away). Point out:

- N1024 has **no** key-switching support at 128-bit security (reported as
  `N/A`, not hidden) — a real cryptographic-parameter constraint, not a
  bug.
- Key-switching ops (rescale/relinearize/rotate) dominate 60-73% of
  per-op latency and *grow* with N — the actual bottleneck finding, with
  a decomposition into NTT / modular-multiply-add / recompose stages in
  `docs/e1-application-mapping.md`.
- The Montgomery-multiplication kernel is **3-4x slower** than naive
  128-bit hardware division, natively — not a JS-VM artifact from the
  TS-era prototype, a genuine finding about this host's divide unit. Say
  this plainly; it's evidence of measurement rigor, not a weakness to
  hide.

## Minute 4 — Security / compliance

```sh
./build/apps/e1cipher security
```

Walk through the PASS/WARN/FAIL gate live. The WARNs are deliberate and
named: no E1 hardware validation, an incident-response doc that's a
template not a capability, dependency/vulnerability scanning that's Node
tooling's job (`tools/security/generate-evidence.ts`), not this binary's.
Mention `docs/compliance/cra-mapping.md` — "CRA-aware engineering
evidence," never "CRA compliant." Mention that the fleet gateway
structurally never holds a `SecretKey` (`apps/fleet_gateway/fleet.cpp`) —
show the one line of code, don't just assert it.

## Minute 5 — Why E1

> The measured bottleneck is specific: RNS decomposition, forward NTT, a
> fixed pattern of modular multiply-adds against a public key matrix,
> inverse NTT, recomposition — five stages, each fixed-size, branch-free,
> dominated by moving coefficient arrays through regular arithmetic. That
> is the shape a spatial-dataflow architecture is supposed to help with.
> Whether Electron E1 actually delivers that advantage is the open,
> testable question this repository sets up (`README.md`'s H1) and
> cannot answer without hardware — `platform/e1/e1_platform.hpp` is a
> clean, honest stub, not a simulation dressed up as a result.

Closing line:

> "I didn't start by asking what demo I could build for the E1. I started
> with a workload an autonomous system actually needs, measured where
> confidentiality becomes expensive, designed the runtime around those
> constraints, and built a C/C++ implementation that's ready to be taken
> to E1 hardware for validation — not rewritten for it."
