import { gaussian, hashSeed, mulberry32 } from './rng.js';
import type { DroneTelemetrySample, MissionScenario } from './types.js';

export interface DroneHomeBase {
  readonly lat: number;
  readonly lon: number;
}

/**
 * Deterministic synthetic telemetry for one drone at one tick. Same
 * (seed, droneId, scenario, tick) always produces the same sample —
 * reproducibility is required by the experiment-tracking format (section 27).
 *
 * Scenario effects are deliberately simple and explainable, not a learned
 * simulator:
 *  - battery_stress: battery drains faster and sags under load.
 *  - network_degradation: available uplink bandwidth is reduced and jittery.
 *  - sensor_anomaly: IMU/vibration readings get periodic fault spikes.
 *  - infra_anomaly: a subset of drones see a rising infraAnomalyScore,
 *    modeling a real pipeline/structural anomaly in their field of view.
 */
export function generateSample(
  seed: number,
  droneId: string,
  tick: number,
  scenario: MissionScenario,
  home: DroneHomeBase,
): DroneTelemetrySample {
  const rand = mulberry32(hashSeed(`${seed}:${droneId}:${scenario}:${tick}`));

  const orbitAngle = (tick * 0.05 + hashSeed(droneId) % 628) / 100;
  const orbitRadiusDeg = 0.01 + 0.002 * Math.sin(tick * 0.01);
  const lat = home.lat + orbitRadiusDeg * Math.cos(orbitAngle);
  const lon = home.lon + orbitRadiusDeg * Math.sin(orbitAngle);

  const baseAltitude = 80 + gaussian(rand, 0, 2);
  const baseBatteryDrainPerTick = scenario === 'battery_stress' ? 0.08 : 0.02;
  const batteryPct = Math.max(0, 100 - baseBatteryDrainPerTick * tick - gaussian(rand, 0, 0.3));

  const baseNetworkMbps =
    scenario === 'network_degradation' ? Math.max(0.1, gaussian(rand, 1.2, 0.6)) : Math.max(0.5, gaussian(rand, 15, 3));

  let sensorFault = false;
  let vibration = Math.max(0, gaussian(rand, 2.5, 0.5));
  let gyroDps = gaussian(rand, 0, 1.5);
  if (scenario === 'sensor_anomaly' && tick % 23 < 3) {
    sensorFault = true;
    vibration += gaussian(rand, 18, 4);
    gyroDps += gaussian(rand, 25, 8);
  }

  let infraAnomalyScore = Math.max(0, gaussian(rand, 0.02, 0.02));
  if (scenario === 'infra_anomaly' && tick > 40) {
    const ramp = Math.min(1, (tick - 40) / 60);
    infraAnomalyScore = Math.min(1, ramp * 0.9 + gaussian(rand, 0, 0.05));
  }

  return {
    droneId,
    tick,
    scenario,
    timestampMs: tick * 200,
    gps: { lat, lon },
    altitudeM: baseAltitude,
    velocityMps: {
      x: gaussian(rand, 3, 0.4),
      y: gaussian(rand, 0, 0.4),
      z: gaussian(rand, 0, 0.2),
    },
    imu: { accelG: 1 + gaussian(rand, 0, 0.02), gyroDps },
    temperatureC: gaussian(rand, 24, 3),
    vibrationMm2s: vibration,
    batteryPct,
    networkMbps: baseNetworkMbps,
    infraAnomalyScore,
    sensorFault,
  };
}

export interface FleetTickOptions {
  readonly seed: number;
  readonly fleetSize: number;
  readonly tick: number;
  readonly scenario: MissionScenario;
}

const SITE_HOME: DroneHomeBase = { lat: 40.758, lon: -111.891 };

export function generateFleetTick(opts: FleetTickOptions): DroneTelemetrySample[] {
  const samples: DroneTelemetrySample[] = [];
  for (let i = 0; i < opts.fleetSize; i++) {
    const droneId = `drone-${String(i).padStart(3, '0')}`;
    samples.push(generateSample(opts.seed, droneId, opts.tick, opts.scenario, SITE_HOME));
  }
  return samples;
}
