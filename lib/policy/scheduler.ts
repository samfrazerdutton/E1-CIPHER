import type { SecurityClass } from './classification.js';

/**
 * Adaptive Confidential Compute placement (section "The breakthrough
 * feature"). The four names below are exactly the ones named in the project
 * brief; LOCAL/EDGE/CLOUD_PLAINTEXT are added for the (common) case of
 * low-sensitivity data that is allowed to leave the device unencrypted —
 * the brief's list wasn't exhaustive, and inventing a fifth bucket here is
 * more honest than forcing unencrypted transmission into an "ENCRYPTED"
 * label.
 */
export type ComputePlacement =
  | 'LOCAL_PLAINTEXT'
  | 'LOCAL_ENCRYPTED'
  | 'EDGE_PLAINTEXT'
  | 'EDGE_ENCRYPTED'
  | 'CLOUD_PLAINTEXT'
  | 'CLOUD_ENCRYPTED'
  | 'BLOCKED';

export interface SchedulerThresholds {
  readonly minNetworkMbpsToTransmit: number;
  readonly criticalBatteryPct: number;
  readonly lowLatencyBudgetMs: number;
}

export const DEFAULT_THRESHOLDS: SchedulerThresholds = {
  minNetworkMbpsToTransmit: 0.3,
  criticalBatteryPct: 15,
  lowLatencyBudgetMs: 50,
};

export interface SchedulerInput {
  readonly dataType: string;
  readonly baselineClass: SecurityClass;
  readonly baselineRationale: string;
  readonly batteryPct: number;
  readonly networkMbps: number;
  readonly missionPriority: 'routine' | 'elevated' | 'critical';
  readonly latencyBudgetMs: number;
  /** Measured (or benchmark-sourced) estimated cost of the encrypted path in ms. Must come from a real measurement — see tools/bench. */
  readonly estimatedEncryptedCostMs: number;
  readonly edgeNodeAvailable: boolean;
  readonly thresholds?: SchedulerThresholds;
}

export interface PlacementDecision {
  readonly placement: ComputePlacement;
  readonly transmitted: boolean;
  readonly encrypted: boolean;
  readonly explanation: readonly string[];
}

/**
 * Deterministic, rule-based policy decision — intentionally not a learned
 * or black-box scheduler, so every decision is explainable (section 8).
 */
export function decidePlacement(input: SchedulerInput): PlacementDecision {
  const t = input.thresholds ?? DEFAULT_THRESHOLDS;
  const explanation: string[] = [];
  explanation.push(`Data type '${input.dataType}' classified ${input.baselineClass}: ${input.baselineRationale}`);
  explanation.push(`Battery remaining: ${input.batteryPct.toFixed(1)}%.`);
  explanation.push(`Network available: ${input.networkMbps.toFixed(2)} Mbps.`);
  explanation.push(`Mission priority: ${input.missionPriority}.`);

  if (input.baselineClass === 'BLOCKED') {
    explanation.push('Classification is BLOCKED by policy; data is discarded, not transmitted.');
    return { placement: 'BLOCKED', transmitted: false, encrypted: false, explanation };
  }

  if (input.baselineClass === 'LOCAL_ONLY') {
    explanation.push('LOCAL_ONLY data never leaves the device regardless of battery/network/mission state.');
    return { placement: 'LOCAL_PLAINTEXT', transmitted: false, encrypted: false, explanation };
  }

  const networkOk = input.networkMbps >= t.minNetworkMbpsToTransmit;
  const needsEncryption = input.baselineClass === 'ENCRYPTED' || input.baselineClass === 'AGGREGATABLE';
  const batteryCritical = input.batteryPct < t.criticalBatteryPct;

  if (!networkOk) {
    explanation.push(`Network below ${t.minNetworkMbpsToTransmit} Mbps transmit threshold; holding data locally.`);
    const placement: ComputePlacement = needsEncryption ? 'LOCAL_ENCRYPTED' : 'LOCAL_PLAINTEXT';
    return { placement, transmitted: false, encrypted: needsEncryption, explanation };
  }

  if (needsEncryption) {
    explanation.push(`Encrypted path estimated cost: ${input.estimatedEncryptedCostMs.toFixed(2)} ms.`);
  }

  if (batteryCritical && input.missionPriority !== 'critical') {
    explanation.push(
      `Battery below critical threshold (${t.criticalBatteryPct}%) and mission priority is not critical; deferring non-essential ${needsEncryption ? 'encryption and ' : ''}transmission.`,
    );
    const placement: ComputePlacement = needsEncryption ? 'LOCAL_ENCRYPTED' : 'LOCAL_PLAINTEXT';
    return { placement, transmitted: false, encrypted: needsEncryption, explanation };
  }

  const tightLatency = input.latencyBudgetMs <= t.lowLatencyBudgetMs;
  if (!needsEncryption) {
    const placement: ComputePlacement = tightLatency && input.edgeNodeAvailable ? 'EDGE_PLAINTEXT' : 'CLOUD_PLAINTEXT';
    explanation.push(`Low-sensitivity data permitted to transmit unencrypted; routed ${placement}.`);
    return { placement, transmitted: true, encrypted: false, explanation };
  }

  if (tightLatency) {
    if (input.edgeNodeAvailable) {
      explanation.push(`Latency budget (${input.latencyBudgetMs} ms) is tight and an edge node is available; edge encrypted execution selected.`);
      return { placement: 'EDGE_ENCRYPTED', transmitted: true, encrypted: true, explanation };
    }
    explanation.push(`Latency budget (${input.latencyBudgetMs} ms) is tight and no edge node is available; local encrypted execution selected.`);
    return { placement: 'LOCAL_ENCRYPTED', transmitted: false, encrypted: true, explanation };
  }

  explanation.push('Latency budget permits off-device aggregation; cloud encrypted execution selected.');
  return { placement: 'CLOUD_ENCRYPTED', transmitted: true, encrypted: true, explanation };
}
