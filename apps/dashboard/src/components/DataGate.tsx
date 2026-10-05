import type { ReactNode } from 'react';
import type { DataState } from '../lib/useData';

/** Renders loading/error states explicitly rather than letting a view silently render nothing. */
export function DataGate<T>({ state, children }: { state: DataState<T>; children: (data: T) => ReactNode }) {
  if (state.status === 'loading') {
    return <div className="mono" style={{ color: 'var(--ink-muted)', padding: 24 }}>loading…</div>;
  }
  if (state.status === 'error') {
    return (
      <div style={{ padding: 24, color: 'var(--status-warning)' }}>
        <p style={{ marginBottom: 8 }}>Could not load this view's data: {state.message}</p>
        <p className="mono" style={{ fontSize: 12, color: 'var(--ink-muted)' }}>
          Run the generating command in the repo root, then `npm run sync-data` in apps/dashboard (or restart `npm run dev`, which syncs automatically).
        </p>
      </div>
    );
  }
  return <>{children(state.data)}</>;
}
