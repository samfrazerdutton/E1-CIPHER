/**
 * CKKS operation benchmark suite (section "Build a CKKS workload suite").
 *
 * For every parameter set in lib/ckks/paramSets.ts, measures real wall-clock
 * latency (via tools/bench/util.ts timeOp, which is a HOST REFERENCE
 * measurement — see docs/README.md) for: encode, encrypt, add, multiply
 * (raw), relinearize, rescale, rotate, decrypt, decode. Also measures
 * ciphertext serialized size and round-trip accuracy.
 *
 * Multiplicative-depth-limited parameter sets (N1024 has no second modulus)
 * explicitly skip multiply/relinearize/rescale and record why, rather than
 * crashing or silently omitting the row — "Do not assume all configurations
 * fit the target architecture. Report failures honestly."
 */
import { buildCkksContext } from '../../lib/ckks/context.js';
import { CKKS_PARAM_SETS, totalCoeffModulusBits, type CkksParamSet } from '../../lib/ckks/paramSets.js';
import { timeOp, experimentMetadata, writeArtifact, type TimingStats } from './util.js';

const TRIALS = 30;
const TEST_VECTOR_SEED = 42;

function makeTestVector(slotCount: number, seed: number): Float64Array {
  // Deterministic pseudo-random values in [-1, 1], reproducible across runs.
  let state = seed >>> 0;
  const next = () => {
    state = (Math.imul(state, 1103515245) + 12345) >>> 0;
    return state / 4294967296;
  };
  return Float64Array.from({ length: slotCount }, () => next() * 2 - 1);
}

function maxAbsError(a: Float64Array, b: Float64Array): number {
  let m = 0;
  for (let i = 0; i < a.length; i++) m = Math.max(m, Math.abs(a[i] - b[i]));
  return m;
}

interface OpResult {
  readonly supported: boolean;
  readonly reason?: string;
  readonly timing?: TimingStats;
}

interface ParamSetResult {
  readonly paramSet: CkksParamSet;
  readonly parametersSet: boolean;
  readonly maxBitsAt128: number;
  readonly requestedBits: number;
  readonly slotCount: number;
  readonly ops: Record<string, OpResult>;
  readonly ciphertextBytes?: number;
  readonly accuracy?: { readonly encodeEncryptDecryptDecodeMaxAbsError: number; readonly multiplyMaxAbsError?: number };
  readonly bottleneck?: { readonly op: string; readonly meanMs: number; readonly shareOfMeasuredTotal: number }[];
}

