/**
 * Fleet aggregation comparison: plaintext vs CKKS-encrypted (section "Multi-
 * drone federated intelligence" / "The no raw data leaves the drone demo").
 *
 * Fleet sizes: 1, 5, 10, 25, 50. For each, every drone's local sensor-fusion
 * feature vector (lib/sensor_fusion — altitude, speed, vibration, temp,
 * battery, infra-anomaly score; see lib/policy for why raw sensors never
 * leave the device) is aggregated two ways:
 *
 *  - PLAINTEXT: drones send the 6 floats in the clear; the aggregator sums
 *    and divides by fleet size. Fast, tiny, but the aggregator sees every
 *    drone's raw readings.
 *  - CKKS: each drone encrypts its feature vector; the aggregator
 *    homomorphically sums the ciphertexts and returns one aggregate
 *    ciphertext, which only the key holder can decrypt. The aggregator
 *    never sees a single drone's plaintext reading — only the final mean,
 *    post-decryption, downstream.
 *
 * This is an in-process simulation — no sockets are opened (see
 * lib/security/manifest.ts "networkInterfaces"). Reported bytes are
 * serialized payload sizes; reported latency is real measured compute
 * (encode/encrypt/add/decrypt/decode), not a simulated network RTT. Both
 * are HOST REFERENCE RESULTS.
 *
 * Honest tradeoff this script is expected to surface (do not hide it):
 * CKKS does NOT reduce per-drone upload bandwidth — a 6-float ciphertext is
 * vastly larger than 6 plaintext floats, because each drone sends its own
 * independent ciphertext regardless of how many unused slots it has. CKKS's
 * benefit here is confidentiality of the aggregator's view, not bandwidth.
 */
import { createCkksBackend } from '../../lib/ckks/ckksBackend.js';
import { createPlaintextBackend } from '../../lib/ckks/plaintextBackend.js';
import { CKKS_PARAM_SETS } from '../../lib/ckks/paramSets.js';
import { generateFleetTick } from '../../lib/telemetry/generator.js';
import { extractFeatureVector, FEATURE_FIELDS } from '../../lib/sensor_fusion/features.js';
import { experimentMetadata, writeArtifact } from '../../tools/bench/util.js';

const FLEET_SIZES = [1, 5, 10, 25, 50];
const TRIALS = 5;
const PARAM_SET = CKKS_PARAM_SETS.find((p) => p.id === 'N4096')!;

function median(xs: number[]): number {
  const s = [...xs].sort((a, b) => a - b);
  return s[Math.floor(s.length / 2)];
}

function maxAbsError(a: Float64Array, b: Float64Array): number {
  let m = 0;
  for (let i = 0; i < a.length; i++) m = Math.max(m, Math.abs(a[i] - b[i]));
  return m;
}

interface FleetSizeResult {
  readonly fleetSize: number;
  readonly plaintext: { readonly latencyMs: number; readonly bytes: number; readonly mean: number[] };
  readonly ckks: {
    readonly encryptAllMs: number;
    readonly encryptCriticalPathMs: number;
    readonly aggregateMs: number;
    readonly decryptDecodeMs: number;
    readonly totalMs: number;
    readonly bytes: number;
    readonly mean: number[];
    readonly maxAbsErrorVsPlaintext: number;
  };
}

