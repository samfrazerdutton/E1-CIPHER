# Asset Inventory

_Generated from the actual repository file tree — not a template. Regenerate with `npm run security:evidence`._

| Path | Kind | Description |
|---|---|---|
| `lib/ckks` | library | CryptoBackend abstraction + real CKKS backend (node-seal/Microsoft SEAL), plaintext baseline, mock-encrypted placeholder. |
| `lib/kernels` | library | From-scratch reference modular-arithmetic and NTT kernels, used for architecture-level bottleneck reasoning. |
| `lib/telemetry` | library | Deterministic synthetic drone-fleet telemetry generator (seeded PRNG, offline). |
| `lib/sensor_fusion` | library | Reduces a telemetry sample to the fixed-layout feature vector that is allowed to leave a drone. |
| `lib/policy` | library | Data classification catalog + explainable adaptive compute-placement scheduler. |
| `lib/security` | library | Security manifest and CRA evidence generation (this module). |
| `lib/platform` | library | Host-info capture and the (explicitly stubbed) Electron E1 hardware abstraction layer. |
| `apps/fleet` | application | Plaintext-vs-CKKS fleet aggregation comparison across 5 fleet sizes. |
| `apps/demo` | application | The end-to-end "killer demo" narrative (confidential fleet anomaly response). |
| `apps/redteam` | application | Defensive red-team demonstration: network intercept, compromised cloud, compromised drone. |
| `apps/dashboard` | application | Separate Vite+React sub-project: browser visualization of this repo's real results/*.json artifacts. Own package.json/tsconfig/build — not scanned as part of this repo's CLI attack surface. |
| `tools/bench` | tool | CKKS operation and kernel micro-benchmark suites. |
| `tools/security` | tool | Security manifest, SBOM, and CRA evidence generator CLIs. |
| `docs` | documentation | Human-readable threat model, bottleneck report, and this evidence set. |
| `results` | configuration | Generated, reproducible benchmark/demo/security artifacts (JSON + CSV). |
| `package.json` | configuration | npm package manifest; also the dependency-inventory source of truth. |
| `tsconfig.json` | configuration | TypeScript compiler configuration. |