async function benchmarkParamSet(paramSet: CkksParamSet): Promise<ParamSetResult> {
  const requestedBits = totalCoeffModulusBits(paramSet);
  const ctx = await buildCkksContext(paramSet);

  if (!ctx.parametersSet) {
    return {
      paramSet,
      parametersSet: false,
      maxBitsAt128: ctx.maxBitsAt128,
      requestedBits,
      slotCount: 0,
      ops: {},
    };
  }

  const { seal, encoder, encryptor, decryptor, evaluator, relinKeys, galoisKeys, slotCount, usingKeyswitching } = ctx;
  const scale = Math.pow(2, paramSet.scaleBits);

  const data1 = makeTestVector(slotCount, TEST_VECTOR_SEED);
  const data2 = makeTestVector(slotCount, TEST_VECTOR_SEED + 1);

  const ops: Record<string, OpResult> = {};

  // --- encode ---
  let plain1!: InstanceType<typeof seal.Plaintext>;
  ops.encode = {
    supported: true,
    timing: timeOp(() => {
      plain1 = new seal.Plaintext();
      encoder.encode(data1, scale, plain1);
    }, TRIALS),
  };
  const plain2 = new seal.Plaintext();
  encoder.encode(data2, scale, plain2);

  // --- encrypt ---
  let cipher1!: InstanceType<typeof seal.Ciphertext>;
  ops.encrypt = {
    supported: true,
    timing: timeOp(() => {
      cipher1 = new seal.Ciphertext();
      encryptor.encrypt(plain1, cipher1);
    }, TRIALS),
  };
  const cipher2 = new seal.Ciphertext();
  encryptor.encrypt(plain2, cipher2);

  // --- add ---
  ops.add = {
    supported: true,
    timing: timeOp(() => {
      const out = new seal.Ciphertext();
      evaluator.add(cipher1, cipher2, out);
    }, TRIALS),
  };

  // --- multiply / relinearize / rescale / rotate: need key-switching support ---
  let productForAccuracy: InstanceType<typeof seal.Ciphertext> | undefined;
  if (usingKeyswitching && relinKeys && galoisKeys) {
    ops.multiply = {
      supported: true,
      timing: timeOp(() => {
        const out = new seal.Ciphertext();
        evaluator.multiply(cipher1, cipher2, out);
      }, TRIALS),
    };

    const freshProduct = () => {
      const out = new seal.Ciphertext();
      evaluator.multiply(cipher1, cipher2, out);
      return out;
    };

    ops.relinearize = {
      supported: true,
      timing: timeOp(() => {
        const p = freshProduct();
        evaluator.relinearizeInplace(p, relinKeys);
      }, TRIALS),
    };

    ops.rescale = {
      supported: true,
      timing: timeOp(() => {
        const p = freshProduct();
        evaluator.relinearizeInplace(p, relinKeys);
        evaluator.rescaleToNextInplace(p);
      }, TRIALS),
    };

    productForAccuracy = freshProduct();
    evaluator.relinearizeInplace(productForAccuracy, relinKeys);
    evaluator.rescaleToNextInplace(productForAccuracy);

    ops.rotate = {
      supported: true,
      timing: timeOp(() => {
        const out = new seal.Ciphertext();
        evaluator.rotateVector(cipher1, 1, galoisKeys, out);
      }, TRIALS),
    };
  } else {
    const reason = `Parameter set has no key-switching support (single-modulus chain, ${requestedBits}-bit budget); ` +
      'relin/Galois keys cannot be generated, so multiply/relinearize/rescale/rotate are not attempted.';
    ops.multiply = { supported: false, reason };
    ops.relinearize = { supported: false, reason };
    ops.rescale = { supported: false, reason };
    ops.rotate = { supported: false, reason };
  }

  // --- decrypt / decode ---
  ops.decrypt = {
    supported: true,
    timing: timeOp(() => {
      const p = new seal.Plaintext();
      decryptor.decrypt(cipher1, p);
    }, TRIALS),
  };
  const decryptedPlain = new seal.Plaintext();
  decryptor.decrypt(cipher1, decryptedPlain);
  ops.decode = {
    supported: true,
    timing: timeOp(() => {
      encoder.decodeFloat64(decryptedPlain);
    }, TRIALS),
  };

  // --- ciphertext size ---
  const ciphertextBytes = (cipher1.saveToArray(seal.ComprModeType.none) as Uint8Array).byteLength;

  // --- accuracy ---
  const roundTrip = encoder.decodeFloat64(decryptedPlain) as Float64Array;
  const encodeEncryptDecryptDecodeMaxAbsError = maxAbsError(data1, roundTrip.subarray(0, data1.length));

  let multiplyMaxAbsError: number | undefined;
  if (productForAccuracy) {
    const prodPlain = new seal.Plaintext();
    decryptor.decrypt(productForAccuracy, prodPlain);
    const prodDecoded = encoder.decodeFloat64(prodPlain) as Float64Array;
    const expected = Float64Array.from(data1, (v, i) => v * data2[i]);
    multiplyMaxAbsError = maxAbsError(expected, prodDecoded.subarray(0, expected.length));
  }

  // --- bottleneck decomposition (measured, not invented) ---
  const measuredOps = Object.entries(ops).filter(([, r]) => r.supported && r.timing) as [string, OpResult & { timing: TimingStats }][];
  const totalMeanMs = measuredOps.reduce((acc, [, r]) => acc + r.timing.meanMs, 0);
  const bottleneck = measuredOps
    .map(([op, r]) => ({ op, meanMs: r.timing.meanMs, shareOfMeasuredTotal: r.timing.meanMs / totalMeanMs }))
    .sort((a, b) => b.meanMs - a.meanMs);

  return {
    paramSet,
    parametersSet: true,
    maxBitsAt128: ctx.maxBitsAt128,
    requestedBits,
    slotCount,
    ops,
    ciphertextBytes,
    accuracy: { encodeEncryptDecryptDecodeMaxAbsError, multiplyMaxAbsError },
    bottleneck,
  };
}

