// Mirrors the real result shapes written by tools/bench, apps/fleet,
// apps/demo, apps/redteam, and lib/security in the parent repo. These types
// exist to keep the dashboard honest about what it's reading — see
// lib/platform/e1Target.ts's ResultClass for the same idea on the data side.

export interface HostInfo {
  resultClass: 'HOST_REFERENCE';
  platform: string;
  arch: string;
  osRelease: string;
  cpuModel: string;
  cpuCount: number;
  totalMemoryBytes: number;
  nodeVersion: string;
  gitCommit: string;
  capturedAt: string;
}

export interface ExperimentMetadata {
  experimentId: string;
  timestamp: string;
  host: HostInfo;
}

export interface TimingStats {
  trials: number;
  meanMs: number;
  medianMs: number;
  stdDevMs: number;
  minMs: number;
  maxMs: number;
}

export interface OpResult {
  supported: boolean;
  reason?: string;
  timing?: TimingStats;
}

export interface CkksParamSetResult {
  paramSet: {
    id: string;
    logN: number;
    polyModulusDegree: number;
    coeffModulusBits: number[];
    scaleBits: number;
    notes: string;
  };
  parametersSet: boolean;
  maxBitsAt128: number;
  requestedBits: number;
  slotCount: number;
  ops: Record<string, OpResult>;
  ciphertextBytes?: number;
  accuracy?: { encodeEncryptDecryptDecodeMaxAbsError: number; multiplyMaxAbsError?: number };
  bottleneck?: { op: string; meanMs: number; shareOfMeasuredTotal: number }[];
}

export interface CkksBenchData {
  experiment: ExperimentMetadata;
  trialsPerOp: number;
  results: CkksParamSetResult[];
}

export interface KernelBenchResult {
  n: number;
  modMulNaive: TimingStats;
  modMulMont: TimingStats;
  nttForward: TimingStats;
  nttStats: { butterflyOps: number; stages: number };
}

export interface KernelBenchData {
  experiment: ExperimentMetadata;
  trialsPerOp: number;
  nttPrime: string;
  results: KernelBenchResult[];
}

export interface FleetSizeResult {
  fleetSize: number;
  plaintext: { latencyMs: number; bytes: number; mean: number[] };
  ckks: {
    encryptAllMs: number;
    encryptCriticalPathMs: number;
    aggregateMs: number;
    decryptDecodeMs: number;
    totalMs: number;
    bytes: number;
    mean: number[];
    maxAbsErrorVsPlaintext: number;
  };
}

export interface FleetCompareData {
  experiment: ExperimentMetadata;
  paramSet: string;
  trialsPerFleetSize: number;
  featureFields: string[];
  results: FleetSizeResult[];
}

export interface PolicyDecisionRecord {
  dataType: string;
  placement: string;
  explanation: string[];
}

export interface KillerDemoData {
  experiment: ExperimentMetadata;
  fleetSize: number;
  tick: number;
  paramSet: string;
  policyDecisions: PolicyDecisionRecord[];
  fleetMeanAnomalyScore: number;
  fleetAlert: boolean;
  responderDroneIds: string[];
  timingMs: { encryptAll: number; aggregate: number; decrypt: number };
  bytes: { plaintext: number; ckks: number };
  conventional: { dataTransmitted: string; latencyMs: string; privacyExposure: string; energy: string };
  confidentialEdge: { dataTransmitted: string; latencyMs: string; privacyExposure: string; energy: string };
}

export interface RedteamData {
  experiment: ExperimentMetadata;
  target: string;
  plaintextSingleReadingByteLength: number;
  plaintextEntropySampleByteLength: number;
  plaintextEntropySampleTicks: number;
  ciphertextByteLength: number;
  plaintextEntropyBitsPerByte: number;
  ciphertextWholeBufferEntropyBitsPerByte: number;
  ciphertextEntropyByWordOffset: number[];
  fleetSnapshotExposedUnderPlaintext: { droneId: string; features: Record<string, number> }[];
}

export interface SecurityManifest {
  product: string;
  version: string;
  generatedAt: string;
  build: { gitCommit: string; nodeVersion: string; platform: string; packageLockHashSha256: string };
  cryptography: {
    schemeFamily: string;
    library: { name: string; version: string; upstream: string };
    parameterSets: { id: string; polyModulusDegree: number; coeffModulusBits: number[]; totalCoeffModulusBits: number; scaleBits: number }[];
    otherBackends: string[];
  };
  dataClassifications: { dataType: string; baselineClass: string; rationale: string }[];
  dependencies: { name: string; version: string; dev: boolean }[];
  knownVulnerabilities: string;
  updateMechanism: string;
  networkInterfaces: string;
  testStatus: string;
}

// --- C++ runtime layer (this session) -- apps/experiments, apps/runtime_demo ---

export interface CppSelectiveExecutionData {
  paramSet: string;
  modes: { name: string; latencyMs: number; bandwidthBytes: number; securityExposedBytes: number }[];
}

export interface CppAcceleratorKernel {
  op: string;
  meanMs: number;
  pctShare: number;
  candidateValue: 'HIGH VALUE' | 'MEDIUM VALUE' | 'LOW VALUE';
  structuralNote: string;
}

export interface CppAcceleratorOpportunityData {
  paramSet: string;
  kernels: CppAcceleratorKernel[];
}

export interface CppScalingRow {
  devices: number;
  plaintextBandwidthMb: number;
  fullCkksBandwidthMb: number;
  selectiveCkksBandwidthMb: number;
  fullCkksEncryptionMs: number;
  selectiveCkksEncryptionMs: number;
  fullCkksGatewayMs: number;
  selectiveCkksGatewayMs: number;
}

export interface CppScalingData {
  paramSet: string;
  measuredPerOp: { encryptMs: number; addMs: number; ciphertextBytes: number };
  rows: CppScalingRow[];
}

export interface CraEvidenceData {
  manifest: SecurityManifest;
  assets: { path: string; kind: string; description: string }[];
  attackSurface: { surface: string; description: string; exposure: string }[];
  vulnReport: {
    generatedAt: string;
    tool: string;
    totalsBySeverity: Record<string, number>;
    entries: { name: string; severity: string; via: string; range: string }[];
    rawExitCode: number;
  };
  testRun: { generatedAt: string; command: string; passed: boolean; summary: string };
}
