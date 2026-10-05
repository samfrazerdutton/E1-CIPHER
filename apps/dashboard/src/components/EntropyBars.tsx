import styles from './EntropyBars.module.css';

/** Single-series ordinal bar chart (8 bits/byte max) — one series needs no legend box per dataviz skill. */
export function EntropyBars({ values }: { values: number[] }) {
  return (
    <div className={styles.chart} role="img" aria-label="Shannon entropy in bits per byte, by byte offset within the 8-byte serialization word">
      {values.map((v, i) => (
        <div className={styles.col} key={i}>
          <span className={styles.value}>{v.toFixed(1)}</span>
          <div className={styles.bar} style={{ height: `${(v / 8) * 100}%`, background: v > 6 ? 'var(--cat-1)' : 'var(--ink-faint)' }} />
          <span className={styles.label}>{i}</span>
        </div>
      ))}
    </div>
  );
}
