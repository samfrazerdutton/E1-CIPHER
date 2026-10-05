import { useResultData } from '../lib/useData';
import { DataGate } from '../components/DataGate';
import { Panel } from '../components/Panel';
import { Badge, ResultClassBadge } from '../components/Badge';
import { TopStrip } from '../components/TopStrip';
import { BytesChart } from '../components/BytesChart';
import tableStyles from '../components/Table.module.css';
import type { FleetCompareData } from '../lib/types';

export function FleetView() {
  const state = useResultData<FleetCompareData>('fleet-aggregation-compare-latest.json');

  return (
    <DataGate state={state}>
      {(data) => {
        const maxRatio = Math.max(...data.results.map((r) => r.ckks.bytes / r.plaintext.bytes));
        return (
          <>
            <TopStrip
              title="Fleet aggregation: plaintext vs CKKS"
              subtitle={`Parameter set ${data.paramSet}, median of ${data.trialsPerFleetSize} trials per fleet size`}
              host={data.experiment.host}
            />
            <div style={{ padding: 20, display: 'flex', flexDirection: 'column', gap: 16 }}>
              <Panel
                title="Upload bandwidth per fleet size"
                subtitle="In-process simulation — no sockets opened; bytes are serialized payload size"
                right={<ResultClassBadge resultClass={data.experiment.host.resultClass} />}
              >
                <BytesChart groups={data.results.map((r) => ({ label: `fleet size ${r.fleetSize}`, plaintextBytes: r.plaintext.bytes, ckksBytes: r.ckks.bytes }))} />
                <p style={{ marginTop: 14, fontSize: 12, color: 'var(--ink-muted)', maxWidth: 680 }}>
                  The ratio is flat at ~{maxRatio.toFixed(0)}x across every fleet size, because each drone's ciphertext
                  size doesn't depend on fleet size. CKKS's actual win here is confidentiality of the aggregator's
                  view, not bandwidth — reported plainly rather than cherry-picked around. See{' '}
                  <Badge variant="warning">H4</Badge> on the Research view.
                </p>
              </Panel>

              <Panel title="Latency & accuracy" subtitle="Encrypt/aggregate/decrypt are real measured compute, not simulated network RTT">
                <div style={{ overflowX: 'auto' }}>
                  <table className={tableStyles.table}>
                    <thead>
                      <tr>
                        <th>fleet size</th>
                        <th className={tableStyles.num}>plaintext (ms)</th>
                        <th className={tableStyles.num}>CKKS encrypt critical path (ms)</th>
                        <th className={tableStyles.num}>CKKS aggregate (ms)</th>
                        <th className={tableStyles.num}>CKKS decrypt+decode (ms)</th>
                        <th className={tableStyles.num}>bandwidth ratio</th>
                        <th className={tableStyles.num}>mean accuracy (max abs err)</th>
                      </tr>
                    </thead>
                    <tbody>
                      {data.results.map((r) => (
                        <tr key={r.fleetSize}>
                          <td>{r.fleetSize}</td>
                          <td className={tableStyles.num}>{r.plaintext.latencyMs.toFixed(4)}</td>
                          <td className={tableStyles.num}>{r.ckks.encryptCriticalPathMs.toFixed(3)}</td>
                          <td className={tableStyles.num}>{r.ckks.aggregateMs.toFixed(3)}</td>
                          <td className={tableStyles.num}>{r.ckks.decryptDecodeMs.toFixed(3)}</td>
                          <td className={tableStyles.num}>{(r.ckks.bytes / r.plaintext.bytes).toFixed(0)}x</td>
                          <td className={tableStyles.num}>{r.ckks.maxAbsErrorVsPlaintext.toExponential(2)}</td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              </Panel>
            </div>
          </>
        );
      }}
    </DataGate>
  );
}
