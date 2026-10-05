import { useResultData } from '../lib/useData';
import { DataGate } from '../components/DataGate';
import { Panel } from '../components/Panel';
import { Badge } from '../components/Badge';
import { TopStrip } from '../components/TopStrip';
import { EntropyBars } from '../components/EntropyBars';
import tableStyles from '../components/Table.module.css';
import type { RedteamData } from '../lib/types';

export function ConfidentialityView() {
  const state = useResultData<RedteamData>('redteam-intercept-demo-latest.json');

  return (
    <DataGate state={state}>
      {(data) => (
        <>
          <TopStrip title="Confidentiality: network intercept" subtitle="A defensive red-team demonstration — see docs/threat-model.md T1/T3/T9" host={data.experiment.host} />
          <div style={{ padding: 20, display: 'flex', flexDirection: 'column', gap: 16 }}>
            <Panel title="Byte-level intercept comparison" subtitle={`Target: ${data.target}`}>
              <div style={{ display: 'flex', gap: 32, flexWrap: 'wrap' }}>
                <div>
                  <p style={{ fontSize: 11, color: 'var(--ink-faint)' }}>plaintext entropy ({data.plaintextEntropySampleTicks} ticks, {data.plaintextEntropySampleByteLength} bytes)</p>
                  <p className="mono" style={{ fontSize: 20, color: 'var(--ink-primary)' }}>
                    {data.plaintextEntropyBitsPerByte.toFixed(2)} bits/byte
                  </p>
                </div>
                <div>
                  <p style={{ fontSize: 11, color: 'var(--ink-faint)' }}>ciphertext whole-buffer entropy ({data.ciphertextByteLength} bytes)</p>
                  <p className="mono" style={{ fontSize: 20, color: 'var(--ink-primary)' }}>
                    {data.ciphertextWholeBufferEntropyBitsPerByte.toFixed(2)} bits/byte
                  </p>
                </div>
              </div>
              <p style={{ marginTop: 10, fontSize: 12, color: 'var(--status-warning)', maxWidth: 680 }}>
                The whole-buffer ciphertext number is lower than plaintext's — not a weakness, an artifact of
                measurement: see the per-word-offset breakdown below.
              </p>
            </Panel>

            <Panel title="Entropy by byte offset within the 8-byte serialization word" subtitle="node-seal stores each RNS coefficient in a fixed 64-bit word; this parameter set's moduli use only 27-33 of those bits">
              <EntropyBars values={data.ciphertextEntropyByWordOffset} />
              <p style={{ marginTop: 14, fontSize: 12, color: 'var(--ink-muted)', maxWidth: 680 }}>
                Low-order bytes (where the actual mod-q coefficient value lives) sit near 8.0 bits/byte — evidence
                consistent with pseudorandomness under the RLWE assumption. High-order bytes are structurally
                near-zero because the moduli are narrower than the 64-bit storage word. This is the metric that
                actually supports "ciphertext looks random," not the whole-buffer number above.
              </p>
            </Panel>

            <Panel title="Scenario 2: compromised cloud / aggregator" subtitle="What a compromised aggregator actually holds, in each architecture">
              <p style={{ fontSize: 12, color: 'var(--ink-muted)', marginBottom: 10 }}>
                Plaintext architecture — a compromised aggregator holds every drone's exact reading, every tick:
              </p>
              <div style={{ overflowX: 'auto' }}>
                <table className={tableStyles.table}>
                  <thead>
                    <tr>
                      <th>drone</th>
                      <th className={tableStyles.num}>altitude (m)</th>
                      <th className={tableStyles.num}>battery (%)</th>
                      <th className={tableStyles.num}>infra anomaly score</th>
                    </tr>
                  </thead>
                  <tbody>
                    {data.fleetSnapshotExposedUnderPlaintext.map((d) => (
                      <tr key={d.droneId}>
                        <td className="mono">{d.droneId}</td>
                        <td className={tableStyles.num}>{d.features.altitudeM.toFixed(2)}</td>
                        <td className={tableStyles.num}>{d.features.batteryPct.toFixed(2)}</td>
                        <td className={tableStyles.num}>{d.features.infraAnomalyScore.toFixed(3)}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
              <div style={{ marginTop: 14, display: 'flex', gap: 10, alignItems: 'center' }}>
                <Badge variant="critical">plaintext: full exposure</Badge>
                <Badge variant="good">CKKS: zero individual readings</Badge>
              </div>
              <p style={{ marginTop: 10, fontSize: 12, color: 'var(--ink-muted)', maxWidth: 680 }}>
                Under CKKS, the same compromised role holds only Ciphertext objects — this repo's aggregator code
                path never constructs a Decryptor or holds the secret key, by construction, not by policy. See{' '}
                <code className="mono">apps/redteam/intercept-demo.ts</code>.
              </p>
            </Panel>
          </div>
        </>
      )}
    </DataGate>
  );
}
