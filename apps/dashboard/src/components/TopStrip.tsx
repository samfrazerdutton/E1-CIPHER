import styles from './TopStrip.module.css';
import type { HostInfo } from '../lib/types';

export function TopStrip({
  title,
  subtitle,
  host,
}: {
  title: string;
  subtitle?: string;
  host?: HostInfo;
}) {
  return (
    <header className={styles.strip}>
      <div className={styles.viewTitle}>
        <h1 style={{ fontSize: 16 }}>{title}</h1>
        {subtitle && <p style={{ fontSize: 12, color: 'var(--ink-muted)' }}>{subtitle}</p>}
      </div>
      {host && (
        <div className={styles.readouts}>
          <div className={styles.readout}>
            <span className={styles.readoutLabel}>git commit</span>
            <span className={styles.readoutValue}>{host.gitCommit.slice(0, 10)}</span>
          </div>
          <div className={styles.readout}>
            <span className={styles.readoutLabel}>host CPU</span>
            <span className={styles.readoutValue}>{host.cpuModel.trim()}</span>
          </div>
          <div className={styles.readout}>
            <span className={styles.readoutLabel}>node</span>
            <span className={styles.readoutValue}>{host.nodeVersion}</span>
          </div>
          <div className={styles.readout}>
            <span className={styles.readoutLabel}>captured</span>
            <span className={styles.readoutValue}>{new Date(host.capturedAt).toLocaleString()}</span>
          </div>
        </div>
      )}
    </header>
  );
}
