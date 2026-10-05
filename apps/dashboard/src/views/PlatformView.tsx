import { Panel } from '../components/Panel';
import { Badge } from '../components/Badge';
import { TopStrip } from '../components/TopStrip';

export function PlatformView() {
  return (
    <>
      <TopStrip title="Platform limits" subtitle="What this environment cannot measure, stated plainly rather than estimated and labeled as real" />
      <div style={{ padding: 20, display: 'flex', flexDirection: 'column', gap: 16 }}>
        <Panel title="Compute" right={<Badge>not measured</Badge>}>
          <p style={{ color: 'var(--ink-secondary)', maxWidth: 680 }}>
            No Electron E1 hardware or effcc toolchain is available in this environment, and no public, verified E1
            architectural specification (PE count, clock, cache sizes, memory bandwidth) exists to build a labeled
            simulated-E1 cost model from. Every latency number elsewhere in this dashboard is a{' '}
            <Badge variant="neutral">host reference</Badge> result on ordinary x86_64 silicon, never silicon
            measurement. See <code className="mono">lib/platform/e1Target.ts</code>.
          </p>
        </Panel>

        <Panel title="Energy" right={<Badge>not measured</Badge>}>
          <p style={{ color: 'var(--ink-secondary)', maxWidth: 680 }}>
            This environment has no power-measurement instrumentation (no RAPL/perf-energy access, no bench-grade
            power meter). Every "energy" field this project would otherwise report — energy per inference, per
            drone-minute, per encrypted operation, per useful decision — is intentionally absent rather than
            estimated from a proxy and presented as measured.
          </p>
        </Panel>

        <Panel title="Compiler" right={<Badge>not available</Badge>}>
          <p style={{ color: 'var(--ink-secondary)', maxWidth: 680 }}>
            No effcc toolchain is available to compile <code className="mono">lib/kernels</code>'s reference NTT/
            modular-arithmetic kernels for E1, or to compare against CPU/GPU baselines. The hardware abstraction
            layer in <code className="mono">lib/platform/e1Target.ts</code> defines the integration surface a real
            E1 backend would need to implement, so the kernels can be retargeted the day a toolchain is available —
            it does not simulate compiler output.
          </p>
        </Panel>

        <Panel title="Why this matters">
          <p style={{ color: 'var(--ink-secondary)', maxWidth: 680 }}>
            This project's credibility rests on never blurring these three states. Every number elsewhere in this
            dashboard is traceable to a real measurement on real (host) hardware — see the Crypto, Research, Fleet,
            Mission, and Confidentiality views. Nothing claims to be an E1 measurement, a power measurement, or a
            compiler result, because none of those three things exist in this environment yet.
          </p>
        </Panel>
      </div>
    </>
  );
}
