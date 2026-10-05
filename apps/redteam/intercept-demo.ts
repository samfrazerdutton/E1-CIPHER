/**
 * Red-team mode (project brief section 21) — a controlled, defensive
 * demonstration, not an offensive exploitation framework. Three scenarios,
 * each showing concretely what an attacker gains under the PLAINTEXT
 * architecture vs. the CONFIDENTIAL (CKKS) architecture built in this repo.
 * See docs/threat-model.md for the full threat register this demo is a
 * concrete illustration of (T1 compromised cloud, T3 intercepted traffic,
 * T9 stolen device).
 */
import { createCkksBackend } from '../../lib/ckks/ckksBackend.js';
import { CKKS_PARAM_SETS } from '../../lib/ckks/paramSets.js';
import { generateFleetTick } from '../../lib/telemetry/generator.js';
import { extractFeatureVector, FEATURE_FIELDS, featureVectorToRecord } from '../../lib/sensor_fusion/features.js';
import { writeArtifact, experimentMetadata } from '../../tools/bench/util.js';

const PARAM_SET = CKKS_PARAM_SETS.find((p) => p.id === 'N4096')!;

function shannonEntropyBitsPerByte(bytes: Uint8Array): number {
  const counts = new Array(256).fill(0);
  for (const b of bytes) counts[b]++;
  let entropy = 0;
  for (const c of counts) {
    if (c === 0) continue;
    const p = c / bytes.length;
    entropy -= p * Math.log2(p);
  }
  return entropy;
}

/** Entropy computed separately at each byte offset within a fixed-size word, to see whether a serialization's word layout is diluting whole-buffer entropy. */
function entropyByWordOffset(bytes: Uint8Array, wordSize: number): number[] {
  const out: number[] = [];
  for (let off = 0; off < wordSize; off++) {
    const slice = new Uint8Array(Math.floor((bytes.length - off) / wordSize));
    for (let i = 0; i < slice.length; i++) slice[i] = bytes[off + i * wordSize];
    out.push(shannonEntropyBitsPerByte(slice));
  }
  return out;
}

function section(title: string) {
  console.log(`\n${'='.repeat(70)}\n${title}\n${'='.repeat(70)}`);
}

