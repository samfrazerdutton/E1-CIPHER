import styles from './Badge.module.css';

export type BadgeVariant = 'neutral' | 'good' | 'warning' | 'critical' | 'serious';

export function Badge({ variant = 'neutral', children }: { variant?: BadgeVariant; children: React.ReactNode }) {
  return (
    <span className={`${styles.badge} ${styles[variant]}`}>
      <span className={styles.dot} />
      {children}
    </span>
  );
}

const RESULT_CLASS_LABEL: Record<string, string> = {
  HOST_REFERENCE: 'host reference',
  REAL_HARDWARE: 'real hardware',
  SIMULATED_E1: 'simulated E1',
  NOT_MEASURED: 'not measured',
};

/** The provenance label required throughout — see lib/platform/e1Target.ts's ResultClass. Always neutral: provenance is not a severity. */
export function ResultClassBadge({ resultClass }: { resultClass: string }) {
  return <Badge variant="neutral">{RESULT_CLASS_LABEL[resultClass] ?? resultClass}</Badge>;
}
