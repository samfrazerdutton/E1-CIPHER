# E1 Application Mapping

For every pipeline stage, what is actually known about its computational
shape (measured, in this repository), and what would need validating
against real Electron E1 architecture/toolchain documentation before any
hardware-fit claim could be made. Nothing below states an E1 fact — every
"E1 mapping question" row is phrased as a question because no verified E1
architectural specification (PE count, clock, cache sizes, memory
bandwidth, ISA) was available while writing this document. See
`docs/architecture-assessment.md` and `platform/e1/e1_platform.hpp`.

## Telemetry generation (`src/telemetry/generator.cpp`)

| | |
|---|---|
| Workload | Per-drone, per-tick scalar math (trig, PRNG) producing a `TelemetrySample` |
| Parallelism | Fully data-parallel across drones and ticks (each sample is independent) |
| Memory pattern | Small, fixed-size struct writes; no cross-sample dependency |
| Data dependency | None across drones; a per-drone PRNG stream is sequential within one drone |
| Compute intensity | Low (a handful of transcendental functions per sample) |
| Branching | A few scenario-conditional branches, data-independent per call |
| State | Stateless generator function; PRNG state is local to each call |
| Expected hardware requirement | Negligible — this is not the workload E1's value proposition is about |
| E1 mapping question | None — this stage is not architecture-sensitive; it exists to produce realistic input, not to be accelerated |

## Sensor fusion (`src/fusion/features.cpp`)

| | |
|---|---|
| Workload | Fixed arithmetic reducing one `TelemetrySample` to a 6-scalar `FeatureVector` |
| Parallelism | Fully data-parallel across drones |
| Memory pattern | Read one small struct, write one smaller struct |
| Data dependency | None |
| Compute intensity | Trivial (one sqrt, five field copies) |
| Branching | None |
| State | Stateless |
| Expected hardware requirement | Negligible |
| E1 mapping question | None — same reasoning as telemetry generation |

## Policy engine (`src/policy/scheduler.cpp`)

