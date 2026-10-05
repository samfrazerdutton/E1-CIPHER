// Copies this repo's real results/*-latest.json artifacts (and the CRA
// evidence bundle) into public/data/ so the dashboard can fetch them as
// static assets. Run automatically before `dev`/`build` (see package.json).
// This script copies — it never generates or edits a number.
import { copyFileSync, existsSync, mkdirSync, readdirSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const REPO_RESULTS = path.resolve(__dirname, '../../../results');
const OUT_DIR = path.resolve(__dirname, '../public/data');

mkdirSync(OUT_DIR, { recursive: true });

if (!existsSync(REPO_RESULTS)) {
  console.error(`No results/ directory at ${REPO_RESULTS} — run the repo's npm run bench:*/demo:*/redteam:*/security:* scripts first.`);
  process.exit(1);
}

const files = readdirSync(REPO_RESULTS).filter((f) => f.endsWith('-latest.json') || f === 'security-manifest.json' || f === 'cra-evidence.json');

if (files.length === 0) {
  console.error('No *-latest.json / security-manifest.json / cra-evidence.json files found in results/.');
  process.exit(1);
}

for (const f of files) {
  copyFileSync(path.join(REPO_RESULTS, f), path.join(OUT_DIR, f));
  console.log(`synced ${f}`);
}
