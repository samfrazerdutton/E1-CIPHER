/**
 * The 3-minute demo (project brief section "The killer demonstration").
 *
 * Scenario: a 20-drone fleet inspecting infrastructure. A subset of drones
 * fly near a developing structural anomaly. Raw imagery never leaves any
 * drone. Each drone's local feature vector (lib/sensor_fusion) is CKKS-
 * encrypted and homomorphically summed by an untrusted aggregator; only the
 * fleet operator (who holds the secret key) ever decrypts, and only the
 * fleet-wide MEAN anomaly score — never an individual drone's reading.
 *
 * Locating *which* drone should respond without leaking any drone's value
 * to the aggregator is the actual hard part of "identify the highest-risk
 * region" under a real confidentiality guarantee (CKKS gives you sum/mean
 * cheaply; it does not give you a cheap, exact argmax across ciphertexts).
 * This demo resolves that honestly: the aggregator's encrypted mean decides
 * *whether* a fleet-wide alert condition exists; each drone then compares
 * its own already-locally-known anomaly score (never transmitted) against
 * a public threshold to self-select as a responder. The aggregator is
 * never told which drone (or how many) self-selected, only that a mission
 * update was broadcast fleet-wide.
 *
 * Every number below is produced by code elsewhere in this repo
 * (lib/telemetry, lib/sensor_fusion, lib/policy, lib/ckks) — this script
 * narrates and compares, it does not introduce new fabricated numbers.
 */
import { generateFleetTick } from '../../lib/telemetry/generator.js';
import { extractFeatureVector, FEATURE_FIELDS } from '../../lib/sensor_fusion/features.js';
import { lookupBaseline } from '../../lib/policy/classification.js';
import { decidePlacement, type SchedulerInput } from '../../lib/policy/scheduler.js';
import { createCkksBackend } from '../../lib/ckks/ckksBackend.js';
import { createPlaintextBackend } from '../../lib/ckks/plaintextBackend.js';
import { CKKS_PARAM_SETS } from '../../lib/ckks/paramSets.js';
import { experimentMetadata, writeArtifact } from '../../tools/bench/util.js';

const FLEET_SIZE = 20;
const TICK = 70; // infra_anomaly ramp (lib/telemetry/generator.ts) is active past tick 40
const FLEET_ALERT_MEAN_THRESHOLD = 0.15; // public, fleet-wide encrypted-mean alert threshold
const DRONE_SELF_SELECT_THRESHOLD = 0.5; // public, per-drone local self-selection threshold
const PARAM_SET = CKKS_PARAM_SETS.find((p) => p.id === 'N4096')!;

function section(title: string) {
  console.log(`\n${'='.repeat(70)}\n${title}\n${'='.repeat(70)}`);
}