async function main() {
  const meta = experimentMetadata();
  console.log(`Fleet aggregation comparison — experiment ${meta.experimentId}`);
  console.log(`CKKS parameter set: ${PARAM_SET.id} (N=${PARAM_SET.polyModulusDegree})`);
  console.log('In-process simulation: no sockets opened, no real network RTT included. HOST REFERENCE RESULTS.\n');

  const backend = await createCkksBackend(PARAM_SET);
  const plaintextBackend = createPlaintextBackend(backend.slotCount);

  const results: FleetSizeResult[] = [];

  for (const fleetSize of FLEET_SIZES) {
    const samples = generateFleetTick({ seed: 7, fleetSize, tick: 0, scenario: 'normal' });
    const vectors = samples.map(extractFeatureVector);

    const plaintextTrials: number[] = [];
    let plaintextMean = new Float64Array(FEATURE_FIELDS.length);
    for (let t = 0; t < TRIALS; t++) {
      const start = process.hrtime.bigint();
      let acc = plaintextBackend.encrypt(vectors[0]);
      for (let i = 1; i < vectors.length; i++) {
        acc = plaintextBackend.add(acc, plaintextBackend.encrypt(vectors[i]));
      }
      const sum = plaintextBackend.decrypt(acc);
      plaintextMean = Float64Array.from(sum, (v) => v / fleetSize);
      plaintextTrials.push(Number(process.hrtime.bigint() - start) / 1e6);
    }
    const plaintextBytes = fleetSize * (FEATURE_FIELDS.length * 8);

    const encryptAllTrials: number[] = [];
    const encryptOneTrials: number[] = [];
    const aggregateTrials: number[] = [];
    const decryptDecodeTrials: number[] = [];
    let ckksMean = new Float64Array(FEATURE_FIELDS.length);
    let ciphertextBytes = 0;

    for (let t = 0; t < TRIALS; t++) {
      const encryptStart = process.hrtime.bigint();
      const ciphers = vectors.map((v) => {
        const oneStart = process.hrtime.bigint();
        const c = backend.encrypt(v);
        if (t === 0) encryptOneTrials.push(Number(process.hrtime.bigint() - oneStart) / 1e6);
        return c;
      });
      encryptAllTrials.push(Number(process.hrtime.bigint() - encryptStart) / 1e6);

      ciphertextBytes = backend.byteLength(ciphers[0]);

      const aggStart = process.hrtime.bigint();
      let acc = ciphers[0];
      for (let i = 1; i < ciphers.length; i++) acc = backend.add(acc, ciphers[i]);
      aggregateTrials.push(Number(process.hrtime.bigint() - aggStart) / 1e6);

      const ddStart = process.hrtime.bigint();
      const sum = backend.decrypt(acc);
      decryptDecodeTrials.push(Number(process.hrtime.bigint() - ddStart) / 1e6);
      ckksMean = Float64Array.from(sum.subarray(0, FEATURE_FIELDS.length), (v) => v / fleetSize);
    }

    const encryptAllMs = median(encryptAllTrials);
    const encryptCriticalPathMs = median(encryptOneTrials.length ? encryptOneTrials : [0]);
    const aggregateMs = median(aggregateTrials);
    const decryptDecodeMs = median(decryptDecodeTrials);

    const ckksBytes = fleetSize * ciphertextBytes;
    const maxErr = maxAbsError(plaintextMean, ckksMean);

    results.push({
      fleetSize,
      plaintext: { latencyMs: median(plaintextTrials), bytes: plaintextBytes, mean: Array.from(plaintextMean) },
      ckks: {
        encryptAllMs,
        encryptCriticalPathMs,
        aggregateMs,
        decryptDecodeMs,
        totalMs: encryptCriticalPathMs + aggregateMs + decryptDecodeMs,
        bytes: ckksBytes,
        mean: Array.from(ckksMean),
        maxAbsErrorVsPlaintext: maxErr,
      },
    });

    console.log(`Fleet size ${fleetSize}:`);
    console.log(`  plaintext: ${median(plaintextTrials).toFixed(4)} ms, ${plaintextBytes} bytes uploaded`);
    console.log(
      `  CKKS:      encrypt(critical path, 1 drone)=${encryptCriticalPathMs.toFixed(3)}ms  ` +
        `encrypt(total compute, all drones)=${encryptAllMs.toFixed(3)}ms  aggregate=${aggregateMs.toFixed(3)}ms  ` +
        `decrypt+decode=${decryptDecodeMs.toFixed(3)}ms, ${ckksBytes} bytes uploaded`,
    );
    console.log(`  bandwidth ratio (CKKS/plaintext): ${(ckksBytes / plaintextBytes).toFixed(0)}x`);
    console.log(`  mean accuracy vs plaintext ground truth: maxAbsErr=${maxErr.toExponential(3)}\n`);
  }

  const csvRows = results.map((r) => [
    r.fleetSize,
    r.plaintext.latencyMs.toFixed(4),
    r.plaintext.bytes,
    r.ckks.encryptCriticalPathMs.toFixed(4),
    r.ckks.aggregateMs.toFixed(4),
    r.ckks.decryptDecodeMs.toFixed(4),
    r.ckks.totalMs.toFixed(4),
    r.ckks.bytes,
    (r.ckks.bytes / r.plaintext.bytes).toFixed(1),
    r.ckks.maxAbsErrorVsPlaintext.toExponential(3),
  ]);

  writeArtifact(
    'fleet-aggregation-compare',
    { experiment: meta, paramSet: PARAM_SET.id, trialsPerFleetSize: TRIALS, featureFields: FEATURE_FIELDS, results },
    {
      header: [
        'fleetSize',
        'plaintextLatencyMs',
        'plaintextBytes',
        'ckksEncryptCriticalPathMs',
        'ckksAggregateMs',
        'ckksDecryptDecodeMs',
        'ckksTotalMs',
        'ckksBytes',
        'bandwidthRatio',
        'meanMaxAbsError',
      ],
      rows: csvRows,
    },
  );
}

main().catch((err) => {
  console.error(err);
  process.exitCode = 1;
});
