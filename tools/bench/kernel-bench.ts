/**
 * Custom kernel micro-benchmarks (section "Write custom high-performance
 * kernels"). These measure lib/kernels — reference modular arithmetic and a
 * from-scratch radix-2 NTT — NOT SEAL's internals (which aren't exposed by
 * node-seal as separately callable). HOST REFERENCE RESULTS only.
 *
 * Purpose: give the bottleneck/E1-mapping discussion in docs/README.md real
 * timing data on the O(N log N) butterfly-network shape of NTT and on
 * naive-vs-Montgomery modular multiplication, at the same N values used by
 * the CKKS parameter sets — not a substitute for the macro CKKS benchmark.
 */
import { NTT_PRIME, ntt } from '../../lib/kernels/ntt.js';
import { MontgomeryContext, modMulScalar, modMulVectorMontgomery, modMulVectorScalar } from '../../lib/kernels/modular.js';
import { CKKS_PARAM_SETS } from '../../lib/ckks/paramSets.js';
import { timeOp, experimentMetadata, writeArtifact } from './util.js';

const TRIALS = 20;

function randomVector(n: number, mod: bigint, seed: number): bigint[] {
  let state = seed >>> 0;
  const next = () => {
    state = (Math.imul(state, 1103515245) + 12345) >>> 0;
    return state;
  };
  return Array.from({ length: n }, () => BigInt(next()) % mod);
}

async function main() {
  const meta = experimentMetadata();
  console.log(`Kernel benchmark suite — experiment ${meta.experimentId}`);
  console.log(`Host: ${meta.host.cpuModel} x${meta.host.cpuCount}, node ${meta.host.nodeVersion}`);
  console.log('HOST REFERENCE RESULTS — scalar TS/BigInt kernels, not SEAL internals, not E1 silicon.\n');

  const sizes = CKKS_PARAM_SETS.map((p) => p.polyModulusDegree);
  const montCtx = new MontgomeryContext(NTT_PRIME, 64n);

  const rows: (string | number)[][] = [];
  const results: unknown[] = [];

  for (const n of sizes) {
    const a = randomVector(n, NTT_PRIME, 1);
    const b = randomVector(n, NTT_PRIME, 2);

    const modMulNaive = timeOp(() => {
      modMulVectorScalar(a, b, NTT_PRIME);
    }, TRIALS);

    const modMulMont = timeOp(() => {
      modMulVectorMontgomery(a, b, montCtx);
    }, TRIALS);

    const nttForward = timeOp(() => {
      ntt(a, false);
    }, TRIALS);

    const { stats: nttStats } = ntt(a, false);

    console.log(`N=${n}:`);
    console.log(`  modMulVector (naive BigInt %)      mean=${modMulNaive.meanMs.toFixed(3)}ms  (${(n / modMulNaive.meanMs * 1000).toFixed(0)} mults/s)`);
    console.log(`  modMulVector (Montgomery)          mean=${modMulMont.meanMs.toFixed(3)}ms  (${(n / modMulMont.meanMs * 1000).toFixed(0)} mults/s)`);
    console.log(`  NTT forward (radix-2, N log N)     mean=${nttForward.meanMs.toFixed(3)}ms  butterflyOps=${nttStats.butterflyOps} stages=${nttStats.stages}`);
    console.log(`  Montgomery speedup vs naive: ${(modMulNaive.meanMs / modMulMont.meanMs).toFixed(2)}x\n`);

    rows.push([n, 'modMulNaive', modMulNaive.meanMs.toFixed(4), modMulNaive.stdDevMs.toFixed(4)]);
    rows.push([n, 'modMulMontgomery', modMulMont.meanMs.toFixed(4), modMulMont.stdDevMs.toFixed(4)]);
    rows.push([n, 'nttForward', nttForward.meanMs.toFixed(4), nttForward.stdDevMs.toFixed(4)]);

    results.push({ n, modMulNaive, modMulMont, nttForward, nttStats });
  }

  writeArtifact(
    'kernel-bench',
    { experiment: meta, trialsPerOp: TRIALS, nttPrime: NTT_PRIME.toString(), results },
    { header: ['n', 'kernel', 'meanMs', 'stdDevMs'], rows },
  );
}

main().catch((err) => {
  console.error(err);
  process.exitCode = 1;
});
