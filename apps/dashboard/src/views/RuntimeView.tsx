import { useResultData } from '../lib/useData';
import { DataGate } from '../components/DataGate';
import { Panel } from '../components/Panel';
import { Badge } from '../components/Badge';
import { TopStrip } from '../components/TopStrip';
import tableStyles from '../components/Table.module.css';
import type { CppSelectiveExecutionData, CppAcceleratorOpportunityData, CppScalingData } from '../lib/types';

function formatBytes(n: number): string {
  if (n < 1024) return `${n.toFixed(0)} B`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
  return `${(n / (1024 * 1024)).toFixed(2)} MB`;
}

const CANDIDATE_VARIANT: Record<string, 'good' | 'warning' | 'neutral'> = {
  'HIGH VALUE': 'good',
  'MEDIUM VALUE': 'warning',
  'LOW VALUE': 'neutral',
};

function SelectivePanel() {
  const state = useResultData<CppSelectiveExecutionData>('cpp-selective-execution-latest.json');
  return (
    <DataGate state={state}>
      {(data) => (
        <Panel
          title="Experiment: Selective Confidential Execution"
          subtitle={`${data.paramSet} -- real C++/SEAL measurement, apps/experiments/selective_execution.cpp`}
        >
          <div style={{ overflowX: 'auto' }}>
            <table className={tableStyles.table}>
              <thead>
                <tr>
                  <th>mode</th>
                  <th className={tableStyles.num}>latency</th>
                  <th className={tableStyles.num}>bandwidth</th>
                  <th className={tableStyles.num}>confidential bytes exposed</th>
                </tr>
              </thead>
              <tbody>
                {data.modes.map((m) => (
                  <tr key={m.name}>
                    <td>{m.name}</td>
                    <td className={tableStyles.num}>{m.latencyMs.toFixed(3)} ms</td>
                    <td className={tableStyles.num}>{formatBytes(m.bandwidthBytes)}</td>
                    <td className={tableStyles.num}>
                      {m.securityExposedBytes > 0 ? (
                        <Badge variant="critical">{m.securityExposedBytes} B</Badge>
                      ) : (
                        <Badge variant="good">0 B</Badge>
                      )}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
          <p style={{ marginTop: 14, fontSize: 12, color: 'var(--ink-muted)', maxWidth: 680 }}>
            Mode C (selective, this runtime) matches Mode B's zero exposure at a fraction of its bandwidth and
            latency -- see <code className="mono">docs/adr/001-selective-confidential-execution.md</code>.
          </p>
        </Panel>
      )}
    </DataGate>
  );
}

function AcceleratorPanel() {
  const state = useResultData<CppAcceleratorOpportunityData>('cpp-accelerator-opportunity-latest.json');
  return (
    <DataGate state={state}>
      {(data) => {
        const keySwitchFamily = data.kernels.filter((k) => k.candidateValue === 'HIGH VALUE');
        const keySwitchPct = keySwitchFamily.reduce((acc, k) => acc + k.pctShare, 0);
        return (
          <Panel
            title="Experiment: Accelerator Opportunity Analysis"
            subtitle={`${data.paramSet} -- real C++/SEAL measurement, apps/experiments/accelerator_opportunity.cpp`}
          >
            <div style={{ overflowX: 'auto' }}>
              <table className={tableStyles.table}>
                <thead>
                  <tr>
                    <th>op</th>
                    <th className={tableStyles.num}>mean (ms)</th>
                    <th className={tableStyles.num}>% share</th>
                    <th>candidate</th>
                  </tr>
                </thead>
                <tbody>
                  {data.kernels.map((k) => (
                    <tr key={k.op}>
                      <td className="mono">{k.op}</td>
                      <td className={tableStyles.num}>{k.meanMs.toFixed(4)}</td>
                      <td className={tableStyles.num}>{k.pctShare.toFixed(1)}%</td>
                      <td>
                        <Badge variant={CANDIDATE_VARIANT[k.candidateValue]}>{k.candidateValue}</Badge>
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
            <p style={{ marginTop: 14, fontSize: 12, color: 'var(--ink-muted)', maxWidth: 680 }}>
              Key-switching family (relinearize + rescale + rotate), combined: <strong>{keySwitchPct.toFixed(1)}%</strong>{' '}
              of measured runtime -- more than <code className="mono">encrypt</code> alone (which measures the
              single highest individual share but is classified MEDIUM VALUE, not HIGH -- see{' '}
              <code className="mono">docs/adr/002-ntt-as-accelerator-candidate.md</code>).
            </p>
          </Panel>
        );
      }}
    </DataGate>
  );
}

function ScalingPanel() {
  const state = useResultData<CppScalingData>('cpp-scaling-latest.json');
  return (
    <DataGate state={state}>
      {(data) => (
        <Panel
          title="Experiment: Application Scaling"
          subtitle={`${data.paramSet} -- real per-op costs measured once, multiplied by device count`}
        >
          <div style={{ overflowX: 'auto' }}>
            <table className={tableStyles.table}>
              <thead>
                <tr>
                  <th className={tableStyles.num}>devices</th>
                  <th className={tableStyles.num}>plaintext</th>
                  <th className={tableStyles.num}>full CKKS</th>
                  <th className={tableStyles.num}>selective CKKS</th>
                  <th className={tableStyles.num}>full encrypt (ms)</th>
                  <th className={tableStyles.num}>selective encrypt (ms)</th>
                </tr>
              </thead>
              <tbody>
                {data.rows.map((r) => (
                  <tr key={r.devices}>
                    <td className={tableStyles.num}>{r.devices}</td>
                    <td className={tableStyles.num}>{formatBytes(r.plaintextBandwidthMb * 1024 * 1024)}</td>
                    <td className={tableStyles.num}>{formatBytes(r.fullCkksBandwidthMb * 1024 * 1024)}</td>
                    <td className={tableStyles.num}>{formatBytes(r.selectiveCkksBandwidthMb * 1024 * 1024)}</td>
                    <td className={tableStyles.num}>{r.fullCkksEncryptionMs.toFixed(2)}</td>
                    <td className={tableStyles.num}>{r.selectiveCkksEncryptionMs.toFixed(2)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
          <p style={{ marginTop: 14, fontSize: 12, color: 'var(--ink-muted)' }}>
            Measured per-op: encrypt {data.measuredPerOp.encryptMs.toFixed(4)} ms, add{' '}
            {data.measuredPerOp.addMs.toFixed(4)} ms, ciphertext {data.measuredPerOp.ciphertextBytes} bytes.
            Device-count rows multiply these measured constants by an exact integer count.
          </p>
        </Panel>
      )}
    </DataGate>
  );
}

export function RuntimeView() {
  return (
    <>
      <TopStrip
        title="Confidential Edge Compute Runtime"
        subtitle="runtime::Runtime -- real placement decisions, cost model, and three experiments (apps/experiments/)"
      />
      <div style={{ padding: 20, display: 'flex', flexDirection: 'column', gap: 16 }}>
        <SelectivePanel />
        <AcceleratorPanel />
        <ScalingPanel />
      </div>
    </>
  );
}