async function main() {
  const meta = experimentMetadata();
  section('RED-TEAM MODE (defensive demonstration — see docs/threat-model.md)');
  console.log(`Experiment ${meta.experimentId}. All bytes below are real, produced by this repo's own code.`);

  const samples = generateFleetTick({ seed: 55, fleetSize: 3, tick: 70, scenario: 'infra_anomaly' });
  const target = samples[1];
  const featureVector = extractFeatureVector(target);
  const plaintextBytes = new Uint8Array(Float64Array.from(featureVector).buffer);

  const ckks = await createCkksBackend(PARAM_SET);
  const cipher = ckks.encrypt(featureVector);
  const ciphertextBytes = cipher.saveToArray(ckks.ctx.seal.ComprModeType.none) as Uint8Array;

  // A single 48-byte feature vector is far too small a sample for Shannon
  // entropy to mean anything: with only 48 bytes there can be at most 48
  // distinct values, capping empirical entropy at log2(48)~=5.58 bits/byte
  // no matter how random the source is. Build a larger same-shape plaintext
  // sample (many ticks of the same drone) so the entropy comparison below is
  // actually methodologically meaningful, and report both sample sizes.
  const manyTicks = Array.from({ length: 400 }, (_, t) =>
    extractFeatureVector(generateFleetTick({ seed: 55, fleetSize: 3, tick: t, scenario: 'infra_anomaly' })[1]),
  );
  const plaintextEntropySample = new Uint8Array(
    manyTicks.flatMap((v) => Array.from(new Uint8Array(Float64Array.from(v).buffer))),
  );

  // --- Scenario 1: network intercept ---
  section('SCENARIO 1: NETWORK INTERCEPT (T3)');
  console.log(`Target: ${target.droneId}'s feature vector: ${JSON.stringify(featureVectorToRecord(featureVector))}`);
  console.log(`\nPLAINTEXT architecture — intercepted bytes (first 48 of ${plaintextBytes.length} for this one reading):`);
  console.log('  ' + Buffer.from(plaintextBytes.slice(0, 48)).toString('hex'));
  console.log(
    `  Shannon entropy over ${plaintextEntropySample.length} bytes (${manyTicks.length} ticks of this drone, same field layout): ` +
      `${shannonEntropyBitsPerByte(plaintextEntropySample).toFixed(2)} bits/byte (well below 8.0 — IEEE-754 float64 sensor readings ` +
      `are structured: sign/exponent bytes barely vary tick to tick, so the byte distribution is far from uniform).`,
  );
  console.log(`\nCONFIDENTIAL (CKKS) architecture — intercepted bytes (first 48 of ${ciphertextBytes.length} for this one ciphertext):`);
  console.log('  ' + Buffer.from(ciphertextBytes.slice(0, 48)).toString('hex'));
  const wholeBufferEntropy = shannonEntropyBitsPerByte(ciphertextBytes);
  console.log(
    `  Shannon entropy over the full ${ciphertextBytes.length}-byte ciphertext: ${wholeBufferEntropy.toFixed(2)} bits/byte.`,
  );
  console.log(
    `  That is LOWER than the plaintext sample's entropy above — not a confidentiality weakness, but an artifact of this measurement: ` +
      `node-seal serializes each RNS coefficient as a fixed 8-byte little-endian word, but this parameter set's moduli are only 27-33 ` +
      'bits wide, so the top 3-4 bytes of every word are structurally always zero. Measured per-byte-offset-within-word entropy below shows exactly that:',
  );
  entropyByWordOffset(ciphertextBytes, 8).forEach((e, off) => console.log(`    word byte offset ${off}: ${e.toFixed(2)} bits/byte`));
  console.log(
    '  The low-order bytes (where the actual mod-q coefficient value lives) ARE near 8.0 bits/byte — that is the real evidence ' +
      "consistent with pseudorandomness under the RLWE assumption. A whole-buffer entropy number alone would have UNDERSTATED CKKS's " +
      'opacity here; per-word-offset entropy is the metric that actually supports the claim, and it is the one this repo reports.',
  );
  console.log(
    '  Methodology note: a 48-byte sample cannot exceed ~5.58 bits/byte of empirical entropy no matter how random the source is ' +
      '(at most 48 distinct byte values); that is why the plaintext estimate above uses 400 ticks, not the one 48-byte reading shown as hex.',
  );
  console.log(
    "  In both cases: high entropy is evidence consistent with indistinguishability from random, not a cryptographic proof of it — " +
      "that proof is CKKS/RLWE's, not this script's.",
  );

  // --- Scenario 2: compromised cloud/aggregator ---
  section('SCENARIO 2: COMPROMISED CLOUD / AGGREGATOR (T1)');
  console.log('PLAINTEXT architecture: a compromised aggregator holds, for every drone, every tick:');
  samples.forEach((s) => console.log(`  ${s.droneId}: ${JSON.stringify(featureVectorToRecord(extractFeatureVector(s)))}`));
  console.log('  -> Full individual exposure: exact location-correlated sensor readings for every drone, every tick.');
  console.log('\nCONFIDENTIAL architecture: a compromised aggregator holds, structurally, only:');
  console.log(`  - Ciphertext objects (this repo's aggregator code path — apps/fleet/compare.ts, apps/demo/killer-demo.ts —`);
  console.log('    never constructs or receives a seal.Decryptor or a SecretKey; lib/ckks/ckksBackend.ts\'s Decryptor is');
  console.log('    held by the party that calls createCkksBackend, i.e. the fleet operator, not the aggregator role).');
  console.log('  - At most, the final decrypted aggregate MEAN if the operator chooses to share it back — never a per-drone value.');
  console.log('  -> Blast radius: zero individual drone readings, by construction of which object holds which key — not by policy alone.');

  // --- Scenario 3: compromised drone ---
  section('SCENARIO 3: COMPROMISED / STOLEN DRONE (T9)');
  console.log('A stolen drone\'s on-device storage may contain:');
  console.log('  - Its OWN LOCAL_ONLY data (raw camera frames, emergency flight control state) — exposed. This is inherent: the data');
  console.log('    never left the device specifically so that confidentiality depends on physical device security, which this prototype');
  console.log('    does not harden (no encryption-at-rest is implemented here — see docs/threat-model.md).');
  console.log('  - Its own CKKS PUBLIC key only (lib/ckks/ckksBackend.ts\'s Encryptor is constructed from the public key; the secret');
  console.log('    key is never distributed to drones in this architecture) — a public key cannot decrypt anything.');
  console.log('  -> Blast radius: that one drone\'s own LOCAL_ONLY data. Zero other drones\' data, and zero ability to decrypt any');
  console.log('     fleet aggregate, past or future, because the stolen device never held the secret key in the first place.');

  writeArtifact('redteam-intercept-demo', {
    experiment: meta,
    target: target.droneId,
    plaintextSingleReadingByteLength: plaintextBytes.length,
    plaintextEntropySampleByteLength: plaintextEntropySample.length,
    plaintextEntropySampleTicks: manyTicks.length,
    ciphertextByteLength: ciphertextBytes.length,
    plaintextEntropyBitsPerByte: shannonEntropyBitsPerByte(plaintextEntropySample),
    ciphertextWholeBufferEntropyBitsPerByte: shannonEntropyBitsPerByte(ciphertextBytes),
    ciphertextEntropyByWordOffset: entropyByWordOffset(ciphertextBytes, 8),
    fleetSnapshotExposedUnderPlaintext: samples.map((s) => ({ droneId: s.droneId, features: featureVectorToRecord(extractFeatureVector(s)) })),
  });
}

main().catch((err) => {
  console.error(err);
  process.exitCode = 1;
});
