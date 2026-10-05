import styles from './BytesChart.module.css';

export interface BytesGroup {
  label: string;
  plaintextBytes: number;
  ckksBytes: number;
}

function formatBytes(n: number): string {
  if (n < 1024) return `${n} B`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
  return `${(n / (1024 * 1024)).toFixed(2)} MB`;
}

/** Log-scale bar widths (the magnitudes span ~5 orders of magnitude) with real byte counts as direct labels, so the compression never hides the actual numbers. */
export function BytesChart({ groups }: { groups: BytesGroup[] }) {
  const maxLog = Math.log10(Math.max(...groups.map((g) => g.ckksBytes)));

  return (
    <div className={styles.chart}>
      {groups.map((g) => (
        <div className={styles.group} key={g.label}>
          <span className={styles.groupLabel}>{g.label}</span>
          <div className={styles.track}>
            <div className={styles.bar} style={{ width: `${(Math.log10(g.plaintextBytes) / maxLog) * 100}%`, background: 'var(--cat-6)' }}>
              <span className={styles.barLabel}>{formatBytes(g.plaintextBytes)}</span>
            </div>
          </div>
          <div className={styles.track}>
            <div className={styles.bar} style={{ width: `${(Math.log10(g.ckksBytes) / maxLog) * 100}%`, background: 'var(--cat-1)' }}>
              <span className={styles.barLabel}>{formatBytes(g.ckksBytes)}</span>
            </div>
          </div>
        </div>
      ))}
      <div className={styles.legend}>
        <span className={styles.legendItem}>
          <span className={styles.swatch} style={{ background: 'var(--cat-6)' }} />
          plaintext
        </span>
        <span className={styles.legendItem}>
          <span className={styles.swatch} style={{ background: 'var(--cat-1)' }} />
          CKKS
        </span>
      </div>
      <p style={{ fontSize: 11, color: 'var(--ink-faint)' }}>Bar width is log-scale (magnitudes span ~5 orders); labels show the real byte counts.</p>
    </div>
  );
}
