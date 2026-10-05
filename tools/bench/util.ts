import { mkdirSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { randomUUID } from 'node:crypto';
import { captureHostInfo, type HostInfo } from '../../lib/platform/hostInfo.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
export const RESULTS_DIR = path.resolve(__dirname, '../../results');

export interface TimingStats {
  readonly trials: number;
  readonly meanMs: number;
  readonly medianMs: number;
  readonly stdDevMs: number;
  readonly minMs: number;
  readonly maxMs: number;
}

/**
 * Times `fn` for `trials` iterations after `warmup` untimed iterations (to
 * let the WASM JIT/SEAL internal caches settle). Each call to `fn` should
 * perform exactly one unit of work; side effects (output buffers) should be
 * allocated inside `fn` so allocation cost is included, matching how the
 * real pipeline would behave call-to-call.
 */
export function timeOp(fn: () => void, trials: number, warmup = Math.max(3, Math.floor(trials / 5))): TimingStats {
  for (let i = 0; i < warmup; i++) fn();

  const samplesMs: number[] = [];
  for (let i = 0; i < trials; i++) {
    const start = process.hrtime.bigint();
    fn();
    const end = process.hrtime.bigint();
    samplesMs.push(Number(end - start) / 1e6);
  }

  const mean = samplesMs.reduce((a, b) => a + b, 0) / samplesMs.length;
  const sorted = [...samplesMs].sort((a, b) => a - b);
  const median = sorted[Math.floor(sorted.length / 2)];
  const variance = samplesMs.reduce((acc, v) => acc + (v - mean) ** 2, 0) / samplesMs.length;

  return {
    trials,
    meanMs: mean,
    medianMs: median,
    stdDevMs: Math.sqrt(variance),
    minMs: sorted[0],
    maxMs: sorted[sorted.length - 1],
  };
}

export interface ExperimentMetadata {
  readonly experimentId: string;
  readonly timestamp: string;
  readonly host: HostInfo;
}

export function experimentMetadata(): ExperimentMetadata {
  return {
    experimentId: randomUUID(),
    timestamp: new Date().toISOString(),
    host: captureHostInfo(),
  };
}

/** Writes both a timestamped artifact and a "-latest" copy, as JSON and (if rows given) CSV. */
export function writeArtifact(name: string, data: unknown, csvRows?: { header: string[]; rows: (string | number)[][] }) {
  mkdirSync(RESULTS_DIR, { recursive: true });
  const stamp = new Date().toISOString().replace(/[:.]/g, '-');

  const jsonBody = JSON.stringify(data, null, 2);
  writeFileSync(path.join(RESULTS_DIR, `${name}-${stamp}.json`), jsonBody);
  writeFileSync(path.join(RESULTS_DIR, `${name}-latest.json`), jsonBody);

  if (csvRows) {
    const csv = [csvRows.header.join(','), ...csvRows.rows.map((r) => r.join(','))].join('\n');
    writeFileSync(path.join(RESULTS_DIR, `${name}-${stamp}.csv`), csv);
    writeFileSync(path.join(RESULTS_DIR, `${name}-latest.csv`), csv);
  }

  console.log(`Wrote results/${name}-latest.{json${csvRows ? ',csv' : ''}} (and timestamped copy)`);
}
