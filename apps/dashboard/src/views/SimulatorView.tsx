import { useEffect, useMemo, useState } from 'react';
import { Panel } from '../components/Panel';
import { Badge } from '../components/Badge';
import { TopStrip } from '../components/TopStrip';
import { FleetMap } from '../components/FleetMap';
import controls from '../components/Controls.module.css';
import { generateFleetTick } from '../../../../lib/telemetry/generator.js';
import type { MissionScenario } from '../../../../lib/telemetry/types.js';
import { extractFeatureVector } from '../../../../lib/sensor_fusion/features.js';
import { lookupBaseline } from '../../../../lib/policy/classification.js';
import { decidePlacement, type ComputePlacement } from '../../../../lib/policy/scheduler.js';

const SCENARIOS: { value: MissionScenario; label: string }[] = [
  { value: 'normal', label: 'normal' },
  { value: 'battery_stress', label: 'battery stress' },
  { value: 'network_degradation', label: 'network degradation' },
  { value: 'sensor_anomaly', label: 'sensor anomaly' },
  { value: 'infra_anomaly', label: 'infra anomaly' },
];

const PLACEMENT_VARIANT: Record<ComputePlacement, 'good' | 'warning' | 'critical' | 'neutral'> = {
  LOCAL_PLAINTEXT: 'neutral',
  LOCAL_ENCRYPTED: 'warning',
  EDGE_PLAINTEXT: 'neutral',
  EDGE_ENCRYPTED: 'warning',
  CLOUD_PLAINTEXT: 'good',
  CLOUD_ENCRYPTED: 'warning',
  BLOCKED: 'critical',
};

/**
 * A genuinely live, steerable panel — not a replay. It runs
 * lib/telemetry/generator.ts and lib/policy/scheduler.ts unmodified, in
 * this browser tab, against whatever the controls below are set to.
 *
 * It deliberately does NOT run CKKS encryption live in-browser (that would
 * mean bundling node-seal's WASM build into this app for a demonstration
 * that adds little over the already-real benchmark in the Fleet/Mission
 * views) — the fleet-mean readout here is a plain client-side average,
 * labeled as such, not a stand-in for the real encrypted aggregation.
 */
