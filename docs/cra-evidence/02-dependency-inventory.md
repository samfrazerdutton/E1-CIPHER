# Dependency Inventory

_Generated from this repo's actual `package.json` at commit `864bb08330182b3a620d78b31dac18aea4e29ed4`. Full transitive tree: `results/sbom.json` (CycloneDX, `npm run security:sbom`)._

**Scope note:** this covers the root `package.json` only. `apps/dashboard` is a separate npm sub-project (its own `package.json`/`package-lock.json`, React/Vite toolchain) with its own dependency tree, not included here or in `results/sbom.json`. Run `cd apps/dashboard && npx cyclonedx-npm` for its SBOM if needed.

| Package | Declared version | Runtime or dev |
|---|---|---|
| `node-seal` | `^7.0.0` | runtime |
| `@cyclonedx/cyclonedx-npm` | `^6.0.1` | dev |
| `@types/node` | `^26.6.4` | dev |
| `tsx` | `^4.23.15` | dev |
| `typescript` | `^7.0.2` | dev |
