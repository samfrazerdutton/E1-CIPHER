# Architecture Assessment (pre-rewrite)

Written before the C/C++ restructure (commit `b52a52e` is the last
TypeScript-only state — see `git log` / `git show b52a52e` to inspect it in
full). This assessment decides what moves to C++, what stays as tooling,
and what gets cut.

## What exists today (TypeScript)

| Area | What it does | Verdict |
|---|---|---|
| `lib/ckks` (node-seal/SEAL 4.1.2) | CryptoBackend abstraction; real CKKS encode/encrypt/add/multiply/relinearize/rescale/rotate/decrypt/decode across 5 parameter sets | **Valuable research, wrong language.** The CKKS parameter sets, the N2048-needs-3-primes finding, and the accuracy numbers are real and worth keeping as a *baseline reference*. The computation itself must move to C++ against real Microsoft SEAL (confirmed buildable here with Clang+Ninja+CMake, no Visual Studio — see below), not node-seal's WASM build. |
| `lib/kernels` (modular.ts, ntt.ts) | From-scratch reference modular-arithmetic/NTT kernels, used to argue about E1's architectural fit | **Valuable finding (Montgomery mult. is *slower* than naive BigInt `%` in a managed VM), wrong substrate.** That finding is specifically a JS-BigInt-VM artifact — it doesn't transfer to C++. The kernels get rewritten in C/C++ from scratch; the *question* (what's the real modmul/NTT cost shape) carries over, the *numbers* do not. |
| `lib/telemetry`, `lib/sensor_fusion` | Deterministic seeded synthetic drone telemetry + feature extraction | **Logic is sound and small.** Ports directly to C++ structs/functions. No change in design. |
| `lib/policy` | Data classification + adaptive placement scheduler, explainable decisions | **Logic is sound.** Ports to C++, but gets a real addition it didn't have before: **fail-closed** behavior (unknown classification, missing policy, or unavailable crypto backend must block, not default-permit — this repo's TS version always resolved to *some* placement; that's a gap, not a feature). |
| `apps/fleet` (plaintext-vs-CKKS aggregation) | Real measured comparison, correctly reports the ~2,733x ciphertext expansion | **Valuable result, wrong language.** Reruns in C++ against real SEAL; the finding (confidentiality costs bandwidth, doesn't save it) is expected to reproduce and must be reported again even if it's still unflattering. |
| `apps/demo`, `apps/redteam` | Narrative demos reusing the above | **Presentation on top of real computation.** Rebuilds naturally once the C++ core exists; not a priority before the core itself. |
| `lib/security`, `tools/security`, `docs/cra-evidence` | SBOM/manifest/evidence generation from live repo state (real `npm audit`, real `npm test`, real git state) | **Genuinely useful, correctly scoped as tooling.** This is exactly the kind of thing that may legitimately *stay* in TypeScript/Node per the new thesis ("TypeScript may remain for... development tooling... report generation") — but it must be extended to describe the C++ subsystem (new dependency: SEAL; new asset tree: `src/`, `include/`, `apps/`, `platform/`), not just the npm one. |
| `apps/dashboard` | Vite+React browser dashboard, 8 views, one genuinely live (imports `lib/telemetry`/`lib/policy` into the browser bundle) | **Presentation layer, correctly scoped as secondary.** Explicitly allowed to remain ("TypeScript/React may remain ONLY for... optional visualization"). Its live Simulator view is the one place TS logic was load-bearing rather than decorative — that capability is superseded once the C++ core + CLI exist, but the dashboard itself is not the thing being graded here and is left as-is for now rather than immediately reworked. |
| `lib/platform` (`hostInfo.ts`, `e1Target.ts`) | Host-info capture + an explicitly-stubbed E1 hardware abstraction that always returns `NOT_MEASURED` | **Right idea, ports to C++ as the real platform layer** (`platform/host`, `platform/e1`, `platform/mock`). The "never fabricate E1 numbers" discipline carries over unchanged — it's a project-wide invariant, not implementation detail. |

## Decision

**Move the computational core to C++20 + CMake.** Concretely: telemetry
generation, sensor fusion, the policy engine, the CryptoBackend abstraction
and its CKKS/plaintext/mock implementations, the fleet-aggregation and
drone-inspection applications, the benchmark suite, and the reference
kernels. TypeScript's remaining job is tooling: the security/CRA evidence
generator (extended to cover the C++ subsystem) and the dashboard
(unchanged for now, visualizing C++-generated result artifacts going
forward instead of TS-generated ones).

## Toolchain reality check (done before writing any C++)

This environment has **no Visual Studio, no MSBuild, no `make`/`nmake`**.
It does have CMake 4.2, Clang 22 (targeting `x86_64-pc-windows-msvc`), and
Ninja (installed via `pip install ninja` — not preinstalled). Verified
before committing to this plan, not assumed:

- A trivial C++20 program compiles and links with `clang++` directly.
- CMake configures and builds with `-G Ninja -DCMAKE_CXX_COMPILER=clang++`.
- **Real Microsoft SEAL 4.1.2 builds from source** against this toolchain
  (`-DSEAL_BUILD_DEPS=ON` to fetch Microsoft GSL via CMake FetchContent,
  `-DSEAL_USE_ZLIB=OFF -DSEAL_USE_ZSTD=OFF -DSEAL_USE_INTEL_HEXL=OFF` to
  avoid extra dependencies not needed for this project), producing a static
  `seal-4.1.lib`.
- A real CKKS round-trip (encode → encrypt → homomorphic add → decrypt →
  decode) against that build executes correctly (`max_err ≈ 1.5e-8` at
  N=8192). The one build wrinkle — a CRT linkage mismatch between the
  default static-CRT test program and SEAL's default dynamic-CRT release
  build — is resolved with `-fms-runtime-lib=dll`, documented in
  `cmake/` once the real build is wired up, not papered over.

This means the project's single biggest technical risk — "can a real C/C++
CKKS library actually be built and run here, without Visual Studio" — is
resolved before any application code is written, not discovered halfway
through.

## What does not change

- No E1 hardware, effcc toolchain, or power instrumentation exists in this
  environment. Every constraint already documented in the TS-era README
  under Limitations/Not-built carries over unchanged to the C++ project —
  restructuring the implementation does not create hardware that doesn't
  exist. `platform/e1/` is a clean interface, not a simulation.
- The project's honesty invariants (label every result HOST_REFERENCE /
  REAL_HARDWARE / SIMULATED / NOT_MEASURED; never report a fabricated
  number; report unflattering findings — the 2,733x ciphertext expansion,
  the N2048 parameter-set failure — rather than hide them) are unchanged.
