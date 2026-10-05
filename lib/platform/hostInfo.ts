import os from 'node:os';
import { execFileSync } from 'node:child_process';

export interface HostInfo {
  readonly resultClass: 'HOST_REFERENCE';
  readonly platform: string;
  readonly arch: string;
  readonly osRelease: string;
  readonly cpuModel: string;
  readonly cpuCount: number;
  readonly totalMemoryBytes: number;
  readonly nodeVersion: string;
  readonly gitCommit: string;
  readonly capturedAt: string;
}

function tryGitCommit(): string {
  try {
    return execFileSync('git', ['rev-parse', 'HEAD'], { stdio: ['ignore', 'pipe', 'ignore'] }).toString().trim();
  } catch {
    return 'unknown (not a git checkout or git unavailable)';
  }
}

/**
 * Every number produced by this host counts as a HOST REFERENCE RESULT, not
 * a silicon measurement — see docs/README.md's results-labeling convention.
 * This function exists so that distinction is attached to benchmark
 * artifacts automatically instead of relying on someone remembering to add it.
 */
export function captureHostInfo(): HostInfo {
  const cpus = os.cpus();
  return {
    resultClass: 'HOST_REFERENCE',
    platform: os.platform(),
    arch: os.arch(),
    osRelease: os.release(),
    cpuModel: cpus[0]?.model ?? 'unknown',
    cpuCount: cpus.length,
    totalMemoryBytes: os.totalmem(),
    nodeVersion: process.version,
    gitCommit: tryGitCommit(),
    capturedAt: new Date().toISOString(),
  };
}