| | |
|---|---|
| Workload | A fixed sequence of comparisons and branches over ~10 scalar inputs |
| Parallelism | Fully data-parallel across drones/data-items; each decision is independent |
| Memory pattern | Negligible; reads a small input struct, builds a small output struct + string explanation |
| Data dependency | None across decisions |
| Compute intensity | Trivial |
| Branching | Control-flow-heavy (this *is* the workload — it's a decision tree) |
| State | Stateless (deterministic function of its input) |
| Expected hardware requirement | Negligible; this is scalar control logic, not a numerics kernel |
| E1 mapping question | None — a spatial-dataflow architecture's advantages (regular, branch-free, high-arithmetic-intensity execution) are specifically *not* what this stage needs. Running it on E1 would be a reasonable systems choice (co-locate control logic with the crypto accelerator) but not an acceleration win in itself. |

## CKKS crypto (`src/crypto/ckks_backend.cpp`, benchmarked in `benchmarks/crypto/ckks_bench.cpp`)

This is the stage with an actual, measured architectural argument — see
`docs/bottleneck-report.md` for the full data. Summary, per sub-operation:

| | encode/decode | encrypt/decrypt | add | multiply (raw) | **relinearize/rescale/rotate** |
|---|---|---|---|---|---|
| Workload | FFT-like transform between slot and coefficient domains | Sample RLWE error, polynomial add | Elementwise polynomial add across RNS limbs | Elementwise polynomial multiply across RNS limbs | RNS decomposition -> NTT -> fixed modular multiply-adds against a public key matrix -> inverse NTT -> recompose |
| Parallelism | Data-parallel across coefficients/limbs | Data-parallel across coefficients | Fully data-parallel across coefficients | Fully data-parallel across coefficients | Data-parallel within each NTT stage; stages themselves are sequential |
| Memory pattern | Strided access per FFT butterfly stage | Mostly sequential | Sequential, high reuse | Sequential, high reuse | Repeated full-polynomial sweeps per RNS limb -- the most data movement per useful op of any stage measured |
| Data dependency | Sequential across log(N) butterfly stages | None | None | None | Sequential across both NTT passes' stages; the modular multiply-add stage depends on the completed forward NTT |
| Compute intensity | Moderate | Moderate | Low (one add per coefficient) | Moderate | **Highest of all measured ops** -- this is where 60-73% of per-op latency goes (see bottleneck report), growing with N |
| Branching | None (fixed-size loops) | None | None | None | None |
| State | None beyond the ciphertext/key objects | Needs the public key | None | None | Needs the relin/Galois keys (a public, fixed matrix) |
| Expected hardware requirement | Moderate memory bandwidth | Moderate | Low | Moderate | High arithmetic throughput AND high memory bandwidth, sustained across every RNS limb |
| E1 mapping question | Does E1's architecture give an NTT butterfly network a parallelism/data-movement advantage over this host's measured numbers? **Requires validation against E1 architecture/toolchain** -- not answerable from this repository alone. | Same question, smaller magnitude. | Likely the least E1-sensitive sub-op (embarrassingly parallel, low intensity) -- but still **requires validation**. | Same as add, one more op. | **This is the central open question** (H1 in `README.md`): if E1's spatial-dataflow execution model has a genuine advantage on fixed, regular, data-movement-heavy transforms, key-switching is where it would show up first. Cannot be answered without real E1 hardware/toolchain access. |

## Reference kernels (`src/crypto/kernels/`)

| | modular multiply (naive, `unsigned __int128 %`) | Montgomery multiply | NTT (forward/inverse) |
|---|---|---|---|
| Workload | One 128-bit-by-64-bit hardware division per element | Shifts + two 128-bit multiplies per element, no division | O(N log N) butterfly network, one modular multiply-add per butterfly |
| Measured finding (this host) | **Faster** than Montgomery (see `docs/bottleneck-report.md` §4) -- this host's `div` instruction is cheap enough that avoiding it isn't worth the extra multiply/shift work | **3-4x slower** than naive, natively, not just in the TS prototype's JS VM | O(N log N) scaling confirmed; see `results/kernel-bench-latest.json` |
| E1 mapping question | Does E1 have a comparably cheap wide-integer divide, or would Montgomery's division-free approach actually win there? **Requires validation** -- the naive approach's advantage here is specifically an x86_64 hardware-divide artifact, which may not generalize. | Same question, inverted. | Does E1's architecture reward the butterfly network's regular, parallel structure more than this host's branch-predictor/cache hierarchy already does? **Requires validation.** |

## Fleet aggregation (`apps/fleet_gateway/fleet.cpp`)

| | |
|---|---|
| Workload | N-way homomorphic ciphertext summation (`CryptoBackend::add`, N-1 times) |
| Parallelism | Encryption of each drone's vector is embarrassingly parallel across drones; the summation itself is a simple reduction (log-depth parallel, or linear-depth as implemented here) |
| Memory pattern | One ciphertext-sized buffer per drone, summed into an accumulator |
| Data dependency | The reduction has a dependency chain of depth N-1 in this implementation (not tree-reduced) |
| Compute intensity | Dominated by whatever `add`'s cost is (low, per the CKKS table above) |
| Branching | None |
| State | None beyond the accumulator |
| Expected hardware requirement | Scales with fleet size; bandwidth to move N ciphertexts is the dominant real-world cost (see the ~2,733x expansion finding) |
| E1 mapping question | Not primarily a compute question -- the dominant cost here is ciphertext *size*, which is a CKKS parameter-set property, not something any accelerator changes. The open, answerable-today question is whether tree-reducing the summation (not yet done — see README Future Work) changes the critical path meaningfully on *any* platform; that should be measured on this host before it's worth asking an E1-specific question. |
