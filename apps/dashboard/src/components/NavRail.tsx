import styles from './NavRail.module.css';
import type { ViewId } from '../views';

const ITEMS: { id: ViewId; code: string; label: string }[] = [
  { id: 'crypto', code: 'CK', label: 'Crypto' },
  { id: 'research', code: 'RS', label: 'Research' },
  { id: 'fleet', code: 'FL', label: 'Fleet' },
  { id: 'mission', code: 'MI', label: 'Mission' },
  { id: 'confidentiality', code: 'CO', label: 'Confidentiality' },
  { id: 'security', code: 'SE', label: 'Security & CRA' },
  { id: 'platform', code: 'PL', label: 'Platform limits' },
];

export function NavRail({ active, onSelect }: { active: ViewId; onSelect: (v: ViewId) => void }) {
  return (
    <nav className={styles.rail}>
      <div className={styles.brand}>
        <div className={styles.brandName}>E1-CIPHER</div>
        <div className={styles.brandTag}>Confidential physical AI fabric</div>
      </div>
      {ITEMS.map((item) => (
        <button
          key={item.id}
          className={`${styles.item} ${active === item.id ? styles.itemActive : ''}`}
          onClick={() => onSelect(item.id)}
          aria-current={active === item.id}
        >
          <span className={styles.code}>{item.code}</span>
          {item.label}
        </button>
      ))}
      <div className={styles.spacer} />
      <div className={styles.foot}>
        Reads results/*.json from this repo. No number here is invented for display.
      </div>
    </nav>
  );
}
