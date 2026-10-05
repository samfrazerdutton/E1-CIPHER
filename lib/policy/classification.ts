export type SecurityClass = 'LOCAL_ONLY' | 'PLAINTEXT' | 'ENCRYPTED' | 'AGGREGATABLE' | 'BLOCKED';

export interface DataCatalogEntry {
  readonly dataType: string;
  readonly baselineClass: SecurityClass;
  readonly rationale: string;
}

/**
 * Static baseline classification table (section "Create a hybrid security
 * pipeline"). This is the starting point the adaptive scheduler (scheduler.ts)
 * then adjusts for runtime conditions — it is not itself the final decision.
 */
export const DATA_CATALOG: readonly DataCatalogEntry[] = [
  {
    dataType: 'camera_frame',
    baselineClass: 'LOCAL_ONLY',
    rationale: 'Raw imagery is large, highly sensitive, and not needed off-device once features are extracted.',
  },
  {
    dataType: 'object_bounding_box',
    baselineClass: 'PLAINTEXT',
    rationale: 'Low-sensitivity geometric summary already stripped of raw pixels; local use only, cheap to keep plaintext.',
  },
  {
    dataType: 'object_embedding',
    baselineClass: 'ENCRYPTED',
    rationale: 'Derived feature vector can re-identify what the drone observed; encrypt before it leaves the device.',
  },
  {
    dataType: 'fleet_aggregate_statistic',
    baselineClass: 'AGGREGATABLE',
    rationale: 'Only the aggregate (mean/sum across drones) is useful centrally — CKKS lets the server compute it without seeing inputs.',
  },
  {
    dataType: 'battery_telemetry',
    baselineClass: 'PLAINTEXT',
    rationale: 'Operationally necessary, low sensitivity, needed quickly and cheaply for fleet health dashboards.',
  },
  {
    dataType: 'restricted_location_telemetry',
    baselineClass: 'ENCRYPTED',
    rationale: 'GPS trace over a restricted site is sensitive on its own, independent of any other signal.',
  },
  {
    dataType: 'emergency_flight_control',
    baselineClass: 'LOCAL_ONLY',
    rationale: 'Safety-critical control loop; must never depend on network or cryptographic availability.',
  },
] as const;

export function lookupBaseline(dataType: string): DataCatalogEntry {
  const entry = DATA_CATALOG.find((e) => e.dataType === dataType);
  if (!entry) throw new Error(`Unknown data type in policy catalog: ${dataType}`);
  return entry;
}
