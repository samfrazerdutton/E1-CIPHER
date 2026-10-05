import styles from './BottleneckChart.module.css';
import type { CkksParamSetResult } from '../lib/types';

// Fixed category order + color, independent of magnitude, so the same op
// always occupies the same color across every row (dataviz skill: "assign
// categorical hues in fixed order, never cycled"; "color follows the
// entity, never its rank"). decrypt+decode are merged into one bucket —
// both are the ciphertext-to-plaintext finalize step.
const CATEGORY_ORDER: { key: string; label: string; color: string; ops: string[] }[] = [
  { key: 'rescale', label: 'rescale', color: 'var(--cat-1)', ops: ['rescale'] },
  { key: 'relinearize', label: 'relinearize', color: 'var(--cat-2)', ops: ['relinearize'] },
  { key: 'rotate', label: 'rotate', color: 'var(--cat-3)', ops: ['rotate'] },
  { key: 'encrypt', label: 'encrypt', color: 'var(--cat-4)', ops: ['encrypt'] },
  { key: 'multiply', label: 'multiply', color: 'var(--cat-5)', ops: ['multiply'] },
  { key: 'encode', label: 'encode', color: 'var(--cat-6)', ops: ['encode'] },
  { key: 'add', label: 'add', color: 'var(--cat-7)', ops: ['add'] },
  { key: 'finalize', label: 'decrypt + decode', color: 'var(--cat-8)', ops: ['decrypt', 'decode'] },
];

export function BottleneckChart({ results }: { results: CkksParamSetResult[] }) {
  const rows = results.filter((r) => r.parametersSet && r.bottleneck);

  return (
    <div className={styles.chart}>
      {rows.map((r) => {
        const byOp = new Map(r.bottleneck!.map((b) => [b.op, b]));
        const segments = CATEGORY_ORDER.map((cat) => {
          const share = cat.ops.reduce((acc, op) => acc + (byOp.get(op)?.shareOfMeasuredTotal ?? 0), 0);
          const ms = cat.ops.reduce((acc, op) => acc + (byOp.get(op)?.meanMs ?? 0), 0);
          return { ...cat, share, ms };
        }).filter((s) => s.share > 0);

        return (
          <div className={styles.row} key={r.paramSet.id}>
            <span className={styles.rowLabel}>{r.paramSet.id}</span>
            <div className={styles.bar} role="img" aria-label={`${r.paramSet.id} per-op time share`}>
              {segments.map((s) => (
                <div
                  key={s.key}
                  className={styles.segment}
                  style={{ width: `${s.share * 100}%`, background: s.color }}
                  title={`${s.label}: ${(s.share * 100).toFixed(1)}% (${s.ms.toFixed(3)} ms mean)`}
                />
              ))}
            </div>
          </div>
        );
      })}

      <div className={styles.legend}>
        {CATEGORY_ORDER.map((c) => (
          <span className={styles.legendItem} key={c.key}>
            <span className={styles.swatch} style={{ background: c.color }} />
            {c.label}
          </span>
        ))}
      </div>
    </div>
  );
}
