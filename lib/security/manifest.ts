import { readFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { CKKS_PARAM_SETS, totalCoeffModulusBits } from '../ckks/paramSets.js';
import { DATA_CATALOG } from '../policy/classification.js';
import { captureHostInfo } from '../platform/hostInfo.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const REPO_ROOT = path.resolve(__dirname, '../..');

interface PackageJson {
  name: string;
  version: string;
  dependencies?: Record<string, string>;
  devDependencies?: Record<string, string>;
}

function readPackageJson(): PackageJson {
  const raw = readFileSync(path.join(REPO_ROOT, 'package.json'), 'utf8');
  return JSON.parse(raw) as PackageJson;
}

export interface SecurityManifest {
  readonly product: string;
  readonly version: string;
  readonly generatedAt: string;
  readonly build: {
    readonly gitCommit: string;
    readonly nodeVersion: string;
    readonly platform: string;
    readonly packageLockHashSha256: string;
  };
  readonly cryptography: {
    readonly schemeFamily: string;
    readonly library: { readonly name: string; readonly version: string; readonly upstream: string };
    readonly parameterSets: ReadonlyArray<{
      readonly id: string;
      readonly polyModulusDegree: number;
      readonly coeffModulusBits: readonly number[];
      readonly totalCoeffModulusBits: number;
      readonly scaleBits: number;
    }>;
    readonly otherBackends: readonly string[];
  };
  readonly dataClassifications: typeof DATA_CATALOG;
  readonly dependencies: { readonly name: string; readonly version: string; readonly dev: boolean }[];
  readonly knownVulnerabilities: string;
  readonly updateMechanism: string;
  readonly networkInterfaces: string;
  readonly testStatus: string;
}

/**
 * Builds the machine-readable security manifest from the actual state of
 * this repository (package.json, git HEAD, the CKKS parameter sets the code
 * really uses) — nothing in this object is invented. Fields that would
 * require tooling or processes this prototype does not have (vulnerability
 * scanning, a shipped update mechanism) say so explicitly rather than
 * being populated with plausible-looking placeholder values.
 */
export function buildSecurityManifest(): SecurityManifest {
  const pkg = readPackageJson();
  const host = captureHostInfo();
  const lockRaw = readFileSync(path.join(REPO_ROOT, 'package-lock.json'));
  const packageLockHashSha256 = createHash('sha256').update(lockRaw).digest('hex');

  const dependencies = [
    ...Object.entries(pkg.dependencies ?? {}).map(([name, version]) => ({ name, version, dev: false })),
    ...Object.entries(pkg.devDependencies ?? {}).map(([name, version]) => ({ name, version, dev: true })),
  ];

  return {
    product: pkg.name,
    version: pkg.version,
    generatedAt: new Date().toISOString(),
    build: {
      gitCommit: host.gitCommit,
      nodeVersion: host.nodeVersion,
      platform: `${host.platform}-${host.arch}`,
      packageLockHashSha256,
    },
    cryptography: {
      schemeFamily: 'CKKS (approximate-arithmetic leveled homomorphic encryption, RLWE-based)',
      library: { name: 'node-seal', version: pkg.dependencies?.['node-seal'] ?? 'unknown', upstream: 'Microsoft SEAL 4.1.2' },
      parameterSets: CKKS_PARAM_SETS.map((p) => ({
        id: p.id,
        polyModulusDegree: p.polyModulusDegree,
        coeffModulusBits: p.coeffModulusBits,
        totalCoeffModulusBits: totalCoeffModulusBits(p),
        scaleBits: p.scaleBits,
      })),
      otherBackends: ['Plaintext (no cryptography, baseline)', 'MockEncrypted (non-cryptographic placeholder, development only)'],
    },
    dataClassifications: DATA_CATALOG,
    dependencies,
    knownVulnerabilities:
      'Not scanned by this generator. Run `npm run security:sbom` to produce results/sbom.json (CycloneDX, via @cyclonedx/cyclonedx-npm) ' +
      'and feed it to a vulnerability scanner (e.g. `npm audit`, Grype, OSV-Scanner) for an actual vulnerability register entry.',
    updateMechanism:
      'None implemented. This is a research prototype distributed as source; there is no signed-update or OTA mechanism. ' +
      'A production system would need one before any CRA security-update-policy claim could be supported.',
    networkInterfaces:
      'None opened by default. apps/fleet/compare.ts simulates the fleet/aggregator network path in-process (no sockets) ' +
      'so that reported latency/bandwidth numbers are pure compute+serialization cost, not this host\'s network stack.',
    testStatus: 'See results/ for the latest tools/bench and apps/fleet run artifacts; no automated CI is configured in this prototype.',
  };
}