async function main() {
  const meta = experimentMetadata();
  section('E1-CIPHER KILLER DEMO: confidential fleet anomaly response');
  console.log(`Experiment ${meta.experimentId} — HOST REFERENCE RESULTS (no E1 hardware).`);
  console.log(`Fleet size ${FLEET_SIZE}, scenario 'infra_anomaly', tick ${TICK}.`);

  // --- 1. Local perception + sensor fusion (stays entirely on-device except the feature vector) ---
  section('1. LOCAL PERCEPTION (per drone, on-device)');
  const samples = generateFleetTick({ seed: 99, fleetSize: FLEET_SIZE, tick: TICK, scenario: 'infra_anomaly' });
  const vectors = samples.map(extractFeatureVector);
  console.log(`Each drone computed a ${FEATURE_FIELDS.length}-scalar feature vector locally: [${FEATURE_FIELDS.join(', ')}]`);
  console.log('Raw camera/LiDAR/IMU streams are not represented beyond this point — only the derived vector exists off-sensor.');

  // --- 2. Policy classification (section 7/8) ---
  section('2. POLICY ENGINE: classification + adaptive placement');
  const sampleDrone = samples[0];
  const policyExamples: { dataType: string; input: Omit<SchedulerInput, 'dataType' | 'baselineClass' | 'baselineRationale'> }[] = [
    {
      dataType: 'camera_frame',
      input: { batteryPct: sampleDrone.batteryPct, networkMbps: sampleDrone.networkMbps, missionPriority: 'elevated', latencyBudgetMs: 20, estimatedEncryptedCostMs: 0, edgeNodeAvailable: true },
    },
    {
      dataType: 'object_embedding',
      input: { batteryPct: sampleDrone.batteryPct, networkMbps: sampleDrone.networkMbps, missionPriority: 'elevated', latencyBudgetMs: 200, estimatedEncryptedCostMs: 1.95, edgeNodeAvailable: true },
    },
    {
      dataType: 'fleet_aggregate_statistic',
      input: { batteryPct: sampleDrone.batteryPct, networkMbps: sampleDrone.networkMbps, missionPriority: 'elevated', latencyBudgetMs: 500, estimatedEncryptedCostMs: 1.95, edgeNodeAvailable: false },
    },
    {
      dataType: 'battery_telemetry',
      input: { batteryPct: sampleDrone.batteryPct, networkMbps: sampleDrone.networkMbps, missionPriority: 'routine', latencyBudgetMs: 1000, estimatedEncryptedCostMs: 0, edgeNodeAvailable: true },
    },
  ];
  const policyDecisions = policyExamples.map(({ dataType, input }) => {
    const baseline = lookupBaseline(dataType);
    const decision = decidePlacement({ dataType, baselineClass: baseline.baselineClass, baselineRationale: baseline.rationale, ...input });
    console.log(`\n[${dataType}] -> ${decision.placement}`);
    decision.explanation.forEach((line) => console.log(`    ${line}`));
    return { dataType, decision };
  });

  // --- 3. Encrypt + homomorphic aggregate (the aggregator never decrypts) ---
  section('3. CONFIDENTIAL FLEET AGGREGATION (CKKS, this repo\'s real code path)');
  const ckks = await createCkksBackend(PARAM_SET);
  const plaintext = createPlaintextBackend(ckks.slotCount);

  const encryptStart = process.hrtime.bigint();
  const ciphers = vectors.map((v) => ckks.encrypt(v));
  const encryptMs = Number(process.hrtime.bigint() - encryptStart) / 1e6;

  const aggStart = process.hrtime.bigint();
  let aggCipher = ciphers[0];
  for (let i = 1; i < ciphers.length; i++) aggCipher = ckks.add(aggCipher, ciphers[i]);
  const aggregateMs = Number(process.hrtime.bigint() - aggStart) / 1e6;

  const decryptStart = process.hrtime.bigint();
  const sumVector = ckks.decrypt(aggCipher);
  const decryptMs = Number(process.hrtime.bigint() - decryptStart) / 1e6;

  const anomalyFieldIndex = FEATURE_FIELDS.indexOf('infraAnomalyScore');
  const fleetMeanAnomalyScore = sumVector[anomalyFieldIndex] / FLEET_SIZE;

  console.log(`Aggregator homomorphically summed ${FLEET_SIZE} ciphertexts (${encryptMs.toFixed(2)}ms encrypt total, ${aggregateMs.toFixed(3)}ms aggregate).`);
  console.log(`Only the fleet operator decrypts (${decryptMs.toFixed(3)}ms) — and only the MEAN, not any individual drone's vector.`);
  console.log(`Fleet-wide mean infraAnomalyScore (decrypted): ${fleetMeanAnomalyScore.toFixed(4)}`);

  const fleetAlert = fleetMeanAnomalyScore > FLEET_ALERT_MEAN_THRESHOLD;
  console.log(`Fleet alert threshold: ${FLEET_ALERT_MEAN_THRESHOLD} -> ${fleetAlert ? 'ALERT: fleet-wide anomaly condition confirmed' : 'no fleet-wide alert'}`);

  // --- 4. Local self-selection (never transmitted) ---
  section('4. LOCAL RESPONSE SELECTION (never transmitted to the aggregator)');
  const responders = samples.filter((s) => s.infraAnomalyScore > DRONE_SELF_SELECT_THRESHOLD);
  if (fleetAlert) {
    console.log(`Mission-update broadcast fleet-wide (plaintext, content-free: "elevated priority, self-assess").`);
    console.log(`Each drone compares its OWN already-known infraAnomalyScore (never shared) against ${DRONE_SELF_SELECT_THRESHOLD}:`);
    if (responders.length) {
      responders.forEach((r) => console.log(`  ${r.droneId} self-selects as responder (local score ${r.infraAnomalyScore.toFixed(3)}, never sent to aggregator)`));
    } else {
      console.log('  No individual drone crossed the self-selection threshold this tick (fleet alert was still correctly raised from the aggregate).');
    }
  } else {
    console.log('No fleet-wide alert was raised, so no mission update was broadcast.');
  }
  console.log('\nAt no point did the aggregator (the untrusted/cloud party performing the sum) receive any individual drone\'s plaintext reading.');

  // --- 5. Conventional vs Confidential Edge comparison ---
  section('5. CONVENTIONAL ARCHITECTURE vs. EFFICIENT CONFIDENTIAL EDGE');
  const plaintextBytes = FLEET_SIZE * FEATURE_FIELDS.length * 8;
  const ckksBytes = FLEET_SIZE * ckks.byteLength(ciphers[0]);

  const conventional = {
    dataTransmitted: `${plaintextBytes} bytes (raw feature vectors, plaintext)`,
    latencyMs: 'sub-millisecond (no cryptographic overhead)',
    privacyExposure: `Aggregator/cloud operator sees all ${FLEET_SIZE} drones' raw feature vectors individually, including each drone's exact infraAnomalyScore.`,
    energy: 'Not measured.',
  };
  const confidentialEdge = {
    dataTransmitted: `${ckksBytes} bytes (${FLEET_SIZE} CKKS ciphertexts, ${PARAM_SET.id})`,
    latencyMs: `${(encryptMs + aggregateMs + decryptMs).toFixed(2)} ms total compute (encrypt+aggregate+decrypt; see apps/fleet/compare.ts for critical-path-only timing)`,
    privacyExposure: 'Aggregator sees only ciphertext bytes and (if ever shared back) the fleet mean — never an individual drone\'s reading.',
    energy: 'Not measured.',
  };

  console.log('\nCONVENTIONAL (plaintext to cloud):');
  Object.entries(conventional).forEach(([k, v]) => console.log(`  ${k}: ${v}`));
  console.log('\nEFFICIENT CONFIDENTIAL EDGE (CKKS, this repo):');
  Object.entries(confidentialEdge).forEach(([k, v]) => console.log(`  ${k}: ${v}`));
  console.log(`\nBandwidth cost of confidentiality: ${(ckksBytes / plaintextBytes).toFixed(0)}x. This is the real, measured tradeoff — not hidden.`);
  console.log('Energy: NOT MEASURED in either architecture — no power instrumentation is available in this environment (see README "Energy Analysis").');
  console.log('E1 vs conventional CPU/GPU: NOT MEASURED — no E1 hardware or effcc toolchain is available (see README "Limitations").');

  writeArtifact('killer-demo', {
    experiment: meta,
    fleetSize: FLEET_SIZE,
    tick: TICK,
    paramSet: PARAM_SET.id,
    policyDecisions: policyExamples.map(({ dataType }, i) => ({ dataType, placement: policyDecisions[i].decision.placement, explanation: policyDecisions[i].decision.explanation })),
    fleetMeanAnomalyScore,
    fleetAlert,
    responderDroneIds: responders.map((r) => r.droneId),
    timingMs: { encryptAll: encryptMs, aggregate: aggregateMs, decrypt: decryptMs },
    bytes: { plaintext: plaintextBytes, ckks: ckksBytes },
    conventional,
    confidentialEdge,
  });
}

main().catch((err) => {
  console.error(err);
  process.exitCode = 1;
});
