import { useResultData } from '../lib/useData';
import { DataGate } from '../components/DataGate';
import { Panel } from '../components/Panel';
import { Badge } from '../components/Badge';
import { TopStrip } from '../components/TopStrip';
import tableStyles from '../components/Table.module.css';
import type { CraEvidenceData } from '../lib/types';

// Mirrors docs/threat-model.md's threat register — duplicated here for
// presentation, not a second source of truth; the markdown file is
// authoritative.
const THREATS: { id: string; threat: string; addressed: 'good' | 'warning' | 'critical' }[] = [
  { id: 'T1', threat: 'Compromised cloud / fleet aggregator reads raw sensor data', addressed: 'good' },
  { id: 'T2', threat: 'Malicious fleet operator requests individual drone data', addressed: 'warning' },
  { id: 'T3', threat: 'Intercepted network traffic between drone and aggregator', addressed: 'good' },
  { id: 'T4', threat: 'Compromised peer drone submits a falsified ciphertext', addressed: 'critical' },
  { id: 'T5', threat: 'Malicious/falsified telemetry from a legitimate drone', addressed: 'critical' },
  { id: 'T6', threat: 'Replay attack (resending an old encrypted message)', addressed: 'critical' },
  { id: 'T7', threat: 'Aggregate / model poisoning by outlier drones', addressed: 'critical' },
  { id: 'T8', threat: 'Unauthorized firmware / compromised software update', addressed: 'critical' },
  { id: 'T9', threat: 'Stolen device (physical access to a drone or key holder)', addressed: 'warning' },
  { id: 'T10', threat: 'Side-channel attacks on the CKKS implementation', addressed: 'critical' },
];

export function SecurityView() {
  const state = useResultData<CraEvidenceData>('cra-evidence.json');

  return (
    <DataGate state={state}>
      {(data) => (
        <>
          <TopStrip title="Security & CRA readiness evidence" subtitle="Readiness evidence for a risk assessment — not a compliance or certification claim" />
          <div style={{ padding: 20, display: 'flex', flexDirection: 'column', gap: 16 }}>
            <Panel title="Threat register" subtitle="docs/threat-model.md — full register, scope, and reasoning">
              <div style={{ overflowX: 'auto' }}>
                <table className={tableStyles.table}>
                  <thead>
                    <tr>
                      <th></th>
                      <th>threat</th>
                      <th>addressed</th>
                    </tr>
                  </thead>
                  <tbody>
                    {THREATS.map((t) => (
                      <tr key={t.id}>
                        <td className="mono">{t.id}</td>
                        <td>{t.threat}</td>
                        <td>
                          <Badge variant={t.addressed}>{t.addressed === 'good' ? 'yes' : t.addressed === 'warning' ? 'partial' : 'no'}</Badge>
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </Panel>

            <Panel title="Vulnerability register" subtitle={`${data.vulnReport.tool}, generated ${new Date(data.vulnReport.generatedAt).toLocaleString()}`}>
              <div style={{ display: 'flex', gap: 24 }}>
                {Object.entries(data.vulnReport.totalsBySeverity).map(([sev, n]) => (
                  <div key={sev}>
                    <p style={{ fontSize: 11, color: 'var(--ink-faint)' }}>{sev}</p>
                    <p className="mono" style={{ fontSize: 18, color: n > 0 ? 'var(--status-critical)' : 'var(--ink-primary)' }}>
                      {n}
                    </p>
                  </div>
                ))}
              </div>
              {data.vulnReport.entries.length === 0 && (
                <p style={{ marginTop: 12, fontSize: 12, color: 'var(--ink-muted)' }}>
                  No known vulnerabilities at generation time — a point-in-time result; regenerate with{' '}
                  <code className="mono">npm run security:evidence</code> before any release decision.
                </p>
              )}
            </Panel>

            <Panel title="Attack surface map" subtitle="docs/cra-evidence/10-attack-surface-map.md">
              <div style={{ overflowX: 'auto' }}>
                <table className={tableStyles.table}>
                  <thead>
                    <tr>
                      <th>surface</th>
                      <th>exposure</th>
                      <th>description</th>
                    </tr>
                  </thead>
                  <tbody>
                    {data.attackSurface.map((a) => (
                      <tr key={a.surface}>
                        <td style={{ whiteSpace: 'normal', minWidth: 140 }}>{a.surface}</td>
                        <td className="mono">{a.exposure}</td>
                        <td style={{ whiteSpace: 'normal', minWidth: 360 }}>{a.description}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </Panel>

            <Panel title="Security test results" subtitle={data.testRun.command}>
              <div style={{ display: 'flex', gap: 12, alignItems: 'center' }}>
                <Badge variant={data.testRun.passed ? 'good' : 'critical'}>{data.testRun.passed ? 'pass' : 'fail'}</Badge>
                <span className="mono" style={{ fontSize: 12, color: 'var(--ink-secondary)' }}>
                  {data.testRun.summary}
                </span>
              </div>
            </Panel>

            <Panel title="Cryptographic inventory" subtitle={`${data.manifest.cryptography.schemeFamily} via ${data.manifest.cryptography.library.name} (${data.manifest.cryptography.library.upstream})`}>
              <div style={{ overflowX: 'auto' }}>
                <table className={tableStyles.table}>
                  <thead>
                    <tr>
                      <th>data type</th>
                      <th>classification</th>
                    </tr>
                  </thead>
                  <tbody>
                    {data.manifest.dataClassifications.map((c) => (
                      <tr key={c.dataType}>
                        <td className="mono">{c.dataType}</td>
                        <td>{c.baselineClass}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
              <p style={{ marginTop: 12, fontSize: 12, color: 'var(--ink-muted)' }}>
                Full evidence set (11 documents): <code className="mono">docs/cra-evidence/</code>. Update mechanism:{' '}
                <em>{data.manifest.updateMechanism}</em>
              </p>
            </Panel>
          </div>
        </>
      )}
    </DataGate>
  );
}
