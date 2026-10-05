import { execSync } from 'node:child_process';
import { readdirSync, statSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { buildSecurityManifest, type SecurityManifest } from './manifest.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const REPO_ROOT = path.resolve(__dirname, '../..');

/**
 * CRA-readiness evidence generation (project brief sections 12-13).
 *
 * Every function here reads the repository's actual state (file tree,
 * npm audit, git, package.json) — nothing is a template filled with
 * invented values. Where a real evidence artifact genuinely cannot be
 * produced without infrastructure this prototype doesn't have (a shipped
 * update mechanism, an operational incident-response team), the generated
 * document says so explicitly instead of simulating one. This is evidence-
 * generation tooling for a CRA risk assessment, not a compliance claim —
 * see docs/README.md "CRA-Aware Security Evidence".
 */

export interface AssetEntry {
  readonly path: string;
  readonly kind: 'library' | 'application' | 'tool' | 'documentation' | 'configuration';
  readonly description: string;
}

const ASSET_DESCRIPTIONS: Record<string, string> = {
  'lib/ckks': 'CryptoBackend abstraction + real CKKS backend (node-seal/Microsoft SEAL), plaintext baseline, mock-encrypted placeholder.',
  'lib/kernels': 'From-scratch reference modular-arithmetic and NTT kernels, used for architecture-level bottleneck reasoning.',
  'lib/telemetry': 'Deterministic synthetic drone-fleet telemetry generator (seeded PRNG, offline).',
  'lib/sensor_fusion': 'Reduces a telemetry sample to the fixed-layout feature vector that is allowed to leave a drone.',
  'lib/policy': 'Data classification catalog + explainable adaptive compute-placement scheduler.',
  'lib/security': 'Security manifest and CRA evidence generation (this module).',
  'lib/platform': 'Host-info capture and the (explicitly stubbed) Electron E1 hardware abstraction layer.',
  'apps/fleet': 'Plaintext-vs-CKKS fleet aggregation comparison across 5 fleet sizes.',
  'apps/demo': 'The end-to-end "killer demo" narrative (confidential fleet anomaly response).',
  'apps/redteam': 'Defensive red-team demonstration: network intercept, compromised cloud, compromised drone.',
  'apps/dashboard': 'Separate Vite+React sub-project: browser visualization of this repo\'s real results/*.json artifacts, plus one live view that imports lib/telemetry and lib/policy directly into the browser bundle. Own package.json/tsconfig/build — not scanned as part of this repo\'s CLI attack surface.',
  'tools/bench': 'CKKS operation and kernel micro-benchmark suites.',
  'tools/security': 'Security manifest, SBOM, and CRA evidence generator CLIs.',
  'docs': 'Human-readable threat model, bottleneck report, and this evidence set.',
  'results': 'Generated, reproducible benchmark/demo/security artifacts (JSON + CSV).',
};

export function buildAssetInventory(): AssetEntry[] {
  const entries: AssetEntry[] = [];
  for (const [rel, description] of Object.entries(ASSET_DESCRIPTIONS)) {
    const abs = path.join(REPO_ROOT, rel);
    if (!statSync(abs, { throwIfNoEntry: false })) continue;
    const kind: AssetEntry['kind'] = rel.startsWith('lib/')
      ? 'library'
      : rel.startsWith('apps/')
        ? 'application'
        : rel.startsWith('tools/')
          ? 'tool'
          : rel === 'docs'
            ? 'documentation'
            : 'configuration';
    entries.push({ path: rel, kind, description });
  }
  entries.push(
    { path: 'package.json', kind: 'configuration', description: 'npm package manifest; also the dependency-inventory source of truth.' },
    { path: 'tsconfig.json', kind: 'configuration', description: 'TypeScript compiler configuration.' },
  );
  return entries;
}

export interface VulnerabilityRegisterEntry {
  readonly name: string;
  readonly severity: string;
  readonly via: string;
  readonly range: string;
}

export interface VulnerabilityRegister {
  readonly generatedAt: string;
  readonly tool: string;
  readonly totalsBySeverity: Record<string, number>;
  readonly entries: VulnerabilityRegisterEntry[];
  readonly rawExitCode: number;
}

/**
 * Runs the real `npm audit --json` against this repo's actual lockfile.
 * Never fabricates a scan result. Uses execSync (not execFileSync) with a
 * fixed, literal command string containing no interpolated/external input —
 * `npm` resolves to `npm.cmd` on Windows, which Node's execFile cannot
 * invoke without a shell, so this is the documented safe case for exec().
 */
export function runVulnerabilityScan(): VulnerabilityRegister {
  let stdout = '';
  let exitCode = 0;
  try {
    stdout = execSync('npm audit --json', { cwd: REPO_ROOT, encoding: 'utf8' });
  } catch (err) {
    const e = err as { stdout?: string; status?: number };
    stdout = e.stdout ?? '{}';
    exitCode = e.status ?? 1;
  }

  const parsed = JSON.parse(stdout || '{}') as {
    vulnerabilities?: Record<string, { severity: string; via: unknown[]; range?: string }>;
    metadata?: { vulnerabilities?: Record<string, number> };
  };

  const entries: VulnerabilityRegisterEntry[] = Object.entries(parsed.vulnerabilities ?? {}).map(([name, v]) => ({
    name,
    severity: v.severity,
    via: v.via.map((x) => (typeof x === 'string' ? x : (x as { name?: string }).name ?? JSON.stringify(x))).join(', '),
    range: v.range ?? 'unknown',
  }));

  return {
    generatedAt: new Date().toISOString(),
    tool: 'npm audit (via this repo\'s own package-lock.json)',
    totalsBySeverity: parsed.metadata?.vulnerabilities ?? {},
    entries,
    rawExitCode: exitCode,
  };
}

export interface TestRunResult {
  readonly generatedAt: string;
  readonly command: string;
  readonly passed: boolean;
  readonly summary: string;
}

/** Actually runs this repo's test suite and reports the real result — never a precomputed/assumed pass. Same fixed-literal-command exec() exception as runVulnerabilityScan above. */
export function runSecurityRelevantTests(): TestRunResult {
  const command = 'npm test';
  try {
    const stdout = execSync(command, { cwd: REPO_ROOT, encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] });
    const summaryLine = stdout.split('\n').find((l) => l.includes('pass')) ?? stdout.trim().slice(-200);
    return { generatedAt: new Date().toISOString(), command, passed: true, summary: summaryLine.trim() };
  } catch (err) {
    const e = err as { stdout?: string; stderr?: string };
    const output = `${e.stdout ?? ''}\n${e.stderr ?? ''}`.trim();
    return { generatedAt: new Date().toISOString(), command, passed: false, summary: output.slice(-500) };
  }
}

