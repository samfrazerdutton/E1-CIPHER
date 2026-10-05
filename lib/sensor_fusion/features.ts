import type { DroneTelemetrySample } from '../telemetry/types.js';

/**
 * A derived feature vector is what is allowed to leave the drone (see
 * lib/policy). Raw IMU/camera/LiDAR streams are never included here —
 * sensor fusion reduces a telemetry sample down to the small set of scalars
 * that fleet analytics actually needs.
 *
 * Field order is fixed (FEATURE_FIELDS) because it doubles as the CKKS slot
 * layout used by apps/fleet/compare.ts: index i of the Float64Array always
 * corresponds to FEATURE_FIELDS[i].
 */
export const FEATURE_FIELDS = [
  'altitudeM',
  'speedMps',
  'vibrationMm2s',
  'temperatureC',
  'batteryPct',
  'infraAnomalyScore',
] as const;

export type FeatureField = (typeof FEATURE_FIELDS)[number];

export function extractFeatureVector(sample: DroneTelemetrySample): Float64Array {
  const speed = Math.sqrt(sample.velocityMps.x ** 2 + sample.velocityMps.y ** 2 + sample.velocityMps.z ** 2);
  return Float64Array.from([
    sample.altitudeM,
    speed,
    sample.vibrationMm2s,
    sample.temperatureC,
    sample.batteryPct,
    sample.infraAnomalyScore,
  ]);
}

export function featureVectorToRecord(v: Float64Array): Record<FeatureField, number> {
  const out = {} as Record<FeatureField, number>;
  FEATURE_FIELDS.forEach((field, i) => {
    out[field] = v[i];
  });
  return out;
}