export function SimulatorView() {
  const [fleetSize, setFleetSize] = useState(20);
  const [scenario, setScenario] = useState<MissionScenario>('infra_anomaly');
  const [tick, setTick] = useState(60);
  const [playing, setPlaying] = useState(false);
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const [missionPriority, setMissionPriority] = useState<'routine' | 'elevated' | 'critical'>('elevated');
  const [edgeNodeAvailable, setEdgeNodeAvailable] = useState(true);
  const [latencyBudgetMs, setLatencyBudgetMs] = useState(200);

  useEffect(() => {
    if (!playing) return;
    const id = setInterval(() => setTick((t) => t + 1), 400);
    return () => clearInterval(id);
  }, [playing]);

  const samples = useMemo(() => generateFleetTick({ seed: 7, fleetSize, tick, scenario }), [fleetSize, tick, scenario]);
  const meanAnomaly = useMemo(() => samples.reduce((acc, s) => acc + s.infraAnomalyScore, 0) / samples.length, [samples]);

  const selected = samples.find((s) => s.droneId === selectedId) ?? samples[0];
  const baseline = lookupBaseline('object_embedding');
  const decision = decidePlacement({
    dataType: 'object_embedding',
    baselineClass: baseline.baselineClass,
    baselineRationale: baseline.rationale,
    batteryPct: selected.batteryPct,
    networkMbps: selected.networkMbps,
    missionPriority,
    latencyBudgetMs,
    estimatedEncryptedCostMs: 1.95,
    edgeNodeAvailable,
  });
  const featureVector = extractFeatureVector(selected);

  return (
    <>
      <TopStrip title="Simulator" subtitle="Live, steerable — runs lib/telemetry and lib/policy unmodified in this tab, not a replay" />
      <div style={{ padding: 20, display: 'flex', flexDirection: 'column', gap: 16 }}>
        <Panel title="Controls">
          <div className={controls.row}>
            <span className={controls.label}>fleet size</span>
            <input type="range" min={1} max={50} value={fleetSize} onChange={(e) => setFleetSize(Number(e.target.value))} />
            <span className={controls.value}>{fleetSize}</span>
          </div>
          <div className={controls.row}>
            <span className={controls.label}>tick</span>
            <input type="range" min={0} max={200} value={tick} onChange={(e) => setTick(Number(e.target.value))} />
            <span className={controls.value}>{tick}</span>
            <button className={controls.playButton} onClick={() => setPlaying((p) => !p)}>
              {playing ? 'pause' : 'play'}
            </button>
          </div>
          <div className={controls.row}>
            <span className={controls.label}>scenario</span>
            <select value={scenario} onChange={(e) => setScenario(e.target.value as MissionScenario)}>
              {SCENARIOS.map((s) => (
                <option key={s.value} value={s.value}>
                  {s.label}
                </option>
              ))}
            </select>
          </div>
          <div className={controls.row}>
            <span className={controls.label}>mission priority</span>
            <select value={missionPriority} onChange={(e) => setMissionPriority(e.target.value as typeof missionPriority)}>
              <option value="routine">routine</option>
              <option value="elevated">elevated</option>
              <option value="critical">critical</option>
            </select>
          </div>
          <div className={controls.row}>
            <span className={controls.label}>latency budget</span>
            <input type="range" min={10} max={1000} step={10} value={latencyBudgetMs} onChange={(e) => setLatencyBudgetMs(Number(e.target.value))} />
            <span className={controls.value}>{latencyBudgetMs}ms</span>
          </div>
          <div className={controls.row}>
            <span className={controls.label}>edge node</span>
            <input type="checkbox" checked={edgeNodeAvailable} onChange={(e) => setEdgeNodeAvailable(e.target.checked)} />
          </div>
        </Panel>

        <Panel title="Fleet map" subtitle={`seed 7, tick ${tick}, ${fleetSize} drones, scenario '${scenario}'`}>
          <FleetMap samples={samples} selectedId={selected.droneId} onSelect={setSelectedId} />
          <p style={{ marginTop: 12, fontSize: 12, color: 'var(--ink-muted)' }}>
            Fleet mean infra anomaly score (plain client-side average, not CKKS-encrypted — see Fleet/Mission views for
            the real encrypted-aggregation numbers): <span className="mono">{meanAnomaly.toFixed(4)}</span>
          </p>
        </Panel>

        <Panel title={`Selected: ${selected.droneId}`} subtitle="Live policy decision for this drone's object_embedding, recomputed on every control change">
          <div style={{ display: 'flex', gap: 24, flexWrap: 'wrap', marginBottom: 14 }}>
            <div>
              <p style={{ fontSize: 11, color: 'var(--ink-faint)' }}>battery</p>
              <p className="mono" style={{ color: 'var(--ink-primary)' }}>
                {selected.batteryPct.toFixed(1)}%
              </p>
            </div>
            <div>
              <p style={{ fontSize: 11, color: 'var(--ink-faint)' }}>network</p>
              <p className="mono" style={{ color: 'var(--ink-primary)' }}>
                {selected.networkMbps.toFixed(2)} Mbps
              </p>
            </div>
            <div>
              <p style={{ fontSize: 11, color: 'var(--ink-faint)' }}>infra anomaly score</p>
              <p className="mono" style={{ color: 'var(--ink-primary)' }}>
                {selected.infraAnomalyScore.toFixed(3)}
              </p>
            </div>
            <div>
              <p style={{ fontSize: 11, color: 'var(--ink-faint)' }}>feature vector</p>
              <p className="mono" style={{ color: 'var(--ink-primary)', fontSize: 11 }}>
                [{Array.from(featureVector).map((v) => v.toFixed(1)).join(', ')}]
              </p>
            </div>
          </div>
          <div style={{ display: 'flex', gap: 12, alignItems: 'flex-start' }}>
            <Badge variant={PLACEMENT_VARIANT[decision.placement]}>{decision.placement}</Badge>
            <div>
              {decision.explanation.map((line, i) => (
                <p key={i} style={{ fontSize: 12, color: 'var(--ink-muted)', marginBottom: 2 }}>
                  {line}
                </p>
              ))}
            </div>
          </div>
        </Panel>
      </div>
    </>
  );
}