function listFilesRecursive(dir: string, exts: string[]): string[] {
  const out: string[] = [];
  let entries: string[];
  try {
    entries = readdirSync(dir);
  } catch {
    return out;
  }
  for (const entry of entries) {
    // dashboard is a separate sub-project (its own package.json/tsconfig/Vite build) —
    // its React source isn't part of this repo's CLI-tool attack surface.
    if (entry === 'node_modules' || entry === '.git' || entry === 'results' || entry === 'dashboard') continue;
    const abs = path.join(dir, entry);
    const st = statSync(abs);
    if (st.isDirectory()) out.push(...listFilesRecursive(abs, exts));
    else if (exts.some((e) => entry.endsWith(e))) out.push(path.relative(REPO_ROOT, abs).split(path.sep).join('/'));
  }
  return out;
}

export interface AttackSurfaceEntry {
  readonly surface: string;
  readonly description: string;
  readonly exposure: 'none' | 'local-process' | 'filesystem' | 'third-party-dependency';
}

/** Enumerates this prototype's real attack surface — no open network ports exist; see lib/security/manifest.ts networkInterfaces. */
export function buildAttackSurfaceMap(manifest: SecurityManifest): AttackSurfaceEntry[] {
  const cliEntryPoints = listFilesRecursive(path.join(REPO_ROOT, 'apps'), ['.ts'])
    .concat(listFilesRecursive(path.join(REPO_ROOT, 'tools'), ['.ts']));

  const entries: AttackSurfaceEntry[] = [
    { surface: 'Network listeners', description: 'None. No server/socket is opened by any script in this repo.', exposure: 'none' },
    {
      surface: 'CLI entry points',
      description: `${cliEntryPoints.length} scripts runnable via \`npx tsx\`/npm scripts (${cliEntryPoints.slice(0, 6).join(', ')}${cliEntryPoints.length > 6 ? ', …' : ''}). Attack surface here is local-process only: whoever can execute Node in this checkout.`,
      exposure: 'local-process',
    },
    {
      surface: 'Filesystem (results/)',
      description: 'Benchmark/demo/security tools read package.json and package-lock.json and write JSON/CSV under results/. No path comes from untrusted/network input.',
      exposure: 'filesystem',
    },
    {
      surface: `npm dependency tree (${manifest.dependencies.length} declared, ${manifest.dependencies.filter((d) => !d.dev).length} runtime)`,
      description: 'node-seal (runtime) and its WASM binary are the actual cryptographic trust base; a compromised upstream package is this prototype\'s largest realistic attack surface. See results/sbom.json for the full transitive tree.',
      exposure: 'third-party-dependency',
    },
  ];
  return entries;
}
