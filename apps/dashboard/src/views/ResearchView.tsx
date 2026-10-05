import { useResultData } from '../lib/useData';
import { DataGate } from '../components/DataGate';
import { Panel } from '../components/Panel';
import { Badge, ResultClassBadge } from '../components/Badge';
import { TopStrip } from '../components/TopStrip';
import tableStyles from '../components/Table.module.css';
import type { KernelBenchData } from '../lib/types';

const HYPOTHESES: { id: string; text: string; status: 'good' | 'warning' | 'neutral'; note: string }[] = [
  {
    id: 'H1',
    text: 'Spatial execution reduces data-movement cost in CKKS-related workloads.',
    status: 'neutral',
    note: 'Not tested — no E1 hardware. This repo produces the prerequisite: a measured bottleneck showing key-switching (rescale/relinearize/rotate) dominates, the workload H1 would need to be tested against.',
  },
  {
    id: 'H2',
    text: 'Whole-application execution provides a larger advantage than accelerating only the AI model.',
    status: 'neutral',
    note: 'Not tested on E1. apps/fleet/compare.ts does measure the whole pipeline (fusion -> encode -> encrypt -> aggregate -> decrypt), which is the right methodology — but there is no E1 run to compare it against.',
  },
  {
    id: 'H3',
    text: 'Adaptive encryption policies reduce energy and latency while preserving confidentiality for sensitive data.',
    status: 'warning',
    note: 'Partially tested: lib/policy/scheduler.ts\'s decision logic is deterministic, explainable, and unit-testable. Energy was not measured (no power instrumentation available).',
  },
  {
    id: 'H4',
    text: 'Encrypted edge aggregation can reduce raw-data transmission from autonomous systems.',
    status: 'warning',
    note: 'True for what the aggregator learns (confidentiality), false for bytes on the wire (~2,733x more bandwidth per drone at N4096). Both halves are reported — see Fleet view.',
  },
];

export function ResearchView() {
  const state = useResultData<KernelBenchData>('kernel-bench-latest.json');

  return (
    <DataGate state={state}>
      {(data) => (
        <>
          <TopStrip title="Research: hypotheses & reference kernels" subtitle="Research question status + from-scratch NTT/modular-arithmetic kernels" host={data.experiment.host} />
          <div style={{ padding: 20, display: 'flex', flexDirection: 'column', gap: 16 }}>
            <Panel title="Research question" subtitle="Can a spatial-dataflow general-purpose processor make privacy-preserving physical-AI workloads practical at the energy-constrained edge?">
              <div style={{ display: 'flex', flexDirection: 'column', gap: 14 }}>
                {HYPOTHESES.map((h) => (
                  <div key={h.id} style={{ display: 'flex', gap: 12, alignItems: 'flex-start' }}>
                    <Badge variant={h.status === 'neutral' ? 'neutral' : h.status}>{h.id}</Badge>
                    <div>
                      <p style={{ color: 'var(--ink-primary)', fontSize: 13 }}>{h.text}</p>
                      <p style={{ color: 'var(--ink-muted)', fontSize: 12, marginTop: 4, maxWidth: 680 }}>{h.note}</p>
                    </div>
                  </div>
                ))}
              </div>
            </Panel>

            <Panel
              title="Reference kernel micro-benchmarks"
              subtitle="lib/kernels — a from-scratch NTT and modular-multiplication implementation, independent of SEAL's internals"
              right={<ResultClassBadge resultClass={data.experiment.host.resultClass} />}
            >
              <div style={{ overflowX: 'auto' }}>
                <table className={tableStyles.table}>
                  <thead>
                    <tr>
                      <th>N</th>
                      <th className={tableStyles.num}>naive modmul (ms)</th>
                      <th className={tableStyles.num}>Montgomery modmul (ms)</th>
                      <th className={tableStyles.num}>Montgomery speedup</th>
                      <th className={tableStyles.num}>NTT forward (ms)</th>
                      <th className={tableStyles.num}>butterfly ops</th>
                    </tr>
                  </thead>
                  <tbody>
                    {data.results.map((r) => (
                      <tr key={r.n}>
                        <td>{r.n}</td>
                        <td className={tableStyles.num}>{r.modMulNaive.meanMs.toFixed(3)}</td>
                        <td className={tableStyles.num}>{r.modMulMont.meanMs.toFixed(3)}</td>
                        <td className={tableStyles.num} style={{ color: 'var(--status-warning)' }}>
                          {(r.modMulNaive.meanMs / r.modMulMont.meanMs).toFixed(2)}x
                        </td>
                        <td className={tableStyles.num}>{r.nttForward.meanMs.toFixed(3)}</td>
                        <td className={tableStyles.num}>{r.nttStats.butterflyOps}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
              <p style={{ marginTop: 14, fontSize: 12, color: 'var(--ink-muted)', maxWidth: 680 }}>
                Montgomery multiplication is consistently <em>slower</em> than naive BigInt modulo here (19-83x) — V8's
                native BigInt <code className="mono">%</code> is already a tuned C++ routine, and this managed-VM
                implementation pays pure overhead with no division cost to avoid. Reported as a loss, not omitted —
                see docs/bottleneck-report.md §4.
              </p>
            </Panel>
          </div>
        </>
      )}
    </DataGate>
  );
}
