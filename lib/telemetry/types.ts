export type MissionScenario =
  | 'normal'
  | 'battery_stress'
  | 'network_degradation'
  | 'sensor_anomaly'
  | 'infra_anomaly';

export interface DroneTelemetrySample {
  readonly droneId: string;
  readonly tick: number;
  readonly scenario: MissionScenario;
  readonly timestampMs: number;
  readonly gps: { readonly lat: number; readonly lon: number };
  readonly altitudeM: number;
  readonly velocityMps: { readonly x: number; readonly y: number; readonly z: number };
  readonly imu: { readonly accelG: number; readonly gyroDps: number };
  readonly temperatureC: number;
  readonly vibrationMm2s: number;
  readonly batteryPct: number;
  readonly networkMbps: number;
  /** 0 = nominal, 1 = pipeline/infrastructure anomaly signature present in this drone's local view. */
  readonly infraAnomalyScore: number;
  /** True when the generator injected a sensor fault (dropout/noise spike) this tick. */
  readonly sensorFault: boolean;
}
