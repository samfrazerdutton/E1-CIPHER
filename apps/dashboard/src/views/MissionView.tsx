import { useResultData } from '../lib/useData';
import { DataGate } from '../components/DataGate';
import { Panel } from '../components/Panel';
import { Badge } from '../components/Badge';
import { TopStrip } from '../components/TopStrip';
import tableStyles from '../components/Table.module.css';
import type { KillerDemoData } from '../lib/types';

const PLACEMENT_VARIANT: Record<string, 'good' | 'warning' | 'critical' | 'neutral'> = {
  LOCAL_PLAINTEXT: 'neutral',
  LOCAL_ENCRYPTED: 'warning',
  EDGE_PLAINTEXT: 'neutral',
  EDGE_ENCRYPTED: 'warning',
  CLOUD_PLAINTEXT: 'good',
  CLOUD_ENCRYPTED: 'warning',
  BLOCKED: 'critical',
};

export function MissionView() {
  const state = useResultData<KillerDemoData>('killer-demo-latest.json');

  return (
    <DataGate state={state}>
      {(data) => (
        <>
          <TopStrip
            title="Mission: confidential fleet anomaly response"
            subtitle={`${data.fleetSize}-drone fleet, scenario infra_anomaly, tick ${data.tick}, parameter set ${data.paramSet}`}
            host={data.experiment.host}
          />
          <div style={{ padding: 20, display: 'flex', flexDirection: 'column', gap: 16 }}>
            <Panel title="Policy engine decisions" subtitle="lib/policy/scheduler.ts — classification + adaptive placement, explained">
              <div style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
                {data.policyDecisions.map((d) => (
                  <div key={d.dataType} style={{ display: 'flex', gap: 12, alignItems: 'flex-start' }}>
                    <Badge variant={PLACEMENT_VARIANT[d.placement] ?? 'neutral'}>{d.placement}</Badge>
                    <div>
                      <p className="mono" style={{ fontSize: 12.5, color: 'var(--ink-primary)' }}>
                        {d.dataType}
                      </p>
                      <p style={{ fontSize: 12, color: 'var(--ink-muted)', marginTop: 3, maxWidth: 620 }}>{d.explanation[0]}</p>
                    </div>
                  </div>
                ))}
              </div>
            </Panel>

            <Panel title="Encrypted fleet aggregation outcome" subtitle="The aggregator sums ciphertexts; only the operator decrypts, and only the mean">
              <div style={{ display: 'flex', gap: 24, flexWrap: 'wrap' }}>
                <div>
                  <p style={{ fontSize: 11, color: 'var(--ink-faint)' }}>fleet-wide mean anomaly score (decrypted)</p>
                  <p className="mono" style={{ fontSize: 22, color: 'var(--ink-primary)' }}>
                    {data.fleetMeanAnomalyScore.toFixed(4)}
                  </p>
                </div>
                <div>
                  <p style={{ fontSize: 11, color: 'var(--ink-faint)' }}>fleet alert</p>
                  <Badge variant={data.fleetAlert ? 'critical' : 'good'}>{data.fleetAlert ? 'raised' : 'none'}</Badge>
                </div>
                <div>
                  <p style={{ fontSize: 11, color: 'var(--ink-faint)' }}>local self-selected responders</p>
                  <p className="mono" style={{ fontSize: 13, color: 'var(--ink-primary)' }}>
                    {data.responderDroneIds.length ? data.responderDroneIds.join(', ') : 'none this tick'}
                  </p>
                </div>
              </div>
              <p style={{ marginTop: 14, fontSize: 12, color: 'var(--ink-muted)', maxWidth: 680 }}>
                Each responder's own anomaly score never left its device — only the fleet-wide mean, computed
                homomorphically, was ever decrypted. See the Confidentiality view for what an attacker would see
                intercepting this exchange.
              </p>
            </Panel>

            <Panel title="Conventional architecture vs. Efficient confidential edge">
              <div style={{ overflowX: 'auto' }}>
                <table className={tableStyles.table}>
                  <thead>
                    <tr>
                      <th></th>
                      <th>conventional (plaintext)</th>
                      <th>confidential edge (CKKS)</th>
                    </tr>
                  </thead>
                  <tbody>
                    <tr>
                      <td>data transmitted</td>
                      <td className="mono">{data.conventional.dataTransmitted}</td>
                      <td className="mono">{data.confidentialEdge.dataTransmitted}</td>
                    </tr>
                    <tr>
                      <td>latency</td>
                      <td className="mono">{data.conventional.latencyMs}</td>
                      <td className="mono">{data.confidentialEdge.latencyMs}</td>
                    </tr>
                    <tr>
                      <td>privacy exposure</td>
                      <td>{data.conventional.privacyExposure}</td>
                      <td>{data.confidentialEdge.privacyExposure}</td>
                    </tr>
                    <tr>
                      <td>energy</td>
                      <td>
                        <Badge>not measured</Badge>
                      </td>
                      <td>
                        <Badge>not measured</Badge>
                      </td>
                    </tr>
                  </tbody>
                </table>
              </div>
              <p style={{ marginTop: 14, fontSize: 12, color: 'var(--ink-muted)' }}>
                Bandwidth cost of confidentiality: {(data.bytes.ckks / data.bytes.plaintext).toFixed(0)}x. Reproduce with{' '}
                <code className="mono">npm run demo:killer</code>.
              </p>
            </Panel>
          </div>
        </>
      )}
    </DataGate>
  );
}
