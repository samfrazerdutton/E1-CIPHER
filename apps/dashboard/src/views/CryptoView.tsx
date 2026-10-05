import { useResultData } from '../lib/useData';
import { DataGate } from '../components/DataGate';
import { Panel } from '../components/Panel';
import { ResultClassBadge } from '../components/Badge';
import { BottleneckChart } from '../components/BottleneckChart';
import { TopStrip } from '../components/TopStrip';
import tableStyles from '../components/Table.module.css';
import type { CkksBenchData } from '../lib/types';

const OP_ORDER = ['encode', 'encrypt', 'add', 'multiply', 'relinearize', 'rescale', 'rotate', 'decrypt', 'decode'];

export function CryptoView() {
  const state = useResultData<CkksBenchData>('ckks-bench-latest.json');

  return (
    <DataGate state={state}>
      {(data) => (
        <>
          <TopStrip
            title="CKKS operation benchmark"
            subtitle={`${data.trialsPerOp} trials per operation, 5 parameter sets (N=2^10..2^14)`}
            host={data.experiment.host}
          />
          <div style={{ padding: 20, display: 'flex', flexDirection: 'column', gap: 16 }}>
            <Panel
              title="Where the time actually goes"
              subtitle="Share of measured per-operation time, by parameter set — see docs/bottleneck-report.md"
              right={<ResultClassBadge resultClass={data.experiment.host.resultClass} />}
            >
              <BottleneckChart results={data.results} />
              <p style={{ marginTop: 14, fontSize: 12, color: 'var(--ink-muted)', maxWidth: 640 }}>
                Key-switching operations (rescale, relinearize, rotate) grow from roughly 60% to 73% of per-op time as N
                increases from 2048 to 16384 — the headline finding this project is built around. N1024's bar has no
                rescale/relinearize/rotate segments at all: its single-modulus chain has no key-switching support, so
                only encode/encrypt/add/decrypt+decode are measured there.
              </p>
            </Panel>

            <Panel title="Per-operation latency" subtitle="Mean of N trials, milliseconds. N/A = operation unsupported at this parameter set, not a crash.">
              <div style={{ overflowX: 'auto' }}>
                <table className={tableStyles.table}>
                  <thead>
                    <tr>
                      <th>op</th>
                      {data.results.map((r) => (
                        <th key={r.paramSet.id} className={tableStyles.num}>
                          {r.paramSet.id}
                        </th>
                      ))}
                    </tr>
                  </thead>
                  <tbody>
                    {OP_ORDER.map((op) => (
                      <tr key={op}>
                        <td>{op}</td>
                        {data.results.map((r) => {
                          const o = r.ops[op];
                          return (
                            <td key={r.paramSet.id} className={tableStyles.num}>
                              {!r.parametersSet ? '—' : o?.supported && o.timing ? o.timing.meanMs.toFixed(3) : 'N/A'}
                            </td>
                          );
                        })}
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </Panel>

            <Panel title="Parameter sets" subtitle="lib/ckks/paramSets.ts — includes one unsupported and one empirically-fixed configuration">
              <div style={{ overflowX: 'auto' }}>
                <table className={tableStyles.table}>
                  <thead>
                    <tr>
                      <th>id</th>
                      <th>N</th>
                      <th>coeff modulus chain (bits)</th>
                      <th className={tableStyles.num}>scale (bits)</th>
                      <th className={tableStyles.num}>supported</th>
                      <th className={tableStyles.num}>ciphertext bytes</th>
                      <th className={tableStyles.num}>roundtrip max abs err</th>
                    </tr>
                  </thead>
                  <tbody>
                    {data.results.map((r) => (
                      <tr key={r.paramSet.id}>
                        <td>{r.paramSet.id}</td>
                        <td className={tableStyles.num}>{r.paramSet.polyModulusDegree}</td>
                        <td className="mono">[{r.paramSet.coeffModulusBits.join(', ')}]</td>
                        <td className={tableStyles.num}>{r.paramSet.scaleBits}</td>
                        <td className={tableStyles.num}>{r.parametersSet ? 'yes' : `no (needs ${r.requestedBits}, budget ${r.maxBitsAt128})`}</td>
                        <td className={tableStyles.num}>{r.ciphertextBytes ?? '—'}</td>
                        <td className={tableStyles.num}>{r.accuracy ? r.accuracy.encodeEncryptDecryptDecodeMaxAbsError.toExponential(2) : '—'}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </Panel>
          </div>
        </>
      )}
    </DataGate>
  );
}
