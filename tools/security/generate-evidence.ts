/**
 * Generates the CRA-readiness evidence set (project brief sections 12-13)
 * into docs/cra-evidence/*.md + evidence.json. This is evidence-generation
 * tooling for a security risk assessment, not a compliance claim — every
 * document says "CRA readiness evidence", never "CRA compliant/certified".
 */
import { mkdirSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { buildSecurityManifest } from '../../lib/security/manifest.js';
import { buildAssetInventory, buildAttackSurfaceMap, runSecurityRelevantTests, runVulnerabilityScan } from '../../lib/security/evidence.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const REPO_ROOT = path.resolve(__dirname, '../..');
const EVIDENCE_DIR = path.join(REPO_ROOT, 'docs', 'cra-evidence');
const RESULTS_DIR = path.join(REPO_ROOT, 'results');

function write(name: string, content: string) {
  writeFileSync(path.join(EVIDENCE_DIR, name), content);
  console.log(`  wrote docs/cra-evidence/${name}`);
}

async function main() {
  mkdirSync(EVIDENCE_DIR, { recursive: true });
  mkdirSync(RESULTS_DIR, { recursive: true });

  console.log('Generating CRA readiness evidence set (not a compliance claim) ...');

  const manifest = buildSecurityManifest();
  const assets = buildAssetInventory();
  const attackSurface = buildAttackSurfaceMap(manifest);
  console.log('  running npm audit (real scan of this repo\'s lockfile) ...');
  const vulnReport = runVulnerabilityScan();
  console.log('  running npm test (real test run) ...');
  const testRun = runSecurityRelevantTests();

  // 1. Asset inventory
  write(
    '01-asset-inventory.md',
    [
      '# Asset Inventory',
      '',
      '_Generated from the actual repository file tree — not a template. Regenerate with `npm run security:evidence`._',
      '',
      '| Path | Kind | Description |',
      '|---|---|---|',
      ...assets.map((a) => `| \`${a.path}\` | ${a.kind} | ${a.description} |`),
    ].join('\n') + '\n',
  );

  // 2. Dependency inventory
  write(
    '02-dependency-inventory.md',
    [
      '# Dependency Inventory',
      '',
      `_Generated from this repo's actual \`package.json\` at commit \`${manifest.build.gitCommit}\`. Full transitive tree: \`results/sbom.json\` (CycloneDX, \`npm run security:sbom\`)._`,
      '',
      '**Scope note:** this covers the root `package.json` only. `apps/dashboard` is a separate npm sub-project ' +
        '(its own `package.json`/`package-lock.json`, React/Vite toolchain) with its own dependency tree, not ' +
        'included here or in `results/sbom.json`. Run `cd apps/dashboard && npx cyclonedx-npm` for its SBOM if needed.',
      '',
      '| Package | Declared version | Runtime or dev |',
      '|---|---|---|',
      ...manifest.dependencies.map((d) => `| \`${d.name}\` | \`${d.version}\` | ${d.dev ? 'dev' : 'runtime'} |`),
    ].join('\n') + '\n',
  );

  // 3. Cryptographic inventory / configuration record
  write(
    '03-cryptographic-configuration-record.md',
    [
      '# Cryptographic Inventory & Configuration Record',
      '',
      `_Scheme family: ${manifest.cryptography.schemeFamily}. Library: ${manifest.cryptography.library.name} (${manifest.cryptography.library.upstream})._`,
      '',
      '## Parameter sets in use (`lib/ckks/paramSets.ts`)',
      '',
      '| ID | N (poly modulus degree) | Coeff modulus chain (bits) | Total bits | Scale (bits) |',
      '|---|---|---|---|---|',
      ...manifest.cryptography.parameterSets.map(
        (p) => `| ${p.id} | ${p.polyModulusDegree} | [${p.coeffModulusBits.join(', ')}] | ${p.totalCoeffModulusBits} | ${p.scaleBits} |`,
      ),
      '',
      '## Other registered crypto backends (`lib/ckks/backend.ts`)',
      '',
      ...manifest.cryptography.otherBackends.map((b) => `- ${b}`),
      '',
      '## Key custody model',
      '',
      '- Drones hold only the CKKS **public** key (`Encryptor`).',
      '- The fleet operator alone holds the **secret** key (`Decryptor`).',
      '- The aggregator role holds neither key — only `Ciphertext` objects (see `apps/fleet/compare.ts`, `apps/demo/killer-demo.ts`).',
      '- No key rotation, HSM integration, or key-escrow mechanism is implemented in this prototype.',
    ].join('\n') + '\n',
  );

  // 4. Vulnerability register
  write(
    '04-vulnerability-register.md',
    [
      '# Vulnerability Register',
      '',
      `_Generated ${vulnReport.generatedAt} via ${vulnReport.tool}. Real scan output, not a placeholder — audit process exit code ${vulnReport.rawExitCode}._`,
      '',
      `**Totals by severity:** ${JSON.stringify(vulnReport.totalsBySeverity)}`,
      '',
      vulnReport.entries.length === 0
        ? '_No known vulnerabilities reported by `npm audit` against the current lockfile at generation time. This is a point-in-time result — regenerate before any release decision._'
        : [
            '| Package | Severity | Via | Vulnerable range |',
            '|---|---|---|---|',
            ...vulnReport.entries.map((e) => `| \`${e.name}\` | ${e.severity} | ${e.via} | ${e.range} |`),
          ].join('\n'),
    ].join('\n') + '\n',
  );

  // 5. Security update policy
  write(
    '05-security-update-policy.md',
    [
      '# Security Update Policy',
      '',
      '**Current state: none implemented.** This is a research prototype distributed as source via git, not a',
      'shipped product. There is no signed-update mechanism, no OTA channel, and no version-pinning guidance',
      'issued to downstream consumers.',
      '',
      'What a real product built on this codebase would need before any security-update-policy claim could be',
      'supported:',
      '',
      '- A signed release/build pipeline (this repo has none — see `08-build-reproducibility-record.md`).',
      '- A defined patch cadence and severity-to-SLA mapping (e.g. critical within 7 days).',
      '- A notification channel for downstream consumers (mailing list, security advisory feed, etc).',
      '',
      'This document exists so that gap is visible, not hidden.',
    ].join('\n') + '\n',
  );

  // 6. Support period
  write(
    '06-support-period.md',
    [
      '# Support Period',
      '',
      '**No committed support period.** This is a single-session research prototype (see the main README\'s',
      '"Not built this session" list). There is no maintenance commitment, no end-of-life date, and no LTS branch.',
      '',
      'Dependency freshness at generation time: see `02-dependency-inventory.md` and `results/sbom.json` for exact',
      'declared versions; re-run `npm outdated` for current upstream status (not captured here to avoid a stale,',
      'point-in-time claim masquerading as a live one).',
    ].join('\n') + '\n',
  );

  // 7. Incident response procedure (template)
  write(
    '07-incident-response-procedure.md',
    [
      '# Incident Response Procedure (template)',
      '',
      '**This is a draft/example procedure, not an operating capability.** No on-call rotation, ticketing',
      'integration, or disclosure mailbox actually exists for this prototype. It is included so a team adopting',
      'this codebase has a starting skeleton, not because this project runs one today.',
      '',
      '1. **Intake** — a report arrives (e.g. a vulnerability in `node-seal` or in code in this repo).',
      '2. **Triage** — classify severity using the same bands as `04-vulnerability-register.md`',
      '   (info/low/moderate/high/critical, per `npm audit`\'s scale where applicable).',
      '3. **Containment** — for a dependency vulnerability: pin/patch via `package-lock.json`, rerun',
      '   `npm run security:sbom` and `npm run security:evidence` to regenerate evidence. For a logic/crypto',
      '   issue: identify the affected parameter set(s) in `lib/ckks/paramSets.ts` or policy rule in',
      '   `lib/policy/scheduler.ts`.',
      '4. **Remediation** — fix, add a regression test under `lib/**/*.test.ts`, rerun `npm test`.',
      '5. **Disclosure** — for a real deployment: notify affected downstream consumers per',
      '   `05-security-update-policy.md` (not yet implemented here).',
      '6. **Postmortem** — record root cause and update `docs/threat-model.md` if a new threat class was involved.',
    ].join('\n') + '\n',
  );

  // 8. Build / reproducibility record
  write(
    '08-build-reproducibility-record.md',
    [
      '# Build / Reproducibility Record',
      '',
      `- Git commit: \`${manifest.build.gitCommit}\``,
      `- Node version: \`${manifest.build.nodeVersion}\``,
      `- Platform: \`${manifest.build.platform}\``,
      `- \`package-lock.json\` SHA-256: \`${manifest.build.packageLockHashSha256}\``,
      `- Generated: ${manifest.generatedAt}`,
      '',
      '## Reproduction',
      '',
      '```sh',
      'npm install          # uses the exact package-lock.json hashed above',
      'npm run bench:ckks',
      'npm run bench:kernels',
      'npm run fleet:compare',
      'npm run demo:killer',
      'npm run redteam:intercept',
      'npm run security:sbom',
      'npm run security:manifest',
      'npm run security:evidence   # this document',
      '```',
      '',
      'Every `results/*.json` artifact embeds its own experiment ID, git commit, and host info',
      '(`lib/platform/hostInfo.ts`) independent of this record.',
    ].join('\n') + '\n',
  );

  // 9. Security test results
  write(
    '09-security-test-results.md',
    [
      '# Security Test Results',
      '',
      `_Generated ${testRun.generatedAt} by actually running \`${testRun.command}\` — not a recorded/assumed pass._`,
      '',
      `**Result: ${testRun.passed ? 'PASS' : 'FAIL'}**`,
      '',
      '```',
      testRun.summary,
      '```',
      '',
      'Scope note: this repo\'s test suite (`lib/kernels/*.test.ts`) covers cryptographic-kernel *correctness*',
      '(NTT round-trip, convolution-vs-naive match, Montgomery-vs-naive modular multiplication match) — it is not',
      'a penetration test, a fuzzing campaign, or a formal security audit. See `docs/threat-model.md` for what has',
      'and has not been assessed.',
    ].join('\n') + '\n',
  );

  // 10. Attack surface map
  write(
    '10-attack-surface-map.md',
    [
      '# Attack Surface Map',
      '',
      '| Surface | Exposure | Description |',
      '|---|---|---|',
      ...attackSurface.map((a) => `| ${a.surface} | ${a.exposure} | ${a.description} |`),
      '',
      'See `docs/threat-model.md` for the corresponding threat register and `lib/security/manifest.ts`\'s',
      '`networkInterfaces` field for the machine-readable version of the "no open network ports" claim above.',
    ].join('\n') + '\n',
  );

  // 11. Data flow / trust boundary diagram
  write(
    '11-data-flow-and-trust-boundary-diagram.md',
    [
      '# Data Flow & Trust Boundary Diagram',
      '',
      '```mermaid',
      'flowchart TB',
      '  subgraph DRONE[Drone -- holds PUBLIC key only]',
      '    SENSOR[Raw sensors: camera/LiDAR/IMU] -->|LOCAL_ONLY, never leaves| FUSION[Sensor fusion]',
      '    FUSION --> FEATURE[Feature vector, 6 scalars]',
      '    FEATURE --> POLICY[Policy engine: classify + place]',
      '    POLICY -->|ENCRYPTED/AGGREGATABLE| ENCRYPT[CKKS encrypt]',
      '    POLICY -->|PLAINTEXT, low sensitivity| PLAINSEND[Plaintext send]',
      '  end',
      '  subgraph BOUNDARY1[" "]',
      '  end',
      '  subgraph AGGREGATOR[Aggregator -- UNTRUSTED, holds NO key]',
      '    ENCRYPT -->|ciphertext only| SUM[Homomorphic sum across fleet]',
      '    PLAINSEND -->|plaintext, e.g. battery| DASH[Fleet health dashboard]',
      '  end',
      '  subgraph BOUNDARY2[" "]',
      '  end',
      '  subgraph OPERATOR[Fleet operator -- holds SECRET key]',
      '    SUM -->|aggregate ciphertext| DECRYPT[Decrypt]',
      '    DECRYPT --> MEAN[Fleet-wide mean/statistic only]',
      '  end',
      '```',
      '',
      'Trust boundaries (the horizontal `BOUNDARY` rows above): crossing **drone -> aggregator** exposes only',
      'ciphertext bytes and explicitly-plaintext-classified fields (see `lib/policy/classification.ts`);',
      'crossing **aggregator -> operator** is where decryption actually happens, and only on the aggregate, never',
      'on an individual drone\'s ciphertext. Compare against `docs/threat-model.md` T1-T3 and',
      '`apps/redteam/intercept-demo.ts` for a concrete demonstration of what crosses each boundary.',
    ].join('\n') + '\n',
  );

  const indexPath = path.join(EVIDENCE_DIR, 'README.md');
  writeFileSync(
    indexPath,
    [
      '# CRA Readiness Evidence Set',
      '',
      '**CRA readiness evidence, not a compliance or certification claim.** This directory is generated —',
      'regenerate with `npm run security:evidence` — and documents what a product team could point to when',
      'building an actual Cyber Resilience Act risk assessment, plus the gaps that remain.',
      '',
      '1. [Asset Inventory](01-asset-inventory.md)',
      '2. [Dependency Inventory](02-dependency-inventory.md)',
      '3. [Cryptographic Configuration Record](03-cryptographic-configuration-record.md)',
      '4. [Vulnerability Register](04-vulnerability-register.md)',
      '5. [Security Update Policy](05-security-update-policy.md)',
      '6. [Support Period](06-support-period.md)',
      '7. [Incident Response Procedure (template)](07-incident-response-procedure.md)',
      '8. [Build / Reproducibility Record](08-build-reproducibility-record.md)',
      '9. [Security Test Results](09-security-test-results.md)',
      '10. [Attack Surface Map](10-attack-surface-map.md)',
      '11. [Data Flow & Trust Boundary Diagram](11-data-flow-and-trust-boundary-diagram.md)',
      '',
      'See also `../threat-model.md` (full threat register) and `../../lib/security/manifest.ts` /',
      '`results/security-manifest.json` (machine-readable companion).',
    ].join('\n') + '\n',
  );
  console.log('  wrote docs/cra-evidence/README.md (index)');

  writeFileSync(
    path.join(RESULTS_DIR, 'cra-evidence.json'),
    JSON.stringify({ manifest, assets, attackSurface, vulnReport, testRun }, null, 2),
  );
  console.log('  wrote results/cra-evidence.json (machine-readable bundle)');

  console.log(`\nDone. ${testRun.passed ? 'Tests passed' : 'TESTS FAILED — see 09-security-test-results.md'}.`);
}

main().catch((err) => {
  console.error(err);
  process.exitCode = 1;
});
