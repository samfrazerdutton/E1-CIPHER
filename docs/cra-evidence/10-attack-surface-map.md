# Attack Surface Map

| Surface | Exposure | Description |
|---|---|---|
| Network listeners | none | None. No server/socket is opened by any script in this repo. |
| CLI entry points | local-process | 8 scripts runnable via `npx tsx`/npm scripts (apps/demo/killer-demo.ts, apps/fleet/compare.ts, apps/redteam/intercept-demo.ts, tools/bench/ckks-bench.ts, tools/bench/kernel-bench.ts, tools/bench/util.ts, …). Attack surface here is local-process only: whoever can execute Node in this checkout. |
| Filesystem (results/) | filesystem | Benchmark/demo/security tools read package.json and package-lock.json and write JSON/CSV under results/. No path comes from untrusted/network input. |
| npm dependency tree (5 declared, 1 runtime) | third-party-dependency | node-seal (runtime) and its WASM binary are the actual cryptographic trust base; a compromised upstream package is this prototype's largest realistic attack surface. See results/sbom.json for the full transitive tree. |

See `docs/threat-model.md` for the corresponding threat register and `lib/security/manifest.ts`'s
`networkInterfaces` field for the machine-readable version of the "no open network ports" claim above.
