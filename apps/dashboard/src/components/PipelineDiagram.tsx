import styles from './PipelineDiagram.module.css';

const STAGES = [
  { title: 'Input (RNS limbs)', note: 'Per-modulus coefficient arrays' },
  { title: 'Forward NTT', note: 'O(N log N), regular butterfly network — lib/kernels/ntt.ts measures this shape standalone' },
  { title: 'Modular multiply-add', note: 'Fixed pattern against a public key-switch matrix — lib/kernels/modular.ts: naive vs. Montgomery' },
  { title: 'Inverse NTT', note: 'Mirror of the forward pass' },
  { title: 'Recompose / mod-switch', note: 'Base conversion across the RNS chain' },
  { title: 'Ciphertext out', note: 'Serialized size measured per parameter set' },
];

/** The rescale/relinearize/rotate decomposition from docs/README.md "E1 Mapping" — the actual argument for why this workload is architecture-sensitive. */
export function PipelineDiagram() {
  return (
    <div className={styles.pipeline} role="img" aria-label="CKKS key-switching pipeline, input to ciphertext">
      {STAGES.map((s, i) => (
        <div key={s.title} style={{ display: 'flex', alignItems: 'stretch' }}>
          <div className={styles.stage}>
            <div className={styles.stageTitle}>{s.title}</div>
            <div className={styles.stageNote}>{s.note}</div>
          </div>
          {i < STAGES.length - 1 && <div className={styles.arrow}>&#8594;</div>}
        </div>
      ))}
    </div>
  );
}
