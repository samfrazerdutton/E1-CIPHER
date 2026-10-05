import { mkdirSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { buildSecurityManifest } from '../../lib/security/manifest.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const RESULTS_DIR = path.resolve(__dirname, '../../results');

function main() {
  const manifest = buildSecurityManifest();
  mkdirSync(RESULTS_DIR, { recursive: true });
  const outPath = path.join(RESULTS_DIR, 'security-manifest.json');
  writeFileSync(outPath, JSON.stringify(manifest, null, 2));

  console.log(`Wrote ${outPath}`);
  console.log('');
  console.log(`Product: ${manifest.product}@${manifest.version}`);
  console.log(`Git commit: ${manifest.build.gitCommit}`);
  console.log(`Crypto: ${manifest.cryptography.schemeFamily} via ${manifest.cryptography.library.name} (${manifest.cryptography.library.upstream})`);
  console.log(`Parameter sets: ${manifest.cryptography.parameterSets.map((p) => p.id).join(', ')}`);
  console.log(`Dependencies: ${manifest.dependencies.length} (${manifest.dependencies.filter((d) => !d.dev).length} runtime)`);
  console.log(`Known vulnerabilities: ${manifest.knownVulnerabilities}`);
}

main();