async function main() {
  const meta = experimentMetadata();
  console.log(`CKKS benchmark suite — experiment ${meta.experimentId}`);
  console.log(`Host: ${meta.host.cpuModel} x${meta.host.cpuCount}, node ${meta.host.nodeVersion}, commit ${meta.host.gitCommit}`);
  console.log('All timings below are HOST REFERENCE RESULTS (this CPU, via node-seal/WASM) — not E1 silicon measurements.\n');

  const results: ParamSetResult[] = [];
  for (const paramSet of CKKS_PARAM_SETS) {
    console.log(`--- ${paramSet.id} (N=${paramSet.polyModulusDegree}, chain=[${paramSet.coeffModulusBits.join(',')}]) ---`);
    const r = await benchmarkParamSet(paramSet);
    results.push(r);

    if (!r.parametersSet) {
      console.log(`  FAILED: parametersSet()=false. Requested ${r.requestedBits} bits > ${r.maxBitsAt128}-bit budget at 128-bit security.\n`);
      continue;
    }

    console.log(`  slotCount=${r.slotCount}, ciphertextBytes=${r.ciphertextBytes}`);
    for (const [op, res] of Object.entries(r.ops)) {
      if (res.supported && res.timing) {
        console.log(`  ${op.padEnd(12)} mean=${res.timing.meanMs.toFixed(3)}ms  median=${res.timing.medianMs.toFixed(3)}ms  stddev=${res.timing.stdDevMs.toFixed(3)}ms`);
      } else {
        console.log(`  ${op.padEnd(12)} N/A — ${res.reason}`);
      }
    }
    if (r.accuracy) {
      console.log(`  accuracy: roundtrip maxAbsErr=${r.accuracy.encodeEncryptDecryptDecodeMaxAbsError.toExponential(3)}` +
        (r.accuracy.multiplyMaxAbsError !== undefined ? `, multiply maxAbsErr=${r.accuracy.multiplyMaxAbsError.toExponential(3)}` : ''));
    }
    if (r.bottleneck) {
      console.log('  bottleneck (share of measured per-op time, this param set):');
      for (const b of r.bottleneck) {
        console.log(`    ${b.op.padEnd(12)} ${(b.shareOfMeasuredTotal * 100).toFixed(1)}%`);
      }
    }
    console.log('');
  }

  const csvRows: (string | number)[][] = [];
  for (const r of results) {
    if (!r.parametersSet) {
      csvRows.push([r.paramSet.id, r.paramSet.polyModulusDegree, 'FAILED', '', '', '', '', '']);
      continue;
    }
    for (const [op, res] of Object.entries(r.ops)) {
      csvRows.push([
        r.paramSet.id,
        r.paramSet.polyModulusDegree,
        op,
        res.supported ? 'ok' : 'unsupported',
        res.timing?.meanMs.toFixed(4) ?? '',
        res.timing?.medianMs.toFixed(4) ?? '',
        res.timing?.stdDevMs.toFixed(4) ?? '',
        r.ciphertextBytes ?? '',
      ]);
    }
  }

  writeArtifact(
    'ckks-bench',
    { experiment: meta, trialsPerOp: TRIALS, results },
    { header: ['paramSetId', 'polyModulusDegree', 'op', 'status', 'meanMs', 'medianMs', 'stdDevMs', 'ciphertextBytes'], rows: csvRows },
  );
}

main().catch((err) => {
  console.error(err);
  process.exitCode = 1;
});
